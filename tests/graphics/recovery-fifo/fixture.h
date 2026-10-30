#ifndef CIDER_RECOVERY_FIFO_FIXTURE_H
#define CIDER_RECOVERY_FIFO_FIXTURE_H
#include <stdint.h>
struct recovery_fifo_fixture
{
    uint32_t size,version,operation,status,root,target,flags,acquire_error;
    uint64_t older,newer;
    uint32_t setups,queued,busy,active,older_present,newer_present,older_last,newer_last;
    uint32_t older_retries,newer_retries,pe_watches,pe_timers,query_status,set_status,reserved[2],older_position,newer_position;
};
_Static_assert(sizeof(struct recovery_fifo_fixture)==120,"fifo frame120");
/* flags: 1 targetRootpresent, 2 olderClearApplied, 4 newerClearApplied,
 * 8 olderClearInProgress,16 newerClearInProgress. All operations one stream. */
#endif
