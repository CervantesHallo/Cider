/*
 * Public Win32 lifecycle probe for either MinGW i686 or x86_64 target:
 *   gcc -std=c99 -O2 -Wall -Wextra -o waitall-lifecycle.exe waitall-lifecycle.c
 *   waitall-lifecycle.exe [--iterations 64] [--seed 0x51c1d3a7]
 * Iterations: 1..4096, per case; each child also executes that many WaitAlls.
 * Only the parent writes JSONL. Acceptance requires complete.ok=true, exit 0,
 * and an intact output stream. Compilation alone supplies no runtime evidence.
 *
 * Each interruption round suspends a looping WaitAll/ReleaseMutex worker,
 * obtains CONTEXT_CONTROL, resumes it, observes new progress, then terminates
 * it after a reproducible pseudorandom delay. This samples live transactions;
 * it does NOT identify an internal journal/lease boundary. Normal and abandoned
 * mutex acquisition after death are both valid, depending on the schedule.
 * Process rounds create this same executable suspended, assign a private
 * kill-on-close job, then resume it while a local WaitAll worker remains active.
 * WaitAny rounds reuse two unsignaled events across all terminated threads.
 * Readiness plus a live-thread sample does not prove native wait registration.
 * Mach registrations, pool occupancy and native FD rotation require separate
 * native observation. No private Wine interfaces or pool counters are used.
 *
 * All waits have finite timeouts, continuous workers also have time/call caps,
 * and no worker uses stdio, malloc, or a CRT lock. A context and its handles stay
 * alive until its owned thread is reaped. Unreapable cleanup terminates THIS
 * process without unwinding a worker's stack context. Child cleanup targets
 * only the exact process/job created here; assignment failure leaves a suspended
 * child which is terminated and reaped directly. No process enumeration/killing.
 * An external watchdog is still needed if an API itself or output I/O hangs.
 * Reported case ticks include scheduling, checks, I/O, creation and cleanup;
 * they are lifecycle timings, not isolated WaitAll latency or a performance gate.
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

#define DEFAULT_ITERATIONS 64u
#define MAX_ITERATIONS 4096u
#define WAIT_MS 5000u
#define BLOCK_MS 10000u
#define REAP_MS 5000u
#define WORKER_MS 30000u
#define MAX_WORKER_PAIRS 100000000L
#define TERMINATED_CODE 19u
#define CLEANUP_CODE 97u
#define PATH_CHARS 32768u
#define NAME_CHARS 160u

typedef struct fixture {
    HANDLE all[3]; /* two ready manual events, then an initially unowned mutex */
} fixture;

typedef struct worker {
    HANDLE all[3], ready;
    volatile LONG stop, progress;
    DWORD wait_result, error;
} worker;

typedef struct blocked_worker {
    HANDLE events[2], ready;
    volatile LONG armed;
} blocked_worker;

typedef struct counts {
    unsigned interrupted, suspended, resumed, terminated;
    unsigned normal_mutex, abandoned_mutex, children_created, children_reaped;
    unsigned process_rounds, waitany_armed, waitany_live, waitany_terminated, waitany_rounds;
    uint64_t worker_pairs, child_pairs, recovery_pairs;
} counts;

static unsigned iterations = DEFAULT_ITERATIONS, checks, failures, round_number;
static uint32_t random_state = UINT32_C(0x51c1d3a7);
static int output_failed;
static const char *case_name = "setup";
static LARGE_INTEGER frequency;
static WCHAR executable[PATH_CHARS];
static HANDLE active_job;
static counts totals;

static void flush_output(void)
{
    /* ferror is sticky; a later successful flush cannot undo an earlier error. */
    if (fflush(stdout) != 0 || ferror(stdout)) output_failed = 1;
}

static int check(const char *name, int ok, int64_t actual, int64_t expected)
{
    ++checks;
    if (!ok) ++failures;
    if (printf("{\"type\":\"check\",\"case\":\"%s\",\"iteration\":%u,"
               "\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRId64 ","
               "\"expected\":%" PRId64 "}\n", case_name, round_number, name,
               ok ? "true" : "false", actual, expected) < 0) output_failed = 1;
    flush_output();
    return ok && !output_failed;
}

static int equal(const char *name, DWORD actual, DWORD expected)
{
    return check(name, actual == expected, actual, expected);
}

static int api(const char *name, BOOL ok)
{
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    return check(name, ok != FALSE, error, ERROR_SUCCESS);
}

static int created(const char *name, HANDLE handle)
{
    return api(name, handle != NULL);
}

static void close_handle(HANDLE *handle)
{
    if (*handle) {
        BOOL closed = CloseHandle(*handle);
        api("cleanup.CloseHandle", closed);
        /* Handle lifetime follows the API result even if logging failed. */
        if (closed) *handle = NULL;
    }
}

static LONG read_long(volatile LONG *value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

static uint32_t next_random(void)
{
    uint32_t value = random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random_state = value;
    return value;
}

static int finish(void)
{
    int ok;
    flush_output();
    ok = !failures && !output_failed && totals.interrupted == iterations &&
         totals.suspended == iterations && totals.resumed == iterations &&
         totals.terminated == iterations && totals.process_rounds == iterations &&
         totals.children_created == iterations && totals.children_reaped == iterations &&
         totals.waitany_armed == iterations && totals.waitany_live == iterations &&
         totals.waitany_terminated == iterations && totals.waitany_rounds == iterations;
    if (printf("{\"type\":\"complete\",\"ok\":%s,\"checks\":%u,\"failures\":%u,"
               "\"output_error\":%s,\"requested_per_case\":%u,"
               "\"interruption_rounds\":%u,\"suspensions_with_context\":%u,\"resumed_progress\":%u,"
               "\"terminated_waitall_threads\":%u,\"mutex_normal\":%u,"
               "\"mutex_abandoned\":%u,\"process_rounds\":%u,\"children_created\":%u,"
               "\"children_reaped\":%u,\"waitany_armed\":%u,\"waitany_live_samples\":%u,"
               "\"terminated_waitany_threads\":%u,\"waitany_rounds\":%u,"
               "\"worker_completed_pairs_observed\":%" PRIu64 ","
               "\"child_completed_pairs\":%" PRIu64 ",\"parent_recovery_pairs\":%" PRIu64 "}\n",
               ok ? "true" : "false", checks, failures, output_failed ? "true" : "false",
               iterations, totals.interrupted, totals.suspended, totals.resumed,
               totals.terminated, totals.normal_mutex, totals.abandoned_mutex,
               totals.process_rounds, totals.children_created, totals.children_reaped,
               totals.waitany_armed, totals.waitany_live, totals.waitany_terminated, totals.waitany_rounds,
               totals.worker_pairs, totals.child_pairs, totals.recovery_pairs) < 0)
        output_failed = 1;
    flush_output();
    return ok && !output_failed ? 0 : 1;
}

static void fatal_cleanup(void)
{
    /* Do not return, close a live worker's fixture, or unwind its stack context. */
    check("cleanup.unreapable_owned_execution", 0, 1, 0);
    if (active_job) {
        (void)TerminateJobObject(active_job, CLEANUP_CODE);
        (void)CloseHandle(active_job);
        active_job = NULL;
    }
    (void)finish();
    TerminateProcess(GetCurrentProcess(), CLEANUP_CODE);
    ExitProcess(CLEANUP_CODE);
}

/* Always reap before returning, even if the initial wait or exit check fails. */
static int join_thread(HANDLE *thread, DWORD expected)
{
    DWORD result, code = STILL_ACTIVE;
    int ok;
    if (!*thread) return 1;
    result = WaitForSingleObject(*thread, REAP_MS);
    ok = equal("thread.bounded_join", result, WAIT_OBJECT_0);
    if (result != WAIT_OBJECT_0) {
        api("cleanup.TerminateThread", TerminateThread(*thread, CLEANUP_CODE));
        result = WaitForSingleObject(*thread, REAP_MS);
        if (!equal("cleanup.thread_reaped", result, WAIT_OBJECT_0)) fatal_cleanup();
    }
    if (api("thread.GetExitCodeThread", GetExitCodeThread(*thread, &code)))
        ok = equal("thread.exit_code", code, expected) && ok;
    else ok = 0;
    close_handle(thread);
    return ok && !failures && !output_failed;
}

static int make_fixture(fixture *f)
{
    memset(f, 0, sizeof(*f));
    f->all[0] = CreateEventW(NULL, TRUE, TRUE, NULL);
    if (!created("fixture.manual_event_0", f->all[0])) return 0;
    f->all[1] = CreateEventW(NULL, TRUE, TRUE, NULL);
    if (!created("fixture.manual_event_1", f->all[1])) return 0;
    f->all[2] = CreateMutexW(NULL, FALSE, NULL);
    return created("fixture.mutex", f->all[2]);
}

static void close_fixture(fixture *f)
{
    unsigned i;
    for (i = 0; i < 3; ++i) close_handle(&f->all[i]);
}

static DWORD WINAPI waitall_worker(void *argument)
{
    worker *w = (worker *)argument;
    ULONGLONG begin = GetTickCount64();
    LONG completed = 0;
    while (!read_long(&w->stop)) {
        if (GetTickCount64() - begin >= WORKER_MS || completed >= MAX_WORKER_PAIRS)
            return 20;
        w->wait_result = WaitForMultipleObjects(3, w->all, TRUE, WAIT_MS);
        if (w->wait_result != WAIT_OBJECT_0) {
            w->error = w->wait_result == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
            /* An unexpected abandoned success still conveys ownership. */
            if (w->wait_result >= WAIT_ABANDONED_0 && w->wait_result < WAIT_ABANDONED_0 + 3)
                (void)ReleaseMutex(w->all[2]);
            return 21;
        }
        if (!ReleaseMutex(w->all[2])) { w->error = GetLastError(); return 22; }
        completed = InterlockedIncrement(&w->progress);
        /* Signal after the first successful WaitAll AND ReleaseMutex. */
        if (completed == 1 && !SetEvent(w->ready)) { w->error = GetLastError(); return 23; }
    }
    return 0;
}

static int start_worker(worker *w, fixture *f, HANDLE *thread)
{
    memset(w, 0, sizeof(*w));
    memcpy(w->all, f->all, sizeof(w->all));
    w->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!created("worker.ready_event", w->ready)) return 0;
    *thread = CreateThread(NULL, 0, waitall_worker, w, 0, NULL);
    if (!created("worker.CreateThread", *thread)) return 0;
    return equal("worker.first_pair_ready", WaitForSingleObject(w->ready, WAIT_MS), WAIT_OBJECT_0);
}

static int progress_after(worker *w, HANDLE thread, LONG previous)
{
    ULONGLONG begin = GetTickCount64();
    LONG current;
    DWORD state;
    do {
        current = read_long(&w->progress);
        if (current > previous) return check("worker.new_progress", 1, current, previous + 1);
        state = WaitForSingleObject(thread, 0);
        if (state != WAIT_TIMEOUT) return equal("worker.must_remain_live", state, WAIT_TIMEOUT);
        Sleep(1);
    } while (GetTickCount64() - begin < WAIT_MS);
    return check("worker.new_progress", 0, read_long(&w->progress), previous + 1);
}

static void stop_worker(worker *w, HANDLE *thread)
{
    if (*thread) {
        InterlockedExchange(&w->stop, 1);
        (void)join_thread(thread, 0);
    }
    totals.worker_pairs += (uint64_t)(unsigned long)read_long(&w->progress);
    close_handle(&w->ready);
}

static int no_extra_ownership(HANDLE mutex)
{
    BOOL released;
    DWORD error;
    SetLastError(ERROR_SUCCESS);
    released = ReleaseMutex(mutex);
    error = released ? ERROR_SUCCESS : GetLastError();
    return check("mutex.no_extra_ownership", !released && error == ERROR_NOT_OWNER,
                 error, ERROR_NOT_OWNER);
}

static int recovery_waitall(fixture *f)
{
    DWORD result = WaitForMultipleObjects(3, f->all, TRUE, WAIT_MS);
    int acquired = result == WAIT_OBJECT_0 ||
                   (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + 3);
    int ok = equal("parent.recovery_WaitAll", result, WAIT_OBJECT_0);
    if (acquired) {
        if (!api("parent.recovery_ReleaseMutex", ReleaseMutex(f->all[2]))) return 0;
        if (ok) ++totals.recovery_pairs;
        return no_extra_ownership(f->all[2]) && ok;
    }
    return 0;
}

static int mutex_after_death(fixture *f)
{
    DWORD result = WaitForSingleObject(f->all[2], WAIT_MS);
    int acquired = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED_0;
    if (!check("mutex.after_death.normal_or_abandoned", acquired, result, WAIT_OBJECT_0)) {
        if (acquired) (void)ReleaseMutex(f->all[2]);
        return 0;
    }
    if (result == WAIT_ABANDONED_0) ++totals.abandoned_mutex;
    else ++totals.normal_mutex;
    if (!api("mutex.after_death.release", ReleaseMutex(f->all[2]))) return 0;
    if (!no_extra_ownership(f->all[2])) return 0;
    /* Probe availability by acquiring again: abandonment must have cleared. */
    result = WaitForSingleObject(f->all[2], WAIT_MS);
    acquired = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED_0;
    if (!equal("mutex.query_again.acquire", result, WAIT_OBJECT_0)) {
        if (acquired) (void)ReleaseMutex(f->all[2]);
        return 0;
    }
    if (!api("mutex.query_again.release", ReleaseMutex(f->all[2]))) return 0;
    if (!no_extra_ownership(f->all[2])) return 0;
    return recovery_waitall(f);
}

static void report_case(LARGE_INTEGER begin, unsigned before, unsigned completed)
{
    LARGE_INTEGER end;
    if (!api("QueryPerformanceCounter.end", QueryPerformanceCounter(&end))) return;
    if (printf("{\"type\":\"case\",\"name\":\"%s\",\"ok\":%s,\"completed\":%u,"
               "\"requested\":%u,\"elapsed_ticks\":%" PRId64 ",\"frequency\":%" PRId64 ","
               "\"timing_includes_checks_io_creation_cleanup\":true}\n", case_name,
               failures == before && !output_failed && completed == iterations ? "true" : "false",
               completed, iterations, (int64_t)(end.QuadPart - begin.QuadPart),
               (int64_t)frequency.QuadPart) < 0) output_failed = 1;
    flush_output();
}

static void interruption_case(void)
{
    LARGE_INTEGER begin;
    unsigned before = failures;
    case_name = "suspend_context_resume_terminate_waitall";
    round_number = 0;
    if (!api("QueryPerformanceCounter.begin", QueryPerformanceCounter(&begin))) return;
    for (round_number = 1; round_number <= iterations && !failures && !output_failed; ++round_number) {
        fixture f;
        worker w;
        HANDLE thread = NULL;
        CONTEXT context;
        DWORD result;
        LONG suspended_progress;
        int suspended = 0;
        memset(&w, 0, sizeof(w));
        if (!make_fixture(&f)) goto done;
        if (!start_worker(&w, &f, &thread)) goto done;
        result = SuspendThread(thread);
        suspended = result != (DWORD)-1;
        if (!equal("worker.SuspendThread.previous_count", result, 0)) goto done;
        memset(&context, 0, sizeof(context));
        context.ContextFlags = CONTEXT_CONTROL;
        if (!api("worker.GetThreadContext.CONTEXT_CONTROL", GetThreadContext(thread, &context))) goto done;
        ++totals.suspended;
        suspended_progress = read_long(&w.progress);
        Sleep(1);
        if (!check("worker.suspended_progress_stable", read_long(&w.progress) == suspended_progress,
                   read_long(&w.progress), suspended_progress)) goto done;
        result = ResumeThread(thread);
        if (result != (DWORD)-1) suspended = 0;
        if (!equal("worker.ResumeThread.previous_count", result, 1)) goto done;
        if (!progress_after(&w, thread, suspended_progress)) goto done;
        ++totals.resumed;
        /* No handshake into the implementation: interrupt an arbitrary live phase. */
        Sleep(next_random() & 3u);
        if (!equal("worker.live_before_termination", WaitForSingleObject(thread, 0), WAIT_TIMEOUT)) goto done;
        if (!api("worker.TerminateThread.19", TerminateThread(thread, TERMINATED_CODE))) goto done;
        if (!join_thread(&thread, TERMINATED_CODE)) goto done;
        ++totals.terminated;
        if (!mutex_after_death(&f)) goto done;
        ++totals.interrupted;
done:
        if (suspended && thread) {
            result = ResumeThread(thread);
            equal("cleanup.ResumeThread.previous_count", result, 1);
        }
        stop_worker(&w, &thread);
        close_fixture(&f);
    }
    report_case(begin, before, totals.interrupted);
}

static int make_named_event(HANDLE *event, const WCHAR *name)
{
    DWORD error;
    SetLastError(ERROR_SUCCESS);
    *event = CreateEventW(NULL, TRUE, FALSE, name);
    error = GetLastError();
    if (!created("process.handshake_event", *event)) return 0;
    return check("process.handshake_name_is_new", error != ERROR_ALREADY_EXISTS, error, ERROR_SUCCESS);
}

static void cleanup_process(PROCESS_INFORMATION *process, int assigned)
{
    DWORD state;
    if (!process->hProcess) return;
    state = WaitForSingleObject(process->hProcess, 0);
    if (state != WAIT_OBJECT_0) {
        BOOL killed = FALSE;
        if (assigned) {
            killed = TerminateJobObject(active_job, CLEANUP_CODE);
            api("cleanup.TerminateJobObject.owned_job", killed);
        }
        if (!killed) api("cleanup.TerminateProcess.owned_child", TerminateProcess(process->hProcess, CLEANUP_CODE));
        state = WaitForSingleObject(process->hProcess, REAP_MS);
    }
    if (!equal("cleanup.child_reaped", state, WAIT_OBJECT_0)) fatal_cleanup();
    ++totals.children_reaped;
    close_handle(&process->hThread);
    close_handle(&process->hProcess);
}

static void process_case(void)
{
    LARGE_INTEGER begin;
    unsigned before = failures;
    case_name = "createprocess_during_local_waitall";
    round_number = 0;
    if (!api("QueryPerformanceCounter.begin", QueryPerformanceCounter(&begin))) return;
    for (round_number = 1; round_number <= iterations && !failures && !output_failed; ++round_number) {
        fixture f;
        worker w;
        HANDLE thread = NULL, child_ready = NULL, child_go = NULL;
        PROCESS_INFORMATION process;
        STARTUPINFOW startup;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
        LARGE_INTEGER token;
        WCHAR ready_name[NAME_CHARS], go_name[NAME_CHARS], command[PATH_CHARS];
        DWORD result, code = STILL_ACTIVE;
        LONG previous;
        int assigned = 0, formatted, completed = 0;
        BOOL launched = FALSE;
        memset(&w, 0, sizeof(w));
        memset(&process, 0, sizeof(process));
        memset(&startup, 0, sizeof(startup));
        startup.cb = sizeof(startup);
        if (!make_fixture(&f)) goto done;
        if (!start_worker(&w, &f, &thread)) goto done;
        if (!api("process.name_token", QueryPerformanceCounter(&token))) goto done;
        formatted = swprintf(ready_name, NAME_CHARS,
                             L"Local\\Cider.waitall-lifecycle.%lu.%08lx%08lx.%u.ready",
                             (unsigned long)GetCurrentProcessId(), (unsigned long)token.HighPart,
                             (unsigned long)token.LowPart, round_number);
        if (!check("process.ready_name_fits", formatted > 0 && (unsigned)formatted < NAME_CHARS,
                   formatted, NAME_CHARS - 1)) goto done;
        formatted = swprintf(go_name, NAME_CHARS,
                             L"Local\\Cider.waitall-lifecycle.%lu.%08lx%08lx.%u.go",
                             (unsigned long)GetCurrentProcessId(), (unsigned long)token.HighPart,
                             (unsigned long)token.LowPart, round_number);
        if (!check("process.go_name_fits", formatted > 0 && (unsigned)formatted < NAME_CHARS,
                   formatted, NAME_CHARS - 1)) goto done;
        if (!make_named_event(&child_ready, ready_name) || !make_named_event(&child_go, go_name)) goto done;
        active_job = CreateJobObjectW(NULL, NULL);
        if (!created("process.private_job", active_job)) goto done;
        memset(&limits, 0, sizeof(limits));
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!api("process.kill_on_close", SetInformationJobObject(active_job,
                 JobObjectExtendedLimitInformation, &limits, sizeof(limits)))) goto done;
        formatted = swprintf(command, PATH_CHARS, L"\"%ls\" --child %u \"%ls\" \"%ls\"",
                             executable, iterations, ready_name, go_name);
        if (!check("process.command_fits", formatted > 0 && (unsigned)formatted < PATH_CHARS - 1,
                   formatted, PATH_CHARS - 2)) goto done;
        previous = read_long(&w.progress);
        /* No inherited handles. The child opens only its two named events. */
        launched = CreateProcessW(executable, command, NULL, NULL, FALSE,
                                 CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
        if (launched) ++totals.children_created;
        if (!api("process.CreateProcessW.suspended", launched)) goto done;
        assigned = AssignProcessToJobObject(active_job, process.hProcess) != FALSE;
        if (!api("process.assign_before_resume", assigned)) goto done;
        result = ResumeThread(process.hThread);
        if (!equal("process.ResumeThread.previous_count", result, 1)) goto done;
        if (!equal("process.child_ready", WaitForSingleObject(child_ready, WAIT_MS), WAIT_OBJECT_0)) goto done;
        if (!progress_after(&w, thread, previous)) goto done;
        previous = read_long(&w.progress);
        if (!api("process.release_child_gate", SetEvent(child_go))) goto done;
        if (!equal("process.bounded_join", WaitForSingleObject(process.hProcess, WAIT_MS), WAIT_OBJECT_0)) goto done;
        if (!api("process.GetExitCodeProcess", GetExitCodeProcess(process.hProcess, &code))) goto done;
        if (!equal("process.child_exit_code", code, 0)) goto done;
        totals.child_pairs += iterations;
        /* This checks the same worker still progresses after fork/child exit. */
        if (!progress_after(&w, thread, previous)) goto done;
        completed = 1;
done:
        if (launched) cleanup_process(&process, assigned);
        close_handle(&active_job);
        stop_worker(&w, &thread);
        if (completed && !failures && !output_failed && recovery_waitall(&f)) ++totals.process_rounds;
        close_handle(&child_ready);
        close_handle(&child_go);
        close_fixture(&f);
    }
    report_case(begin, before, totals.process_rounds);
}

static DWORD WINAPI waitany_worker(void *argument)
{
    blocked_worker *w = (blocked_worker *)argument;
    DWORD result;
    InterlockedExchange(&w->armed, 1);
    if (!SetEvent(w->ready)) return 30;
    result = WaitForMultipleObjects(2, w->events, FALSE, BLOCK_MS);
    /* Any natural exit is a failure in the parent's expected-exit-19 check. */
    return result == WAIT_TIMEOUT ? 31 : 32;
}

static void waitany_case(void)
{
    LARGE_INTEGER begin;
    unsigned before = failures, i;
    HANDLE events[2] = {NULL, NULL};
    fixture f;
    case_name = "terminate_waitany_persistent_unsignaled_events";
    round_number = 0;
    memset(&f, 0, sizeof(f));
    if (!api("QueryPerformanceCounter.begin", QueryPerformanceCounter(&begin))) return;
    if (!make_fixture(&f)) goto done;
    for (i = 0; i < 2; ++i) {
        events[i] = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!created("waitany.persistent_unsignaled_event", events[i])) goto done;
    }
    for (round_number = 1; round_number <= iterations && !failures && !output_failed; ++round_number) {
        blocked_worker w;
        HANDLE thread = NULL;
        memset(&w, 0, sizeof(w));
        memcpy(w.events, events, sizeof(events));
        w.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!created("waitany.ready_event", w.ready)) goto round_done;
        thread = CreateThread(NULL, 0, waitany_worker, &w, 0, NULL);
        if (!created("waitany.CreateThread", thread)) goto round_done;
        if (!equal("waitany.armed_ready", WaitForSingleObject(w.ready, WAIT_MS), WAIT_OBJECT_0)) goto round_done;
        if (!check("waitany.armed_flag", read_long(&w.armed) == 1, read_long(&w.armed), 1)) goto round_done;
        ++totals.waitany_armed;
        /* Allow blocking, without claiming this establishes native registration. */
        Sleep(1u + (next_random() & 3u));
        if (!equal("waitany.live_sample", WaitForSingleObject(thread, 0), WAIT_TIMEOUT)) goto round_done;
        ++totals.waitany_live;
        if (!api("waitany.TerminateThread.19", TerminateThread(thread, TERMINATED_CODE))) goto round_done;
        if (!join_thread(&thread, TERMINATED_CODE)) goto round_done;
        ++totals.waitany_terminated;
        for (i = 0; i < 2; ++i)
            if (!equal("waitany.event_still_unsignaled", WaitForSingleObject(events[i], 0), WAIT_TIMEOUT)) goto round_done;
        if (!equal("waitany.query_again", WaitForMultipleObjects(2, events, FALSE, 0), WAIT_TIMEOUT)) goto round_done;
        if (!recovery_waitall(&f)) goto round_done;
        ++totals.waitany_rounds;
round_done:
        if (thread) {
            /* The blocked test has no stop signal: contain only this owned thread. */
            api("cleanup.waitany.TerminateThread", TerminateThread(thread, CLEANUP_CODE));
            (void)join_thread(&thread, CLEANUP_CODE);
        }
        close_handle(&w.ready);
    }
done:
    for (i = 0; i < 2; ++i) close_handle(&events[i]);
    close_fixture(&f);
    report_case(begin, before, totals.waitany_rounds);
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;
    if (!text[0] || text[0] < '0' || text[0] > '9') return 0;
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno || *end || !parsed || parsed > UINT32_MAX) return 0;
    *value = (uint32_t)parsed;
    return 1;
}

static int child_main(int argc, char **argv)
{
    HANDLE all[3] = {NULL, NULL, NULL}, ready = NULL, go = NULL;
    WCHAR ready_name[NAME_CHARS], go_name[NAME_CHARS];
    uint32_t count;
    unsigned i;
    int code = 40;
    ULONGLONG begin;
    if (argc != 5 || !parse_u32(argv[2], &count) || count > MAX_ITERATIONS) return 40;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[3], -1, ready_name, NAME_CHARS) ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[4], -1, go_name, NAME_CHARS)) return 41;
    ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready_name);
    go = OpenEventW(SYNCHRONIZE, FALSE, go_name);
    if (!ready || !go) { code = 42; goto done; }
    all[0] = CreateEventW(NULL, TRUE, TRUE, NULL);
    all[1] = CreateEventW(NULL, TRUE, TRUE, NULL);
    all[2] = CreateMutexW(NULL, FALSE, NULL);
    if (!all[0] || !all[1] || !all[2]) { code = 43; goto done; }
    if (!SetEvent(ready)) { code = 44; goto done; }
    if (WaitForSingleObject(go, WAIT_MS) != WAIT_OBJECT_0) { code = 45; goto done; }
    begin = GetTickCount64();
    for (i = 0; i < count; ++i) {
        DWORD result;
        if (GetTickCount64() - begin >= WORKER_MS) { code = 46; goto done; }
        result = WaitForMultipleObjects(3, all, TRUE, WAIT_MS);
        if (result != WAIT_OBJECT_0) {
            if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + 3)
                (void)ReleaseMutex(all[2]);
            code = 47; goto done;
        }
        if (!ReleaseMutex(all[2])) { code = 48; goto done; }
    }
    code = 0;
done:
    for (i = 0; i < 3; ++i) if (all[i] && !CloseHandle(all[i])) code = 49;
    if (ready && !CloseHandle(ready)) code = 49;
    if (go && !CloseHandle(go)) code = 49;
    return code;
}

int main(int argc, char **argv)
{
    DWORD length;
    uint32_t value;
    int i, valid = 1;
    if (argc >= 2 && !strcmp(argv[1], "--child")) return child_main(argc, argv);
    for (i = 1; i < argc; ++i) {
        if (i + 1 >= argc || !parse_u32(argv[i + 1], &value)) { valid = 0; break; }
        if (!strcmp(argv[i], "--iterations") && value <= MAX_ITERATIONS) iterations = value;
        else if (!strcmp(argv[i], "--seed")) random_state = value;
        else { valid = 0; break; }
        ++i;
    }
    if (!check("arguments.valid_iterations_and_nonzero_seed", valid, valid, 1)) return finish();
    if (printf("{\"type\":\"limits\",\"iterations_per_case\":%u,\"child_iterations\":%u,"
               "\"max_iterations\":%u,\"seed\":%" PRIu32 ",\"pointer_bits\":%u,"
               "\"wait_ms\":%u,\"blocked_wait_ms\":%u,\"reap_ms\":%u,"
               "\"worker_limit_ms\":%u,\"worker_limit_pairs\":%ld,"
               "\"termination_exit_code\":%u,\"internal_boundary_injection\":false,"
               "\"native_registration_or_pool_measurement\":false,\"external_watchdog_required\":true}\n",
               iterations, iterations, MAX_ITERATIONS, random_state, (unsigned)(sizeof(void *) * 8),
               WAIT_MS, BLOCK_MS, REAP_MS, WORKER_MS, MAX_WORKER_PAIRS, TERMINATED_CODE) < 0)
        output_failed = 1;
    flush_output();
    if (output_failed) return finish();
    if (!api("QueryPerformanceFrequency", QueryPerformanceFrequency(&frequency))) return finish();
    if (!check("QueryPerformanceFrequency.positive", frequency.QuadPart > 0, frequency.QuadPart, 1)) return finish();
    length = GetModuleFileNameW(NULL, executable, PATH_CHARS);
    if (!check("self_executable_path.complete", length > 0 && length < PATH_CHARS, length, PATH_CHARS - 1)) return finish();
    interruption_case();
    if (!failures && !output_failed) process_case();
    if (!failures && !output_failed) waitany_case();
    return finish();
}
