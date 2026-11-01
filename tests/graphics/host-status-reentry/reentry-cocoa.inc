/* Main-thread-only model observation around the actual ordinary Wine-thread CANCEL.
 * No model pointer survives this callback; Native owns independent h/p refs. */
#include "reentry.h"
extern bool macdrv_reentry_cancel_after_attach(void *,unsigned int,uint64_t,uint32_t,uint32_t,uint32_t,bool);
extern void macdrv_reentry_record_after(void *,unsigned int,uint64_t,uint32_t,uint32_t,uint32_t,uint32_t);
@interface WineContentView (CiderReentryFixture)
- (void)ciderReentryCounts:(uint32_t *)counts context:(unsigned int)cid ticket:(uint64_t)ticket;
@end
@implementation WineContentView (CiderReentryFixture)
- (void)ciderReentryCounts:(uint32_t *)counts context:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_model *model;
    struct remote_host_selection *selection;
    CALayer *parent=self.layer;
    counts[0]=counts[1]=counts[2]=0;
    for(CALayer *layer in [parent sublayers])
        if([layer isKindOfClass:[WineRemoteLayerHost class]]&&[(WineRemoteLayerHost *)layer generation]==ticket)++counts[0];
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==ticket)++counts[1];
    for(selection=_remoteHostSelections;selection;selection=selection->next)
        if(selection->context_id==cid&&selection->current&&selection->current->generation==ticket)++counts[2];
}
@end
static void reentry_fixture_after_attach(id content,struct remote_host_model *model)
{
    uint32_t before[3],after[3]={UINT32_MAX,UINT32_MAX,UINT32_MAX};
    NSWindow *candidate=[content window];
    WineWindow *window;
    if(![candidate isKindOfClass:[WineWindow class]])return;
    window=(WineWindow *)candidate;
    unsigned int cid=model->context_id;
    uint64_t ticket=model->generation;
    bool completed=false;
    @try
    {
        [(WineContentView *)content ciderReentryCounts:before context:cid ticket:ticket];
        completed=macdrv_reentry_cancel_after_attach(window,cid,ticket,before[0],before[1],before[2],[NSThread isMainThread]);
        if(completed)
        {
            [(WineContentView *)content ciderReentryCounts:after context:cid ticket:ticket];
            macdrv_reentry_record_after(window,cid,ticket,after[0],after[1],after[2],0);
        }
    }
    @catch(NSException *exception)
    {
        if(completed)macdrv_reentry_record_after(window,cid,ticket,UINT32_MAX,UINT32_MAX,UINT32_MAX,1);
        ERR(@"reentry observer exception %@\n",[exception name]);
    }
}
