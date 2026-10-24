/* Exact extracted SetEvent/frame/notify bodies with EXPLICIT native doubles.
 * Not Wine/Windows equivalence or signal-handler validation. Own threads only. */
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <inttypes.h>
#include <mach/mach.h>
#include "wine/msync_coord.h"
typedef int32_t LONG;
typedef uint32_t NTSTATUS;
#define STATUS_PENDING 0x103u
#define STATUS_SUCCESS 0u
#define MAXIMUM_WAIT_OBJECTS 64
#define MSYNC_AUTO_EVENT 2
#define MSYNC_MANUAL_EVENT 3
#define MSYNC_AUTO_SERVER 5
#define UL_COMPARE_AND_WAIT_SHARED 3
#define ULF_WAKE_ALL 0x100
struct inproc_frame;
struct inproc_sync { LONG refcount; int fd; };
struct thread_data {
 struct inproc_frame *msync_frames;
 unsigned msync_private_frames,msync_control_depth,msync_in_transaction,tid;
 uint64_t msync_epoch;
};
struct event { int signaled,unused; unsigned short msync_type,refcount; int multiple_waiters; };
_Static_assert(sizeof(struct event)==16,"actual event field offsets");
struct coord_fd_record { struct coord_fd_record *next; int fd; uint64_t token; struct thread_data *thread_data; unsigned retired; };
static _Thread_local struct thread_data *current_data;
static struct thread_data *get_thread_data(void){assert(current_data);return current_data;}
#define GetCurrentThreadId() (get_thread_data()->tid)
static struct msync_coord coord;
static pthread_key_t coord_fd_key;
static unsigned coord_fd_key_ready,coord_forking,retire_on_slot;
_Alignas(8) static struct event event; /* Real slots are mapped at 16-byte offsets. */
static unsigned checks,failures,wakes,yields,messages,recovery_entries,closes,mode;
static int pipefd[2];
enum { AUTO,MANUAL,ALREADY,NO_IMPORT,NO_KEY,COLD,RETIRED,BUSY,DIRTY,EPOCH,SERVER_EVENT,FORK,MSG,RECOVER,GATE_HINT,FORK_NOTIFY,MODES };
static void check(int condition){checks++;failures+=!condition;}
static void alarm_expired(int sig){(void)sig;_exit(124);}
static inline LONG InterlockedCompareExchange(LONG *p,LONG value,LONG old){__atomic_compare_exchange_n(p,&old,value,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return old;}
static inline LONG InterlockedDecrement(LONG *p){return __atomic_sub_fetch(p,1,__ATOMIC_SEQ_CST);}
static void msync_control_begin_data(struct thread_data *d){__atomic_store_n(&d->msync_control_depth,d->msync_control_depth+1,__ATOMIC_RELAXED);__atomic_signal_fence(__ATOMIC_SEQ_CST);}
static void msync_control_end_data(struct thread_data *d){__atomic_signal_fence(__ATOMIC_SEQ_CST);check(d->msync_control_depth>0);--d->msync_control_depth;}
static void release_inproc_sync(struct inproc_sync *s){int fd=s->fd;LONG r=InterlockedDecrement(&s->refcount);(void)fd;check(r>=0);if(!r){closes++;_exit(90);}}
#include "frame-extracted.h"
static int policy_state(void){int old;if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&old)||pthread_setcancelstate(old,NULL))_exit(91);return old;}
static void *coord_slot(unsigned idx,int map){check(!map);if(retire_on_slot){__atomic_store_n(&msync_coord_epochs(&coord)[1],3,__ATOMIC_RELEASE);retire_on_slot=0;}return idx==1?&event:NULL;}
/* Native bookkeeping double; no Wine signal replay. */
static void coord_depart(struct thread_data *d){__atomic_store_n(&d->msync_in_transaction,0,__ATOMIC_SEQ_CST);if(coord_forking){inproc_prepare_kernel();__ulock_wake(1|MSYNC_COORD_UL_WAKE_ALL,&d->msync_in_transaction,0);}}
static void set_active(int active){if(active)get_thread_data()->msync_in_transaction=1;else coord_depart(get_thread_data());}
/* Recovery uses the exact shared coordinator, with explicit fixture fd/token. */
static void coord_enter(struct msync_coord_guard *g){check(policy_state()==PTHREAD_CANCEL_DISABLE);check(get_thread_data()->msync_control_depth==0);recovery_entries++;msync_coord_enter(&coord,-1,7,g);}
static int wake_call(uint32_t flags,void *shm,uint64_t value){
 check(get_thread_data()->msync_control_depth==(coord.header->owner?1u:0u));check(event.signaled==1);wakes++;
 if(mode==FORK_NOTIFY)coord_forking=1;
 return __ulock_wake(flags,shm,value);
}
static int yield_call(void){check(!coord.header->owner&&!get_thread_data()->msync_in_transaction);check(get_thread_data()->msync_control_depth==1);yields++;return sched_yield();}
static int (*ready_event_wake)(uint32_t,void*,uint64_t);
static int (*ready_event_yield)(void);
static const mach_msg_bits_t msgh_bits_send=MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
static mach_port_t server_port=17;
/* A real read cancellation point substitutes for the unproved Mach exchange.
 * The parent pre-fills the private pipe; the worker's cancellation is pending. */
static mach_msg_return_t mach_msg2(mach_msg_header_t *h,uint64_t options,mach_msg_size_t send,mach_msg_size_t receive,mach_port_t port,uint64_t timeout,uint32_t priority){
 char byte;ssize_t got;(void)h;(void)send;(void)receive;(void)port;(void)timeout;(void)priority;
 check(policy_state()==PTHREAD_CANCEL_DISABLE);check(get_thread_data()->msync_control_depth==((options&MACH_SEND_TIMEOUT)?1u:0u));messages++;
 got=read(pipefd[0],&byte,1);check(got==1&&byte=='X');
 if(mode==RECOVER&&!(options&MACH_SEND_TIMEOUT))check(coord.header->state==MSYNC_COORD_APPLIED);
 return mode==RECOVER&&messages==1?MACH_SEND_TIMED_OUT:MACH_MSG_SUCCESS;
}
#include "set-event-extracted.h"
static int coord_notify(unsigned idx,void *shm,int blocking){return signal_all(shm,idx,blocking);}
struct context {struct thread_data data;struct inproc_frame frame;struct inproc_sync sync;struct coord_fd_record record;atomic_int ready,go;unsigned queued,cleanup_count;NTSTATUS status;LONG previous;};
static void cleanup(void *opaque){struct context *c=opaque;c->cleanup_count++;check(!c->data.msync_frames&&!c->data.msync_private_frames&&!c->data.msync_control_depth&&!c->data.msync_in_transaction);check(!c->frame.count&&c->sync.refcount==1);check(coord.header->owner==(mode==BUSY?9u:0u));}
static void cpu_pause(void){
#ifdef __x86_64__
 __asm__ volatile("pause":::"memory");
#else
 __asm__ volatile("yield":::"memory");
#endif
}
static void *worker(void *opaque){
 struct context *c=opaque;current_data=&c->data;
 if(pthread_setspecific(coord_fd_key,mode==COLD?NULL:&c->record)||pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL)||pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL))_exit(92);
 pthread_cleanup_push(cleanup,c);
 inproc_frame_enter_ready(&c->frame);c->frame.pins[c->frame.count++]=&c->sync;
 atomic_store(&c->ready,1);while(!atomic_load(&c->go))cpu_pause();
 c->status=msync_try_set_event_obj(1,&c->previous);
 check(c->data.msync_control_depth==1&&c->frame.count==1&&c->data.msync_private_frames==1);
 if(c->status==STATUS_PENDING){check(event.signaled==0);inproc_frame_promote(&c->frame);}
 else {check(c->status==STATUS_SUCCESS&&event.signaled==1);check(c->previous==(mode==ALREADY?1:0));}
 inproc_frame_leave(&c->frame);
 if(c->queued)pthread_testcancel();
 pthread_cleanup_pop(1);
 return NULL;
}
int main(void){
 /* This fixture exercises ready entry; full entry has separate exact-frame evidence. */
 (void)inproc_frame_enter;
 unsigned queued,total=0;if(getpid()!=getsid(0)||getpid()!=getpgrp())return 64;
 signal(SIGALRM,alarm_expired);alarm(25);
 if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL)||pthread_key_create(&coord_fd_key,NULL))return 65;
 coord.header=mmap(NULL,MSYNC_COORD_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);if(coord.header==MAP_FAILED)return 66;
 coord.slot=coord_slot;coord.notify=coord_notify;coord.before_wake=inproc_prepare_kernel;coord.set_active=set_active;coord.cancellation_managed=1;
 /* Resolve native imports before pending-cancellation cases. */
 (void)__ulock_wake(3|0x100,&coord.header->wake_sequence,0);(void)sched_yield();
 for(queued=0;queued<2;queued++)for(mode=0;mode<MODES;mode++){
  struct context c={0};pthread_t thread;void *status=NULL;unsigned first=failures;char bytes[2]={'X','X'};
  coord.header->owner=mode==BUSY?9:0;coord.header->state=mode==DIRTY?MSYNC_COORD_COMMITTED:MSYNC_COORD_CLEAN;coord.header->sequence=0;coord.header->wait_hint=mode==GATE_HINT;
  __atomic_store_n(&msync_coord_epochs(&coord)[1],2,__ATOMIC_RELEASE);event=(struct event){.signaled=mode==ALREADY,.msync_type=mode==MANUAL?MSYNC_MANUAL_EVENT:mode==SERVER_EVENT?MSYNC_AUTO_SERVER:MSYNC_AUTO_EVENT,.multiple_waiters=mode==MSG||mode==RECOVER};
  wakes=yields=messages=recovery_entries=closes=0;coord_forking=mode==FORK;coord_fd_key_ready=mode!=NO_KEY;retire_on_slot=mode==EPOCH;
  ready_event_wake=mode==NO_IMPORT?NULL:wake_call;ready_event_yield=yield_call;
  c.data.tid=1;c.data.msync_epoch=2;c.sync.refcount=2;c.sync.fd=13;c.record=(struct coord_fd_record){.fd=-1,.token=7,.thread_data=&c.data,.retired=mode==RETIRED};c.previous=-1;c.queued=queued;atomic_init(&c.ready,0);atomic_init(&c.go,0);
  if(pipe(pipefd)||write(pipefd[1],bytes,sizeof(bytes))!=(ssize_t)sizeof(bytes)||pthread_create(&thread,NULL,worker,&c))return 67;
  while(!atomic_load(&c.ready))cpu_pause();if(queued&&pthread_cancel(thread))return 68;atomic_store(&c.go,1);
  if(pthread_join(thread,&status))return 69;total++;
  check(status==(queued?PTHREAD_CANCELED:NULL));check(c.cleanup_count==1);check(!closes);
  if(mode>=NO_IMPORT&&mode<=FORK){check(c.status==STATUS_PENDING);check(!wakes&&!yields&&!messages);}
  else {check(c.status==STATUS_SUCCESS);check(yields==(mode==MANUAL||mode==ALREADY?0u:1u));check(messages==(mode==MSG?1u:mode==RECOVER?2u:0u));check(recovery_entries==(mode==RECOVER?1u:0u));check(coord.header->state==MSYNC_COORD_CLEAN);}
  if(close(pipefd[0])||close(pipefd[1]))return 70;
  printf("{\"type\":\"case\",\"mode\":%u,\"queued_cancel\":%s,\"passed\":%s,\"wakes\":%u,\"yields\":%u,\"messages\":%u,\"recovery_entries\":%u}\n",mode,queued?"true":"false",failures==first?"true":"false",wakes,yields,messages,recovery_entries);
 }
 if(pthread_key_delete(coord_fd_key)||munmap(coord.header,MSYNC_COORD_SIZE))return 71;alarm(0);
 printf("{\"type\":\"complete\",\"passed\":%s,\"checks\":%u,\"failures\":%u,\"owned_threads_joined\":%u,\"scope\":\"exact SetEvent/notify/frame and production coordinator with explicit native doubles; not Wine/Windows/signal proof\"}\n",failures?"false":"true",checks,failures,total);
 return !failures&&fflush(stdout)==0&&!ferror(stdout)?0:1;
}
