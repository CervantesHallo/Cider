#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <inttypes.h>
#include "wine/msync_coord.h"
typedef uint32_t NTSTATUS;
#define STATUS_PENDING 0x103u
#define STATUS_SUCCESS 0u
#define MSYNC_AUTO_EVENT 2
#define MSYNC_MANUAL_EVENT 3
#define MSYNC_AUTO_SERVER 5
struct event { int signaled,unused,msync_type,multiple_waiters; };
struct thread_data { unsigned msync_private_frames,msync_control_depth,msync_in_transaction,tid; uint64_t msync_epoch; };
struct coord_fd_record { struct coord_fd_record *next; int fd; uint64_t token; struct thread_data *thread_data; unsigned retired; };
static struct thread_data data={1,1,0,1,2};
static struct thread_data *get_thread_data(void) { return &data; }
#define GetCurrentThreadId() (get_thread_data()->tid)
static struct msync_coord coord;
static pthread_key_t coord_fd_key;
static unsigned coord_fd_key_ready,coord_forking,retire_on_slot,calls,checks,failures;
static struct event event;
static void *coord_slot(unsigned idx,int map) {
 (void)map;
 if(retire_on_slot) { __atomic_store_n(&msync_coord_epochs(&coord)[1],3,__ATOMIC_RELEASE);retire_on_slot=0; }
 return idx==1?&event:NULL;
}
static void before_wake(void) { if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL))_exit(4);calls++; }
/* Deliberate test double of native control bookkeeping; no Wine signal replay. */
static void coord_depart(struct thread_data *d) {
 __atomic_store_n(&d->msync_in_transaction,0,__ATOMIC_SEQ_CST);
 if(__atomic_load_n(&coord_forking,__ATOMIC_SEQ_CST)) { before_wake();__ulock_wake(1|MSYNC_COORD_UL_WAKE_ALL,&d->msync_in_transaction,0); }
}
#include "ready-event-extracted.h"
static void check(const char *name,int ok) { checks++;failures+=!ok;printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s}\n",name,ok?"true":"false"); }
static void reset(void) {
 coord.header->owner=0;coord.header->state=MSYNC_COORD_CLEAN;coord.header->sequence=0;coord.header->wait_hint=0;
 __atomic_store_n(&msync_coord_epochs(&coord)[1],2,__ATOMIC_RELEASE);event=(struct event){1,0,MSYNC_AUTO_EVENT,0};data.msync_in_transaction=0;
}
int main(void) {
 struct coord_fd_record record={.fd=-1,.token=7,.thread_data=&data};NTSTATUS status;int old;
 if(getpid()!=getpgrp()||getpid()!=getsid(0))return 64;
 coord.header=mmap(NULL,MSYNC_COORD_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);if(coord.header==MAP_FAILED)return 65;
 coord.slot=coord_slot;coord.before_wake=before_wake;
 if(pthread_key_create(&coord_fd_key,NULL))return 66;
 reset();check("uninitialized_key_falls_back",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1);
 coord_fd_key_ready=1;check("cold_thread_key_falls_back",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1);
 if(pthread_setspecific(coord_fd_key,&record))return 67;
 reset();status=msync_try_wait_ready_event(1);check("auto_actual_commit",status==STATUS_SUCCESS&&event.signaled==0&&coord.header->sequence==1&&coord.header->state==MSYNC_COORD_CLEAN&&coord.header->owner==0&&data.msync_in_transaction==0);
 reset();event.msync_type=MSYNC_MANUAL_EVENT;check("manual_retains_signal",msync_try_wait_ready_event(1)==STATUS_SUCCESS&&event.signaled==1&&coord.header->sequence==0);
 reset();event.signaled=0;check("unready_has_no_effect",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==0&&coord.header->sequence==0&&data.msync_in_transaction==0);
 reset();event.msync_type=MSYNC_AUTO_SERVER;check("server_event_falls_back",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1);
 reset();coord.header->owner=9;check("busy_owner_has_no_effect",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1&&coord.header->owner==9&&data.msync_in_transaction==0);
 reset();coord.header->state=MSYNC_COORD_COMMITTED;check("dirty_journal_untouched",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1&&coord.header->state==MSYNC_COORD_COMMITTED&&coord.header->owner==0);
 reset();record.retired=1;check("fork_child_retired_record",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1);record.retired=0;
 reset();retire_on_slot=1;check("epoch_retired_between_checks",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1&&coord.header->sequence==0&&data.msync_in_transaction==0);
 reset();data.msync_control_depth=0;check("unprotected_scope_falls_back",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1);data.msync_control_depth=1;
 reset();check("missing_mapping_falls_back",msync_try_wait_ready_event(2)==STATUS_PENDING&&event.signaled==1);
 reset();coord_forking=1;if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&old))return 68;
 check("closed_fork_admission_falls_back",msync_try_wait_ready_event(1)==STATUS_PENDING&&event.signaled==1&&data.msync_in_transaction==0&&calls==1);
 {int observed;if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&observed))return 69;check("fork_wake_mask_retained",observed==PTHREAD_CANCEL_DISABLE);}
 if(pthread_setcancelstate(old,NULL))return 70;coord_forking=0;
 if(pthread_setspecific(coord_fd_key,NULL)||pthread_key_delete(coord_fd_key)||munmap(coord.header,MSYNC_COORD_SIZE))return 71;
 printf("{\"type\":\"complete\",\"passed\":%s,\"checks\":%u,\"failures\":%u,\"cleanupChecked\":true,\"scope\":\"exact production ready helper with constructed inputs and native test doubles, not Wine runtime\"}\n",failures?"false":"true",checks,failures);
 return !failures&&fflush(stdout)==0&&!ferror(stdout)?0:1;
}
