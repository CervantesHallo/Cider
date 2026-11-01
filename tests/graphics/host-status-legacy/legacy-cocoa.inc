/* Pure actual model observation. Each test Root creates exactly one context;
 * attached generation-zero layers are counted independently of its ledger. */
#include "legacy.h"
@interface WineContentView (CiderLegacyHandoffFixture)
- (void)ciderLegacyHandoffProbe:(struct legacy_frame *)f context:(unsigned int)cid ticket:(uint64_t)ticket;
@end
@implementation WineContentView (CiderLegacyHandoffFixture)
- (void)ciderLegacyHandoffProbe:(struct legacy_frame *)f context:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_model *model;
    struct remote_host_selection *selection;
    CALayer *parent=self.layer;
    if(!parent||!cid)return;
    f->legacy_attached=f->legacy_ledger=f->legacy_indexed=0;
    f->managed_attached=f->managed_ledger=f->managed_indexed=f->managed_flag=0;
    for(CALayer *layer in [parent sublayers])
    {
        if(![layer isKindOfClass:[WineRemoteLayerHost class]])continue;
        uint64_t generation=[(WineRemoteLayerHost *)layer generation];
        if(!generation)++f->legacy_attached;
        else if(ticket&&generation==ticket)++f->managed_attached;
    }
    for(model=_remoteHostModels;model;model=model->next)
    {
        if(model->context_id!=cid)continue;
        if(!model->generation)++f->legacy_ledger;
        else if(ticket&&model->generation==ticket){++f->managed_ledger;f->managed_flag=model->managed;}
    }
    for(selection=_remoteHostSelections;selection;selection=selection->next)
    {
        if(selection->context_id!=cid||!selection->current)continue;
        if(!selection->current->generation)++f->legacy_indexed;
        else if(ticket&&selection->current->generation==ticket)++f->managed_indexed;
    }
    f->cocoa_valid=1;
}
@end
void macdrv_legacy_handoff_probe(void *object,unsigned int cid,uint64_t ticket,struct legacy_frame *f)
{
    WineWindow *window=object;
    f->cocoa_valid=0;
    f->legacy_attached=f->legacy_ledger=f->legacy_indexed=UINT32_MAX;
    f->managed_attached=f->managed_ledger=f->managed_indexed=f->managed_flag=UINT32_MAX;
    OnMainThread(^{
        @try
        {
            NSView *content=[window contentView];
            if([content isKindOfClass:[WineContentView class]])
                [(WineContentView *)content ciderLegacyHandoffProbe:f context:cid ticket:ticket];
        }
        @catch(NSException *exception)
        {
            f->cocoa_valid=0;
            ERR(@"legacy handoff observer exception %@\n",[exception name]);
        }
    });
}
