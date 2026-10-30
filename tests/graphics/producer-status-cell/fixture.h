#ifndef CIDER_PRODUCER_CELL_FIXTURE_H
#define CIDER_PRODUCER_CELL_FIXTURE_H
#include <stdint.h>
struct producer_cell_fixture
{
    uint32_t size,version,operation,status,root,child,request_inflight,cleanup_inflight;
    uint64_t lease,sequence;
    uint32_t watched,finalized,completion_seen,process_open,pending_models,pe_watches,pe_timers,reserved;
    uint32_t decision_seen,decision_detached,status_owner,status_cleanup;
};
_Static_assert(sizeof(struct producer_cell_fixture)==96,"producer cell fixture96");
#endif
