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
    HWND root, child;
    UINT32 root_value, child_value, context_id;
    UINT64 lease, ticket, epoch, birth;
    LONGLONG frequency;
    struct partial_model_fixture frame, initial, primary, settled_frame;
    unsigned checks, failures, snapshots;
    BOOL io_failed, setup_called, identity, ever_context, revoked, submitted;
    BOOL initial_seen, primary_seen, primary_bad, settled, context_released, finished;
};
static void emit(struct client *c, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format); result = vprintf(format, args); va_end(args);
    if (result < 0 || ferror(stdout)) c->io_failed = TRUE;
}
static void end_line(struct client *c)
{
    emit(c, "\n");
    if (fflush(stdout) == EOF || ferror(stdout)) c->io_failed = TRUE;
}
static void frame_json(struct client *c, const struct partial_model_fixture *f)
{
    emit(c, "{");
#define F32(n) emit(c, "\"" #n "\":%u,", (unsigned)f->n)
#define F64(n) emit(c, "\"" #n "\":%llu,", (unsigned long long)f->n)
    F32(size); F32(version); F32(operation); F32(status);
    F32(root); F32(child); F32(context_id); F32(flags);
    F64(root_epoch); F64(lease); F64(ticket); F64(generation); F64(install_result);
    F64(witness_ticket); F64(producer_birth); F64(witness_serial);
    F32(producer_valid); F32(active); F32(watched); F32(finalized);
    F32(completion_seen); F32(process_open); F32(cleanup_inflight); F32(request_inflight);
    F32(pending); F32(retirement_total); F32(native_producers); F32(native_watches);
    F32(native_setups); F32(native_cleanup); F32(model_jobs); F32(held);
    F32(cocoa_valid); F32(attached); F32(indexed); F32(ledger);
    F32(attach_faults); F32(rollback_faults); F32(witness_seen); F32(witness_removed);
    F32(witness_valid); F32(witness_attached); F32(witness_indexed); F32(witness_ledger);
    F32(witness_faults); F32(witness_rollback_faults); F32(pe_watches); F32(pe_timers);
    F32(context_owned); F32(window_owned); F32(producer_owned); F32(release_attempted);
    F32(revoke_error); F32(cocoa_error); F32(witness_error);
#undef F32
#undef F64
    emit(c, "\"reserved\":%u}", (unsigned)f->reserved);
}
static BOOL check(struct client *c, BOOL value, const char *name)
{
    ++c->checks;
    if (!value) ++c->failures;
    emit(c, "{\"type\":\"host-partial-model\",\"event\":\"check\",\"check\":%u,"
            "\"name\":\"%s\",\"ok\":%s}", c->checks, name, value ? "true" : "false");
    end_line(c);
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
    d.end = d.start + 4 * c->frequency; d.valid = TRUE;
    return d;
}
static BOOL within(const struct deadline *d)
{
    LONGLONG now;
    return d->valid && ticks(&now) && now >= d->start && now < d->end;
}
static BOOL pace(const struct deadline *d)
{
    if (!within(d)) return FALSE;
    Sleep(5); /* pacing only; never a readiness or ordering oracle */
    return within(d);
}
static BOOL shape(const struct client *c, unsigned op)
{
    const struct partial_model_fixture *f = &c->frame;
    return f->size == sizeof(*f) && f->version == PARTIAL_MODEL_VERSION && f->operation == op &&
        f->root == c->root_value && f->child == c->child_value && !f->reserved && !(f->flags & ~255u) &&
        f->producer_valid <= 1 && f->cocoa_valid <= 1 && f->witness_valid <= 1 && f->witness_seen <= 1 &&
        f->pe_watches != UINT32_MAX && f->pe_timers != UINT32_MAX;
}
static BOOL operation(struct client *c, unsigned op, const struct deadline *d, BOOL force)
{
    struct partial_model_fixture *f = &c->frame;
    BOOL start = within(d), returned, valid;
    NTSTATUS status;
    if (!start && !force) return FALSE;
    memset(f, 0, sizeof(*f));
    f->size = sizeof(*f); f->version = PARTIAL_MODEL_VERSION; f->operation = op;
    f->root = c->root_value; f->child = c->child_value;
    f->status = ERROR_INVALID_DATA; f->pe_watches = f->pe_timers = UINT32_MAX;
    if (op == PARTIAL_MODEL_SETUP) c->setup_called = TRUE;
    status = c->fixture(f);
    valid = shape(c, op);
    /* Track real owners/effects before rejecting a late return. CLEANUP may
     * already have released the context while its PE wrapper reports pending. */
    if (!status && valid)
    {
        if (f->context_owned || f->context_id) c->ever_context = TRUE;
        if (!c->identity && f->lease && f->ticket && f->root_epoch && f->context_id)
        {
            c->lease = f->lease; c->ticket = f->ticket; c->epoch = f->root_epoch;
            c->context_id = f->context_id; c->identity = TRUE;
        }
        if (f->flags & PARTIAL_MODEL_REVOKE_CALLED) c->revoked = TRUE;
        if (f->flags & PARTIAL_MODEL_SUBMIT_CALLED) c->submitted = TRUE;
        if ((f->flags & PARTIAL_MODEL_CONTEXT_RELEASED) && !f->context_owned) c->context_released = TRUE;
        if ((f->flags & PARTIAL_MODEL_FINISHED) && !f->window_owned && !f->producer_owned && !f->context_owned)
            c->finished = TRUE;
    }
    returned = within(d);
    ++c->snapshots;
    emit(c, "{\"type\":\"host-partial-model\",\"event\":\"snapshot\",\"snapshot\":%u,"
            "\"ntstatus\":%u,\"started_in_time\":%s,\"returned_in_time\":%s,\"frame\":",
         c->snapshots, (unsigned)(UINT32)status, start ? "true" : "false", returned ? "true" : "false");
    frame_json(c, f); emit(c, "}"); end_line(c);
    return start && returned && within(d) && !status && valid &&
        (!f->status || (op == PARTIAL_MODEL_CLEANUP && f->status == ERROR_IO_PENDING));
}
static BOOL once(struct client *c, unsigned op)
{
    struct deadline d = new_deadline(c);
    return operation(c, op, &d, FALSE) && !c->frame.status;
}
static BOOL identity_matches(const struct client *c)
{
    const struct partial_model_fixture *f = &c->frame;
    return c->identity && f->lease == c->lease && f->ticket == c->ticket && f->root_epoch == c->epoch &&
        f->context_id == c->context_id && f->generation == PARTIAL_MODEL_HIGH && f->producer_birth == c->birth;
}
static BOOL partial_present(const struct client *c)
{
    const struct partial_model_fixture *f = &c->frame;
    return identity_matches(c) && f->cocoa_valid == 1 && !f->cocoa_error && f->attached == 1 &&
        !f->indexed && f->ledger == 1 && f->attach_faults == 1 && f->rollback_faults == 1 &&
        f->context_owned == 1 && f->window_owned == 1 && f->producer_owned == 1 && !f->release_attempted;
}
static BOOL native_empty(const struct partial_model_fixture *f)
{
    return !f->held && !f->retirement_total && !f->native_producers && !f->native_watches &&
        !f->native_setups && !f->native_cleanup && f->producer_valid == 1 && !f->active &&
        !f->watched && f->finalized == 1 && !f->process_open && !f->cleanup_inflight &&
        !f->request_inflight && !f->pending;
}
enum poll_kind { HELD_COMPLETION, WITNESS, DRAINED };
static BOOL poll_snapshot(struct client *c, enum poll_kind kind)
{
    struct deadline d = new_deadline(c);
    for (;;)
    {
        const struct partial_model_fixture *f = &c->frame;
        BOOL ready;
        if (!operation(c, PARTIAL_MODEL_SNAPSHOT, &d, FALSE)) return FALSE;
        if (!IsWindow(c->root) || !IsWindow(c->child) || !within(&d)) return FALSE;
        if (kind == HELD_COMPLETION)
            ready = partial_present(c) && f->producer_valid == 1 && !f->active && f->watched == 1 &&
                !f->finalized && f->completion_seen == 1 && !f->process_open && !f->cleanup_inflight &&
                !f->request_inflight && f->pending == 1 && f->retirement_total == 1 &&
                f->held == 1 && !f->model_jobs && f->pe_watches == 1 && f->pe_timers;
        else if (kind == WITNESS) ready = f->witness_seen == 1;
        else ready = identity_matches(c) && native_empty(f) && f->cocoa_valid == 1 && !f->cocoa_error &&
            !f->attached && !f->indexed && !f->ledger && !f->pe_watches && !f->pe_timers;
        if (!within(&d)) return FALSE;
        if (ready) return TRUE;
        if (!pace(&d)) return FALSE;
    }
}
static BOOL cleanup_resources(struct client *c, BOOL one_attempt)
{
    struct deadline d = new_deadline(c);
    BOOL first = TRUE;
    for (;;)
    {
        /* Forced first invocation releases the held owner even if the clock
         * failed. Its late effects are latched but cannot pass readiness. */
        BOOL ok = operation(c, PARTIAL_MODEL_CLEANUP, &d, first);
        first = FALSE;
        if (!ok) return FALSE;
        if (!c->frame.status)
            return !c->frame.context_owned && (!c->ever_context || c->context_released) &&
                !c->frame.pe_watches && !c->frame.pe_timers && within(&d);
        if (one_attempt || c->frame.status != ERROR_IO_PENDING || !pace(&d)) return FALSE;
    }
}
static BOOL destroy_owned_window(struct client *c, HWND *window, BOOL force)
{
    struct deadline d = new_deadline(c);
    BOOL started = within(&d), destroyed;
    if (!*window) return TRUE;
    if (!started && !force) return FALSE;
    destroyed = DestroyWindow(*window);
    if (destroyed) *window = NULL; /* actual effect before late rejection */
    return started && destroyed && within(&d);
}
int main(int argc, char **argv)
{
    static const WCHAR class_name[] = L"CiderHostPartialModelV1";
    struct client c;
    WNDCLASSW cls;
    HINSTANCE instance = GetModuleHandleW(NULL);
    LARGE_INTEGER frequency;
    FILETIME created, exited, kernel, user;
    ATOM atom = 0;
    DWORD pid = 0, child_pid = 0, tid = 0, child_tid = 0;
    BOOL ok, internal_cleanup = TRUE;
    (void)argv;
    memset(&c, 0, sizeof(c)); memset(&cls, 0, sizeof(cls));
    if (!check(&c, argc == 1, "one-fresh-case")) goto cleanup;                       /* 1 */
    ok = QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 && frequency.QuadPart <= LLONG_MAX / 4;
    if (ok) c.frequency = frequency.QuadPart;
    if (!check(&c, ok, "monotonic-clock")) goto cleanup;                             /* 2 */
    cls.hInstance = instance; cls.lpfnWndProc = DefWindowProcW; cls.lpszClassName = class_name;
    atom = RegisterClassW(&cls);
    if (!check(&c, atom != 0, "register-real-window-class")) goto cleanup;           /* 3 */
    c.root = CreateWindowW(class_name, L"Cider partial model", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           20, 20, 360, 240, NULL, NULL, instance, NULL);
    c.root_value = (UINT32)(uintptr_t)c.root;
    if (!check(&c, c.root != NULL, "create-self-root")) goto cleanup;                /* 4 */
    c.child = CreateWindowW(class_name, L"partial child", WS_CHILD | WS_BORDER | WS_VISIBLE,
                            5, 5, 180, 100, c.root, NULL, instance, NULL);
    c.child_value = (UINT32)(uintptr_t)c.child;
    if (!check(&c, c.child != NULL, "create-bordered-child")) goto cleanup;          /* 5 */
    tid = GetWindowThreadProcessId(c.root, &pid); child_tid = GetWindowThreadProcessId(c.child, &child_pid);
    ok = pid == GetCurrentProcessId() && child_pid == pid && tid == GetCurrentThreadId() && child_tid == tid &&
        GetAncestor(c.child, GA_ROOT) == c.root && GetParent(c.child) == c.root &&
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    if (ok) c.birth = (UINT64)created.dwLowDateTime | ((UINT64)created.dwHighDateTime << 32);
    if (!check(&c, ok && c.birth, "actual-self-topology-pid-tid-birth")) goto cleanup; /* 6 */
    c.module = LoadLibraryW(L"winemac.drv");
    if (!check(&c, c.module != NULL, "load-private-driver")) goto cleanup;           /* 7 */
    c.fixture = (fixture_fn)(uintptr_t)GetProcAddress(c.module, "cider_partial_model_fixture");
    if (!check(&c, c.fixture != NULL, "resolve-partial-model-fixture")) goto cleanup; /* 8 */
    if (!check(&c, once(&c, PARTIAL_MODEL_SETUP), "actual-acquire-context-install-return")) goto cleanup; /* 9 */
    ok = identity_matches(&c) && c.epoch >= PARTIAL_MODEL_HIGH && c.lease >= PARTIAL_MODEL_HIGH &&
        c.ticket >= PARTIAL_MODEL_HIGH && c.context_id && !c.frame.install_result &&
        (c.frame.flags & PARTIAL_MODEL_SETUP_RETURNED);
    if (!check(&c, ok, "immutable-high-identities-actual-install-zero")) goto cleanup; /* 10 */
    ok = partial_present(&c) && c.frame.producer_valid == 1 && c.frame.active == 1 &&
        c.frame.watched == 1 && !c.frame.finalized && c.frame.pending == 1 && c.frame.retirement_total == 1 &&
        c.frame.held == 1 && !c.frame.model_jobs && c.frame.pe_watches == 1 && c.frame.pe_timers &&
        !c.frame.witness_seen;
    if (!check(&c, ok, "real-two-exceptions-attached-partial-index0-ledger1-held")) goto cleanup; /* 11 */
    c.initial = c.frame; c.initial_seen = TRUE;
    if (!check(&c, IsWindow(c.root) && IsWindow(c.child), "root-and-child-stay-live")) goto cleanup; /* 12 */
    if (!check(&c, once(&c, PARTIAL_MODEL_REVOKE) && c.revoked && !c.frame.revoke_error,
               "actual-producer-revoke-and-stop")) goto cleanup;                   /* 13 */
    if (!check(&c, poll_snapshot(&c, HELD_COMPLETION), "completion-held-pending-process-closed")) goto cleanup; /* 14 */
    if (!check(&c, IsWindow(c.root) && IsWindow(c.child) && partial_present(&c),
               "root-live-through-held-partial-observation")) goto cleanup;       /* 15 */
    if (!check(&c, once(&c, PARTIAL_MODEL_SUBMIT_HELD) && c.submitted && !c.frame.held,
               "ordinary-submit-transferred-retirement-owner")) goto cleanup;      /* 16 */
    if (!check(&c, poll_snapshot(&c, WITNESS), "actual-checked-removal-witness-arrived")) goto cleanup; /* 17 */
    ok = identity_matches(&c) && c.frame.witness_valid == 1 && !c.frame.witness_error &&
        c.frame.witness_ticket == c.ticket && c.frame.witness_serial == 1 && c.frame.witness_removed == 1 &&
        c.frame.witness_faults == 1 && c.frame.witness_rollback_faults == 1 && !c.frame.witness_indexed &&
        c.frame.witness_attached <= 1 && c.frame.witness_ledger == c.frame.witness_attached &&
        c.frame.cocoa_valid == 1 && !c.frame.cocoa_error && c.frame.context_owned == 1 &&
        !c.frame.release_attempted && IsWindow(c.root) && IsWindow(c.child);
    if (!check(&c, ok, "bound-true-removal-with-qualified-partial-exception-history")) goto cleanup; /* 18 */
    c.primary = c.frame; c.primary_seen = TRUE;
    c.primary_bad = c.primary.witness_removed && c.primary.witness_attached;
    if (!check(&c, !c.primary_bad, "actual-successful-removal-must-not-leave-attached-partial")) goto cleanup; /* 19 */
    if (!check(&c, poll_snapshot(&c, DRAINED), "actual-model-native-watch-and-pe-owner-zero")) goto cleanup; /* 20 */
    c.settled = TRUE; c.settled_frame = c.frame;
    if (!check(&c, cleanup_resources(&c, FALSE), "release-context-only-after-real-model-absence")) goto cleanup; /* 21 */
    ok = c.context_released && !c.frame.context_owned && c.frame.release_attempted == 1 &&
        c.frame.window_owned == 1 && c.frame.producer_owned == 1;
    if (!check(&c, ok, "actual-context-release-effect-recorded")) goto cleanup;      /* 22 */
    ok = once(&c, PARTIAL_MODEL_SNAPSHOT) && native_empty(&c.frame) && c.frame.cocoa_valid == 1 &&
        !c.frame.attached && !c.frame.indexed && !c.frame.ledger && !c.frame.pe_watches && !c.frame.pe_timers &&
        IsWindow(c.root) && IsWindow(c.child);
    if (!check(&c, ok, "pure-zero-observation-before-window-destruction")) goto cleanup; /* 23 */
    if (!check(&c, destroy_owned_window(&c, &c.child, FALSE), "destroy-child-after-primary-and-context-release")) goto cleanup; /* 24 */
    if (!check(&c, destroy_owned_window(&c, &c.root, FALSE), "destroy-root-after-primary-and-context-release")) goto cleanup; /* 25 */
    if (!check(&c, once(&c, PARTIAL_MODEL_FINISH), "release-fixture-producer-window-pins")) goto cleanup; /* 26 */
    ok = c.finished && !c.frame.context_owned && !c.frame.window_owned && !c.frame.producer_owned &&
        !c.frame.native_producers && !c.frame.native_watches && !c.frame.native_setups && !c.frame.native_cleanup &&
        !c.frame.retirement_total && !c.frame.pe_watches && !c.frame.pe_timers;
    if (!check(&c, ok, "final-owned-resources-and-observed-registries-zero")) goto cleanup; /* 27 */
    if (!check(&c, FreeLibrary(c.module), "release-module-reference")) goto cleanup; /* 28 */
    c.module = NULL;
    if (!check(&c, UnregisterClassW(class_name, instance), "unregister-window-class")) goto cleanup; /* 29 */
    atom = 0;
    check(&c, !c.io_failed, "json-output-healthy");                                  /* 30 */

cleanup:
    if (c.module && c.fixture && c.setup_called && !c.finished)
    {
        /* A qualified negative gets one honest cleanup attempt. If the actual
         * model remains, retain its context and live windows through process
         * exit/external cleanup; do not erase the evidence by destroying root. */
        internal_cleanup = cleanup_resources(&c, c.primary_bad);
        if (internal_cleanup)
        {
            if (c.child) internal_cleanup = destroy_owned_window(&c, &c.child, TRUE) && internal_cleanup;
            if (c.root && internal_cleanup)
                internal_cleanup = destroy_owned_window(&c, &c.root, TRUE) && internal_cleanup;
            if (internal_cleanup) internal_cleanup = once(&c, PARTIAL_MODEL_FINISH) && c.finished;
        }
    }
    else if (!c.setup_called)
    {
        if (c.child) internal_cleanup = destroy_owned_window(&c, &c.child, TRUE) && internal_cleanup;
        if (c.root) internal_cleanup = destroy_owned_window(&c, &c.root, TRUE) && internal_cleanup;
    }
    if (c.module && internal_cleanup)
    { if (FreeLibrary(c.module)) c.module = NULL; else internal_cleanup = FALSE; }
    if (atom && !c.root && !c.child)
    { if (UnregisterClassW(class_name, instance)) atom = 0; else internal_cleanup = FALSE; }
    emit(&c, "{\"type\":\"host-partial-model\",\"event\":\"summary\",\"bits\":%u,\"pid\":%u,\"tid\":%u,"
            "\"birth\":%llu,\"checks\":%u,\"expected_checks\":%u,\"primary_check\":%u,\"failures\":%u,"
            "\"snapshots\":%u,\"primary_seen\":%s,\"primary_bad\":%s,\"context_release_applied\":%s,"
            "\"root_live\":%s,\"child_live\":%s,\"module_owned\":%s,\"internal_cleanup\":%s,"
            "\"external_cleanup_required\":%s,\"initial\":",
         (unsigned)(8 * sizeof(void *)), (unsigned)GetCurrentProcessId(), (unsigned)GetCurrentThreadId(),
         (unsigned long long)c.birth, c.checks, PARTIAL_MODEL_EXPECTED_CHECKS, PARTIAL_MODEL_PRIMARY_CHECK,
         c.failures, c.snapshots, c.primary_seen ? "true" : "false", c.primary_bad ? "true" : "false",
         c.context_released ? "true" : "false", c.root && IsWindow(c.root) ? "true" : "false",
         c.child && IsWindow(c.child) ? "true" : "false", c.module ? "true" : "false",
         internal_cleanup ? "true" : "false", internal_cleanup ? "false" : "true");
    if (c.initial_seen) frame_json(&c, &c.initial); else emit(&c, "null");
    emit(&c, ",\"primary\":");
    if (c.primary_seen) frame_json(&c, &c.primary); else emit(&c, "null");
    emit(&c, ",\"settled\":");
    if (c.settled) frame_json(&c, &c.settled_frame); else emit(&c, "null");
    emit(&c, ",\"last\":"); frame_json(&c, &c.frame);
    emit(&c, ",\"io_error\":%s}", c.io_failed ? "true" : "false"); end_line(&c);
    return c.failures || c.io_failed || !internal_cleanup || !c.finished || !c.primary_seen || c.primary_bad ||
        !c.context_released || c.checks != PARTIAL_MODEL_EXPECTED_CHECKS ? 1 : 0;
}
