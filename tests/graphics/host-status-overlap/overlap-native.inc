/* Included after the ordinary coordinator and the admitted live-context
 * helper. Only RUN reads immutable live identity; it never writes live_host.
 * ARM sets the conservative import-attempt latch before starting a thread.
 * Host/producer observation refs do not participate in model/control guards. */
#include "overlap.h"
static struct
{
    struct remote_host *host;
    struct remote_producer *producer;
    struct macdrv_remote_packet packet;
    UINT event;
    BOOL bound,armed,held,done,timely,started,returned;
    DWORD error;
    NTSTATUS wait_status;
} overlap;

static void overlap_after_install_begin(struct remote_host *host)
{
    HANDLE event=NULL;
    LARGE_INTEGER begin={0},frequency={0},end={0},timeout;
    NTSTATUS clock,status;
    BOOL timely;
    pthread_mutex_lock(&remote_lock);
    if(overlap.host==host&&overlap.armed&&overlap.started&&host->model_jobs&&host->status_controls)
    {
        overlap.armed=FALSE;overlap.held=TRUE;event=ULongToHandle(overlap.event);
    }
    pthread_mutex_unlock(&remote_lock);
    if(!event)return;
    clock=NtQueryPerformanceCounter(&begin,&frequency);
    timeout.QuadPart=-40000000;
    status=NtWaitForSingleObject(event,FALSE,&timeout);
    timely=!clock&&!NtQueryPerformanceCounter(&end,NULL)&&begin.QuadPart>=0&&frequency.QuadPart>0&&
        frequency.QuadPart<=0x7fffffffffffffffLL/4&&end.QuadPart>=begin.QuadPart&&
        end.QuadPart-begin.QuadPart<4*frequency.QuadPart;
    pthread_mutex_lock(&remote_lock);
    overlap.wait_status=status;overlap.timely=timely;overlap.held=FALSE;overlap.done=TRUE;
    pthread_mutex_unlock(&remote_lock);
}
static void overlap_snapshot(struct overlap_frame *f)
{
    struct remote_host *h;
    struct remote_producer *p,*cursor;
    struct producer_setup *setup;
    pthread_mutex_lock(&remote_lock);
    h=overlap.host;p=overlap.producer;
    f->epoch=overlap.packet.root_epoch;f->lease=overlap.packet.lease_id;
    f->ticket=overlap.packet.host_ticket;f->sequence=overlap.packet.sequence;
    f->bound=overlap.bound;f->held=overlap.held;f->done=overlap.done;f->timely=overlap.timely;
    f->wait_status=overlap.done?(UINT32)overlap.wait_status:~(UINT32)0;
    f->started=overlap.started;f->returned=overlap.returned;
    f->error=overlap.returned?overlap.error:ERROR_IO_PENDING;
    f->controls=h?h->status_controls:~(UINT32)0;f->jobs=h?h->model_jobs:~(UINT32)0;
    f->retirement=h?h->retirement_pending:~(UINT32)0;f->queued=h?h->retirement_queued:~(UINT32)0;
    f->registered=h?h->status_registered:~(UINT32)0;f->owners=p?p->host_status_owners:~(UINT32)0;
    f->resource_done=h?h->status_resource_done:~(UINT32)0;f->terminal=h?h->status_terminal:~(UINT32)0;
    f->requests=p?p->request_inflight:~(UINT32)0;f->cell=h?h->status_cell!=NULL:~(UINT32)0;
    f->registry_count=host_status_registered_count>~(UINT32)0?~(UINT32)0:(UINT32)host_status_registered_count;
    f->observer=h!=NULL;f->event_borrow=overlap.event!=0;f->active=p?p->active:~(UINT32)0;
    f->cleanup=f->setups=0;
    for(cursor=producer_cleanup;cursor;cursor=cursor->cleanup_next)++f->cleanup;
    for(setup=setup_cleanup;setup;setup=setup->cleanup_next)++f->cleanup;
    for(setup=producer_setups;setup;setup=setup->next)++f->setups;
    pthread_mutex_unlock(&remote_lock);
}
static DWORD overlap_arm(const struct overlap_frame *f)
{
    struct remote_host *h;
    struct remote_producer *p;
    struct macdrv_remote_packet packet={0};
    if(!f->event||!live_host.setup_done||!live_host.host_reader||!live_host.context||live_host.install_sent||
       wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)
        return ERROR_INVALID_STATE;
    if(live_host.sequence==~(UINT64)0)return ERROR_ARITHMETIC_OVERFLOW;
    packet.size=sizeof(packet);packet.version=MACDRV_REMOTE_PREPARED_PACKET_VERSION;
    packet.operation=MACDRV_REMOTE_INSTALL_PREPARED_HOST;packet.root=f->root;
    packet.root_epoch=live_host.epoch;packet.lease_id=live_host.lease;packet.host_ticket=live_host.ticket;
    packet.producer_pid=GetCurrentProcessId();packet.producer_generation=(UINT64)1<<33;
    packet.root_pid=live_host.identity.root_pid;packet.root_create_time=live_host.identity.root_birth;
    packet.sequence=live_host.sequence+1;
    pthread_mutex_lock(&remote_lock);
    h=host_status_lookup_locked(packet.host_ticket);p=h?h->producer:NULL;
    if(overlap.bound||!h||!p||!h->status_registered||h->state!=HOST_RESERVED||!p->active||p->request_inflight)
    {pthread_mutex_unlock(&remote_lock);return ERROR_INVALID_STATE;}
    InterlockedIncrement(&h->refs);InterlockedIncrement(&p->refs);
    overlap.host=h;overlap.producer=p;overlap.packet=packet;overlap.event=f->event;
    overlap.bound=overlap.armed=TRUE;
    pthread_mutex_unlock(&remote_lock);
    live_host.sequence=packet.sequence;
    live_host.install_sent=TRUE; /* cleanup must assume effects even if CreateThread fails */
    return ERROR_SUCCESS;
}
static DWORD overlap_run(void)
{
    struct macdrv_remote_packet packet;
    struct remote_producer *producer=NULL;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if(!overlap.bound||!overlap.armed||overlap.started)
    {pthread_mutex_unlock(&remote_lock);return ERROR_INVALID_STATE;}
    overlap.started=TRUE;packet=overlap.packet;
    pthread_mutex_unlock(&remote_lock);
    packet.producer_tid=GetCurrentThreadId();
    error=packet_producer_begin(&packet,&producer);
    if(!error)
    {
        error=install_prepared_host(producer,&packet);
        error=packet_producer_end(producer,packet.operation,error);
    }
    pthread_mutex_lock(&remote_lock);
    overlap.returned=TRUE;overlap.error=error;overlap.armed=FALSE;
    pthread_mutex_unlock(&remote_lock);
    return ERROR_SUCCESS; /* diagnostic dispatch, actual operation error is separate */
}
static DWORD overlap_finish(void)
{
    struct remote_host *h;
    struct remote_producer *p;
    pthread_mutex_lock(&remote_lock);
    h=overlap.host;p=overlap.producer;
    if(!h){pthread_mutex_unlock(&remote_lock);return ERROR_SUCCESS;}
    if((overlap.started&&!overlap.returned)||overlap.held||h->status_controls||h->model_jobs||
       h->retirement_pending||h->retirement_queued||h->status_registered||p->host_status_owners||p->request_inflight)
    {pthread_mutex_unlock(&remote_lock);return ERROR_IO_PENDING;}
    overlap.host=NULL;overlap.producer=NULL;overlap.armed=FALSE;overlap.event=0;
    pthread_mutex_unlock(&remote_lock);
    host_release(h);producer_release(p);
    return ERROR_SUCCESS;
}
NTSTATUS macdrv_host_overlap_fixture(void *args)
{
    struct overlap_frame *f=args;
    DWORD error=ERROR_SUCCESS;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<OVERLAP_ARM||f->operation>OVERLAP_FINISH)
        return STATUS_INVALID_PARAMETER;
    if(f->operation==OVERLAP_ARM)error=overlap_arm(f);
    else if(f->operation==OVERLAP_RUN)error=overlap_run();
    else if(f->operation==OVERLAP_FINISH)error=overlap_finish();
    overlap_snapshot(f);f->status=error;
    return STATUS_SUCCESS;
}
