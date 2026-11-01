#ifndef CIDER_CROSS_PROTOCOL_H
#define CIDER_CROSS_PROTOCOL_H
#include "fixture.h"
enum cross_operation { CROSS_PROBE=1,CROSS_FINISHED=2,CROSS_REPARENT=3,CROSS_RETURN=4 };
struct cross_message
{
    uint32_t size,version,operation,status;
    uint64_t nonce[2],sequence;
    struct live_host_fixture frame;
};
_Static_assert(sizeof(struct cross_message)==200,"cross message200");
_Static_assert(offsetof(struct cross_message,frame)==40,"frame40");
#endif
