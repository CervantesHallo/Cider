/* Exact production hint helper, actual Darwin shared ulock. This does not
 * establish Wine/Windows equivalence. Native alarm and outer private-session
 * deadline bound joins; a missing cleanup/completion marker is never success. */
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <mach/mach.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "wine/msync_waiter_hint.h"

struct shared { uint64_t value; uint32_t marker,hint; unsigned int phase; int wait_ret; };
static struct shared *shared;
static unsigned int failures;
static void check(int ok,const char *message)
{
    if(!ok) { __atomic_add_fetch(&failures,1,__ATOMIC_RELAXED); fprintf(stderr,"%s\n",message); }
}
static uint64_t now_ns(void)
{
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)) _exit(70);
    return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec;
}
static unsigned int errors(void) { return __atomic_load_n(&failures,__ATOMIC_RELAXED); }
static void safe_leave(uint32_t *hint)
{
    uint32_t v=__atomic_load_n(hint,__ATOMIC_SEQ_CST);
    /* Corrupt negative variants already lost the registration. Report that
     * violation and finish teardown instead of invoking an invalid decrement. */
    if(!(v & (MSYNC_HINT_COUNT|MSYNC_HINT_STICKY))) check(0,"registration count disappeared");
    else msync_hint_leave(hint);
}
static void balance_case(void)
{
    uint32_t hint=0,before,snapshot;
    msync_hint_enter(&hint,0);
    before=__atomic_load_n(&hint,__ATOMIC_SEQ_CST);
    snapshot=msync_hint_snapshot(&hint);
    check(before==1 && snapshot==before &&
          (__atomic_load_n(&hint,__ATOMIC_SEQ_CST)^before)==MSYNC_HINT_PUBLISH,
          "snapshot must publish without changing an advertised single");
    if(errors()) return;
    msync_hint_enter(&hint,1);
    check((hint & MSYNC_HINT_COUNT)==2 && msync_hint_wake_many(hint),"multi presence/count");
    safe_leave(&hint); msync_hint_clear_many(&hint);
    check((hint & MSYNC_HINT_COUNT)==1 && !msync_hint_wake_many(hint),"clearing MANY erased single");
    safe_leave(&hint);
    snapshot=msync_hint_snapshot(&hint);
    check(!msync_hint_wake_single(snapshot) && !msync_hint_wake_many(snapshot),
          "balanced hint still advertises a waiter");
}
static void sticky_case(void)
{
    uint32_t hint=MSYNC_HINT_COUNT;
    msync_hint_enter(&hint,0);
    check((hint & MSYNC_HINT_STICKY)!=0 && msync_hint_wake_single(hint),"hint bound lost unconditional fallback");
    if(errors()) return;
    msync_hint_enter(&hint,1); safe_leave(&hint); msync_hint_clear_many(&hint);
    safe_leave(&hint);
    check((hint & MSYNC_HINT_STICKY)!=0 && msync_hint_wake_single(hint) &&
          !msync_hint_wake_many(hint),"sticky/MANY independence");
}
static void *count_worker(void *arg)
{
    unsigned int i; int many=(int)(intptr_t)arg;
    for(i=0;i<100000;++i)
    {
        msync_hint_enter(&shared->hint,many);
        if(!(i & 255)) sched_yield();
        safe_leave(&shared->hint);
        if(many) msync_hint_clear_many(&shared->hint); /* sole simulated pump */
    }
    return NULL;
}
static void parallel_case(void)
{
    pthread_t threads[9]; unsigned int i,created=0;
    __atomic_store_n(&shared->hint,0,__ATOMIC_SEQ_CST);
    for(i=0;i<9;++i)
    {
        int ret=pthread_create(&threads[i],NULL,count_worker,(void *)(intptr_t)(i==8));
        check(!ret,"count worker creation"); if(ret) break; ++created;
    }
    for(i=0;i<created;++i) check(!pthread_join(threads[i],NULL),"count worker join");
    check(__atomic_load_n(&shared->hint,__ATOMIC_SEQ_CST)==0,"parallel registrations did not balance");
}
static void *aba_reader(void *unused)
{
    uint64_t end=now_ns()+2000000000;
    int ret;
    (void)unused;
    msync_hint_enter(&shared->hint,0);
    __atomic_store_n(&shared->phase,1,__ATOMIC_RELEASE);
    while(__atomic_load_n(&shared->phase,__ATOMIC_ACQUIRE)!=2)
    {
        if(now_ns()>end) { check(0,"ABA reader release deadline"); safe_leave(&shared->hint); return NULL; }
        sched_yield();
    }
    check(__atomic_load_n(&shared->value,__ATOMIC_ACQUIRE)==0,"ABA input not reset before park");
    __atomic_store_n(&shared->phase,3,__ATOMIC_RELEASE);
    ret=__ulock_wait(3u|0x1000000u,&shared->value,0,1000000);
    __atomic_store_n(&shared->wait_ret,ret,__ATOMIC_RELEASE);
    check(ret==0,"future signal did not wake kernel-parked reader");
    check(__atomic_load_n(&shared->value,__ATOMIC_ACQUIRE)==1,"reader did not observe future signal");
    safe_leave(&shared->hint);
    return NULL;
}
static void wake(void)
{
    int ret=__ulock_wake(3u|0x100u,&shared->value,0);
    check(ret==0 || (ret==-1 && errno==ENOENT),"unexpected shared ulock wake error");
}
static void aba_case(void)
{
    pthread_t thread; uint64_t end; uint32_t first,second;
    int parked=0,ret;
    __atomic_store_n(&shared->hint,0,__ATOMIC_SEQ_CST);
    __atomic_store_n(&shared->phase,0,__ATOMIC_RELEASE);
    __atomic_store_n(&shared->value,0,__ATOMIC_RELEASE);
    shared->wait_ret=INT_MIN;
    ret=pthread_create(&thread,NULL,aba_reader,NULL); check(!ret,"ABA reader creation");
    if(ret) return;
    end=now_ns()+2000000000;
    while(__atomic_load_n(&shared->phase,__ATOMIC_ACQUIRE)!=1 && now_ns()<end) sched_yield();
    check(__atomic_load_n(&shared->phase,__ATOMIC_ACQUIRE)==1,"ABA registration deadline");
    __atomic_store_n(&shared->value,1,__ATOMIC_RELEASE);
    first=msync_hint_snapshot(&shared->hint);
    if(msync_hint_wake_single(first)) wake(); /* reader cannot park yet */
    __atomic_store_n(&shared->value,0,__ATOMIC_RELEASE); /* another legal consumer/reset */
    __atomic_store_n(&shared->phase,2,__ATOMIC_RELEASE);
    end=now_ns()+1500000000;
    while(now_ns()<end)
    {
        thread_basic_info_data_t info; mach_msg_type_number_t n=THREAD_BASIC_INFO_COUNT;
        if(__atomic_load_n(&shared->phase,__ATOMIC_ACQUIRE)==3 &&
           __atomic_load_n(&shared->wait_ret,__ATOMIC_ACQUIRE)==INT_MIN &&
           thread_info(pthread_mach_thread_np(thread),THREAD_BASIC_INFO,(thread_info_t)&info,&n)==KERN_SUCCESS &&
           info.run_state==TH_STATE_WAITING) { parked=1; break; }
        sched_yield();
    }
    check(parked,"did not observe exact reader kernel wait after ABA");
    __atomic_store_n(&shared->value,1,__ATOMIC_RELEASE);
    second=msync_hint_snapshot(&shared->hint);
    if(msync_hint_wake_single(second)) wake();
    check(!pthread_join(thread,NULL),"ABA reader join");
    check(__atomic_load_n(&shared->hint,__ATOMIC_SEQ_CST)==0,"ABA registration left a hint");
    printf("{\"kind\":\"waiter-hint-aba\",\"kernelParkObserved\":%s,\"firstHint\":%u,\"secondHint\":%u,\"waitReturn\":%d}\n",
           parked ? "true":"false",first,second,shared->wait_ret);
}
int main(int argc,char **argv)
{
    char path[4096]={0}; int fd=-1,created=0; struct sigaction action={0}; sigset_t mask;
    const char *mode=argc==3 ? argv[2]:"all";
    if(argc<2 || argc>3) return 64;
    if(strcmp(mode,"all") && strcmp(mode,"balance") && strcmp(mode,"sticky") && strcmp(mode,"aba")) return 64;
    action.sa_handler=SIG_DFL; sigemptyset(&action.sa_mask);
    if(sigaction(SIGALRM,&action,NULL)) return 65;
    sigemptyset(&mask); sigaddset(&mask,SIGALRM);
    if(pthread_sigmask(SIG_UNBLOCK,&mask,NULL)) return 65;
    alarm(20);
    if(snprintf(path,sizeof(path),"%s/hint-XXXXXX",argv[1])>=(int)sizeof(path)) return 66;
    fd=mkstemp(path); if(fd<0) return 67; created=1;
    if(fchmod(fd,0600) || ftruncate(fd,4096)) { check(0,"owned mapping file"); goto done; }
    shared=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(shared==MAP_FAILED) { shared=NULL; check(0,"owned mapping"); goto done; }
    shared->marker=0x551122aa;
    if(!strcmp(mode,"all") || !strcmp(mode,"balance")) balance_case();
    if(!strcmp(mode,"all") || !strcmp(mode,"sticky")) sticky_case();
    if(!strcmp(mode,"all") && !errors()) parallel_case();
    if(!strcmp(mode,"all") || !strcmp(mode,"aba")) aba_case();
    check(shared->marker==0x551122aa,"hint helper changed adjacent object metadata");
done:
    if(shared) check(!munmap(shared,4096),"mapping teardown");
    if(fd>=0) check(!close(fd),"owned fd close");
    if(created) check(!unlink(path),"owned file unlink");
    alarm(0);
    printf("{\"kind\":\"waiter-hint-complete\",\"failures\":%u,\"cleanupChecked\":true,\"passed\":%s}\n",
           errors(),errors() ? "false":"true");
    return fflush(stdout) || ferror(stdout) || errors() ? 1:0;
}
