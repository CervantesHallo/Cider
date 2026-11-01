/* Owned diagnostic Metal work. Present requested, GPU completion, readback and
 * WindowServer screenshot evidence remain distinct. No NT/PE work on Cocoa. */
#include "pixel.h"
#include <unistd.h>
static struct
{
    void *context; /* borrow: original live_host owner outlives drain */
    CAMetalLayer *layer;
    id<MTLCommandQueue> queue;
    id<MTLCommandBuffer> command;
    id<CAMetalDrawable> drawable;
    id<MTLBuffer> readback;
    BOOL configured,old_framebuffer,commit_attempted,uncertain;
} pixel_metal;
static BOOL pixel_release_metal(void)
{
    @try
    {
        id<MTLCommandBuffer> command=pixel_metal.command;pixel_metal.command=nil;[command release];
        id<CAMetalDrawable> drawable=pixel_metal.drawable;pixel_metal.drawable=nil;[drawable release];
        id<MTLBuffer> buffer=pixel_metal.readback;pixel_metal.readback=nil;[buffer release];
        id<MTLCommandQueue> queue=pixel_metal.queue;pixel_metal.queue=nil;[queue release];
        return YES;
    }
    @catch(NSException *exception)
    {pixel_metal.uncertain=YES;ERR(@"pixel Metal release exception %@\n",[exception name]);return NO;}
}
bool macdrv_pixel_fixture_drain(void *context)
{
    @autoreleasepool
    {

    if(!pixel_metal.context)return true;
    if(pixel_metal.context!=context||pixel_metal.uncertain)return false;
    @try
    {
        if(pixel_metal.commit_attempted)
        {
            MTLCommandBufferStatus status=pixel_metal.command.status;
            if(status!=MTLCommandBufferStatusCompleted&&status!=MTLCommandBufferStatusError)return false;
        }
        if(!pixel_release_metal())return false;
        if(pixel_metal.configured)
        {
            __block BOOL restored=NO;
            OnMainThread(^{
                @try {pixel_metal.layer.framebufferOnly=pixel_metal.old_framebuffer;restored=YES;}
                @catch(NSException *exception)
                {pixel_metal.uncertain=YES;ERR(@"pixel layer restore exception %@\n",[exception name]);}
            });
            if(!restored)return false;
        }
        pixel_metal.context=NULL;pixel_metal.layer=nil;pixel_metal.configured=NO;pixel_metal.commit_attempted=NO;
        return true;
    }
    @catch(NSException *exception)
    {pixel_metal.uncertain=YES;ERR(@"pixel drain exception %@\n",[exception name]);return false;}
    }
}
void macdrv_pixel_fixture_render(void *context,struct pixel_frame *f)
{
    @autoreleasepool
    {

    const NSUInteger width=160,height=100,row_bytes=768;
    uint32_t expected=f->operation==PIXEL_RED?0xffff0000u:0xff0000ffu;
    f->status=PIXEL_METAL;f->mac_pid=(uint32_t)getpid();
    if(!macdrv_pixel_fixture_drain(context)){f->status=PIXEL_PENDING;f->resource_pending=1;return;}
    pixel_metal.context=context;
    @try
    {
        pixel_metal.layer=macdrv_swapchain_get_layer(context);
        if(!pixel_metal.layer)return;
        __block BOOL configured=NO;
        OnMainThread(^{
            @try
            {
                pixel_metal.old_framebuffer=pixel_metal.layer.framebufferOnly;
                pixel_metal.configured=YES; /* before ambiguous property side effects */
                pixel_metal.layer.framebufferOnly=NO; /* diagnostic readback cost, not product setting */
                pixel_metal.layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
                pixel_metal.layer.drawableSize=CGSizeMake(width,height);
                configured=YES;
            }
            @catch(NSException *exception)
            {pixel_metal.uncertain=YES;ERR(@"pixel layer configure exception %@\n",[exception name]);}
        });
        f->framebuffer_only_before=pixel_metal.old_framebuffer;
        if(!configured){f->status=PIXEL_EXCEPTION;return;}
        id<MTLDevice> device=macdrv_swapchain_get_device(context);
        if(!device)return;
        pixel_metal.queue=[device newCommandQueue];if(!pixel_metal.queue)return;
        pixel_metal.drawable=[[pixel_metal.layer nextDrawable] retain];if(!pixel_metal.drawable)return;
        id<MTLTexture> texture=pixel_metal.drawable.texture;
        f->width=(uint32_t)texture.width;f->height=(uint32_t)texture.height;f->format=(uint32_t)texture.pixelFormat;
        if(texture.width!=width||texture.height!=height||texture.pixelFormat!=MTLPixelFormatBGRA8Unorm||texture.framebufferOnly)return;
        pixel_metal.readback=[device newBufferWithLength:row_bytes*height options:MTLResourceStorageModeShared];
        if(!pixel_metal.readback||!pixel_metal.readback.contents)return;
        memset(pixel_metal.readback.contents,0xa5,row_bytes*height);
        pixel_metal.command=[[pixel_metal.queue commandBuffer] retain];if(!pixel_metal.command)return;
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=f->operation==PIXEL_RED?MTLClearColorMake(1,0,0,1):MTLClearColorMake(0,0,1,1);
        id<MTLRenderCommandEncoder> render=[pixel_metal.command renderCommandEncoderWithDescriptor:pass];if(!render)return;
        [render endEncoding];
        id<MTLBlitCommandEncoder> blit=[pixel_metal.command blitCommandEncoder];if(!blit)return;
        [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:pixel_metal.readback destinationOffset:0
            destinationBytesPerRow:row_bytes destinationBytesPerImage:row_bytes*height];
        [blit endEncoding];
        f->color=f->operation==PIXEL_RED?1:2;
        f->present_requested=1;[pixel_metal.command presentDrawable:pixel_metal.drawable];
        pixel_metal.commit_attempted=YES;f->commit_attempted=1; /* before possible commit effects */
        [pixel_metal.command commit];[pixel_metal.command waitUntilCompleted];
        f->command_status=(uint32_t)pixel_metal.command.status;
        if(pixel_metal.command.status!=MTLCommandBufferStatusCompleted||pixel_metal.command.error)return;
        f->completed=1;
        const unsigned char *bytes=pixel_metal.readback.contents;
        for(NSUInteger y=0;y<height;++y)
            for(NSUInteger x=0;x<width;++x)
            {
                uint32_t value;memcpy(&value,bytes+y*row_bytes+x*4,4);
                if(!x&&!y)f->first_pixel=value;
                if(x==width-1&&y==height-1)f->last_pixel=value;
                if(value!=expected)++f->mismatches;
            }
        f->readback_ok=f->mismatches==0;f->resource_pending=1;
        if(f->readback_ok)f->status=PIXEL_OK;
    }
    @catch(NSException *exception)
    {pixel_metal.uncertain=YES;f->status=PIXEL_EXCEPTION;f->resource_pending=1;ERR(@"pixel render exception %@\n",[exception name]);}
    }
}
void macdrv_pixel_fixture_window(void *object,struct pixel_frame *f)
{
    WineWindow *window=object;f->status=PIXEL_STATE;f->mac_pid=(uint32_t)getpid();
    OnMainThread(^{
        @try
        {
            NSInteger number=window.windowNumber;
            if(number>0&&(uint64_t)number<=UINT32_MAX)
            {f->window_number=(uint32_t)number;f->status=PIXEL_OK;}
        }
        @catch(NSException *exception)
        {f->status=PIXEL_EXCEPTION;ERR(@"pixel window probe exception %@\n",[exception name]);}
    });
}
