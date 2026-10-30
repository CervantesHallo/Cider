/* Include at end of pinned remote_watch_pe.c. See README for three hook calls.
 * All state below is protected by the existing watch_lock. Callback code and
 * this global slot are process-local; no callback retains a caller frame. */
#include "fixture.h"

/* Serialize private controls without holding the ordinary watch_lock across
 * Native calls (ARM re-enters the ordinary PE callback). Timer hooks never
 * acquire this lock. */
static SRWLOCK recovery_epoch_control_lock = SRWLOCK_INIT;

static struct
{
    BOOL used, enabled, selected, held, released, wait_finished, timely, hook_done, cleaned;
    HANDLE event;
    UINT event_users, controls;
    UINT64 selected_epoch, decision_epoch, wait_start, wait_end, wait_frequency;
    NTSTATUS recovered;
    UINT32 pending, wait_result, signal_error;
    BOOL decision_seen, decision_needed, decision_timer;
} recovery_epoch_pe;

static BOOL epoch_pe_clock(UINT64 *ticks, UINT64 *frequency)
{
    LARGE_INTEGER now, freq;
    if (!QueryPerformanceCounter(&now) || now.QuadPart < 0) return FALSE;
    if (frequency)
    {
        if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0 ||
            (UINT64)freq.QuadPart > (~(UINT64)0) / 4) return FALSE;
        *frequency = freq.QuadPart;
    }
    *ticks = now.QuadPart;
    return TRUE;
}

/* AFTER the actual MACDRV_CALL returns and BEFORE taking watch_lock for its
 * ordinary epoch decision. Waits on a real owned manual-reset event OUTSIDE
 * watch_lock. Returns a per-invocation selection token, not an epoch guess. */
static BOOL recovery_epoch_fixture_after_recover(UINT64 observed_epoch, NTSTATUS recovered, UINT32 pending)
{
    HANDLE event = NULL;
    UINT64 start = 0, end = 0, frequency = 0;
    BOOL clock_ok = epoch_pe_clock(&start, &frequency), timely;
    DWORD result;
    AcquireSRWLockExclusive(&watch_lock);
    if (recovery_epoch_pe.enabled && !recovery_epoch_pe.selected && recovery_epoch_pe.event &&
        !recovered && !pending)
    {
        recovery_epoch_pe.selected = recovery_epoch_pe.held = TRUE;
        recovery_epoch_pe.selected_epoch = observed_epoch;
        recovery_epoch_pe.recovered = recovered;
        recovery_epoch_pe.pending = pending;
        recovery_epoch_pe.wait_start = start;
        recovery_epoch_pe.wait_frequency = frequency;
        event = recovery_epoch_pe.event;
        ++recovery_epoch_pe.event_users;
    }
    ReleaseSRWLockExclusive(&watch_lock);
    if (!event) return FALSE;
    result = WaitForSingleObject(event, 4000);
    timely = epoch_pe_clock(&end, NULL) && clock_ok && end >= start && end - start < 4 * frequency;
    AcquireSRWLockExclusive(&watch_lock);
    recovery_epoch_pe.wait_end = end;
    recovery_epoch_pe.wait_result = result;
    recovery_epoch_pe.timely = timely;
    recovery_epoch_pe.held = FALSE;
    recovery_epoch_pe.wait_finished = TRUE;
    --recovery_epoch_pe.event_users;
    ReleaseSRWLockExclusive(&watch_lock);
    return TRUE;
}

/* CALL UNDER THE SAME EXCLUSIVE watch_lock, immediately AFTER the existing
 * ordinary equality guard/body. Do not reproduce or gate that predicate here. */
static void recovery_epoch_fixture_decision_locked(BOOL selected)
{
    if (!selected || recovery_epoch_pe.decision_seen) return;
    recovery_epoch_pe.decision_epoch = recovery_epoch;
    recovery_epoch_pe.decision_needed = native_recovery_needed;
    recovery_epoch_pe.decision_timer = active_timer != NULL;
    recovery_epoch_pe.decision_seen = TRUE;
}

/* Immediately AFTER the ordinary lock release. No NT calls or signals in the
 * recording hook. Event ownership can retire once this invocation leaves it. */
static void recovery_epoch_fixture_after_decision(BOOL selected)
{
    UINT64 end = 0;
    BOOL clock_ok;
    if (!selected) return;
    clock_ok = epoch_pe_clock(&end, NULL); /* outside watch_lock */
    AcquireSRWLockExclusive(&watch_lock);
    recovery_epoch_pe.timely = recovery_epoch_pe.timely && clock_ok &&
        end >= recovery_epoch_pe.wait_start &&
        end - recovery_epoch_pe.wait_start < 4 * recovery_epoch_pe.wait_frequency;
    recovery_epoch_pe.wait_end = end; /* post-decision, not merely event-return */
    recovery_epoch_pe.hook_done = TRUE;
    ReleaseSRWLockExclusive(&watch_lock);
}

static DWORD epoch_pe_prepare(void)
{
    HANDLE event;
    DWORD error;
    AcquireSRWLockExclusive(&watch_lock);
    if (recovery_epoch_pe.used || watches || active_timer || retired_timers || native_recovery_needed)
    { ReleaseSRWLockExclusive(&watch_lock); return ERROR_BUSY; }
    recovery_epoch_pe.used = TRUE;
    ReleaseSRWLockExclusive(&watch_lock);
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    error = event ? ERROR_SUCCESS : GetLastError();
    AcquireSRWLockExclusive(&watch_lock);
    recovery_epoch_pe.event = event;
    recovery_epoch_pe.enabled = event != NULL;
    recovery_epoch_pe.signal_error = ~(UINT32)0;
    recovery_epoch_pe.wait_result = ~(UINT32)0;
    ReleaseSRWLockExclusive(&watch_lock);
    return error;
}

static DWORD epoch_pe_release(void)
{
    HANDLE event;
    DWORD error;
    AcquireSRWLockExclusive(&watch_lock);
    if (recovery_epoch_pe.released || recovery_epoch_pe.cleaned)
    { ReleaseSRWLockExclusive(&watch_lock); return ERROR_SUCCESS; }
    event = recovery_epoch_pe.event;
    if (!event) { ReleaseSRWLockExclusive(&watch_lock); return ERROR_INVALID_HANDLE; }
    ++recovery_epoch_pe.controls;
    ReleaseSRWLockExclusive(&watch_lock);
    error = SetEvent(event) ? ERROR_SUCCESS : GetLastError();
    AcquireSRWLockExclusive(&watch_lock);
    recovery_epoch_pe.signal_error = error;
    if (!error) recovery_epoch_pe.released = TRUE;
    --recovery_epoch_pe.controls;
    ReleaseSRWLockExclusive(&watch_lock);
    return error;
}

static DWORD epoch_pe_cleanup(void)
{
    HANDLE event;
    DWORD error;
    /* Always unblock first. This is gate cleanup, not a recovery operation. */
    AcquireSRWLockExclusive(&watch_lock);
    recovery_epoch_pe.enabled = FALSE;
    event = recovery_epoch_pe.event;
    ReleaseSRWLockExclusive(&watch_lock);
    if (event && (error = epoch_pe_release())) return error;
    AcquireSRWLockExclusive(&watch_lock);
    if (recovery_epoch_pe.event_users || recovery_epoch_pe.controls ||
        (recovery_epoch_pe.selected && !recovery_epoch_pe.hook_done))
    { ReleaseSRWLockExclusive(&watch_lock); return ERROR_IO_PENDING; }
    event = recovery_epoch_pe.event;
    recovery_epoch_pe.event = NULL;
    if (event) ++recovery_epoch_pe.controls; /* exclusive close owner */
    ReleaseSRWLockExclusive(&watch_lock);
    error = event && !CloseHandle(event) ? GetLastError() : ERROR_SUCCESS;
    AcquireSRWLockExclusive(&watch_lock);
    if (error) recovery_epoch_pe.event = event; /* preserve failed owner */
    else recovery_epoch_pe.cleaned = TRUE;
    if (event) --recovery_epoch_pe.controls;
    ReleaseSRWLockExclusive(&watch_lock);
    return error;
}

static void epoch_pe_snapshot(struct recovery_epoch_fixture *f)
{
    UINT32 state = 0;
    AcquireSRWLockShared(&watch_lock);
    if (recovery_epoch_pe.used) state |= RECOVERY_EPOCH_GATE_ARMED;
    if (recovery_epoch_pe.selected) state |= RECOVERY_EPOCH_SELECTED;
    if (recovery_epoch_pe.held) state |= RECOVERY_EPOCH_HELD;
    if (recovery_epoch_pe.released) state |= RECOVERY_EPOCH_RELEASED;
    if (recovery_epoch_pe.wait_finished) state |= RECOVERY_EPOCH_WAIT_FINISHED;
    if (recovery_epoch_pe.hook_done) state |= RECOVERY_EPOCH_HOOK_DONE;
    if (recovery_epoch_pe.cleaned) state |= RECOVERY_EPOCH_EVENT_CLOSED;
    if (native_recovery_needed) state |= RECOVERY_EPOCH_LIVE_NEEDED;
    if (active_timer) state |= RECOVERY_EPOCH_LIVE_TIMER;
    if (recovery_epoch_pe.timely) state |= RECOVERY_EPOCH_WAIT_TIMELY;
    f->pe_state = state;
    f->selected_epoch = recovery_epoch_pe.selected_epoch;
    f->decision_epoch = recovery_epoch_pe.decision_epoch;
    f->live_epoch = recovery_epoch;
    f->wait_start = recovery_epoch_pe.wait_start;
    f->wait_end = recovery_epoch_pe.wait_end;
    f->wait_frequency = recovery_epoch_pe.wait_frequency;
    f->recover_status = recovery_epoch_pe.selected ? (UINT32)recovery_epoch_pe.recovered : ~(UINT32)0;
    f->recover_pending = recovery_epoch_pe.selected ? recovery_epoch_pe.pending : ~(UINT32)0;
    f->wait_result = recovery_epoch_pe.wait_finished ? recovery_epoch_pe.wait_result : ~(UINT32)0;
    f->decision_seen = recovery_epoch_pe.decision_seen;
    f->decision_needed = recovery_epoch_pe.decision_seen ? recovery_epoch_pe.decision_needed : ~(UINT32)0;
    f->decision_timer = recovery_epoch_pe.decision_seen ? recovery_epoch_pe.decision_timer : ~(UINT32)0;
    f->signal_error = recovery_epoch_pe.used ? recovery_epoch_pe.signal_error : ~(UINT32)0;
    f->event_owners = !!recovery_epoch_pe.event + !!recovery_epoch_pe.controls;
    f->hook_users = recovery_epoch_pe.event_users;
    ReleaseSRWLockShared(&watch_lock);
    /* Existing pinned diagnostic counters include timer creation/deletion
     * frames; active_timer alone is NOT a complete owner count. */
    f->pe_watches = InterlockedCompareExchange(&fixture_live_watches, 0, 0);
    f->pe_timers = InterlockedCompareExchange(&fixture_live_timers, 0, 0);
}

NTSTATUS WINAPI cider_recovery_epoch_fixture(void *args)
{
    struct recovery_epoch_fixture *f = args;
    NTSTATUS status;
    DWORD control = ERROR_SUCCESS;
    UINT32 operation;
    if (!f || f->size != sizeof(*f) || f->version != RECOVERY_EPOCH_FIXTURE_VERSION || f->reserved ||
        f->operation < RECOVERY_EPOCH_ARM_EMPTY || f->operation > RECOVERY_EPOCH_CLEANUP || !f->root)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&recovery_epoch_control_lock);
    operation = f->operation;
    if (f->operation == RECOVERY_EPOCH_ARM_EMPTY) control = epoch_pe_prepare();
    if (f->operation == RECOVERY_EPOCH_RELEASE_TICK) control = epoch_pe_release();
    if (f->operation == RECOVERY_EPOCH_CLEANUP) control = epoch_pe_cleanup();
    /* A failed preparation must not call Native ARM. Still obtain an actual
     * pure Native snapshot/NTSTATUS, rather than inventing zero owner counts. */
    if (f->operation == RECOVERY_EPOCH_ARM_EMPTY && control)
        f->operation = RECOVERY_EPOCH_SNAPSHOT;
    status = MACDRV_CALL(recovery_epoch_fixture, f);
    f->operation = operation;
    f->control_error = control;
    if (!status && !f->status && control) f->status = control;
    epoch_pe_snapshot(f);
    ReleaseSRWLockExclusive(&recovery_epoch_control_lock);
    return status;
}
