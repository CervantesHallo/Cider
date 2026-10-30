#include "fixture.h"
static struct
{
    BOOL initialized,clearing[2],cleared[2],older_held,order_seen,order_good,schedule_failed;
    HWND root;
    UINT64 epoch,ids[2];
    UINT32 retries[2],last_retry[2],older_position,newer_position;
    DWORD acquire_error;
    NTSTATUS query_status,set_status;
} fifo_fixture;
static struct producer_setup *fifo_lookup_locked(UINT64 id)
{
    struct producer_setup *s;
    for(s=producer_setups;s;s=s->next)if(s->id==id)return s;
    return NULL;
}
/* Test schedule only: serialize real ordinary cleanup consumers, then hold
 * the older selected item until the independent client clears the newer one.
 * No ordinary queue decision is replaced by these hooks. */
static pthread_mutex_t fifo_schedule_lock=PTHREAD_MUTEX_INITIALIZER;
static void recovery_fifo_after_pop(struct producer_setup *s)
{
    LARGE_INTEGER begin,now,freq,delay;
    LONGLONG end;BOOL hold,ready;
    pthread_mutex_lock(&remote_lock);
    hold=s->id==fifo_fixture.ids[0]&&fifo_fixture.ids[1]&&!fifo_fixture.older_held;
    if(hold)fifo_fixture.older_held=TRUE;
    pthread_mutex_unlock(&remote_lock);
    if(!hold)return;
    if(NtQueryPerformanceCounter(&begin,&freq)||freq.QuadPart<=0||freq.QuadPart>0x7fffffffffffffffLL/3||begin.QuadPart<0||begin.QuadPart>0x7fffffffffffffffLL-3*freq.QuadPart)goto failed;
    end=begin.QuadPart+3*freq.QuadPart;delay.QuadPart=-100000;
    for(;;)
    {
        if(NtQueryPerformanceCounter(&now,NULL)||now.QuadPart<begin.QuadPart||now.QuadPart>=end)goto failed;
        pthread_mutex_lock(&remote_lock);ready=fifo_fixture.cleared[1];pthread_mutex_unlock(&remote_lock);
        if(NtQueryPerformanceCounter(&now,NULL)||now.QuadPart<begin.QuadPart||now.QuadPart>=end)goto failed;
        if(ready)return;
        if(NtDelayExecution(FALSE,&delay))goto failed;
    }
failed:
    pthread_mutex_lock(&remote_lock);fifo_fixture.schedule_failed=TRUE;pthread_mutex_unlock(&remote_lock);
}
/* remote_lock held AFTER the actual ordinary requeue. */
static void recovery_fifo_requeue_observed_locked(struct producer_setup *s)
{
    struct producer_setup *cursor,*newer;UINT32 position=0,old_pos=~(UINT32)0,new_pos=~(UINT32)0;
    if(fifo_fixture.order_seen||s->id!=fifo_fixture.ids[0]||!fifo_fixture.cleared[1]||fifo_fixture.cleared[0]||s->last_close!=STATUS_HANDLE_NOT_CLOSABLE)return;
    newer=fifo_lookup_locked(fifo_fixture.ids[1]);
    if(!newer||newer->busy||newer->active||!newer->cleanup_queued)return;
    for(cursor=setup_cleanup;cursor;cursor=cursor->cleanup_next,++position)
    {if(cursor==s)old_pos=position;if(cursor==newer)new_pos=position;}
    if(old_pos==~(UINT32)0||new_pos==~(UINT32)0)return;
    fifo_fixture.older_position=old_pos;fifo_fixture.newer_position=new_pos;
    fifo_fixture.order_good=new_pos<old_pos;fifo_fixture.order_seen=TRUE;
}
static void recovery_fifo_retry_observed(struct producer_setup *s)
{
    unsigned i;
    pthread_mutex_lock(&remote_lock);
    for(i=0;i<2;++i)if(fifo_fixture.ids[i]==s->id)
    {
        if(fifo_fixture.retries[i]!=~(UINT32)0)++fifo_fixture.retries[i];
        fifo_fixture.last_retry[i]=(UINT32)s->last_close;
    }
    pthread_mutex_unlock(&remote_lock);
}
static void fifo_snapshot_locked(struct recovery_fifo_fixture *f)
{
    struct producer_setup *s;
    struct remote_root *root=root_lookup_locked(fifo_fixture.root);
    f->older=fifo_fixture.ids[0];f->newer=fifo_fixture.ids[1];f->acquire_error=fifo_fixture.acquire_error;
    f->flags=(root&&root->epoch==fifo_fixture.epoch)?1:0;
    f->flags|=fifo_fixture.cleared[0]?2:0;f->flags|=fifo_fixture.cleared[1]?4:0;
    f->flags|=fifo_fixture.clearing[0]?8:0;f->flags|=fifo_fixture.clearing[1]?16:0;
    f->flags|=fifo_fixture.older_held?32:0;f->flags|=fifo_fixture.order_seen?64:0;f->flags|=fifo_fixture.order_good?128:0;f->flags|=fifo_fixture.schedule_failed?256:0;
    f->older_position=fifo_fixture.older_position;f->newer_position=fifo_fixture.newer_position;
    f->setups=f->queued=f->busy=f->active=0;
    for(s=producer_setups;s;s=s->next){if(f->setups!=~(UINT32)0)++f->setups;if(s->busy)++f->busy;if(s->active)++f->active;}
    for(s=setup_cleanup;s;s=s->cleanup_next)if(f->queued!=~(UINT32)0)++f->queued;
    s=fifo_lookup_locked(fifo_fixture.ids[0]);f->older_present=s!=NULL;f->older_last=s?(UINT32)s->last_close:fifo_fixture.last_retry[0];
    s=fifo_lookup_locked(fifo_fixture.ids[1]);f->newer_present=s!=NULL;f->newer_last=s?(UINT32)s->last_close:fifo_fixture.last_retry[1];
    f->older_retries=fifo_fixture.retries[0];f->newer_retries=fifo_fixture.retries[1];
    f->query_status=(UINT32)fifo_fixture.query_status;f->set_status=(UINT32)fifo_fixture.set_status;
}
static DWORD fifo_run(const struct recovery_fifo_fixture *f)
{
    struct producer_setup *s;
    unsigned i,matches;
    UINT64 id,lease;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if(fifo_fixture.initialized||producer_setups||producers||producer_watches||producer_cleanup||retirement_outstanding)
    {pthread_mutex_unlock(&remote_lock);return ERROR_BUSY;}
    fifo_fixture.initialized=TRUE;fifo_fixture.root=wine_server_ptr_handle(f->root);
    pthread_mutex_unlock(&remote_lock);
    fifo_fixture.epoch=macdrv_remote_root_epoch(fifo_fixture.root);
    if(!fifo_fixture.epoch)return ERROR_INVALID_WINDOW_HANDLE;
    for(i=0;i<2;++i)
    {
        lease=producer_acquire(fifo_fixture.root,fifo_fixture.epoch,GetCurrentProcessId(),GetCurrentThreadId(),(UINT64)1<<33,&error);
        pthread_mutex_lock(&remote_lock);
        fifo_fixture.acquire_error=error;matches=0;id=0;
        for(s=producer_setups;s;s=s->next)
            if(!s->active&&s->thread&&s->last_close==STATUS_HANDLE_NOT_CLOSABLE&&s->id!=fifo_fixture.ids[0]){++matches;id=s->id;}
        if(matches==1)fifo_fixture.ids[i]=id;
        pthread_mutex_unlock(&remote_lock);
        if(lease||error!=RtlNtStatusToDosError(STATUS_HANDLE_NOT_CLOSABLE)||matches!=1)return ERROR_INVALID_DATA;
    }
    return ERROR_SUCCESS;
}
static DWORD fifo_clear(unsigned index)
{
    struct producer_setup *s;
    OBJECT_HANDLE_FLAG_INFORMATION flags;
    HANDLE h;
    NTSTATUS q,status=STATUS_SUCCESS;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if(!fifo_fixture.ids[index]){pthread_mutex_unlock(&remote_lock);return ERROR_NOT_FOUND;}
    if(fifo_fixture.cleared[index]){pthread_mutex_unlock(&remote_lock);return ERROR_SUCCESS;}
    if(fifo_fixture.clearing[index]){pthread_mutex_unlock(&remote_lock);return ERROR_IO_PENDING;}
    s=fifo_lookup_locked(fifo_fixture.ids[index]);
    if(!s){pthread_mutex_unlock(&remote_lock);return ERROR_NOT_FOUND;}
    if(s->active||s->busy||!s->thread){pthread_mutex_unlock(&remote_lock);return ERROR_IO_PENDING;}
    h=s->thread;fifo_fixture.clearing[index]=TRUE;
    pthread_mutex_unlock(&remote_lock);
    /* Never use s after unlock. Actual protection preserves this H until the
     * first successful clear; the persistent latch prevents a second mutation. */
    q=NtQueryObject(h,ObjectHandleFlagInformation,&flags,sizeof(flags),NULL);
    if(!q&&flags.ProtectFromClose)
    {
        flags.ProtectFromClose=FALSE;status=NtSetInformationObject(h,ObjectHandleFlagInformation,&flags,sizeof(flags));
        error=status?RtlNtStatusToDosError(status):ERROR_SUCCESS;
    }
    else error=q?RtlNtStatusToDosError(q):ERROR_INVALID_DATA;
    pthread_mutex_lock(&remote_lock);
    fifo_fixture.query_status=q;fifo_fixture.set_status=status;
    if(!error)fifo_fixture.cleared[index]=TRUE;
    fifo_fixture.clearing[index]=FALSE;
    pthread_mutex_unlock(&remote_lock);
    return error;
}
NTSTATUS macdrv_recovery_fifo_fixture(void *args)
{
    struct recovery_fifo_fixture *f=args;
    DWORD error=ERROR_SUCCESS;
    if(!f||f->size!=sizeof(*f)||f->version!=2||f->operation<1||f->operation>3||f->reserved[0]||f->reserved[1]||
       (f->operation==3&&(f->target<1||f->target>2)))return STATUS_INVALID_PARAMETER;
    if(f->operation==1)error=f->root?fifo_run(f):ERROR_INVALID_WINDOW_HANDLE;
    else if(f->operation==3)error=fifo_clear(f->target-1);
    pthread_mutex_lock(&remote_lock);fifo_snapshot_locked(f);pthread_mutex_unlock(&remote_lock);
    f->status=error;return STATUS_SUCCESS;
}
