/* Production try-clean helper, constructed journal states, actual guarded wake.
 * Not Wine execution, public Win32 equivalence, or process-death evidence. */
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <inttypes.h>
#include "wine/msync_coord.h"
static struct msync_coord_header header;
static unsigned calls,checks,failures;
static int callback_old=-1;
static void before_wake(void) {
 if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&callback_old))_exit(2);
 calls++;
}
static void check(const char *name,int ok) {
 checks++;failures+=!ok;
 printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s}\n",name,ok?"true":"false");
}
int main(void) {
 struct msync_coord coord={.header=&header,.before_wake=before_wake};
 struct msync_coord_guard guard;
 struct msync_coord_entry saved;
 unsigned state;int old;
 if(getpid()!=getpgrp()||getpid()!=getsid(0))return 64;
 check("empty_clean_acquires",msync_coord_try_clean(&coord,-1,7,&guard)==1&&header.owner==7);
 check("clean_state_unchanged",header.state==MSYNC_COORD_CLEAN&&calls==0);
 msync_coord_give(&guard);
 check("uncontended_no_kernel_callback",header.owner==0&&calls==0);
 header.owner=9;
 check("busy_owner_untouched",!msync_coord_try_clean(&coord,-1,7,&guard)&&header.owner==9&&calls==0);
 header.owner=0;
 check("zero_token_no_acquisition",!msync_coord_try_clean(&coord,-1,0,&guard)&&header.owner==0);
 for(state=MSYNC_COORD_PREPARING;state<=MSYNC_COORD_APPLIED;state++) {
  header.state=state;header.count=1;header.sequence=17;header.entries[0]=(struct msync_coord_entry){3,1,123};saved=header.entries[0];
  check("dirty_released_without_recovery",!msync_coord_try_clean(&coord,-1,7,&guard)&&header.owner==0&&header.state==state&&header.count==1&&header.sequence==17&&memcmp(&saved,&header.entries[0],sizeof(saved))==0);
 }
 header.state=MSYNC_COORD_CLEAN;header.wait_hint=1;
 if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&old))return 65;
 check("hinted_clean_acquires",msync_coord_try_clean(&coord,-1,7,&guard));
 msync_coord_give(&guard);
 check("kernel_callback_masks_before_wake",calls==1&&callback_old==PTHREAD_CANCEL_ENABLE&&header.owner==0&&header.wake_sequence==1&&header.wait_hint==0);
 {int observed;if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&observed))return 66;check("mask_retained_until_caller_cleanup",observed==PTHREAD_CANCEL_DISABLE);}
 if(pthread_setcancelstate(old,NULL))return 67;
 printf("{\"type\":\"complete\",\"passed\":%s,\"checks\":%u,\"failures\":%u,\"actual_guarded_ulock_wake\":true}\n",failures?"false":"true",checks,failures);
 return !failures&&fflush(stdout)==0&&!ferror(stdout)?0:1;
}
