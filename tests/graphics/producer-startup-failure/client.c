#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define EXPECTED_CHECKS 25u
#define KNOWN_FLAGS 0x3ffffu
typedef NTSTATUS (WINAPI *fixture_fn)(void *);

struct deadline { LONGLONG start, end; BOOL valid; };
struct client
{
    fixture_fn fixture, probe;
    struct producer_startup_failure_fixture frame, pending;
    struct producer_startup_owner_probe probe_frame;
    HWND root;
    HMODULE module;
    LONGLONG frequency;
    uint64_t birth, owner_id;
    unsigned mode, checks, failures, snapshots, probes;
    BOOL io_failed, run_called, pending_seen, cleared, settled, root_destroyed;
    BOOL clear_attempted, clear_retryable;
};

#define FRAME_FORMAT \
    "\"size\":%u,\"version\":%u,\"operation\":%u,\"status\":%u," \
    "\"mode\":%u,\"root\":%u,\"acquire_error\":%u,\"flags\":%u," \
    "\"owner_id\":%llu,\"returned_lease\":%llu,\"producers\":%u,\"watches\":%u," \
    "\"setups\":%u,\"cleanup\":%u,\"models\":%u,\"owner_inflight\":%u," \
    "\"pe_watches\":%u,\"pe_timers\":%u,\"last_close\":%u," \
    "\"query_status\":%u,\"set_status\":%u,\"reserved\":%u"
#define FRAME_VALUES(f) \
    (unsigned)(f)->size, (unsigned)(f)->version, (unsigned)(f)->operation, (unsigned)(f)->status, \
    (unsigned)(f)->mode, (unsigned)(f)->root, (unsigned)(f)->acquire_error, (unsigned)(f)->flags, \
    (unsigned long long)(f)->owner_id, (unsigned long long)(f)->returned_lease, \
    (unsigned)(f)->producers, (unsigned)(f)->watches, (unsigned)(f)->setups, (unsigned)(f)->cleanup, \
    (unsigned)(f)->models, (unsigned)(f)->owner_inflight, (unsigned)(f)->pe_watches, \
    (unsigned)(f)->pe_timers, (unsigned)(f)->last_close, (unsigned)(f)->query_status, \
    (unsigned)(f)->set_status, (unsigned)(f)->reserved

static void emit(struct client *client, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
    if (result < 0 || fflush(stdout) == EOF) client->io_failed = TRUE;
}

static BOOL check(struct client *client, BOOL value, const char *name)
{
    ++client->checks;
    if (!value) ++client->failures;
    emit(client, "{\"type\":\"producer-startup-failure\",\"event\":\"check\","
                 "\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",
         client->checks, name, value ? "true" : "false");
    return value;
}

static BOOL ticks(LONGLONG *out)
{
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now) || now.QuadPart < 0) return FALSE;
    *out = now.QuadPart;
    return TRUE;
}

static struct deadline new_deadline(const struct client *client)
{
    struct deadline d = {0};
    if (client->frequency <= 0 || client->frequency > LLONG_MAX / 4 || !ticks(&d.start) ||
        d.start > LLONG_MAX - 4 * client->frequency) return d;
    d.end = d.start + 4 * client->frequency;
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
    Sleep(10); /* pacing only; every accepted observation comes from an actual call */
    return within(d);
}

static BOOL frame_shape(const struct client *client, unsigned operation)
{
    const struct producer_startup_failure_fixture *f = &client->frame;
    return f->size == sizeof(*f) && f->version == PRODUCER_STARTUP_FIXTURE_VERSION &&
        f->operation == operation && f->mode == client->mode && !f->reserved &&
        f->root == (uint32_t)(uintptr_t)client->root && !(f->flags & ~KNOWN_FLAGS);
}

static BOOL operation(struct client *client, unsigned op, const struct deadline *d, BOOL force)
{
    struct producer_startup_failure_fixture *f = &client->frame;
    BOOL started = within(d), finished, shape;
    NTSTATUS status;

    if (!started && !force)
    {
        emit(client, "{\"type\":\"producer-startup-failure\",\"event\":\"skipped\","
                     "\"operation\":%u,\"reason\":\"deadline-or-clock\"}\n", op);
        return FALSE;
    }
    f->size = sizeof(*f); f->version = PRODUCER_STARTUP_FIXTURE_VERSION;
    f->operation = op; f->mode = client->mode; f->root = (uint32_t)(uintptr_t)client->root;
    f->status = ERROR_INVALID_DATA; f->flags = 0; f->reserved = 0;
    f->pe_watches = f->pe_timers = UINT32_MAX; /* wrapper must postfill actual counts */
    if (op == PRODUCER_STARTUP_RUN) client->run_called = TRUE;
    if (op == PRODUCER_STARTUP_CLEAR)
    { client->clear_attempted = TRUE; client->clear_retryable = FALSE; }
    status = client->fixture(f);
    shape = frame_shape(client, op);
    /* Applied effect and resource capture are tracked before timing rejection. */
    if (!status && shape && (f->flags & PRODUCER_STARTUP_OWNER_CAPTURED) && f->owner_id)
        client->owner_id = f->owner_id;
    if (!status && shape && op == PRODUCER_STARTUP_CLEAR && !f->status &&
        (f->flags & PRODUCER_STARTUP_CLEARED)) client->cleared = TRUE;
    if (!status && shape && op == PRODUCER_STARTUP_CLEAR && f->status == ERROR_IO_PENDING &&
        !(f->flags & PRODUCER_STARTUP_CLEARED)) client->clear_retryable = TRUE;
    finished = within(d);
    ++client->snapshots;
    emit(client, "{\"type\":\"producer-startup-failure\",\"event\":\"snapshot\","
                 "\"snapshot\":%u,\"ntstatus\":%u,\"started_in_time\":%s,"
                 "\"returned_in_time\":%s,\"frame\":{" FRAME_FORMAT "}}\n",
         client->snapshots, (unsigned)(uint32_t)status, started ? "true" : "false",
         finished ? "true" : "false", FRAME_VALUES(f));
    return started && finished && within(d) && !status && shape &&
        (!f->status || (op == PRODUCER_STARTUP_CLEAR && f->status == ERROR_IO_PENDING));
}

static BOOL probe(struct client *client)
{
    struct producer_startup_owner_probe *p = &client->probe_frame;
    struct deadline d = new_deadline(client);
    NTSTATUS status;
    BOOL started = within(&d), returned;

    if (!started) return FALSE;
    memset(p, 0, sizeof(*p)); p->size = sizeof(*p); p->version = 2;
    p->target_lease = client->mode == PRODUCER_STARTUP_SECTION_FAILURE ? client->owner_id : 0;
    status = client->probe(p);
    returned = within(&d);
    ++client->probes;
    emit(client, "{\"type\":\"producer-startup-failure\",\"event\":\"probe\","
                 "\"probe\":%u,\"ntstatus\":%u,\"returned_in_time\":%s,\"size\":%u,"
                 "\"version\":%u,\"roots\":%u,\"producers\":%u,\"watch_pins\":%u,"
                 "\"hosts\":%u,\"pe_watches\":%u,\"cleanup_pins\":%u,"
                 "\"target_lease\":%llu,\"target_active\":%u,\"target_watched\":%u}\n",
         client->probes, (unsigned)(uint32_t)status, returned ? "true" : "false",
         (unsigned)p->size, (unsigned)p->version, (unsigned)p->roots, (unsigned)p->producers,
         (unsigned)p->watch_pins, (unsigned)p->hosts, (unsigned)p->pe_watches,
         (unsigned)p->cleanup_pins, (unsigned long long)p->target_lease,
         (unsigned)p->target_active, (unsigned)p->target_watched);
    return returned && within(&d) && !status && p->size == sizeof(*p) && p->version == 2 &&
        p->target_lease == (client->mode == PRODUCER_STARTUP_SECTION_FAILURE ? client->owner_id : 0);
}

static BOOL retained(const struct client *client)
{
    const struct producer_startup_failure_fixture *f = &client->frame;
    uint32_t common = PRODUCER_STARTUP_RUN_RETURNED | PRODUCER_STARTUP_OWNER_CAPTURED |
                      PRODUCER_STARTUP_OWNER_PRESENT;
    if (!!(f->flags & PRODUCER_STARTUP_TARGET_ROOT_PRESENT) == !!client->root_destroyed) return FALSE;
    if ((f->flags & common) != common || !f->owner_id || f->owner_id != client->owner_id ||
        f->returned_lease || !f->acquire_error || f->producers || f->models || f->pe_watches ||
        !f->pe_timers || f->pe_timers == UINT32_MAX ||
        (f->flags & (PRODUCER_STARTUP_OWNER_ACTIVE | PRODUCER_STARTUP_PROCESS_OPEN |
                     PRODUCER_STARTUP_WATCH_STARTED | PRODUCER_STARTUP_CLEARED))) return FALSE;
    if (client->mode == PRODUCER_STARTUP_SECTION_FAILURE)
        return f->watches == 1 && !f->setups &&
            (f->flags & (PRODUCER_STARTUP_STATUS_OWNER | PRODUCER_STARTUP_STATUS_CLEANUP |
                         PRODUCER_STARTUP_COMPLETION_SEEN)) ==
            (PRODUCER_STARTUP_STATUS_OWNER | PRODUCER_STARTUP_STATUS_CLEANUP |
             PRODUCER_STARTUP_COMPLETION_SEEN);
    return !f->watches && f->setups == 1 &&
        !(f->flags & (PRODUCER_STARTUP_STATUS_OWNER | PRODUCER_STARTUP_STATUS_CLEANUP |
                      PRODUCER_STARTUP_COMPLETION_SEEN)) &&
        (f->flags & PRODUCER_STARTUP_LAST_CLOSE_VALID) &&
        f->last_close == (uint32_t)STATUS_HANDLE_NOT_CLOSABLE;
}

static BOOL empty(const struct client *client)
{
    const struct producer_startup_failure_fixture *f = &client->frame;
    uint32_t live = PRODUCER_STARTUP_OWNER_PRESENT | PRODUCER_STARTUP_OWNER_ACTIVE |
        PRODUCER_STARTUP_OWNER_BUSY | PRODUCER_STARTUP_CLEANUP_QUEUED |
        PRODUCER_STARTUP_STATUS_OWNER | PRODUCER_STARTUP_STATUS_CLEANUP |
        PRODUCER_STARTUP_PROCESS_OPEN | PRODUCER_STARTUP_WATCH_STARTED |
        PRODUCER_STARTUP_COMPLETION_SEEN | PRODUCER_STARTUP_TARGET_ROOT_PRESENT;
    return !f->producers && !f->watches && !f->setups && !f->cleanup && !f->models &&
        !f->owner_inflight && !f->pe_watches && !f->pe_timers && !(f->flags & live);
}

static BOOL poll_snapshot(struct client *client, BOOL want_empty)
{
    struct deadline d = new_deadline(client);
    for (;;)
    {
        BOOL ready;
        if (!operation(client, PRODUCER_STARTUP_SNAPSHOT, &d, FALSE)) return FALSE;
        /* Any PE process-watch object disqualifies the no-watch experiment. */
        if (client->frame.pe_watches && !client->failures) return FALSE;
        ready = want_empty ? empty(client) : retained(client);
        if (!within(&d)) return FALSE;
        if (ready) return TRUE;
        if (!pause_poll(&d)) return FALSE;
    }
}

static BOOL clear_owned(struct client *client, BOOL cleanup_attempt)
{
    struct deadline d = new_deadline(client);
    BOOL first = TRUE;
    for (;;)
    {
        BOOL ok = operation(client, PRODUCER_STARTUP_CLEAR, &d, first && cleanup_attempt);
        first = FALSE;
        if (!ok) return FALSE; /* an actual clear is still latched if it returned late */
        if (client->frame.pe_watches) return FALSE;
        if (!client->frame.status) return client->cleared && within(&d);
        if (client->frame.status != ERROR_IO_PENDING || !pause_poll(&d)) return FALSE;
    }
}

int main(int argc, char **argv)
{
    static const WCHAR class_name[] = L"CiderProducerStartupFailureV1";
    struct client client;
    WNDCLASSW cls;
    HINSTANCE instance = GetModuleHandleW(NULL);
    ATOM atom = 0;
    LARGE_INTEGER frequency;
    FILETIME created, exited, kernel, user;
    DWORD pid = 0, tid = 0, expected_error = 0;
    struct deadline d;
    BOOL clean = TRUE, ok;

    memset(&client, 0, sizeof(client)); memset(&cls, 0, sizeof(cls));
    if (argc == 3 && !strcmp(argv[1], "--mode"))
    {
        if (!strcmp(argv[2], "1")) client.mode = PRODUCER_STARTUP_SECTION_FAILURE;
        if (!strcmp(argv[2], "2")) client.mode = PRODUCER_STARTUP_THREAD_FAILURE;
    }
    if (!check(&client, client.mode != 0, "one-mode-one-fresh-process")) goto cleanup; /* 1 */
    ok = QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 &&
         frequency.QuadPart <= LLONG_MAX / 4;
    if (ok) client.frequency = frequency.QuadPart;
    if (!check(&client, ok, "monotonic-clock")) goto cleanup;                         /* 2 */
    cls.hInstance = instance; cls.lpfnWndProc = DefWindowProcW; cls.lpszClassName = class_name;
    atom = RegisterClassW(&cls);
    if (!check(&client, atom != 0, "register-root-class")) goto cleanup;              /* 3 */
    client.root = CreateWindowW(class_name, L"Cider startup failure", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                20, 20, 360, 240, NULL, NULL, instance, NULL);
    if (!check(&client, client.root != NULL, "create-real-root")) goto cleanup;       /* 4 */
    tid = GetWindowThreadProcessId(client.root, &pid);
    ok = tid == GetCurrentThreadId() && pid == GetCurrentProcessId() &&
         GetAncestor(client.root, GA_ROOT) == client.root &&
         GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    if (ok) client.birth = (uint64_t)created.dwLowDateTime | ((uint64_t)created.dwHighDateTime << 32);
    if (!check(&client, ok && client.birth != 0, "actual-self-pid-tid-birth-root")) goto cleanup; /* 5 */
    client.module = LoadLibraryW(L"winemac.drv");
    if (!check(&client, client.module != NULL, "load-private-driver")) goto cleanup; /* 6 */
    client.fixture = (fixture_fn)(uintptr_t)GetProcAddress(client.module, "cider_producer_startup_failure_fixture");
    if (!check(&client, client.fixture != NULL, "resolve-startup-fixture")) goto cleanup; /* 7 */
    client.probe = (fixture_fn)(uintptr_t)GetProcAddress(client.module, "cider_remote_transport_probe");
    if (!check(&client, client.probe != NULL, "resolve-existing-probe")) goto cleanup; /* 8 */
    ok = probe(&client) && client.probe_frame.roots >= 1 && !client.probe_frame.producers &&
         !client.probe_frame.watch_pins && !client.probe_frame.hosts &&
         !client.probe_frame.pe_watches && !client.probe_frame.cleanup_pins;
    if (!check(&client, ok, "fresh-native-and-pe-baseline")) goto cleanup;            /* 9 */

    d = new_deadline(&client);
    ok = operation(&client, PRODUCER_STARTUP_RUN, &d, FALSE);
    if (!check(&client, ok, "ordinary-acquire-return-and-owner-capture")) goto cleanup; /* 10 */
    expected_error = client.mode == PRODUCER_STARTUP_SECTION_FAILURE ? ERROR_ACCESS_DENIED :
        RtlNtStatusToDosError(STATUS_HANDLE_NOT_CLOSABLE);
    ok = !client.frame.returned_lease && client.frame.acquire_error == expected_error &&
         client.owner_id && (client.frame.flags & (PRODUCER_STARTUP_RUN_RETURNED |
         PRODUCER_STARTUP_OWNER_CAPTURED)) == (PRODUCER_STARTUP_RUN_RETURNED | PRODUCER_STARTUP_OWNER_CAPTURED);
    if (!check(&client, ok, "no-lease-exact-real-acquisition-error")) goto cleanup;     /* 11 */
    ok = client.mode == PRODUCER_STARTUP_THREAD_FAILURE ?
        (client.frame.flags & PRODUCER_STARTUP_LAST_CLOSE_VALID) &&
        client.frame.last_close == (uint32_t)STATUS_HANDLE_NOT_CLOSABLE :
        !(client.frame.flags & PRODUCER_STARTUP_LAST_CLOSE_VALID) && client.frame.last_close == UINT32_MAX;
    if (!check(&client, ok, "close-code-witness-or-explicitly-unavailable")) goto cleanup; /* 12 */
    if (!check(&client, poll_snapshot(&client, FALSE), "retained-owner-no-pe-watch-timer-live")) goto cleanup; /* 13 */
    ok = probe(&client) && client.probe_frame.roots >= 1 && !client.probe_frame.producers &&
         !client.probe_frame.hosts && !client.probe_frame.pe_watches && client.probe_frame.cleanup_pins &&
         !client.probe_frame.target_active &&
         client.probe_frame.watch_pins == (client.mode == PRODUCER_STARTUP_SECTION_FAILURE ? 1u : 0u) &&
         client.probe_frame.target_watched == (client.mode == PRODUCER_STARTUP_SECTION_FAILURE ? 1u : 0u);
    if (!check(&client, ok, "independent-read-only-pending-probe")) goto cleanup;      /* 14 */
    if (!check(&client, DestroyWindow(client.root), "destroy-root-before-clear")) goto cleanup; /* 15 */
    client.root = NULL; client.root_destroyed = TRUE;
    ok = probe(&client) && !client.probe_frame.producers &&
         !client.probe_frame.hosts && !client.probe_frame.pe_watches && client.probe_frame.cleanup_pins;
    if (!check(&client, ok, "root-registry-gone-owner-still-retained")) goto cleanup; /* 16 */
    if (!check(&client, poll_snapshot(&client, FALSE), "retention-survives-root-destroy")) goto cleanup; /* 17 */
    client.pending = client.frame; client.pending_seen = TRUE;
    if (!check(&client, clear_owned(&client, FALSE), "actual-clear-within-4s")) goto cleanup; /* 18 */
    ok = client.cleared && (client.frame.flags & (PRODUCER_STARTUP_CLEARED |
        PRODUCER_STARTUP_QUERY_ATTEMPTED | PRODUCER_STARTUP_SET_ATTEMPTED | PRODUCER_STARTUP_WAS_PROTECTED)) ==
        (PRODUCER_STARTUP_CLEARED | PRODUCER_STARTUP_QUERY_ATTEMPTED |
         PRODUCER_STARTUP_SET_ATTEMPTED | PRODUCER_STARTUP_WAS_PROTECTED) &&
        !client.frame.query_status && !client.frame.set_status && !client.frame.pe_watches &&
        (client.mode != PRODUCER_STARTUP_SECTION_FAILURE ||
         (client.frame.flags & PRODUCER_STARTUP_RETRY_UNDER_CLEAR_PIN));
    if (!check(&client, ok, "real-query-set-success-protected-handle")) goto cleanup; /* 19 */
    client.settled = poll_snapshot(&client, TRUE);
    if (!check(&client, client.settled, "autonomous-native-and-pe-timer-zero")) goto cleanup; /* 20 */
    ok = probe(&client) && !client.probe_frame.producers &&
         !client.probe_frame.watch_pins && !client.probe_frame.hosts && !client.probe_frame.pe_watches &&
         !client.probe_frame.cleanup_pins && !client.probe_frame.target_active && !client.probe_frame.target_watched;
    if (!check(&client, ok, "independent-final-zero-probe")) goto cleanup;             /* 21 */
    ok = client.owner_id == client.frame.owner_id && client.pending_seen && client.root_destroyed &&
         client.cleared && (client.frame.flags & PRODUCER_STARTUP_CLEARED) &&
         !client.frame.returned_lease && client.frame.acquire_error == expected_error;
    if (!check(&client, ok, "same-owner-actual-failure-and-clear")) goto cleanup;      /* 22 */
    if (!check(&client, FreeLibrary(client.module), "release-driver")) goto cleanup; /* 23 */
    client.module = NULL;
    if (!check(&client, UnregisterClassW(class_name, instance), "unregister-root-class")) goto cleanup; /* 24 */
    atom = 0;
    check(&client, !client.io_failed, "json-output-healthy");                        /* 25 */

cleanup:
    /* No root or unrelated USER message can drive cleanup after this point.
     * On failure, clearing is attempted only for the captured protected owner.
     * Native's own cleared latch prevents replay after a late applied clear.
     */
    if (client.root)
    {
        if (DestroyWindow(client.root)) { client.root = NULL; client.root_destroyed = TRUE; }
        else clean = FALSE;
    }
    if (client.module && client.fixture && client.run_called)
    {
        if (client.owner_id && !client.cleared)
        {
            if (!client.root_destroyed || (client.clear_attempted && !client.clear_retryable)) clean = FALSE;
            else clean = clear_owned(&client, TRUE) && clean;
        }
        if (!client.settled) client.settled = poll_snapshot(&client, TRUE);
        clean = client.settled && clean;
        if (client.probe)
        {
            BOOL zero = probe(&client) && !client.probe_frame.producers &&
                !client.probe_frame.watch_pins && !client.probe_frame.hosts &&
                !client.probe_frame.pe_watches && !client.probe_frame.cleanup_pins;
            clean = zero && clean;
        }
    }
    if (client.module) clean = FreeLibrary(client.module) && clean;
    if (atom) clean = UnregisterClassW(class_name, instance) && clean;
    emit(&client, "{\"type\":\"producer-startup-failure\",\"event\":\"summary\","
                 "\"mode\":%u,\"bits\":%u,\"pid\":%u,\"tid\":%u,\"birth\":%llu,"
                 "\"checks\":%u,\"expected_checks\":%u,\"failures\":%u,\"snapshots\":%u,"
                 "\"probes\":%u,\"owner_id\":%llu,\"root_destroyed\":%s,\"clear_applied\":%s,"
                 "\"settled\":%s,\"cleanup\":%s,\"pending_observed\":%s,\"pending\":",
         client.mode, (unsigned)(8 * sizeof(void *)), (unsigned)GetCurrentProcessId(),
         (unsigned)GetCurrentThreadId(), (unsigned long long)client.birth, client.checks,
         EXPECTED_CHECKS, client.failures, client.snapshots, client.probes,
         (unsigned long long)client.owner_id, client.root_destroyed ? "true" : "false",
         client.cleared ? "true" : "false", client.settled ? "true" : "false",
         clean ? "true" : "false", client.pending_seen ? "true" : "false");
    if (client.pending_seen) emit(&client, "{" FRAME_FORMAT "}", FRAME_VALUES(&client.pending));
    else emit(&client, "null");
    emit(&client, ",\"final\":{" FRAME_FORMAT "},\"io_error\":%s}\n",
         FRAME_VALUES(&client.frame), client.io_failed ? "true" : "false");
    return client.failures || client.io_failed || !clean || client.checks != EXPECTED_CHECKS ||
        !client.pending_seen || !client.root_destroyed || !client.cleared || !client.settled ? 1 : 0;
}
