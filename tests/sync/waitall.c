/*
 * Standalone Win32 synchronization acceptance probe. No Wine-private interfaces.
 * Build with either MinGW target, e.g. gcc -std=c99 -O2 -Wall -Wextra waitall.c.
 *
 *   waitall.exe --correctness
 *   waitall.exe --stress iterations [threads=6, minimum 4]
 *   waitall.exe --duration seconds [threads=6, minimum 4]  (1..3600 seconds)
 *   waitall.exe --bench iterations [independent-sem/private-CS threads=4]
 *
 * Iterations are PER WORKER, including both pingpong peers. A benchmark operation
 * is one wait, release, SetEvent, EnterCriticalSection or LeaveCriticalSection
 * API call; a pingpong roundtrip is four ops.
 * Batch percentiles describe observed batch durations (sizes can differ by one
 * iteration). Parallel ns/op uses gate-to-join wall time / aggregate operations.
 * Warmups, thread creation, object creation and final validation are excluded.
 * Scheduling/gate/join overhead is included in wall time, but not batch timings.
 * Separate RTT samples span worker 0's SetEvent(ping1) to its next completed
 * Wait(ping0), after the initial ready token. Every 64th measured outgoing is
 * sampled, capped at the first 4096 completed samples. Warmups/gate are excluded;
 * an outgoing without a following iteration is discarded. Percentiles use
 * nearest rank over actual completed RTT samples, never batch averages.
 * Safety timeouts are failure containment, not performance acceptance thresholds.
 *
 * Workers never use stdio/heap allocation. TerminateThread is used ONLY for this
 * executable's deliberately abandoned owner, or to contain a failed test. A child
 * is created suspended and assigned to a kill-on-close job before it can run.
 * Child cases return stage-specific exit codes; only the parent emits JSONL.
 * A complete record with ok:true AND exit status 0 is required for acceptance.
 * A broken output stream may prevent a complete record; that is always failure.
 * No execution/native-Windows equivalence is implied by compiling this file.
 * Quick stress runs two scenarios, each with iterations per worker. Duration
 * stress runs the randomized mixed scenario for the requested wall-clock period;
 * its WaitAny role randomly mixes 0/1 ms waits to exercise blocking registration
 * and removal paths. Output counts calls/timeouts; public APIs cannot report the
 * runtime's actual Mach registration or pool occupancy.
 * Wide correctness covers 8/64 handles first with a manual sentinel, then with
 * every handle consumable (semaphore + 7/63 autos), covering a full 64 updates.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

#define TEST_MS 10000u
#define RUN_MS 120000u
#define REAP_MS 5000u
#define MAX_THREADS 32u
#define BATCHES 32u
#define RTT_CAPACITY 4096u
#define RTT_INTERVAL 64u
#define MAX_ITERATIONS UINT64_C(1000000000)

typedef LONG (NTAPI *query_semaphore_fn)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef struct semaphore_basic { LONG count, maximum; } semaphore_basic;
static query_semaphore_fn query_semaphore;
static LARGE_INTEGER frequency;
static unsigned checks, failures;
static int output_failed;
static const char *mode_name = "arguments";
/* Only measured ping worker 0 writes these fields; the parent reads after join.
 * Keep the sample buffer out of the 32-element worker array and worker stacks. */
static struct {
    LONGLONG ticks[RTT_CAPACITY];
    LARGE_INTEGER begin;
    uint64_t outgoing;
    unsigned count;
    int pending;
} ping_rtt;
/* One sender publishes the timestamp before the event; the receiving worker
 * owns the samples. Interlocked arming/disarming makes the timestamp handoff
 * explicit on both PE widths. This is distinct from the four-API RTT. */
static struct {
    LARGE_INTEGER begin;
    LONGLONG ticks[RTT_CAPACITY];
    unsigned count;
    volatile LONG armed;
} ping_wake;

static void flush_output(void)
{
    if (fflush(stdout) != 0 || ferror(stdout)) output_failed = 1;
}

static int check(const char *name, int ok, int64_t actual, int64_t expected)
{
    ++checks;
    if (!ok) ++failures;
    if (printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,"
               "\"actual\":%" PRId64 ",\"expected\":%" PRId64 "}\n",
               name, ok ? "true" : "false", actual, expected) < 0)
        output_failed = 1;
    flush_output();
    return ok;
}

static int equal(const char *name, DWORD actual, DWORD expected)
{
    return check(name, actual == expected, (int64_t)actual, (int64_t)expected);
}

static int api(const char *name, BOOL ok)
{
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    return check(name, ok != FALSE, error, ERROR_SUCCESS);
}

static void close_handle(HANDLE *handle)
{
    if (*handle && *handle != INVALID_HANDLE_VALUE)
        api("cleanup.CloseHandle", CloseHandle(*handle));
    *handle = NULL;
}

static int sem_read(HANDLE handle, semaphore_basic *info, LONG *status)
{
    ULONG length = 0;
    memset(info, 0, sizeof(*info));
    *status = query_semaphore(handle, 0 /* SemaphoreBasicInformation */, info,
                              (ULONG)sizeof(*info), &length);
    return *status >= 0 && length == sizeof(*info);
}

static int sem_check(const char *name, HANDLE handle, LONG count, LONG maximum)
{
    semaphore_basic info;
    LONG status;
    int read_ok = sem_read(handle, &info, &status);
    if (!check("NtQuerySemaphore", read_ok, status, 0))
        return 0;
    check(name, info.count == count, info.count, count);
    return check("semaphore.maximum", info.maximum == maximum, info.maximum, maximum);
}

static int finish(int requested_status)
{
    int ok;
    flush_output();
    ok = !requested_status && !failures && !output_failed;
    if (printf("{\"type\":\"complete\",\"mode\":\"%s\",\"ok\":%s,"
               "\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",
               mode_name, ok ? "true" : "false", checks, failures,
               output_failed ? "true" : "false") < 0) output_failed = 1;
    flush_output();
    return ok && !output_failed ? 0 : 1;
}

/* Do not return through a stack still referenced by an unreapable worker. */
static void fatal_cleanup(void)
{
    check("cleanup.unreapable_worker", 0, 1, 0);
    (void)finish(1);
    TerminateProcess(GetCurrentProcess(), 3);
    ExitProcess(3);
}

static int join_thread(HANDLE *thread, DWORD timeout)
{
    DWORD result, code = 0;
    int ok;
    if (!*thread) return 1;
    result = WaitForSingleObject(*thread, timeout);
    ok = equal("worker.join", result, WAIT_OBJECT_0);
    if (!ok) {
        api("cleanup.TerminateThread", TerminateThread(*thread, 2));
        if (WaitForSingleObject(*thread, REAP_MS) != WAIT_OBJECT_0) fatal_cleanup();
    } else {
        if (api("worker.GetExitCodeThread", GetExitCodeThread(*thread, &code)))
            ok = equal("worker.exit_code", code, 0) && ok;
    }
    close_handle(thread);
    return ok;
}

static void release_unowned_check(HANDLE mutex)
{
    BOOL result;
    DWORD error;
    SetLastError(ERROR_SUCCESS);
    result = ReleaseMutex(mutex);
    error = GetLastError();
    check("mutex.no_extra_recursion", !result && error == ERROR_NOT_OWNER,
          result ? 0 : error, ERROR_NOT_OWNER);
    /* Drain an incorrectly retained recursion before discarding this fixture. */
    if (result) {
        unsigned i;
        for (i = 0; i < 16 && ReleaseMutex(mutex); ++i) { }
    }
}

static void not_ready(void)
{
    unsigned rotation;
    for (rotation = 0; rotation < 4; ++rotation) {
        HANDLE h[4] = {NULL, NULL, NULL, NULL}, ordered[4];
        unsigned i;
        int made;
        h[0] = CreateSemaphoreW(NULL, 1, 1, NULL);
        h[1] = CreateEventW(NULL, FALSE, TRUE, NULL);
        h[2] = CreateMutexW(NULL, TRUE, NULL);
        h[3] = CreateEventW(NULL, TRUE, FALSE, NULL);
        made = check("not_ready.create", h[0] && h[1] && h[2] && h[3], 0, 0);
        if (made) {
            for (i = 0; i < 4; ++i) ordered[i] = h[(i + rotation) % 4];
            equal("not_ready.WaitAll.zero_timeout",
                  WaitForMultipleObjects(4, ordered, TRUE, 0), WAIT_TIMEOUT);
            sem_check("not_ready.permit_preserved", h[0], 1, 1);
            equal("not_ready.auto_preserved", WaitForSingleObject(h[1], 0), WAIT_OBJECT_0);
            equal("not_ready.auto_single_signal", WaitForSingleObject(h[1], 0), WAIT_TIMEOUT);
            api("not_ready.preowned_release", ReleaseMutex(h[2]));
            release_unowned_check(h[2]);
        } else if (h[2]) api("cleanup.owned_mutex", ReleaseMutex(h[2]));
        for (i = 0; i < 4; ++i) close_handle(&h[i]);
    }
}

static void mixed_ready(void)
{
    HANDLE h[4] = {NULL, NULL, NULL, NULL};
    unsigned i;
    h[0] = CreateSemaphoreW(NULL, 1, 2, NULL);
    h[1] = CreateEventW(NULL, FALSE, TRUE, NULL);
    h[2] = CreateEventW(NULL, TRUE, TRUE, NULL);
    h[3] = CreateMutexW(NULL, TRUE, NULL);
    if (check("mixed.create", h[0] && h[1] && h[2] && h[3], 0, 0)) {
        DWORD result = WaitForMultipleObjects(4, h, TRUE, 0);
        equal("mixed.WaitAll", result, WAIT_OBJECT_0);
        sem_check("mixed.permit_consumed", h[0], 0, 2);
        equal("mixed.auto_consumed", WaitForSingleObject(h[1], 0), WAIT_TIMEOUT);
        equal("mixed.manual_preserved", WaitForSingleObject(h[2], 0), WAIT_OBJECT_0);
        api("mixed.mutex_release_wait_recursion", ReleaseMutex(h[3]));
        if (result == WAIT_OBJECT_0) api("mixed.mutex_release_original", ReleaseMutex(h[3]));
        release_unowned_check(h[3]);
    } else if (h[3]) api("cleanup.owned_mutex", ReleaseMutex(h[3]));
    for (i = 0; i < 4; ++i) close_handle(&h[i]);
}

static void wide_phase(unsigned count, const char *phase, unsigned autos, unsigned manuals)
{
    if (printf("{\"type\":\"correctness_layout\",\"name\":\"wide_WaitAll\",\"phase\":\"%s\","
               "\"wait_count\":%u,\"semaphores\":1,\"auto_events\":%u,\"manual_events\":%u,"
               "\"consumable_handles\":%u,\"timeout_ms\":%u}\n",
               phase, count, autos, manuals, 1 + autos,
               !strcmp(phase, "sentinel_not_ready") ? 0 : TEST_MS) < 0) output_failed = 1;
    flush_output();
}

static void wide_waitall(unsigned count)
{
    HANDLE h[MAXIMUM_WAIT_OBJECTS] = {NULL};
    HANDLE blocked[MAXIMUM_WAIT_OBJECTS], sentinel = NULL;
    unsigned i, before = failures;
    LONG previous = -1;
    if (!check("wide.supported_count", count >= 3 && count <= MAXIMUM_WAIT_OBJECTS, count, MAXIMUM_WAIT_OBJECTS))
        return;
    h[0] = CreateSemaphoreW(NULL, 1, 1, NULL);
    if (!check("wide.create_semaphore", h[0] != NULL, GetLastError(), 0)) goto done;
    for (i = 1; i < count; ++i) {
        h[i] = CreateEventW(NULL, FALSE, TRUE, NULL);
        if (!check("wide.create_auto", h[i] != NULL, GetLastError(), 0)) goto done;
    }
    sentinel = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!check("wide.create_manual_sentinel", sentinel != NULL, GetLastError(), 0)) goto done;
    for (i = 0; i < count; ++i) blocked[i] = h[i];
    blocked[count - 1] = sentinel;

    wide_phase(count, "sentinel_not_ready", count - 2, 1);
    equal("wide.not_ready_zero_timeout", WaitForMultipleObjects(count, blocked, TRUE, 0), WAIT_TIMEOUT);
    sem_check("wide.not_ready_permit_preserved", h[0], 1, 1);
    for (i = 1; i < count; ++i) {
        /* Probing consumes each auto signal; restore it for the ready phase. */
        equal("wide.auto_preserved", WaitForSingleObject(h[i], 0), WAIT_OBJECT_0);
        equal("wide.auto_exactly_one_signal", WaitForSingleObject(h[i], 0), WAIT_TIMEOUT);
        api("wide.restore_probed_auto", SetEvent(h[i]));
    }
    equal("wide.manual_still_not_ready", WaitForSingleObject(sentinel, 0), WAIT_TIMEOUT);
    if (failures != before) goto done;

    api("wide.set_manual_sentinel", SetEvent(sentinel));
    wide_phase(count, "sentinel_ready", count - 2, 1);
    if (!equal("wide.ready_with_sentinel", WaitForMultipleObjects(count, blocked, TRUE, TEST_MS), WAIT_OBJECT_0)) goto done;
    sem_check("wide.sentinel_ready_permit_consumed", h[0], 0, 1);
    for (i = 1; i + 1 < count; ++i)
        equal("wide.sentinel_ready_auto_consumed", WaitForSingleObject(h[i], 0), WAIT_TIMEOUT);
    equal("wide.manual_signal_preserved", WaitForSingleObject(sentinel, 0), WAIT_OBJECT_0);
    equal("wide.manual_still_signaled", WaitForSingleObject(sentinel, 0), WAIT_OBJECT_0);
    equal("wide.sentinel_ready_cannot_consume_twice", WaitForMultipleObjects(count, blocked, TRUE, 0), WAIT_TIMEOUT);
    /* The final auto was outside the sentinel wait, so it must still be set. */
    equal("wide.excluded_auto_preserved", WaitForSingleObject(h[count - 1], 0), WAIT_OBJECT_0);
    api("wide.restore_excluded_auto", SetEvent(h[count - 1]));
    if (failures != before) goto done;

    /* Keeping a manual sentinel inside a 64-handle wait would allow only 63
     * changes. Replace it with the final auto for the full consuming case. */
    if (!api("wide.restore_semaphore", ReleaseSemaphore(h[0], 1, &previous))) goto done;
    check("wide.semaphore_was_empty", previous == 0, previous, 0);
    for (i = 1; i + 1 < count; ++i) api("wide.restore_consumed_auto", SetEvent(h[i]));
    if (failures != before) goto done;
    wide_phase(count, "all_consuming_ready", count - 1, 0);
    if (!equal("wide.full_consuming_WaitAll", WaitForMultipleObjects(count, h, TRUE, TEST_MS), WAIT_OBJECT_0)) goto done;
    sem_check("wide.full_consuming_permit", h[0], 0, 1);
    for (i = 1; i < count; ++i)
        equal("wide.full_consuming_auto", WaitForSingleObject(h[i], 0), WAIT_TIMEOUT);
    equal("wide.full_wait_cannot_consume_twice", WaitForMultipleObjects(count, h, TRUE, 0), WAIT_TIMEOUT);
    sem_check("wide.full_repeat_did_not_create_permit", h[0], 0, 1);
done:
    for (i = 0; i < count; ++i) close_handle(&h[i]);
    close_handle(&sentinel);
    if (printf("{\"type\":\"correctness_case\",\"name\":\"wide_WaitAll\",\"wait_count\":%u,\"ok\":%s}\n",
               count, failures == before && !output_failed ? "true" : "false") < 0) output_failed = 1;
    flush_output();
}

static void waitany_priority(void)
{
    HANDLE h[3] = {NULL, NULL, NULL};
    unsigned i;
    h[0] = CreateSemaphoreW(NULL, 1, 1, NULL);
    h[1] = CreateEventW(NULL, FALSE, TRUE, NULL);
    h[2] = CreateEventW(NULL, TRUE, TRUE, NULL);
    if (check("waitany.create", h[0] && h[1] && h[2], 0, 0)) {
        equal("waitany.lowest_index_0", WaitForMultipleObjects(3, h, FALSE, 0), WAIT_OBJECT_0);
        sem_check("waitany.sem_consumed", h[0], 0, 1);
        equal("waitany.lowest_index_1", WaitForMultipleObjects(3, h, FALSE, 0), WAIT_OBJECT_0 + 1);
        equal("waitany.remaining_manual_2", WaitForMultipleObjects(3, h, FALSE, 0), WAIT_OBJECT_0 + 2);
        equal("waitany.manual_stays_ready", WaitForMultipleObjects(3, h, FALSE, 0), WAIT_OBJECT_0 + 2);
    }
    for (i = 0; i < 3; ++i) close_handle(&h[i]);
}

static volatile LONG apc_count;
static VOID CALLBACK count_apc(ULONG_PTR parameter)
{
    (void)parameter;
    InterlockedIncrement(&apc_count);
}

static void apc_priority(void)
{
    HANDLE h[2] = {NULL, NULL};
    h[0] = CreateSemaphoreW(NULL, 1, 1, NULL);
    h[1] = CreateEventW(NULL, FALSE, TRUE, NULL);
    if (check("apc.create", h[0] && h[1], 0, 0)) {
        InterlockedExchange(&apc_count, 0);
        if (api("apc.queue_alertable", QueueUserAPC(count_apc, GetCurrentThread(), 0) != 0)) {
            equal("apc.priority_over_ready_WaitAll",
                  WaitForMultipleObjectsEx(2, h, TRUE, 0, TRUE), WAIT_IO_COMPLETION);
            check("apc.dispatched_once", apc_count == 1, apc_count, 1);
            sem_check("apc.permit_preserved", h[0], 1, 1);
            equal("apc.auto_preserved", WaitForSingleObject(h[1], 0), WAIT_OBJECT_0);
            api("apc.restore_auto", SetEvent(h[1]));
            equal("apc.ready_after_dispatch", WaitForMultipleObjectsEx(2, h, TRUE, 0, TRUE), WAIT_OBJECT_0);
            sem_check("apc.permit_then_consumed", h[0], 0, 1);
        }
        /* Drain a pending callback even when a backend violated priority. */
        (void)SleepEx(0, TRUE);
        api("apc.restore_permit", ReleaseSemaphore(h[0], 1, NULL));
        api("apc.restore_auto_nonalertable", SetEvent(h[1]));
        InterlockedExchange(&apc_count, 0);
        if (api("apc.queue_nonalertable", QueueUserAPC(count_apc, GetCurrentThread(), 0) != 0)) {
            equal("apc.nonalertable_ready", WaitForMultipleObjectsEx(2, h, TRUE, 0, FALSE), WAIT_OBJECT_0);
            check("apc.nonalertable_deferred", apc_count == 0, apc_count, 0);
            equal("apc.later_alertable_dispatch", SleepEx(0, TRUE), WAIT_IO_COMPLETION);
            check("apc.later_dispatched_once", apc_count == 1, apc_count, 1);
        }
        (void)SleepEx(0, TRUE);
    }
    close_handle(&h[0]); close_handle(&h[1]);
}

typedef struct owner_context {
    HANDLE mutex, ready, hold;
    DWORD error;
    int terminate_case;
} owner_context;

static DWORD WINAPI own_mutex(void *argument)
{
    owner_context *ctx = (owner_context *)argument;
    DWORD result = WaitForSingleObject(ctx->mutex, TEST_MS);
    if (result != WAIT_OBJECT_0) { ctx->error = result; SetEvent(ctx->ready); return 0; }
    if (!SetEvent(ctx->ready)) { ctx->error = GetLastError(); return 0; }
    if (ctx->terminate_case) {
        result = WaitForSingleObject(ctx->hold, RUN_MS);
        ctx->error = result == WAIT_OBJECT_0 ? ERROR_CANCELLED : ERROR_TIMEOUT;
    }
    /* Intentionally exit while owning the mutex. */
    return 0;
}

static void abandoned(int terminate_case)
{
    owner_context ctx;
    HANDLE thread = NULL, h[4] = {NULL, NULL, NULL, NULL};
    unsigned i;
    DWORD result;
    memset(&ctx, 0, sizeof(ctx));
    ctx.terminate_case = terminate_case;
    h[0] = CreateSemaphoreW(NULL, 1, 1, NULL);
    h[1] = CreateEventW(NULL, FALSE, TRUE, NULL);
    h[2] = ctx.mutex = CreateMutexW(NULL, FALSE, NULL);
    h[3] = CreateEventW(NULL, TRUE, FALSE, NULL);
    ctx.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    ctx.hold = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!check("abandon.create", h[0] && h[1] && h[2] && h[3] && ctx.ready && ctx.hold, 0, 0)) goto done;
    thread = CreateThread(NULL, 0, own_mutex, &ctx, 0, NULL);
    if (!check("abandon.CreateThread", thread != NULL, GetLastError(), 0)) goto done;
    if (!equal("abandon.owner_ready", WaitForSingleObject(ctx.ready, TEST_MS), WAIT_OBJECT_0)) goto done;
    if (terminate_case) {
        if (!api("abandon.deliberate_TerminateThread", TerminateThread(thread, 0))) goto done;
    }
    if (!join_thread(&thread, TEST_MS)) goto done;
    if (!equal("abandon.owner_error", ctx.error, 0)) goto done;
    equal("abandon.not_ready_does_not_acquire", WaitForMultipleObjects(4, h, TRUE, 0), WAIT_TIMEOUT);
    sem_check("abandon.not_ready_permit", h[0], 1, 1);
    equal("abandon.not_ready_auto", WaitForSingleObject(h[1], 0), WAIT_OBJECT_0);
    api("abandon.restore_auto", SetEvent(h[1]));
    api("abandon.make_ready", SetEvent(h[3]));
    result = WaitForMultipleObjects(4, h, TRUE, TEST_MS);
    /* WaitAll does not promise the abandoned object's index. */
    check(terminate_case ? "abandon.terminated_WaitAll" : "abandon.exited_WaitAll",
          result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + 4,
          result, WAIT_ABANDONED_0);
    sem_check("abandon.success_permit", h[0], 0, 1);
    equal("abandon.success_auto", WaitForSingleObject(h[1], 0), WAIT_TIMEOUT);
    if (result == WAIT_OBJECT_0 || (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + 4)) {
        api("abandon.release_acquired", ReleaseMutex(h[2]));
        release_unowned_check(h[2]);
        equal("abandon.cleared_after_acquire", WaitForSingleObject(h[2], 0), WAIT_OBJECT_0);
        api("abandon.release_again", ReleaseMutex(h[2]));
    }
done:
    if (thread) { SetEvent(ctx.hold); join_thread(&thread, TEST_MS); }
    close_handle(&ctx.ready); close_handle(&ctx.hold);
    for (i = 0; i < 4; ++i) close_handle(&h[i]);
}

typedef struct child_process { HANDLE process, thread, job; } child_process;

static int start_child(child_process *child, const char *kind, const char *base)
{
    WCHAR executable[32768], command[33024], wide_kind[64], wide_base[128];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    DWORD length;
    memset(child, 0, sizeof(*child));
    memset(&startup, 0, sizeof(startup)); startup.cb = sizeof(startup);
    memset(&process, 0, sizeof(process));
    memset(&limits, 0, sizeof(limits));
    length = GetModuleFileNameW(NULL, executable, 32768);
    if (!check("child.module_path", length > 0 && length < 32768, length, 1)) return 0;
    if (!MultiByteToWideChar(CP_ACP, 0, kind, -1, wide_kind, 64) ||
        !MultiByteToWideChar(CP_ACP, 0, base, -1, wide_base, 128)) {
        api("child.convert_arguments", FALSE); return 0;
    }
    if (swprintf(command, sizeof(command) / sizeof(command[0]), L"\"%ls\" --child %ls %ls",
                 executable, wide_kind, wide_base) < 0) {
        check("child.command_length", 0, 0, 1); return 0;
    }
    child->job = CreateJobObjectW(NULL, NULL);
    if (!check("child.CreateJobObject", child->job != NULL, GetLastError(), 0)) return 0;
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!api("child.kill_on_job_close", SetInformationJobObject(child->job,
             JobObjectExtendedLimitInformation, &limits, sizeof(limits)))) return 0;
    if (!api("child.CreateProcess", CreateProcessW(executable, command, NULL, NULL, FALSE,
             CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &startup, &process))) return 0;
    child->process = process.hProcess; child->thread = process.hThread;
    if (!api("child.AssignProcessToJobObject", AssignProcessToJobObject(child->job, child->process))) return 0;
    {
        DWORD previous = ResumeThread(child->thread);
        if (!check("child.ResumeThread", previous != (DWORD)-1, previous, 1)) return 0;
    }
    return 1;
}

static void close_child(child_process *child)
{
    if (child->process) {
        DWORD result = WaitForSingleObject(child->process, 0);
        if (result != WAIT_OBJECT_0) {
            api("child.cleanup.TerminateProcess", TerminateProcess(child->process, 2));
            if (WaitForSingleObject(child->process, REAP_MS) != WAIT_OBJECT_0) {
                if (child->job) TerminateJobObject(child->job, 2);
                if (WaitForSingleObject(child->process, REAP_MS) != WAIT_OBJECT_0) fatal_cleanup();
            }
        }
    }
    close_handle(&child->thread); close_handle(&child->process); close_handle(&child->job);
}

static int wait_child(child_process *child, const char *name)
{
    DWORD code = 0;
    if (!equal("child.bounded_join", WaitForSingleObject(child->process, TEST_MS), WAIT_OBJECT_0)) return 0;
    if (!api("child.GetExitCodeProcess", GetExitCodeProcess(child->process, &code))) return 0;
    return equal(name, code, 0);
}

static void object_name(char *target, size_t size, const char *base, const char *suffix)
{
    (void)snprintf(target, size, "%s.%s", base, suffix);
}

static int child_main(const char *kind, const char *base)
{
    HANDLE h[4] = {NULL, NULL, NULL, NULL};
    semaphore_basic info;
    LONG status;
    char name[160];
    int code = 0;
    unsigned i;
    if (!strcmp(kind, "duplicate")) {
        DWORD result, error;
        h[0] = CreateSemaphoreW(NULL, 1, 1, NULL);
        if (!h[0]) return 10;
        h[1] = h[0];
        SetLastError(ERROR_SUCCESS);
        result = WaitForMultipleObjects(2, h, TRUE, 0);
        error = GetLastError();
        if (result != WAIT_FAILED || error != ERROR_INVALID_PARAMETER) code = 11;
        else if (!sem_read(h[0], &info, &status) || info.count != 1 || info.maximum != 1) code = 12;
        if (!CloseHandle(h[0]) && !code) code = 13;
        return code;
    }
    if (strcmp(kind, "named")) return 14;
    object_name(name, sizeof(name), base, "sem");
    h[0] = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name);
    object_name(name, sizeof(name), base, "event");
    h[1] = OpenEventA(EVENT_ALL_ACCESS, FALSE, name);
    object_name(name, sizeof(name), base, "mutex");
    h[2] = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, name);
    object_name(name, sizeof(name), base, "ready");
    h[3] = OpenEventA(EVENT_ALL_ACCESS, FALSE, name);
    if (!h[0] || !h[1] || !h[2] || !h[3]) code = 20;
    else if (WaitForMultipleObjects(3, h, TRUE, 0) != WAIT_TIMEOUT) code = 27;
    else if (!sem_read(h[0], &info, &status) || info.count != 0 || info.maximum != 1) code = 28;
    else if (!SetEvent(h[3])) code = 21;
    else if (WaitForMultipleObjects(3, h, TRUE, TEST_MS) != WAIT_OBJECT_0) code = 22;
    else {
        if (!sem_read(h[0], &info, &status) || info.count != 0 || info.maximum != 1) code = 23;
        if (!ReleaseMutex(h[2]) && !code) code = 24;
        if (!ReleaseSemaphore(h[0], 1, NULL) && !code) code = 25;
    }
    for (i = 0; i < 4; ++i) if (h[i] && !CloseHandle(h[i]) && !code) code = 26;
    return code;
}

static void crossprocess(void)
{
    child_process child;
    HANDLE h[4] = {NULL, NULL, NULL, NULL};
    LARGE_INTEGER tick;
    char base[128], name[160];
    unsigned i;
    int owned = 0;
    memset(&child, 0, sizeof(child));
    if (start_child(&child, "duplicate", "unused")) wait_child(&child, "duplicate.WaitAll_rejected_without_consumption");
    close_child(&child);
    if (!api("named.QueryPerformanceCounter", QueryPerformanceCounter(&tick))) return;
    (void)snprintf(base, sizeof(base), "Local\\CiderWaitAll_%lu_%" PRIu64,
                   (unsigned long)GetCurrentProcessId(), (uint64_t)tick.QuadPart);
    object_name(name, sizeof(name), base, "sem");
    h[0] = CreateSemaphoreA(NULL, 0, 1, name);
    if (!check("named.fresh_semaphore", h[0] && GetLastError() != ERROR_ALREADY_EXISTS, GetLastError(), 0)) goto done;
    object_name(name, sizeof(name), base, "event");
    h[1] = CreateEventA(NULL, TRUE, FALSE, name);
    if (!check("named.fresh_event", h[1] && GetLastError() != ERROR_ALREADY_EXISTS, GetLastError(), 0)) goto done;
    object_name(name, sizeof(name), base, "mutex");
    h[2] = CreateMutexA(NULL, TRUE, name);
    if (!check("named.fresh_mutex", h[2] && GetLastError() != ERROR_ALREADY_EXISTS, GetLastError(), 0)) goto done;
    owned = 1;
    object_name(name, sizeof(name), base, "ready");
    h[3] = CreateEventA(NULL, TRUE, FALSE, name);
    if (!check("named.fresh_ready", h[3] && GetLastError() != ERROR_ALREADY_EXISTS, GetLastError(), 0)) goto done;
    if (!start_child(&child, "named", base)) goto done;
    if (!equal("named.child_opened", WaitForSingleObject(h[3], TEST_MS), WAIT_OBJECT_0)) goto done;
    sem_check("named.initial_empty", h[0], 0, 1);
    if (!api("named.release_permit", ReleaseSemaphore(h[0], 1, NULL))) goto done;
    if (!api("named.set_event", SetEvent(h[1]))) goto done;
    if (!api("named.release_parent_mutex", ReleaseMutex(h[2]))) goto done;
    owned = 0;
    if (!wait_child(&child, "named.child_WaitAll_and_release")) goto done;
    sem_check("named.returned_permit", h[0], 1, 1);
    equal("named.manual_preserved", WaitForSingleObject(h[1], 0), WAIT_OBJECT_0);
    {
        DWORD result = WaitForSingleObject(h[2], TEST_MS);
        equal("named.mutex_not_abandoned", result, WAIT_OBJECT_0);
        if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED_0) owned = 1;
    }
done:
    close_child(&child);
    if (owned) api("named.cleanup.ReleaseMutex", ReleaseMutex(h[2]));
    for (i = 0; i < 4; ++i) close_handle(&h[i]);
}

typedef struct group {
    HANDLE ready, go;
    HANDLE semaphore, automatic, manual, mutex;
    HANDLE ping[2];
    volatile LONG ready_count, stop;
    unsigned threads;
    uint64_t iterations;
    int workload;
    unsigned duration_seconds;
    ULONGLONG stop_at;
    uint32_t seed;
} group;

typedef struct sample { LONGLONG ticks; uint64_t iterations; } sample;
typedef struct worker {
    group *group;
    unsigned index;
    HANDLE private_sem;
    CRITICAL_SECTION private_cs;
    int cs_initialized, measuring;
    DWORD error;
    unsigned stage;
    uint64_t completed, acquired, released, release_full, release_attempts;
    uint64_t all_ready, any_event, timeouts, queries;
    uint64_t any_poll_calls, any_timed_calls, any_timed_timeouts;
    LONG peak;
    unsigned samples;
    sample batch[BATCHES];
} worker;

enum { BENCH_SEM = 1, BENCH_MANUAL, BENCH_ALL_SEM, BENCH_PRIVATE, BENCH_PING,
       STRESS_BLOCKED, BENCH_CS };

static int worker_fail(worker *w, unsigned stage, DWORD error)
{
    if (!w->stage) {
        w->stage = stage;
        w->error = error;
    }
    InterlockedExchange(&w->group->stop, 1);
    return 0;
}

static int group_gate(worker *w)
{
    group *g = w->group;
    DWORD result;
    if ((unsigned)InterlockedIncrement(&g->ready_count) == g->threads && !SetEvent(g->ready))
        return worker_fail(w, 1, GetLastError());
    result = WaitForSingleObject(g->go, RUN_MS);
    if (result != WAIT_OBJECT_0) return worker_fail(w, 2, result);
    return !InterlockedCompareExchange(&g->stop, 0, 0);
}

static int observe_semaphore(worker *w)
{
    semaphore_basic info;
    LONG status;
    if (!sem_read(w->group->semaphore, &info, &status))
        return worker_fail(w, 3, (DWORD)status);
    ++w->queries;
    if (info.count > w->peak) w->peak = info.count;
    if (info.count < 0 || info.count > 1 || info.maximum != 1)
        return worker_fail(w, 4, (DWORD)info.count);
    return 1;
}

/* A producer can refill while an acquirer holds a permit. Failure with
 * ERROR_TOO_MANY_POSTS is valid; count ONLY successful releases in the ledger. */
static int stress_release(worker *w)
{
    LONG previous = -1;
    ++w->release_attempts;
    if (ReleaseSemaphore(w->group->semaphore, 1, &previous)) {
        ++w->released;
        if (previous != 0) return worker_fail(w, 5, (DWORD)previous);
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_TOO_MANY_POSTS) return worker_fail(w, 6, error);
        ++w->release_full;
    }
    return 1;
}

static DWORD WINAPI stress_worker(void *argument)
{
    worker *w = (worker *)argument;
    group *g = w->group;
    ULONGLONG deadline;
    unsigned role = w->index % 4;
    uint32_t random = g->seed ^ ((w->index + 1) * UINT32_C(0x9e3779b9));
    uint64_t i;
    if (!random) random = 1;
    if (!group_gate(w)) return 0;
    deadline = g->duration_seconds ? g->stop_at : GetTickCount64() + RUN_MS;
    for (i = 0; (g->duration_seconds || i < g->iterations) &&
                !InterlockedCompareExchange(&g->stop, 0, 0); ++i) {
        DWORD result;
        HANDLE h[4];
        if ((i & 127) == 0 && GetTickCount64() >= deadline) {
            if (g->duration_seconds) InterlockedExchange(&g->stop, 1);
            else worker_fail(w, 7, ERROR_TIMEOUT);
            break;
        }
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        if (g->workload == STRESS_BLOCKED) {
            /* The manual event is permanently unsignaled, and NOBODY consumes
             * a permit successfully. The semaphore starts full. A release that
             * succeeds here exposes temporary WaitAll consumption even if a
             * later rollback/query misses the transient count > maximum. */
            if (role == 0 || role == 2) {
                h[i & 1] = g->semaphore;
                h[1 - (i & 1)] = g->manual;
                result = WaitForMultipleObjects(2, h, TRUE, 0);
                if (result != WAIT_TIMEOUT) {
                    if (result == WAIT_OBJECT_0) ++w->acquired;
                    worker_fail(w, 50, result); break;
                }
                ++w->timeouts;
            } else if (role == 1) {
                if (!stress_release(w)) break;
                if (w->released) { worker_fail(w, 51, ERROR_INVALID_DATA); break; }
            }
        } else if (role == 0) {
            /* Rotate all positions, including pre-acquisition of the mutex. */
            HANDLE original[4];
            unsigned j;
            original[0] = g->semaphore; original[1] = g->automatic;
            original[2] = g->manual; original[3] = g->mutex;
            for (j = 0; j < 4; ++j) h[j] = original[(j + (unsigned)(random & 3)) % 4];
            result = WaitForMultipleObjects(4, h, TRUE, 0);
            if (result == WAIT_OBJECT_0) {
                ++w->acquired; ++w->all_ready;
                if (!ReleaseMutex(g->mutex)) { worker_fail(w, 8, GetLastError()); break; }
                if (!stress_release(w)) break;
            } else if (result == WAIT_TIMEOUT) ++w->timeouts;
            else { worker_fail(w, 9, result); break; }
        } else if (role == 1) {
            unsigned sem_index = (unsigned)(random & 1);
            DWORD wait_ms = g->duration_seconds && (random & 4) ? 1 : 0;
            h[sem_index] = g->semaphore; h[1 - sem_index] = g->automatic;
            if (wait_ms) ++w->any_timed_calls;
            else ++w->any_poll_calls;
            result = WaitForMultipleObjects(2, h, FALSE, wait_ms);
            if (result == WAIT_OBJECT_0 + sem_index) {
                ++w->acquired;
                if (!stress_release(w)) break;
            } else if (result == WAIT_OBJECT_0 + 1 - sem_index) ++w->any_event;
            else if (result == WAIT_TIMEOUT) {
                ++w->timeouts;
                if (wait_ms) ++w->any_timed_timeouts;
            }
            else { worker_fail(w, 10, result); break; }
        } else if (role == 2) {
            if (!stress_release(w)) break;
            if (!(random & 1 ? SetEvent(g->automatic) : ResetEvent(g->automatic))) {
                worker_fail(w, 11, GetLastError()); break;
            }
            if (!(random & 2 ? SetEvent(g->manual) : ResetEvent(g->manual))) {
                worker_fail(w, 12, GetLastError()); break;
            }
        }
        if (role == 3 || (i & 31) == 0) if (!observe_semaphore(w)) break;
        ++w->completed;
        if ((i & 63) == 0) SwitchToThread();
    }
    return 0;
}

static int create_group(group *g, unsigned threads, uint64_t iterations)
{
    memset(g, 0, sizeof(*g));
    g->threads = threads; g->iterations = iterations;
    g->seed = (uint32_t)GetTickCount() ^ UINT32_C(0xa511e9b3);
    g->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    g->go = CreateEventW(NULL, TRUE, FALSE, NULL);
    return check("group.create_gates", g->ready && g->go, GetLastError(), 0);
}

static void close_group(group *g)
{
    close_handle(&g->ready); close_handle(&g->go);
    close_handle(&g->semaphore); close_handle(&g->automatic);
    close_handle(&g->manual); close_handle(&g->mutex);
    close_handle(&g->ping[0]); close_handle(&g->ping[1]);
}

static int start_workers(group *g, worker *workers, HANDLE *threads, LPTHREAD_START_ROUTINE entry)
{
    unsigned i;
    for (i = 0; i < g->threads; ++i) {
        workers[i].group = g; workers[i].index = i;
        threads[i] = CreateThread(NULL, 0, entry, &workers[i], 0, NULL);
        if (!check("group.CreateThread", threads[i] != NULL, GetLastError(), 0)) return 0;
    }
    return equal("group.all_workers_ready", WaitForSingleObject(g->ready, RUN_MS), WAIT_OBJECT_0);
}

static int join_workers(group *g, HANDLE *threads)
{
    unsigned i;
    DWORD result = WaitForMultipleObjects(g->threads, threads, TRUE, RUN_MS);
    int ok = equal("group.bounded_join", result, WAIT_OBJECT_0);
    if (!ok) InterlockedExchange(&g->stop, 1);
    for (i = 0; i < g->threads; ++i) {
        if (!ok && threads[i] && WaitForSingleObject(threads[i], 0) != WAIT_OBJECT_0) {
            api("group.cleanup.TerminateThread", TerminateThread(threads[i], 2));
            if (WaitForSingleObject(threads[i], REAP_MS) != WAIT_OBJECT_0) fatal_cleanup();
        }
    }
    /* Handles stay open until timing has ended. */
    return ok;
}

static int duration_join(group *g, HANDLE *threads, ULONGLONG start)
{
    ULONGLONG next_progress = start + 10000;
    for (;;) {
        DWORD result = WaitForMultipleObjects(g->threads, threads, TRUE, 1000);
        ULONGLONG now = GetTickCount64();
        if (now >= g->stop_at) InterlockedExchange(&g->stop, 1);
        if (now >= next_progress) {
            /* Per-worker 64-bit counters are read only after joining, so this
             * progress record cannot introduce torn loads on 32-bit targets. */
            if (printf("{\"type\":\"progress\",\"mode\":\"duration\",\"elapsed_ms\":%" PRIu64 ","
                       "\"remaining_ms\":%" PRIu64 ",\"threads\":%u,\"stop_requested\":%s}\n",
                       (uint64_t)(now - start), (uint64_t)(now < g->stop_at ? g->stop_at - now : 0),
                       g->threads, InterlockedCompareExchange(&g->stop, 0, 0) ? "true" : "false") < 0)
                output_failed = 1;
            flush_output();
            if (output_failed) { InterlockedExchange(&g->stop, 1); return 0; }
            next_progress = now + 10000;
        }
        if (result == WAIT_OBJECT_0) {
            return check("duration.reached_requested_time", now >= g->stop_at,
                         (int64_t)(now - start), (int64_t)g->duration_seconds * 1000);
        }
        if (result != WAIT_TIMEOUT) return equal("duration.bounded_join", result, WAIT_OBJECT_0);
        if (now >= g->stop_at + TEST_MS) {
            check("duration.workers_stop_deadline", 0, WAIT_TIMEOUT, WAIT_OBJECT_0);
            return 0;
        }
    }
}

static void cleanup_workers(group *g, HANDLE *threads, worker *workers)
{
    unsigned i, count = 0;
    InterlockedExchange(&g->stop, 1);
    if (g->go) api("cleanup.open_gate", SetEvent(g->go));
    while (count < g->threads && threads[count]) ++count;
    if (count && WaitForMultipleObjects(count, threads, TRUE, REAP_MS) != WAIT_OBJECT_0) {
        check("cleanup.workers_did_not_stop", 0, 1, 0);
        /* Stop every still-running worker before starting bounded reaps. */
        for (i = 0; i < count; ++i)
            if (WaitForSingleObject(threads[i], 0) != WAIT_OBJECT_0)
                api("cleanup.TerminateThread", TerminateThread(threads[i], 2));
        for (i = 0; i < count; ++i)
            if (WaitForSingleObject(threads[i], REAP_MS) != WAIT_OBJECT_0) fatal_cleanup();
    }
    for (i = 0; i < g->threads; ++i) {
        join_thread(&threads[i], REAP_MS);
        close_handle(&workers[i].private_sem);
        if (workers[i].cs_initialized) {
            DeleteCriticalSection(&workers[i].private_cs);
            workers[i].cs_initialized = 0;
        }
    }
}

static int workers_ok(group *g, worker *workers)
{
    unsigned i;
    int ok = 1;
    for (i = 0; i < g->threads; ++i) {
        worker *w = &workers[i];
        if (printf("{\"type\":\"worker\",\"index\":%u,\"stage\":%u,\"error\":%lu,"
                   "\"completed\":%" PRIu64 "}\n", i, w->stage, (unsigned long)w->error, w->completed) < 0)
            output_failed = 1;
        ok = check("worker.no_error", w->stage == 0, w->error, 0) && ok;
        if (g->duration_seconds)
            ok = check("worker.duration_progress", w->completed > 0, (int64_t)w->completed, 1) && ok;
        else
            ok = check("worker.iterations", w->completed == g->iterations,
                       (int64_t)w->completed, (int64_t)g->iterations) && ok;
    }
    return ok;
}

static void stress_case(uint64_t iterations, unsigned count, int blocked, unsigned duration_seconds)
{
    group g;
    worker workers[MAX_THREADS];
    HANDLE threads[MAX_THREADS] = {NULL};
    unsigned i;
    uint64_t acquired = 0, released = 0, full = 0, attempts = 0;
    uint64_t all = 0, event = 0, timeouts = 0, queries = 0;
    uint64_t any_poll_calls = 0, any_timed_calls = 0, any_timed_timeouts = 0;
    LONG peak = 0, status;
    semaphore_basic info;
    ULONGLONG start = 0;
    DWORD handles_before = 0, handles_after = 0;
    int handles_before_ok;
    handles_before_ok = api("stress.handle_count_before", GetProcessHandleCount(GetCurrentProcess(), &handles_before));
    memset(workers, 0, sizeof(workers));
    if (!create_group(&g, count, iterations)) goto done;
    g.workload = blocked ? STRESS_BLOCKED : 0;
    g.duration_seconds = duration_seconds;
    g.semaphore = CreateSemaphoreW(NULL, 1, 1, NULL);
    g.automatic = CreateEventW(NULL, FALSE, TRUE, NULL);
    g.manual = CreateEventW(NULL, TRUE, !blocked, NULL);
    g.mutex = CreateMutexW(NULL, FALSE, NULL);
    if (!check("stress.create_objects", g.semaphore && g.automatic && g.manual && g.mutex, 0, 0)) goto done;
    if (!start_workers(&g, workers, threads, stress_worker)) goto done;
    start = GetTickCount64();
    g.stop_at = start + (ULONGLONG)duration_seconds * 1000;
    if (!api("stress.start", SetEvent(g.go))) goto done;
    if (duration_seconds) {
        if (!duration_join(&g, threads, start)) goto done;
    } else if (!join_workers(&g, threads)) goto done;
    workers_ok(&g, workers);
    for (i = 0; i < count; ++i) {
        worker *w = &workers[i];
        acquired += w->acquired; released += w->released; full += w->release_full;
        attempts += w->release_attempts; all += w->all_ready; event += w->any_event;
        timeouts += w->timeouts; queries += w->queries;
        any_poll_calls += w->any_poll_calls; any_timed_calls += w->any_timed_calls;
        any_timed_timeouts += w->any_timed_timeouts;
        if (w->peak > peak) peak = w->peak;
    }
    {
        int read_ok = sem_read(g.semaphore, &info, &status);
        if (!check("stress.final_query", read_ok, status, 0)) goto done;
    }
    check("stress.maximum_one", info.maximum == 1, info.maximum, 1);
    check("stress.final_count_bounds", info.count >= 0 && info.count <= 1, info.count, 1);
    check("stress.exact_permit_accounting", (int64_t)released + 1 - (int64_t)acquired == info.count,
          (int64_t)released + 1 - (int64_t)acquired, info.count);
    check("stress.release_accounting", released + full == attempts, (int64_t)(released + full), (int64_t)attempts);
    if (blocked) check("stress.blocked_waitall_cannot_create_release_capacity", released == 0, (int64_t)released, 0);
    if (printf("{\"type\":\"stress\",\"scenario\":\"%s\",\"threads\":%u,\"iterations_per_worker\":%" PRIu64 ","
               "\"duration_seconds\":%u,\"elapsed_ms\":%" PRIu64 ",\"seed\":%" PRIu32 ","
               "\"initial_count\":1,\"final_count\":%ld,\"maximum\":%ld,\"sampled_peak\":%ld,"
               "\"sem_acquisitions\":%" PRIu64 ",\"successful_releases\":%" PRIu64 ","
               "\"release_too_many_posts\":%" PRIu64 ",\"release_attempts\":%" PRIu64 ","
               "\"waitall_successes\":%" PRIu64 ",\"waitany_auto_successes\":%" PRIu64 ","
               "\"wait_timeouts\":%" PRIu64 ",\"count_samples\":%" PRIu64 ","
               "\"waitany_0ms_calls\":%" PRIu64 ",\"waitany_1ms_calls\":%" PRIu64 ","
               "\"waitany_1ms_timeouts\":%" PRIu64 ",\"waitany_policy\":\"%s\","
               "\"waitany_1ms_timeout_observed\":%s,\"mach_pool_occupancy_observed\":false}\n",
               blocked ? "blocked_waitall_release_race" : "mixed_waitall_waitany_events",
               count, iterations, duration_seconds, (uint64_t)(GetTickCount64() - start), g.seed,
               info.count, info.maximum, peak, acquired, released,
               full, attempts, all, event, timeouts, queries,
               any_poll_calls, any_timed_calls, any_timed_timeouts,
               duration_seconds ? "role1_random_half_1ms_otherwise_0ms" : "zero_timeout_only",
               any_timed_timeouts ? "true" : "false") < 0) output_failed = 1;
done:
    cleanup_workers(&g, threads, workers);
    close_group(&g);
    if (api("stress.handle_count_after", GetProcessHandleCount(GetCurrentProcess(), &handles_after)) && handles_before_ok) {
        /* Runtime lazy initialization can retain handles. Observe the process
         * count for parent leak analysis; every owned handle is checked above. */
        if (printf("{\"type\":\"resources\",\"scenario\":\"%s\",\"scope\":\"test_process_handles\","
                   "\"before\":%lu,\"after\":%lu,\"delta\":%" PRId64 ",\"observation_only\":true}\n",
                   blocked ? "blocked_waitall_release_race" : "mixed_waitall_waitany_events",
                   (unsigned long)handles_before, (unsigned long)handles_after,
                   (int64_t)handles_after - (int64_t)handles_before) < 0) output_failed = 1;
        flush_output();
    }
}

static int bench_iteration(worker *w)
{
    group *g = w->group;
    HANDLE h[2];
    DWORD result;
    if (g->workload == BENCH_PING) {
        int measured_sender = w->index == 0 && w->measuring;
        int finish_sample = measured_sender && ping_rtt.pending;
        int start_sample;
        LARGE_INTEGER end, wake_end;
        LONG wake_armed;
        BOOL timed = TRUE, wake_timed = TRUE;
        result = WaitForSingleObject(g->ping[w->index], TEST_MS);
        wake_armed = w->index == 1 ? InterlockedExchange(&ping_wake.armed, 0) : 0;
        if (wake_armed) wake_timed = QueryPerformanceCounter(&wake_end);
        /* Capture BEFORE validation/bookkeeping, immediately on wait return. */
        if (finish_sample) timed = QueryPerformanceCounter(&end);
        if (result != WAIT_OBJECT_0) return worker_fail(w, 30, result);
        if (wake_armed) {
            if (!wake_timed) return worker_fail(w, 44, GetLastError());
            if (wake_end.QuadPart < ping_wake.begin.QuadPart || ping_wake.count >= RTT_CAPACITY)
                return worker_fail(w, 45, ERROR_INVALID_DATA);
            ping_wake.ticks[ping_wake.count++] = wake_end.QuadPart - ping_wake.begin.QuadPart;
        }
        if (finish_sample) {
            if (!timed) return worker_fail(w, 41, GetLastError());
            if (end.QuadPart < ping_rtt.begin.QuadPart || ping_rtt.count >= RTT_CAPACITY)
                return worker_fail(w, 42, ERROR_INVALID_DATA);
            ping_rtt.ticks[ping_rtt.count++] = end.QuadPart - ping_rtt.begin.QuadPart;
            ping_rtt.pending = 0;
        }
        start_sample = 0;
        if (measured_sender) {
            ++ping_rtt.outgoing;
            start_sample = ping_rtt.count < RTT_CAPACITY && ping_rtt.outgoing % RTT_INTERVAL == 0;
            ping_rtt.pending = start_sample;
        }
        /* The one-way measurement includes timestamp publication/disarm cost;
         * neither this cost nor QPC cost is subtracted from reported latency. */
        if (start_sample) {
            if (!QueryPerformanceCounter(&ping_rtt.begin)) return worker_fail(w, 43, GetLastError());
            ping_wake.begin = ping_rtt.begin;
            if (InterlockedExchange(&ping_wake.armed, 1)) return worker_fail(w, 46, ERROR_INVALID_DATA);
        }
        if (!SetEvent(g->ping[1 - w->index])) return worker_fail(w, 31, GetLastError());
    } else if (g->workload == BENCH_CS) {
        EnterCriticalSection(&w->private_cs);
        LeaveCriticalSection(&w->private_cs);
    } else if (g->workload == BENCH_MANUAL) {
        h[0] = g->automatic; h[1] = g->manual; /* both manual-reset */
        result = WaitForMultipleObjects(2, h, TRUE, 0);
        if (result != WAIT_OBJECT_0) return worker_fail(w, 32, result);
    } else if (g->workload == BENCH_ALL_SEM) {
        h[0] = g->semaphore; h[1] = w->private_sem;
        result = WaitForMultipleObjects(2, h, TRUE, 0);
        if (result != WAIT_OBJECT_0) return worker_fail(w, 33, result);
        if (!ReleaseSemaphore(h[0], 1, NULL) || !ReleaseSemaphore(h[1], 1, NULL))
            return worker_fail(w, 34, GetLastError());
    } else {
        HANDLE sem = g->workload == BENCH_PRIVATE ? w->private_sem : g->semaphore;
        result = WaitForSingleObject(sem, 0);
        if (result != WAIT_OBJECT_0) return worker_fail(w, 35, result);
        if (!ReleaseSemaphore(sem, 1, NULL)) return worker_fail(w, 36, GetLastError());
    }
    return 1;
}

static uint64_t bench_warmup(const group *g)
{
    if (g->workload == BENCH_CS) return UINT64_C(16000000);
    return g->iterations < 1024 ? g->iterations : UINT64_C(1024);
}

static DWORD WINAPI bench_worker(void *argument)
{
    worker *w = (worker *)argument;
    group *g = w->group;
    uint64_t i, warmup = bench_warmup(g);
    unsigned b, batches = g->iterations < BATCHES ? (unsigned)g->iterations : BATCHES;
    ULONGLONG deadline = GetTickCount64() + RUN_MS;
    for (i = 0; i < warmup && !InterlockedCompareExchange(&g->stop, 0, 0); ++i) {
        if (GetTickCount64() >= deadline) { worker_fail(w, 37, ERROR_TIMEOUT); break; }
        if (!bench_iteration(w)) break;
    }
    if (!group_gate(w)) return 0;
    w->measuring = 1;
    deadline = GetTickCount64() + RUN_MS;
    for (b = 0; b < batches; ++b) {
        LARGE_INTEGER begin, end;
        uint64_t size = g->iterations / batches + (b < g->iterations % batches ? 1 : 0);
        if (!QueryPerformanceCounter(&begin)) { worker_fail(w, 38, GetLastError()); break; }
        for (i = 0; i < size; ++i) {
            /* A shared atomic on EVERY iteration would serialize the independent
             * semaphore benchmark. Sample cancellation/deadlines every 128 ops. */
            if ((i & 127) == 0) {
                if (InterlockedCompareExchange(&g->stop, 0, 0)) { w->completed += i; return 0; }
                if (GetTickCount64() >= deadline) {
                    w->completed += i; worker_fail(w, 39, ERROR_TIMEOUT); return 0;
                }
            }
            if (!bench_iteration(w)) { w->completed += i; return 0; }
        }
        w->completed += size;
        if (!QueryPerformanceCounter(&end) || end.QuadPart < begin.QuadPart) {
            worker_fail(w, 40, ERROR_INVALID_DATA); break;
        }
        w->batch[b].ticks = end.QuadPart - begin.QuadPart;
        w->batch[b].iterations = size;
        ++w->samples;
    }
    return 0;
}

static int compare_ticks(const void *left, const void *right)
{
    LONGLONG a = *(const LONGLONG *)left, b = *(const LONGLONG *)right;
    return (a > b) - (a < b);
}

static double ticks_ns(LONGLONG ticks)
{
    return (double)ticks * 1000000000.0 / (double)frequency.QuadPart;
}

static unsigned rtt_rank(unsigned numerator, unsigned denominator)
{
    /* Called only for nonempty samples; 4096 * 999 fits unsigned int. */
    return (ping_rtt.count * numerator + denominator - 1) / denominator - 1;
}

static void report_roundtrips(void)
{
    unsigned count = ping_rtt.count;
    qsort(ping_rtt.ticks, count, sizeof(ping_rtt.ticks[0]), compare_ticks);
    if (printf("{\"type\":\"roundtrip\",\"name\":\"two_thread_event_pingpong\","
               "\"scope\":\"roundtrip4APIcalls\",\"roundtrip_api_calls\":4,\"sampleCount\":%u,"
               "\"sample_every_outgoing\":%u,\"sample_limit\":%u,\"sampling\":\"prefix_capped\","
               "\"measured_outgoing\":%" PRIu64 ",\"unfinished_sample_discarded\":%s,"
               "\"percentile_method\":\"nearest_rank\",\"qpc_frequency\":%" PRId64 ",",
               count, RTT_INTERVAL, RTT_CAPACITY, ping_rtt.outgoing,
               ping_rtt.pending ? "true" : "false", (int64_t)frequency.QuadPart) < 0)
        output_failed = 1;
    if (count) {
        if (printf("\"p50_ns\":%.3f,\"p95_ns\":%.3f,\"p99_ns\":%.3f,"
                   "\"p99_9_ns\":%.3f,\"max_ns\":%.3f}\n",
                   ticks_ns(ping_rtt.ticks[rtt_rank(50, 100)]),
                   ticks_ns(ping_rtt.ticks[rtt_rank(95, 100)]),
                   ticks_ns(ping_rtt.ticks[rtt_rank(99, 100)]),
                   ticks_ns(ping_rtt.ticks[rtt_rank(999, 1000)]),
                   ticks_ns(ping_rtt.ticks[count - 1])) < 0) output_failed = 1;
    } else {
        /* Fewer than 65 measured iterations cannot finish outgoing sample 64. */
        if (printf("\"p50_ns\":null,\"p95_ns\":null,\"p99_ns\":null,"
                   "\"p99_9_ns\":null,\"max_ns\":null}\n") < 0) output_failed = 1;
    }
    flush_output();
}

static void report_wakeups(void)
{
    unsigned n = ping_wake.count;
    equal("bench.wake_sample_handoff_complete", (int64_t)n, (int64_t)ping_rtt.count + (ping_rtt.pending ? 1 : 0));
    equal("bench.wake_sample_not_armed_after_join", InterlockedCompareExchange(&ping_wake.armed, 0, 0), 0);
    qsort(ping_wake.ticks, n, sizeof(ping_wake.ticks[0]), compare_ticks);
    if (printf("{\"type\":\"wakeup\",\"name\":\"auto_event_single_wake\",\"scope\":\"sender_QPC_before_arm_and_SetEvent_to_peer_QPC_after_Wait_and_disarm\",\"measurement_overhead\":\"two_Interlocked_ops_and_QPC_not_subtracted\",\"sampleCount\":%u,\"sample_every_outgoing\":%u,\"sample_limit\":%u,\"percentile_method\":\"nearest_rank\",\"qpc_frequency\":%" PRId64 ",",n,RTT_INTERVAL,RTT_CAPACITY,(int64_t)frequency.QuadPart) < 0) output_failed = 1;
    if (n) {
        if (printf("\"p50_ns\":%.3f,\"p95_ns\":%.3f,\"p99_ns\":%.3f,\"p99_9_ns\":%.3f,\"max_ns\":%.3f}\n",
                   ticks_ns(ping_wake.ticks[(n*50+99)/100-1]),
                   ticks_ns(ping_wake.ticks[(n*95+99)/100-1]),
                   ticks_ns(ping_wake.ticks[(n*99+99)/100-1]),
                   ticks_ns(ping_wake.ticks[(n*999+999)/1000-1]),ticks_ns(ping_wake.ticks[n-1])) < 0) output_failed = 1;
    } else if (printf("\"p50_ns\":null,\"p95_ns\":null,\"p99_ns\":null,\"p99_9_ns\":null,\"max_ns\":null}\n") < 0) output_failed = 1;
    flush_output();
}

static void benchmark(int workload, const char *name, uint64_t iterations, unsigned count)
{
    group g;
    worker workers[MAX_THREADS];
    HANDLE threads[MAX_THREADS] = {NULL};
    LARGE_INTEGER begin, end;
    LONGLONG sorted[MAX_THREADS * BATCHES], work_ticks = 0;
    uint64_t minimum = UINT64_MAX, maximum = 0, operations;
    unsigned i, j, samples = 0, per_iteration;
    int valid;
    BOOL begin_ok, start_ok, end_ok;
    DWORD wait_result, begin_error, start_error, end_error;
    memset(workers, 0, sizeof(workers));
    if (!create_group(&g, count, iterations)) goto done;
    g.workload = workload;
    if (workload == BENCH_SEM || workload == BENCH_ALL_SEM) {
        g.semaphore = CreateSemaphoreW(NULL, 1, 1, NULL);
        if (!check("bench.create_sem", g.semaphore != NULL, GetLastError(), 0)) goto done;
    }
    if (workload == BENCH_MANUAL) {
        g.automatic = CreateEventW(NULL, TRUE, TRUE, NULL);
        g.manual = CreateEventW(NULL, TRUE, TRUE, NULL);
        if (!check("bench.create_manual_events", g.automatic && g.manual, GetLastError(), 0)) goto done;
    }
    if (workload == BENCH_PING) {
        memset(&ping_rtt, 0, sizeof(ping_rtt));
        memset(&ping_wake, 0, sizeof(ping_wake));
        g.ping[0] = CreateEventW(NULL, FALSE, TRUE, NULL);
        g.ping[1] = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!check("bench.create_ping_events", g.ping[0] && g.ping[1], GetLastError(), 0)) goto done;
    }
    if (workload == BENCH_PRIVATE || workload == BENCH_ALL_SEM) {
        for (i = 0; i < count; ++i) {
            workers[i].private_sem = CreateSemaphoreW(NULL, 1, 1, NULL);
            if (!check("bench.create_private_sem", workers[i].private_sem != NULL, GetLastError(), 0)) goto done;
        }
    }
    if (workload == BENCH_CS) {
        for (i = 0; i < count; ++i) {
            if (!api("bench.initialize_private_cs", InitializeCriticalSectionAndSpinCount(&workers[i].private_cs, 0)))
                goto done;
            workers[i].cs_initialized = 1;
        }
    }
    if (!start_workers(&g, workers, threads, bench_worker)) goto done;
    /* No stdio or validation between the two wall-clock samples. */
    begin_ok = QueryPerformanceCounter(&begin);
    begin_error = begin_ok ? 0 : GetLastError();
    start_ok = begin_ok ? SetEvent(g.go) : FALSE;
    start_error = start_ok ? 0 : GetLastError();
    wait_result = start_ok ? WaitForMultipleObjects(count, threads, TRUE, RUN_MS) : WAIT_FAILED;
    end_ok = QueryPerformanceCounter(&end);
    end_error = end_ok ? 0 : GetLastError();
    if (!check("bench.QPC_begin", begin_ok, begin_error, 0)) goto done;
    if (!check("bench.open_gate", start_ok, start_error, 0)) goto done;
    if (!check("bench.QPC_end", end_ok, end_error, 0)) goto done;
    valid = equal("bench.bounded_join", wait_result, WAIT_OBJECT_0);
    if (!valid) goto done;
    valid = workers_ok(&g, workers) && valid;
    if (!check("bench.QPC_monotonic", end.QuadPart >= begin.QuadPart, end.QuadPart - begin.QuadPart, 0)) valid = 0;
    if (!valid) goto done;
    if (g.semaphore) sem_check("bench.shared_sem_restored", g.semaphore, 1, 1);
    if (workload == BENCH_MANUAL) {
        equal("bench.manual_0_still_ready", WaitForSingleObject(g.automatic, 0), WAIT_OBJECT_0);
        equal("bench.manual_1_still_ready", WaitForSingleObject(g.manual, 0), WAIT_OBJECT_0);
    }
    if (workload == BENCH_PING) {
        equal("bench.ping_token_restored", WaitForSingleObject(g.ping[0], 0), WAIT_OBJECT_0);
        equal("bench.pong_empty", WaitForSingleObject(g.ping[1], 0), WAIT_TIMEOUT);
    }
    for (i = 0; i < count; ++i) {
        worker *w = &workers[i];
        if (w->private_sem) sem_check("bench.private_sem_restored", w->private_sem, 1, 1);
        if (w->cs_initialized) {
            BOOL entered = TryEnterCriticalSection(&w->private_cs);
            check("bench.private_cs_unowned_after_join", entered != FALSE, entered, TRUE);
            if (entered) LeaveCriticalSection(&w->private_cs);
        }
        for (j = 0; j < w->samples; ++j) {
            sorted[samples++] = w->batch[j].ticks;
            work_ticks += w->batch[j].ticks;
            if (w->batch[j].iterations < minimum) minimum = w->batch[j].iterations;
            if (w->batch[j].iterations > maximum) maximum = w->batch[j].iterations;
        }
    }
    if (!check("bench.has_batches", samples > 0, samples, 1)) goto done;
    qsort(sorted, samples, sizeof(sorted[0]), compare_ticks);
    per_iteration = workload == BENCH_MANUAL ? 1 : workload == BENCH_ALL_SEM ? 3 : 2;
    operations = iterations * count * per_iteration;
    if (printf("{\"type\":\"bench\",\"name\":\"%s\",\"threads\":%u,"
               "\"iterations_per_worker\":%" PRIu64 ",\"warmup_iterations_per_worker\":%" PRIu64 ","
               "\"operation_count\":%" PRIu64 ",\"operations_per_worker_iteration\":%u,"
               "\"qpc_frequency\":%" PRId64 ",\"qpc_wall_ticks\":%" PRId64 ","
               "\"qpc_batch_ticks_sum\":%" PRId64 ",\"wall_ns\":%.3f,\"ns_per_op\":%.3f,"
               "\"timing_scope\":\"gate_to_join\",\"batch_count\":%u,"
               "\"batch_iterations_min\":%" PRIu64 ",\"batch_iterations_max\":%" PRIu64 ","
               "\"batch_median_ns\":%.3f,\"batch_p95_ns\":%.3f,\"batch_p99_ns\":%.3f,\"batch_max_ns\":%.3f}\n",
               name, count, iterations, bench_warmup(&g),
               operations, per_iteration, (int64_t)frequency.QuadPart,
               (int64_t)(end.QuadPart - begin.QuadPart), (int64_t)work_ticks,
               ticks_ns(end.QuadPart - begin.QuadPart), ticks_ns(end.QuadPart - begin.QuadPart) / (double)operations,
               samples, minimum, maximum,
               ticks_ns(sorted[(samples - 1) / 2]) * 0.5 + ticks_ns(sorted[samples / 2]) * 0.5,
               ticks_ns(sorted[(samples * 95 + 99) / 100 - 1]),
               ticks_ns(sorted[(samples * 99 + 99) / 100 - 1]), ticks_ns(sorted[samples - 1])) < 0)
        output_failed = 1;
    flush_output();
    if (workload == BENCH_PING) { report_wakeups(); report_roundtrips(); }
done:
    cleanup_workers(&g, threads, workers);
    close_group(&g);
}

static int parse_number(const char *text, uint64_t maximum, uint64_t *value)
{
    char *end;
    unsigned long long parsed;
    if (!text[0] || text[0] < '0' || text[0] > '9') return 0;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno || *end || !parsed || parsed > maximum) return 0;
    *value = (uint64_t)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    HMODULE ntdll;
    FARPROC address;
    uint64_t iterations = 0, count = 0;
    unsigned duration_seconds = 0;
    int child = argc == 4 && !strcmp(argv[1], "--child");
    ntdll = GetModuleHandleW(L"ntdll.dll");
    address = ntdll ? GetProcAddress(ntdll, "NtQuerySemaphore") : NULL;
    /* Function pointer copy avoids a nonportable object/function pointer cast. */
    if (sizeof(address) != sizeof(query_semaphore)) return child ? 30 : finish(1);
    memcpy(&query_semaphore, &address, sizeof(query_semaphore));
    if (child) return query_semaphore ? child_main(argv[2], argv[3]) : 31;
    if (argc >= 2 && !strcmp(argv[1], "--correctness") && argc == 2) mode_name = "correctness";
    else if (argc >= 3 && argc <= 4 &&
             (!strcmp(argv[1], "--stress") || !strcmp(argv[1], "--bench") || !strcmp(argv[1], "--duration")) &&
             parse_number(argv[2], !strcmp(argv[1], "--duration") ? 3600 : MAX_ITERATIONS, &iterations) &&
             (argc != 4 || parse_number(argv[3], MAX_THREADS, &count))) {
        mode_name = !strcmp(argv[1], "--stress") ? "stress" : !strcmp(argv[1], "--duration") ? "duration" : "bench";
        if (!count) count = !strcmp(mode_name, "bench") ? 4 : 6;
        if (strcmp(mode_name, "bench") && count < 4) {
            check("arguments.stress_requires_four_roles", 0, (int64_t)count, 4); return finish(1);
        }
        if (!strcmp(mode_name, "duration")) { duration_seconds = (unsigned)iterations; iterations = 0; }
    } else {
        check("arguments.use_correctness_stress_N_duration_seconds_or_bench_N_with_optional_threads", 0, argc, 0);
        return finish(1);
    }
    if (printf("{\"type\":\"begin\",\"schema\":1,\"mode\":\"%s\",\"pointer_bits\":%u,"
               "\"pid\":%lu,\"safety_timeout_ms\":%u,\"iterations_per_worker\":%" PRIu64 ","
               "\"requested_threads\":%" PRIu64 ",\"duration_seconds\":%u}\n", mode_name, (unsigned)(sizeof(void *) * 8),
               (unsigned long)GetCurrentProcessId(), duration_seconds ? duration_seconds * 1000 + TEST_MS : RUN_MS,
               iterations, count, duration_seconds) < 0) output_failed = 1;
    flush_output();
    if (!check("NtQuerySemaphore.available", query_semaphore != NULL, query_semaphore != NULL, 1)) return finish(1);
    if (!api("QueryPerformanceFrequency", QueryPerformanceFrequency(&frequency))) return finish(1);
    if (!check("QPC.frequency_positive", frequency.QuadPart > 0, frequency.QuadPart, 1)) return finish(1);
    if (!strcmp(mode_name, "correctness")) {
        not_ready(); mixed_ready(); waitany_priority(); apc_priority();
        wide_waitall(8); wide_waitall(64);
        abandoned(0); abandoned(1); crossprocess();
    } else if (!strcmp(mode_name, "stress")) {
        stress_case(iterations, (unsigned)count, 1, 0);
        stress_case(iterations, (unsigned)count, 0, 0);
    } else if (!strcmp(mode_name, "duration")) stress_case(0, (unsigned)count, 0, duration_seconds);
    else {
        benchmark(BENCH_SEM, "sem_wait1_release", iterations, 1);
        benchmark(BENCH_MANUAL, "ready_WaitAll2_manual", iterations, 1);
        benchmark(BENCH_ALL_SEM, "WaitAll2_semaphores_releases", iterations, 1);
        benchmark(BENCH_PRIVATE, "independent_per_thread_semaphore", iterations, (unsigned)count);
        /* The syscall benchmarks need short iteration counts; the attribution
         * control needs a longer interval to avoid thread-start/join noise.
         * Its JSON row records this actual, distinct iteration count. */
        /* The v2 control lasted roughly 0.1s and varied between identical PE
         * paths. Use a fixed longer warmup and measurement; retain v2 results
         * separately rather than discard noisy trials or relax the threshold. */
        benchmark(BENCH_CS, "uncontended_private_critical_section", iterations * 1024, (unsigned)count);
        benchmark(BENCH_PING, "two_thread_event_pingpong", iterations, 2);
    }
    return finish(0);
}
