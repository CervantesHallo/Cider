#ifndef CIDER_GENERATION_FIXTURE_H
#define CIDER_GENERATION_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
enum generation_op { GENERATION_RUN=1,GENERATION_SNAPSHOT,GENERATION_CLEANUP };
struct generation_frame
{
    uint32_t size,version,operation,status,root,child,context,flags;
    uint64_t epoch,lease,old_generation,new_generation;
    uint32_t outer_called,outer_result,inner_called,inner_result;
    uint32_t hook_seen,hook_main,hook_error,before_old_attached;
    uint32_t before_old_ledger,before_old_indexed,after_old_attached,after_old_ledger;
    uint32_t after_old_indexed,after_new_attached,after_new_ledger,after_new_indexed;
    uint32_t final_old_attached,final_old_ledger,final_old_indexed,final_new_attached;
    uint32_t final_new_ledger,final_new_indexed,cocoa_valid,context_owned;
    uint32_t window_owned,producer_reader_owned,producer_read,producer_valid;
    uint32_t owner_mask,busy_mask,done_mask,absent;
    uint32_t callbacks_old,callbacks_new,cleanup_done,close_status;
    uint32_t old_managed,new_managed,hook_users,retire_before,retire_after,reserved[7];
};
_Static_assert(sizeof(struct generation_frame)==256,"generation256");
_Static_assert(offsetof(struct generation_frame,old_generation)==48,"caller labels48");
#endif
