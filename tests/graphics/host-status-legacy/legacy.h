#ifndef CIDER_HOST_LEGACY_H
#define CIDER_HOST_LEGACY_H
#include <stdint.h>
#include <stddef.h>
enum legacy_op { LEGACY_CREATE=1,LEGACY_SNAPSHOT,LEGACY_DROP,LEGACY_CLEANUP };
struct legacy_frame
{
    uint32_t size,version,operation,status,root,child,context,flags;
    uint64_t epoch,lease,ticket,sequence;
    uint32_t legacy_attached,legacy_ledger,legacy_indexed,managed_attached,managed_ledger,managed_indexed,managed_flag;
    uint32_t cocoa_valid,context_owned,window_owned,host_reader_owned,producer_reader_owned;
    uint32_t legacy_attempted,drop_sent,drop_busy,drop_done;
    uint32_t drop_callbacks,drop_result,legacy_absent,cleanup_done;
    uint32_t host_read,producer_read,host_values_valid,producer_values_valid;
    uint32_t last_reply,close_status,create_called,create_result,reserved[4];
};
_Static_assert(sizeof(struct legacy_frame)==192,"legacy frame192");
_Static_assert(offsetof(struct legacy_frame,epoch)==32,"identity32");
_Static_assert(offsetof(struct legacy_frame,host_read)==144,"pages144");
#endif
