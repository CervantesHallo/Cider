/* DIAGNOSTIC ONLY: include after ordinary producer-status-v4/v5 coordinator.
 * No ordinary mutation is replayed. SNAPSHOT only copies protected state.
 * The matching fault variant supplies real protection and real failing NT calls.
 */
#include "fixture.h"

static struct
{
    BOOL initialized, run_returned, captured, clearing, cleared, retry_under_pin;
    BOOL last_close_valid, query_attempted, set_attempted, was_protected;
    UINT mode, owner_kind;
    HWND root;
    DWORD pid, acquire_error;
    UINT64 owner_id, returned_lease, root_epoch, clear_attempt;
    UINT32 last_close, query_status, set_status;
} producer_startup_fixture_slot;

static void startup_count(UINT32 *count)
{
    if (*count != ~(UINT32)0) ++*count;
}

/* remote_lock held. No pointer in this snapshot is borrowed by the caller. */
static void startup_snapshot_locked(struct producer_startup_failure_fixture *frame)
{
    struct remote_producer *producer;
    struct producer_setup *setup;
    struct remote_root *target_root;
    UINT32 flags = 0;

    target_root = root_lookup_locked(producer_startup_fixture_slot.root);
    if (target_root && target_root->epoch == producer_startup_fixture_slot.root_epoch)
        flags |= PRODUCER_STARTUP_TARGET_ROOT_PRESENT;
    frame->owner_id = producer_startup_fixture_slot.owner_id;
    frame->returned_lease = producer_startup_fixture_slot.returned_lease;
    frame->acquire_error = producer_startup_fixture_slot.run_returned ?
        producer_startup_fixture_slot.acquire_error : ~(UINT32)0;
    frame->last_close = producer_startup_fixture_slot.last_close_valid ?
        producer_startup_fixture_slot.last_close : ~(UINT32)0;
    frame->query_status = producer_startup_fixture_slot.query_attempted ?
        producer_startup_fixture_slot.query_status : ~(UINT32)0;
    frame->set_status = producer_startup_fixture_slot.set_attempted ?
        producer_startup_fixture_slot.set_status : ~(UINT32)0;
    frame->producers = frame->watches = frame->setups = frame->cleanup = 0;
    frame->owner_inflight = 0;
    frame->models = retirement_outstanding > ~(UINT32)0 ? ~(UINT32)0 : retirement_outstanding;

    if (producer_startup_fixture_slot.retry_under_pin) flags |= PRODUCER_STARTUP_RETRY_UNDER_CLEAR_PIN;
    if (producer_startup_fixture_slot.run_returned) flags |= PRODUCER_STARTUP_RUN_RETURNED;
    if (producer_startup_fixture_slot.captured) flags |= PRODUCER_STARTUP_OWNER_CAPTURED;
    if (producer_startup_fixture_slot.cleared) flags |= PRODUCER_STARTUP_CLEARED;
    if (producer_startup_fixture_slot.last_close_valid) flags |= PRODUCER_STARTUP_LAST_CLOSE_VALID;
    if (producer_startup_fixture_slot.query_attempted) flags |= PRODUCER_STARTUP_QUERY_ATTEMPTED;
    if (producer_startup_fixture_slot.set_attempted) flags |= PRODUCER_STARTUP_SET_ATTEMPTED;
    if (producer_startup_fixture_slot.was_protected) flags |= PRODUCER_STARTUP_WAS_PROTECTED;

    for (producer = producers; producer; producer = producer->next) startup_count(&frame->producers);
    for (producer = producer_cleanup; producer; producer = producer->cleanup_next) startup_count(&frame->cleanup);
    for (setup = setup_cleanup; setup; setup = setup->cleanup_next) startup_count(&frame->cleanup);
    for (producer = producer_watches; producer; producer = producer->watch_next)
    {
        startup_count(&frame->watches);
        if (producer_startup_fixture_slot.owner_kind != PRODUCER_STARTUP_SECTION_FAILURE ||
            producer->id != producer_startup_fixture_slot.owner_id) continue;
        flags |= PRODUCER_STARTUP_OWNER_PRESENT;
        if (producer->active) flags |= PRODUCER_STARTUP_OWNER_ACTIVE;
        if (producer->cleanup_inflight) flags |= PRODUCER_STARTUP_OWNER_BUSY;
        if (producer->cleanup_listed) flags |= PRODUCER_STARTUP_CLEANUP_QUEUED;
        if (producer->status_cell) flags |= PRODUCER_STARTUP_STATUS_OWNER;
        if (producer->status_cleanup) flags |= PRODUCER_STARTUP_STATUS_CLEANUP;
        if (producer->process || producer->cleanup_process) flags |= PRODUCER_STARTUP_PROCESS_OPEN;
        if (producer->watch_started) flags |= PRODUCER_STARTUP_WATCH_STARTED;
        if (producer->completion_requested) flags |= PRODUCER_STARTUP_COMPLETION_SEEN;
        frame->owner_inflight = producer->cleanup_inflight;
    }
    for (setup = producer_setups; setup; setup = setup->next)
    {
        startup_count(&frame->setups);
        if (producer_startup_fixture_slot.owner_kind != PRODUCER_STARTUP_THREAD_FAILURE ||
            setup->id != producer_startup_fixture_slot.owner_id) continue;
        flags |= PRODUCER_STARTUP_OWNER_PRESENT;
        if (setup->active) flags |= PRODUCER_STARTUP_OWNER_ACTIVE;
        if (setup->busy) flags |= PRODUCER_STARTUP_OWNER_BUSY;
        if (setup->cleanup_queued) flags |= PRODUCER_STARTUP_CLEANUP_QUEUED;
        if (setup->process) flags |= PRODUCER_STARTUP_PROCESS_OPEN;
        frame->owner_inflight = !!setup->busy;
    }
    frame->flags = flags;
}

static DWORD startup_capture_owner_locked(void)
{
    struct remote_producer *producer;
    struct producer_setup *setup;
    UINT matches = 0, kind = 0;
    UINT64 id = 0;
    NTSTATUS close_status = STATUS_SUCCESS;

    for (producer = producer_watches; producer; producer = producer->watch_next)
    {
        if (producer->root->hwnd != producer_startup_fixture_slot.root ||
            producer->pid != producer_startup_fixture_slot.pid ||
            producer->generation != PRODUCER_STARTUP_GENERATION ||
            producer->active || !producer->status_cleanup ||
            (!producer->status_cell && !producer->cleanup_inflight)) continue;
        ++matches; id = producer->id; kind = PRODUCER_STARTUP_SECTION_FAILURE;
    }
    for (setup = producer_setups; setup; setup = setup->next)
    {
        if (setup->active || !setup->thread || setup->last_close != STATUS_HANDLE_NOT_CLOSABLE) continue;
        ++matches; id = setup->id; kind = PRODUCER_STARTUP_THREAD_FAILURE;
        close_status = setup->last_close;
    }
    if (matches != 1) return matches ? ERROR_INVALID_DATA : ERROR_NOT_FOUND;
    producer_startup_fixture_slot.owner_id = id;
    producer_startup_fixture_slot.owner_kind = kind;
    producer_startup_fixture_slot.captured = TRUE;
    if (kind == PRODUCER_STARTUP_THREAD_FAILURE)
    {
        producer_startup_fixture_slot.last_close = close_status;
        producer_startup_fixture_slot.last_close_valid = TRUE;
    }
    /* A mismatched fault module remains a failed case, but its exact captured
     * owner can still be cleared safely during failure cleanup. */
    return kind == producer_startup_fixture_slot.mode ? ERROR_SUCCESS : ERROR_INVALID_DATA;
}

static DWORD startup_run(const struct producer_startup_failure_fixture *frame)
{
    struct remote_producer *producer;
    UINT64 epoch, lease;
    DWORD acquire_error, error, pid = GetCurrentProcessId();

    pthread_mutex_lock(&remote_lock);
    if (producer_startup_fixture_slot.initialized)
    { pthread_mutex_unlock(&remote_lock); return ERROR_ALREADY_EXISTS; }
    if (producers || producer_watches || producer_setups || producer_cleanup || hosts || retirement_outstanding)
    { pthread_mutex_unlock(&remote_lock); return ERROR_BUSY; }
    producer_startup_fixture_slot.initialized = TRUE;
    producer_startup_fixture_slot.mode = frame->mode;
    producer_startup_fixture_slot.root = wine_server_ptr_handle(frame->root);
    producer_startup_fixture_slot.pid = pid;
    pthread_mutex_unlock(&remote_lock);

    epoch = macdrv_remote_root_epoch(producer_startup_fixture_slot.root);
    if (!epoch) return ERROR_INVALID_WINDOW_HANDLE;
    pthread_mutex_lock(&remote_lock);
    producer_startup_fixture_slot.root_epoch = epoch;
    pthread_mutex_unlock(&remote_lock);
    lease = producer_acquire(producer_startup_fixture_slot.root, epoch,
        GetCurrentProcessId(), GetCurrentThreadId(), PRODUCER_STARTUP_GENERATION, &acquire_error);
    pthread_mutex_lock(&remote_lock);
    producer_startup_fixture_slot.returned_lease = lease;
    producer_startup_fixture_slot.acquire_error = acquire_error;
    producer_startup_fixture_slot.run_returned = TRUE;
    pthread_mutex_unlock(&remote_lock);

    if (lease)
    {
        /* Unexpected success is never accepted. Revoke the unexpected valid
         * lease here solely for failure cleanup, not as an observed recovery. */
        pthread_mutex_lock(&remote_lock);
        producer = producer_lookup_locked(lease);
        if (producer) InterlockedIncrement(&producer->refs);
        pthread_mutex_unlock(&remote_lock);
        if (producer)
        {
            producer_revoke(producer, TRUE);
            producer_release(producer);
        }
    }
    pthread_mutex_lock(&remote_lock);
    error = startup_capture_owner_locked();
    pthread_mutex_unlock(&remote_lock);
    if (lease || !acquire_error) return ERROR_INVALID_DATA;
    return error;
}

/* Observe an actual ordinary retry AFTER its finalizer decision. Never
 * enqueue or close from this hook; the ordinary code must hand off ownership. */
static UINT64 producer_startup_retry_before_finalize(struct remote_producer *producer)
{
    UINT64 attempt = 0;
    pthread_mutex_lock(&remote_lock);
    if (producer_startup_fixture_slot.clearing &&
        producer_startup_fixture_slot.owner_kind == PRODUCER_STARTUP_SECTION_FAILURE &&
        producer_startup_fixture_slot.owner_id == producer->id &&
        producer->cleanup_inflight && producer->status_cleanup)
        attempt = producer_startup_fixture_slot.clear_attempt;
    pthread_mutex_unlock(&remote_lock);
    return attempt;
}
static void producer_startup_retry_observed(struct remote_producer *producer, UINT64 attempt)
{
    pthread_mutex_lock(&remote_lock);
    if (attempt && attempt == producer_startup_fixture_slot.clear_attempt &&
        producer_startup_fixture_slot.clearing &&
        producer_startup_fixture_slot.owner_kind == PRODUCER_STARTUP_SECTION_FAILURE &&
        producer_startup_fixture_slot.owner_id == producer->id &&
        producer->cleanup_inflight && producer->status_cleanup)
        producer_startup_fixture_slot.retry_under_pin = TRUE;
    pthread_mutex_unlock(&remote_lock);
}
static BOOL startup_wait_for_pinned_retry(void)
{
    LARGE_INTEGER begin, now, frequency, delay;
    LONGLONG end;
    BOOL seen;
    if (NtQueryPerformanceCounter(&begin, &frequency) || begin.QuadPart < 0 ||
        frequency.QuadPart <= 0 || frequency.QuadPart > 0x7fffffffffffffffLL / 2 ||
        begin.QuadPart > 0x7fffffffffffffffLL - 2 * frequency.QuadPart) return FALSE;
    end = begin.QuadPart + 2 * frequency.QuadPart;
    delay.QuadPart = -100000;
    for (;;)
    {
        if (NtQueryPerformanceCounter(&now, NULL) || now.QuadPart < begin.QuadPart || now.QuadPart >= end) return FALSE;
        pthread_mutex_lock(&remote_lock);
        seen = producer_startup_fixture_slot.retry_under_pin;
        pthread_mutex_unlock(&remote_lock);
        if (NtQueryPerformanceCounter(&now, NULL) || now.QuadPart < begin.QuadPart || now.QuadPart >= end) return FALSE;
        if (seen) return TRUE;
        if (NtDelayExecution(FALSE, &delay)) return FALSE;
    }
}
static DWORD startup_clear(void)
{
    struct remote_producer *producer = NULL, *candidate;
    struct producer_setup *setup;
    OBJECT_HANDLE_FLAG_INFORMATION flags;
    HANDLE handle = NULL;
    NTSTATUS query_status, set_status = STATUS_SUCCESS;
    BOOL set_attempted = FALSE;
    DWORD error;

    pthread_mutex_lock(&remote_lock);
    if (producer_startup_fixture_slot.cleared)
    { pthread_mutex_unlock(&remote_lock); return ERROR_SUCCESS; }
    if (!producer_startup_fixture_slot.captured)
    { pthread_mutex_unlock(&remote_lock); return ERROR_NOT_FOUND; }
    if (producer_startup_fixture_slot.clearing)
    { pthread_mutex_unlock(&remote_lock); return ERROR_IO_PENDING; }
    if (producer_startup_fixture_slot.clear_attempt == ~(UINT64)0)
    { pthread_mutex_unlock(&remote_lock); return ERROR_ARITHMETIC_OVERFLOW; }
    if (producer_startup_fixture_slot.owner_kind == PRODUCER_STARTUP_SECTION_FAILURE)
    {
        for (candidate = producer_watches; candidate; candidate = candidate->watch_next)
            if (candidate->id == producer_startup_fixture_slot.owner_id) break;
        if (!candidate)
        { pthread_mutex_unlock(&remote_lock); return ERROR_NOT_FOUND; }
        if (candidate->status_cell) handle = macdrv_status_cell_handle(candidate->status_cell);
        if (!handle)
        { pthread_mutex_unlock(&remote_lock); return ERROR_IO_PENDING; }
        producer = candidate;
        InterlockedIncrement(&producer->refs);
        ++producer->cleanup_inflight;
    }
    else
    {
        for (setup = producer_setups; setup; setup = setup->next)
            if (setup->id == producer_startup_fixture_slot.owner_id) break;
        if (!setup)
        { pthread_mutex_unlock(&remote_lock); return ERROR_NOT_FOUND; }
        if (setup->active || setup->busy || !setup->thread)
        { pthread_mutex_unlock(&remote_lock); return ERROR_IO_PENDING; }
        handle = setup->thread;
        /* Deliberately do not retain setup or use it after unlock. Until the
         * first successful clear, actual protection prevents handle closure.
         * Native clearing/cleared latches exclude a second flag operation. */
    }
    ++producer_startup_fixture_slot.clear_attempt;
    producer_startup_fixture_slot.clearing = TRUE;
    pthread_mutex_unlock(&remote_lock);

    if (producer && !startup_wait_for_pinned_retry())
    {
        pthread_mutex_lock(&remote_lock);
        producer_startup_fixture_slot.clearing = FALSE;
        --producer->cleanup_inflight;
        pthread_mutex_unlock(&remote_lock);
        producer_release(producer);
        return ERROR_TIMEOUT;
    }
    query_status = NtQueryObject(handle, ObjectHandleFlagInformation, &flags, sizeof(flags), NULL);
    if (!query_status && flags.ProtectFromClose)
    {
        flags.ProtectFromClose = FALSE;
        set_attempted = TRUE;
        set_status = NtSetInformationObject(handle, ObjectHandleFlagInformation, &flags, sizeof(flags));
        error = set_status ? RtlNtStatusToDosError(set_status) : ERROR_SUCCESS;
    }
    else error = query_status ? RtlNtStatusToDosError(query_status) : ERROR_INVALID_DATA;

    pthread_mutex_lock(&remote_lock);
    producer_startup_fixture_slot.query_attempted = TRUE;
    producer_startup_fixture_slot.query_status = query_status;
    if (!query_status && flags.ProtectFromClose) producer_startup_fixture_slot.was_protected = TRUE;
    /* flags was changed before NtSet; successful query plus an attempted set
     * also proves that its original ProtectFromClose value was TRUE. */
    if (set_attempted) producer_startup_fixture_slot.was_protected = TRUE;
    producer_startup_fixture_slot.set_attempted = set_attempted;
    producer_startup_fixture_slot.set_status = set_attempted ? set_status : ~(UINT32)0;
    if (set_attempted && !set_status) producer_startup_fixture_slot.cleared = TRUE;
    producer_startup_fixture_slot.clearing = FALSE;
    if (producer) --producer->cleanup_inflight;
    pthread_mutex_unlock(&remote_lock);
    if (producer) producer_release(producer);
    /* No handle/record use follows the successful set or producer pin drop. */
    return error;
}

NTSTATUS macdrv_producer_startup_failure_fixture(void *args)
{
    struct producer_startup_failure_fixture *frame = args;
    DWORD error;

    if (!frame || frame->size != sizeof(*frame) || frame->version != PRODUCER_STARTUP_FIXTURE_VERSION ||
        frame->reserved || frame->operation < PRODUCER_STARTUP_RUN || frame->operation > PRODUCER_STARTUP_CLEAR ||
        (frame->mode != PRODUCER_STARTUP_SECTION_FAILURE && frame->mode != PRODUCER_STARTUP_THREAD_FAILURE))
        return STATUS_INVALID_PARAMETER;
    if (frame->operation == PRODUCER_STARTUP_RUN)
        error = frame->root ? startup_run(frame) : ERROR_INVALID_WINDOW_HANDLE;
    else
    {
        pthread_mutex_lock(&remote_lock);
        error = !producer_startup_fixture_slot.initialized ? ERROR_INVALID_HANDLE :
            producer_startup_fixture_slot.mode != frame->mode ? ERROR_INVALID_PARAMETER : ERROR_SUCCESS;
        pthread_mutex_unlock(&remote_lock);
        if (!error && frame->operation == PRODUCER_STARTUP_CLEAR) error = startup_clear();
    }
    pthread_mutex_lock(&remote_lock);
    startup_snapshot_locked(frame);
    pthread_mutex_unlock(&remote_lock);
    frame->status = error;
    return STATUS_SUCCESS;
}
