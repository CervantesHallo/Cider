#ifndef CIDER_HOST_STATUS_LIVE_FIXTURE_H
#define CIDER_HOST_STATUS_LIVE_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
enum live_host_operation { LIVE_SETUP=1,LIVE_PREPARE,LIVE_INSTALL,LIVE_CANCEL,LIVE_SNAPSHOT,LIVE_RELEASE_PRODUCER,LIVE_CLEANUP,LIVE_HOST_SETUP,LIVE_HOST_PROBE,LIVE_HOST_FINISH };
struct live_host_fixture
{
    uint32_t size,version,operation,status,root,child,context_id,flags;
    uint64_t epoch,lease,ticket,sequence,birth;
    uint32_t host_read,producer_read,cocoa_valid,attached,ledger,indexed;
    uint32_t context_owned,host_reader_owned,producer_reader_owned,window_owned;
    uint32_t close_status,reply_status,host_values_valid,producer_values_valid;
    uint32_t root_pid,producer_pid,role,probe_bound;
    uint64_t root_birth,probe_sequence;
};
_Static_assert(sizeof(struct live_host_fixture)==160,"cross frame160");
_Static_assert(offsetof(struct live_host_fixture,epoch)==32,"live identity offset");
_Static_assert(offsetof(struct live_host_fixture,host_read)==72,"live result offset");
#define LIVE_SETUP_DONE 1u
#define LIVE_PREPARE_SENT 2u
#define LIVE_INSTALL_SENT 4u
#define LIVE_CANCEL_SENT 8u
#define LIVE_RELEASE_SENT 16u
#define LIVE_CONTEXT_RELEASED 32u
#define LIVE_FIXTURE_VERSION 2u
#define LIVE_ROLE_PRODUCER 1u
#define LIVE_ROLE_HOST 2u
#endif
