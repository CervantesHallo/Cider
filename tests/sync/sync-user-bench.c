/*
 * Bounded standalone Win32 U-group attribution benchmark; no Wine internals.
 * Build: <target>-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Werror
 *        sync-user-bench.c -o sync-user-bench.exe
 * Usage: sync-user-bench.exe --bench cs|srw|cv|address|once iterations [threads=4] [batches=8]
 * Threads must be 2, 4, or 8. Iterations are PER WORKER PER BATCH, including
 * both peers of each address pair and every producer/consumer. No calibration,
 * subtraction, percentile latency, quiet-host claim, or backend selection.
 * Run the SAME PE under externally selected server/msync bottle settings.
 *
 * Every requested batch has its own fixed warmup, then fixed measured work
 * on those SAME worker threads. State resets between phases while all peers
 * are parked at an atomic gate; thread identities and primitive instances stay.
 * All raw warmup/measurement rows are retained. Worker QPC intervals enclose
 * only the workload loop, including predicates, payload, local accounting,
 * cancellation reads, and (for blocking predicates) deadline clock reads.
 * No event/semaphore/mutex/thread/process wait or signal is in that loop.
 * QPC and GetTickCount64 are clocks, not counted primitive operations.
 *
 * The main thread participates as worker 0. All workers leave the event gate
 * before the atomic start barrier opens; all stop their clocks before the
 * atomic finish barrier opens. Thus gate, thread exit, and join calls do not
 * overlap ANY worker's timed loop. Finishers spin outside their own timing;
 * this may affect unfinished peers and is not subtracted. The watchdog has
 * one independent bounded event wait, not a recurring hot-loop poll.
 *
 * Primary metrics: full worker interval envelope / completed worker iterations
 * and aggregate completed primitive API calls. Envelope is min(begin) to
 * max(end), NOT sum(worker times) and NOT gate-to-join wall time. These are
 * throughput costs, NOT isolated primitive latency or wakeup percentiles.
 * API counts include CV's accompanying CS calls and actual predicate sleeps;
 * address counts include actual WaitOnAddress calls and all single wakes.
 *
 * CS: all workers acquire ONE shared CS (spin count zero) and update a checked sequence. This
 * creates contention opportunity, not proof that every acquisition blocked.
 * SRW: even workers write exclusively; odd workers read shared and validate.
 * CV: one-slot MPMC queue, half producers/half consumers, separate predicates.
 * Address: independent two-thread pingpong pairs, release/acquire turn word;
 * a roundtrip is two worker iterations, not a fixed number of wait API calls.
 * Once: preinitialized before warmup; callback must remain at exactly one.
 *
 * No worker is forcibly terminated and then allowed to return through owned
 * stack/lock storage. Unprovable cleanup immediately terminates THIS process.
 * Complete ok:true AND exit 0 AND successful outer cleanup are required.
 * The 115s watchdog includes warmup, startup, output and cleanup. An external
 * <=120s wall bound remains necessary for OS suspension/watchdog starvation.
 * Source/API decisions and untested limits: out/sync-user-bench-20261024/source-notes.json.
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
#include <errno.h>

#define MAX_THREADS 8u
#define MAX_BATCHES 8u
#define WATCHDOG_MS 115000u
#define PHASE_MS 20000u
#define JOIN_MS 3000u
#define CANCEL_INTERVAL 1024u

enum kind { KIND_CS, KIND_SRW, KIND_CV, KIND_ADDRESS, KIND_ONCE };
typedef BOOL (WINAPI *wait_address_fn)(volatile VOID *, PVOID, SIZE_T, DWORD);
typedef VOID (WINAPI *wake_address_fn)(PVOID);
static wait_address_fn wait_address;
static wake_address_fn wake_address;
static HMODULE address_module;
static LARGE_INTEGER frequency;
static ULONGLONG process_start, process_deadline;
static unsigned checks, failures;
static int output_failed;

struct counts {
    uint64_t completed, acquire_calls, release_calls, wait_calls, wake_calls;
    uint64_t once_calls, timeout_returns, observations, containment_wake_calls;
    DWORD error, stage;
};
struct group;
struct sample {
    struct counts counts;
    LARGE_INTEGER begin, end;
};
struct worker {
    struct group *group;
    unsigned index;
    struct sample samples[2];
};
struct address_pair { volatile LONG turn; uint64_t sequence; };
struct final_state {
    uint64_t sequence, inverse, produced, consumed, value, cs_peer_switches;
    unsigned last_owner;
    int full;
    struct address_pair pairs[MAX_THREADS / 2];
};
struct group {
    enum kind kind;
    unsigned threads;
    uint64_t iterations;
    HANDLE gate;
    ULONGLONG deadline;
    volatile LONG arrived, finished, abort, published, second_go;
    uint64_t phase_iterations[2];
    ULONGLONG phase_deadline[2], phase_end[2];
    LONG phase_abort[2], phase_callbacks[2];
    int measured;
    CRITICAL_SECTION cs;
    SRWLOCK srw;
    CONDITION_VARIABLE full_cv, empty_cv;
    INIT_ONCE once;
    DWORD cookie;
    volatile LONG callback_count;
    struct final_state state, final[2];
    struct worker workers[MAX_THREADS];
};
struct watchdog { HANDLE stop; ULONGLONG deadline; };

/* Never block on stdio or unwind shared objects when cleanup is unprovable. */
static void fatal(UINT code)
{
    TerminateProcess(GetCurrentProcess(), code);
    ExitProcess(code);
}

static void flush_output(void)
{
    if (fflush(stdout) != 0 || ferror(stdout)) output_failed = 1;
}

static int check(const char *name, int ok, uint64_t actual, uint64_t expected)
{
    ++checks;
    if (!ok) ++failures;
    if (printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,"
               "\"actual\":%" PRIu64 ",\"expected\":%" PRIu64 "}\n",
               name, ok ? "true" : "false", actual, expected) < 0) output_failed = 1;
    flush_output();
    return ok;
}

static DWORD remaining(ULONGLONG deadline)
{
    ULONGLONG now = GetTickCount64();
    return now >= deadline ? 0 : (DWORD)(deadline - now);
}

static DWORD WINAPI watchdog_main(void *opaque)
{
    struct watchdog *guard = opaque;
    if (WaitForSingleObject(guard->stop, remaining(guard->deadline)) != WAIT_OBJECT_0)
        fatal(124);
    return 0;
}

static LONG atomic_read(volatile LONG *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void barrier(volatile LONG *counter, unsigned target, ULONGLONG deadline)
{
    unsigned spins = 0;
    while (atomic_read(counter) != (LONG)target) {
        YieldProcessor();
        if ((++spins & 4095u) == 0 && !remaining(deadline)) fatal(122);
    }
}

static void abort_group(struct group *g, struct counts *c)
{
    unsigned i;
    __atomic_store_n(&g->abort, 1, __ATOMIC_RELEASE);
    if (g->kind == KIND_CV) {
        WakeAllConditionVariable(&g->full_cv);
        WakeAllConditionVariable(&g->empty_cv);
        c->wake_calls += 2; c->containment_wake_calls += 2;
    } else if (g->kind == KIND_ADDRESS) {
        for (i = 0; i < g->threads / 2; ++i) {
            /* At most two owned waiters per pair; deadlines also bound races. */
            wake_address((PVOID)&g->state.pairs[i].turn);
            wake_address((PVOID)&g->state.pairs[i].turn);
            c->wake_calls += 2; c->containment_wake_calls += 2;
        }
    }
}

static void worker_error(struct group *g, struct counts *c, DWORD stage, DWORD error)
{
    if (!c->stage) {
        c->stage = stage;
        c->error = error ? error : ERROR_GEN_FAILURE;
    }
    abort_group(g, c);
}

static BOOL CALLBACK initialize_once(PINIT_ONCE once, PVOID opaque, PVOID *context)
{
    struct group *g = opaque;
    (void)once;
    __atomic_add_fetch(&g->callback_count, 1, __ATOMIC_RELAXED);
    *context = &g->cookie; /* DWORD aligned; reserved context bits are zero. */
    return TRUE;
}

static int cancelled(struct group *g, uint64_t iteration)
{
    return iteration % CANCEL_INTERVAL == 0 && atomic_read(&g->abort);
}

static void hot_cs(struct worker *w, struct counts *c)
{
    struct group *g = w->group;
    uint64_t i;
    for (i = 0; i < g->iterations && !cancelled(g, i); ++i) {
        EnterCriticalSection(&g->cs); ++c->acquire_calls;
        if (g->state.inverse != ~g->state.sequence) worker_error(g, c, 10, ERROR_INVALID_DATA);
        if (g->state.last_owner != MAX_THREADS && g->state.last_owner != w->index) ++g->state.cs_peer_switches;
        g->state.last_owner = w->index;
        ++g->state.sequence;
        g->state.inverse = ~g->state.sequence;
        LeaveCriticalSection(&g->cs); ++c->release_calls;
        ++c->completed;
        if (c->stage) break;
    }
}

static void hot_srw(struct worker *w, struct counts *c)
{
    struct group *g = w->group;
    uint64_t i, last = 0;
    for (i = 0; i < g->iterations && !cancelled(g, i); ++i) {
        if ((w->index & 1u) == 0) {
            AcquireSRWLockExclusive(&g->srw); ++c->acquire_calls;
            if (g->state.inverse != ~g->state.sequence) worker_error(g, c, 20, ERROR_INVALID_DATA);
            ++g->state.sequence;
            g->state.inverse = ~g->state.sequence;
            ReleaseSRWLockExclusive(&g->srw); ++c->release_calls;
        } else {
            uint64_t value;
            AcquireSRWLockShared(&g->srw); ++c->acquire_calls;
            value = g->state.sequence;
            if (g->state.inverse != ~value || value < last) worker_error(g, c, 21, ERROR_INVALID_DATA);
            c->observations ^= value;
            last = value;
            ReleaseSRWLockShared(&g->srw); ++c->release_calls;
        }
        ++c->completed;
        if (c->stage) break;
    }
}

static void hot_cv(struct worker *w, struct counts *c)
{
    struct group *g = w->group;
    int producer = (w->index & 1u) == 0;
    uint64_t i;
    for (i = 0; i < g->iterations; ++i) {
        EnterCriticalSection(&g->cs); ++c->acquire_calls;
        while ((producer ? g->state.full : !g->state.full) && !atomic_read(&g->abort)) {
            DWORD left = remaining(g->deadline);
            BOOL ok;
            if (!left) { worker_error(g, c, 30, ERROR_TIMEOUT); break; }
            ++c->wait_calls;
            ok = SleepConditionVariableCS(producer ? &g->empty_cv : &g->full_cv, &g->cs, left);
            if (!ok) {
                DWORD error = GetLastError();
                if (error == ERROR_TIMEOUT) {
                    ++c->timeout_returns;
                    if (!remaining(g->deadline)) worker_error(g, c, 31, ERROR_TIMEOUT);
                } else worker_error(g, c, 32, error);
            }
            /* Predicate is rechecked after success, timeout, or spurious wake.
             * The absolute PHASE deadline is NEVER renewed by a wake. */
        }
        if (atomic_read(&g->abort)) {
            LeaveCriticalSection(&g->cs); ++c->release_calls;
            break;
        }
        if (producer) {
            g->state.value = ++g->state.produced;
            g->state.full = 1;
        } else {
            if (g->state.value != g->state.consumed + 1) worker_error(g, c, 33, ERROR_INVALID_DATA);
            ++g->state.consumed;
            g->state.full = 0;
        }
        WakeConditionVariable(producer ? &g->full_cv : &g->empty_cv); ++c->wake_calls;
        LeaveCriticalSection(&g->cs); ++c->release_calls;
        ++c->completed;
        if (c->stage) break;
    }
}

static void hot_address(struct worker *w, struct counts *c)
{
    struct group *g = w->group;
    struct address_pair *pair = &g->state.pairs[w->index / 2];
    LONG role = (LONG)(w->index & 1u);
    uint64_t i;
    for (i = 0; i < g->iterations; ++i) {
        LONG observed;
        while ((observed = atomic_read(&pair->turn)) != role && !atomic_read(&g->abort)) {
            DWORD left = remaining(g->deadline);
            if (!left) { worker_error(g, c, 40, ERROR_TIMEOUT); break; }
            ++c->wait_calls;
            if (!wait_address(&pair->turn, &observed, sizeof(observed), left)) {
                DWORD error = GetLastError();
                if (error == ERROR_TIMEOUT) {
                    ++c->timeout_returns;
                    if (!remaining(g->deadline)) worker_error(g, c, 41, ERROR_TIMEOUT);
                } else worker_error(g, c, 42, error);
            }
        }
        if (atomic_read(&g->abort)) break;
        if (pair->sequence != i * 2 + (uint64_t)role) {
            worker_error(g, c, 43, ERROR_INVALID_DATA); break;
        }
        ++pair->sequence;
        __atomic_store_n(&pair->turn, 1 - role, __ATOMIC_RELEASE);
        wake_address((PVOID)&pair->turn); ++c->wake_calls;
        ++c->completed;
    }
}

static void hot_once(struct worker *w, struct counts *c)
{
    struct group *g = w->group;
    uint64_t i;
    for (i = 0; i < g->iterations && !cancelled(g, i); ++i) {
        PVOID context = NULL;
        ++c->once_calls;
        if (!InitOnceExecuteOnce(&g->once, initialize_once, g, &context)) {
            worker_error(g, c, 50, GetLastError()); break;
        }
        if (context != &g->cookie) { worker_error(g, c, 51, ERROR_INVALID_DATA); break; }
        ++c->completed;
    }
}

static void reset_work(struct group *g)
{
    g->arrived = g->finished = g->abort = 0;
    memset(&g->state, 0, sizeof(g->state));
    g->state.inverse = ~UINT64_C(0);
    g->state.last_owner = MAX_THREADS;
}

static ULONGLONG new_phase_deadline(void)
{
    ULONGLONG deadline = GetTickCount64() + PHASE_MS;
    return deadline < process_deadline - JOIN_MS ? deadline : process_deadline - JOIN_MS;
}

static void worker_interval(struct worker *w, unsigned phase)
{
    struct group *g = w->group;
    struct counts c = {0};
    LARGE_INTEGER begin = {0}, end = {0};
    __atomic_add_fetch(&g->arrived, 1, __ATOMIC_ACQ_REL);
    barrier(&g->arrived, g->threads, g->deadline);
    if (!QueryPerformanceCounter(&begin)) worker_error(g, &c, 1, GetLastError());
    else {
        switch (g->kind) {
        case KIND_CS: hot_cs(w, &c); break;
        case KIND_SRW: hot_srw(w, &c); break;
        case KIND_CV: hot_cv(w, &c); break;
        case KIND_ADDRESS: hot_address(w, &c); break;
        case KIND_ONCE: hot_once(w, &c); break;
        }
    }
    if (!QueryPerformanceCounter(&end) || end.QuadPart <= begin.QuadPart)
        worker_error(g, &c, 2, ERROR_INVALID_DATA);
    /* No return/ExitThread/handle signal until every QPC interval has ended. */
    __atomic_add_fetch(&g->finished, 1, __ATOMIC_ACQ_REL);
    barrier(&g->finished, g->threads, g->deadline);
    w->samples[phase].begin = begin; w->samples[phase].end = end;
    w->samples[phase].counts = c;
}

static void capture_phase(struct group *g, unsigned phase)
{
    g->final[phase] = g->state;
    g->phase_deadline[phase] = g->deadline;
    g->phase_end[phase] = GetTickCount64();
    g->phase_abort[phase] = atomic_read(&g->abort);
    g->phase_callbacks[phase] = atomic_read(&g->callback_count);
}

static DWORD WINAPI worker_main(void *opaque)
{
    struct worker *w = opaque;
    struct group *g = w->group;
    unsigned spins = 0;
    LONG go;
    if (WaitForSingleObject(g->gate, remaining(g->deadline)) != WAIT_OBJECT_0) fatal(122);
    worker_interval(w, 0);
    __atomic_add_fetch(&g->published, 1, __ATOMIC_ACQ_REL);
    if (w->index == 0) {
        /* published proves every worker has LEFT the warmup finish barrier.
         * Resetting its counter earlier could strand a late barrier reader. */
        barrier(&g->published, g->threads, g->deadline);
        capture_phase(g, 0);
        if (!g->phase_abort[0] && g->phase_end[0] < g->deadline) {
            reset_work(g);
            g->iterations = g->phase_iterations[1];
            g->deadline = new_phase_deadline();
            g->measured = remaining(g->deadline) != 0;
        }
        __atomic_store_n(&g->second_go, g->measured ? 1 : -1, __ATOMIC_RELEASE);
    }
    while ((go = atomic_read(&g->second_go)) == 0) {
        YieldProcessor();
        if ((++spins & 4095u) == 0 && !remaining(process_deadline - JOIN_MS)) fatal(122);
    }
    if (go < 0) return 1;
    worker_interval(w, 1);
    if (w->index == 0) capture_phase(g, 1);
    return w->samples[0].counts.stage || w->samples[1].counts.stage ? 1 : 0;
}

static uint64_t calls(const struct counts *c)
{
    return c->acquire_calls + c->release_calls + c->wait_calls + c->wake_calls + c->once_calls;
}

static double nanoseconds(LONGLONG ticks)
{
    return (double)ticks * 1000000000.0 / (double)frequency.QuadPart;
}

static const char *role_name(enum kind kind, unsigned index)
{
    if (kind == KIND_SRW) return index & 1u ? "reader_shared" : "writer_exclusive";
    if (kind == KIND_CV) return index & 1u ? "consumer" : "producer";
    if (kind == KIND_ADDRESS) return index & 1u ? "pong" : "ping";
    return kind == KIND_CS ? "shared_cs" : "initialized_once";
}

static int report_phase(struct group *g, unsigned batch, int measured, int valid,
                        LONGLONG *total_ticks, uint64_t *total_units, uint64_t *total_calls)
{
    unsigned i;
    const struct final_state *s = &g->final[measured];
    LONGLONG first = g->workers[0].samples[measured].begin.QuadPart;
    LONGLONG last = g->workers[0].samples[measured].end.QuadPart, sum = 0;
    uint64_t units = 0, api_calls = 0, waits = 0, wakes = 0, timeouts = 0, transfers = 0;
    for (i = 0; i < g->threads; ++i) {
        const struct sample *w = &g->workers[i].samples[measured];
        if (w->begin.QuadPart < first) first = w->begin.QuadPart;
        if (w->end.QuadPart > last) last = w->end.QuadPart;
        sum += w->end.QuadPart - w->begin.QuadPart;
        units += w->counts.completed; api_calls += calls(&w->counts);
        waits += w->counts.wait_calls; wakes += w->counts.wake_calls;
        timeouts += w->counts.timeout_returns;
    }
    if (g->kind == KIND_CV) transfers = s->consumed;
    else if (g->kind == KIND_ADDRESS)
        for (i = 0; i < g->threads / 2; ++i) transfers += s->pairs[i].sequence / 2;
    if (g->kind == KIND_ADDRESS && !check("address.wait_api_exercised", waits > 0, waits, 1)) valid = 0;
    if (printf("{\"type\":\"batch\",\"batch\":%u,\"phase\":\"%s\",\"ok\":%s,"
               "\"threads\":%u,\"iterations_per_worker\":%" PRIu64 ","
               "\"qpc_frequency\":%" PRId64 ",\"qpc_begin_min\":%" PRId64 ",\"qpc_end_max\":%" PRId64 ","
               "\"qpc_envelope_ticks\":%" PRId64 ",\"qpc_worker_ticks_sum\":%" PRId64 ","
               "\"phase_deadline_tick_ms\":%" PRIu64 ",\"phase_completion_tick_ms\":%" PRIu64 ","
               "\"completed_worker_iterations\":%" PRIu64 ",\"primitive_api_calls\":%" PRIu64 ","
               "\"predicate_wait_calls\":%" PRIu64 ",\"wake_api_calls\":%" PRIu64 ","
               "\"timeout_returns\":%" PRIu64 ",\"envelope_ns\":%.3f,"
               "\"timing_scope\":\"worker_loop_envelope_excludes_gates_and_joins\","
               "\"ns_per_worker_iteration\":",
               batch, measured ? "measurement" : "warmup", valid ? "true" : "false", g->threads,
               g->phase_iterations[measured], (int64_t)frequency.QuadPart, (int64_t)first, (int64_t)last,
               (int64_t)(last - first), (int64_t)sum,
               (uint64_t)g->phase_deadline[measured], (uint64_t)g->phase_end[measured],
               units, api_calls, waits, wakes, timeouts,
               nanoseconds(last - first)) < 0) output_failed = 1;
    if (units && last > first) { if (printf("%.9f", nanoseconds(last - first) / (double)units) < 0) output_failed = 1; }
    else if (printf("null") < 0) output_failed = 1;
    if (printf(",\"ns_per_primitive_api_call\":") < 0) output_failed = 1;
    if (api_calls && last > first) { if (printf("%.9f", nanoseconds(last - first) / (double)api_calls) < 0) output_failed = 1; }
    else if (printf("null") < 0) output_failed = 1;
    if (printf(",\"worker_iterations_per_second\":") < 0) output_failed = 1;
    if (last > first) { if (printf("%.3f", (double)units * (double)frequency.QuadPart / (double)(last - first)) < 0) output_failed = 1; }
    else if (printf("null") < 0) output_failed = 1;
    if (printf(",\"primitive_api_calls_per_second\":") < 0) output_failed = 1;
    if (last > first) { if (printf("%.3f", (double)api_calls * (double)frequency.QuadPart / (double)(last - first)) < 0) output_failed = 1; }
    else if (printf("null") < 0) output_failed = 1;
    if (printf(",\"transfer_or_roundtrip_count\":%" PRIu64 ",\"produced\":%" PRIu64 ",\"consumed\":%" PRIu64 ","
               "\"slot_full\":%d,\"protected_sequence\":%" PRIu64 ",\"cs_peer_switches\":%" PRIu64 ","
               "\"once_callback_count\":%ld,\"address_pairs\":[",
               transfers, s->produced, s->consumed, s->full, s->sequence,
               s->cs_peer_switches, (long)g->phase_callbacks[measured]) < 0) output_failed = 1;
    if (g->kind == KIND_ADDRESS) for (i = 0; i < g->threads / 2; ++i) {
        if (printf("%s{\"index\":%u,\"sequence\":%" PRIu64 ",\"turn\":%ld}", i ? "," : "", i,
                   s->pairs[i].sequence, (long)s->pairs[i].turn) < 0) output_failed = 1;
    }
    if (printf("],\"workers\":[") < 0) output_failed = 1;
    for (i = 0; i < g->threads; ++i) {
        const struct sample *w = &g->workers[i].samples[measured];
        const struct counts *c = &w->counts;
        if (printf("%s{\"index\":%u,\"role\":\"%s\",\"pair_index\":%u,"
                   "\"completed\":%" PRIu64 ",\"acquire_calls\":%" PRIu64 ",\"release_calls\":%" PRIu64 ","
                   "\"wait_calls\":%" PRIu64 ",\"wake_calls\":%" PRIu64 ",\"once_calls\":%" PRIu64 ","
                   "\"timeout_returns\":%" PRIu64 ",\"containment_wake_calls\":%" PRIu64 ",\"observation_xor\":%" PRIu64 ","
                   "\"qpc_begin\":%" PRId64 ",\"qpc_end\":%" PRId64 ",\"qpc_ticks\":%" PRId64 ","
                   "\"error_stage\":%lu,\"win32_error\":%lu}",
                   i ? "," : "", i, role_name(g->kind, i), i / 2, c->completed,
                   c->acquire_calls, c->release_calls, c->wait_calls, c->wake_calls, c->once_calls,
                   c->timeout_returns, c->containment_wake_calls, c->observations, (int64_t)w->begin.QuadPart,
                   (int64_t)w->end.QuadPart, (int64_t)(w->end.QuadPart - w->begin.QuadPart),
                   (unsigned long)c->stage, (unsigned long)c->error) < 0) output_failed = 1;
    }
    if (printf("]}\n") < 0) output_failed = 1;
    flush_output();
    if (measured) { *total_ticks += last - first; *total_units += units; *total_calls += api_calls; }
    return valid && !output_failed;
}

static int validate_phase(struct group *g, unsigned batch, int measured,
                          LONGLONG *ticks, uint64_t *units, uint64_t *api_calls)
{
    const struct final_state *s = &g->final[measured];
    uint64_t iterations = g->phase_iterations[measured];
    unsigned i;
    int valid = 1;
    if (!check("phase.absolute_deadline", g->phase_end[measured] < g->phase_deadline[measured],
               g->phase_end[measured], g->phase_deadline[measured])) valid = 0;
    for (i = 0; i < g->threads; ++i) {
        const struct counts *c = &g->workers[i].samples[measured].counts;
        if (!check("worker.error_stage", c->stage == 0, c->stage, 0)) valid = 0;
        if (!check("worker.completed", c->completed == iterations, c->completed, iterations)) valid = 0;
    }
    if (!check("phase.not_aborted", !g->phase_abort[measured], (uint64_t)g->phase_abort[measured], 0)) valid = 0;
    if (g->kind == KIND_CS || g->kind == KIND_SRW) {
        uint64_t expected = iterations * (g->kind == KIND_CS ? g->threads : g->threads / 2);
        if (!check("locks.final_sequence", s->sequence == expected, s->sequence, expected)) valid = 0;
        if (!check("locks.final_checkword", s->inverse == ~s->sequence, s->inverse, ~s->sequence)) valid = 0;
    } else if (g->kind == KIND_CV) {
        uint64_t expected = iterations * (g->threads / 2);
        if (!check("cv.produced", s->produced == expected, s->produced, expected)) valid = 0;
        if (!check("cv.consumed", s->consumed == expected, s->consumed, expected)) valid = 0;
        if (!check("cv.empty_after_join", !s->full, (uint64_t)s->full, 0)) valid = 0;
    } else if (g->kind == KIND_ADDRESS) {
        for (i = 0; i < g->threads / 2; ++i) {
            if (!check("address.final_turn", !s->pairs[i].turn, (uint64_t)s->pairs[i].turn, 0)) valid = 0;
            if (!check("address.final_sequence", s->pairs[i].sequence == iterations * 2, s->pairs[i].sequence, iterations * 2)) valid = 0;
        }
    } else if (!check("once.callback_exactly_once", g->phase_callbacks[measured] == 1,
                      (uint64_t)g->phase_callbacks[measured], 1)) valid = 0;
    return report_phase(g, batch, measured, valid, ticks, units, api_calls);
}

static int run_batch(struct group *g, unsigned batch, uint64_t warmup, uint64_t iterations,
                     LONGLONG *ticks, uint64_t *units, uint64_t *api_calls)
{
    HANDLE handles[MAX_THREADS - 1] = {0};
    unsigned i;
    int valid = 1, warmup_ok, measurement_ok;
    reset_work(g);
    g->iterations = warmup;
    g->phase_iterations[0] = warmup; g->phase_iterations[1] = iterations;
    g->deadline = new_phase_deadline();
    if (!remaining(g->deadline)) return check("phase.time_budget_available", 0, 0, 1);
    g->gate = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g->gate) return check("phase.create_gate", 0, GetLastError(), 0);
    for (i = 0; i < g->threads; ++i) { g->workers[i].group = g; g->workers[i].index = i; }
    for (i = 1; i < g->threads; ++i) {
        handles[i - 1] = CreateThread(NULL, 0, worker_main, &g->workers[i], 0, NULL);
        if (!handles[i - 1]) fatal(122); /* Partial launch cannot unwind owned storage. */
    }
    if (!SetEvent(g->gate)) fatal(122);
    (void)worker_main(&g->workers[0]);
    /* Both phases ended before any of these N waits or thread exits. */
    if (WaitForMultipleObjects(g->threads - 1, handles, TRUE, JOIN_MS) != WAIT_OBJECT_0) fatal(122);
    for (i = 1; i < g->threads; ++i) {
        DWORD code;
        if (!GetExitCodeThread(handles[i - 1], &code)) fatal(122);
        if (!check("worker.exit", code == 0, code, 0)) valid = 0;
        if (!CloseHandle(handles[i - 1])) fatal(122);
    }
    if (!CloseHandle(g->gate)) fatal(122);
    g->gate = NULL;
    warmup_ok = validate_phase(g, batch, 0, ticks, units, api_calls);
    measurement_ok = g->measured ? validate_phase(g, batch, 1, ticks, units, api_calls)
                                 : check("phase.measurement_entered", 0, 0, 1);
    return valid && warmup_ok && measurement_ok;
}

static int resolve_address(void)
{
    FARPROC wait_proc, wake_proc;
    /* Documented API-set, then ordinary exports for Wine/older loader layouts.
     * Both functions always come from one module; hold its ref through joins. */
    address_module = LoadLibraryW(L"api-ms-win-core-synch-l1-2-0.dll");
    wait_proc = address_module ? GetProcAddress(address_module, "WaitOnAddress") : NULL;
    wake_proc = address_module ? GetProcAddress(address_module, "WakeByAddressSingle") : NULL;
    if (!wait_proc || !wake_proc) {
        if (address_module && !FreeLibrary(address_module)) fatal(122);
        address_module = LoadLibraryW(L"kernelbase.dll");
        wait_proc = address_module ? GetProcAddress(address_module, "WaitOnAddress") : NULL;
        wake_proc = address_module ? GetProcAddress(address_module, "WakeByAddressSingle") : NULL;
    }
    if (!check("address.apis_available_no_skip", wait_proc && wake_proc,
               wait_proc != NULL && wake_proc != NULL, 1)) return 0;
    if (sizeof(wait_proc) != sizeof(wait_address) || sizeof(wake_proc) != sizeof(wake_address))
        return check("address.function_pointer_size", 0, sizeof(wait_proc), sizeof(wait_address));
    memcpy(&wait_address, &wait_proc, sizeof(wait_address));
    memcpy(&wake_address, &wake_proc, sizeof(wake_address));
    return 1;
}

static int parse_number(const char *text, uint64_t maximum, uint64_t *out)
{
    char *end;
    unsigned long long value;
    const char *p;
    if (!*text) return 0;
    for (p = text; *p; ++p) if (*p < '0' || *p > '9') return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno || *end || !value || value > maximum) return 0;
    *out = (uint64_t)value;
    return 1;
}

int main(int argc, char **argv)
{
    static const char *names[] = {"cs", "srw", "cv", "address", "once"};
    static const uint64_t maximum[] = {4000000, 4000000, 100000, 200000, 64000000};
    static const uint64_t warmups[] = {32768, 32768, 512, 1024, 262144};
    struct watchdog guard;
    HANDLE watchdog;
    unsigned kind, batch, completed_batches = 0;
    uint64_t iterations = 0, threads = 4, batches = 8, warmup;
    LONGLONG total_ticks = 0;
    uint64_t total_units = 0, total_calls = 0;
    int ok = 1;
    process_start = GetTickCount64();
    process_deadline = process_start + WATCHDOG_MS;
    guard.deadline = process_deadline;
    guard.stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!guard.stop) fatal(122);
    watchdog = CreateThread(NULL, 0, watchdog_main, &guard, 0, NULL);
    if (!watchdog) fatal(122);
    for (kind = 0; kind < sizeof(names) / sizeof(names[0]); ++kind)
        if (argc >= 3 && !strcmp(argv[2], names[kind])) break;
    if (argc < 4 || argc > 6 || strcmp(argv[1], "--bench") || kind >= sizeof(names) / sizeof(names[0]) ||
        !parse_number(argv[3], maximum[kind], &iterations) ||
        (argc >= 5 && !parse_number(argv[4], MAX_THREADS, &threads)) ||
        (threads != 2 && threads != 4 && threads != 8) ||
        (argc == 6 && !parse_number(argv[5], MAX_BATCHES, &batches))) {
        check("arguments.use_bench_cs_srw_cv_address_once_iterations_threads_2_4_8_batches_1_8", 0, (uint64_t)argc, 0);
        ok = 0;
        goto finish;
    }
    warmup = iterations < warmups[kind] ? iterations : warmups[kind];
    {
        BOOL queried = QueryPerformanceFrequency(&frequency);
        if (!check("QPC.frequency", queried && frequency.QuadPart > 0,
                   (uint64_t)frequency.QuadPart, 1)) { ok = 0; goto finish; }
    }
    if (printf("{\"type\":\"begin\",\"schema\":1,\"benchmark\":\"sync_user\",\"mode\":\"%s\","
               "\"pointer_bits\":%u,\"pid\":%lu,\"threads\":%" PRIu64 ","
               "\"batches_requested\":%" PRIu64 ",\"iterations_per_worker_per_batch\":%" PRIu64 ","
               "\"warmup_iterations_per_worker_per_batch\":%" PRIu64 ",\"qpc_frequency\":%" PRId64 ","
               "\"process_watchdog_ms\":%u,\"phase_absolute_limit_ms\":%u,\"join_limit_ms\":%u,"
               "\"cancellation_read_interval_cs_srw_once\":%u,\"backend\":\"external_unverified\","
               "\"timing_scope\":\"raw_worker_loop_envelope\",\"subtraction\":false,"
               "\"worker_zero_is_main_thread\":true,\"warmup_reuses_measured_workers\":true,"
               "\"latency_percentiles\":false}\n",
               names[kind], (unsigned)(sizeof(void *) * 8), (unsigned long)GetCurrentProcessId(), threads,
               batches, iterations, warmup, (int64_t)frequency.QuadPart, WATCHDOG_MS, PHASE_MS, JOIN_MS,
               CANCEL_INTERVAL) < 0) output_failed = 1;
    flush_output();
    if (kind == KIND_ADDRESS && !resolve_address()) { ok = 0; goto finish; }
    for (batch = 0; batch < batches && ok && !output_failed; ++batch) {
        struct group g;
        int cs_initialized = 0;
        memset(&g, 0, sizeof(g));
        g.kind = (enum kind)kind; g.threads = (unsigned)threads;
        if (kind == KIND_CS || kind == KIND_CV) {
            BOOL initialized = InitializeCriticalSectionAndSpinCount(&g.cs, 0);
            DWORD error = initialized ? 0 : GetLastError();
            if (!check("setup.initialize_cs", initialized, error, 0)) { ok = 0; break; }
            cs_initialized = 1;
        }
        InitializeSRWLock(&g.srw);
        InitializeConditionVariable(&g.full_cv); InitializeConditionVariable(&g.empty_cv);
        InitOnceInitialize(&g.once);
        if (kind == KIND_ONCE) {
            PVOID context = NULL;
            BOOL initialized = InitOnceExecuteOnce(&g.once, initialize_once, &g, &context);
            if (!check("setup.once_initialized_before_warmup", initialized && context == &g.cookie &&
                       atomic_read(&g.callback_count) == 1, (uint64_t)atomic_read(&g.callback_count), 1)) ok = 0;
        }
        if (ok) ok = run_batch(&g, batch, warmup, iterations, &total_ticks, &total_units, &total_calls);
        /* run_batch joined every owned worker, or terminated us. */
        if (cs_initialized) DeleteCriticalSection(&g.cs);
        if (ok) ++completed_batches;
    }
    if (printf("{\"type\":\"summary\",\"mode\":\"%s\",\"ok\":%s,\"batches_requested\":%" PRIu64 ","
               "\"batches_completed\":%u,\"all_measurement_rows_retained\":true,"
               "\"qpc_frequency\":%" PRId64 ",\"measurement_envelope_ticks_sum\":%" PRId64 ","
               "\"completed_worker_iterations\":%" PRIu64 ",\"primitive_api_calls\":%" PRIu64 ","
               "\"sum_of_measurement_envelopes_ns\":%.3f,\"ns_per_worker_iteration\":",
               names[kind], ok && !failures && !output_failed ? "true" : "false", batches,
               completed_batches, (int64_t)frequency.QuadPart, (int64_t)total_ticks,
               total_units, total_calls, nanoseconds(total_ticks)) < 0) output_failed = 1;
    if (total_units) { if (printf("%.9f", nanoseconds(total_ticks) / (double)total_units) < 0) output_failed = 1; }
    else if (printf("null") < 0) output_failed = 1;
    if (printf(",\"ns_per_primitive_api_call\":") < 0) output_failed = 1;
    if (total_calls) { if (printf("%.9f", nanoseconds(total_ticks) / (double)total_calls) < 0) output_failed = 1; }
    else if (printf("null") < 0) output_failed = 1;
    if (printf("}\n") < 0) output_failed = 1;
finish:
    if (address_module && !FreeLibrary(address_module)) { ok = 0; ++failures; }
    flush_output();
    ok = ok && !failures && !output_failed;
    if (printf("{\"type\":\"complete\",\"ok\":%s,\"checks\":%u,\"failures\":%u,"
               "\"batches_completed\":%u,\"process_elapsed_ms_through_reporting\":%" PRIu64 ","
               "\"worker_cleanup\":\"joined\",\"output_error\":%s}\n",
               ok ? "true" : "false", checks, failures, completed_batches,
               (uint64_t)(GetTickCount64() - process_start), output_failed ? "true" : "false") < 0) output_failed = 1;
    flush_output();
    /* Keep watchdog active through output flush; then join it before guard dies. */
    if (!SetEvent(guard.stop) || WaitForSingleObject(watchdog, JOIN_MS) != WAIT_OBJECT_0) fatal(122);
    if (!CloseHandle(watchdog) || !CloseHandle(guard.stop)) fatal(122);
    return ok && !output_failed ? 0 : 1;
}
