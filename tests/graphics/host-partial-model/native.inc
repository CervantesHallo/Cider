/* Include after the ordinary host-job-fence-v3 coordinator. No caller-frame
 * pointer survives a control. The only asynchronous fixture ownership transfer
 * is the real retired-list/FIFO owner into held, under remote_lock. */
#include "fixture.h"
extern bool macdrv_partial_model_fixture_arm(void *, unsigned int);
extern void macdrv_partial_model_fixture_observe(void *, unsigned int, uint64_t, struct partial_model_fixture *);
extern void macdrv_partial_model_fixture_disarm(void *);
static struct
{
    BOOL initialized, install_started, setup_returned, hold_enabled, held_once, revoked, submitted;
    BOOL context_released, release_attempted, finished;
    HWND root, child;
    WineWindow *window;                  /* one explicit retained object */
    id_WineMetalSwapChain context;        /* actual exported context owner */
    struct remote_producer *producer;     /* independent observation reference */
    struct remote_host *held;             /* transferred ordinary retirement ref */
    UINT context_id;
    UINT64 epoch, lease, ticket, generation, birth, install_result;
    DWORD revoke_error;
} partial_model_native;

/* Add after submit_retirement's `if (deferred) return;`, before Cocoa dispatch.
 * It consumes/transfers the original retirement owner, not an extra fake ref. */
static BOOL partial_model_fixture_hold_retirement(struct remote_host *host)
{
    BOOL held = FALSE;
    pthread_mutex_lock(&remote_lock);
    if (partial_model_native.hold_enabled && !partial_model_native.held &&
        host->producer == partial_model_native.producer && host->producer &&
        host->producer->id == partial_model_native.lease && !host->model_jobs &&
        host->retirement_pending && !host->retirement_queued)
    {
        partial_model_native.held = host;
        partial_model_native.ticket = host->ticket;
        partial_model_native.held_once = TRUE;
        partial_model_native.hold_enabled = FALSE;
        held = TRUE;
    }
    pthread_mutex_unlock(&remote_lock);
    return held;
}
static void partial_native_count(UINT32 *n)
{
    if (*n != ~(UINT32)0) ++*n;
}
static void partial_native_snapshot(struct partial_model_fixture *f)
{
    struct remote_producer *p, *cursor;
    struct producer_setup *setup;
    WineWindow *window;
    UINT context_id;
    UINT64 ticket;
    UINT32 flags = 0;
    pthread_mutex_lock(&remote_lock);
    if (partial_model_native.initialized) flags |= PARTIAL_MODEL_INITIALIZED;
    if (partial_model_native.setup_returned) flags |= PARTIAL_MODEL_SETUP_RETURNED;
    if (partial_model_native.hold_enabled) flags |= PARTIAL_MODEL_HOLD_ARMED;
    if (partial_model_native.held_once) flags |= PARTIAL_MODEL_HELD_ONCE;
    if (partial_model_native.revoked) flags |= PARTIAL_MODEL_REVOKE_CALLED;
    if (partial_model_native.submitted) flags |= PARTIAL_MODEL_SUBMIT_CALLED;
    if (partial_model_native.context_released) flags |= PARTIAL_MODEL_CONTEXT_RELEASED;
    if (partial_model_native.finished) flags |= PARTIAL_MODEL_FINISHED;
    f->flags = flags;
    f->context_id = context_id = partial_model_native.context_id;
    f->root_epoch = partial_model_native.epoch;
    f->lease = partial_model_native.lease;
    f->ticket = ticket = partial_model_native.ticket;
    f->generation = partial_model_native.generation;
    f->producer_birth = partial_model_native.birth;
    f->install_result = partial_model_native.install_result;
    f->revoke_error = partial_model_native.revoke_error;
    f->held = partial_model_native.held != NULL;
    f->model_jobs = partial_model_native.held ? partial_model_native.held->model_jobs : ~(UINT32)0;
    f->context_owned = partial_model_native.context != NULL;
    f->window_owned = (window = partial_model_native.window) != NULL;
    f->producer_owned = (p = partial_model_native.producer) != NULL;
    f->release_attempted = partial_model_native.release_attempted;
    f->producer_valid = p != NULL;
    f->active = p ? p->active : ~(UINT32)0;
    f->watched = p ? p->watched : ~(UINT32)0;
    f->finalized = p ? p->finalized : ~(UINT32)0;
    f->completion_seen = p ? p->completion_requested : ~(UINT32)0;
    f->process_open = p ? !!(p->process || p->cleanup_process) : ~(UINT32)0;
    f->cleanup_inflight = p ? p->cleanup_inflight : ~(UINT32)0;
    f->request_inflight = p ? p->request_inflight : ~(UINT32)0;
    f->pending = p ? (p->pending_retirements > ~(UINT32)0 ? ~(UINT32)0 : (UINT32)p->pending_retirements) : ~(UINT32)0;
    f->retirement_total = retirement_outstanding > ~(UINT32)0 ? ~(UINT32)0 : (UINT32)retirement_outstanding;
    f->native_producers = f->native_watches = f->native_setups = f->native_cleanup = 0;
    for (cursor = producers; cursor; cursor = cursor->next) partial_native_count(&f->native_producers);
    for (cursor = producer_watches; cursor; cursor = cursor->watch_next) partial_native_count(&f->native_watches);
    for (cursor = producer_cleanup; cursor; cursor = cursor->cleanup_next) partial_native_count(&f->native_cleanup);
    for (setup = producer_setups; setup; setup = setup->next) partial_native_count(&f->native_setups);
    for (setup = setup_cleanup; setup; setup = setup->cleanup_next) partial_native_count(&f->native_cleanup);
    pthread_mutex_unlock(&remote_lock);
    /* Private controls serialize; window remains owned across this synchronous
     * Cocoa sample. Neither registry/window lock is held while entering Cocoa. */
    f->cocoa_valid = 0; f->cocoa_error = 1;
    f->attached = f->indexed = f->ledger = ~(UINT32)0;
    f->witness_seen = f->witness_valid = 0;
    f->witness_removed = f->witness_attached = f->witness_indexed = f->witness_ledger = ~(UINT32)0;
    f->witness_error = ~(UINT32)0;
    /* With no remaining window, the helper reports live probe unavailable but
     * can still copy the actual immutable checked-removal witness. */
    macdrv_partial_model_fixture_observe(window, context_id, ticket, f);
}
static void partial_native_submit(void)
{
    struct remote_host *host;
    pthread_mutex_lock(&remote_lock);
    partial_model_native.hold_enabled = FALSE;
    host = partial_model_native.held;
    partial_model_native.held = NULL;
    if (host) partial_model_native.submitted = TRUE;
    pthread_mutex_unlock(&remote_lock);
    if (host) submit_retirement(host); /* consumes transferred owner; no access after */
}
static DWORD partial_native_revoke(void)
{
    struct remote_producer *p;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if (partial_model_native.revoked)
    { error = partial_model_native.revoke_error; pthread_mutex_unlock(&remote_lock); return error; }
    p = partial_model_native.producer;
    if (!p) { pthread_mutex_unlock(&remote_lock); return ERROR_INVALID_HANDLE; }
    partial_model_native.revoked = TRUE; /* actual invocation, not a success claim */
    pthread_mutex_unlock(&remote_lock);
    error = producer_revoke(p, TRUE);
    pthread_mutex_lock(&remote_lock);
    partial_model_native.revoke_error = error;
    pthread_mutex_unlock(&remote_lock);
    return error;
}
static DWORD partial_native_setup(const struct partial_model_fixture *f)
{
    struct macdrv_win_data *data;
    struct remote_producer *p;
    WineWindow *window;
    id_WineMetalSwapChain context;
    UINT context_id;
    UINT64 epoch, lease, result;
    HWND root = wine_server_ptr_handle(f->root), child = wine_server_ptr_handle(f->child);
    RECT bounds;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if (partial_model_native.initialized)
    { pthread_mutex_unlock(&remote_lock); return ERROR_ALREADY_EXISTS; }
    partial_model_native.initialized = TRUE;
    partial_model_native.root = root; partial_model_native.child = child;
    partial_model_native.generation = PARTIAL_MODEL_HIGH;
    pthread_mutex_unlock(&remote_lock);
    if (!root || !child || root == child || NtUserGetAncestor(child, GA_ROOT) != root ||
        !NtUserGetClientRect(child, &bounds, NtUserGetWinMonitorDpi(child, MDT_RAW_DPI)))
        return ERROR_INVALID_WINDOW_HANDLE;
    if (!(data = get_win_data(root))) return ERROR_INVALID_WINDOW_HANDLE;
    window = macdrv_legacy_retain_object(data->cocoa_window);
    release_win_data(data);
    pthread_mutex_lock(&remote_lock); partial_model_native.window = window; pthread_mutex_unlock(&remote_lock);
    if (!window || !(epoch = macdrv_remote_root_epoch(root))) return ERROR_INVALID_HANDLE;
    pthread_mutex_lock(&remote_lock); partial_model_native.epoch = epoch; pthread_mutex_unlock(&remote_lock);
    lease = producer_acquire(root, epoch, GetCurrentProcessId(), GetCurrentThreadId(), PARTIAL_MODEL_HIGH, &error);
    pthread_mutex_lock(&remote_lock);
    partial_model_native.lease = lease;
    p = lease ? producer_lookup_locked(lease) : NULL;
    if (p)
    {
        InterlockedIncrement(&p->refs);
        partial_model_native.producer = p;
        partial_model_native.birth = p->birth;
    }
    pthread_mutex_unlock(&remote_lock);
    if (error || !p) return error ? error : ERROR_INVALID_HANDLE;
    context = macdrv_create_exported_metal_context(cgrect_from_rect(bounds), NULL);
    pthread_mutex_lock(&remote_lock); partial_model_native.context = context; pthread_mutex_unlock(&remote_lock);
    if (!context || !(context_id = macdrv_exported_metal_context_id(context))) return ERROR_NOT_ENOUGH_MEMORY;
    pthread_mutex_lock(&remote_lock); partial_model_native.context_id = context_id; pthread_mutex_unlock(&remote_lock);
    if (!macdrv_partial_model_fixture_arm(window, context_id)) return ERROR_INVALID_STATE;
    pthread_mutex_lock(&remote_lock);
    partial_model_native.hold_enabled = TRUE;
    partial_model_native.install_started = TRUE;
    pthread_mutex_unlock(&remote_lock);
    result = host_install(root, epoch, child, context_id, lease, GetCurrentProcessId(), PARTIAL_MODEL_HIGH);
    pthread_mutex_lock(&remote_lock);
    partial_model_native.install_result = result;
    partial_model_native.setup_returned = TRUE;
    pthread_mutex_unlock(&remote_lock);
    return ERROR_SUCCESS; /* harness completed; actual install_result is separate */
}
static BOOL partial_native_empty(const struct partial_model_fixture *f)
{
    return !f->held && !f->retirement_total && !f->native_producers && !f->native_watches &&
        !f->native_setups && !f->native_cleanup && (!f->producer_valid ||
        (!f->active && !f->watched && f->finalized == 1 && !f->process_open &&
         !f->cleanup_inflight && !f->request_inflight && !f->pending));
}
static DWORD partial_native_cleanup(struct partial_model_fixture *f)
{
    id_WineMetalSwapChain context;
    BOOL has_producer, revoked, install_started;
    partial_native_submit(); /* always release the real held owner, even on failure */
    pthread_mutex_lock(&remote_lock);
    has_producer = partial_model_native.producer != NULL; revoked = partial_model_native.revoked;
    pthread_mutex_unlock(&remote_lock);
    if (has_producer && !revoked) partial_native_revoke();
    partial_native_snapshot(f);
    /* No release based on SAFE/removed alone. Negative deliberately retains
     * actual context/window/producer pins if the real object/ledger remains. */
    if (!partial_native_empty(f)) return ERROR_IO_PENDING;
    pthread_mutex_lock(&remote_lock);
    context = partial_model_native.context;
    install_started = partial_model_native.install_started;
    pthread_mutex_unlock(&remote_lock);
    if (context && install_started && (!f->cocoa_valid || f->attached || f->indexed || f->ledger))
        return ERROR_IO_PENDING;
    if (context)
    {
        pthread_mutex_lock(&remote_lock);
        if (partial_model_native.release_attempted)
        { pthread_mutex_unlock(&remote_lock); return ERROR_INVALID_STATE; }
        partial_model_native.release_attempted = TRUE;
        pthread_mutex_unlock(&remote_lock);
        macdrv_destroy_swapchain(context); /* actual owned exported context release */
        pthread_mutex_lock(&remote_lock);
        partial_model_native.context = NULL;
        partial_model_native.context_released = TRUE;
        pthread_mutex_unlock(&remote_lock);
    }
    return ERROR_SUCCESS;
}
static DWORD partial_native_finish(struct partial_model_fixture *f)
{
    struct remote_producer *p;
    struct remote_root *root;
    WineWindow *window;
    partial_native_snapshot(f);
    pthread_mutex_lock(&remote_lock);
    root = root_lookup_locked(partial_model_native.root);
    if (!partial_native_empty(f) || partial_model_native.context ||
        (root && root->epoch == partial_model_native.epoch))
    { pthread_mutex_unlock(&remote_lock); return ERROR_BUSY; }
    p = partial_model_native.producer; partial_model_native.producer = NULL;
    window = partial_model_native.window; partial_model_native.window = NULL;
    partial_model_native.finished = TRUE;
    pthread_mutex_unlock(&remote_lock);
    if (window) macdrv_partial_model_fixture_disarm(window);
    if (p) producer_release(p); /* no NT handles remain */
    if (window) macdrv_legacy_release_object(window);
    return ERROR_SUCCESS;
}
NTSTATUS macdrv_partial_model_fixture(void *args)
{
    struct partial_model_fixture *f = args;
    DWORD error = ERROR_SUCCESS;
    if (!f || f->size != sizeof(*f) || f->version != PARTIAL_MODEL_VERSION || f->reserved ||
        f->operation < PARTIAL_MODEL_SETUP || f->operation > PARTIAL_MODEL_FINISH || !f->root || !f->child)
        return STATUS_INVALID_PARAMETER;
    if (f->operation == PARTIAL_MODEL_SETUP) error = partial_native_setup(f);
    else
    {
        pthread_mutex_lock(&remote_lock);
        if (!partial_model_native.initialized || wine_server_ptr_handle(f->root) != partial_model_native.root ||
            wine_server_ptr_handle(f->child) != partial_model_native.child) error = ERROR_INVALID_HANDLE;
        pthread_mutex_unlock(&remote_lock);
        if (!error && f->operation == PARTIAL_MODEL_REVOKE) error = partial_native_revoke();
        if (!error && f->operation == PARTIAL_MODEL_SUBMIT_HELD) partial_native_submit();
        if (!error && f->operation == PARTIAL_MODEL_CLEANUP) error = partial_native_cleanup(f);
        if (!error && f->operation == PARTIAL_MODEL_FINISH) error = partial_native_finish(f);
    }
    partial_native_snapshot(f);
    f->status = error;
    return STATUS_SUCCESS;
}
