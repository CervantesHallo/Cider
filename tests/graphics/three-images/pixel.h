#ifndef CIDER_PIXEL_FIXTURE_H
#define CIDER_PIXEL_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
enum pixel_op { PIXEL_RED=1,PIXEL_BLUE,PIXEL_HOST_WINDOW,PIXEL_DRAIN };
enum pixel_result { PIXEL_OK=0,PIXEL_STATE=1,PIXEL_METAL=2,PIXEL_EXCEPTION=3,PIXEL_PENDING=4 };
struct pixel_frame
{
    uint32_t size,version,operation,status,root,context,color,flags;
    uint64_t lease,ticket;
    uint32_t width,height,format,command_status;
    uint32_t commit_attempted,completed,present_requested,readback_ok;
    uint32_t first_pixel,last_pixel,window_number,mac_pid;
    uint32_t resource_pending,cleanup_done,framebuffer_only_before,mismatches,reserved[4];
};
_Static_assert(sizeof(struct pixel_frame)==128,"pixel128");
#endif
