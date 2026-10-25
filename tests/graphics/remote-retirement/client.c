#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fixture.h"

#define NORMAL_CHECKS 20u
#define PRIMARY_CHECK 12u
#define MINIMUM_ID (UINT64_C(1) << 33)

typedef NTSTATUS (WINAPI *retirement_fixture_fn)(struct retirement_fixture *);

struct deadline
{
    LONGLONG start, end;
    BOOL valid;
};

struct client
{
    retirement_fixture_fn invoke;
    struct retirement_fixture frame, primary;
    HWND root, child;
    LONGLONG frequency;
    NTSTATUS last_ntstatus;
    unsigned checks, failures, snapshots, primary_check;
    BOOL output_failed, quit_seen, primary_observed, primary_in_time;
    BOOL cleanup_attempted, cleanup_complete;
};

#define FRAME_FORMAT \
    "\"size\":%u,\"version\":%u,\"operation\":%u,\"status\":%u," \
    "\"root\":%u,\"child\":%u,\"context_id\":%u,\"model_found\":%u," \
    "\"lease\":%llu,\"ticket\":%llu,\"pending\":%u,\"watched\":%u," \
    "\"finalized\":%u,\"completion_seen\":%u,\"retirement_total\":%u," \
    "\"pe_watches\":%u,\"pe_timers\":%u,\"reserved\":%u"
#define FRAME_VALUES(f) \
    (unsigned)(f)->size, (unsigned)(f)->version, (unsigned)(f)->operation, \
    (unsigned)(f)->status, (unsigned)(f)->root, (unsigned)(f)->child, \
    (unsigned)(f)->context_id, (unsigned)(f)->model_found, \
    (unsigned long long)(f)->lease, (unsigned long long)(f)->ticket, \
    (unsigned)(f)->pending, (unsigned)(f)->watched, (unsigned)(f)->finalized, \
    (unsigned)(f)->completion_seen, (unsigned)(f)->retirement_total, \
    (unsigned)(f)->pe_watches, (unsigned)(f)->pe_timers, (unsigned)(f)->reserved

static void emit(struct client *client, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
    if (result < 0 || fflush(stdout) == EOF) client->output_failed = TRUE;
}

static BOOL check(struct client *client, BOOL condition, const char *name)
{
    ++client->checks;
    if (!condition) ++client->failures;
    emit(client, "{\"type\":\"retirement-fence\",\"event\":\"check\","
                 "\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",
         client->checks, name, condition ? "true" : "false");
    return condition;
}

static BOOL ticks(LONGLONG *value)
{
    LARGE_INTEGER counter;

    if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0) return FALSE;
    *value = counter.QuadPart;
    return TRUE;
}

static struct deadline new_deadline(const struct client *client)
{
    struct deadline deadline = {0};

    if (client->frequency <= 0 || client->frequency > LLONG_MAX / 3 ||
        !ticks(&deadline.start) || deadline.start > LLONG_MAX - 3 * client->frequency)
        return deadline;
    deadline.end = deadline.start + 3 * client->frequency;
    deadline.valid = TRUE;
    return deadline;
}

static BOOL within(const struct deadline *deadline, LONGLONG *now)
{
    LONGLONG value;

    if (!deadline->valid || !ticks(&value)) return FALSE;
    if (now) *now = value;
    return value >= deadline->start && value < deadline->end;
}

/* A deadline rejects late results; it does not cancel a synchronous export.
 * The frame lives in main's client until all calls and cleanup have returned.
 * The first cleanup call is attempted even after a clock failure/descheduling,
 * but a late or untimed call can never satisfy a successful cleanup check.
 */
static BOOL operation(struct client *client, uint32_t op,
                      const struct deadline *deadline, BOOL force_attempt)
{
    struct retirement_fixture *frame = &client->frame;
    LONGLONG finished = 0;
    BOOL started_in_time = within(deadline, NULL), finished_in_time;
    BOOL shape, status;

    if (!started_in_time && !force_attempt)
    {
        emit(client, "{\"type\":\"retirement-fence\",\"event\":\"operation-skipped\","
                     "\"operation\":%u,\"reason\":\"deadline-or-clock\"}\n", (unsigned)op);
        return FALSE;
    }

    frame->size = sizeof(*frame);
    frame->version = RETIREMENT_FIXTURE_VERSION;
    frame->operation = op;
    /* Fail closed if a nominal NT success did not write the operation result. */
    frame->status = ERROR_INVALID_DATA;
    frame->root = (uint32_t)(uintptr_t)client->root;
    frame->child = (uint32_t)(uintptr_t)client->child;
    frame->reserved = 0;
    if (op == RETIREMENT_FIXTURE_CLEANUP) client->cleanup_attempted = TRUE;
    client->last_ntstatus = client->invoke(frame);
    finished_in_time = within(deadline, &finished);
    ++client->snapshots;
    emit(client, "{\"type\":\"retirement-fence\",\"event\":\"snapshot\","
                 "\"snapshot\":%u,\"requested_operation\":%u,\"ntstatus\":%u,"
                 "\"started_in_time\":%s,\"call_finished_in_time\":%s,"
                 "\"elapsed_ticks\":%llu,\"frame\":{" FRAME_FORMAT "}}\n",
         client->snapshots, (unsigned)op, (unsigned)(uint32_t)client->last_ntstatus,
         started_in_time ? "true" : "false", finished_in_time ? "true" : "false",
         (unsigned long long)(finished >= deadline->start ? finished - deadline->start : 0),
         FRAME_VALUES(frame));

    shape = frame->size == sizeof(*frame) && frame->version == RETIREMENT_FIXTURE_VERSION &&
            frame->operation == op && frame->reserved == 0 &&
            frame->root == (uint32_t)(uintptr_t)client->root &&
            frame->child == (uint32_t)(uintptr_t)client->child;
    status = frame->status == ERROR_SUCCESS ||
             (op == RETIREMENT_FIXTURE_CLEANUP && frame->status == ERROR_IO_PENDING);
    return started_in_time && finished_in_time && within(deadline, NULL) &&
           client->last_ntstatus == 0 && shape && status;
}

/* Poll at most once per 20ms. Pumping is bounded even under a perpetually busy
 * message queue, with time checked around PeekMessage and every dispatch/wait.
 * No sleep or forced success substitutes for an actual native/PE observation.
 */
static BOOL pump_to_next_poll(struct client *client, const struct deadline *deadline)
{
    LONGLONG now, target, interval = client->frequency / 50;

    if (!within(deadline, &now)) return FALSE;
    if (interval < 1) interval = 1;
    target = deadline->end - now < interval ? deadline->end : now + interval;
    for (;;)
    {
        MSG message;
        unsigned drained;
        DWORD result, timeout;
        double milliseconds;

        if (!within(deadline, &now)) return FALSE;
        if (now >= target) return TRUE;
        for (drained = 0; drained < 32; ++drained)
        {
            BOOL present;

            if (!within(deadline, NULL)) return FALSE;
            present = PeekMessageW(&message, NULL, 0, 0, PM_REMOVE);
            if (!within(deadline, &now)) return FALSE;
            if (!present) break;
            if (message.message == WM_QUIT)
            {
                client->quit_seen = TRUE;
                return FALSE;
            }
            TranslateMessage(&message);
            if (!within(deadline, NULL)) return FALSE;
            DispatchMessageW(&message);
            if (!within(deadline, &now)) return FALSE;
            if (now >= target) return TRUE;
        }
        if (!within(deadline, &now)) return FALSE;
        if (now >= target) return TRUE;
        /* Floating point affects only wait length, never deadline acceptance. */
        milliseconds = (double)(target - now) * 1000.0 / (double)client->frequency;
        timeout = milliseconds > 20.0 ? 20 : (DWORD)milliseconds;
        result = MsgWaitForMultipleObjectsEx(0, NULL, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (!within(deadline, NULL)) return FALSE;
        if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT)
        {
            emit(client, "{\"type\":\"retirement-fence\",\"event\":\"wait-error\","
                         "\"result\":%u,\"last_error\":%u}\n",
                 (unsigned)result, (unsigned)GetLastError());
            return FALSE;
        }
    }
}

static BOOL poll_completion(struct client *client, const struct deadline *deadline)
{
    for (;;)
    {
        BOOL ready;

        if (!operation(client, RETIREMENT_FIXTURE_SNAPSHOT, deadline, FALSE)) return FALSE;
        ready = client->frame.completion_seen == 1 && client->frame.model_found == 1;
        if (!within(deadline, NULL)) return FALSE;
        if (ready) return TRUE;
        if (client->frame.model_found != 1 || !pump_to_next_poll(client, deadline)) return FALSE;
    }
}

static BOOL poll_detached(struct client *client, const struct deadline *deadline)
{
    for (;;)
    {
        BOOL ready;

        if (!operation(client, RETIREMENT_FIXTURE_SNAPSHOT, deadline, FALSE)) return FALSE;
        ready = client->frame.model_found == 0 && client->frame.retirement_total == 0 &&
                client->frame.pe_watches == 0 && client->frame.pe_timers == 0;
        if (!within(deadline, NULL)) return FALSE;
        if (ready) return TRUE;
        if (!pump_to_next_poll(client, deadline)) return FALSE;
    }
}

static BOOL cleanup(struct client *client)
{
    struct deadline deadline = new_deadline(client);
    BOOL first = TRUE;

    for (;;)
    {
        BOOL ready;

        if (!operation(client, RETIREMENT_FIXTURE_CLEANUP, &deadline, first)) return FALSE;
        first = FALSE;
        ready = client->frame.status == ERROR_SUCCESS &&
                client->frame.pe_watches == 0 && client->frame.pe_timers == 0;
        if (!within(&deadline, NULL)) return FALSE;
        if (ready) return TRUE;
        if (!pump_to_next_poll(client, &deadline)) return FALSE;
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(window, message, wparam, lparam);
}

int main(void)
{
    static const WCHAR class_name[] = L"CiderRetirementFenceFixtureV1";
    struct client client;
    struct deadline deadline;
    LARGE_INTEGER frequency;
    WNDCLASSW window_class;
    HINSTANCE instance = GetModuleHandleW(NULL);
    HMODULE module = NULL;
    ATOM atom = 0;
    DWORD root_pid = 0, child_pid = 0, root_tid, child_tid;
    BOOL ok;

    memset(&client, 0, sizeof(client));
    memset(&window_class, 0, sizeof(window_class));
    ok = QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 &&
         frequency.QuadPart <= LLONG_MAX / 3;
    if (ok) client.frequency = frequency.QuadPart;
    if (!check(&client, ok, "monotonic-clock")) goto done;                         /* 1 */

    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    atom = RegisterClassW(&window_class);
    if (!check(&client, atom != 0, "register-window-class")) goto done;           /* 2 */
    client.root = CreateWindowExW(0, class_name, L"Cider retirement fence",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 480, 320,
                                NULL, NULL, instance, NULL);
    if (!check(&client, client.root != NULL, "create-root")) goto done;           /* 3 */
    client.child = CreateWindowExW(0, class_name, L"Retirement child",
                                 WS_CHILD | WS_VISIBLE | WS_BORDER,
                                 20, 20, 240, 160, client.root, NULL, instance, NULL);
    if (!check(&client, client.child != NULL, "create-bordered-child")) goto done; /* 4 */
    root_tid = GetWindowThreadProcessId(client.root, &root_pid);
    child_tid = GetWindowThreadProcessId(client.child, &child_pid);
    ok = root_tid == GetCurrentThreadId() && child_tid == GetCurrentThreadId() &&
         root_pid == GetCurrentProcessId() && child_pid == GetCurrentProcessId() &&
         GetParent(client.child) == client.root &&
         (GetWindowLongPtrW(client.child, GWL_STYLE) & (WS_CHILD | WS_BORDER)) ==
         (WS_CHILD | WS_BORDER);
    if (!check(&client, ok, "actual-same-process-window-topology")) goto done;     /* 5 */

    module = LoadLibraryW(L"winemac.drv");
    if (!check(&client, module != NULL, "load-winemac-driver")) goto done;         /* 6 */
    client.invoke = (retirement_fixture_fn)(uintptr_t)GetProcAddress(module, "cider_retirement_fixture");
    if (!check(&client, client.invoke != NULL, "resolve-private-export")) goto done; /* 7 */

    deadline = new_deadline(&client);
    ok = operation(&client, RETIREMENT_FIXTURE_SETUP, &deadline, FALSE);
    if (!check(&client, ok, "setup-operation")) goto done;                        /* 8 */
    ok = client.frame.context_id != 0 && client.frame.lease >= MINIMUM_ID &&
         client.frame.ticket >= MINIMUM_ID && client.frame.model_found == 1;
    ok = within(&deadline, NULL) && ok;
    if (!check(&client, ok, "real-context-high-lease-ticket-model")) goto done;    /* 9 */

    /* Start before real STOP submission; logging/dispatch never extends it. */
    deadline = new_deadline(&client);
    ok = operation(&client, RETIREMENT_FIXTURE_REVOKE_HELD, &deadline, FALSE);
    if (!check(&client, ok, "revoke-held-operation")) goto done;                  /* 10 */
    ok = poll_completion(&client, &deadline);
    if (!check(&client, ok, "completion-seen-with-model-held-within-3s")) goto done; /* 11 */

    client.primary_in_time = within(&deadline, NULL);
    client.primary = client.frame;
    client.primary_observed = TRUE;
    client.primary_check = client.checks + 1;
    /* The only compound primary. Continue removal/cleanup even on its failure.
     * The controlled negative should finish 20 checks with only CHECK 12 false.
     */
    ok = client.primary.pending == 1 && client.primary.watched == 1 &&
         client.primary.finalized == 0 && client.primary.model_found == 1 &&
         client.primary.retirement_total == 1;
    /* Check time unconditionally, including when the negative tuple is false. */
    client.primary_in_time = within(&deadline, NULL) && client.primary_in_time;
    check(&client, ok && client.primary_in_time, "held-retirement-fence-primary"); /* 12 */

    deadline = new_deadline(&client);
    ok = operation(&client, RETIREMENT_FIXTURE_SUBMIT_HELD, &deadline, FALSE);
    if (!check(&client, ok, "submit-held-operation")) goto done;                  /* 13 */
    ok = poll_detached(&client, &deadline);
    check(&client, ok, "model-retirement-and-pe-owners-drained-within-3s");        /* 14 */

done:
    /* A fresh cleanup budget is independent of failed readiness/removal. Native
     * CLEANUP owns partial setup and any still-held retirement; no setup/revoke/
     * submit mutation is replayed here. Counts may drain after context release.
     */
    if (client.invoke) client.cleanup_complete = cleanup(&client);
    check(&client, client.cleanup_attempted && client.cleanup_complete, "cleanup-within-3s"); /* 15 */
    ok = !client.child || DestroyWindow(client.child);
    check(&client, ok, "destroy-child-after-cleanup");                            /* 16 */
    ok = !client.root || DestroyWindow(client.root);
    check(&client, ok, "destroy-root-after-cleanup");                             /* 17 */
    ok = !module || FreeLibrary(module);
    check(&client, ok, "release-driver-module");                                 /* 18 */
    ok = !atom || UnregisterClassW(class_name, instance);
    check(&client, ok, "unregister-window-class");                               /* 19 */
    check(&client, !client.output_failed, "json-output-healthy");                 /* 20 */

    emit(&client, "{\"type\":\"retirement-fence\",\"event\":\"summary\","
                  "\"cases\":1,\"pointer_bits\":%u,\"expected_checks\":%u,"
                  "\"expected_primary_check_id\":%u,\"checks\":%u,\"failures\":%u,"
                  "\"snapshots\":%u,\"primary_check_id\":%u,\"primary_observed\":%s,\"primary_in_time\":%s,"
                  "\"cleanup_attempted\":%s,\"cleanup_complete\":%s,\"quit_seen\":%s,"
                  "\"qpc_frequency\":%llu,\"primary\":",
         (unsigned)(8 * sizeof(void *)), NORMAL_CHECKS, PRIMARY_CHECK,
         client.checks, client.failures, client.snapshots, client.primary_check,
         client.primary_observed ? "true" : "false", client.primary_in_time ? "true" : "false",
         client.cleanup_attempted ? "true" : "false",
         client.cleanup_complete ? "true" : "false", client.quit_seen ? "true" : "false",
         (unsigned long long)client.frequency);
    if (client.primary_observed)
        emit(&client, "{" FRAME_FORMAT "}", FRAME_VALUES(&client.primary));
    else
        emit(&client, "null");
    emit(&client, ",\"last_frame_observed\":%s,\"last_ntstatus\":%u,\"last_frame\":{" FRAME_FORMAT
                  "},\"io_error\":%s}\n",
         client.snapshots ? "true" : "false", (unsigned)(uint32_t)client.last_ntstatus,
         FRAME_VALUES(&client.frame), client.output_failed ? "true" : "false");
    return client.failures || client.output_failed || client.checks != NORMAL_CHECKS ||
           !client.primary_observed || !client.primary_in_time ||
           client.primary_check != PRIMARY_CHECK || !client.cleanup_complete ? 1 : 0;
}
