#include "pixel.h"
extern void macdrv_pixel_fixture_render(void *,struct pixel_frame *);
extern void macdrv_pixel_fixture_window(void *,struct pixel_frame *);
NTSTATUS macdrv_pixel_fixture(void *args)
{
    struct pixel_frame *f=args;unsigned int i;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<PIXEL_RED||f->operation>PIXEL_DRAIN)
        return STATUS_INVALID_PARAMETER;
    for(i=0;i<4;++i)if(f->reserved[i])return STATUS_INVALID_PARAMETER;
    f->status=PIXEL_STATE;
    if(f->operation==PIXEL_HOST_WINDOW)
    {
        if(cross_host.used&&cross_host.window&&wine_server_ptr_handle(f->root)==cross_host.root)
            macdrv_pixel_fixture_window(cross_host.window,f);
    }
    else if(live_host.context&&live_host.install_sent&&wine_server_ptr_handle(f->root)==live_host.root)
    {
        f->context=live_host.context_id;f->lease=live_host.lease;f->ticket=live_host.ticket;
        if(f->operation==PIXEL_DRAIN)
        {f->cleanup_done=macdrv_pixel_fixture_drain(live_host.context);f->resource_pending=!f->cleanup_done;f->status=f->cleanup_done?PIXEL_OK:PIXEL_PENDING;}
        else macdrv_pixel_fixture_render(live_host.context,f);
    }
    return STATUS_SUCCESS;
}
