#ifndef CIDER_HOST_STATUS_LIVE_FIXTURE_H
#define CIDER_HOST_STATUS_LIVE_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
enum live_host_operation { LIVE_SETUP=1,LIVE_PREPARE,LIVE_INSTALL,LIVE_CANCEL,LIVE_SNAPSHOT,LIVE_RELEASE_PRODUCER,LIVE_CLEANUP };
struct live_host_fixture
{
    uint32_t size,version,operation,status,root,child,context_id,flags;
    uint64_t epoch,lease,ticket,sequence,birth;
    uint32_t host_read,producer_read,cocoa_valid,attached,ledger,indexed;
    uint32_t context_owned,host_reader_owned,producer_reader_owned,window_owned;
    uint32_t close_status,reply_status,host_values_valid,producer_values_valid;
};
_Static_assert(sizeof(struct live_host_fixture)==128,"live frame128");
_Static_assert(offsetof(struct live_host_fixture,epoch)==32,"live identity offset");
_Static_assert(offsetof(struct live_host_fixture,host_read)==72,"live result offset");
#define LIVE_SETUP_DONE 1u
#define LIVE_PREPARE_SENT 2u
#define LIVE_INSTALL_SENT 4u
#define LIVE_CANCEL_SENT 8u
#define LIVE_RELEASE_SENT 16u
#define LIVE_CONTEXT_RELEASED 32u
#endif
