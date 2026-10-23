/* Public Win32 single-event lifecycle probe for MinGW i686 / x86_64.
 *   event-lifecycle.exe [--iterations 64]       (1..256)
 *
 * Reuses ONE initially unsignaled manual-reset subject across all rounds.
 * Each round terminates an owned ready/live worker after suspend/context/resume,
 * checks that the subject remains unsignaled, then wakes a fresh owned worker
 * with SetEvent, checks retained manual signaling, and resets the same subject.
 * Ready + Sleep(2) + a live-thread sample DOES NOT prove native/kernel wait
 * registration or identify an internal transaction boundary.
 *
 * A worker only signals its ready event and waits on subject with INFINITE.
 * Workers never use stdio, malloc or a CRT lock. The main-stack worker context,
 * ready and subject handles survive until its exact thread has been reaped.
 * Failed joins trigger bounded cleanup of that owned thread. If reaping cannot
 * be confirmed, terminate THIS process without returning/unwinding that stack.
 * No other process is opened, enumerated, signaled or terminated.
 *
 * Ready/join/cleanup waits are bounded at 5000 ms. An external host watchdog is
 * still required if an API itself or output I/O hangs; the intentional worker
 * INFINITE wait is not an acceptance timeout extension. After closing handles,
 * an idle row precedes a 1500 ms observation hold and the unique complete row.
 * Acceptance requires exit 0, complete.ok/passed=true and intact JSONL output.
 * These are public API observations, not native registration, Windows kernel
 * equivalence, performance, or game compatibility evidence.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ITERATIONS 64u
#define MAX_ITERATIONS 256u
#define WAIT_MS 5000u
#define HOLD_MS 1500u
#define READY_SETTLE_MS 2u
#define TERMINATED_CODE 19u
#define CLEANUP_CODE 97u

typedef struct worker {
    HANDLE subject, ready, thread;
    DWORD wait_result, error;
    int reaped, suspended;
} worker;

typedef struct counts {
    unsigned int created, reaped, termination_requests, terminated, woken;
    unsigned int suspended, contexts, resumed, rounds, cleanup_terminations;
} counts;

static unsigned int iterations = DEFAULT_ITERATIONS, round_number, checks, failures;
static int output_failed, completion_written;
static const char *case_name = "setup";
static counts totals;

static void flush_output(void)
{
    /* ferror is sticky: a later successful flush must not erase an earlier loss. */
    if (fflush(stdout) != 0 || ferror(stdout)) output_failed = 1;
}

static int check(const char *name, int ok, int64_t actual, int64_t expected)
{
    ++checks;
    if (!ok) ++failures;
    if (printf("{\"type\":\"check\",\"case\":\"%s\",\"iteration\":%u,"
               "\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRId64 ",\"expected\":%" PRId64 "}\n",
               case_name, round_number, name, ok ? "true" : "false", actual, expected) < 0)
        output_failed = 1;
    flush_output();
    return ok && !output_failed;
}

static int equal(const char *name, DWORD actual, DWORD expected)
{
    return check(name, actual == expected, (int64_t)actual, (int64_t)expected);
}

static int api(const char *name, BOOL ok)
{
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    return check(name, ok != FALSE, (int64_t)error, (int64_t)ERROR_SUCCESS);
}

static void close_handle(HANDLE *handle, const char *name)
{
    if (*handle) {
        BOOL closed = CloseHandle(*handle);
        (void)api(name, closed);
        /* Lifetime follows the actual API result even when logging failed. */
        if (closed) *handle = NULL;
    }
}

static int complete(int cleanup_checked)
{
    int ok;
    if (completion_written) return 1;
    completion_written = 1;
    flush_output();
    ok = !failures && !output_failed && cleanup_checked &&
         totals.created == iterations * 2u && totals.reaped == totals.created &&
         totals.termination_requests == iterations && totals.terminated == iterations &&
         totals.woken == iterations && totals.suspended == iterations &&
         totals.contexts == iterations && totals.resumed == iterations && totals.rounds == iterations;
    if (printf("{\"type\":\"complete\",\"ok\":%s,\"passed\":%s,\"checks\":%u,"
               "\"failures\":%u,\"output_error\":%s,\"requested_per_case\":%u,"
               "\"created\":%u,\"reaped\":%u,\"terminated\":%u,\"woken\":%u,"
               "\"termination_requests\":%u,\"suspended\":%u,\"contexts\":%u,\"resumed\":%u,"
               "\"completed_rounds\":%u,\"cleanup_terminations\":%u,"
               "\"all_owned_threads_reaped\":%s,\"cleanupChecked\":%s}\n",
               ok ? "true" : "false", ok ? "true" : "false", checks, failures,
               output_failed ? "true" : "false", iterations, totals.created, totals.reaped,
               totals.terminated, totals.woken, totals.termination_requests, totals.suspended,
               totals.contexts, totals.resumed, totals.rounds, totals.cleanup_terminations,
               totals.created == totals.reaped ? "true" : "false",
               cleanup_checked ? "true" : "false") < 0)
        output_failed = 1;
    flush_output();
    return ok && !output_failed ? 0 : 1;
}

static void fatal_cleanup(void)
{
    /* Do not release a live worker's handles or return through its stack owner. */
    (void)check("cleanup.unreapable_owned_thread", 0, 1, 0);
    (void)complete(0);
    (void)TerminateProcess(GetCurrentProcess(), CLEANUP_CODE);
    ExitProcess(CLEANUP_CODE);
}

static void note_reaped(worker *w)
{
    if (!w->reaped) {
        w->reaped = 1;
        w->suspended = 0;
        ++totals.reaped;
    }
}

static DWORD WINAPI wait_worker(void *argument)
{
    worker *w = (worker *)argument;
    if (!SetEvent(w->ready)) { w->error = GetLastError(); return 20u; }
    w->wait_result = WaitForSingleObject(w->subject, INFINITE);
    if (w->wait_result == WAIT_FAILED) w->error = GetLastError();
    return w->wait_result == WAIT_OBJECT_0 ? 0u : 21u;
}

static int start_worker(worker *w, HANDLE subject)
{
    if (!check("worker.previous_handles_closed", !w->thread && !w->ready,
               (w->thread || w->ready) ? 1 : 0, 0)) return 0;
    memset(w, 0, sizeof(*w));
    w->subject = subject;
    w->wait_result = WAIT_FAILED;
    w->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!api("worker.ready.CreateEventW.manual_unsignaled", w->ready != NULL)) return 0;
    w->thread = CreateThread(NULL, 0, wait_worker, w, 0, NULL);
    if (w->thread) ++totals.created;
    if (!api("worker.CreateThread", w->thread != NULL)) return 0;
    if (!equal("worker.ready", WaitForSingleObject(w->ready, WAIT_MS), WAIT_OBJECT_0)) return 0;
    /* This deliberately is NOT a handshake into native/kernel registration. */
    Sleep(READY_SETTLE_MS);
    return equal("worker.live_sample", WaitForSingleObject(w->thread, 0), WAIT_TIMEOUT);
}

static int join_worker(worker *w, DWORD expected_code)
{
    DWORD result, code = STILL_ACTIVE;
    result = WaitForSingleObject(w->thread, WAIT_MS);
    /* Reaping remains true even when the subsequent output or exit check fails. */
    if (result == WAIT_OBJECT_0) note_reaped(w);
    if (!equal("worker.bounded_join", result, WAIT_OBJECT_0)) return 0;
    if (!api("worker.GetExitCodeThread", GetExitCodeThread(w->thread, &code))) return 0;
    return equal("worker.exit_code", code, expected_code);
}

static void cleanup_worker(worker *w)
{
    if (w->thread && !w->reaped) {
        DWORD result = WaitForSingleObject(w->thread, 0);
        if (result == WAIT_OBJECT_0) note_reaped(w);
        else {
            BOOL killed;
            if (w->suspended) {
                DWORD previous = ResumeThread(w->thread);
                if (previous != (DWORD)-1) w->suspended = 0;
                (void)equal("cleanup.ResumeThread.previous_count", previous, 1u);
            }
            killed = TerminateThread(w->thread, CLEANUP_CODE);
            if (killed) ++totals.cleanup_terminations;
            (void)api("cleanup.TerminateThread.owned", killed);
            result = WaitForSingleObject(w->thread, WAIT_MS);
            if (result == WAIT_OBJECT_0) note_reaped(w);
            if (!equal("cleanup.thread_reaped", result, WAIT_OBJECT_0) && !w->reaped)
                fatal_cleanup();
        }
    }
    /* No worker can access this context/ready event after confirmed reaping. */
    if (w->thread && !w->reaped) fatal_cleanup();
    close_handle(&w->thread, "cleanup.thread.CloseHandle");
    close_handle(&w->ready, "cleanup.ready.CloseHandle");
}

static int terminated_round(worker *w, HANDLE subject)
{
    CONTEXT context;
    DWORD previous;
    BOOL killed;
    int ok = 0;
    case_name = "suspend_context_resume_terminate_single_event";
    if (!equal("subject.initially_unsignaled", WaitForSingleObject(subject, 0), WAIT_TIMEOUT)) goto done;
    if (!start_worker(w, subject)) goto done;
    previous = SuspendThread(w->thread);
    if (previous != (DWORD)-1) w->suspended = 1;
    if (!equal("worker.SuspendThread.previous_count", previous, 0u)) goto done;
    ++totals.suspended;
    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_CONTROL;
    if (!api("worker.GetThreadContext.CONTEXT_CONTROL", GetThreadContext(w->thread, &context))) goto done;
    ++totals.contexts;
    previous = ResumeThread(w->thread);
    if (previous != (DWORD)-1) w->suspended = 0;
    if (!equal("worker.ResumeThread.previous_count", previous, 1u)) goto done;
    ++totals.resumed;
    if (!equal("worker.live_after_resume", WaitForSingleObject(w->thread, 0), WAIT_TIMEOUT)) goto done;
    killed = TerminateThread(w->thread, TERMINATED_CODE);
    if (killed) ++totals.termination_requests;
    if (!api("worker.TerminateThread.19", killed)) goto done;
    if (!join_worker(w, TERMINATED_CODE)) goto done;
    ++totals.terminated;
    ok = equal("subject.unsignaled_after_termination", WaitForSingleObject(subject, 0), WAIT_TIMEOUT);
done:
    cleanup_worker(w);
    return ok && !failures && !output_failed;
}

static int woken_round(worker *w, HANDLE subject)
{
    int ok = 0;
    case_name = "set_same_manual_event_wakes_fresh_worker";
    if (!equal("subject.unsignaled_before_fresh_worker", WaitForSingleObject(subject, 0), WAIT_TIMEOUT)) goto done;
    if (!start_worker(w, subject)) goto done;
    if (!api("subject.SetEvent", SetEvent(subject))) goto done;
    if (!join_worker(w, 0u)) goto done;
    if (!equal("worker.WaitForSingleObject.result", w->wait_result, WAIT_OBJECT_0)) goto done;
    if (!equal("worker.error", w->error, ERROR_SUCCESS)) goto done;
    ++totals.woken;
    if (!equal("subject.manual_signal_retained", WaitForSingleObject(subject, 0), WAIT_OBJECT_0)) goto done;
    if (!api("subject.ResetEvent", ResetEvent(subject))) goto done;
    ok = equal("subject.unsignaled_after_reset", WaitForSingleObject(subject, 0), WAIT_TIMEOUT);
done:
    cleanup_worker(w);
    return ok && !failures && !output_failed;
}

static int parse_iterations(const char *text, unsigned int *value)
{
    char *end;
    unsigned long parsed;
    if (!text[0] || text[0] < '0' || text[0] > '9') return 0;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno || *end || parsed < 1ul || parsed > (unsigned long)MAX_ITERATIONS) return 0;
    *value = (unsigned int)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    worker w;
    HANDLE subject = NULL;
    int valid, cleanup_checked, all_reaped, handles_closed;
    memset(&w, 0, sizeof(w));
    valid = argc == 1 || (argc == 3 && !strcmp(argv[1], "--iterations") &&
                          parse_iterations(argv[2], &iterations));
    if (printf("{\"type\":\"limits\",\"pointer_bits\":%u,\"iterations\":%u,"
               "\"iterations_per_case\":%u,\"max_iterations\":%u,\"hold_ms\":%u,"
               "\"ready_wait_ms\":%u,\"join_ms\":%u,\"ready_settle_ms\":%u,"
               "\"worker_wait_infinite\":true,\"subject_reused\":true,"
               "\"native_registration_proven\":false,\"external_watchdog_required\":true}\n",
               (unsigned int)(sizeof(void *) * 8u), iterations, iterations, MAX_ITERATIONS,
               HOLD_MS, WAIT_MS, WAIT_MS, READY_SETTLE_MS) < 0)
        output_failed = 1;
    flush_output();
    if (!check("arguments.iterations_1_to_256", valid, valid, 1)) goto cleanup;
    subject = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!api("subject.CreateEventW.manual_unsignaled", subject != NULL)) goto cleanup;
    for (round_number = 1; round_number <= iterations && !failures && !output_failed; ++round_number) {
        if (!terminated_round(&w, subject)) break;
        if (!woken_round(&w, subject)) break;
        ++totals.rounds;
    }
cleanup:
    case_name = "teardown";
    cleanup_worker(&w);
    close_handle(&subject, "cleanup.subject.CloseHandle");
    all_reaped = totals.created == totals.reaped;
    handles_closed = !w.thread && !w.ready && !subject;
    cleanup_checked = all_reaped && handles_closed;
    if (!all_reaped) fatal_cleanup();
    if (printf("{\"type\":\"idle\",\"ok\":%s,\"all_owned_threads_reaped\":true,"
               "\"owned_handles_closed\":%s,\"created\":%u,\"reaped\":%u,\"hold_ms\":%u}\n",
               cleanup_checked && !failures && !output_failed ? "true" : "false",
               handles_closed ? "true" : "false", totals.created, totals.reaped, HOLD_MS) < 0)
        output_failed = 1;
    flush_output();
    Sleep(HOLD_MS);
    return complete(cleanup_checked);
}
