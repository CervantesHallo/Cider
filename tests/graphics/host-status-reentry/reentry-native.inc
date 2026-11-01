/* Native/Cocoa callback records only scalar observations under remote_lock.
 * Cocoa performs only pure RO reads and Unix waits; the Wine worker performs CANCEL.
 * The actual read-only reader is borrowed while the one main LIVE control
 * holds its existing PE lock. Its read function uses atomic loads, not NT work.
 * Lifetime observation refs never add a model/control/completion guard. */
#include "reentry.h"
#include <errno.h>
#include <time.h>
static pthread_cond_t reentry_condition=PTHREAD_COND_INITIALIZER;
static struct
{
    struct remote_host *host;
    struct remote_producer *producer;
    struct macdrv_status_cell *reader;
    struct reentry_frame witness;
    struct macdrv_remote_packet cancel;
    BOOL used,armed,callback_held,worker_started,worker_done,aborted;
} reentry;

/* Pure Unix synchronization. One monotone budget also bounds spurious wakes. */
static BOOL reentry_now(uint64_t *value)
{
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0||t.tv_nsec<0||t.tv_nsec>=1000000000||
       (uint64_t)t.tv_sec>(~(uint64_t)0-999999999)/1000000000)return FALSE;
    *value=(uint64_t)t.tv_sec*1000000000+t.tv_nsec;return TRUE;
}
static int reentry_wait_locked(const BOOL *flag,uint64_t *elapsed,uint32_t *timely)
{
    uint64_t begin,now,remaining;struct timespec timeout;int error=0;
    *elapsed=0;*timely=0;if(!reentry_now(&begin))return EINVAL;
    while(!*flag&&!reentry.aborted)
    {
        if(!reentry_now(&now)||now<begin){error=EINVAL;break;}
        if(now-begin>=4000000000ULL){error=ETIMEDOUT;break;}
        remaining=4000000000ULL-(now-begin);
        timeout.tv_sec=remaining/1000000000;timeout.tv_nsec=remaining%1000000000;
        error=pthread_cond_timedwait_relative_np(&reentry_condition,&remote_lock,&timeout);
        if(error&&error!=ETIMEDOUT&&error!=EINTR)break;
        error=0;
    }
    if(!reentry_now(&now)||now<begin)return EINVAL;
    *elapsed=now-begin;if(reentry.aborted)return ECANCELED;
    if(!error&&*flag&&*elapsed<4000000000ULL){*timely=1;return 0;}
    return error?error:ETIMEDOUT;
}
bool macdrv_reentry_cancel_after_attach(void *object,unsigned int cid,uint64_t ticket,
                                      uint32_t attached,uint32_t ledger,uint32_t indexed,bool main_thread)
{
    struct remote_host *h;struct remote_producer *p;
    struct macdrv_status_snapshot snapshot={0};
    pthread_mutex_lock(&remote_lock);h=reentry.host;p=reentry.producer;
    if(!reentry.armed||!h||!p||h->root->window!=object||h->context_id!=cid||h->ticket!=ticket)
    {pthread_mutex_unlock(&remote_lock);return false;}
    reentry.armed=FALSE;reentry.callback_held=TRUE;reentry.witness.fired=1;
    reentry.witness.before_attached=attached;reentry.witness.before_ledger=ledger;
    reentry.witness.before_indexed=indexed;reentry.witness.callback_main=main_thread;
    reentry.witness.pre_controls=h->status_controls;reentry.witness.pre_jobs=h->model_jobs;
    reentry.witness.pre_requests=p->request_inflight;reentry.witness.pre_resource_done=h->status_resource_done;
    pthread_cond_broadcast(&reentry_condition);
    reentry.witness.callback_wait_error=reentry_wait_locked(&reentry.worker_done,
        &reentry.witness.callback_elapsed_ns,&reentry.witness.callback_timely);
    /* Atomic RO loads only: LIVE still holds/serializes its existing reader. */
    reentry.witness.host_read=macdrv_status_cell_read(reentry.reader,&snapshot);
    reentry.witness.values_valid=snapshot.values_valid;
    reentry.callback_held=FALSE;reentry.witness.callback_returned=1;
    pthread_mutex_unlock(&remote_lock);return true;
}
void macdrv_reentry_record_after(void *object,unsigned int cid,uint64_t ticket,
                                 uint32_t attached,uint32_t ledger,uint32_t indexed,uint32_t error)
{
    pthread_mutex_lock(&remote_lock);
    if(reentry.host&&reentry.host->root->window==object&&reentry.host->context_id==cid&&reentry.host->ticket==ticket&&
       reentry.witness.fired&&reentry.witness.callback_returned)
    {
        reentry.witness.after_attached=attached;reentry.witness.after_ledger=ledger;
        reentry.witness.after_indexed=indexed;reentry.witness.hook_error=error;
    }
    pthread_mutex_unlock(&remote_lock);
}
static DWORD reentry_arm(const struct reentry_frame *f)
{
    struct remote_host *h;
    struct remote_producer *p;
    if(!live_host.setup_done||!live_host.context||!live_host.host_reader||live_host.install_sent||
       wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)
        return ERROR_INVALID_STATE;
    if(live_host.sequence>~(uint64_t)0-2)return ERROR_ARITHMETIC_OVERFLOW;
    pthread_mutex_lock(&remote_lock);
    h=host_status_lookup_locked(live_host.ticket);p=h?h->producer:NULL;
    if(reentry.used||!h||!p||!h->status_registered||h->state!=HOST_RESERVED||!p->active||p->request_inflight)
    {pthread_mutex_unlock(&remote_lock);return ERROR_INVALID_STATE;}
    InterlockedIncrement(&h->refs);InterlockedIncrement(&p->refs);
    reentry.host=h;reentry.producer=p;reentry.reader=live_host.host_reader;
    reentry.used=reentry.armed=TRUE;
    reentry.witness.root=f->root;reentry.witness.child=f->child;reentry.witness.context=h->context_id;
    reentry.witness.epoch=h->root->epoch;reentry.witness.lease=p->id;reentry.witness.ticket=h->ticket;
    reentry.witness.sequence=live_host.sequence;
    reentry.witness.cancel_sequence=live_host.sequence+2;
    reentry.cancel.size=sizeof(reentry.cancel);reentry.cancel.version=MACDRV_REMOTE_PREPARED_PACKET_VERSION;
    reentry.cancel.operation=MACDRV_REMOTE_CANCEL_HOST;reentry.cancel.root=f->root;
    reentry.cancel.root_epoch=h->root->epoch;reentry.cancel.lease_id=p->id;reentry.cancel.host_ticket=h->ticket;
    reentry.cancel.producer_pid=p->pid;reentry.cancel.producer_generation=p->generation;
    reentry.cancel.sequence=reentry.witness.cancel_sequence;
    reentry.witness.hook_error=~(uint32_t)0;
    pthread_mutex_unlock(&remote_lock);
    return ERROR_SUCCESS;
}
/* Only this Wine-thread entry performs actual ordinary control/NT work. */
static DWORD reentry_run_cancel(void)
{
    struct remote_producer *producer=NULL;struct macdrv_remote_packet packet;
    DWORD error;int waited;
    pthread_mutex_lock(&remote_lock);
    if(!reentry.host||reentry.worker_started)
    {pthread_mutex_unlock(&remote_lock);return ERROR_INVALID_STATE;}
    reentry.worker_started=TRUE;
    if(reentry.aborted)
    {
        /* An actual worker entered after ABORT; no request was admitted. */
        reentry.worker_done=TRUE;reentry.witness.worker_error=ERROR_CANCELLED;
        reentry.witness.worker_wait_error=ECANCELED;
        pthread_cond_broadcast(&reentry_condition);
        pthread_mutex_unlock(&remote_lock);return ERROR_SUCCESS;
    }
    waited=reentry_wait_locked(&reentry.callback_held,&reentry.witness.worker_elapsed_ns,&reentry.witness.worker_timely);
    reentry.witness.worker_wait_error=waited;packet=reentry.cancel;
    pthread_mutex_unlock(&remote_lock);
    error=waited?(waited==ETIMEDOUT?ERROR_TIMEOUT:ERROR_CANCELLED):ERROR_SUCCESS;
    if(!error)
    {
        packet.producer_tid=GetCurrentThreadId();
        error=packet_producer_begin(&packet,&producer);
        if(!error)
        {
            pthread_mutex_lock(&remote_lock);reentry.witness.flags|=1;pthread_mutex_unlock(&remote_lock);
            error=cancel_prepared_host(producer,&packet);
            error=packet_producer_end(producer,packet.operation,error);
        }
    }
    pthread_mutex_lock(&remote_lock);
    reentry.witness.worker_error=error;
    reentry.witness.post_controls=reentry.host->status_controls;reentry.witness.post_jobs=reentry.host->model_jobs;
    reentry.witness.post_requests=reentry.producer->request_inflight;reentry.witness.post_resource_done=reentry.host->status_resource_done;
    reentry.witness.post_retirement=reentry.host->retirement_pending;reentry.witness.post_queued=reentry.host->retirement_queued;
    reentry.witness.post_root_open=reentry.host->root->open;reentry.witness.post_active=reentry.producer->active;
    reentry.worker_done=TRUE;pthread_cond_broadcast(&reentry_condition);
    pthread_mutex_unlock(&remote_lock);return ERROR_SUCCESS;
}
static DWORD reentry_sync_sequence(void)
{
    DWORD error=ERROR_IO_PENDING;
    /* Main calls after actual worker join and INSTALL return, under its existing
     * live control lock. Reconcile only the test helper's request bookkeeping. */
    pthread_mutex_lock(&remote_lock);
    if(reentry.host&&reentry.worker_done&&!reentry.callback_held&&!reentry.host->model_jobs&&!reentry.host->status_controls)
    {
        if((reentry.witness.flags&1)&&live_host.sequence<reentry.cancel.sequence)live_host.sequence=reentry.cancel.sequence;
        reentry.witness.sequence_synced=1;error=ERROR_SUCCESS;
    }
    pthread_mutex_unlock(&remote_lock);return error;
}
static DWORD reentry_finish(void)
{
    struct remote_host *h;
    struct remote_producer *p;
    pthread_mutex_lock(&remote_lock);
    h=reentry.host;p=reentry.producer;
    if(!h){pthread_mutex_unlock(&remote_lock);return ERROR_SUCCESS;}
    if((reentry.worker_started&&!reentry.worker_done)||reentry.callback_held||h->status_controls||h->model_jobs||h->retirement_pending||h->retirement_queued||h->status_registered||
       p->host_status_owners||p->request_inflight)
    {pthread_mutex_unlock(&remote_lock);return ERROR_IO_PENDING;}
    reentry.host=NULL;reentry.producer=NULL;reentry.reader=NULL;reentry.armed=FALSE;
    pthread_mutex_unlock(&remote_lock);
    host_release(h);producer_release(p);
    return ERROR_SUCCESS;
}
NTSTATUS macdrv_host_reentry_fixture(void *args)
{
    struct reentry_frame *f=args;
    struct remote_producer *p;
    struct producer_setup *setup;
    DWORD error=ERROR_SUCCESS;
    uint32_t op;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<REENTRY_ARM||f->operation>REENTRY_ABORT||f->reserved[0]||f->reserved[1]||f->reserved64)
        return STATUS_INVALID_PARAMETER;
    op=f->operation;
    if(op==REENTRY_ARM)error=reentry_arm(f);
    else if(op==REENTRY_FINISH)error=reentry_finish();
    else if(op==REENTRY_RUN_CANCEL)error=reentry_run_cancel();
    else if(op==REENTRY_SYNC_SEQUENCE)error=reentry_sync_sequence();
    else if(op==REENTRY_ABORT)
    {pthread_mutex_lock(&remote_lock);reentry.aborted=TRUE;reentry.armed=FALSE;pthread_cond_broadcast(&reentry_condition);pthread_mutex_unlock(&remote_lock);}
    pthread_mutex_lock(&remote_lock);
    *f=reentry.witness;f->size=sizeof(*f);f->version=1;f->operation=op;f->status=error;
    f->armed=reentry.armed;f->observer=reentry.host!=NULL;
    f->worker_started=reentry.worker_started;f->worker_done=reentry.worker_done;
    f->registered=reentry.host?reentry.host->status_registered:0;f->owners=reentry.producer?reentry.producer->host_status_owners:0;
    f->cleanup=f->setups=0;
    for(p=producer_cleanup;p;p=p->cleanup_next)++f->cleanup;
    for(setup=setup_cleanup;setup;setup=setup->cleanup_next)++f->cleanup;
    for(setup=producer_setups;setup;setup=setup->next)++f->setups;
    pthread_mutex_unlock(&remote_lock);
    return STATUS_SUCCESS;
}
