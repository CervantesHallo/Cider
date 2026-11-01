/* Two unmanaged generations have independent actual retirement obligations.
 * The original caller's context/window are held; no rescue retain is added. */
#include "generation.h"
extern bool macdrv_generation_arm(void *,unsigned int,uint64_t,uint64_t);
extern void macdrv_generation_observe(void *,unsigned int,uint64_t,uint64_t,struct generation_frame *);
extern void macdrv_generation_disarm(void *);
struct generation_owner_item { uint64_t generation;BOOL busy,done;UINT32 callbacks; };
static struct
{
    BOOL used,outer_called,outer_result,absent,disarmed,cleanup_done;
    UINT32 mask;
    struct generation_owner_item items[2];
} generation_owner;
static void generation_complete(void *context,bool removed)
{
    unsigned int i;
    pthread_mutex_lock(&remote_lock);
    for(i=0;i<2;++i)
        if(context==&generation_owner.items[i]&&generation_owner.items[i].busy)
        {
            generation_owner.items[i].busy=FALSE;
            if(generation_owner.items[i].callbacks!=~(UINT32)0)++generation_owner.items[i].callbacks;
            if(removed)generation_owner.items[i].done=TRUE;
        }
    pthread_mutex_unlock(&remote_lock);
}
static DWORD generation_run(void)
{
    if(generation_owner.used||!live_host.context||!live_host.window||!live_host.setup_done||live_host.prepare_sent)
        return ERROR_INVALID_STATE;
    generation_owner.used=TRUE;generation_owner.items[0].generation=(uint64_t)1<<33;
    generation_owner.items[1].generation=((uint64_t)1<<33)+1;
    generation_owner.mask=3; /* before any possible outer/nested import effects */
    if(!macdrv_generation_arm(live_host.window,live_host.context_id,generation_owner.items[0].generation,
        generation_owner.items[1].generation))return ERROR_INVALID_STATE;
    generation_owner.outer_called=TRUE;
    generation_owner.outer_result=macdrv_window_create_remote_host(live_host.window,live_host.context_id,
        generation_owner.items[0].generation,CGRectMake(12,18,160,80),CGRectMake(20,22,100,50),7,true);
    return ERROR_SUCCESS; /* procedure completion, not outer import success */
}
static DWORD generation_collect(void)
{
    BOOL schedule[2]={FALSE,FALSE},done=TRUE;
    struct generation_frame observation={0};unsigned int i;
    if(!generation_owner.mask||generation_owner.absent)return ERROR_SUCCESS;
    pthread_mutex_lock(&remote_lock);
    for(i=0;i<2;++i)
        if((generation_owner.mask&(1u<<i))&&!generation_owner.items[i].done)
        {
            done=FALSE;
            if(!generation_owner.items[i].busy){generation_owner.items[i].busy=TRUE;schedule[i]=TRUE;}
        }
    pthread_mutex_unlock(&remote_lock);
    for(i=0;i<2;++i)
        if(schedule[i])macdrv_window_release_remote_host_deferred_checked(live_host.window,live_host.context_id,
            generation_owner.items[i].generation,generation_complete,&generation_owner.items[i]);
    if(!done||!live_host.window)return ERROR_IO_PENDING;
    macdrv_generation_observe(live_host.window,live_host.context_id,generation_owner.items[0].generation,
        generation_owner.items[1].generation,&observation);
    if(!observation.cocoa_valid||observation.final_old_attached||observation.final_old_ledger||observation.final_old_indexed||
       observation.final_new_attached||observation.final_new_ledger||observation.final_new_indexed)return ERROR_IO_PENDING;
    generation_owner.absent=TRUE;
    return ERROR_SUCCESS;
}
static void generation_snapshot(struct generation_frame *f)
{
    struct macdrv_status_snapshot producer={0};unsigned int i;
    f->context=live_host.context_id;f->epoch=live_host.epoch;f->lease=live_host.lease;
    f->old_generation=generation_owner.items[0].generation;f->new_generation=generation_owner.items[1].generation;
    f->flags=(live_host.setup_done?LIVE_SETUP_DONE:0)|(live_host.release_sent?LIVE_RELEASE_SENT:0)|
        (live_host.context_released?LIVE_CONTEXT_RELEASED:0)|(generation_owner.disarmed?64:0);
    f->outer_called=generation_owner.outer_called;f->outer_result=generation_owner.outer_result;
    f->context_owned=live_host.context!=NULL;f->window_owned=live_host.window!=NULL;
    f->producer_reader_owned=live_host.producer_reader!=NULL;
    f->producer_read=live_host.producer_reader?macdrv_status_cell_read(live_host.producer_reader,&producer):MACDRV_STATUS_READ_INVALID;
    f->producer_valid=producer.values_valid;f->close_status=live_host.close_status;
    pthread_mutex_lock(&remote_lock);
    f->owner_mask=generation_owner.mask;f->busy_mask=f->done_mask=0;
    for(i=0;i<2;++i){if(generation_owner.items[i].busy)f->busy_mask|=1u<<i;if(generation_owner.items[i].done)f->done_mask|=1u<<i;}
    f->callbacks_old=generation_owner.items[0].callbacks;f->callbacks_new=generation_owner.items[1].callbacks;
    pthread_mutex_unlock(&remote_lock);
    f->absent=generation_owner.absent;f->cleanup_done=generation_owner.cleanup_done;
    f->cocoa_valid=0;f->final_old_attached=f->final_old_ledger=f->final_old_indexed=UINT32_MAX;
    f->final_new_attached=f->final_new_ledger=f->final_new_indexed=UINT32_MAX;
    if(live_host.window&&generation_owner.mask)
        macdrv_generation_observe(live_host.window,live_host.context_id,f->old_generation,f->new_generation,f);
}
NTSTATUS macdrv_generation_fixture(void *args)
{
    struct generation_frame *f=args;struct live_host_fixture cleanup={0};DWORD error=ERROR_SUCCESS;unsigned int i;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<GENERATION_RUN||f->operation>GENERATION_CLEANUP)return STATUS_INVALID_PARAMETER;
    for(i=0;i<10;++i)if(f->reserved[i])return STATUS_INVALID_PARAMETER;
    if(wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)error=ERROR_INVALID_HANDLE;
    else if(f->operation==GENERATION_RUN)error=generation_run();
    else if(f->operation==GENERATION_CLEANUP&&!generation_owner.cleanup_done)
    {
        error=generation_collect();
        if(!error)
        {
            if(live_host.window&&!generation_owner.disarmed)
            {macdrv_generation_disarm(live_host.window);generation_owner.disarmed=TRUE;}
            error=live_host_cleanup(&cleanup);
            if(!error&&!live_host.context&&!live_host.producer_reader&&!live_host.window)generation_owner.cleanup_done=TRUE;
        }
    }
    generation_snapshot(f);f->status=error;
    return STATUS_SUCCESS;
}
