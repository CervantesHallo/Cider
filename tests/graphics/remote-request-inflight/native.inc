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
} request_fixture_slot;
static struct request_fixture request_fixture_decision;
/* Called after the ordinary finalizer decision while its remote_lock is held.
 * Select the held pin's zero-other-resource epoch, never restate its verdict. */
static void request_fixture_decision_locked(struct remote_producer *producer, BOOL detached)
{
    if(request_fixture_decision.decision_seen||request_fixture_slot.held!=producer||
       producer->request_inflight!=1||!producer->completion_requested||producer->process||
       producer->cleanup_process||producer->cleanup_listed||producer->cleanup_inflight||producer->pending_retirements)return;
    request_fixture_decision.request_inflight=producer->request_inflight;
    request_fixture_decision.cleanup_inflight=producer->cleanup_inflight;
    request_fixture_decision.watched=producer->watched;
    request_fixture_decision.finalized=producer->finalized;
    request_fixture_decision.completion_seen=producer->completion_requested;
    request_fixture_decision.process_open=!!producer->process;
    request_fixture_decision.pending_models=producer->pending_retirements;
    request_fixture_decision.decision_seen=1;
    request_fixture_decision.decision_detached=detached;
}
static void request_fixture_snapshot(struct request_fixture *frame)
{
    struct remote_producer *producer;
    frame->lease=request_fixture_slot.lease;frame->sequence=request_fixture_slot.sequence;
    frame->request_inflight=frame->cleanup_inflight=frame->watched=frame->finalized=0;
    frame->completion_seen=frame->process_open=frame->pending_models=0;
    frame->decision_seen=frame->decision_detached=0;
    pthread_mutex_lock(&remote_lock);
    producer=request_fixture_slot.held;
    if(!producer)
        for(producer=producer_watches;producer;producer=producer->watch_next)
            if(producer->id==request_fixture_slot.lease)break;
    if(producer)
    {
        frame->request_inflight=producer->request_inflight;frame->cleanup_inflight=producer->cleanup_inflight;
        frame->watched=producer->watched;frame->finalized=producer->finalized;
        frame->completion_seen=producer->completion_requested;frame->process_open=!!producer->process;
        frame->pending_models=producer->pending_retirements>~(UINT32)0?~(UINT32)0:producer->pending_retirements;
    }
    if(frame->operation==2&&request_fixture_decision.decision_seen)
    {
        frame->request_inflight=request_fixture_decision.request_inflight;
        frame->cleanup_inflight=request_fixture_decision.cleanup_inflight;
        frame->watched=request_fixture_decision.watched;
        frame->finalized=request_fixture_decision.finalized;
        frame->completion_seen=request_fixture_decision.completion_seen;
        frame->process_open=request_fixture_decision.process_open;
        frame->pending_models=request_fixture_decision.pending_models;
        frame->decision_seen=request_fixture_decision.decision_seen;
        frame->decision_detached=request_fixture_decision.decision_detached;
    }
    pthread_mutex_unlock(&remote_lock);
}
NTSTATUS macdrv_request_fixture(void *args)
{
    struct request_fixture *frame=args;
    struct macdrv_remote_packet packet={0};struct remote_producer *producer;
    RECT bounds;DWORD error=ERROR_SUCCESS;
    if(!frame||frame->size!=sizeof(*frame)||frame->version!=2||frame->reserved||
       frame->operation<1||frame->operation>4)return STATUS_INVALID_PARAMETER;
    if(frame->operation==1)
    {
        if(request_fixture_slot.initialized){error=ERROR_ALREADY_EXISTS;goto done;}
        request_fixture_slot.initialized=TRUE;
        request_fixture_slot.root=wine_server_ptr_handle(frame->root);
        request_fixture_slot.child=wine_server_ptr_handle(frame->child);
        if(NtUserGetAncestor(request_fixture_slot.child,GA_ROOT)!=request_fixture_slot.root||
           !NtUserGetClientRect(request_fixture_slot.child,&bounds,NtUserGetWinMonitorDpi(request_fixture_slot.child,MDT_RAW_DPI)))
        {error=ERROR_INVALID_WINDOW_HANDLE;goto done;}
        request_fixture_slot.epoch=macdrv_remote_root_epoch(request_fixture_slot.root);
        request_fixture_slot.generation=(UINT64)1<<33;
        request_fixture_slot.lease=producer_acquire(request_fixture_slot.root,request_fixture_slot.epoch,
            GetCurrentProcessId(),GetCurrentThreadId(),request_fixture_slot.generation,&error);
        if(error)goto done;
        request_fixture_slot.context=macdrv_create_exported_metal_context(cgrect_from_rect(bounds),NULL);
        if(!request_fixture_slot.context){error=ERROR_NOT_ENOUGH_MEMORY;goto done;}
        packet.size=sizeof(packet);packet.version=MACDRV_REMOTE_PACKET_VERSION;packet.operation=MACDRV_REMOTE_INSTALL_HOST;
        packet.root=frame->root;packet.child=frame->child;packet.producer_pid=GetCurrentProcessId();packet.producer_tid=GetCurrentThreadId();
        packet.producer_generation=request_fixture_slot.generation;packet.root_epoch=request_fixture_slot.epoch;
        packet.lease_id=request_fixture_slot.lease;packet.context_id=macdrv_exported_metal_context_id(request_fixture_slot.context);
        packet.sequence=request_fixture_slot.sequence=((UINT64)1<<33)+11;
        if(!packet.context_id){error=ERROR_INVALID_HANDLE;goto done;}
        producer=NULL;
        error=packet_producer_begin(&packet,&producer);
        if(!error)
        {
            pthread_mutex_lock(&remote_lock);
            assert(!request_fixture_slot.held);
            request_fixture_slot.held=producer; /* transfer the one admitted request reference */
            pthread_mutex_unlock(&remote_lock);
        }
        if(!error)WARN("request-fixture actual admitted pin lease %#llx sequence %#llx context %u\n",
            (unsigned long long)packet.lease_id,(unsigned long long)packet.sequence,packet.context_id);
    }
    else if(frame->operation==3)
    {
        pthread_mutex_lock(&remote_lock);
        producer=request_fixture_slot.held;request_fixture_slot.held=NULL;
        pthread_mutex_unlock(&remote_lock);
        if(!producer){error=ERROR_INVALID_HANDLE;goto done;}
        error=packet_producer_end(producer,MACDRV_REMOTE_INSTALL_HOST,ERROR_SUCCESS);
    }
    else if(frame->operation==4)
    {
        pthread_mutex_lock(&remote_lock);
        producer=request_fixture_slot.held;request_fixture_slot.held=NULL;
        pthread_mutex_unlock(&remote_lock);
        if(producer)packet_producer_end(producer,MACDRV_REMOTE_INSTALL_HOST,ERROR_SUCCESS);
        pthread_mutex_lock(&remote_lock);
        producer=producer_lookup_locked(request_fixture_slot.lease);
        if(producer)InterlockedIncrement(&producer->refs);
        pthread_mutex_unlock(&remote_lock);
        if(producer){error=producer_revoke(producer,TRUE);producer_release(producer);if(error)goto done;}
        retry_producer_cleanup();retry_one_retirement();request_fixture_snapshot(frame);
        if(frame->watched||frame->request_inflight||frame->cleanup_inflight||frame->pending_models)
        {error=ERROR_IO_PENDING;goto done;}
        if(request_fixture_slot.context)
        {macdrv_destroy_swapchain(request_fixture_slot.context);request_fixture_slot.context=NULL;}
    }
done:
    request_fixture_snapshot(frame);frame->status=error;return STATUS_SUCCESS;
}
