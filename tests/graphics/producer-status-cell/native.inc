/* DIAGNOSTIC ONLY. Hold a real admitted request pin before host dispatch.
 * No synthetic counts or terminal status; no actual Cocoa install reentry claim. */
#include "fixture.h"
static struct
{
    HWND root,child;
    UINT64 epoch,lease,sequence,generation;
    struct remote_producer *held;
    id_WineMetalSwapChain context;
    BOOL initialized;
} producer_cell_fixture_slot;
static struct producer_cell_fixture producer_cell_fixture_decision;
/* Called after the ordinary finalizer decision while its remote_lock is held.
 * Select the held pin's zero-other-resource epoch, never restate its verdict. */
static void producer_cell_fixture_decision_locked(struct remote_producer *producer, BOOL detached)
{
    if(producer_cell_fixture_decision.decision_seen||producer_cell_fixture_slot.held!=producer||
       producer->request_inflight!=1||!producer->completion_requested||producer->process||
       producer->cleanup_process||producer->cleanup_listed||producer->cleanup_inflight||producer->pending_retirements)return;
    producer_cell_fixture_decision.request_inflight=producer->request_inflight;
    producer_cell_fixture_decision.cleanup_inflight=producer->cleanup_inflight;
    producer_cell_fixture_decision.watched=producer->watched;
    producer_cell_fixture_decision.finalized=producer->finalized;
    producer_cell_fixture_decision.completion_seen=producer->completion_requested;
    producer_cell_fixture_decision.process_open=!!producer->process;
    producer_cell_fixture_decision.pending_models=producer->pending_retirements;
    producer_cell_fixture_decision.status_owner=producer->status_cell!=NULL;
    producer_cell_fixture_decision.status_cleanup=producer->status_cleanup;
    producer_cell_fixture_decision.decision_seen=1;
    producer_cell_fixture_decision.decision_detached=detached;
}
static void producer_cell_fixture_snapshot(struct producer_cell_fixture *frame)
{
    struct remote_producer *producer;
    frame->lease=producer_cell_fixture_slot.lease;frame->sequence=producer_cell_fixture_slot.sequence;
    frame->request_inflight=frame->cleanup_inflight=frame->watched=frame->finalized=0;
    frame->completion_seen=frame->process_open=frame->pending_models=0;
    frame->decision_seen=frame->decision_detached=0;
    frame->status_owner=frame->status_cleanup=0;
    pthread_mutex_lock(&remote_lock);
    producer=producer_cell_fixture_slot.held;
    if(!producer)
        for(producer=producer_watches;producer;producer=producer->watch_next)
            if(producer->id==producer_cell_fixture_slot.lease)break;
    if(producer)
    {
        frame->request_inflight=producer->request_inflight;frame->cleanup_inflight=producer->cleanup_inflight;
        frame->watched=producer->watched;frame->finalized=producer->finalized;
        frame->completion_seen=producer->completion_requested;frame->process_open=!!producer->process;
        frame->pending_models=producer->pending_retirements>~(UINT32)0?~(UINT32)0:producer->pending_retirements;
        frame->status_owner=producer->status_cell!=NULL;frame->status_cleanup=producer->status_cleanup;
    }
    if(frame->operation==2&&producer_cell_fixture_decision.decision_seen)
    {
        frame->request_inflight=producer_cell_fixture_decision.request_inflight;
        frame->cleanup_inflight=producer_cell_fixture_decision.cleanup_inflight;
        frame->watched=producer_cell_fixture_decision.watched;
        frame->finalized=producer_cell_fixture_decision.finalized;
        frame->completion_seen=producer_cell_fixture_decision.completion_seen;
        frame->process_open=producer_cell_fixture_decision.process_open;
        frame->pending_models=producer_cell_fixture_decision.pending_models;
        frame->status_owner=producer_cell_fixture_decision.status_owner;
        frame->status_cleanup=producer_cell_fixture_decision.status_cleanup;
        frame->decision_seen=producer_cell_fixture_decision.decision_seen;
        frame->decision_detached=producer_cell_fixture_decision.decision_detached;
    }
    pthread_mutex_unlock(&remote_lock);
}
NTSTATUS macdrv_producer_cell_fixture(void *args)
{
    struct producer_cell_fixture *frame=args;
    struct macdrv_remote_packet packet={0};struct remote_producer *producer;
    RECT bounds;DWORD error=ERROR_SUCCESS;
    if(!frame||frame->size!=sizeof(*frame)||frame->version!=1||frame->reserved||
       frame->operation<1||frame->operation>7)return STATUS_INVALID_PARAMETER;
    if(frame->operation==1)
    {
        if(producer_cell_fixture_slot.initialized){error=ERROR_ALREADY_EXISTS;goto done;}
        producer_cell_fixture_slot.initialized=TRUE;
        producer_cell_fixture_slot.root=wine_server_ptr_handle(frame->root);
        producer_cell_fixture_slot.child=wine_server_ptr_handle(frame->child);
        if(NtUserGetAncestor(producer_cell_fixture_slot.child,GA_ROOT)!=producer_cell_fixture_slot.root||
           !NtUserGetClientRect(producer_cell_fixture_slot.child,&bounds,NtUserGetWinMonitorDpi(producer_cell_fixture_slot.child,MDT_RAW_DPI)))
        {error=ERROR_INVALID_WINDOW_HANDLE;goto done;}
        producer_cell_fixture_slot.epoch=macdrv_remote_root_epoch(producer_cell_fixture_slot.root);
        producer_cell_fixture_slot.generation=(UINT64)1<<33;
        producer_cell_fixture_slot.lease=producer_acquire(producer_cell_fixture_slot.root,producer_cell_fixture_slot.epoch,
            GetCurrentProcessId(),GetCurrentThreadId(),producer_cell_fixture_slot.generation,&error);
        if(error)goto done;
        producer_cell_fixture_slot.context=macdrv_create_exported_metal_context(cgrect_from_rect(bounds),NULL);
        if(!producer_cell_fixture_slot.context){error=ERROR_NOT_ENOUGH_MEMORY;goto done;}
        packet.size=sizeof(packet);packet.version=MACDRV_REMOTE_PACKET_VERSION;packet.operation=MACDRV_REMOTE_INSTALL_HOST;
        packet.root=frame->root;packet.child=frame->child;packet.producer_pid=GetCurrentProcessId();packet.producer_tid=GetCurrentThreadId();
        packet.producer_generation=producer_cell_fixture_slot.generation;packet.root_epoch=producer_cell_fixture_slot.epoch;
        packet.lease_id=producer_cell_fixture_slot.lease;packet.context_id=macdrv_exported_metal_context_id(producer_cell_fixture_slot.context);
        packet.sequence=producer_cell_fixture_slot.sequence=((UINT64)1<<33)+11;
        if(!packet.context_id){error=ERROR_INVALID_HANDLE;goto done;}
        producer=NULL;
        error=packet_producer_begin(&packet,&producer);
        if(!error)
        {
            pthread_mutex_lock(&remote_lock);
            assert(!producer_cell_fixture_slot.held);
            producer_cell_fixture_slot.held=producer; /* transfer the one admitted request reference */
            pthread_mutex_unlock(&remote_lock);
        }
        if(!error)WARN("request-fixture actual admitted pin lease %#llx sequence %#llx context %u\n",
            (unsigned long long)packet.lease_id,(unsigned long long)packet.sequence,packet.context_id);
    }
    else if(frame->operation==3)
    {
        pthread_mutex_lock(&remote_lock);
        producer=producer_cell_fixture_slot.held;producer_cell_fixture_slot.held=NULL;
        pthread_mutex_unlock(&remote_lock);
        if(!producer){error=ERROR_INVALID_HANDLE;goto done;}
        error=packet_producer_end(producer,MACDRV_REMOTE_INSTALL_HOST,ERROR_SUCCESS);
    }
    else if(frame->operation==5||frame->operation==6)
    {
        HANDLE handle=NULL;
        OBJECT_HANDLE_FLAG_INFORMATION flags;
        NTSTATUS status;
        pthread_mutex_lock(&remote_lock);
        producer=producer_cell_fixture_slot.held;
        if(!producer)
            for(producer=producer_watches;producer;producer=producer->watch_next)
                if(producer->id==producer_cell_fixture_slot.lease)break;
        if(producer)
        {
            InterlockedIncrement(&producer->refs);
            if(producer->status_cell)handle=macdrv_status_cell_handle(producer->status_cell);
            if(handle)++producer->cleanup_inflight; /* excludes writer detachment during NT flag operations */
        }
        pthread_mutex_unlock(&remote_lock);
        if(!producer){error=ERROR_NOT_FOUND;goto done;}
        if(!handle){producer_release(producer);error=ERROR_IO_PENDING;goto done;}
        /* Setup protection while the real request blocks writer close. For
         * clearing, that same protection keeps the captured handle valid until
         * the NT flag operation itself; never access the cell after unlock. */
        status=NtQueryObject(handle,ObjectHandleFlagInformation,&flags,sizeof(flags),NULL);
        if(!status)
        {
            flags.ProtectFromClose=frame->operation==5;
            status=NtSetInformationObject(handle,ObjectHandleFlagInformation,&flags,sizeof(flags));
        }
        pthread_mutex_lock(&remote_lock);
        --producer->cleanup_inflight;
        pthread_mutex_unlock(&remote_lock);
        producer_release(producer);
        if(status)error=RtlNtStatusToDosError(status);
    }
    else if(frame->operation==4)
    {
        pthread_mutex_lock(&remote_lock);
        producer=producer_cell_fixture_slot.held;producer_cell_fixture_slot.held=NULL;
        pthread_mutex_unlock(&remote_lock);
        if(producer)packet_producer_end(producer,MACDRV_REMOTE_INSTALL_HOST,ERROR_SUCCESS);
        pthread_mutex_lock(&remote_lock);
        producer=producer_lookup_locked(producer_cell_fixture_slot.lease);
        if(producer)InterlockedIncrement(&producer->refs);
        pthread_mutex_unlock(&remote_lock);
        if(producer){error=producer_revoke(producer,TRUE);producer_release(producer);if(error)goto done;}
        retry_producer_cleanup();retry_one_retirement();producer_cell_fixture_snapshot(frame);
        if(frame->watched||frame->request_inflight||frame->cleanup_inflight||frame->pending_models)
        {error=ERROR_IO_PENDING;goto done;}
        if(producer_cell_fixture_slot.context)
        {macdrv_destroy_swapchain(producer_cell_fixture_slot.context);producer_cell_fixture_slot.context=NULL;}
    }
done:
    producer_cell_fixture_snapshot(frame);frame->status=error;return STATUS_SUCCESS;
}
