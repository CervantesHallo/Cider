/* Self-owned native observation only: real pending cancellation, syscall paths
 * and a known read CP. No Wine, Windows, private pthread fields, or other task. */
#define _DARWIN_C_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
typedef int (*wake_fn)(uint32_t,void*,uint64_t);
typedef int (*wait_fn)(uint32_t,void*,uint64_t,uint32_t);
typedef int (*yield_fn)(void);
static wake_fn wake_call;static wait_fn wait_call;static yield_fn yield_call;
static uint32_t *word;static unsigned joined;
struct trial {unsigned op;int pipefd[2];atomic_int ready,go,held,observed_held,returned,cleanup_calls,waiter_ready;int result,error,waiter_result;};
static void fatal(int code){_exit(code);}
static void alarm_expired(int sig){(void)sig;fatal(97);}
static void pause_cpu(void){
#ifdef __x86_64__
 __asm__ volatile("pause":::"memory");
#else
 __asm__ volatile("yield":::"memory");
#endif
}
static uint64_t ns(void){return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);}
static void wait_flag(atomic_int *flag){uint64_t until=ns()+UINT64_C(3000000000);while(!atomic_load(flag)){if(ns()>until)fatal(70);pause_cpu();}}
static void cleanup(void *opaque){struct trial*t=opaque;atomic_store(&t->observed_held,atomic_load(&t->held));atomic_fetch_add(&t->cleanup_calls,1);}
static void *target(void *opaque){
 struct trial*t=opaque;char byte;
 if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL)||pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL))fatal(71);
 pthread_cleanup_push(cleanup,t);
 errno=0;atomic_store(&t->held,1);atomic_store(&t->ready,1);
 while(!atomic_load(&t->go))pause_cpu(); /* No hidden CP while request queues. */
 if(t->op<=1){__atomic_store_n(word,1,__ATOMIC_RELEASE);t->result=wake_call(3u|0x100u,word,0);}
 else if(t->op==2)t->result=yield_call();
 else t->result=(int)read(t->pipefd[0],&byte,1); /* Known CP, expected early cancellation. */
 atomic_store(&t->returned,1);atomic_store(&t->held,0);t->error=errno;
 pthread_testcancel(); /* Explicit delivery after modeled obligation drains. */
 pthread_cleanup_pop(1);
 return NULL;
}
static void *waiter(void *opaque){struct trial*t=opaque;atomic_store(&t->waiter_ready,1);t->waiter_result=wait_call(3u|0x1000000u,word,0,2000000);return NULL;}
static int parked(pthread_t thread){
 thread_basic_info_data_t info;mach_msg_type_number_t count=THREAD_BASIC_INFO_COUNT;
 return thread_info(pthread_mach_thread_np(thread),THREAD_BASIC_INFO,(thread_info_t)&info,&count)==KERN_SUCCESS&&info.run_state==TH_STATE_WAITING;
}
static void describe(const char *name,void *symbol){
 Dl_info info={0};unsigned char bytes[96];size_t length=0,i;
 mach_vm_address_t region=(mach_vm_address_t)(uintptr_t)symbol;mach_vm_size_t size=0;
 vm_region_basic_info_data_64_t basic;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;
 if(!dladdr(symbol,&info)||!info.dli_fname)fatal(72);
 if(mach_vm_region(mach_task_self(),&region,&size,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&basic,&count,&object)==KERN_SUCCESS &&
    (basic.protection&VM_PROT_READ) && region<=(uintptr_t)symbol && (uintptr_t)symbol<region+size){
  length=(size_t)(region+size-(uintptr_t)symbol);if(length>sizeof(bytes))length=sizeof(bytes);
  for(i=0;i<length;i++)bytes[i]=((const unsigned char*)symbol)[i];
 }
 if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
 printf("{\"type\":\"symbol\",\"name\":\"%s\",\"image\":\"%s\",\"address\":\"0x%" PRIxPTR "\",\"entry_bytes\":\"",name,info.dli_fname,(uintptr_t)symbol);
 for(i=0;i<length;i++)printf("%02x",bytes[i]);
 printf("\",\"boundary\":\"entry bytes may include following functions; not a complete call-graph proof\"}\n");
}
int main(void){
 const char*names[]={"ulock_wake_no_waiter","ulock_wake_real_waiter","sched_yield","known_read_cancellation_point"};unsigned op;int result_ok=1,all_deferred_safe=1;
 if(getpid()!=getsid(0)||getpid()!=getpgrp())return 64;signal(SIGALRM,alarm_expired);alarm(25);
 if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL))return 65;
 wake_call=(wake_fn)dlsym(RTLD_DEFAULT,"__ulock_wake");wait_call=(wait_fn)dlsym(RTLD_DEFAULT,"__ulock_wait");yield_call=(yield_fn)dlsym(RTLD_DEFAULT,"sched_yield");if(!wake_call||!wait_call||!yield_call)return 66;
 word=mmap(NULL,(size_t)getpagesize(),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);if(word==MAP_FAILED)return 67;
 /* Resolve/execute imports before the critical interval; no lazy lookup there. */
 (void)wake_call(3u|0x100u,word,0);(void)wait_call(3u|0x1000000u,word,0,1);(void)yield_call();
 describe("__ulock_wake",(void*)wake_call);describe("__ulock_wait",(void*)wait_call);describe("sched_yield",(void*)yield_call);if(fflush(stdout)||ferror(stdout))return 68;
 for(op=0;op<4;op++){
  struct trial t={.op=op};pthread_t worker,waiting;void *status=NULL;int has_waiter=0,park_observed=0,ok,safe;
  atomic_init(&t.ready,0);atomic_init(&t.go,0);atomic_init(&t.held,0);atomic_init(&t.observed_held,-1);atomic_init(&t.returned,0);atomic_init(&t.cleanup_calls,0);atomic_init(&t.waiter_ready,0);
  __atomic_store_n(word,0,__ATOMIC_RELEASE);if(pipe(t.pipefd))fatal(73);
  if(op==1){uint64_t until=ns()+UINT64_C(1000000000);if(pthread_create(&waiting,NULL,waiter,&t))fatal(74);has_waiter=1;wait_flag(&t.waiter_ready);while(ns()<until){if(parked(waiting)){park_observed=1;break;}pause_cpu();}if(!park_observed)fatal(75);}
  if(pthread_create(&worker,NULL,target,&t))fatal(76);wait_flag(&t.ready);if(pthread_cancel(worker))fatal(77);atomic_store(&t.go,1);
  if(pthread_join(worker,&status))fatal(78);joined++;
  /* Join before any rescue wake: a timeout cannot masquerade as target delivery.
   * The owned raw waiter already has its own two-second timeout. */
  if(has_waiter){if(pthread_join(waiting,NULL))fatal(79);joined++;}
  if(close(t.pipefd[0])||close(t.pipefd[1]))fatal(80);
  safe=atomic_load(&t.observed_held)==0&&atomic_load(&t.returned)==1;
  ok=status==PTHREAD_CANCELED&&atomic_load(&t.cleanup_calls)==1;
  if(op==1)ok=ok&&park_observed&&atomic_load(&t.returned)&&t.result==0&&t.waiter_result==0;
  if(op<3)all_deferred_safe=all_deferred_safe&&safe;else ok=ok&&!safe&&atomic_load(&t.observed_held)==1;
  result_ok=result_ok&&ok;
  printf("{\"type\":\"case\",\"name\":\"%s\",\"request_queued_before_call\":true,\"call_returned\":%s,\"held_at_cancel\":%d,\"modeled_deferred_safe\":%s,\"result\":%d,\"errno\":%d,\"real_waiter_park_observed\":%s,\"waiter_result\":%d,\"cleanup_rescue_wake\":false,\"joined_canceled\":%s,\"observation_valid\":%s}\n",names[op],atomic_load(&t.returned)?"true":"false",atomic_load(&t.observed_held),safe?"true":"false",t.result,t.error,park_observed?"true":"false",t.waiter_result,status==PTHREAD_CANCELED?"true":"false",ok?"true":"false");
  if(fflush(stdout)||ferror(stdout))fatal(81);
 }
 if(munmap(word,(size_t)getpagesize()))return 82;alarm(0);
 printf("{\"type\":\"complete\",\"observations_valid\":%s,\"three_calls_observed_deferred_safe\":%s,\"owned_threads_joined\":%u,\"scope\":\"self-owned native calls with modeled obligation; not Wine, signal-safety or macOS-version-wide proof\"}\n",result_ok?"true":"false",all_deferred_safe?"true":"false",joined);
 return result_ok&&fflush(stdout)==0&&!ferror(stdout)?0:1;
}
