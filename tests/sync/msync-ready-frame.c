/*
 * Native EXACT-FRAME helper WITH EXPLICIT STUBS for optional candidate0120.
 * Not Wine execution, Windows/API equivalence, or signal-handler validation.
 *
 * Run extract-ready-source.py --part frame --source <applied-sync.c> before EACH build.
 * It extracts struct inproc_frame through just before inproc_thread_cleanup
 * from the explicitly selected sync.c. No frame function is modeled here.
 * Compile as C11 with pthreads; do not define NDEBUG. See the prepare script.
 * The caller MUST create a new process session and impose an external deadline.
 * This process additionally owns a 25-second alarm and bounded handshakes.
 *
 * The parent owns every worker, argument/frame/object record, and private pipe.
 * It joins the exact pthread_t before closing the pipe or releasing its stack.
 * A failed handshake/join terminates the process, never returns past a live
 * worker's storage. No detached threads, shared pipes, Wine, or network access.
 */
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef NDEBUG
#error "The exact production frame assertions must remain enabled"
#endif

#define MAXIMUM_WAIT_OBJECTS 64
#define HANDSHAKE_NS UINT64_C(2000000000)
typedef int32_t LONG; /* Wine LONG is 32 bits on both native architectures. */
_Static_assert(sizeof(LONG) * CHAR_BIT == 32, "LONG must be exactly 32 bits");

struct worker;
struct inproc_frame;

/* Explicit replacement for Wine's native thread_data, NOT a Wine layout. */
struct thread_data
{
    struct inproc_frame *msync_frames;
    unsigned int msync_private_frames;
    sig_atomic_t msync_control_depth;
    struct worker *owner;
};

/* Only the refcount and saved close descriptor are relevant to the stub. */
struct inproc_sync
{
    LONG refcount; /* Accessed exclusively with production-style __atomic ops. */
    int fd;
};

static _Thread_local struct thread_data *fixture_thread_data;

static struct thread_data *get_thread_data(void)
{
    assert(fixture_thread_data);
    return fixture_thread_data;
}

/* Wine Interlocked return values and full sequential consistency, not LP64 long. */
static inline LONG InterlockedCompareExchange(LONG *dest, LONG value, LONG comparand)
{
    __atomic_compare_exchange_n(dest, &comparand, value, 0,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}

static inline LONG InterlockedDecrement(LONG *dest)
{
    return __atomic_sub_fetch(dest, 1, __ATOMIC_SEQ_CST);
}

static void msync_control_begin_data(struct thread_data *data);
static void msync_control_end_data(struct thread_data *data);
static void release_inproc_sync(struct inproc_sync *sync);

/* The ONLY definitions of all six tested frame functions come from sync.c. */
#ifndef FRAME_FIXTURE_EXTRACTED_HEADER
#define FRAME_FIXTURE_EXTRACTED_HEADER "frame-extracted.h"
#endif
#include FRAME_FIXTURE_EXTRACTED_HEADER
#if FRAME_FIXTURE_EXTRACT_VERSION != 1
#error "Prepare the current exact frame header before building"
#endif

enum scenario
{
    READY_NONTERMINAL, READY_TERMINAL, SAME_FRAME_PROMOTE,
    BEFORE_KERNEL, ORIGINAL_ENTRY, NESTED_FRAMES, READY_NONTERMINAL_PENDING, SCENARIO_COUNT
};

enum event
{
    ARMED = 1u << 0,
    CLOSE_ENTER = 1u << 1, CLOSE_RETURN = 1u << 2,
    SLOW_ENTER = 1u << 3, SLOW_RETURN = 1u << 4,
    KERNEL_ENTER = 1u << 5, KERNEL_RETURN = 1u << 6,
    AFTER_LEAVE = 1u << 7, NORMAL_RETURN = 1u << 8,
    CLEANUP_DONE = 1u << 9, DELIVERY_ENTER = 1u << 10
};

struct cancel_policy { int state, type; };

struct cleanup_record
{
    unsigned int private_frames, linked_frames, pins, unknown_frame;
    unsigned int frame_counts[2];
    sig_atomic_t depth;
    LONG refs[2];
    int ref_obligations;
    unsigned int begins, ends, replays, closes, close_returns;
    unsigned int events;
};

struct worker
{
    unsigned int id;
    enum scenario scenario;
    int incoming_state, incoming_type, expect_cancel, expect_close;
    int pipefd[2];
    struct thread_data data;
    struct inproc_frame frames[2];
    struct inproc_sync syncs[2];
    LONG expected_refs[2]; /* Surviving EXTERNAL refs are not frame obligations. */
    atomic_uint events, go, cancel_sent, checks, failures, first_failure;
    atomic_uint cleanup_calls, release_calls, close_calls, close_returns;
    atomic_uint begins, ends, replays;
    const char *first_expression; /* Written by worker; read only after join. */
    struct cancel_policy entry_policy, close_policy, slow_policy, kernel_policy;
    struct cancel_policy exit_policy;
    struct cleanup_record cleanup;
    int normal_token;
};

static const char *const names[SCENARIO_COUNT] = {
    "ready-nonterminal", "ready-terminal", "same-frame-promote",
    "before-kernel", "original-entry", "nested-frames", "ready-nonterminal-pending"
};

static void fatal(const char *message, int status)
{
    if (fprintf(stderr, "frame-fixture: %s\n", message) < 0 ||
        fflush(stderr) == EOF || ferror(stderr)) _exit(125);
    _exit(status); /* Never unwind storage while an owned worker may be live. */
}

static void alarm_expired(int signal_number)
{
    static const char message[] = "frame-fixture: own 25-second alarm expired\n";
    (void)signal_number;
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(124);
}

static uint64_t now_ns(void)
{
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) fatal("clock_gettime failed", 2);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}

static void checked_output(void)
{
    if (fflush(stdout) == EOF || ferror(stdout)) fatal("stdout flush/ferror", 2);
}

static void expect_impl(struct worker *w, int condition, unsigned int line,
                        const char *expression)
{
    unsigned int empty = 0;
    atomic_fetch_add_explicit(&w->checks, 1, memory_order_relaxed);
    if (condition) return;
    atomic_fetch_add_explicit(&w->failures, 1, memory_order_relaxed);
    if (atomic_compare_exchange_strong_explicit(&w->first_failure, &empty, line,
                                               memory_order_relaxed, memory_order_relaxed))
        w->first_expression = expression;
}
#define EXPECT(w, condition) expect_impl((w), !!(condition), __LINE__, #condition)

static unsigned int events(struct worker *w)
{
    return atomic_load_explicit(&w->events, memory_order_acquire);
}

static void publish(struct worker *w, unsigned int bits)
{
    atomic_fetch_or_explicit(&w->events, bits, memory_order_release);
}

/* No CP in gate polling: sched_yield and clock_gettime are not POSIX CPs. */
static void await_go(struct worker *w)
{
    uint64_t deadline = now_ns() + HANDSHAKE_NS;
    while (!atomic_load_explicit(&w->go, memory_order_acquire))
    {
        if (now_ns() >= deadline) fatal("worker gate deadline", 124);
        sched_yield();
    }
    EXPECT(w, atomic_load_explicit(&w->cancel_sent, memory_order_acquire)
               == (unsigned int)w->expect_cancel);
}

/* Parent stops expecting further stages after an early cancellation cleanup. */
static int await_event(struct worker *w, unsigned int bit)
{
    uint64_t deadline = now_ns() + HANDSHAKE_NS;
    for (;;)
    {
        unsigned int seen = events(w);
        if (seen & bit) return 1;
        if (seen & CLEANUP_DONE) return 0;
        if (now_ns() >= deadline) fatal("parent handshake deadline", 124);
        sched_yield();
    }
}

/*
 * State/type have no POSIX getter. This readback temporarily disables, records
 * the old settings, then RESTORES BOTH before any test read. A missing mask
 * therefore still exposes the pending cancellation at that read (or restore).
 * It never leaves the worker artificially protected across a tested CP.
 */
static struct cancel_policy read_policy(void)
{
    struct cancel_policy old;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old.state) ||
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old.type) ||
        pthread_setcanceltype(old.type, NULL) ||
        pthread_setcancelstate(old.state, NULL)) fatal("cancellation policy readback", 2);
    return old;
}

static sig_atomic_t depth(struct thread_data *data)
{
    return __atomic_load_n(&data->msync_control_depth, __ATOMIC_RELAXED);
}

/*
 * EXPLICIT Wine-control doubles: same single-executor depth load/store and
 * compiler-fence ordering as production; replay is counted, no signals sent.
 * These doubles neither mask cancellation nor repair leaked control depth.
 */
static void msync_control_begin_data(struct thread_data *data)
{
    unsigned int old = (unsigned int)depth(data);
    EXPECT(data->owner, old < 2);
    __atomic_store_n(&data->msync_control_depth, (sig_atomic_t)(old + 1), __ATOMIC_RELAXED);
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    atomic_fetch_add_explicit(&data->owner->begins, 1, memory_order_relaxed);
}

static void msync_control_end_data(struct thread_data *data)
{
    unsigned int current;
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    EXPECT(data->owner, depth(data) > 0);
    current = (unsigned int)depth(data) - 1;
    __atomic_store_n(&data->msync_control_depth, (sig_atomic_t)current, __ATOMIC_RELAXED);
    atomic_fetch_add_explicit(&data->owner->ends, 1, memory_order_relaxed);
    if (!current)
    {
        EXPECT(data->owner, !data->msync_frames && !data->msync_private_frames);
        atomic_fetch_add_explicit(&data->owner->replays, 1, memory_order_relaxed);
    }
}

static LONG refs(struct inproc_sync *sync)
{
    return __atomic_load_n(&sync->refcount, __ATOMIC_SEQ_CST);
}

static void active_frame(struct worker *w, unsigned int count)
{
    EXPECT(w, w->data.msync_frames == &w->frames[0]);
    EXPECT(w, w->data.msync_private_frames == 1);
    EXPECT(w, depth(&w->data) == 1);
    EXPECT(w, w->frames[0].data == &w->data);
    EXPECT(w, w->frames[0].count == count);
}

/* Real read(2) CP on this worker's private pipe; cancellation is already queued. */
static void pipe_cp(struct worker *w, unsigned int enter, unsigned int returned,
                    char token, int expected_type, struct cancel_policy *policy,
                    unsigned int pin_count)
{
    char byte = 0;
    ssize_t result;
    *policy = read_policy();
    EXPECT(w, policy->state == PTHREAD_CANCEL_DISABLE);
    EXPECT(w, policy->type == expected_type);
    EXPECT(w, atomic_load_explicit(&w->cancel_sent, memory_order_acquire) == 1);
    active_frame(w, pin_count);
    publish(w, enter);
    do result = read(w->pipefd[0], &byte, 1); while (result < 0 && errno == EINTR);
    EXPECT(w, result == 1 && byte == token);
    active_frame(w, pin_count); /* The protected CP returned with frame intact. */
    publish(w, returned);
}

/*
 * EXPLICIT reference-release double, not Wine's release function. Preserve its
 * seq_cst decrement and save-fd-before-decrement rule. Terminal close deliberately
 * substitutes a blocking read CP. Never dereference sync after decrement to 0.
 */
static void release_inproc_sync(struct inproc_sync *sync)
{
    struct worker *w = get_thread_data()->owner;
    int saved_fd = sync->fd;
    LONG remaining = InterlockedDecrement(&sync->refcount);
    atomic_fetch_add_explicit(&w->release_calls, 1, memory_order_relaxed);
    EXPECT(w, remaining >= 0);
    if (!remaining)
    {
        int type = (w->scenario == SAME_FRAME_PROMOTE || w->scenario == ORIGINAL_ENTRY)
                   ? w->incoming_type : PTHREAD_CANCEL_DEFERRED;
        EXPECT(w, saved_fd == w->pipefd[0]);
        atomic_fetch_add_explicit(&w->close_calls, 1, memory_order_relaxed);
        pipe_cp(w, CLOSE_ENTER, CLOSE_RETURN, 'C', type, &w->close_policy, 0);
        atomic_fetch_add_explicit(&w->close_returns, 1, memory_order_relaxed);
    }
}

/* Record obligations BEFORE any repair; deliberately do not call frame_leave. */
static void record_cleanup(void *opaque)
{
    struct worker *w = opaque;
    struct cleanup_record *r = &w->cleanup;
    struct inproc_frame *frame;
    unsigned int i;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL)) fatal("cleanup mask", 2);
    EXPECT(w, atomic_fetch_add_explicit(&w->cleanup_calls, 1, memory_order_relaxed) == 0);
    r->events = events(w);
    r->private_frames = w->data.msync_private_frames;
    r->depth = depth(&w->data);
    frame = w->data.msync_frames;
    while (frame && r->linked_frames < 2)
    {
        if (frame != &w->frames[0] && frame != &w->frames[1])
        {
            r->unknown_frame = 1;
            break;
        }
        ++r->linked_frames;
        frame = frame->prev;
    }
    if (frame) r->unknown_frame = 1; /* Unknown pointer, cycle, or excessive chain. */
    for (i = 0; i < 2; ++i)
    {
        r->frame_counts[i] = w->frames[i].count;
        r->pins += r->frame_counts[i];
        r->refs[i] = refs(&w->syncs[i]);
        r->ref_obligations += (int)r->refs[i] - (int)w->expected_refs[i];
        EXPECT(w, r->refs[i] == w->expected_refs[i]);
    }
    r->begins = atomic_load_explicit(&w->begins, memory_order_relaxed);
    r->ends = atomic_load_explicit(&w->ends, memory_order_relaxed);
    r->replays = atomic_load_explicit(&w->replays, memory_order_relaxed);
    r->closes = atomic_load_explicit(&w->close_calls, memory_order_relaxed);
    r->close_returns = atomic_load_explicit(&w->close_returns, memory_order_relaxed);
    EXPECT(w, !r->private_frames && !r->linked_frames && !r->unknown_frame);
    EXPECT(w, !r->pins && r->depth == 0 && !r->ref_obligations);
    EXPECT(w, r->begins == (w->scenario == NESTED_FRAMES ? 2u : 1u));
    EXPECT(w, r->ends == r->begins && r->replays == 1);
    EXPECT(w, r->closes == (w->expect_close ? 1u : 0u));
    EXPECT(w, r->close_returns == r->closes);
    publish(w, CLEANUP_DONE);
}

static void pin(struct worker *w, struct inproc_frame *frame, unsigned int object)
{
    EXPECT(w, frame->count < MAXIMUM_WAIT_OBJECTS);
    if (frame->count >= MAXIMUM_WAIT_OBJECTS) fatal("fixture pin overflow", 2);
    frame->pins[frame->count++] = &w->syncs[object];
}

static void check_exit(struct worker *w)
{
    EXPECT(w, !w->data.msync_frames && !w->data.msync_private_frames);
    EXPECT(w, depth(&w->data) == 0);
    EXPECT(w, !w->frames[0].count && !w->frames[1].count);
    EXPECT(w, refs(&w->syncs[0]) == w->expected_refs[0]);
    EXPECT(w, refs(&w->syncs[1]) == w->expected_refs[1]);
    w->exit_policy = read_policy();
    EXPECT(w, w->exit_policy.state == w->incoming_state);
    EXPECT(w, w->exit_policy.type == w->incoming_type);
    publish(w, AFTER_LEAVE);
}

static void *worker_main(void *opaque)
{
    struct worker *w = opaque;
    struct inproc_frame *frame = &w->frames[0];
    fixture_thread_data = &w->data;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL) ||
        pthread_setcanceltype(w->incoming_type, NULL) ||
        pthread_setcancelstate(w->incoming_state, NULL)) fatal("worker initial policy", 2);

    pthread_cleanup_push(record_cleanup, w);
    if (w->scenario == ORIGINAL_ENTRY) inproc_frame_enter(frame);
    else inproc_frame_enter_ready(frame);
    pin(w, frame, 0);
    w->entry_policy = read_policy();
    EXPECT(w, w->entry_policy.state == (w->scenario == ORIGINAL_ENTRY
               ? PTHREAD_CANCEL_DISABLE : w->incoming_state));
    EXPECT(w, w->entry_policy.type == (w->scenario == ORIGINAL_ENTRY
               ? w->incoming_type : PTHREAD_CANCEL_DEFERRED));
    active_frame(w, 1);
    EXPECT(w, frame->prev == NULL);
    publish(w, ARMED); /* Parent cannot cancel until the frame/pin is published. */
    await_go(w);

    switch (w->scenario)
    {
    case READY_NONTERMINAL:
    case READY_NONTERMINAL_PENDING:
        EXPECT(w, refs(&w->syncs[0]) == 2);
        inproc_frame_leave(frame); /* Exact CAS fast path, never the release stub. */
        EXPECT(w, atomic_load_explicit(&w->release_calls, memory_order_relaxed) == 0);
        break;
    case READY_TERMINAL:
    case ORIGINAL_ENTRY:
        inproc_frame_leave(frame); /* The terminal release contains a real CP. */
        break;
    case SAME_FRAME_PROMOTE:
        inproc_frame_promote(frame);
        active_frame(w, 0); /* SAME pointer; no replacement frame or extra begin. */
        EXPECT(w, frame->cancel_type == -1);
        EXPECT(w, frame->cancel_state == w->incoming_state);
        EXPECT(w, refs(&w->syncs[0]) == 0);
        pipe_cp(w, SLOW_ENTER, SLOW_RETURN, 'S', w->incoming_type, &w->slow_policy, 0);
        inproc_frame_leave(frame);
        break;
    case BEFORE_KERNEL:
        inproc_prepare_kernel();
        EXPECT(w, frame->cancel_state == w->incoming_state);
        inproc_prepare_kernel(); /* Repeated callback must retain the saved state. */
        EXPECT(w, frame->cancel_state == w->incoming_state);
        EXPECT(w, refs(&w->syncs[0]) == 1);
        pipe_cp(w, KERNEL_ENTER, KERNEL_RETURN, 'K', PTHREAD_CANCEL_DEFERRED,
                &w->kernel_policy, 1);
        inproc_frame_leave(frame);
        break;
    case NESTED_FRAMES:
    {
        struct inproc_frame *inner = &w->frames[1];
        struct cancel_policy middle;
        inproc_frame_enter_ready(inner);
        pin(w, inner, 1);
        EXPECT(w, w->data.msync_frames == inner && inner->prev == frame);
        EXPECT(w, w->data.msync_private_frames == 2 && depth(&w->data) == 2);
        inproc_frame_mask(inner);
        EXPECT(w, inner->cancel_state == w->incoming_state);
        inproc_frame_mask(inner); /* Direct mask, including its idempotent path. */
        EXPECT(w, inner->cancel_state == w->incoming_state);
        EXPECT(w, frame->cancel_state == -1); /* Only the innermost frame changed. */
        inproc_frame_leave(inner);
        active_frame(w, 1);
        middle = read_policy();
        EXPECT(w, middle.state == w->incoming_state && middle.type == PTHREAD_CANCEL_DEFERRED);
        EXPECT(w, refs(&w->syncs[1]) == 1);
        inproc_frame_leave(frame);
        EXPECT(w, atomic_load_explicit(&w->release_calls, memory_order_relaxed) == 1);
        break;
    }
    case SCENARIO_COUNT:
        fatal("invalid scenario", 2);
    }

    check_exit(w); /* Pending ASYNC may legitimately cancel before this point. */
    if (w->expect_cancel)
    {
        char byte;
        /* All obligations are gone. DISABLE-input cases now permit delivery.
         * Change to DEFERRED while disabled solely for the final private read CP.
         * Cancellation may also arrive at the exact function's earlier restore.
         */
        if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL) ||
            pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) fatal("delivery policy", 2);
        publish(w, DELIVERY_ENTER);
        if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)) fatal("delivery enable", 2);
        /* Explicitly observe a request queued while the caller was DISABLE.
         * This is AFTER all obligations are drained. Do not rely on re-enable
         * alone propagating a pending library request to a subsequent syscall. */
        pthread_testcancel();
        (void)read(w->pipefd[0], &byte, 1); /* No parent token is sent for this CP. */
        EXPECT(w, 0); /* Returning instead of cancellation is a failure. */
    }
    publish(w, NORMAL_RETURN);
    pthread_cleanup_pop(1); /* Also snapshot normal exits; never repair state. */
    return &w->normal_token;
}

static void release_read(struct worker *w, unsigned int stage, char token)
{
    ssize_t result;
    if (!await_event(w, stage)) return; /* Early cancellation will fail after join. */
    do result = write(w->pipefd[1], &token, 1); while (result < 0 && errno == EINTR);
    if (result != 1) fatal("private pipe write failed", 2);
}

static void initialize_worker(struct worker *w, unsigned int id, enum scenario scenario,
                              int type, int state)
{
    unsigned int i;
    memset(w, 0, sizeof(*w));
    w->id = id;
    w->scenario = scenario;
    w->incoming_type = type;
    w->incoming_state = state;
    w->expect_cancel = scenario != READY_NONTERMINAL && scenario != NESTED_FRAMES;
    w->expect_close = w->expect_cancel && scenario != READY_NONTERMINAL_PENDING;
    w->data.owner = w;
    atomic_init(&w->events, 0);
    atomic_init(&w->go, 0);
    atomic_init(&w->cancel_sent, 0);
    atomic_init(&w->checks, 0);
    atomic_init(&w->failures, 0);
    atomic_init(&w->first_failure, 0);
    atomic_init(&w->cleanup_calls, 0);
    atomic_init(&w->release_calls, 0);
    atomic_init(&w->close_calls, 0);
    atomic_init(&w->close_returns, 0);
    atomic_init(&w->begins, 0);
    atomic_init(&w->ends, 0);
    atomic_init(&w->replays, 0);
    if (pipe(w->pipefd)) fatal("private pipe creation failed", 2);
    for (i = 0; i < 2; ++i)
        if (fcntl(w->pipefd[i], F_SETFD, FD_CLOEXEC) == -1) fatal("private pipe CLOEXEC", 2);
    __atomic_store_n(&w->syncs[0].refcount, w->expect_close ? 1 : 2, __ATOMIC_SEQ_CST);
    __atomic_store_n(&w->syncs[1].refcount, scenario == NESTED_FRAMES ? 2 : 0, __ATOMIC_SEQ_CST);
    w->expected_refs[0] = w->expect_close ? 0 : 1;
    w->expected_refs[1] = scenario == NESTED_FRAMES ? 1 : 0;
    w->syncs[0].fd = w->syncs[1].fd = w->pipefd[0];
}

static int run_case(unsigned int id, enum scenario scenario, int type, int state,
                    unsigned int *total_checks)
{
    struct worker w; /* Entire record stays alive through the EXACT join below. */
    pthread_t thread;
    void *result = NULL;
    unsigned int required, seen, failed;
    int close_read_result, close_write_result;
    initialize_worker(&w, id, scenario, type, state);
    if (pthread_create(&thread, NULL, worker_main, &w)) fatal("pthread_create failed", 2);
    /* Default POSIX attributes are joinable. There is exactly one owner/joiner. */
    if (await_event(&w, ARMED))
    {
        if (w.expect_cancel)
        {
            if (pthread_cancel(thread)) fatal("pthread_cancel failed", 2);
            atomic_store_explicit(&w.cancel_sent, 1, memory_order_release);
        }
        atomic_store_explicit(&w.go, 1, memory_order_release);
        if (scenario == BEFORE_KERNEL) release_read(&w, KERNEL_ENTER, 'K');
        if (w.expect_close) release_read(&w, CLOSE_ENTER, 'C');
        if (scenario == SAME_FRAME_PROMOTE) release_read(&w, SLOW_ENTER, 'S');
    }
    if (pthread_join(thread, &result)) fatal("exact pthread_join failed", 2);
    /* No worker can touch w or its descriptors after this successful join. */
    close_read_result = close(w.pipefd[0]);
    close_write_result = close(w.pipefd[1]);
    EXPECT(&w, close_read_result == 0 && close_write_result == 0);
    EXPECT(&w, result == (w.expect_cancel ? PTHREAD_CANCELED : (void *)&w.normal_token));
    EXPECT(&w, atomic_load_explicit(&w.cleanup_calls, memory_order_relaxed) == 1);
    required = ARMED | CLEANUP_DONE;
    if (w.expect_close) required |= CLOSE_ENTER | CLOSE_RETURN;
    if (!w.expect_cancel) required |= AFTER_LEAVE | NORMAL_RETURN;
    if (scenario == SAME_FRAME_PROMOTE) required |= SLOW_ENTER | SLOW_RETURN;
    if (scenario == BEFORE_KERNEL) required |= KERNEL_ENTER | KERNEL_RETURN;
    seen = events(&w);
    EXPECT(&w, (seen & required) == required);
    EXPECT(&w, !!(seen & NORMAL_RETURN) == !w.expect_cancel);
    failed = atomic_load_explicit(&w.failures, memory_order_relaxed);
    *total_checks += atomic_load_explicit(&w.checks, memory_order_relaxed);
    if (printf("{\"kind\":\"case\",\"id\":%u,\"scenario\":\"%s\","
               "\"type\":\"%s\",\"state\":\"%s\",\"pass\":%s,\"joined\":true,"
               "\"cancelled\":%s,\"checks\":%u,\"failures\":%u,\"first_line\":%u,"
               "\"events\":%u,\"cleanup_events\":%u,\"frames\":%u,\"linked\":%u,"
               "\"unknown_frame\":%u,\"pins\":%u,\"frame_counts\":[%u,%u],\"depth\":%d,"
               "\"refs\":[%" PRId32 ",%" PRId32 "],\"ref_obligations\":%d,"
               "\"begins\":%u,\"ends\":%u,\"replays\":%u,\"closes\":%u,\"close_returns\":%u}\n",
               id, names[scenario], type == PTHREAD_CANCEL_ASYNCHRONOUS ? "ASYNC" : "DEFERRED",
               state == PTHREAD_CANCEL_ENABLE ? "ENABLE" : "DISABLE", failed ? "false" : "true",
               result == PTHREAD_CANCELED ? "true" : "false",
               atomic_load_explicit(&w.checks, memory_order_relaxed), failed,
               atomic_load_explicit(&w.first_failure, memory_order_relaxed), seen,
               w.cleanup.events, w.cleanup.private_frames, w.cleanup.linked_frames,
               w.cleanup.unknown_frame, w.cleanup.pins, w.cleanup.frame_counts[0],
               w.cleanup.frame_counts[1], (int)w.cleanup.depth, w.cleanup.refs[0], w.cleanup.refs[1],
               w.cleanup.ref_obligations, w.cleanup.begins, w.cleanup.ends, w.cleanup.replays,
               w.cleanup.closes, w.cleanup.close_returns) < 0) fatal("case output failed", 2);
    checked_output();
    if (failed && w.first_expression)
    {
        if (fprintf(stderr, "frame-fixture case %u first failed check: %s\n",
                    id, w.first_expression) < 0 || fflush(stderr) == EOF || ferror(stderr))
            fatal("diagnostic output failed", 2);
    }
    return failed != 0;
}

int main(void)
{
    const int types[] = { PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_ASYNCHRONOUS };
    const int states[] = { PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE };
    struct sigaction action;
    unsigned int scenario, type, state, owned = 0, failed = 0, checks = 0;
    uint64_t run_stamp;
    pid_t pid = getpid();
    if (getsid(0) != pid || getpgrp() != pid)
        fatal("requires a NEW process session (e.g. subprocess start_new_session=True)", 2);
    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_expired;
    if (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL)) fatal("alarm setup", 2);
    alarm(25);
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL)) fatal("parent cancel mask", 2);
    run_stamp = now_ns();
    if (printf("{\"kind\":\"scope\",\"scope\":\"native exact-frame helper WITH STUBS; "
               "not Wine/API/signal-handler/Windows equivalence\",\"pid\":%ld,"
               "\"run_stamp\":%" PRIu64 ",\"source_sha256\":\"%s\",\"block_sha256\":\"%s\"}\n",
               (long)pid, run_stamp, FRAME_FIXTURE_SOURCE_SHA256, FRAME_FIXTURE_BLOCK_SHA256) < 0)
        fatal("scope output failed", 2);
    checked_output();
    for (scenario = 0; scenario < SCENARIO_COUNT; ++scenario)
        for (type = 0; type < sizeof(types) / sizeof(types[0]); ++type)
            for (state = 0; state < sizeof(states) / sizeof(states[0]); ++state)
            {
                failed += (unsigned int)run_case(owned + 1, (enum scenario)scenario,
                                                 types[type], states[state], &checks);
                ++owned;
            }
    /* One completion record, only after ALL exact joins and descriptor closes.
     * Exit status, pass, source hashes, 28 cases, and this record all matter.
     * A timeout, assertion, output error, or unjoined worker cannot claim pass.
     */
    if (printf("{\"kind\":\"completion\",\"pid\":%ld,\"run_stamp\":%" PRIu64 ","
               "\"workers\":%u,\"joined\":%u,\"failed_cases\":%u,\"checks\":%u,\"pass\":%s}\n",
               (long)pid, run_stamp, owned, owned, failed, checks,
               !failed && owned == 28 ? "true" : "false") < 0) fatal("completion output failed", 2);
    checked_output();
    alarm(0);
    return failed || owned != 28 ? 1 : 0;
}
