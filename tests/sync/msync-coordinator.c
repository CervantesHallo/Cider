/* Exercise the exact production Darwin coordinator, not a replacement.
 * Coordinator recovery only: this is not Windows/Wine API equivalence.
 *
 * The launcher MUST use a fresh session (e.g. subprocess start_new_session=True).
 * We require pid == sid == pgid before creating resources. An independent child
 * watchdog bounds coordinator calls, joins and resource teardown to 120 seconds.
 * It can kill ONLY this verified private process group, including itself. A
 * watchdog abort has no successful completion marker; the wrapper must treat a
 * signal/missing completion as failure and may remove the owned temporary file.
 * Normal and initialization-failure teardown is checked before completion.
 *
 * Requires the lease-token header API: msync_coord_new_lease(coord,fd), and
 * msync_coord_enter(coord,fd,token,guard). Each thread opens its own description;
 * closing it releases the lifetime lease. Fork children close/reset inherited
 * descriptions/tokens. No performance acceptance threshold lives in this file.
 *
 * The APPLIED and stale-helper fixtures assert shared per-slot notification
 * delivery. The latter leaves a NEW pending journal on indices 3/4 while a helper
 * holds an OLD snapshot of 1/2. Separate isolated-header mutations that omit a
 * recovered notification or remove the sequence guard should be rejected; such
 * mutation builds/runs are the parent's responsibility, not claimed here.
 * Live-owner probes confirm SIGSTOP, observe a fresh contender wait hint, and
 * require exclusion beyond the 1 ms reclaim timeout until explicit release.
 * A killed-waiter probe checks later transactions and a fresh contended handoff.
 * These are native header fixtures, not production Wine atfork coverage.
 */
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "wine/msync_coord.h"

#define WATCHDOG_MS 120000
#define CHILD_MS 5000
#define PIPE_MS 3000
#define SHARED_SIZE 4096
#define SLOT_COUNT 8
#define MAX_CHILDREN 8
#define LIVE_OWNER_HOLD_MS 50u
#define QUIET_TRANSACTIONS 32u

struct shared_state
{
    uint64_t notices[SLOT_COUNT];
    uint64_t probe_owner_token, probe_waiter_token;
    unsigned int probe_acquired, probe_owner_released;
    unsigned int close_errors, worker_error, stop;
#ifdef CIDER_TEST_MSYNC_DEFER_NOTIFY
    uint64_t notify_scope_calls;
    unsigned int notify_under_owner;
#endif
};
_Static_assert(sizeof(struct shared_state) <= SHARED_SIZE, "shared state bounds");
static struct shared_state *shared;
static struct msync_coord coord;
/* Distinct real pages let death injection trap the second payload store,
 * independent of how many times a production header resolves its slot. */
static void *slot_memory;
static size_t slot_page_size, slot_memory_size;
#define SLOT(i) (*(uint64_t *)((char *)slot_memory + (size_t)(i) * slot_page_size))
static int identity_fd = -1, failures, output_error, injection;
static int notify_block_fd = -1, notify_ready_fd = -1;
static volatile sig_atomic_t protected_slot = 2;
static int watchdog_pipe[2] = {-1,-1};
static pid_t session_owner, watchdog_pid = -1, children[MAX_CHILDREN];
static __thread int work_fd = -1;
static __thread uint64_t work_token;
static pthread_key_t fd_key;
static int key_created;
static const char *test_name = "initialization";

/* Optional tagged headers keep the same journal phases in one versioned word.
 * Constructed phase inputs run without workers, as documented at each fixture. */
static unsigned int journal_phase(void)
{
#ifdef CIDER_TEST_MSYNC_TAGGED_DECISION
    return msync_coord_phase(coord.header);
#else
    return coord.header->state;
#endif
}
static uint64_t journal_serial(void)
{
#ifdef CIDER_TEST_MSYNC_TAGGED_DECISION
    return msync_coord_serial(coord.header);
#else
    return coord.header->sequence;
#endif
}
static void set_journal_phase(unsigned int phase)
{
#ifdef CIDER_TEST_MSYNC_TAGGED_DECISION
    uint64_t tag=__atomic_load_n(&coord.header->decision,__ATOMIC_ACQUIRE);
    __atomic_store_n(&coord.header->decision,(tag & ~MSYNC_COORD_PHASE_MASK) | phase,__ATOMIC_RELEASE);
#else
    __atomic_store_n(&coord.header->state,phase,__ATOMIC_RELEASE);
#endif
}


static void fail(const char *message)
{
    ++failures;
    if (fprintf(stderr,"%s: %s (errno=%d)\n",test_name,message,errno) < 0) output_error = 1;
}
#define CHECK(c,m) do { if (!(c)) fail(m); } while (0)
static void emit(const char *format,...)
{
    va_list args;
    va_start(args,format);
    if (vprintf(format,args) < 0) output_error = 1;
    va_end(args);
}
static void flush_output(void)
{
    if (fflush(stdout) || ferror(stdout)) output_error = 1;
}
static int now_ns(uint64_t *value)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC,&ts)) { fail("clock_gettime"); return 0; }
    *value = (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
    return 1;
}
static void close_fd(int *value)
{
    int fd = *value;
    *value = -1;
    if (fd >= 0) CHECK(close(fd) == 0,"close owned fd");
}
static int make_pipe(int result[2])
{
    unsigned int i;
    int flags;
    result[0] = result[1] = -1;
    if (pipe(result)) { fail("pipe"); return 0; }
    for (i=0;i<2;++i)
    {
        flags = fcntl(result[i],F_GETFL);
        if (flags < 0 || fcntl(result[i],F_SETFL,flags | O_NONBLOCK) < 0 ||
            fcntl(result[i],F_SETFD,FD_CLOEXEC) < 0)
        {
            fail("pipe flags"); close_fd(&result[0]); close_fd(&result[1]); return 0;
        }
    }
    return 1;
}
/* Deadline/retry handling never changes a blocking descriptor's status flags. */
static int pipe_byte(int fd,int writing,char expected)
{
    uint64_t start, current;
    if (fd < 0 || !now_ns(&start)) return 0;
    for (;;)
    {
        struct pollfd pfd = {fd,writing ? POLLOUT : POLLIN,0};
        char byte = expected;
        ssize_t count;
        int ret, remaining;
        if (!now_ns(&current) || current-start >= (uint64_t)PIPE_MS*1000000) return 0;
        remaining = PIPE_MS-(int)((current-start)/1000000);
        ret = poll(&pfd,1,remaining);
        if (ret < 0 && errno == EINTR) continue;
        if (ret <= 0 || (pfd.revents & (POLLNVAL | POLLERR))) return 0;
        count = writing ? write(fd,&byte,1) : read(fd,&byte,1);
        if (count == 1) return writing || byte == expected;
        if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        return 0;
    }
}
/* This is the ONLY group-signal site. The creator must already be a session
 * leader; each invocation rechecks membership in that exact private session AND
 * group. A watchdog/worker child has a different pid but the same recorded sid. */
static void abort_owned_session(pid_t owner)
{
    if (owner > 1 && getsid(0) == owner && getpgrp() == owner)
        (void)kill(-owner,SIGKILL);
    _exit(124);
}
static int owned_index(pid_t pid)
{
    unsigned int i;
    if (pid <= 1) return -1;
    for (i=0;i<MAX_CHILDREN;++i) if (children[i] == pid) return (int)i;
    return -1;
}
static int free_child_index(void)
{
    unsigned int i;
    for (i=0;i<MAX_CHILDREN;++i) if (!children[i]) return (int)i;
    fail("owned child table full"); return -1;
}
static int reap_for(pid_t pid,int *status)
{
    uint64_t start,current;
    int index = owned_index(pid);
    if (pid <= 1 || index < 0) { fail("reject non-owned/nonpositive child pid"); return -1; }
    if (!now_ns(&start)) return -1;
    for (;;)
    {
        pid_t ret = waitpid(pid,status,WNOHANG);
        if (ret == pid) { children[index] = 0; return 1; }
        if (ret < 0 && errno != EINTR)
        {
            if (errno == ECHILD) children[index] = 0; /* never signal a potentially reused pid */
            fail("waitpid owned child"); return -1;
        }
        if (!now_ns(&current)) return -1;
        if (current-start >= (uint64_t)CHILD_MS*1000000) return 0;
        (void)poll(NULL,0,1);
    }
}
static int send_owned_signal(pid_t pid,int signo)
{
    pid_t sid,pgid;
    if (pid <= 1 || owned_index(pid) < 0) { fail("refuse signal to non-owned child"); return 0; }
    sid = getsid(pid); pgid = getpgid(pid);
    if ((sid < 0 || pgid < 0) && errno == ESRCH) return 1; /* reap already-exited child */
    if (sid != session_owner || pgid != session_owner)
    {
        fail("owned child left private session/group"); return 0;
    }
    if (kill(pid,signo) && errno != ESRCH) { fail("signal owned child"); return 0; }
    return 1;
}
static int signal_owned_child(pid_t pid)
{
    return send_owned_signal(pid,SIGKILL);
}
static int wait_stopped(pid_t pid)
{
    uint64_t start,current;
    int index = owned_index(pid), status = 0;
    if (pid <= 1 || index < 0) { fail("stop observation rejects non-owned pid"); return 0; }
    if (!now_ns(&start)) return 0;
    for (;;)
    {
        pid_t ret = waitpid(pid,&status,WNOHANG | WUNTRACED);
        if (ret == pid)
        {
            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) return 1;
            if (WIFEXITED(status) || WIFSIGNALED(status)) children[index] = 0;
            fail("owner did not enter requested SIGSTOP state"); return 0;
        }
        if (ret < 0 && errno != EINTR)
        {
            if (errno == ECHILD) children[index] = 0;
            fail("waitpid stopped owner"); return 0;
        }
        if (!now_ns(&current)) return 0;
        if (current-start >= (uint64_t)CHILD_MS*1000000) { fail("owner stop deadline"); return 0; }
        (void)poll(NULL,0,1);
    }
}
static int stop_child(pid_t *pid)
{
    int status = 0, ok;
    if (*pid <= 1) { *pid = -1; return 1; }
    if (owned_index(*pid) < 0) { *pid = -1; return 1; } /* already reaped */
    ok = signal_owned_child(*pid);
    if (reap_for(*pid,&status) != 1)
    {
        fail("cannot safely reap owned child"); abort_owned_session(session_owner);
    }
    *pid = -1;
    return ok;
}
static int wait_child(pid_t pid,int expected)
{
    int status = 0, ret;
    pid_t cleanup = pid;
    if (pid <= 1 || owned_index(pid) < 0)
    {
        fail("wait_child rejects pid <= 1 or unowned pid"); return 0;
    }
    ret = reap_for(pid,&status);
    if (ret != 1)
    {
        fail("owned child deadline/reap failure"); stop_child(&cleanup); return 0;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != expected)
    {
        fail("owned child exit/injection marker mismatch"); return 0;
    }
    return 1;
}
static void watchdog_loop(pid_t owner)
{
    struct timespec ts;
    uint64_t start,current;
    if (owner <= 1 || getsid(0) != owner || getpgrp() != owner) _exit(125);
    if (close(watchdog_pipe[1])) abort_owned_session(owner);
    if (identity_fd >= 0 && close(identity_fd)) abort_owned_session(owner);
    if (clock_gettime(CLOCK_MONOTONIC,&ts)) abort_owned_session(owner);
    start = (uint64_t)ts.tv_sec*1000000000 + (uint64_t)ts.tv_nsec;
    for (;;)
    {
        struct pollfd pfd = {watchdog_pipe[0],POLLIN,0};
        char byte;
        ssize_t count;
        int ret,remaining;
        if (clock_gettime(CLOCK_MONOTONIC,&ts)) abort_owned_session(owner);
        current = (uint64_t)ts.tv_sec*1000000000 + (uint64_t)ts.tv_nsec;
        if (current-start >= (uint64_t)WATCHDOG_MS*1000000) abort_owned_session(owner);
        remaining = WATCHDOG_MS-(int)((current-start)/1000000);
        ret = poll(&pfd,1,remaining);
        if (ret < 0 && errno == EINTR) continue;
        if (ret <= 0 || (pfd.revents & (POLLNVAL | POLLERR))) abort_owned_session(owner);
        count = read(watchdog_pipe[0],&byte,1);
        if (count < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        if (count != 1 || byte != 'D') abort_owned_session(owner); /* EOF includes parent death */
        if (close(watchdog_pipe[0])) _exit(126);
        _exit(0);
    }
}
static int start_watchdog(void)
{
    int index = free_child_index();
    if (index < 0 || !make_pipe(watchdog_pipe)) return 0;
    watchdog_pid = fork();
    if (!watchdog_pid) watchdog_loop(session_owner);
    if (watchdog_pid < 0)
    {
        fail("fork watchdog"); close_fd(&watchdog_pipe[0]); close_fd(&watchdog_pipe[1]); return 0;
    }
    children[index] = watchdog_pid;
    close_fd(&watchdog_pipe[0]);
    return 1;
}
static void close_work(void *value)
{
    if (value && close((int)(intptr_t)value-1))
        __atomic_add_fetch(&shared->close_errors,1,__ATOMIC_RELAXED);
    work_fd = -1; work_token = 0;
}
static int fd(void)
{
    if (work_fd < 0)
    {
        work_fd = msync_coord_open(identity_fd);
        work_token = msync_coord_new_lease(&coord,work_fd);
        if (!work_token) _exit(86);
        if (pthread_setspecific(fd_key,(void *)(intptr_t)(work_fd+1))) _exit(87);
    }
    return work_fd;
}
static void child_fd(void)
{
    /* Parent is single-threaded at each fork; the watchdog is another process.
     * Drop the inherited lease before making an independent open/token. */
    if (work_fd >= 0 && close(work_fd)) _exit(88);
    work_fd = -1; work_token = 0;
    if (pthread_setspecific(fd_key,NULL)) _exit(89);
    if (watchdog_pipe[1] >= 0 && close(watchdog_pipe[1])) _exit(90);
    watchdog_pipe[1] = -1;
}
static pid_t fork_owned(void)
{
    int index = free_child_index();
    pid_t pid;
    if (index < 0) return -1;
    pid = fork();
    if (!pid) { child_fd(); return 0; }
    if (pid < 0) { fail("fork case child"); return -1; }
    children[index] = pid;
    return pid;
}
static void enter(struct msync_coord_guard *guard)
{
    /* Separate statements: C argument evaluation must not read token before fd(). */
    int local_fd = fd();
    uint64_t local_token = work_token;
    msync_coord_enter(&coord,local_fd,local_token,guard);
}
static void finish(struct msync_coord_guard *guard)
{
#ifdef CIDER_TEST_MSYNC_DIRECT_FINISH
    msync_coord_finish(guard);
#else
    int pending = journal_phase() != MSYNC_COORD_CLEAN;
    msync_coord_leave(guard);
    if (pending) { enter(guard); msync_coord_leave(guard); }
#endif
}
static void *slot(unsigned int idx,int map)
{
    (void)map;
    if (idx < 1 || idx >= SLOT_COUNT) return NULL;
    return &SLOT(idx);
}
static int notify(unsigned int idx,void *shm,int blocking)
{
    if (idx < 1 || idx >= SLOT_COUNT || shm != &SLOT(idx)) _exit(96);
#ifdef CIDER_TEST_MSYNC_DEFER_NOTIFY
    __atomic_add_fetch(&shared->notify_scope_calls,1,__ATOMIC_RELAXED);
    if (!work_token || __atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST)==work_token)
        __atomic_store_n(&shared->notify_under_owner,1,__ATOMIC_RELAXED);
#endif
    if (injection == 2 && !blocking) return 0;
    /* Inject at delivery entry even if a broken header cleared its phase early:
     * such a mutation must expose the lost wake, rather than skip the crash. */
    if (injection == 3) _exit(38);
    if (blocking && notify_ready_fd >= 0)
    {
        if (!pipe_byte(notify_ready_fd,1,'N')) _exit(97);
        if (!pipe_byte(notify_block_fd,0,'R')) _exit(98);
        notify_ready_fd = -1;
    }
    __atomic_add_fetch(&shared->notices[idx],1,__ATOMIC_RELEASE);
    return 1;
}
static uint64_t notices(unsigned int idx)
{
    return __atomic_load_n(&shared->notices[idx],__ATOMIC_ACQUIRE);
}
static void set_pair(uint64_t a,uint64_t b,int wake)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry entries[2] = {{1,(uint32_t)wake,a},{2,(uint32_t)wake,b}};
    enter(&guard); msync_coord_commit(&coord,entries,2); finish(&guard);
}
static void reset(void)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry entries[4] = {{1,0,1},{2,0,1},{3,0,1},{4,0,1}};
    unsigned int i;
    injection = 0; notify_block_fd = notify_ready_fd = -1;
    enter(&guard); msync_coord_commit(&coord,entries,4); finish(&guard);
    /* No case child/worker is live when resetting these shared observations. */
    for (i=0;i<SLOT_COUNT;++i) __atomic_store_n(&shared->notices[i],0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->stop,0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->worker_error,0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->probe_owner_token,0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->probe_waiter_token,0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->probe_acquired,0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->probe_owner_released,0,__ATOMIC_RELEASE);
}
static void report(int before)
{
    emit("{\"kind\":\"coordinator-case\",\"name\":\"%s\",\"passed\":%s}\n",test_name,
         failures == before && !output_error ? "true" : "false");
    flush_output();
}
/* Child-local page protection changes no shared value. Reads/preparation stay
 * valid, but the actual production store to slot2 faults after slot1 is written.
 * Only async-signal-safe operations are used at this deliberate fault boundary. */
static void payload_fault(int signal_number, siginfo_t *info, void *context)
{
    uintptr_t address=(uintptr_t)info->si_addr, first=(uintptr_t)&SLOT(protected_slot);
    char marker='S';
    (void)signal_number; (void)context;
    if (address < first || address >= first+slot_page_size ||
        journal_phase() != MSYNC_COORD_COMMITTED) _exit(92);
    if (injection == 1) _exit(37);
    if (injection == 4)
    {
        if (write(notify_ready_fd,&marker,1) != 1) _exit(94);
        if (kill(getpid(),SIGSTOP)) _exit(93);
        _exit(95);
    }
    _exit(90);
}
static void arm_payload_fault_at(unsigned int index)
{
    struct sigaction action;
    if (!index || index >= SLOT_COUNT) _exit(88);
    protected_slot=(sig_atomic_t)index;
    memset(&action,0,sizeof(action)); sigemptyset(&action.sa_mask);
    action.sa_sigaction=payload_fault; action.sa_flags=SA_SIGINFO;
    if (sigaction(SIGSEGV,&action,NULL) || sigaction(SIGBUS,&action,NULL) ||
        mprotect(&SLOT(index),slot_page_size,PROT_READ)) _exit(89);
}
static void arm_payload_fault(void) { arm_payload_fault_at(2); }

/* All three boundaries use the real production count1 path. The first is a
 * constructed unpublished metadata input, not an observed interrupted copy. */
static void single_recovery_case(int kind)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry entry={1,1,4};
    pid_t child=-1;
    int before=failures,expected=kind==0 ? 36 : kind==1 ? 37 : 38;
    test_name=kind==0 ? "single-unpublished-CLEAN-metadata-input" : kind==1 ?
              "single-death-before-payload-store" : "single-death-before-notify";
    reset(); child=fork_owned(); if(child<0)goto done;
    if(!child)
    {
        if(kind==0)
        {
            enter(&guard);
            /* CLEAN metadata is not a published decision, even if an entry
             * would be invalid if a helper incorrectly tried to replay it. */
            coord.header->count=1;
            coord.header->entries[0]=(struct msync_coord_entry){0,1,99};
            _exit(36);
        }
        injection=kind==1 ? 1 : 3;
        if(kind==1)arm_payload_fault_at(1);
        enter(&guard); msync_coord_commit(&coord,&entry,1); finish(&guard); _exit(99);
    }
    if(!wait_child(child,expected))goto done;
    CHECK(SLOT(1)==(kind==2 ? 4u : 1u),"single pre-recovery payload boundary");
    CHECK(notices(1)==0,"single notification must not precede injected boundary");
    enter(&guard);
    CHECK(SLOT(1)==(kind==0 ? 1u : 4u),"single committed decision recovery");
    CHECK(journal_phase()==MSYNC_COORD_CLEAN,"single recovered journal not clean");
    msync_coord_leave(&guard);
    if(kind)CHECK(notices(1)>=1,"single committed notification obligation lost");
    CHECK(SLOT(2)==1,"single recovery changed a different slot");
 done:
    stop_child(&child); report(before);
}
static void single_noop_and_queued_wake_case(void)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry entry={1,1,4};
    uint64_t serial;
    int before=failures;
    test_name="single-noop-and-queued-wake";reset();
    enter(&guard);injection=2;msync_coord_commit(&coord,&entry,1);injection=0;
    CHECK(SLOT(1)==4 && journal_phase()!=MSYNC_COORD_CLEAN,"single failed delivery lost committed decision");
    finish(&guard);CHECK(notices(1)>=1,"single queued wake not delivered");
    serial=journal_serial();entry.notify=0;
    enter(&guard);msync_coord_commit(&coord,&entry,1);finish(&guard);
    CHECK(journal_serial()==serial,"single no-op created a decision");
    enter(&guard);msync_coord_commit(&coord,NULL,0);finish(&guard);
    CHECK(journal_serial()==serial,"empty commit created a decision");
    entry.notify=1;
    enter(&guard);msync_coord_commit(&coord,&entry,1);finish(&guard);
    CHECK(journal_serial()>serial && notices(1)>=2,"single same-value wake obligation omitted");
    report(before);
}
static void filtered_commit_case(void)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry entries[4]={{1,0,1},{2,0,4},{3,1,1},{4,0,1}};
    uint64_t serial;
    int before=failures;
    test_name="filtered-commit-preserves-slot-order"; reset();
    enter(&guard); msync_coord_commit(&coord,entries,4); finish(&guard);
    CHECK(SLOT(1)==1 && SLOT(2)==4 && SLOT(3)==1 && SLOT(4)==1,"filtered payload/slot order");
    CHECK(notices(1)==0 && notices(2)==0 && notices(3)==1 && notices(4)==0,"unchanged notifying slot retained");
    serial=journal_serial(); entries[1].value=4; entries[2].notify=0;
    enter(&guard); msync_coord_commit(&coord,entries,4); finish(&guard);
    CHECK(journal_serial()==serial,"all-unchanged commit must not create a new decision");
    report(before);
}
static void simple_recovery(int kind)
{
    struct msync_coord_guard guard;
    pid_t child = -1;
    int before = failures, expected = kind == 0 ? 36 : kind == 1 ? 37 : 38;
    test_name = kind == 0 ? "preparing-recovery-input" : kind == 1 ?
                "death-after-first-committed-write" : "death-before-notify";
    reset();
    child = fork_owned();
    if (child < 0) goto done;
    if (!child)
    {
        if (kind == 0)
        {
            enter(&guard);
            /* Constructed PREPARING input, not an observed interrupted memcpy. */
            set_journal_phase(MSYNC_COORD_PREPARING);
            _exit(36);
        }
        injection = kind == 1 ? 1 : 3;
        if (kind == 1) arm_payload_fault();
        set_pair(kind == 1 ? 0 : 4,kind == 1 ? 0 : 4,kind == 1 ? 0 : 1);
        _exit(99);
    }
    if (!wait_child(child,expected)) goto done;
    if (kind == 1) CHECK(SLOT(1) == 0 && SLOT(2) == 1,"first committed write boundary");
    if (kind == 2) CHECK(notices(1) == 0 && notices(2) == 0,"APPLIED death must precede notifications");
    enter(&guard);
    CHECK(SLOT(1) == (kind == 0 ? 1u : kind == 1 ? 0u : 4u) && SLOT(2) == SLOT(1),"recovered values");
    CHECK(journal_phase() == MSYNC_COORD_CLEAN,"recovered journal not clean");
    msync_coord_leave(&guard);
    if (kind == 2)
    {
        CHECK(notices(1) >= 1 && notices(2) >= 1,"both APPLIED notification obligations must be delivered");
        emit("{\"kind\":\"coordinator-wakes\",\"name\":\"death-before-notify\",\"slot1\":%" PRIu64 ",\"slot2\":%" PRIu64 "}\n",notices(1),notices(2));
    }
done:
    stop_child(&child); report(before);
}
static void killed_owner_case(void)
{
    struct msync_coord_guard guard;
    int before = failures, ready[2] = {-1,-1}, status = 0;
    pid_t child = -1;
    test_name = "sigkill-owner-during-commit"; reset();
    if (!make_pipe(ready)) goto done;
    child = fork_owned();
    if (child < 0) goto done;
    if (!child)
    {
        if (close(ready[0])) _exit(91);
        notify_ready_fd = ready[1]; injection = 4; arm_payload_fault(); set_pair(2,2,0); _exit(99);
    }
    close_fd(&ready[1]);
    if (!pipe_byte(ready[0],0,'S')) { fail("COMMITTED kill boundary marker absent"); goto done; }
    CHECK(SLOT(1) == 2 && SLOT(2) == 1 && journal_phase() == MSYNC_COORD_COMMITTED,"kill boundary state");
    if (!signal_owned_child(child)) goto done;
    if (reap_for(child,&status) != 1) { fail("killed owner reap deadline"); goto done; }
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL,"owner did not die from requested SIGKILL");
    enter(&guard);
    CHECK(SLOT(1) == 2 && SLOT(2) == 2,"killed owner's committed decision was not recovered");
    CHECK(journal_phase() == MSYNC_COORD_CLEAN,"killed owner journal not clean");
    msync_coord_leave(&guard);
done:
    stop_child(&child); close_fd(&ready[0]); close_fd(&ready[1]); report(before);
}
static void stale_helper_case(void)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry old[2] = {{1,1,6},{2,1,6}}, newer[2] = {{3,1,8},{4,1,8}};
    int before = failures, ready[2] = {-1,-1}, resume[2] = {-1,-1};
    pid_t child = -1;
    uint64_t old_sequence, new_sequence;
    test_name = "stale-helper-preserves-new-pending-journal"; reset();
    if (!make_pipe(ready) || !make_pipe(resume)) goto done;
    enter(&guard); injection = 2; msync_coord_commit(&coord,old,2); injection = 0;
    old_sequence = journal_serial();
    CHECK(journal_phase() == MSYNC_COORD_APPLIED,"old journal must remain APPLIED");
    msync_coord_leave(&guard);
    if (failures != before) goto done;
    child = fork_owned();
    if (child < 0) goto done;
    if (!child)
    {
        if (close(ready[0]) || close(resume[1])) _exit(91);
        notify_ready_fd = ready[1]; notify_block_fd = resume[0];
        enter(&guard); msync_coord_leave(&guard); _exit(0);
    }
    close_fd(&ready[1]); close_fd(&resume[0]);
    if (!pipe_byte(ready[0],0,'N')) { fail("old helper notification boundary absent"); goto done; }
    /* Another helper drains the old journal while the child is outside the gate. */
    enter(&guard); msync_coord_leave(&guard);
    set_pair(7,7,0); /* old snapshot must never overwrite these new values */
    enter(&guard); injection = 2; msync_coord_commit(&coord,newer,2); injection = 0;
    new_sequence = journal_serial();
    CHECK(journal_phase() == MSYNC_COORD_APPLIED && new_sequence > old_sequence,"new pending journal not established");
    msync_coord_leave(&guard);
    CHECK(notices(3) == 0 && notices(4) == 0,"new journal delivered before resume");
    if (failures != before) goto done;
    if (!pipe_byte(resume[1],1,'R')) { fail("resume old helper"); goto done; }
    close_fd(&resume[1]);
    if (!wait_child(child,0)) goto done;
    /* Do NOT enter/recover in the parent before these assertions: that would
     * hide a stale helper that incorrectly cleared the newer APPLIED journal. */
    CHECK(notices(3) >= 1 && notices(4) >= 1,"owed wakes for distinct new slots 3/4 were lost");
    CHECK(SLOT(1) == 7 && SLOT(2) == 7,"old helper replayed old slot 1/2 values");
    CHECK(SLOT(3) == 8 && SLOT(4) == 8,"new slot 3/4 values changed");
    CHECK(journal_phase() == MSYNC_COORD_CLEAN && journal_serial() == new_sequence,"new journal not recovered by resumed helper");
    emit("{\"kind\":\"coordinator-wakes\",\"name\":\"stale-helper\",\"oldIndices\":[1,2],\"newIndices\":[3,4],\"slot3\":%" PRIu64 ",\"slot4\":%" PRIu64 "}\n",notices(3),notices(4));
done:
    stop_child(&child);
    close_fd(&ready[0]); close_fd(&ready[1]); close_fd(&resume[0]); close_fd(&resume[1]);
    report(before);
}

struct gate_child
{
    pid_t pid;
    int ready[2], command[2];
};
static void cleanup_gate_child(struct gate_child *child)
{
    stop_child(&child->pid);
    close_fd(&child->ready[0]); close_fd(&child->ready[1]);
    close_fd(&child->command[0]); close_fd(&child->command[1]);
}
/* Staging only: these children call the actual header's enter/leave functions.
 * No production Wine atfork hooks or admission algorithm are replicated here.
 * A contender opens its own lease before W, but cannot attempt entry until G. */
static int start_gate_child(struct gate_child *child,int holder)
{
    int before = failures;
    if (!make_pipe(child->ready) || !make_pipe(child->command)) return 0;
    if (!holder)
    {
        __atomic_store_n(&shared->probe_waiter_token,0,__ATOMIC_RELEASE);
        __atomic_store_n(&shared->probe_acquired,0,__ATOMIC_RELEASE);
    }
    child->pid = fork_owned();
    if (child->pid < 0) return 0;
    if (!child->pid)
    {
        struct msync_coord_guard guard;
        if (close(child->ready[0]) || close(child->command[1])) _exit(100);
        if (holder)
        {
            enter(&guard);
            __atomic_store_n(&shared->probe_owner_token,work_token,__ATOMIC_RELEASE);
            if (!pipe_byte(child->ready[1],1,'O')) _exit(101);
            if (raise(SIGSTOP)) _exit(102);
            if (!pipe_byte(child->command[0],0,'R')) _exit(103);
            msync_coord_leave(&guard);
            __atomic_store_n(&shared->probe_owner_released,1,__ATOMIC_RELEASE);
        }
        else
        {
            (void)fd();
            __atomic_store_n(&shared->probe_waiter_token,work_token,__ATOMIC_RELEASE);
            if (!pipe_byte(child->ready[1],1,'W')) _exit(104);
            if (!pipe_byte(child->command[0],0,'G')) _exit(105);
            enter(&guard);
            __atomic_store_n(&shared->probe_acquired,1,__ATOMIC_RELEASE);
            msync_coord_leave(&guard);
        }
        _exit(0);
    }
    close_fd(&child->ready[1]); close_fd(&child->command[0]);
    if (failures != before) return 0;
    if (!pipe_byte(child->ready[0],0,holder ? 'O' : 'W'))
    {
        fail("gate child readiness/lease marker absent"); return 0;
    }
    return !holder || wait_stopped(child->pid);
}
static int unclaimed_owner(uint64_t token)
{
    if (!token || __atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST) != token ||
        __atomic_load_n(&shared->probe_acquired,__ATOMIC_ACQUIRE))
    {
        fail("live lease owner was reclaimed or contender entered before release"); return 0;
    }
    return 1;
}
static int observe_contention(uint64_t owner)
{
    uint64_t start,current,waiter;
    if (!now_ns(&start)) return 0;
    for (;;)
    {
        if (!unclaimed_owner(owner)) return 0;
        waiter = __atomic_load_n(&shared->probe_waiter_token,__ATOMIC_ACQUIRE);
        if (!waiter || waiter == owner) { fail("contender lacks an independent lease token"); return 0; }
        if (__atomic_load_n(&coord.header->wait_hint,__ATOMIC_SEQ_CST)) return 1;
        if (!now_ns(&current)) return 0;
        if (current-start >= (uint64_t)PIPE_MS*1000000) { fail("contender did not publish slow-path wait hint"); return 0; }
        (void)poll(NULL,0,1);
    }
}
static int observe_held(uint64_t owner,unsigned int milliseconds,uint64_t *elapsed)
{
    uint64_t start,current;
    if (!now_ns(&start)) return 0;
    for (;;)
    {
        if (!unclaimed_owner(owner) || !now_ns(&current)) return 0;
        *elapsed = current-start;
        if (*elapsed >= (uint64_t)milliseconds*1000000) return 1;
        (void)poll(NULL,0,1);
    }
}
static void stopped_live_owner_case(void)
{
    struct gate_child owner = {-1,{-1,-1},{-1,-1}}, contender = {-1,{-1,-1},{-1,-1}};
    uint64_t token, stopped_ns = 0, resumed_ns = 0;
    int before = failures;
    test_name = "stopped-live-owner-is-not-reclaimed"; reset();
    if (!start_gate_child(&owner,1)) goto done;
    token = __atomic_load_n(&shared->probe_owner_token,__ATOMIC_ACQUIRE);
    if (!unclaimed_owner(token)) goto done;
    if (__atomic_load_n(&coord.header->wait_hint,__ATOMIC_SEQ_CST)) { fail("unexpected initial wait hint"); goto done; }
    if (!start_gate_child(&contender,0)) goto done;
    if (!pipe_byte(contender.command[1],1,'G')) { fail("start contender"); goto done; }
    if (!observe_contention(token) || !observe_held(token,LIVE_OWNER_HOLD_MS,&stopped_ns)) goto done;
    if (!send_owned_signal(owner.pid,SIGCONT)) goto done;
    /* SIGCONT alone is not permission to release: the owner now waits for R. */
    if (!observe_held(token,10,&resumed_ns)) goto done;
    if (!pipe_byte(owner.command[1],1,'R')) { fail("explicit owner release"); goto done; }
    close_fd(&owner.command[1]);
    if (!wait_child(owner.pid,0) || !wait_child(contender.pid,0)) goto done;
    CHECK(__atomic_load_n(&shared->probe_owner_released,__ATOMIC_ACQUIRE) == 1,"owner release marker absent");
    CHECK(__atomic_load_n(&shared->probe_acquired,__ATOMIC_ACQUIRE) == 1,"contender did not progress after release");
    CHECK(!__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST),"gate still owned after both children exited");
    emit("{\"kind\":\"coordinator-live-owner\",\"ownerStopConfirmed\":true,\"requiredStoppedMs\":%u,\"observedStoppedNs\":%" PRIu64 ",\"observedResumedBeforeReleaseNs\":%" PRIu64 ",\"contenderEnteredAfterRelease\":%s,\"scope\":\"native header lease/gate only; no Wine atfork coverage\"}\n",LIVE_OWNER_HOLD_MS,stopped_ns,resumed_ns,__atomic_load_n(&shared->probe_acquired,__ATOMIC_ACQUIRE) == 1 ? "true" : "false");
done:
    cleanup_gate_child(&contender); cleanup_gate_child(&owner); report(before);
}
static void killed_contending_waiter_case(void)
{
    struct gate_child victim = {-1,{-1,-1},{-1,-1}}, successor = {-1,{-1,-1},{-1,-1}};
    struct msync_coord_guard guard;
    uint64_t elapsed = 0, sequence, token;
    uint32_t wake_after_release, quiet_wake_delta;
    unsigned int i;
    int before = failures, held = 0, status = 0;
    test_name = "killed-contending-waiter-preserves-later-progress"; reset();
    if (!start_gate_child(&victim,0)) goto done;
    /* Fork while idle, then admit the parent owner BEFORE sending G. */
    enter(&guard); held = 1; token = work_token;
    if (__atomic_load_n(&coord.header->wait_hint,__ATOMIC_SEQ_CST)) { fail("unexpected initial victim wait hint"); goto done; }
    if (!pipe_byte(victim.command[1],1,'G')) { fail("start victim contender"); goto done; }
    if (!observe_contention(token) || !observe_held(token,10,&elapsed)) goto done;
    if (!signal_owned_child(victim.pid)) goto done;
    if (reap_for(victim.pid,&status) != 1) { fail("killed waiter reap deadline"); goto done; }
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
    {
        fail("waiter was not killed while contending"); goto done;
    }
    if (!unclaimed_owner(token)) goto done;
    msync_coord_leave(&guard); held = 0;
    CHECK(!__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST),"owner not released after waiter death");
    CHECK(!__atomic_load_n(&coord.header->wait_hint,__ATOMIC_SEQ_CST),"dead waiter left a persistent wait hint");
    wake_after_release = __atomic_load_n(&coord.header->wake_sequence,__ATOMIC_SEQ_CST);
    sequence = journal_serial();
    for (i=0;i<QUIET_TRANSACTIONS;++i)
    {
        set_pair(i & 1u,i & 1u,0);
        CHECK(SLOT(1) == (i & 1u) && SLOT(2) == SLOT(1),"post-death transaction values");
        CHECK(!__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST),"post-death owner stuck");
    }
    quiet_wake_delta = __atomic_load_n(&coord.header->wake_sequence,__ATOMIC_SEQ_CST)-wake_after_release;
    CHECK(quiet_wake_delta == 0,"dead waiter caused persistent quiescent wake-sequence updates");
    CHECK(journal_serial() == sequence+QUIET_TRANSACTIONS,"post-death commits not completed exactly once");
    cleanup_gate_child(&victim);
    if (failures != before) goto done;
    if (!start_gate_child(&successor,0)) goto done;
    enter(&guard); held = 1; token = work_token;
    if (__atomic_load_n(&coord.header->wait_hint,__ATOMIC_SEQ_CST)) { fail("successor wait hint was not fresh"); goto done; }
    if (!pipe_byte(successor.command[1],1,'G')) { fail("start successor contender"); goto done; }
    if (!observe_contention(token) || !observe_held(token,10,&elapsed)) goto done;
    msync_coord_leave(&guard); held = 0;
    if (!wait_child(successor.pid,0)) goto done;
    CHECK(__atomic_load_n(&shared->probe_acquired,__ATOMIC_ACQUIRE) == 1,"successor did not progress after wake");
    CHECK(!__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST),"successor left gate owned");
    CHECK(!__atomic_load_n(&coord.header->wait_hint,__ATOMIC_SEQ_CST),"successor left a stale wait hint");
    emit("{\"kind\":\"coordinator-waiter-death\",\"victimSigkillConfirmed\":true,\"quietTransactions\":%u,\"quietWakeSequenceDelta\":%" PRIu32 ",\"successorEnteredAfterRelease\":%s,\"scope\":\"native header lease/gate only; no Wine atfork coverage\"}\n",QUIET_TRANSACTIONS,quiet_wake_delta,__atomic_load_n(&shared->probe_acquired,__ATOMIC_ACQUIRE) == 1 ? "true" : "false");
done:
    cleanup_gate_child(&successor); cleanup_gate_child(&victim);
    if (held) msync_coord_leave(&guard);
    report(before);
}
struct stress_args { unsigned int iterations, completed; uint64_t consumed,released; };
static void *stress_worker(void *opaque)
{
    struct stress_args *args = opaque;
    unsigned int i;
    struct msync_coord_guard guard;
    for (i=0;i<args->iterations && !__atomic_load_n(&shared->stop,__ATOMIC_ACQUIRE);++i)
    {
        struct msync_coord_entry entries[2];
        enter(&guard);
        if (SLOT(1) != SLOT(2) || SLOT(1) > 1)
        {
            __atomic_store_n(&shared->worker_error,93,__ATOMIC_RELEASE);
            __atomic_store_n(&shared->stop,1,__ATOMIC_RELEASE);
            msync_coord_leave(&guard); break;
        }
        if (SLOT(1))
        {
            entries[0]=(struct msync_coord_entry){1,0,0}; entries[1]=(struct msync_coord_entry){2,0,0};
            msync_coord_commit(&coord,entries,2); ++args->consumed;
        }
        finish(&guard); enter(&guard);
        if (!SLOT(1))
        {
            entries[0]=(struct msync_coord_entry){1,0,1}; entries[1]=(struct msync_coord_entry){2,0,1};
            msync_coord_commit(&coord,entries,2); ++args->released;
        }
        if (SLOT(1) > 1 || SLOT(2) != SLOT(1))
        {
            __atomic_store_n(&shared->worker_error,92,__ATOMIC_RELEASE);
            __atomic_store_n(&shared->stop,1,__ATOMIC_RELEASE);
        }
        finish(&guard); ++args->completed;
    }
    return NULL;
}
static void stress_case(unsigned int threads,unsigned int iterations)
{
    pthread_t workers[16];
    struct stress_args args[16] = {{0}};
    uint64_t consumed=0,released=0,start=0,end=0;
    unsigned int i,created=0;
    int before=failures;
    test_name = "native-thread-transaction-stress"; reset();
    if (!threads || threads > 16 || !now_ns(&start)) { fail("invalid thread stress setup"); report(before); return; }
    for (i=0;i<threads;++i)
    {
        args[i].iterations=iterations;
        if (pthread_create(&workers[i],NULL,stress_worker,&args[i]))
        {
            fail("pthread_create"); __atomic_store_n(&shared->stop,1,__ATOMIC_RELEASE); break;
        }
        ++created;
    }
    for (i=0;i<created;++i)
    {
        /* Independent watchdog bounds a join/coordinator deadlock. Never read
         * worker memory or unmap shared state following a failed join. */
        if (pthread_join(workers[i],NULL)) { fail("pthread_join"); abort_owned_session(session_owner); }
        consumed+=args[i].consumed; released+=args[i].released;
        CHECK(args[i].completed == iterations,"thread iterations incomplete");
    }
    CHECK(created == threads,"requested thread count not created");
    CHECK(!__atomic_load_n(&shared->worker_error,__ATOMIC_ACQUIRE),"thread invariant failure");
    CHECK(SLOT(1) == 1+released-consumed && SLOT(1) <= 1 && SLOT(2) == SLOT(1),"thread permit/pair accounting");
    if (now_ns(&end))
        emit("{\"kind\":\"coordinator-stress\",\"threads\":%u,\"iterationsPerThread\":%u,\"consumed\":%" PRIu64 ",\"released\":%" PRIu64 ",\"seconds\":%.9f,\"timingScope\":\"thread_create_through_join\"}\n",threads,iterations,consumed,released,(double)(end-start)/1e9);
    report(before);
}
static void process_stress_case(void)
{
    struct stress_args *args = NULL;
    pid_t pids[4] = {-1,-1,-1,-1};
    unsigned int i,created=0;
    uint64_t consumed=0,released=0;
    int before=failures;
    test_name = "native-process-transaction-stress"; reset();
    args = mmap(NULL,SHARED_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);
    if (args == MAP_FAILED) { args=NULL; fail("process counters mmap"); goto done; }
    for (i=0;i<4;++i)
    {
        args[i].iterations=10000;
        pids[i]=fork_owned();
        if (pids[i] < 0) goto done;
        if (!pids[i]) { stress_worker(&args[i]); _exit(0); }
        ++created;
    }
    for (i=0;i<created;++i)
    {
        if (!wait_child(pids[i],0)) goto done;
        consumed+=args[i].consumed; released+=args[i].released;
        CHECK(args[i].completed == args[i].iterations,"process iterations incomplete");
    }
    CHECK(!__atomic_load_n(&shared->worker_error,__ATOMIC_ACQUIRE),"process invariant failure");
    CHECK(SLOT(1) == 1+released-consumed && SLOT(1) <= 1 && SLOT(1) == SLOT(2),"process permit/pair accounting");
    emit("{\"kind\":\"coordinator-process-stress\",\"processes\":%u,\"consumed\":%" PRIu64 ",\"released\":%" PRIu64 "}\n",created,consumed,released);
done:
    for (i=0;i<4;++i) stop_child(&pids[i]);
    if (args) CHECK(munmap(args,SHARED_SIZE) == 0,"process counters munmap");
    report(before);
}
static int compare_double(const void *a,const void *b)
{
    double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y);
}
static void bench(void)
{
    const unsigned int batches=101,iterations=1000;
    double samples[101];
    uint64_t start,end;
    unsigned int b,i;
    int before=failures;
    struct msync_coord_guard guard;
    struct msync_coord_entry entries[2]={{1,0,1},{2,0,1}};
    uint64_t sequence;
    test_name = "native-two-slot-coordinator-benchmark"; reset();
    sequence = journal_serial();
    for (i=0;i<1000;++i)
    {
        entries[0].value=entries[1].value=i & 1u;
        enter(&guard); msync_coord_commit(&coord,entries,2); finish(&guard);
    }
    for (b=0;b<batches;++b)
    {
        if (!now_ns(&start)) goto done;
        for (i=0;i<iterations;++i)
        {
            /* The production header elides unchanged entries. Alternate both
             * values so every reported transaction really changes two slots. */
            entries[0].value=entries[1].value=i & 1u;
            enter(&guard); msync_coord_commit(&coord,entries,2); finish(&guard);
        }
        if (!now_ns(&end)) goto done;
        if (end < start) { fail("benchmark clock regressed"); goto done; }
        samples[b]=(double)(end-start)/iterations;
    }
    qsort(samples,batches,sizeof(*samples),compare_double);
    CHECK(SLOT(1) == 1 && SLOT(2) == 1,"benchmark final alternating values");
    CHECK(journal_serial() == sequence + 102000,"benchmark transactions were elided or duplicated");
    emit("{\"kind\":\"coordinator-bench\",\"workload\":\"two-slot-alternating-transaction\",\"batchCount\":101,\"transactionsPerBatch\":1000,\"warmupTransactions\":1000,\"measuredTransactions\":101000,\"medianBatchAverageNsPerTransaction\":%.3f,\"p95BatchAverageNsPerTransaction\":%.3f,\"maxBatchAverageNsPerTransaction\":%.3f,\"distribution\":\"101 batch averages, not individual latency tails\",\"scope\":\"production coordinator enter/commit/leave only; notifications disabled; setup and warmup excluded\"}\n",samples[50],samples[95],samples[100]);
done:
    report(before);
}
static void teardown_children(void)
{
    unsigned int i;
    for (i=0;i<MAX_CHILDREN;++i)
        if (children[i] > 1 && children[i] != watchdog_pid)
        {
            pid_t child = children[i];
            fail("unexpected live/unreaped case child at teardown"); stop_child(&child);
        }
}
static void stop_watchdog(void)
{
    if (watchdog_pid > 1 && owned_index(watchdog_pid) >= 0)
    {
        if (!pipe_byte(watchdog_pipe[1],1,'D'))
        {
            fail("disarm watchdog"); stop_child(&watchdog_pid);
        }
        else
        {
            close_fd(&watchdog_pipe[1]);
            (void)wait_child(watchdog_pid,0); watchdog_pid = -1;
        }
    }
    close_fd(&watchdog_pipe[0]); close_fd(&watchdog_pipe[1]);
}
#ifdef CIDER_TEST_MSYNC_TAGGED_DECISION
static void tagged_serial_boundary_case(void)
{
    struct msync_coord_guard guard;
    struct msync_coord_entry entry = {1,0,UINT64_C(0x77112233)};
    uint64_t before_value=__atomic_load_n(&SLOT(1),__ATOMIC_ACQUIRE);
    pid_t child;
    int before=failures;
    test_name="tagged-serial-boundary";
    /* Last case, no live workers/helpers: jump forward only, never reset a
     * serial. The child supplies a constructed maximum-serial CLEAN input. */
    child=fork_owned();
    if (child < 0) return;
    if (!child)
    {
        enter(&guard);
        __atomic_store_n(&coord.header->decision,MSYNC_COORD_SERIAL_MAX<<2,__ATOMIC_RELEASE);
        msync_coord_commit(&coord,&entry,1); /* must fail before a write/wrap */
        msync_coord_finish(&guard);
        _exit(99);
    }
    if (!wait_child(child,1)) return;
    enter(&guard); /* reclaim the genuinely dead child's owner lease */
    CHECK(journal_phase()==MSYNC_COORD_CLEAN && journal_serial()==MSYNC_COORD_SERIAL_MAX,"serial maximum wrapped or journal changed");
    CHECK(__atomic_load_n(&SLOT(1),__ATOMIC_ACQUIRE)==before_value,"overflow commit wrote payload");
    msync_coord_leave(&guard);
    emit("{\"kind\":\"coordinator-serial-boundary\",\"wrapAccepted\":false,\"serial\":%" PRIu64 "}\n",journal_serial());
    report(before);
}
#endif

#ifdef CIDER_TEST_MSYNC_SNAPSHOT
static void snapshot_contract_case(void)
{
    struct msync_coord_guard guard;
    struct msync_coord_snapshot view = {0};
    struct msync_coord_entry entry = {6,0,UINT64_C(1)<<32};
    unsigned int state;
    const unsigned int tid = 77;
    uint64_t *epochs = msync_coord_epochs(&coord);
    test_name="snapshot-contract";
    __atomic_store_n(&epochs[tid],2,__ATOMIC_RELEASE);
    enter(&guard); msync_coord_commit(&coord,&entry,1); finish(&guard);
    CHECK(msync_coord_snapshot_begin(&coord,tid,2,&view),"clean matching epoch begins");
    CHECK((uint32_t)__atomic_load_n(&SLOT(6),__ATOMIC_ACQUIRE)==0,"low 32 bits retain zero with nonzero semaphore maximum");
    CHECK(msync_coord_snapshot_end(&coord,&view),"unchanged clean view completes");
    enter(&guard);
    CHECK(!msync_coord_snapshot_end(&coord,&view),"held owner rejects previously clean view");
    CHECK(!msync_coord_snapshot_begin(&coord,tid,2,&view),"held owner rejects begin");
    finish(&guard);
    /* Single-threaded fixture: only the independent timeout watchdog is live.
     * No worker reads state while these guard rejection cases are staged. */
    for (state=MSYNC_COORD_PREPARING;state<=MSYNC_COORD_APPLIED;++state)
    {
        CHECK(msync_coord_snapshot_begin(&coord,tid,2,&view),"state test starts clean");
        set_journal_phase(state);
        CHECK(!msync_coord_snapshot_end(&coord,&view),"pending state rejects end");
        CHECK(!msync_coord_snapshot_begin(&coord,tid,2,&view),"pending state rejects begin");
        set_journal_phase(MSYNC_COORD_CLEAN);
    }
    CHECK(msync_coord_snapshot_begin(&coord,tid,2,&view),"sequence test starts clean");
    entry.value|=1;
    enter(&guard); msync_coord_commit(&coord,&entry,1); finish(&guard);
    CHECK(!msync_coord_snapshot_end(&coord,&view),"completed transaction rejects old view even after owner and state clean");
    CHECK(!msync_coord_snapshot_begin(&coord,tid,0,&view),"zero epoch rejects begin");
    CHECK(!msync_coord_snapshot_begin(&coord,tid,3,&view),"retired odd epoch rejects begin");
    CHECK(!msync_coord_snapshot_begin(&coord,MSYNC_COORD_TIDS,2,&view),"out of bounds tid rejects begin");
    CHECK(msync_coord_snapshot_begin(&coord,tid,2,&view),"epoch test starts matching");
    __atomic_store_n(&epochs[tid],4,__ATOMIC_RELEASE);
    CHECK(!msync_coord_snapshot_end(&coord,&view),"reused tid rejects old view");
    CHECK(!msync_coord_snapshot_begin(&coord,tid,2,&view),"reused tid rejects old begin");
    CHECK(msync_coord_snapshot_begin(&coord,tid,4,&view),"new matching incarnation begins");
    CHECK(msync_coord_snapshot_end(&coord,&view),"new matching incarnation completes");
    __atomic_store_n(&epochs[tid],0,__ATOMIC_RELEASE);
    emit("{\"kind\":\"snapshot-contract\",\"scope\":\"production header view validation only; not Windows equivalence\",\"failures\":%d}\n",failures);
}

#endif

int main(int argc,char **argv)
{
    char path[PATH_MAX] = "";
    int length, path_created=0;
    struct sigaction action;
    session_owner = getpid();
    memset(&action,0,sizeof(action)); sigemptyset(&action.sa_mask); action.sa_handler=SIG_IGN;
    if (sigaction(SIGPIPE,&action,NULL)) { fail("ignore SIGPIPE"); goto cleanup; }
    action.sa_handler=SIG_DFL;
    if (sigaction(SIGCHLD,&action,NULL)) { fail("restore waitable SIGCHLD"); goto cleanup; }
    if (session_owner <= 1 || getsid(0) != session_owner || getpgrp() != session_owner)
    {
        fail("requires private session/group: wrapper must use start_new_session=True"); goto cleanup;
    }
    if (argc != 2) { fail("usage: msync-coordinator existing-owned-directory"); goto cleanup; }
    length=snprintf(path,sizeof(path),"%s/coordinator-XXXXXX",argv[1]);
    if (length < 0 || (size_t)length >= sizeof(path)) { fail("owned temporary path length"); goto cleanup; }
    identity_fd=mkstemp(path);
    if (identity_fd < 0) { fail("mkstemp"); goto cleanup; }
    path_created=1;
    if (fchmod(identity_fd,0600) || ftruncate(identity_fd,(off_t)MSYNC_COORD_SIZE)) { fail("identity file setup"); goto cleanup; }
    coord.header=mmap(NULL,MSYNC_COORD_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,identity_fd,0);
    if (coord.header == MAP_FAILED) { coord.header=NULL; fail("coordinator mmap"); goto cleanup; }
    shared=mmap(NULL,SHARED_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);
    if (shared == MAP_FAILED) { shared=NULL; fail("shared observations mmap"); goto cleanup; }
    {
        long page_size=sysconf(_SC_PAGESIZE);
        if (page_size <= 0) { fail("slot page size"); goto cleanup; }
        slot_page_size=(size_t)page_size; slot_memory_size=SLOT_COUNT*slot_page_size;
        slot_memory=mmap(NULL,slot_memory_size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);
        if (slot_memory == MAP_FAILED) { slot_memory=NULL; fail("slot mmap"); goto cleanup; }
    }
    coord.header->magic=MSYNC_COORD_MAGIC; coord.slot=slot; coord.notify=notify;
    if (pthread_key_create(&fd_key,close_work)) { fail("pthread_key_create"); goto cleanup; }
    key_created=1;
    if (!start_watchdog()) goto cleanup;
    emit("{\"kind\":\"coordinator-begin\",\"sessionLeader\":%ld,\"watchdogTimeoutMs\":%d,\"scope\":\"exact production coordinator header, native only\"}\n",(long)session_owner,WATCHDOG_MS);
    flush_output();
    if (output_error) goto cleanup;
    (void)fd();
#ifdef CIDER_TEST_MSYNC_SNAPSHOT
    snapshot_contract_case();
#endif
    filtered_commit_case();
    single_recovery_case(0); single_recovery_case(1); single_recovery_case(2);
    single_noop_and_queued_wake_case();
    simple_recovery(0); simple_recovery(1); killed_owner_case(); simple_recovery(2); stale_helper_case();
    stopped_live_owner_case(); killed_contending_waiter_case();
    stress_case(4,10000); stress_case(8,10000); process_stress_case(); bench();
#ifdef CIDER_TEST_MSYNC_TAGGED_DECISION
    tagged_serial_boundary_case();
#endif
#ifdef CIDER_TEST_MSYNC_DEFER_NOTIFY
    CHECK(__atomic_load_n(&shared->notify_scope_calls,__ATOMIC_RELAXED)>0,"notification scope was exercised");
    CHECK(!__atomic_load_n(&shared->notify_under_owner,__ATOMIC_RELAXED),"notification callback ran under its own transaction gate");
    emit("{\"kind\":\"coordinator-notify-scope\",\"calls\":%" PRIu64 ",\"underOwnGate\":%s}\n",
         __atomic_load_n(&shared->notify_scope_calls,__ATOMIC_RELAXED),
         __atomic_load_n(&shared->notify_under_owner,__ATOMIC_RELAXED) ? "true" : "false");
#endif
cleanup:
    test_name="teardown";
    teardown_children();
    if (key_created)
    {
        CHECK(pthread_setspecific(fd_key,NULL) == 0,"clear main TLS key");
        close_work(work_fd >= 0 ? (void *)(intptr_t)(work_fd+1) : NULL);
        CHECK(pthread_key_delete(fd_key) == 0,"pthread_key_delete"); key_created=0;
    }
    if (shared)
    {
        CHECK(!__atomic_load_n(&shared->close_errors,__ATOMIC_ACQUIRE),"thread/main lease fd close errors");
        CHECK(munmap(shared,SHARED_SIZE) == 0,"shared observations munmap"); shared=NULL;
    }
    if (slot_memory)
    {
        CHECK(munmap(slot_memory,slot_memory_size) == 0,"slot pages munmap"); slot_memory=NULL;
    }
    if (coord.header)
    {
        CHECK(munmap(coord.header,MSYNC_COORD_SIZE) == 0,"coordinator munmap"); coord.header=NULL;
    }
    close_fd(&identity_fd);
    if (path_created) CHECK(unlink(path) == 0,"unlink owned identity file");
    stop_watchdog();
    flush_output();
    emit("{\"kind\":\"coordinator-complete\",\"failures\":%d,\"outputError\":%s,\"passed\":%s,\"teardownChecked\":true}\n",failures,output_error ? "true" : "false",!failures && !output_error ? "true" : "false");
    flush_output();
    return failures || output_error ? 1 : 0;
}
