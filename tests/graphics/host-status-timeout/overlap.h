#ifndef CIDER_HOST_OVERLAP_H
#define CIDER_HOST_OVERLAP_H
#include <stdint.h>
#include <stddef.h>
enum overlap_op { OVERLAP_ARM=1, OVERLAP_SNAPSHOT, OVERLAP_RUN, OVERLAP_FINISH };
struct overlap_frame
{
    uint32_t size,version,operation,status,root,child,event,flags;
    uint64_t epoch,lease,ticket,sequence;
    uint32_t bound,held,done,timely,wait_status,started,returned,error;
    uint32_t controls,jobs,retirement,queued,registered,owners,resource_done,terminal;
    uint32_t requests,cell,registry_count,observer,event_borrow,active,cleanup,setups;
    uint32_t delivered,last_error,elapsed_ms,sender_clock;
};
_Static_assert(sizeof(struct overlap_frame)==176,"timeout frame176");
_Static_assert(offsetof(struct overlap_frame,epoch)==32,"identity32");
_Static_assert(offsetof(struct overlap_frame,controls)==96,"witness96");
#endif
