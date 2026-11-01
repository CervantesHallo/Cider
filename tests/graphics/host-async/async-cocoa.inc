/* Cocoa queue barrier and pure method observation. No NT or PE callback. */
#include "async.h"
extern void macdrv_async_fixture_hold(bool);
extern void macdrv_async_fixture_method(unsigned int,uint64_t,uint32_t,bool,CGRect,CGRect);
void macdrv_async_fixture_schedule_hold(void)
{
    OnMainThreadAsync(^{macdrv_async_fixture_hold([NSThread isMainThread]);});
}
static void async_fixture_cocoa_method(struct remote_host_model *model,CGRect frame,CGRect clip)
{
    macdrv_async_fixture_method(model->context_id,model->generation,model->users,[NSThread isMainThread],frame,clip);
}
