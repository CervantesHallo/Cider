/* Legacy generation zero has no managed host/producer resource owner. Keep
 * the caller's original context/window until its separate checked retirement
 * and an independent actual model-absence probe both complete. */
#include "legacy.h"
extern void macdrv_legacy_handoff_probe(void *,unsigned int,uint64_t,struct legacy_frame *);
static struct
{
    BOOL used,attempted,create_result,drop_sent,drop_busy,drop_done,absent,cleanup_done;
    UINT32 callbacks,last_removed;
} legacy_owner;
static void legacy_drop_complete(void *context,bool removed)
{
    pthread_mutex_lock(&remote_lock);
    if(context==&legacy_owner&&legacy_owner.drop_busy)
    {
        if(legacy_owner.callbacks!=~(UINT32)0)++legacy_owner.callbacks;
        legacy_owner.last_removed=removed;
        legacy_owner.drop_busy=FALSE;
        if(removed)legacy_owner.drop_done=TRUE;
    }
    pthread_mutex_unlock(&remote_lock);
}
static DWORD legacy_create(void)
{
    BOOL result;
    if(legacy_owner.used||!live_host.setup_done||!live_host.context||!live_host.window||live_host.prepare_sent)
        return ERROR_INVALID_STATE;
    legacy_owner.used=legacy_owner.attempted=TRUE; /* before unknown import effects */
    result=macdrv_window_create_remote_host(live_host.window,live_host.context_id,0,
        CGRectNull,CGRectNull,0.0,false);
    legacy_owner.create_result=result;
    return result?ERROR_SUCCESS:ERROR_INVALID_HANDLE;
}
static DWORD legacy_collect_drop(void)
{
    BOOL done,busy,absent;
    struct legacy_frame observation={0};
    if(!legacy_owner.attempted)return ERROR_SUCCESS;
    pthread_mutex_lock(&remote_lock);
    done=legacy_owner.drop_done;busy=legacy_owner.drop_busy;absent=legacy_owner.absent;
    if(!done&&!busy)
    {
        legacy_owner.drop_busy=TRUE;legacy_owner.drop_sent=TRUE;
    }
    pthread_mutex_unlock(&remote_lock);
    if(absent)return ERROR_SUCCESS;
    if(!done)
    {
        if(!busy)macdrv_window_release_remote_host_deferred_checked(live_host.window,
            live_host.context_id,0,legacy_drop_complete,&legacy_owner);
        return ERROR_IO_PENDING;
    }
    if(!live_host.window)return ERROR_IO_PENDING; /* never infer model absence from a lost observer */
    macdrv_legacy_handoff_probe(live_host.window,live_host.context_id,live_host.ticket,&observation);
    if(!observation.cocoa_valid||observation.legacy_attached||observation.legacy_ledger||observation.legacy_indexed)
        return ERROR_IO_PENDING;
    pthread_mutex_lock(&remote_lock);legacy_owner.absent=TRUE;pthread_mutex_unlock(&remote_lock);
    return ERROR_SUCCESS;
}
static void legacy_snapshot(struct legacy_frame *f)
{
    struct macdrv_status_snapshot host={0},producer={0};
    f->context=live_host.context_id;f->epoch=live_host.epoch;f->lease=live_host.lease;
    f->ticket=live_host.ticket;f->sequence=live_host.sequence;
    f->flags=(live_host.setup_done?LIVE_SETUP_DONE:0)|(live_host.prepare_sent?LIVE_PREPARE_SENT:0)|
        (live_host.install_sent?LIVE_INSTALL_SENT:0)|(live_host.cancel_sent?LIVE_CANCEL_SENT:0)|
        (live_host.release_sent?LIVE_RELEASE_SENT:0)|(live_host.context_released?LIVE_CONTEXT_RELEASED:0);
    f->context_owned=live_host.context!=NULL;f->window_owned=live_host.window!=NULL;
    f->host_reader_owned=live_host.host_reader!=NULL;f->producer_reader_owned=live_host.producer_reader!=NULL;
    f->host_read=live_host.host_reader?macdrv_status_cell_read(live_host.host_reader,&host):MACDRV_STATUS_READ_INVALID;
    f->producer_read=live_host.producer_reader?macdrv_status_cell_read(live_host.producer_reader,&producer):MACDRV_STATUS_READ_INVALID;
    f->host_values_valid=host.values_valid;f->producer_values_valid=producer.values_valid;
    f->last_reply=live_host.reply_status;f->close_status=live_host.close_status;
    f->legacy_attempted=f->create_called=legacy_owner.attempted;f->create_result=legacy_owner.create_result;
    pthread_mutex_lock(&remote_lock);
    f->drop_sent=legacy_owner.drop_sent;f->drop_busy=legacy_owner.drop_busy;f->drop_done=legacy_owner.drop_done;
    f->drop_callbacks=legacy_owner.callbacks;f->drop_result=legacy_owner.last_removed;
    f->legacy_absent=legacy_owner.absent;f->cleanup_done=legacy_owner.cleanup_done;
    pthread_mutex_unlock(&remote_lock);
    f->cocoa_valid=0;
    f->legacy_attached=f->legacy_ledger=f->legacy_indexed=UINT32_MAX;
    f->managed_attached=f->managed_ledger=f->managed_indexed=f->managed_flag=UINT32_MAX;
    if(live_host.window&&live_host.context_id)
        macdrv_legacy_handoff_probe(live_host.window,live_host.context_id,live_host.ticket,f);
}
NTSTATUS macdrv_host_legacy_fixture(void *args)
{
    struct legacy_frame *f=args;
    struct live_host_fixture cleanup={0};
    DWORD error=ERROR_SUCCESS;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<LEGACY_CREATE||f->operation>LEGACY_CLEANUP||
       f->reserved[0]||f->reserved[1]||f->reserved[2]||f->reserved[3])return STATUS_INVALID_PARAMETER;
    if(wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)
        error=ERROR_INVALID_HANDLE;
    else if(f->operation==LEGACY_CREATE)error=legacy_create();
    else if(f->operation==LEGACY_DROP)error=legacy_collect_drop();
    else if(f->operation==LEGACY_CLEANUP&&!legacy_owner.cleanup_done)
    {
        error=legacy_collect_drop();
        if(!error)
        {
            error=live_host_cleanup(&cleanup); /* existing managed/producer SAFE and NT obligations */
            if(!error&&!live_host.context&&!live_host.host_reader&&!live_host.producer_reader&&!live_host.window)
                legacy_owner.cleanup_done=TRUE;
        }
    }
    legacy_snapshot(f);f->status=error;
    return STATUS_SUCCESS;
}
