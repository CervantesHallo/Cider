#ifndef CIDER_REQUEST_FIXTURE_H
#define CIDER_REQUEST_FIXTURE_H
#include <stdint.h>
struct request_fixture
{
    uint32_t size,version,operation,status,root,child,request_inflight,cleanup_inflight;
    uint64_t lease,sequence;
    uint32_t watched,finalized,completion_seen,process_open,pending_models,pe_watches,pe_timers,reserved;
    uint32_t decision_seen,decision_detached;
};
_Static_assert(sizeof(struct request_fixture)==88,"request fixture88");
#endif
