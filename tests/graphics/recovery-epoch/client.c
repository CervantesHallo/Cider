#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef NTSTATUS (WINAPI *fixture_fn)(void *);
struct deadline { LONGLONG start, end; BOOL valid; };
struct client
{
    fixture_fn fixture;
    HMODULE module;
    HWND root;
    UINT32 root_value;
    LONGLONG frequency;
    UINT64 birth, initial_epoch, empty_id, owner_id, root_epoch;
    struct recovery_epoch_fixture frame, held, primary;
    unsigned checks, failures, snapshots;
    BOOL io_failed, armed, acquired, released, cleared, clear_attempted, clear_retryable;
    BOOL root_destroyed, held_seen, primary_seen, settled, gate_closed;
};

#define FRAME_FORMAT \
    "\"size\":%u,\"version\":%u,\"operation\":%u,\"status\":%u," \
    "\"root\":%u,\"flags\":%u,\"acquire_error\":%u,\"arm_error\":%u," \
    "\"root_epoch\":%llu,\"empty_setup_id\":%llu,\"owner_id\":%llu,\"returned_lease\":%llu," \
    "\"selected_epoch\":%llu,\"decision_epoch\":%llu,\"live_epoch\":%llu," \
    "\"wait_start\":%llu,\"wait_end\":%llu,\"wait_frequency\":%llu," \
    "\"producers\":%u,\"watches\":%u,\"setups\":%u,\"cleanup\":%u," \
    "\"models\":%u,\"hosts\":%u,\"owner_busy\":%u,\"last_close\":%u," \
    "\"query_status\":%u,\"set_status\":%u,\"pe_watches\":%u,\"pe_timers\":%u," \
    "\"pe_state\":%u,\"recover_status\":%u,\"recover_pending\":%u,\"wait_result\":%u," \
    "\"decision_seen\":%u,\"decision_needed\":%u,\"decision_timer\":%u,\"signal_error\":%u," \
    "\"event_owners\":%u,\"hook_users\":%u,\"control_error\":%u,\"reserved\":%u"
#define FRAME_VALUES(f) \
    (unsigned)(f)->size, (unsigned)(f)->version, (unsigned)(f)->operation, (unsigned)(f)->status, \
    (unsigned)(f)->root, (unsigned)(f)->flags, (unsigned)(f)->acquire_error, (unsigned)(f)->arm_error, \
    (unsigned long long)(f)->root_epoch, (unsigned long long)(f)->empty_setup_id, \
    (unsigned long long)(f)->owner_id, (unsigned long long)(f)->returned_lease, \
    (unsigned long long)(f)->selected_epoch, (unsigned long long)(f)->decision_epoch, \
    (unsigned long long)(f)->live_epoch, (unsigned long long)(f)->wait_start, \
    (unsigned long long)(f)->wait_end, (unsigned long long)(f)->wait_frequency, \
    (unsigned)(f)->producers, (unsigned)(f)->watches, (unsigned)(f)->setups, (unsigned)(f)->cleanup, \
    (unsigned)(f)->models, (unsigned)(f)->hosts, (unsigned)(f)->owner_busy, (unsigned)(f)->last_close, \
    (unsigned)(f)->query_status, (unsigned)(f)->set_status, (unsigned)(f)->pe_watches, (unsigned)(f)->pe_timers, \
    (unsigned)(f)->pe_state, (unsigned)(f)->recover_status, (unsigned)(f)->recover_pending, \
    (unsigned)(f)->wait_result, (unsigned)(f)->decision_seen, (unsigned)(f)->decision_needed, \
    (unsigned)(f)->decision_timer, (unsigned)(f)->signal_error, (unsigned)(f)->event_owners, \
    (unsigned)(f)->hook_users, (unsigned)(f)->control_error, (unsigned)(f)->reserved

static void emit(struct client *c, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
    if (result < 0 || fflush(stdout) == EOF) c->io_failed = TRUE;
}
static BOOL check(struct client *c, BOOL value, const char *name)
{
    ++c->checks;
    if (!value) ++c->failures;
    emit(c, "{\"type\":\"recovery-epoch\",\"event\":\"check\",\"check\":%u,"
            "\"name\":\"%s\",\"ok\":%s}\n", c->checks, name, value ? "true" : "false");
    return value;
}
static BOOL ticks(LONGLONG *value)
{
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now) || now.QuadPart < 0) return FALSE;
    *value = now.QuadPart;
    return TRUE;
}
static struct deadline new_deadline(const struct client *c)
{
    struct deadline d = {0};
    if (c->frequency <= 0 || c->frequency > LLONG_MAX / 4 || !ticks(&d.start) ||
        d.start > LLONG_MAX - 4 * c->frequency) return d;
    d.end = d.start + 4 * c->frequency;
    d.valid = TRUE;
    return d;
}
static BOOL within(const struct deadline *d)
{
    LONGLONG now;
    return d->valid && ticks(&now) && now >= d->start && now < d->end;
}
static BOOL pause_poll(const struct deadline *d)
{
    if (!within(d)) return FALSE;
    Sleep(5); /* pacing only: never supplies readiness or drives recovery */
    return within(d);
}
static BOOL shape(const struct client *c, unsigned op)
{
    const struct recovery_epoch_fixture *f = &c->frame;
    return f->size == sizeof(*f) && f->version == RECOVERY_EPOCH_FIXTURE_VERSION &&
        f->operation == op && f->root == c->root_value && !f->reserved &&
        !(f->flags & ~0x7fffu) && !(f->pe_state & ~0x3ffu) &&
        f->pe_watches != UINT32_MAX && f->pe_timers != UINT32_MAX && f->decision_seen <= 1;
}

static BOOL operation(struct client *c, unsigned op, const struct deadline *d, BOOL force)
{
    struct recovery_epoch_fixture *f = &c->frame;
    BOOL started = within(d), returned, valid;
    NTSTATUS status;
    if (!started && !force) return FALSE;
    memset(f, 0, sizeof(*f));
    f->size = sizeof(*f); f->version = RECOVERY_EPOCH_FIXTURE_VERSION;
    f->operation = op; f->root = c->root_value; f->status = ERROR_INVALID_DATA;
    f->pe_watches = f->pe_timers = UINT32_MAX;
    if (op == RECOVERY_EPOCH_ARM_EMPTY) c->armed = TRUE;
    if (op == RECOVERY_EPOCH_ACQUIRE) c->acquired = TRUE;
    if (op == RECOVERY_EPOCH_CLEAR) { c->clear_attempted = TRUE; c->clear_retryable = FALSE; }
    status = c->fixture(f);
    valid = shape(c, op);
    /* Record ownership/effects BEFORE rejecting a late call. No cleanup replay
     * of a successful flag clear, and no retry of a definitive/unknown error. */
    if (!status && valid)
    {
        if ((f->flags & RECOVERY_EPOCH_EMPTY_RETURNED) && !c->root_epoch)
        { c->empty_id = f->empty_setup_id; c->root_epoch = f->root_epoch; }
        if ((f->flags & RECOVERY_EPOCH_OWNER_CAPTURED) && f->owner_id && !c->owner_id) c->owner_id = f->owner_id;
        if (f->pe_state & RECOVERY_EPOCH_RELEASED) c->released = TRUE;
        if (op == RECOVERY_EPOCH_CLEAR && !f->status && (f->flags & RECOVERY_EPOCH_CLEARED)) c->cleared = TRUE;
        if (op == RECOVERY_EPOCH_CLEAR && f->status == ERROR_IO_PENDING &&
            !f->control_error && !(f->flags & RECOVERY_EPOCH_CLEARED)) c->clear_retryable = TRUE;
        if (f->pe_state & RECOVERY_EPOCH_EVENT_CLOSED) c->gate_closed = TRUE;
    }
    returned = within(d);
    ++c->snapshots;
    emit(c, "{\"type\":\"recovery-epoch\",\"event\":\"snapshot\",\"snapshot\":%u,"
            "\"ntstatus\":%u,\"started_in_time\":%s,\"returned_in_time\":%s,\"frame\":{" FRAME_FORMAT "}}\n",
         c->snapshots, (unsigned)(UINT32)status, started ? "true" : "false",
         returned ? "true" : "false", FRAME_VALUES(f));
    return started && returned && within(d) && !status && valid &&
        (!f->status || ((op == RECOVERY_EPOCH_CLEAR || op == RECOVERY_EPOCH_CLEANUP) &&
                       f->status == ERROR_IO_PENDING));
}
static BOOL once(struct client *c, unsigned op)
{
    struct deadline d = new_deadline(c);
    return operation(c, op, &d, FALSE) && !c->frame.status;
}
static BOOL native_zero(const struct recovery_epoch_fixture *f)
{
    return !f->producers && !f->watches && !f->setups && !f->cleanup && !f->models && !f->hosts &&
        !f->owner_busy && !(f->flags & (RECOVERY_EPOCH_OWNER_PRESENT | RECOVERY_EPOCH_OWNER_ACTIVE |
        RECOVERY_EPOCH_OWNER_BUSY | RECOVERY_EPOCH_OWNER_QUEUED | RECOVERY_EPOCH_THREAD_OPEN |
        RECOVERY_EPOCH_PROCESS_OPEN));
}
static BOOL selected_held(const struct recovery_epoch_fixture *f)
{
    UINT32 needed = RECOVERY_EPOCH_GATE_ARMED | RECOVERY_EPOCH_SELECTED | RECOVERY_EPOCH_HELD;
    return (f->pe_state & needed) == needed && !f->decision_seen && f->hook_users == 1 &&
        f->event_owners != 0 &&
        !(f->pe_state & (RECOVERY_EPOCH_RELEASED | RECOVERY_EPOCH_WAIT_FINISHED | RECOVERY_EPOCH_HOOK_DONE));
}
static BOOL retained(const struct client *c)
{
    const struct recovery_epoch_fixture *f = &c->frame;
    UINT32 flags = RECOVERY_EPOCH_EMPTY_RETURNED | RECOVERY_EPOCH_ACQUIRE_RETURNED |
        RECOVERY_EPOCH_OWNER_CAPTURED | RECOVERY_EPOCH_OWNER_PRESENT |
        RECOVERY_EPOCH_THREAD_OPEN | RECOVERY_EPOCH_LAST_CLOSE_VALID;
    return (f->flags & flags) == flags &&
        !(f->flags & (RECOVERY_EPOCH_OWNER_ACTIVE | RECOVERY_EPOCH_PROCESS_OPEN | RECOVERY_EPOCH_CLEARED)) &&
        !!(f->flags & RECOVERY_EPOCH_TARGET_ROOT_PRESENT) != !!c->root_destroyed &&
        !f->producers && !f->watches && f->setups == 1 && !f->models && !f->hosts &&
        !f->pe_watches &&
        f->owner_id == c->owner_id && f->owner_id && !f->returned_lease &&
        f->empty_setup_id == c->empty_id && f->root_epoch == c->root_epoch && !f->arm_error &&
        f->last_close == (UINT32)STATUS_HANDLE_NOT_CLOSABLE &&
        f->acquire_error == RtlNtStatusToDosError(STATUS_HANDLE_NOT_CLOSABLE);
}
enum poll_kind { POLL_HELD, POLL_RETAINED_HELD, POLL_DECISION, POLL_ZERO };
static BOOL poll_snapshot(struct client *c, enum poll_kind kind)
{
    struct deadline d = new_deadline(c);
    for (;;)
    {
        const struct recovery_epoch_fixture *f = &c->frame;
        BOOL ready;
        if (!operation(c, RECOVERY_EPOCH_SNAPSHOT, &d, FALSE)) return FALSE;
        /* Process-watch creation disqualifies this no-PE-watch experiment. */
        if (f->pe_watches && !c->failures) return FALSE;
        if (kind == POLL_HELD) ready = selected_held(f) && native_zero(f);
        else if (kind == POLL_RETAINED_HELD)
            ready = selected_held(f) && retained(c) && f->pe_timers && f->pe_timers != UINT32_MAX;
        else if (kind == POLL_DECISION)
            ready = f->decision_seen == 1 && (f->pe_state & RECOVERY_EPOCH_HOOK_DONE);
        else ready = native_zero(f) && !f->pe_watches && !f->pe_timers &&
            !(f->flags & RECOVERY_EPOCH_TARGET_ROOT_PRESENT) &&
            !(f->pe_state & (RECOVERY_EPOCH_HELD | RECOVERY_EPOCH_LIVE_TIMER));
        if (!within(&d)) return FALSE;
        if (ready) return TRUE;
        if (!pause_poll(&d)) return FALSE;
    }
}
static BOOL clear_owned(struct client *c, BOOL cleanup)
{
    struct deadline d = new_deadline(c);
    BOOL first = TRUE;
    for (;;)
    {
        BOOL ok = operation(c, RECOVERY_EPOCH_CLEAR, &d, first && cleanup);
        first = FALSE;
        if (!ok || c->frame.pe_watches) return FALSE;
        if (!c->frame.status) return c->cleared && within(&d);
        if (!c->clear_retryable || !pause_poll(&d)) return FALSE;
    }
}
static BOOL cleanup_gate(struct client *c)
{
    struct deadline d = new_deadline(c);
    BOOL first = TRUE;
    for (;;)
    {
        /* The first cleanup attempt must release a blocked callback even if
         * the local clock failed. It is never accepted as timely in that case. */
        BOOL ok = operation(c, RECOVERY_EPOCH_CLEANUP, &d, first);
        first = FALSE;
        if (!ok) return FALSE;
        if (!c->frame.status)
            return c->gate_closed && !c->frame.event_owners && !c->frame.hook_users && within(&d);
        if (c->frame.status != ERROR_IO_PENDING || !pause_poll(&d)) return FALSE;
    }
}
static BOOL bound_decision(const struct client *c)
{
    const struct recovery_epoch_fixture *f = &c->frame;
    UINT32 bits = RECOVERY_EPOCH_SELECTED | RECOVERY_EPOCH_RELEASED | RECOVERY_EPOCH_WAIT_FINISHED |
                  RECOVERY_EPOCH_HOOK_DONE | RECOVERY_EPOCH_WAIT_TIMELY;
    return f->decision_seen == 1 && (f->pe_state & bits) == bits &&
        !(f->pe_state & RECOVERY_EPOCH_HELD) && !f->hook_users && !f->recover_status && !f->recover_pending &&
        f->wait_result == WAIT_OBJECT_0 && !f->signal_error &&
        f->selected_epoch == c->held.selected_epoch && f->selected_epoch >= RECOVERY_EPOCH_HIGH &&
        f->selected_epoch != UINT64_MAX && f->decision_epoch == f->selected_epoch + 1 &&
        f->live_epoch == f->decision_epoch && f->decision_needed <= 1 && f->decision_timer <= 1 &&
        f->wait_frequency == (UINT64)c->frequency && f->wait_frequency <= UINT64_MAX / 4 &&
        f->wait_end >= f->wait_start && f->wait_end - f->wait_start < 4 * f->wait_frequency &&
        retained(c) && !f->pe_watches;
}

int main(int argc, char **argv)
{
    static const WCHAR class_name[] = L"CiderRecoveryEpochV1";
    struct client c;
    WNDCLASSW cls;
    HINSTANCE instance = GetModuleHandleW(NULL);
    ATOM atom = 0;
    LARGE_INTEGER frequency;
    FILETIME created, exited, kernel, user;
    DWORD pid = 0, tid = 0;
    BOOL ok, clean = TRUE;
    (void)argv;
    memset(&c, 0, sizeof(c)); memset(&cls, 0, sizeof(cls));
    if (!check(&c, argc == 1, "one-fresh-case-no-options")) goto cleanup;              /* 1 */
    ok = QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 && frequency.QuadPart <= LLONG_MAX / 4;
    if (ok) c.frequency = frequency.QuadPart;
    if (!check(&c, ok, "monotonic-clock")) goto cleanup;                              /* 2 */
    cls.hInstance = instance; cls.lpfnWndProc = DefWindowProcW; cls.lpszClassName = class_name;
    atom = RegisterClassW(&cls);
    if (!check(&c, atom != 0, "register-root-class")) goto cleanup;                   /* 3 */
    c.root = CreateWindowW(class_name, L"Cider recovery epoch", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           20, 20, 360, 240, NULL, NULL, instance, NULL);
    if (c.root) c.root_value = (UINT32)(uintptr_t)c.root;
    if (!check(&c, c.root != NULL, "create-real-root")) goto cleanup;                 /* 4 */
    tid = GetWindowThreadProcessId(c.root, &pid);
    ok = pid == GetCurrentProcessId() && tid == GetCurrentThreadId() && GetAncestor(c.root, GA_ROOT) == c.root &&
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    if (ok) c.birth = (UINT64)created.dwLowDateTime | ((UINT64)created.dwHighDateTime << 32);
    if (!check(&c, ok && c.birth != 0, "actual-self-pid-tid-birth")) goto cleanup;     /* 5 */
    c.module = LoadLibraryW(L"winemac.drv");
    if (!check(&c, c.module != NULL, "load-private-driver")) goto cleanup;            /* 6 */
    c.fixture = (fixture_fn)(uintptr_t)GetProcAddress(c.module, "cider_recovery_epoch_fixture");
    if (!check(&c, c.fixture != NULL, "resolve-private-fixture")) goto cleanup;       /* 7 */
    ok = once(&c, RECOVERY_EPOCH_SNAPSHOT) && native_zero(&c.frame) &&
        !c.frame.pe_watches && !c.frame.pe_timers && !c.frame.event_owners && !c.frame.hook_users &&
        !c.frame.pe_state && c.frame.live_epoch >= RECOVERY_EPOCH_HIGH && c.frame.live_epoch < UINT64_MAX - 2;
    c.initial_epoch = c.frame.live_epoch;
    if (!check(&c, ok, "fresh-owners-and-actual-high-epoch")) goto cleanup;            /* 8 */
    ok = once(&c, RECOVERY_EPOCH_ARM_EMPTY) && !c.frame.arm_error &&
        (c.frame.flags & RECOVERY_EPOCH_EMPTY_RETURNED) && native_zero(&c.frame);
    if (!check(&c, ok, "ordinary-empty-setup-actual-arm-and-end")) goto cleanup;      /* 9 */
    ok = c.empty_id >= RECOVERY_EPOCH_HIGH && c.root_epoch >= RECOVERY_EPOCH_HIGH &&
        c.frame.live_epoch == c.initial_epoch + 1;
    if (!check(&c, ok, "actual-high-identities-and-first-epoch-increment")) goto cleanup; /* 10 */
    if (!check(&c, poll_snapshot(&c, POLL_HELD), "selected-actual-zero-tick-held")) goto cleanup; /* 11 */
    c.held = c.frame; c.held_seen = TRUE;
    ok = !c.frame.recover_status && !c.frame.recover_pending && !c.frame.pe_watches &&
        c.frame.selected_epoch == c.initial_epoch + 1 &&
        c.frame.live_epoch == c.frame.selected_epoch && c.frame.wait_frequency == (UINT64)c.frequency;
    if (!check(&c, ok, "real-zero-return-bound-to-selected-old-epoch")) goto cleanup; /* 12 */
    ok = once(&c, RECOVERY_EPOCH_ACQUIRE) && !c.frame.returned_lease &&
        c.frame.acquire_error == RtlNtStatusToDosError(STATUS_HANDLE_NOT_CLOSABLE) &&
        (c.frame.flags & RECOVERY_EPOCH_OWNER_CAPTURED) && c.owner_id > c.empty_id;
    if (!check(&c, ok, "real-protected-thread-acquire-failure")) goto cleanup;         /* 13 */
    ok = poll_snapshot(&c, POLL_RETAINED_HELD) &&
        c.frame.live_epoch == c.held.selected_epoch + 1 && (c.frame.pe_state & RECOVERY_EPOCH_LIVE_NEEDED);
    if (!check(&c, ok, "new-arm-advanced-epoch-with-no-pe-watch")) goto cleanup;      /* 14 */
    if (!check(&c, DestroyWindow(c.root), "destroy-root-before-release-and-clear")) goto cleanup; /* 15 */
    c.root = NULL; c.root_destroyed = TRUE;
    if (!check(&c, poll_snapshot(&c, POLL_RETAINED_HELD), "exact-root-epoch-absent-owner-held")) goto cleanup; /* 16 */
    ok = once(&c, RECOVERY_EPOCH_RELEASE_TICK) && c.released && !c.frame.signal_error;
    if (!check(&c, ok, "actual-event-release-outside-watch-lock")) goto cleanup;      /* 17 */
    if (!check(&c, poll_snapshot(&c, POLL_DECISION), "selected-post-guard-decision-returned")) goto cleanup; /* 18 */
    if (!check(&c, bound_decision(&c), "coherent-stale-zero-decision-and-real-retained-owner")) goto cleanup; /* 19 */
    c.primary = c.frame; c.primary_seen = TRUE;
    /* Single primary. A later tick's live needed value cannot repair this
     * frozen actual ordinary decision. Continue cleanup after a negative. */
    check(&c, c.primary.decision_needed == 1, "stale-zero-preserves-new-native-recovery-needed"); /* 20 */
    if (!check(&c, clear_owned(&c, FALSE), "actual-first-clear-within-4s")) goto cleanup; /* 21 */
    ok = c.cleared && (c.frame.flags & (RECOVERY_EPOCH_CLEARED | RECOVERY_EPOCH_QUERY_ATTEMPTED |
        RECOVERY_EPOCH_SET_ATTEMPTED | RECOVERY_EPOCH_WAS_PROTECTED)) ==
        (RECOVERY_EPOCH_CLEARED | RECOVERY_EPOCH_QUERY_ATTEMPTED |
         RECOVERY_EPOCH_SET_ATTEMPTED | RECOVERY_EPOCH_WAS_PROTECTED) &&
        !c.frame.query_status && !c.frame.set_status && !c.frame.pe_watches;
    if (!check(&c, ok, "actual-query-set-protected-handle-success")) goto cleanup;     /* 22 */
    c.settled = poll_snapshot(&c, POLL_ZERO);
    if (!check(&c, c.settled, "autonomous-native-and-pe-timer-zero")) goto cleanup;   /* 23 */
    ok = c.frame.owner_id == c.owner_id && c.frame.empty_setup_id == c.empty_id &&
        c.frame.root_epoch == c.root_epoch && c.root_destroyed && c.frame.decision_seen &&
        c.frame.decision_epoch == c.primary.decision_epoch &&
        c.frame.decision_needed == c.primary.decision_needed && (c.frame.flags & RECOVERY_EPOCH_CLEARED);
    if (!check(&c, ok, "same-high-identities-and-frozen-decision-through-drain")) goto cleanup; /* 24 */
    if (!check(&c, cleanup_gate(&c), "close-owned-gate-after-callback-left")) goto cleanup; /* 25 */
    ok = once(&c, RECOVERY_EPOCH_SNAPSHOT) && native_zero(&c.frame) && !c.frame.pe_watches && !c.frame.pe_timers &&
        !c.frame.event_owners && !c.frame.hook_users && (c.frame.pe_state & RECOVERY_EPOCH_EVENT_CLOSED) &&
        !(c.frame.flags & RECOVERY_EPOCH_TARGET_ROOT_PRESENT);
    if (!check(&c, ok, "final-pure-snapshot-all-observed-owners-zero")) goto cleanup; /* 26 */
    if (!check(&c, FreeLibrary(c.module), "release-driver")) goto cleanup;            /* 27 */
    c.module = NULL;
    if (!check(&c, UnregisterClassW(class_name, instance), "unregister-root-class")) goto cleanup; /* 28 */
    atom = 0;
    check(&c, !c.io_failed, "json-output-healthy");                                  /* 29 */

cleanup:
    /* Unblock before any further window cleanup can wait for another thread. */
    if (c.module && c.fixture && c.armed && !c.gate_closed)
        clean = cleanup_gate(&c) && clean;
    if (c.root)
    {
        if (DestroyWindow(c.root)) { c.root = NULL; c.root_destroyed = TRUE; }
        else clean = FALSE;
    }
    if (c.module && c.fixture && c.armed)
    {
        /* Gate cleanup above runs even before owner discovery. Root destruction
         * still precedes the only NT flag clear, including failed-case cleanup. */
        if (c.owner_id && !c.cleared)
        {
            if (!c.root_destroyed || (c.clear_attempted && !c.clear_retryable)) clean = FALSE;
            else clean = clear_owned(&c, TRUE) && clean;
        }
        if (!c.settled) c.settled = poll_snapshot(&c, POLL_ZERO);
        clean = c.settled && c.gate_closed && clean;
    }
    if (c.module) clean = FreeLibrary(c.module) && clean;
    if (atom) clean = UnregisterClassW(class_name, instance) && clean;
    emit(&c, "{\"type\":\"recovery-epoch\",\"event\":\"summary\",\"bits\":%u,\"pid\":%u,\"tid\":%u,"
            "\"birth\":%llu,\"checks\":%u,\"expected_checks\":%u,\"primary_check\":%u,"
            "\"failures\":%u,\"snapshots\":%u,\"root_destroyed\":%s,\"release_applied\":%s,"
            "\"clear_applied\":%s,\"settled\":%s,\"gate_closed\":%s,\"cleanup\":%s,\"primary\":",
         (unsigned)(8 * sizeof(void *)), (unsigned)GetCurrentProcessId(), (unsigned)GetCurrentThreadId(),
         (unsigned long long)c.birth, c.checks, RECOVERY_EPOCH_EXPECTED_CHECKS, RECOVERY_EPOCH_PRIMARY_CHECK,
         c.failures, c.snapshots, c.root_destroyed ? "true" : "false", c.released ? "true" : "false",
         c.cleared ? "true" : "false", c.settled ? "true" : "false", c.gate_closed ? "true" : "false",
         clean ? "true" : "false");
    if (c.primary_seen) emit(&c, "{" FRAME_FORMAT "}", FRAME_VALUES(&c.primary));
    else emit(&c, "null");
    emit(&c, ",\"held\":");
    if (c.held_seen) emit(&c, "{" FRAME_FORMAT "}", FRAME_VALUES(&c.held));
    else emit(&c, "null");
    emit(&c, ",\"final\":{" FRAME_FORMAT "},\"io_error\":%s}\n",
         FRAME_VALUES(&c.frame), c.io_failed ? "true" : "false");
    return c.failures || c.io_failed || !clean || c.checks != RECOVERY_EPOCH_EXPECTED_CHECKS ||
        !c.held_seen || !c.primary_seen || !c.root_destroyed || !c.released || !c.cleared ||
        !c.settled || !c.gate_closed ? 1 : 0;
}
