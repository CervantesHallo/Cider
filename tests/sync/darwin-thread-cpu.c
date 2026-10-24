/* Read-only own-process timing-provider check, not Wine performance acceptance.
 * Identifies each owned pthread by its kernel64 ID and a distinct Mach port.
 * Two overlapping busy/sleep intervals distinguish thread from process CPU.
 * The caller must impose an outer process deadline; alarm bounds internal joins.
 * PROC15 times have ns UNITS; this is not a nanosecond-resolution claim. */
#include <errno.h>
#include <inttypes.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_info.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc_info.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static unsigned ready,sampled,finished,go,released,checks,failures;
static uint64_t common_deadline;
struct snapshot {uint64_t wall,thread_clock,proc_user,proc_system,mach_user,mach_system,basic_cpu;int run_state;};
struct worker {unsigned role,sleep_calls;uint64_t id,handle;mach_port_t port;struct snapshot begin,end;int port_released;};
static struct worker workers[2];
static void require(int ok,const char *name)
{
    if(!ok){fprintf(stderr,"provider failure: %s (errno=%d)\n",name,errno);_exit(2);}
}
static uint64_t clock_ns(clockid_t id)
{
    struct timespec t;require(!clock_gettime(id,&t),"clock_gettime");return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;
}
static uint64_t timeval_ns(struct timeval t){return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_usec*1000u;}
static void sample(struct worker *w,struct snapshot *s)
{
    struct proc_threadinfo p;thread_extended_info_data_t e;mach_msg_type_number_t count=THREAD_EXTENDED_INFO_COUNT;
    thread_basic_info_data_t basic;mach_msg_type_number_t basic_count=THREAD_BASIC_INFO_COUNT;
    s->wall=clock_ns(CLOCK_MONOTONIC_RAW);s->thread_clock=clock_ns(CLOCK_THREAD_CPUTIME_ID);
    require(proc_pidinfo(getpid(),PROC_PIDTHREADID64INFO,w->id,&p,sizeof(p))==(int)sizeof(p),"proc_pidinfo kernel64 ID");
    require(thread_info(w->port,THREAD_EXTENDED_INFO,(thread_info_t)&e,&count)==KERN_SUCCESS&&count==THREAD_EXTENDED_INFO_COUNT,"thread_info extended");
    require(thread_info(w->port,THREAD_BASIC_INFO,(thread_info_t)&basic,&basic_count)==KERN_SUCCESS&&basic_count==THREAD_BASIC_INFO_COUNT,"thread_info explicit seconds/microseconds");
    s->basic_cpu=((uint64_t)basic.user_time.seconds+(uint64_t)basic.system_time.seconds)*1000000000u+((uint64_t)basic.user_time.microseconds+(uint64_t)basic.system_time.microseconds)*1000u;
    s->proc_user=p.pth_user_time;s->proc_system=p.pth_system_time;s->mach_user=e.pth_user_time;s->mach_system=e.pth_system_time;s->run_state=p.pth_run_state;
#ifdef CPU_PROVIDER_PROCESS_DOUBLE
    {struct rusage usage;require(!getrusage(RUSAGE_SELF,&usage),"process double rusage");s->proc_user=timeval_ns(usage.ru_utime);s->proc_system=timeval_ns(usage.ru_stime);}
#endif
}
static void *run(void *opaque)
{
    struct worker *w=opaque;thread_identifier_info_data_t id;mach_msg_type_number_t count=THREAD_IDENTIFIER_INFO_COUNT;
    volatile uint64_t value=1;uint64_t deadline;
    w->port=mach_thread_self();require(w->port!=MACH_PORT_NULL,"owned thread port");
    require(!pthread_threadid_np(NULL,&w->id),"pthread kernel64 identity");
    require(thread_info(w->port,THREAD_IDENTIFIER_INFO,(thread_info_t)&id,&count)==KERN_SUCCESS&&count==THREAD_IDENTIFIER_INFO_COUNT,"Mach identity");
    require(id.thread_id==w->id,"pthread and Mach kernel64 ID agree");w->handle=id.thread_handle;
    require(!pthread_mutex_lock(&lock),"ready lock");++ready;require(!pthread_cond_broadcast(&condition),"ready notify");
    while(!go)require(!pthread_cond_wait(&condition,&lock),"ready wait");require(!pthread_mutex_unlock(&lock),"ready unlock");
    sample(w,&w->begin);
    /* Both CPU baselines precede either workload, so a process-wide counter
     * cannot appear thread-specific just because one worker starts later. */
    require(!pthread_mutex_lock(&lock),"baseline lock");
    if(++sampled==2){common_deadline=clock_ns(CLOCK_MONOTONIC_RAW)+200000000u;require(!pthread_cond_broadcast(&condition),"baseline notify");}
    while(sampled!=2)require(!pthread_cond_wait(&condition,&lock),"baseline wait");
    deadline=common_deadline;require(!pthread_mutex_unlock(&lock),"baseline unlock");
    while(clock_ns(CLOCK_MONOTONIC_RAW)<deadline)
    {
        if(!w->role){unsigned n;for(n=0;n<4096;++n)value=value*2862933555777941757ULL+3037000493ULL;}
        else
        {
            uint64_t now=clock_ns(CLOCK_MONOTONIC_RAW),left=deadline>now?deadline-now:0;
            struct timespec sleep_time={(time_t)(left/1000000000u),(long)(left%1000000000u)};
            ++w->sleep_calls;if(nanosleep(&sleep_time,NULL)&&errno!=EINTR)require(0,"sleep");
        }
    }
    sample(w,&w->end);(void)value;
    require(!pthread_mutex_lock(&lock),"finished lock");++finished;require(!pthread_cond_broadcast(&condition),"finished notify");
    while(!released)require(!pthread_cond_wait(&condition,&lock),"release wait");require(!pthread_mutex_unlock(&lock),"release unlock");
    w->port_released=mach_port_deallocate(mach_task_self(),w->port)==KERN_SUCCESS;
    return NULL;
}
static void check(const char *name,int ok,uint64_t actual,uint64_t expected)
{
    ++checks;if(!ok)++failures;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRIu64 ",\"expected\":%" PRIu64 "}\n",name,ok?"true":"false",actual,expected);
}
int main(void)
{
    pthread_t threads[2];struct rusage before,after;struct proc_threadinfo external[2];uint64_t proc_cpu[2],clock_cpu[2],mach_cpu[2],basic_cpu[2];unsigned i;mach_timebase_info_data_t timebase;
    alarm(5);require(mach_timebase_info(&timebase)==KERN_SUCCESS&&timebase.denom,"timebase info");
    printf("{\"type\":\"begin\",\"scope\":\"native-own-thread-provider\",\"pid\":%d,\"timebase_numer\":%u,\"timebase_denom\":%u}\n",getpid(),timebase.numer,timebase.denom);
    for(i=0;i<2;++i){workers[i].role=i;require(!pthread_create(&threads[i],NULL,run,&workers[i]),"create owned pthread");}
    require(!pthread_mutex_lock(&lock),"main lock");while(ready!=2)require(!pthread_cond_wait(&condition,&lock),"all ready");
    require(!getrusage(RUSAGE_SELF,&before),"process before");go=1;require(!pthread_cond_broadcast(&condition),"start notify");
    while(finished!=2)require(!pthread_cond_wait(&condition,&lock),"all finished");require(!getrusage(RUSAGE_SELF,&after),"process after");
    for(i=0;i<2;++i)require(proc_pidinfo(getpid(),PROC_PIDTHREADID64INFO,workers[i].id,&external[i],sizeof(external[i]))==(int)sizeof(external[i]),"cross-thread lookup while alive");
    released=1;require(!pthread_cond_broadcast(&condition),"release notify");require(!pthread_mutex_unlock(&lock),"main unlock");
    for(i=0;i<2;++i)require(!pthread_join(threads[i],NULL),"join owned pthread");
    check("distinct_kernel_ids",workers[0].id!=workers[1].id,workers[0].id!=workers[1].id,1);
    check("actual_sleep_call",workers[1].sleep_calls>0,workers[1].sleep_calls>0,1);
    for(i=0;i<2;++i)
    {
        struct worker *w=&workers[i];struct snapshot *a=&w->begin,*b=&w->end;int monotonic;
        monotonic=b->wall>=a->wall&&b->thread_clock>=a->thread_clock&&b->proc_user>=a->proc_user&&b->proc_system>=a->proc_system&&b->mach_user>=a->mach_user&&b->mach_system>=a->mach_system&&b->basic_cpu>=a->basic_cpu;
        check("monotonic_counters",monotonic,monotonic,1);
        if(!monotonic)return fflush(stdout)||ferror(stdout)?2:1;
        proc_cpu[i]=b->proc_user+b->proc_system-a->proc_user-a->proc_system;clock_cpu[i]=b->thread_clock-a->thread_clock;mach_cpu[i]=b->mach_user+b->mach_system-a->mach_user-a->mach_system;
        basic_cpu[i]=b->basic_cpu-a->basic_cpu;
        {uint64_t gap=proc_cpu[i]>basic_cpu[i]?proc_cpu[i]-basic_cpu[i]:basic_cpu[i]-proc_cpu[i];
         check("proc15_matches_explicit_units",gap<5000000u,gap<5000000u,1);}
        check("port_released",w->port_released,w->port_released,1);
        monotonic=external[i].pth_user_time>=b->proc_user&&external[i].pth_system_time>=b->proc_system;
        check("external_lookup_monotonic",monotonic,monotonic,1);
        printf("{\"type\":\"sample\",\"role\":\"%s\",\"kernel64_id\":%" PRIu64 ",\"mach_port\":%u,\"libproc_handle\":%" PRIu64 ",\"wall_ns\":%" PRIu64 ",\"thread_clock_ns\":%" PRIu64 ",\"proc15_cpu_ns\":%" PRIu64 ",\"mach_cpu_ns\":%" PRIu64 ",\"basic_explicit_units_cpu_ns\":%" PRIu64 ",\"run_state_before\":%d,\"run_state_after\":%d}\n",i?"sleep":"busy",w->id,w->port,w->handle,b->wall-a->wall,clock_cpu[i],proc_cpu[i],mach_cpu[i],basic_cpu[i],a->run_state,b->run_state);
    }
    check("proc15_distinguishes_threads",proc_cpu[0]>proc_cpu[1]+10000000u,proc_cpu[0]>proc_cpu[1]+10000000u,1);
    check("clock_distinguishes_threads",clock_cpu[0]>clock_cpu[1]+10000000u,clock_cpu[0]>clock_cpu[1]+10000000u,1);
    check("mach_distinguishes_threads",mach_cpu[0]>mach_cpu[1]+10000000u,mach_cpu[0]>mach_cpu[1]+10000000u,1);
    printf("{\"type\":\"process_observation\",\"cpu_ns\":%" PRIu64 "}\n",timeval_ns(after.ru_utime)+timeval_ns(after.ru_stime)-timeval_ns(before.ru_utime)-timeval_ns(before.ru_stime));
    printf("{\"type\":\"complete\",\"ok\":%s,\"checks\":%u,\"failures\":%u}\n",failures?"false":"true",checks,failures);alarm(0);
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
