#ifndef CIDER_CROSS_PROTOCOL_H
#define CIDER_CROSS_PROTOCOL_H
#include "fixture.h"
enum cross_operation { CROSS_PROBE=1,CROSS_FINISHED=2 };
struct cross_message
{
    uint32_t size,version,operation,status;
    uint64_t nonce[2],sequence;
    struct live_host_fixture frame;
};
_Static_assert(sizeof(struct cross_message)==200,"cross message200");
_Static_assert(offsetof(struct cross_message,frame)==40,"frame40");
#define PIXEL_CAPTURE_MAGIC 0x43504731u
struct pixel_capture
{
    uint32_t magic,size,version,phase,color,window_number,host_mac_pid,host_wine_pid;
    uint64_t nonce[2];
    uint32_t producer_wine_pid,context,width,height;
};
_Static_assert(sizeof(struct pixel_capture)==64,"capture64");
#endif
