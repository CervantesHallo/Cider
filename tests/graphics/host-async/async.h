#ifndef CIDER_HOST_ASYNC_H
#define CIDER_HOST_ASYNC_H
#include <stdint.h>
#include <stddef.h>
enum async_op { ASYNC_RUN=1,ASYNC_SNAPSHOT,ASYNC_FINISH,ASYNC_ABORT };
struct async_frame
{
    uint32_t size,version,operation,status,root,child,context,flags;
    uint64_t epoch,lease,ticket,latest_serial,serial[2];
    uint32_t admission_jobs[2],pre_validate[2],cocoa_validate[2],disposed[2],dispose_jobs[2];
    uint32_t admitted,method_seen,method_users,method_main;
    uint32_t hold_started,hold_done,hold_error,hold_timely;
    uint32_t queued[2],frozen_jobs,frozen_held;
    uint32_t observer,current_jobs,context_owned,host_read;
    uint32_t host_valid,run_complete,finish_observed,private_error;
    uint64_t hold_elapsed_ns,frame[4],clip[4];
    uint32_t reserved[4];
};
_Static_assert(sizeof(struct async_frame)==288,"async288");
_Static_assert(offsetof(struct async_frame,hold_elapsed_ns)==200,"elapsed200");
#endif
