/* Exact production magic7 single-waiter helpers; native Darwin protocol only.
 * Build against the candidate include/ directory. No Wine/WinAPI, performance,
 * Mach-pump or complete transaction-recovery acceptance is supplied here.
 *
 * Launch in a fresh session/group, with one existing owned temporary directory:
 *   msync-single-waiter DIRECTORY [all|balance|sticky|fork|death|negative|aba]
 * pid == sid == pgid is required. An independent 120-second watchdog kills only
 * that verified private group on a hang/parent death. Missing completion fails.
 * Children are tracked, killed only while waitable, and reaped before teardown.
 *
 * Saturation and invalid-removal metadata are explicitly constructed inputs.
 * sticky-write-race uses a REAL read-only shared counter page: remove faults at
 * its write after reading the old count. A native thread in that child tells
 * the parent to set sticky through the production helper, restores protection,
 * then resumes the faulting instruction. The signal handler never mprotects.
 * ABA uses actual shared ulock: only the child main thread waits; a helper in
 * that same task confirms its exact Mach thread is TH_STATE_WAITING while its
 * phase is aboutToUlock. Parent cannot issue the second Set until that report.
 * Set/consume are native payload transitions under the real coordinator owner,
 * not claimed Windows event operations. Fault/write and park observations are
 * separate tests; neither substitutes for runtime lifecycle validation.
 */
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <mach/mach.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
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

#define WATCHDOG_MS 120000u
#define IO_MS 3000u
#define REAP_MS 5000u
#define MAX_CHILDREN 8
#define OBS_OFFSET (MSYNC_COORD_HEADER_SIZE - 4096u)
#define UL_SHARED 3u
#define UL_NO_ERRNO 0x01000000u
#define UL_WAKE_ALL 0x100u
#define ABOUT_TO_ULOCK 1u
#define ULOCK_RETURNED 2u

struct observations
{
    uint32_t payload;
    unsigned int phase, observer_error, kernel_park;
    int wait_result, negative_entered, negative_returned;
};
_Static_assert(sizeof(struct observations) <= 4096, "observation page bounds");
_Static_assert(MSYNC_STATS_OFFSET + sizeof(struct msync_coord_statistics) <= OBS_OFFSET,
               "observations must not overlap production header/statistics");
_Static_assert(MSYNC_COORD_HINT_BUCKETS == 65536, "fixture requires magic7 bucket layout");

static struct msync_coord coord;
static struct observations *obs;
static int identity_fd = -1, failures, output_error;
static int watchdog_pipe[2] = {-1,-1};
static pid_t session_owner, watchdog_pid = -1, children[MAX_CHILDREN];
static __thread int work_fd = -1;
static __thread uint64_t work_token;
static const char *case_name = "initialization";

static void fail(const char *message)
{
    ++failures;
    if (fprintf(stderr,"%s: %s (errno=%d)\n",case_name,message,errno) < 0) output_error=1;
}
#define CHECK(c,m) do { if (!(c)) fail(m); } while (0)
static void emit(const char *format,...)
{
    va_list args;
    va_start(args,format);
    if (vprintf(format,args) < 0) output_error=1;
    va_end(args);
}
static void flush_output(void)
{
    if (fflush(stdout) || ferror(stdout)) output_error=1;
}
static void report(int before,const char *mechanism)
{
    emit("{\"type\":\"case\",\"name\":\"%s\",\"mechanism\":\"%s\",\"ok\":%s}\n",
         case_name,mechanism,failures==before ? "true":"false");
    flush_output();
}
/* Also used by the fault handler: only clock_gettime/poll/read/write, no stdio. */
static uint64_t now_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
static int transfer(int fd,int writing,char expected,unsigned int timeout_ms)
{
    uint64_t start=now_ns(), now;
    if (fd<0 || !start) return 0;
    for (;;)
    {
        struct pollfd p={fd,writing ? POLLOUT:POLLIN,0};
        char value=expected;
        ssize_t n;
        int ret, remaining;
        now=now_ns();
        if (!now || now-start >= (uint64_t)timeout_ms*1000000) return 0;
        remaining=(int)(timeout_ms-(now-start)/1000000);
        ret=poll(&p,1,remaining);
        if (ret<0 && errno==EINTR) continue;
        if (ret<=0 || (p.revents & (POLLNVAL|POLLERR))) return 0;
        n=writing ? write(fd,&value,1):read(fd,&value,1);
        if (n==1) return writing || value==expected;
        if (n<0 && (errno==EINTR || errno==EAGAIN)) continue;
        return 0;
    }
}
static void close_fd(int *fd)
{
    int value=*fd;
    *fd=-1;
    if (value>=0) CHECK(!close(value),"close owned fd");
}
static int make_pipe(int p[2])
{
    unsigned int i;
    p[0]=p[1]=-1;
    if (pipe(p)) { fail("pipe"); return 0; }
    for (i=0;i<2;++i)
    {
        int flags=fcntl(p[i],F_GETFL);
        if (flags<0 || fcntl(p[i],F_SETFL,flags|O_NONBLOCK)<0 || fcntl(p[i],F_SETFD,FD_CLOEXEC)<0)
        { fail("pipe flags"); close_fd(&p[0]); close_fd(&p[1]); return 0; }
    }
    return 1;
}
static void close_pipe(int p[2]) { close_fd(&p[0]); close_fd(&p[1]); }
static void abort_owned_session(void)
{
    if (session_owner>1 && getsid(0)==session_owner && getpgrp()==session_owner)
        (void)kill(-session_owner,SIGKILL);
    _exit(124);
}
static int child_index(pid_t pid)
{
    int i;
    if (pid<=1) return -1;
    for (i=0;i<MAX_CHILDREN;++i) if (children[i]==pid) return i;
    return -1;
}
static int reap_for(pid_t pid,int *status)
{
    uint64_t start=now_ns(), now;
    int index=child_index(pid);
    if (index<0 || !start) return -1;
    for (;;)
    {
        pid_t ret=waitpid(pid,status,WNOHANG);
        if (ret==pid) { children[index]=0; return 1; }
        if (ret<0 && errno!=EINTR)
        { if (errno==ECHILD) children[index]=0; return -1; }
        now=now_ns();
        if (!now || now-start >= (uint64_t)REAP_MS*1000000) return 0;
        (void)poll(NULL,0,1);
    }
}
static int signal_child(pid_t pid)
{
    pid_t sid,pgid;
    if (child_index(pid)<0) { fail("refuse non-owned child signal"); return 0; }
    sid=getsid(pid); pgid=getpgid(pid);
    if ((sid<0 || pgid<0) && errno==ESRCH) return 1;
    if (sid!=session_owner || pgid!=session_owner) { fail("child escaped private session"); return 0; }
    if (kill(pid,SIGKILL) && errno!=ESRCH) { fail("kill owned child"); return 0; }
    return 1;
}
static void stop_child(pid_t *pid)
{
    int status;
    if (child_index(*pid)>=0)
    {
        (void)signal_child(*pid);
        if (reap_for(*pid,&status)!=1) { fail("owned child unreapable"); abort_owned_session(); }
    }
    *pid=-1;
}
static int expect_child(pid_t *pid,int code,int killed)
{
    int status=0,ok;
    if (reap_for(*pid,&status)!=1)
    { fail("child deadline/reap"); stop_child(pid); return 0; }
    *pid=-1;
    ok=killed ? WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL:
                WIFEXITED(status) && WEXITSTATUS(status)==code;
    if (!ok) fail("child exit did not match required outcome");
    return ok;
}
static void close_work(void)
{
    int fd=work_fd;
    work_fd=-1; work_token=0;
    if (fd>=0 && close(fd)) _exit(86);
}
static void child_reset(void)
{
    close_work();
    if (watchdog_pipe[1]>=0 && close(watchdog_pipe[1])) _exit(87);
    watchdog_pipe[1]=-1;
}
static pid_t fork_owned(void)
{
    int i;
    pid_t pid;
    for (i=0;i<MAX_CHILDREN && children[i];++i) ;
    if (i==MAX_CHILDREN) { fail("child table full"); return -1; }
    pid=fork();
    if (!pid) { child_reset(); return 0; }
    if (pid<0) { fail("fork"); return -1; }
    children[i]=pid;
    return pid;
}
static int start_watchdog(void)
{
    int i;
    for (i=0;i<MAX_CHILDREN && children[i];++i) ;
    if (i==MAX_CHILDREN || !make_pipe(watchdog_pipe)) return 0;
    watchdog_pid=fork();
    if (!watchdog_pid)
    {
        if (close(watchdog_pipe[1])) abort_owned_session();
        if (identity_fd>=0 && close(identity_fd)) abort_owned_session();
        if (!transfer(watchdog_pipe[0],0,'D',WATCHDOG_MS)) abort_owned_session();
        if (close(watchdog_pipe[0])) _exit(88);
        _exit(0);
    }
    if (watchdog_pid<0) { fail("fork watchdog"); close_pipe(watchdog_pipe); return 0; }
    children[i]=watchdog_pid;
    close_fd(&watchdog_pipe[0]);
    return 1;
}
static void enter(struct msync_coord_guard *guard)
{
    if (work_fd<0)
    {
        work_fd=msync_coord_open(identity_fd);
        work_token=msync_coord_new_lease(&coord,work_fd);
    }
    msync_coord_enter(&coord,work_fd,work_token,guard);
}
static uint32_t count(unsigned int idx)
{
    return __atomic_load_n(msync_coord_hint(&coord,idx),__ATOMIC_SEQ_CST);
}
static void balance_case(void)
{
    struct msync_coord_guard guard;
    struct msync_single_waiter a={0},b={0},*head=NULL;
    int before=failures;
    case_name="collision-and-balanced-publication";
    CHECK(msync_coord_hint(&coord,1)==msync_coord_hint(&coord,65537),"1/65537 must collide");
    enter(&guard);
    msync_coord_single_publish(&guard,&head,&a,1);
    msync_coord_single_publish(&guard,&head,&b,65537);
    CHECK(head==&b && b.prev==&a && count(1)==2,"two live publications");
    CHECK(msync_coord_event_waiters(&coord,1) && msync_coord_event_waiters(&coord,65537),"both colliding objects advertise");
    CHECK(count(1)==2,"notification query must not consume count");
    msync_coord_leave(&guard);
    if (count(1)==2)
    {
        msync_coord_single_remove(&head,&b);
        CHECK(count(1)==1 && head==&a && !b.counter,"remove only one's own count");
        if (count(1)==1) msync_coord_single_remove(&head,&a);
    }
    CHECK(!head && !count(1) && !a.counter,"last removal clears count and stack");
    CHECK(!msync_coord_event_waiters(&coord,65537),"empty collision bucket");
    report(before,"actual production helper calls; two real stack registrations");
}
static void sticky_case(void)
{
    struct msync_coord_guard guard;
    struct msync_single_waiter a={0},*head=NULL;
    int before=failures;
    case_name="saturation-and-unknown-sticky";
    /* Constructed boundary, not billions of actual simultaneous waiters. */
    __atomic_store_n(msync_coord_hint(&coord,3),UINT32_MAX-1,__ATOMIC_SEQ_CST);
    enter(&guard); msync_coord_single_publish(&guard,&head,&a,3); msync_coord_leave(&guard);
    CHECK(count(3)==UINT32_MAX,"saturation must not wrap");
    msync_coord_single_remove(&head,&a);
    CHECK(count(3)==UINT32_MAX && !head,"sticky removal remains sticky");
    enter(&guard); msync_coord_single_publish(&guard,&head,&a,3); msync_coord_leave(&guard);
    CHECK(count(3)==UINT32_MAX,"sticky publication remains sticky");
    msync_coord_single_remove(&head,&a);
    enter(&guard); msync_coord_single_unknown(&guard,4); msync_coord_leave(&guard);
    CHECK(count(4)==UINT32_MAX && msync_coord_event_waiters(&coord,4),"unknown input forces conservative wake");
    report(before,"constructed UINT32_MAX-1 boundary plus actual helper operations");
}

static int fault_write=-1,fault_resume=-1;
static uintptr_t protected_begin;
static size_t protected_size;
static volatile sig_atomic_t removing, fault_seen;
static void remove_fault(int sig,siginfo_t *info,void *context)
{
    int saved=errno;
    uintptr_t address=(uintptr_t)info->si_addr;
    (void)sig; (void)context;
    if (!removing || fault_seen || address<protected_begin || address-protected_begin>=protected_size) _exit(91);
    fault_seen=1;
    if (!transfer(fault_write,1,'F',IO_MS) || !transfer(fault_resume,0,'A',IO_MS)) _exit(92);
    errno=saved;
}
struct fault_observer_args { int fault_read,resume_write,parent_write,parent_read; };
static void *fault_observer(void *opaque)
{
    struct fault_observer_args *a=opaque;
    int ok=transfer(a->fault_read,0,'F',IO_MS) && transfer(a->parent_write,1,'S',IO_MS) &&
           transfer(a->parent_read,0,'T',IO_MS);
    if (mprotect((void *)protected_begin,protected_size,PROT_READ|PROT_WRITE)) ok=0;
    if (!ok) __atomic_store_n(&obs->observer_error,1,__ATOMIC_RELEASE);
    if (!transfer(a->resume_write,1,'A',IO_MS)) __atomic_store_n(&obs->observer_error,1,__ATOMIC_RELEASE);
    return NULL;
}
static void sticky_race_case(void)
{
    int to_child[2]={-1,-1},from_child[2]={-1,-1},before=failures;
    pid_t pid=-1;
    case_name="sticky-versus-paused-old-remove";
    obs->observer_error=0;
    if (!make_pipe(to_child) || !make_pipe(from_child)) goto done;
    pid=fork_owned();
    if (!pid)
    {
        struct msync_coord_guard guard;
        struct msync_single_waiter waiter={0},*head=NULL;
        struct sigaction action={0};
        struct fault_observer_args args;
        int fault[2],resume[2];
        long page=sysconf(_SC_PAGESIZE);
        pthread_t observer;
        close(to_child[1]); close(from_child[0]);
        if (page<=0 || ((unsigned long)page & ((unsigned long)page-1))) _exit(93);
        enter(&guard); msync_coord_single_publish(&guard,&head,&waiter,5); msync_coord_leave(&guard);
        if (count(5)!=1 || !make_pipe(fault) || !make_pipe(resume)) _exit(94);
        protected_size=(size_t)page;
        protected_begin=(uintptr_t)waiter.counter & ~((uintptr_t)page-1);
        fault_write=fault[1]; fault_resume=resume[0];
        action.sa_sigaction=remove_fault; action.sa_flags=SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGBUS,&action,NULL) || sigaction(SIGSEGV,&action,NULL)) _exit(95);
        args=(struct fault_observer_args){fault[0],resume[1],from_child[1],to_child[0]};
        if (pthread_create(&observer,NULL,fault_observer,&args)) _exit(96);
        if (mprotect((void *)protected_begin,protected_size,PROT_READ)) _exit(97);
        removing=1;
        msync_coord_single_remove(&head,&waiter);
        removing=0;
        if (pthread_join(observer,NULL)) _exit(98); /* independent watchdog also bounds joins */
        if (!fault_seen || head || waiter.counter || count(5)!=UINT32_MAX || obs->observer_error) _exit(99);
        close_pipe(fault); close_pipe(resume); close_work();
        _exit(failures ? 100:0);
    }
    if (pid<0) goto done;
    close_fd(&to_child[0]); close_fd(&from_child[1]);
    if (!transfer(from_child[0],0,'S',IO_MS)) { fail("remove did not fault at its write"); goto done; }
    CHECK(count(5)==1,"parent observes old count before concurrent sticky");
    {
        struct msync_coord_guard guard;
        enter(&guard); msync_coord_single_unknown(&guard,5); msync_coord_leave(&guard);
    }
    CHECK(count(5)==UINT32_MAX,"parent production unknown set sticky");
    if (!transfer(to_child[1],1,'T',IO_MS)) { fail("resume paused old decrement"); goto done; }
    (void)expect_child(&pid,0,0);
    CHECK(count(5)==UINT32_MAX && !obs->observer_error,"old decrement must not overwrite concurrent sticky");
done:
    stop_child(&pid); close_pipe(to_child); close_pipe(from_child);
    report(before,"actual remove write fault and cross-process production sticky store");
}
static void fork_case(void)
{
    struct msync_coord_guard guard;
    struct msync_single_waiter waiter={0},*head=NULL;
    pid_t pid=-1;
    int before=failures;
    case_name="fork-child-drops-inherited-head";
    enter(&guard); msync_coord_single_publish(&guard,&head,&waiter,6); msync_coord_leave(&guard);
    pid=fork_owned();
    if (!pid)
    {
        msync_coord_single_fork_child(&head);
        _exit(head || count(6)!=1 ? 101:0);
    }
    if (pid>1) (void)expect_child(&pid,0,0);
    CHECK(head==&waiter && count(6)==1,"child must not remove parent's registration");
    if (count(6)==1) msync_coord_single_remove(&head,&waiter);
    CHECK(!head && !count(6),"parent still owns exactly one removal");
    stop_child(&pid);
    report(before,"actual fork and production drop-head helper; not Wine atfork coverage");
}
static void death_case(void)
{
    int p[2]={-1,-1},before=failures;
    pid_t pid=-1;
    case_name="sigkill-conservative-residue";
    if (!make_pipe(p)) goto done;
    pid=fork_owned();
    if (!pid)
    {
        struct msync_coord_guard guard;
        struct msync_single_waiter waiter={0},*head=NULL;
        close(p[0]);
        enter(&guard); msync_coord_single_publish(&guard,&head,&waiter,7); msync_coord_leave(&guard);
        if (!transfer(p[1],1,'R',IO_MS)) _exit(102);
        for (;;) pause(); /* parent requests and verifies this exact child's SIGKILL */
    }
    if (pid<0) goto done;
    close_fd(&p[1]);
    if (!transfer(p[0],0,'R',IO_MS)) { fail("SIGKILL victim registration deadline"); goto done; }
    CHECK(count(7)==1,"victim published before kill");
    if (!signal_child(pid)) goto done;
    (void)expect_child(&pid,0,1);
    CHECK(count(7)==1 && msync_coord_event_waiters(&coord,65543),"killed registration remains conservative across collision");
    {
        struct msync_coord_guard guard;
        struct msync_single_waiter waiter={0},*head=NULL;
        enter(&guard); msync_coord_single_publish(&guard,&head,&waiter,65543); msync_coord_leave(&guard);
        CHECK(count(7)==2,"new colliding registration adds to residue");
        if (count(7)==2) msync_coord_single_remove(&head,&waiter);
        CHECK(count(7)==1,"new removal must not erase dead caller residue");
    }
done:
    stop_child(&pid); close_pipe(p);
    report(before,"actual SIGKILL; intentionally retained positive hint, no extra object reference");
}
static void negative_case(int wrong_owner)
{
    pid_t pid;
    int before=failures;
    case_name=wrong_owner ? "reject-publication-with-released-owner":"reject-constructed-zero-removal";
    obs->negative_entered=obs->negative_returned=0;
    pid=fork_owned();
    if (!pid)
    {
        struct msync_coord_guard guard;
        struct msync_single_waiter waiter={0},*head=NULL;
        if (wrong_owner)
        {
            enter(&guard); msync_coord_leave(&guard);
            obs->negative_entered=1;
            msync_coord_single_publish(&guard,&head,&waiter,10);
        }
        else
        {
            waiter.counter=msync_coord_hint(&coord,11); waiter.idx=11; head=&waiter;
            if (count(11)) _exit(103);
            obs->negative_entered=1;
            msync_coord_single_remove(&head,&waiter);
        }
        obs->negative_returned=1;
        _exit(104);
    }
    if (pid>1) (void)expect_child(&pid,1,0); /* production fatal, not an arbitrary crash */
    CHECK(obs->negative_entered && !obs->negative_returned,"invalid operation must reach helper and fail closed");
    stop_child(&pid);
    report(before,wrong_owner ? "actual released guard input":"constructed stack metadata pointing to zero counter");
}
struct park_observer_args { thread_t target; int parent_write; };
static void *park_observer(void *opaque)
{
    struct park_observer_args *a=opaque;
    uint64_t start=now_ns(),now;
    int parked=0;
    do
    {
        thread_basic_info_data_t info;
        mach_msg_type_number_t n=THREAD_BASIC_INFO_COUNT;
        if (__atomic_load_n(&obs->phase,__ATOMIC_ACQUIRE)==ABOUT_TO_ULOCK &&
            __atomic_load_n(&obs->wait_result,__ATOMIC_ACQUIRE)==INT_MIN &&
            thread_info(a->target,THREAD_BASIC_INFO,(thread_info_t)&info,&n)==KERN_SUCCESS &&
            info.run_state==TH_STATE_WAITING &&
            __atomic_load_n(&obs->phase,__ATOMIC_ACQUIRE)==ABOUT_TO_ULOCK)
        { parked=1; break; }
        sched_yield(); now=now_ns();
    } while (start && now && now-start < UINT64_C(1500000000));
    __atomic_store_n(&obs->kernel_park,(unsigned int)parked,__ATOMIC_RELEASE);
    if (!parked || !transfer(a->parent_write,1,'P',IO_MS))
        __atomic_store_n(&obs->observer_error,1,__ATOMIC_RELEASE);
    return NULL;
}
static int set_and_notify(unsigned int idx)
{
    struct msync_coord_guard guard;
    int advertised;
    enter(&guard);
    __atomic_store_n(&obs->payload,1,__ATOMIC_RELEASE);
    advertised=msync_coord_event_waiters(&coord,idx);
    if (advertised)
    {
        int ret=__ulock_wake(UL_SHARED|UL_WAKE_ALL,&obs->payload,0);
        CHECK(!ret || (ret==-1 && errno==ENOENT),"shared payload wake result");
    }
    msync_coord_leave(&guard);
    return advertised;
}
static void aba_case(void)
{
    int to_child[2]={-1,-1},from_child[2]={-1,-1},before=failures,first=-1,second=-1;
    pid_t pid=-1;
    case_name="real-late-park-ABA";
    obs->payload=obs->phase=obs->observer_error=obs->kernel_park=0;
    obs->wait_result=INT_MIN;
    if (!make_pipe(to_child) || !make_pipe(from_child)) goto done;
    pid=fork_owned();
    if (!pid)
    {
        struct msync_coord_guard guard;
        struct msync_single_waiter waiter={0},*head=NULL;
        struct park_observer_args args;
        pthread_t observer;
        int ret;
        close(to_child[1]); close(from_child[0]);
        enter(&guard); msync_coord_single_publish(&guard,&head,&waiter,8); msync_coord_leave(&guard);
        if (!transfer(from_child[1],1,'R',IO_MS) || !transfer(to_child[0],0,'G',IO_MS)) _exit(105);
        if (__atomic_load_n(&obs->payload,__ATOMIC_ACQUIRE)!=0) _exit(106);
        args=(struct park_observer_args){pthread_mach_thread_np(pthread_self()),from_child[1]};
        if (pthread_create(&observer,NULL,park_observer,&args)) _exit(107);
        /* No blocking call between this phase and the exact payload ulock. */
        __atomic_store_n(&obs->phase,ABOUT_TO_ULOCK,__ATOMIC_RELEASE);
        ret=__ulock_wait(UL_SHARED|UL_NO_ERRNO,&obs->payload,0,3000000);
        __atomic_store_n(&obs->wait_result,ret,__ATOMIC_RELEASE);
        __atomic_store_n(&obs->phase,ULOCK_RETURNED,__ATOMIC_RELEASE);
        if (pthread_join(observer,NULL)) _exit(108);
        if (ret || !obs->kernel_park || obs->observer_error || obs->payload!=1) _exit(109);
        msync_coord_single_remove(&head,&waiter);
        if (head || waiter.counter || count(8)) _exit(110);
        close_work(); _exit(0);
    }
    if (pid<0) goto done;
    close_fd(&to_child[0]); close_fd(&from_child[1]);
    if (!transfer(from_child[0],0,'R',IO_MS)) { fail("ABA registration deadline"); goto done; }
    CHECK(count(8)==1,"W1 published before first Set");
    first=set_and_notify(8); /* W1 is still behind the parent gate: no park yet. */
    {
        struct msync_coord_guard guard;
        enter(&guard); __atomic_store_n(&obs->payload,0,__ATOMIC_RELEASE); msync_coord_leave(&guard);
    }
    CHECK(count(8)==1,"first notification must not consume W1's registration");
    if (!transfer(to_child[1],1,'G',IO_MS) || !transfer(from_child[0],0,'P',IO_MS))
    { fail("exact child main thread kernel park not established"); goto done; }
    CHECK(obs->kernel_park && obs->phase==ABOUT_TO_ULOCK && obs->wait_result==INT_MIN,
          "park report must precede second Set and wait return");
    second=set_and_notify(8);
    (void)expect_child(&pid,0,0);
    CHECK(first==1 && second==1 && obs->wait_result==0 && !count(8),"future notification must wake W1 and balance count");
done:
    stop_child(&pid); close_pipe(to_child); close_pipe(from_child);
    emit("{\"type\":\"aba-observation\",\"kernelParkObserved\":%s,\"firstAdvertised\":%d,\"secondAdvertised\":%d,\"waitReturn\":%d,\"finalCount\":%u}\n",
         obs->kernel_park ? "true":"false",first,second,obs->wait_result,count(8));
    report(before,"real forked shared ulock; same-task thread_info observer; parent-gated ABA");
}
static int selected(const char *mode,const char *name) { return !strcmp(mode,"all") || !strcmp(mode,name); }
int main(int argc,char **argv)
{
    char path[PATH_MAX]="";
    const char *mode=argc==3 ? argv[2]:"all";
    int i,length,path_created=0;
    struct sigaction action={0};
    session_owner=getpid();
    action.sa_handler=SIG_IGN; sigemptyset(&action.sa_mask);
    if (sigaction(SIGPIPE,&action,NULL)) { fail("ignore SIGPIPE"); goto cleanup; }
    action.sa_handler=SIG_DFL;
    if (sigaction(SIGCHLD,&action,NULL)) { fail("waitable SIGCHLD"); goto cleanup; }
    if (session_owner<=1 || getsid(0)!=session_owner || getpgrp()!=session_owner)
    { fail("requires fresh private session/group (start_new_session=True)"); goto cleanup; }
    if (argc<2 || argc>3 || (strcmp(mode,"all") && strcmp(mode,"balance") && strcmp(mode,"sticky") &&
        strcmp(mode,"fork") && strcmp(mode,"death") && strcmp(mode,"negative") && strcmp(mode,"aba")))
    { fail("usage: msync-single-waiter owned-directory [all|balance|sticky|fork|death|negative|aba]"); goto cleanup; }
    length=snprintf(path,sizeof(path),"%s/single-waiter-XXXXXX",argv[1]);
    if (length<0 || (size_t)length>=sizeof(path)) { fail("temporary path length"); goto cleanup; }
    identity_fd=mkstemp(path);
    if (identity_fd<0) { fail("mkstemp"); goto cleanup; }
    path_created=1;
    if (fchmod(identity_fd,0600) || ftruncate(identity_fd,(off_t)MSYNC_COORD_SIZE))
    { fail("coordinator file setup"); goto cleanup; }
    coord.header=mmap(NULL,MSYNC_COORD_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,identity_fd,0);
    if (coord.header==MAP_FAILED) { coord.header=NULL; fail("coordinator mmap"); goto cleanup; }
    coord.header->magic=MSYNC_COORD_MAGIC;
    obs=(struct observations *)((char *)coord.header+OBS_OFFSET);
    if (!start_watchdog()) goto cleanup;
    emit("{\"type\":\"begin\",\"suite\":\"production-single-waiter-helper\",\"magic\":\"%016" PRIx64 "\",\"scope\":\"native hint protocol only\",\"watchdogMs\":%u}\n",(uint64_t)MSYNC_COORD_MAGIC,WATCHDOG_MS);
    flush_output();
    if (output_error) goto cleanup;
    if (selected(mode,"balance")) balance_case();
    if (selected(mode,"sticky")) { sticky_case(); sticky_race_case(); }
    if (selected(mode,"fork")) fork_case();
    if (selected(mode,"death")) death_case();
    if (selected(mode,"negative")) { negative_case(0); negative_case(1); }
    if (selected(mode,"aba")) aba_case();
cleanup:
    case_name="teardown";
    for (i=0;i<MAX_CHILDREN;++i)
        if (children[i]>1 && children[i]!=watchdog_pid)
        { pid_t pid=children[i]; fail("unexpected unreaped child"); stop_child(&pid); }
    if (work_fd>=0) { close_fd(&work_fd); work_token=0; }
    if (coord.header)
    {
        CHECK(!__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST),"owner released before teardown");
        CHECK(!munmap(coord.header,MSYNC_COORD_SIZE),"coordinator munmap"); coord.header=NULL;
    }
    close_fd(&identity_fd);
    if (path_created) CHECK(!unlink(path),"unlink owned coordinator file");
    if (child_index(watchdog_pid)>=0)
    {
        if (!transfer(watchdog_pipe[1],1,'D',IO_MS)) { fail("disarm watchdog"); stop_child(&watchdog_pid); }
        else { close_fd(&watchdog_pipe[1]); (void)expect_child(&watchdog_pid,0,0); }
    }
    close_pipe(watchdog_pipe);
    flush_output();
    emit("{\"type\":\"complete\",\"suite\":\"production-single-waiter-helper\",\"failures\":%d,\"output_error\":%s,\"cleanupChecked\":true,\"passed\":%s,\"ok\":%s}\n",
         failures,output_error ? "true":"false",failures || output_error ? "false":"true",failures || output_error ? "false":"true");
    flush_output();
    return failures || output_error ? 1:0;
}
