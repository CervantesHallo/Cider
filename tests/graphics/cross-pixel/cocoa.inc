/* Pure main-thread observation; no fault injection, removal or completion. */
#include "fixture.h"
@interface WineContentView (CiderLiveHostFixture)
- (void)ciderLiveHostProbe:(struct live_host_fixture *)frame context:(unsigned int)cid ticket:(uint64_t)ticket;
@end
@implementation WineContentView (CiderLiveHostFixture)
- (void)ciderLiveHostProbe:(struct live_host_fixture *)f context:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_model *model;
    struct remote_host_selection *selection;
    CALayer *parent=self.layer;
    if(!parent||!cid||!ticket)return;
    f->attached=f->ledger=f->indexed=0;
    for(CALayer *layer in [parent sublayers])
        if([layer isKindOfClass:[WineRemoteLayerHost class]]&&[(WineRemoteLayerHost *)layer generation]==ticket)
            ++f->attached;
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==ticket)++f->ledger;
    for(selection=_remoteHostSelections;selection;selection=selection->next)
        if(selection->context_id==cid&&selection->current&&selection->current->generation==ticket)++f->indexed;
    f->cocoa_valid=1;
}
@end
void macdrv_live_host_probe(void *object,unsigned int cid,uint64_t ticket,struct live_host_fixture *f)
{
    WineWindow *window=object;
    f->cocoa_valid=0;f->attached=f->ledger=f->indexed=UINT32_MAX;
    OnMainThread(^{
        @try
        {
            NSView *content=[window contentView];
            if([content isKindOfClass:[WineContentView class]])
                [(WineContentView *)content ciderLiveHostProbe:f context:cid ticket:ticket];
        }
        @catch(NSException *exception)
        {
            f->cocoa_valid=0;
            ERR(@"live-host observer exception %@\n",[exception name]);
        }
    });
}
