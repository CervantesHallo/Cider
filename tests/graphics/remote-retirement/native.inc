/* DIAGNOSTIC ONLY, included after ordinary remote_coordinator.c. All objects
 * are real and process-local; no native pointer is returned to PE. */
#include "fixture.h"
extern bool macdrv_fixture_retirement_model(void *window, UINT context_id, UINT64 generation);
static struct
{
    HWND root, child;
    WineWindow *window;
    id_WineMetalSwapChain context;
    UINT context_id;
    UINT64 epoch, lease, ticket, generation;
    BOOL initialized, hold, revoked, submitted;
    struct remote_host *held;
} retirement_fixture_slot;

static bool retirement_fixture_hold(struct remote_host *host)
{
    BOOL held = FALSE;
    pthread_mutex_lock(&remote_lock);
    if (retirement_fixture_slot.hold && host->producer &&
        host->producer->id == retirement_fixture_slot.lease)
    {
        assert(!retirement_fixture_slot.held);
        retirement_fixture_slot.held = host;
        held = TRUE; /* the actual retired-list owner transfers to this slot */
    }
    pthread_mutex_unlock(&remote_lock);
    if (held) WARN("retirement-fixture held actual host ticket %#llx\n", (unsigned long long)host->ticket);
    return held;
}
static struct remote_producer *retirement_fixture_find_watch_locked(void)
{
    struct remote_producer *producer;
    if (retirement_fixture_slot.held) return retirement_fixture_slot.held->producer;
    for (producer = producer_watches; producer; producer = producer->watch_next)
        if (producer->id == retirement_fixture_slot.lease) return producer;
    return NULL;
}
static void retirement_fixture_snapshot(struct retirement_fixture *frame)
{
    struct remote_producer *producer;
    frame->context_id = retirement_fixture_slot.context_id;
    frame->lease = retirement_fixture_slot.lease;
    frame->ticket = retirement_fixture_slot.ticket;
    frame->pending = frame->watched = frame->finalized = frame->completion_seen = 0;
    pthread_mutex_lock(&remote_lock);
    producer = retirement_fixture_find_watch_locked();
    if (producer)
    {
        frame->pending = producer->pending_retirements > ~(UINT32)0 ? ~(UINT32)0 : producer->pending_retirements;
        frame->watched = producer->watched;
        frame->finalized = producer->finalized;
        frame->completion_seen = producer->completion_requested;
    }
    frame->retirement_total = retirement_outstanding > ~(UINT32)0 ? ~(UINT32)0 : retirement_outstanding;
    pthread_mutex_unlock(&remote_lock);
    frame->model_found = retirement_fixture_slot.window && retirement_fixture_slot.context_id &&
        macdrv_fixture_retirement_model(retirement_fixture_slot.window,
            retirement_fixture_slot.context_id, retirement_fixture_slot.ticket);
}
static void retirement_fixture_submit_held(void)
{
    struct remote_host *host;
    pthread_mutex_lock(&remote_lock);
    retirement_fixture_slot.hold = FALSE;
    host = retirement_fixture_slot.held;
    retirement_fixture_slot.held = NULL;
    if (host) retirement_fixture_slot.submitted = TRUE;
    pthread_mutex_unlock(&remote_lock);
    if (host) submit_retirement(host); /* original owner now belongs to Cocoa */
}
NTSTATUS macdrv_retirement_fixture(void *args)
{
    struct retirement_fixture *frame = args;
    struct remote_producer *producer;
    struct macdrv_win_data *data;
    RECT bounds;
    DWORD error = ERROR_SUCCESS;
    if (!frame || frame->size != sizeof(*frame) || frame->version != 1 ||
        frame->reserved || frame->operation < 1 || frame->operation > 5) return STATUS_INVALID_PARAMETER;
    if (frame->operation == 1)
    {
        if (retirement_fixture_slot.initialized) { error = ERROR_ALREADY_EXISTS; goto done; }
        retirement_fixture_slot.initialized = TRUE;
        retirement_fixture_slot.root = wine_server_ptr_handle(frame->root);
        retirement_fixture_slot.child = wine_server_ptr_handle(frame->child);
        if (NtUserGetAncestor(retirement_fixture_slot.child, GA_ROOT) != retirement_fixture_slot.root ||
            !NtUserGetClientRect(retirement_fixture_slot.child, &bounds,
                NtUserGetWinMonitorDpi(retirement_fixture_slot.child, MDT_RAW_DPI)))
        { error = ERROR_INVALID_WINDOW_HANDLE; goto done; }
        if (!(data = get_win_data(retirement_fixture_slot.root))) { error = ERROR_INVALID_WINDOW_HANDLE; goto done; }
        retirement_fixture_slot.window = macdrv_legacy_retain_object(data->cocoa_window);
        release_win_data(data);
        if (!retirement_fixture_slot.window ||
            !(retirement_fixture_slot.epoch = macdrv_remote_root_epoch(retirement_fixture_slot.root)))
        { error = ERROR_INVALID_HANDLE; goto done; }
        retirement_fixture_slot.generation = (UINT64)1 << 33;
        retirement_fixture_slot.lease = producer_acquire(retirement_fixture_slot.root,
            retirement_fixture_slot.epoch, GetCurrentProcessId(), GetCurrentThreadId(),
            retirement_fixture_slot.generation, &error);
        if (error) goto done;
        retirement_fixture_slot.context = macdrv_create_exported_metal_context(cgrect_from_rect(bounds), NULL);
        if (!retirement_fixture_slot.context ||
            !(retirement_fixture_slot.context_id = macdrv_exported_metal_context_id(retirement_fixture_slot.context)))
        { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
        retirement_fixture_slot.ticket = host_install(retirement_fixture_slot.root,
            retirement_fixture_slot.epoch, retirement_fixture_slot.child,
            retirement_fixture_slot.context_id, retirement_fixture_slot.lease,
            GetCurrentProcessId(), retirement_fixture_slot.generation);
        if (!retirement_fixture_slot.ticket) error = ERROR_INVALID_HANDLE;
    }
    else if (frame->operation == 2)
    {
        if (!retirement_fixture_slot.ticket || retirement_fixture_slot.revoked)
        { error = ERROR_INVALID_HANDLE; goto done; }
        pthread_mutex_lock(&remote_lock);
        producer = producer_lookup_locked(retirement_fixture_slot.lease);
        if (producer) InterlockedIncrement(&producer->refs);
        retirement_fixture_slot.hold = !!producer;
        pthread_mutex_unlock(&remote_lock);
        if (!producer) { error = ERROR_INVALID_HANDLE; goto done; }
        retirement_fixture_slot.revoked = TRUE;
        error = producer_revoke(producer, TRUE);
        producer_release(producer);
    }
    else if (frame->operation == 4)
        retirement_fixture_submit_held();
    else if (frame->operation == 5)
    {
        retirement_fixture_submit_held();
        pthread_mutex_lock(&remote_lock);
        producer = producer_lookup_locked(retirement_fixture_slot.lease);
        if (producer) InterlockedIncrement(&producer->refs);
        pthread_mutex_unlock(&remote_lock);
        if (producer)
        {
            error = producer_revoke(producer, TRUE);
            producer_release(producer);
            if (error) goto done;
        }
        retry_producer_cleanup(); retry_one_retirement();
        retirement_fixture_snapshot(frame);
        if (frame->watched || frame->retirement_total || frame->model_found)
        { error = ERROR_IO_PENDING; goto done; }
        if (retirement_fixture_slot.context)
        {
            macdrv_destroy_swapchain(retirement_fixture_slot.context);
            retirement_fixture_slot.context = NULL;
            retirement_fixture_slot.context_id = 0;
        }
        if (retirement_fixture_slot.window)
        {
            macdrv_legacy_release_object(retirement_fixture_slot.window);
            retirement_fixture_slot.window = NULL;
        }
    }
done:
    retirement_fixture_snapshot(frame);
    frame->status = error;
    return STATUS_SUCCESS; /* operation error remains in the owned fixed frame */
}
