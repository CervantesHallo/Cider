/* Exact extracted late-park bodies + production coordinator. Explicit native
 * admission/control/time/notification doubles; not Wine/Windows/signal proof. */
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <mach/mach.h>
#include "wine/msync_coord.h"
typedef uint32_t NTSTATUS;
typedef uint64_t ULONGLONG;
#define STATUS_SUCCESS 0u
#define STATUS_PENDING 0x103u
#define STATUS_TIMEOUT 0x102u
#define MSYNC_AUTO_EVENT 2
#define MSYNC_MANUAL_EVENT 3
#define MSYNC_MUTEX 4
#define UL_COMPARE_AND_WAIT_SHARED 3
#define ULF_NO_ERRNO 0x1000000
struct event {int signaled,unused;unsigned short msync_type,refcount;int multiple_waiters;};
struct mutex {int tid;unsigned count;unsigned short msync_type,refcount;int multiple_waiters;};
_Static_assert(sizeof(struct event)==16,"actual slot layout");
enum { READY,SPIN_READY,BEFORE_OWNER,BEFORE_RAW,ABA,EINTR_RETRY,EFAULT_RETRY,EXPIRE_EINTR,EXPIRE_EFAULT,EXPIRE_OWNER,RAW_TIMEOUT,REAL_PARK,CANCEL_PARK,STICKY,MODES };
enum { STAGE_NONE,STAGE_SPIN,STAGE_OWNER,STAGE_RAW,STAGE_DONE };
struct context {
 struct msync_single_waiter node;
 atomic_int stage,go;
 unsigned mode,steps,entries,raw_calls,checks,failures,cleanup_count,active,depth;
 uint64_t deadline;
 uint64_t last_wait_ns;
 NTSTATUS status;
 int published_before_raw,joined_cancel,time_expired;
};
static _Thread_local struct context *current;
static _Thread_local struct msync_single_waiter *single_waiters;
static struct msync_coord coord;
static struct event *event;
static unsigned notify_skips,notify_calls;
static int last_wake_result;
static void check(int ok){current->checks++;current->failures+=!ok;}
static void alarm_expired(int sig){(void)sig;_exit(124);}
static uint64_t now_ns(void){return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);}
static ULONGLONG update_timeout(ULONGLONG end){uint64_t now=now_ns();if(current->time_expired)return 0;return now>=end?0:(end-now)/100;}
static void pause_cpu(void){
#ifdef __x86_64__
 __asm__ volatile("pause":::"memory");
#else
 __asm__ volatile("yield":::"memory");
#endif
}
static void await_go(void){uint64_t limit=now_ns()+UINT64_C(3000000000);while(!atomic_load(&current->go)){if(now_ns()>limit)_exit(80);pause_cpu();}}
static void stage(unsigned value){atomic_store(&current->stage,(int)value);await_go();}
static int policy_state(void){int old;if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&old)||pthread_setcancelstate(old,NULL))_exit(81);return old;}
static void set_active(int active){current->active=(unsigned)active;}
static void *slot(unsigned idx,int map){check(!map);return idx==1?event:NULL;}
static int notify(unsigned idx,void *shm,int blocking){
 if(blocking||msync_coord_event_waiters(&coord,idx)){notify_calls++;last_wake_result=__ulock_wake(3|0x100,shm,0);}else notify_skips++;
 return 1;
}
static void coord_enter(struct msync_coord_guard *g){
 check(policy_state()==PTHREAD_CANCEL_DISABLE);check(current->depth==0);current->entries++;
 if(current->mode==BEFORE_OWNER||current->mode==EXPIRE_OWNER)stage(STAGE_OWNER);
 msync_coord_enter(&coord,-1,7,g);
 check(msync_coord_epochs(&coord)[1]==2);
}
static void msync_control_begin(void){check(policy_state()==PTHREAD_CANCEL_DISABLE);current->depth++;}
static void msync_control_end(void){check(current->depth==1);current->depth--;}
static void step(void){current->steps++;if(current->mode==SPIN_READY&&current->steps==1)stage(STAGE_SPIN);pause_cpu();}
#define YieldProcessor() step()
static int ulock_wait(uint32_t flags,void *address,uint64_t value,uint64_t timeout_ns){
 current->raw_calls++;current->last_wait_ns=timeout_ns;check(policy_state()==PTHREAD_CANCEL_DISABLE);check(!current->active&&!coord.header->owner);check(flags==(3u|0x1000000u));
 check(msync_coord_event_waiters(&coord,1));
 current->published_before_raw=current->mode==STICKY||single_waiters==&current->node;
 check(current->published_before_raw);
 if(current->mode==EINTR_RETRY&&current->raw_calls==1)return -EINTR;
 if(current->mode==EFAULT_RETRY&&current->raw_calls==1)return -EFAULT;
 /* Explicit timeout-double injection: the fixed absolute end is expired
  * between raw attempts. No real elapsed-time claim is made for these cases. */
 if(current->mode==EXPIRE_EINTR&&current->raw_calls==1){current->time_expired=1;return -EINTR;}
 if(current->mode==EXPIRE_EFAULT&&current->raw_calls==1){current->time_expired=1;return -EFAULT;}
 if(current->time_expired)return -ETIMEDOUT; /* stale-timeout mutant must be caught */
 stage(STAGE_RAW);
 return __ulock_wait(flags,address,value,timeout_ns?(uint32_t)((timeout_ns+999)/1000):1000000);
}
#include "late-extracted.h"
static uint32_t hint(void){return __atomic_load_n(msync_coord_hint(&coord,1),__ATOMIC_SEQ_CST);}
static void cleanup(void *opaque){struct context *c=opaque;c->cleanup_count++;check(!single_waiters&&!c->node.counter&&!c->active&&!c->depth);check(__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST)!=7);check(hint()==(c->mode==STICKY?UINT32_MAX:0));}
static void *worker(void *opaque){
 struct context *c=opaque;struct msync_payload_park park={&c->node,&single_waiters,1};current=c;
 if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL)||pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL))_exit(82);
 pthread_cleanup_push(cleanup,c);
 c->status=msync_wait_single(1,event,&c->deadline,1,c->mode==STICKY?NULL:&park);
 check(!c->active&&__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST)!=7);
 if(c->node.counter)single_wait_remove(&c->node);
 check(!single_waiters&&!c->node.counter);
 atomic_store(&c->stage,STAGE_DONE);
 if(c->mode==CANCEL_PARK){if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL))_exit(83);pthread_testcancel();}
 pthread_cleanup_pop(1);
 return NULL;
}
static void parent_stage(struct context *c,unsigned expected){uint64_t limit=now_ns()+UINT64_C(3000000000);while(atomic_load(&c->stage)!=(int)expected){if(atomic_load(&c->stage)==STAGE_DONE||now_ns()>limit)_exit(84);pause_cpu();}}
static void set_value(int signaled){
 struct msync_coord_guard g;struct msync_coord_entry e={1,(uint32_t)signaled,msync_coord_value((unsigned)signaled,0)};
 msync_coord_enter(&coord,-1,11,&g);msync_coord_commit(&coord,&e,1);msync_coord_leave(&g);
}
static int parked(pthread_t thread){thread_basic_info_data_t b;mach_msg_type_number_t n=THREAD_BASIC_INFO_COUNT;return thread_info(pthread_mach_thread_np(thread),THREAD_BASIC_INFO,(thread_info_t)&b,&n)==KERN_SUCCESS&&b.run_state==TH_STATE_WAITING;}
static void await_park(pthread_t thread){uint64_t limit=now_ns()+UINT64_C(1000000000);while(!parked(thread)){if(now_ns()>limit)_exit(85);pause_cpu();}}
int main(void){
 unsigned mode,checks=0,failures=0,joined=0;struct context parent={0};void *page;
 if(getpid()!=getpgrp()||getpid()!=getsid(0))return 64;
 signal(SIGALRM,alarm_expired);alarm(25);current=&parent;
 if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL))return 65;
 coord.header=mmap(NULL,MSYNC_COORD_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);page=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);if(coord.header==MAP_FAILED||page==MAP_FAILED)return 66;
 event=(struct event*)((char*)page+16);coord.slot=slot;coord.notify=notify;coord.set_active=set_active;coord.cancellation_managed=1;
 (void)__ulock_wake(3|0x100,event,0);(void)__ulock_wait(3|0x1000000,event,0,1);
 for(mode=0;mode<MODES;mode++){
  struct context c={.mode=mode};pthread_t thread;void *status=NULL;uint64_t began=now_ns();unsigned old_parent_failures=parent.failures;
  memset(coord.header,0,MSYNC_COORD_HEADER_SIZE);coord.header->magic=MSYNC_COORD_MAGIC;msync_coord_epochs(&coord)[1]=2;__atomic_store_n(msync_coord_hint(&coord,1),mode==STICKY?UINT32_MAX:0,__ATOMIC_SEQ_CST);*event=(struct event){.signaled=mode==READY,.msync_type=MSYNC_MANUAL_EVENT};notify_skips=notify_calls=0;last_wake_result=-999;
  c.deadline=began+(mode==EXPIRE_OWNER?20000000u:mode==RAW_TIMEOUT?100000000u:2000000000u);atomic_init(&c.stage,0);atomic_init(&c.go,0);
  if(pthread_create(&thread,NULL,worker,&c))return 67;
  if(mode==SPIN_READY){parent_stage(&c,STAGE_SPIN);check(hint()==0);set_value(1);check(notify_skips==1);atomic_store(&c.go,1);}
  else if(mode==BEFORE_OWNER){parent_stage(&c,STAGE_OWNER);check(hint()==0);set_value(1);check(notify_skips==1);atomic_store(&c.go,1);}
  else if(mode==EXPIRE_OWNER){parent_stage(&c,STAGE_OWNER);while(now_ns()<=c.deadline+1000000)pause_cpu();atomic_store(&c.go,1);}
  else if(mode!=READY&&mode!=EXPIRE_EINTR&&mode!=EXPIRE_EFAULT){
   parent_stage(&c,STAGE_RAW);check(hint()!=0);
   if(mode==ABA){set_value(1);check(hint()==1);set_value(0);check(hint()==1);atomic_store(&c.go,1);await_park(thread);check(hint()==1);set_value(1);check(last_wake_result==0);}
   else if(mode==REAL_PARK||mode==CANCEL_PARK){atomic_store(&c.go,1);await_park(thread);check(hint()==1);if(mode==CANCEL_PARK&&pthread_cancel(thread))_exit(68);set_value(1);check(last_wake_result==0);}
   else if(mode==RAW_TIMEOUT){atomic_store(&c.go,1);}
   else {set_value(1);atomic_store(&c.go,1);}
  }
  if(pthread_join(thread,&status))_exit(69);joined++;
  check(__atomic_load_n(&coord.header->owner,__ATOMIC_SEQ_CST)==0);check(coord.header->state==MSYNC_COORD_CLEAN);check(!parent.active);
  check(status==(mode==CANCEL_PARK?PTHREAD_CANCELED:NULL));check(c.cleanup_count==1);
  if(mode==EXPIRE_OWNER)check(c.status==STATUS_TIMEOUT&&c.raw_calls==0&&c.last_wait_ns==0);
  else if(mode==RAW_TIMEOUT)check(c.status==STATUS_TIMEOUT&&c.raw_calls==1);
  else if(mode==EXPIRE_EINTR||mode==EXPIRE_EFAULT)check(c.status==STATUS_TIMEOUT&&c.raw_calls==1&&c.time_expired);
  else check(c.status==STATUS_PENDING||c.status==STATUS_SUCCESS);
  if(mode==READY||mode==SPIN_READY)check(c.entries==0&&c.raw_calls==0);
  if(mode==EINTR_RETRY||mode==EFAULT_RETRY)check(c.steps==64&&c.entries==1&&c.raw_calls==2);
  if(mode==EXPIRE_EINTR||mode==EXPIRE_EFAULT)check(c.steps==64&&c.entries==1);
  if(mode==STICKY)check(c.steps==0&&c.entries==0&&hint()==UINT32_MAX);
  check(!c.node.counter&&hint()==(mode==STICKY?UINT32_MAX:0));
  checks+=c.checks;failures+=c.failures;
  printf("{\"type\":\"case\",\"mode\":%u,\"passed\":%s,\"status\":%u,\"spins\":%u,\"late_entries\":%u,\"raw_calls\":%u,\"notify_skips\":%u,\"notify_calls\":%u,\"elapsed_ns\":%" PRIu64 "}\n",mode,c.failures==0&&parent.failures==old_parent_failures?"true":"false",c.status,c.steps,c.entries,c.raw_calls,notify_skips,notify_calls,now_ns()-began);
 }
 failures+=parent.failures;checks+=parent.checks;if(munmap(page,4096)||munmap(coord.header,MSYNC_COORD_SIZE))return 70;alarm(0);
 printf("{\"type\":\"complete\",\"passed\":%s,\"checks\":%u,\"failures\":%u,\"owned_threads_joined\":%u,\"scope\":\"exact late wait/publication/remove with production header and explicit native doubles; not Wine/Windows\"}\n",failures?"false":"true",checks,failures,joined);
 return !failures&&fflush(stdout)==0&&!ferror(stdout)?0:1;
}
