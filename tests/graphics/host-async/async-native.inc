#include "async.h"
#include <errno.h>
#include <time.h>
extern void macdrv_async_fixture_schedule_hold(void);
static pthread_cond_t async_condition=PTHREAD_COND_INITIALIZER;
static struct
{
    struct remote_host *host;
    pthread_t caller;
    BOOL used,scheduled,release;
    struct async_frame record;
} async_fixture;
static BOOL async_now(uint64_t *value)
{
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0||t.tv_nsec<0||t.tv_nsec>=1000000000||
       (uint64_t)t.tv_sec>(~(uint64_t)0-999999999)/1000000000)return FALSE;
    *value=(uint64_t)t.tv_sec*1000000000+t.tv_nsec;return TRUE;
}
static int async_wait_locked(const BOOL *flag,uint64_t *elapsed,uint32_t *timely)
{
    uint64_t begin,now,remaining;struct timespec timeout;int error=0;
    *elapsed=0;*timely=0;if(!async_now(&begin))return EINVAL;
    while(!*flag)
    {
        if(!async_now(&now)||now<begin)return EINVAL;
        if(now-begin>=4000000000ULL)return ETIMEDOUT;
        remaining=4000000000ULL-(now-begin);
        timeout.tv_sec=remaining/1000000000;timeout.tv_nsec=remaining%1000000000;
        error=pthread_cond_timedwait_relative_np(&async_condition,&remote_lock,&timeout);
        if(error&&error!=ETIMEDOUT&&error!=EINTR)return error;
    }
    if(!async_now(&now)||now<begin)return EINVAL;
    *elapsed=now-begin;if(*elapsed>=4000000000ULL)return ETIMEDOUT;
    *timely=1;return 0;
}
void macdrv_async_fixture_hold(bool main_thread)
{
    pthread_mutex_lock(&remote_lock);
    if(async_fixture.scheduled)
    {
        async_fixture.record.hold_started=1;
        if(main_thread)async_fixture.record.flags|=1;
        pthread_cond_broadcast(&async_condition);
        async_fixture.record.hold_error=async_wait_locked(&async_fixture.release,
            &async_fixture.record.hold_elapsed_ns,&async_fixture.record.hold_timely);
        async_fixture.record.hold_done=1;
        pthread_cond_broadcast(&async_condition);
    }
    pthread_mutex_unlock(&remote_lock);
}
static void async_fixture_admitted_locked(struct update_job *job)
{
    uint32_t index=async_fixture.record.admitted;
    if(job->host!=async_fixture.host||!pthread_equal(pthread_self(),async_fixture.caller)||
       async_fixture.record.run_complete||index>=2)return;
    async_fixture.record.serial[index]=job->serial;
    async_fixture.record.admission_jobs[index]=job->host->model_jobs;
    ++async_fixture.record.admitted;
}
static void async_fixture_validated_locked(struct update_job *job,BOOL accepted)
{
    unsigned int i;
    if(job->host!=async_fixture.host)return;
    for(i=0;i<async_fixture.record.admitted;++i)
        if(job->serial==async_fixture.record.serial[i])
        {
            if(pthread_equal(pthread_self(),async_fixture.caller))async_fixture.record.pre_validate[i]=accepted?2:1;
            else async_fixture.record.cocoa_validate[i]=accepted?2:1;
        }
}
static void async_fixture_disposed(struct remote_host *host,UINT64 serial)
{
    unsigned int i;
    pthread_mutex_lock(&remote_lock);
    if(host==async_fixture.host)
        for(i=0;i<async_fixture.record.admitted;++i)
            if(serial==async_fixture.record.serial[i])
            {
                ++async_fixture.record.disposed[i];async_fixture.record.dispose_jobs[i]=host->model_jobs;
            }
    pthread_mutex_unlock(&remote_lock);
}
static uint64_t async_double(double value)
{uint64_t bits;memcpy(&bits,&value,sizeof(bits));return bits;}
void macdrv_async_fixture_method(unsigned int cid,uint64_t ticket,uint32_t users,bool main_thread,CGRect frame,CGRect clip)
{
    pthread_mutex_lock(&remote_lock);
    if(async_fixture.host&&async_fixture.host->context_id==cid&&async_fixture.host->ticket==ticket)
    {
        ++async_fixture.record.method_seen;async_fixture.record.method_users=users;
        async_fixture.record.method_main=main_thread;
        async_fixture.record.frame[0]=async_double(frame.origin.x);async_fixture.record.frame[1]=async_double(frame.origin.y);
        async_fixture.record.frame[2]=async_double(frame.size.width);async_fixture.record.frame[3]=async_double(frame.size.height);
        async_fixture.record.clip[0]=async_double(clip.origin.x);async_fixture.record.clip[1]=async_double(clip.origin.y);
        async_fixture.record.clip[2]=async_double(clip.size.width);async_fixture.record.clip[3]=async_double(clip.size.height);
    }
    pthread_mutex_unlock(&remote_lock);
}
static DWORD async_run(void)
{
    struct remote_host *host;BOOL queued[2]={FALSE,FALSE},started;
    int error;
    if(!live_host.context||!live_host.window||!live_host.host_reader)return ERROR_INVALID_STATE;
    pthread_mutex_lock(&remote_lock);
    host=host_status_lookup_locked(live_host.ticket);
    if(async_fixture.used||!host||host->state!=HOST_INSTALLED||host->model_jobs)
    {pthread_mutex_unlock(&remote_lock);return ERROR_INVALID_STATE;}
    async_fixture.used=async_fixture.scheduled=TRUE;async_fixture.host=host;async_fixture.caller=pthread_self();
    InterlockedIncrement(&host->refs); /* lifetime observer only */
    pthread_mutex_unlock(&remote_lock);
    macdrv_async_fixture_schedule_hold();
    pthread_mutex_lock(&remote_lock);
    /* hold_started is a uint32_t wire fact; use a real BOOL wait predicate. */
    started=async_fixture.record.hold_started!=0;
    if(!started)
    {
        uint64_t begin,now,remaining;struct timespec timeout;
        error=async_now(&begin)?0:EINVAL;
        while(!error&&!async_fixture.record.hold_started)
        {
            if(!async_now(&now)||now<begin){error=EINVAL;break;}
            if(now-begin>=4000000000ULL){error=ETIMEDOUT;break;}
            remaining=4000000000ULL-(now-begin);timeout.tv_sec=remaining/1000000000;timeout.tv_nsec=remaining%1000000000;
            error=pthread_cond_timedwait_relative_np(&async_condition,&remote_lock,&timeout);
            if(error==ETIMEDOUT||error==EINTR)error=0;
        }
    }
    else error=0;
    pthread_mutex_unlock(&remote_lock);
    if(!error)
    {
        queued[0]=macdrv_remote_host_refresh(host->ticket);
        queued[1]=macdrv_remote_host_refresh(host->ticket);
    }
    pthread_mutex_lock(&remote_lock);
    async_fixture.record.queued[0]=queued[0];async_fixture.record.queued[1]=queued[1];
    async_fixture.record.frozen_jobs=host->model_jobs;
    async_fixture.record.frozen_held=async_fixture.record.hold_started&&!async_fixture.record.hold_done&&!async_fixture.release;
    async_fixture.record.latest_serial=host->update_serial;
    async_fixture.release=TRUE;async_fixture.record.run_complete=1;async_fixture.record.private_error=error;
    pthread_cond_broadcast(&async_condition);pthread_mutex_unlock(&remote_lock);
    return error||!queued[0]||!queued[1]?ERROR_INVALID_STATE:ERROR_SUCCESS;
}
static DWORD async_finish(void)
{
    struct remote_host *host;unsigned int i;
    pthread_mutex_lock(&remote_lock);host=async_fixture.host;
    if(!host){pthread_mutex_unlock(&remote_lock);return ERROR_SUCCESS;}
    if(!async_fixture.record.run_complete||(async_fixture.scheduled&&!async_fixture.record.hold_done)||host->model_jobs)
    {pthread_mutex_unlock(&remote_lock);return ERROR_IO_PENDING;}
    for(i=0;i<async_fixture.record.admitted;++i)
        if(!async_fixture.record.disposed[i]){pthread_mutex_unlock(&remote_lock);return ERROR_IO_PENDING;}
    async_fixture.host=NULL;async_fixture.record.finish_observed=1;
    pthread_mutex_unlock(&remote_lock);host_release(host);return ERROR_SUCCESS;
}
NTSTATUS macdrv_host_async_fixture(void *args)
{
    struct async_frame *f=args;struct macdrv_status_snapshot snapshot={0};DWORD error=ERROR_SUCCESS;uint32_t op;unsigned int i;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<ASYNC_RUN||f->operation>ASYNC_ABORT)return STATUS_INVALID_PARAMETER;
    for(i=0;i<4;++i)if(f->reserved[i])return STATUS_INVALID_PARAMETER;
    op=f->operation;
    if(wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)error=ERROR_INVALID_HANDLE;
    else if(op==ASYNC_RUN)error=async_run();
    else if(op==ASYNC_FINISH)error=async_finish();
    else if(op==ASYNC_ABORT)
    {pthread_mutex_lock(&remote_lock);async_fixture.release=TRUE;pthread_cond_broadcast(&async_condition);pthread_mutex_unlock(&remote_lock);}
    pthread_mutex_lock(&remote_lock);*f=async_fixture.record;f->size=sizeof(*f);f->version=1;f->operation=op;f->status=error;
    f->root=wine_server_user_handle(live_host.root);f->child=wine_server_user_handle(live_host.child);f->context=live_host.context_id;
    f->epoch=live_host.epoch;f->lease=live_host.lease;f->ticket=live_host.ticket;
    f->observer=async_fixture.host!=NULL;f->current_jobs=async_fixture.host?async_fixture.host->model_jobs:UINT32_MAX;
    pthread_mutex_unlock(&remote_lock);
    f->context_owned=live_host.context!=NULL;
    f->host_read=live_host.host_reader?macdrv_status_cell_read(live_host.host_reader,&snapshot):MACDRV_STATUS_READ_INVALID;
    f->host_valid=snapshot.values_valid;return STATUS_SUCCESS;
}
