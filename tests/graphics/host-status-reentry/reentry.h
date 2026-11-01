#ifndef CIDER_HOST_REENTRY_H
#define CIDER_HOST_REENTRY_H
#include <stdint.h>
#include <stddef.h>
enum reentry_op { REENTRY_ARM=1,REENTRY_SNAPSHOT,REENTRY_FINISH,REENTRY_RUN_CANCEL,REENTRY_SYNC_SEQUENCE,REENTRY_ABORT };
struct reentry_frame
{
    uint32_t size,version,operation,status,root,child,context,flags;
    uint64_t epoch,lease,ticket,sequence;
    uint32_t armed,fired,observer,callback_returned;
    uint32_t pre_controls,pre_jobs,pre_requests,pre_resource_done;
    uint32_t post_controls,post_jobs,post_requests,post_resource_done;
    uint32_t post_retirement,post_queued,post_root_open,post_active;
    uint32_t before_attached,before_ledger,before_indexed,after_attached,after_ledger,after_indexed;
    uint32_t hook_error,registered,owners,cleanup,setups,callback_main;
    uint32_t host_read,values_valid,worker_started,worker_done;
    uint32_t worker_error,callback_wait_error,worker_wait_error,callback_timely,worker_timely,sequence_synced,reserved[2];
    uint64_t cancel_sequence,callback_elapsed_ns,worker_elapsed_ns,reserved64;
};
_Static_assert(sizeof(struct reentry_frame)==256,"reentry frame256");
_Static_assert(offsetof(struct reentry_frame,epoch)==32,"identity32");
_Static_assert(offsetof(struct reentry_frame,host_read)==176,"ro176");
#endif
