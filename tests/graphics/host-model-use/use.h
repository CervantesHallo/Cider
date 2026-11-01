#ifndef CIDER_MODEL_USE_H
#define CIDER_MODEL_USE_H
#include <stdint.h>
#include <stddef.h>
enum use_result { USE_OK=0,USE_BAD_STATE=1,USE_EXCEPTION=2 };
enum use_op { USE_UPDATE=1,USE_FLIP,USE_RESTORE,USE_REMOVE };
struct use_frame
{
    uint32_t size,version,operation,status,root,child,context,flags;
    uint64_t epoch,lease,ticket,sequence;
    uint32_t mode,initial_mode,op_called,hook_seen;
    uint32_t hook_users,hook_installing,hook_busy,hook_retire_requested;
    uint32_t hook_attached,hook_ledger,hook_indexed,hook_main;
    uint32_t after_remove_attached,after_remove_ledger,after_remove_indexed,hook_error;
    uint32_t users_after,attached_after,ledger_after,indexed_after;
    uint32_t context_owned,host_read,host_valid,cleanup_done;
    uint32_t retire_after_request,geometry_valid,cocoa_valid,reserved;
    uint64_t scale,source_scale,container[4],source[4];
};
_Static_assert(sizeof(struct use_frame)==256,"model use256");
_Static_assert(offsetof(struct use_frame,scale)==176,"geometry176");
#endif
