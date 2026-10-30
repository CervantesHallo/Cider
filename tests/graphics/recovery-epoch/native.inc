/* Include after the ordinary v6 coordinator, on Wine-thread Unix-call paths. */
#include "fixture.h"

static struct
{
    BOOL initialized, empty_returned, acquire_started, acquire_returned;
    BOOL captured, clearing, cleared, query_attempted, set_attempted, was_protected;
    HWND root;
    UINT64 root_epoch, empty_id, owner_id, returned_lease;
    DWORD arm_error, acquire_error;
    UINT32 last_close, query_status, set_status;
} recovery_epoch_native;

static void epoch_native_count(UINT32 *value)
{
    if (*value != ~(UINT32)0) ++*value;
}

/* remote_lock held. Counts include owners temporarily outside their FIFO.
 * Target-root absence is exact HWND+epoch, not a global roots==0 assertion. */
static void epoch_native_snapshot_locked(struct recovery_epoch_fixture *f)
{
    struct remote_producer *producer;
    struct producer_setup *setup;
    struct remote_host *host;
    struct remote_root *root = root_lookup_locked(recovery_epoch_native.root);
    UINT32 flags = 0;
    f->root_epoch = recovery_epoch_native.root_epoch;
    f->empty_setup_id = recovery_epoch_native.empty_id;
    f->owner_id = recovery_epoch_native.owner_id;
    f->returned_lease = recovery_epoch_native.returned_lease;
    f->arm_error = recovery_epoch_native.empty_returned ? recovery_epoch_native.arm_error : ~(UINT32)0;
    f->acquire_error = recovery_epoch_native.acquire_returned ? recovery_epoch_native.acquire_error : ~(UINT32)0;
    f->last_close = recovery_epoch_native.captured ? recovery_epoch_native.last_close : ~(UINT32)0;
    f->query_status = recovery_epoch_native.query_attempted ? recovery_epoch_native.query_status : ~(UINT32)0;
    f->set_status = recovery_epoch_native.set_attempted ? recovery_epoch_native.set_status : ~(UINT32)0;
    f->producers = f->watches = f->setups = f->cleanup = f->hosts = f->owner_busy = 0;
    f->models = retirement_outstanding > ~(UINT32)0 ? ~(UINT32)0 : (UINT32)retirement_outstanding;
    if (recovery_epoch_native.empty_returned) flags |= RECOVERY_EPOCH_EMPTY_RETURNED;
    if (recovery_epoch_native.acquire_returned) flags |= RECOVERY_EPOCH_ACQUIRE_RETURNED;
    if (recovery_epoch_native.captured)
        flags |= RECOVERY_EPOCH_OWNER_CAPTURED | RECOVERY_EPOCH_LAST_CLOSE_VALID;
    if (root && root->epoch == recovery_epoch_native.root_epoch) flags |= RECOVERY_EPOCH_TARGET_ROOT_PRESENT;
    if (recovery_epoch_native.cleared) flags |= RECOVERY_EPOCH_CLEARED;
    if (recovery_epoch_native.query_attempted) flags |= RECOVERY_EPOCH_QUERY_ATTEMPTED;
    if (recovery_epoch_native.set_attempted) flags |= RECOVERY_EPOCH_SET_ATTEMPTED;
    if (recovery_epoch_native.was_protected) flags |= RECOVERY_EPOCH_WAS_PROTECTED;
    for (producer = producers; producer; producer = producer->next) epoch_native_count(&f->producers);
    for (producer = producer_watches; producer; producer = producer->watch_next) epoch_native_count(&f->watches);
    for (producer = producer_cleanup; producer; producer = producer->cleanup_next) epoch_native_count(&f->cleanup);
    for (setup = setup_cleanup; setup; setup = setup->cleanup_next) epoch_native_count(&f->cleanup);
    for (host = hosts; host; host = host->next) epoch_native_count(&f->hosts);
    for (setup = producer_setups; setup; setup = setup->next)
    {
        epoch_native_count(&f->setups);
        if (!recovery_epoch_native.captured || setup->id != recovery_epoch_native.owner_id) continue;
        flags |= RECOVERY_EPOCH_OWNER_PRESENT;
        if (setup->active) flags |= RECOVERY_EPOCH_OWNER_ACTIVE;
        if (setup->busy) flags |= RECOVERY_EPOCH_OWNER_BUSY;
        if (setup->cleanup_queued) flags |= RECOVERY_EPOCH_OWNER_QUEUED;
        if (setup->thread) flags |= RECOVERY_EPOCH_THREAD_OPEN;
        if (setup->process) flags |= RECOVERY_EPOCH_PROCESS_OPEN;
        f->owner_busy = !!setup->busy;
    }
    f->flags = flags;
}

static DWORD epoch_native_arm_empty(const struct recovery_epoch_fixture *f)
{
    struct producer_setup *setup = NULL;
    HWND hwnd = wine_server_ptr_handle(f->root);
    UINT64 epoch = macdrv_remote_root_epoch(hwnd), id = 0;
    NTSTATUS closed;
    DWORD error;
    if (!epoch) return ERROR_INVALID_WINDOW_HANDLE;
    pthread_mutex_lock(&remote_lock);
    if (recovery_epoch_native.initialized)
    { pthread_mutex_unlock(&remote_lock); return ERROR_ALREADY_EXISTS; }
    if (producers || producer_watches || producer_setups || producer_cleanup || setup_cleanup ||
        hosts || retirement_outstanding)
    { pthread_mutex_unlock(&remote_lock); return ERROR_BUSY; }
    recovery_epoch_native.initialized = TRUE;
    recovery_epoch_native.root = hwnd;
    recovery_epoch_native.root_epoch = epoch;
    pthread_mutex_unlock(&remote_lock);

    /* Real registration and actual synchronous ARM. No NT handle is opened. */
    error = begin_producer_setup(&setup);
    if (setup) id = setup->id; /* active caller-owned record */
    closed = end_producer_setup(setup);
    if (!error && closed) error = RtlNtStatusToDosError(closed);
    pthread_mutex_lock(&remote_lock);
    recovery_epoch_native.empty_id = id;
    recovery_epoch_native.arm_error = error;
    recovery_epoch_native.empty_returned = TRUE;
    pthread_mutex_unlock(&remote_lock);
    return error;
}

static DWORD epoch_native_acquire(void)
{
    struct producer_setup *setup;
    struct remote_producer *producer;
    HWND hwnd;
    UINT64 epoch, lease;
    DWORD error, result = ERROR_SUCCESS;
    UINT matches = 0;
    pthread_mutex_lock(&remote_lock);
    if (!recovery_epoch_native.empty_returned || recovery_epoch_native.arm_error ||
        recovery_epoch_native.acquire_started)
    { pthread_mutex_unlock(&remote_lock); return ERROR_INVALID_STATE; }
    recovery_epoch_native.acquire_started = TRUE;
    hwnd = recovery_epoch_native.root; epoch = recovery_epoch_native.root_epoch;
    pthread_mutex_unlock(&remote_lock);
    lease = producer_acquire(hwnd, epoch, GetCurrentProcessId(), GetCurrentThreadId(), RECOVERY_EPOCH_HIGH, &error);
    pthread_mutex_lock(&remote_lock);
    recovery_epoch_native.acquire_error = error;
    recovery_epoch_native.returned_lease = lease;
    recovery_epoch_native.acquire_returned = TRUE;
    for (setup = producer_setups; setup; setup = setup->next)
    {
        if (setup->active || !setup->thread || setup->last_close != STATUS_HANDLE_NOT_CLOSABLE) continue;
        ++matches;
        recovery_epoch_native.owner_id = setup->id;
        recovery_epoch_native.last_close = setup->last_close;
    }
    if (matches == 1) recovery_epoch_native.captured = TRUE;
    else result = matches ? ERROR_INVALID_DATA : ERROR_NOT_FOUND;
    pthread_mutex_unlock(&remote_lock);
    if (lease)
    {
        /* Failed-case cleanup only, never accepted as the no-watch schedule. */
        pthread_mutex_lock(&remote_lock);
        producer = producer_lookup_locked(lease);
        if (producer) InterlockedIncrement(&producer->refs);
        pthread_mutex_unlock(&remote_lock);
        if (producer) { producer_revoke(producer, TRUE); producer_release(producer); }
    }
    if (lease || !error) return ERROR_INVALID_DATA;
    return result;
}

static DWORD epoch_native_clear(void)
{
    struct producer_setup *setup;
    HANDLE handle;
    OBJECT_HANDLE_FLAG_INFORMATION flags;
    NTSTATUS queried, changed = STATUS_SUCCESS;
    BOOL attempted = FALSE, was_protected = FALSE;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if (recovery_epoch_native.cleared)
    { pthread_mutex_unlock(&remote_lock); return ERROR_SUCCESS; }
    if (!recovery_epoch_native.captured)
    { pthread_mutex_unlock(&remote_lock); return ERROR_NOT_FOUND; }
    if (recovery_epoch_native.clearing)
    { pthread_mutex_unlock(&remote_lock); return ERROR_IO_PENDING; }
    for (setup = producer_setups; setup; setup = setup->next)
        if (setup->id == recovery_epoch_native.owner_id) break;
    if (!setup) { pthread_mutex_unlock(&remote_lock); return ERROR_NOT_FOUND; }
    if (setup->active || setup->busy || !setup->thread)
    { pthread_mutex_unlock(&remote_lock); return ERROR_IO_PENDING; }
    /* Copy only H. Real ProtectFromClose keeps this exact handle valid through
     * the first clear. No setup pointer or handle is used after successful set.
     * The serialized private latch excludes concurrent/replayed NT flag use. */
    handle = setup->thread;
    recovery_epoch_native.clearing = TRUE;
    pthread_mutex_unlock(&remote_lock);

    queried = NtQueryObject(handle, ObjectHandleFlagInformation, &flags, sizeof(flags), NULL);
    if (!queried && flags.ProtectFromClose)
    {
        was_protected = TRUE;
        flags.ProtectFromClose = FALSE;
        attempted = TRUE;
        changed = NtSetInformationObject(handle, ObjectHandleFlagInformation, &flags, sizeof(flags));
        error = changed ? RtlNtStatusToDosError(changed) : ERROR_SUCCESS;
    }
    else error = queried ? RtlNtStatusToDosError(queried) : ERROR_INVALID_DATA;
    pthread_mutex_lock(&remote_lock);
    recovery_epoch_native.query_attempted = TRUE;
    recovery_epoch_native.query_status = queried;
    recovery_epoch_native.set_attempted = attempted;
    recovery_epoch_native.set_status = attempted ? (UINT32)changed : ~(UINT32)0;
    recovery_epoch_native.was_protected = was_protected;
    if (attempted && !changed) recovery_epoch_native.cleared = TRUE;
    recovery_epoch_native.clearing = FALSE;
    pthread_mutex_unlock(&remote_lock);
    return error; /* no finalize/retry/recover, no borrowed handle close */
}

NTSTATUS macdrv_recovery_epoch_fixture(void *args)
{
    struct recovery_epoch_fixture *f = args;
    DWORD error = ERROR_SUCCESS;
    if (!f || f->size != sizeof(*f) || f->version != RECOVERY_EPOCH_FIXTURE_VERSION || f->reserved ||
        f->operation < RECOVERY_EPOCH_ARM_EMPTY || f->operation > RECOVERY_EPOCH_CLEANUP || !f->root)
        return STATUS_INVALID_PARAMETER;
    if (f->operation == RECOVERY_EPOCH_ARM_EMPTY) error = epoch_native_arm_empty(f);
    else
    {
        pthread_mutex_lock(&remote_lock);
        if ((!recovery_epoch_native.initialized && f->operation != RECOVERY_EPOCH_SNAPSHOT &&
             f->operation != RECOVERY_EPOCH_CLEANUP && f->operation != RECOVERY_EPOCH_RELEASE_TICK) ||
            (recovery_epoch_native.initialized && wine_server_ptr_handle(f->root) != recovery_epoch_native.root))
            error = ERROR_INVALID_HANDLE;
        pthread_mutex_unlock(&remote_lock);
        if (!error && f->operation == RECOVERY_EPOCH_ACQUIRE) error = epoch_native_acquire();
        if (!error && f->operation == RECOVERY_EPOCH_CLEAR) error = epoch_native_clear();
    }
    pthread_mutex_lock(&remote_lock);
    epoch_native_snapshot_locked(f);
    pthread_mutex_unlock(&remote_lock);
    f->status = error;
    return STATUS_SUCCESS;
}
