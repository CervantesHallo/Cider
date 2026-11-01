/* Main-thread-only local view operations. No NT/USER callback or manufactured
 * model/reference counter. The caller retains original window/context owners. */
#include "use.h"
static struct
{
    BOOL used,active,current_mode,initial_mode;
    WineWindow *window; /* borrowed only while synchronous operation is active */
    unsigned int cid;
    uint64_t ticket;
    struct use_frame *frame; /* stack borrow valid only inside OnMainThread */
} model_use;
static uint64_t use_double(double value)
{ uint64_t result;memcpy(&result,&value,sizeof(result));return result; }
static void use_rect(uint64_t *out,CGRect value)
{
    out[0]=use_double(value.origin.x);out[1]=use_double(value.origin.y);
    out[2]=use_double(value.size.width);out[3]=use_double(value.size.height);
}
@interface WineRemoteLayerHost (CiderUseFixture)
- (void)ciderUseGeometry:(struct use_frame *)f;
@end
@implementation WineRemoteLayerHost (CiderUseFixture)
- (void)ciderUseGeometry:(struct use_frame *)f
{
    if(!source_layer)return;
    f->scale=use_double(self.contentsScale);f->source_scale=use_double(source_layer.contentsScale);
    use_rect(f->container,self.frame);use_rect(f->source,source_layer.frame);f->geometry_valid=1;
}
@end
@interface WineContentView (CiderUseFixture)
- (struct remote_host_model *)ciderUseModel:(unsigned int)cid ticket:(uint64_t)ticket;
- (void)ciderUseCounts:(uint32_t *)counts context:(unsigned int)cid ticket:(uint64_t)ticket;
@end
@implementation WineContentView (CiderUseFixture)
- (struct remote_host_model *)ciderUseModel:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_model *model;
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==ticket)return model;
    return NULL;
}
- (void)ciderUseCounts:(uint32_t *)counts context:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_model *model;
    struct remote_host_selection *selection;
    counts[0]=counts[1]=counts[2]=0;
    for(CALayer *layer in [self.layer sublayers])
        if([layer isKindOfClass:[WineRemoteLayerHost class]]&&[(WineRemoteLayerHost *)layer generation]==ticket)++counts[0];
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==ticket)++counts[1];
    for(selection=_remoteHostSelections;selection;selection=selection->next)
        if(selection->context_id==cid&&selection->current&&selection->current->generation==ticket)++counts[2];
}
@end
static void model_use_fixture_hook(id object)
{
    WineContentView *content;
    struct remote_host_model *model;
    struct use_frame *f;
    uint32_t counts[3];
    if(!model_use.active||!model_use.frame)return;
    content=(WineContentView *)[model_use.window contentView];
    model=[content ciderUseModel:model_use.cid ticket:model_use.ticket];
    if(!model||model->host!=object)return;
    f=model_use.frame;
    if(f->hook_seen++)
    {if(model->users!=1)f->hook_error=2;return;}
    f->hook_users=model->users;f->hook_installing=model->installing;
    f->hook_busy=model->busy;f->hook_retire_requested=model->retire_requested;
    f->hook_main=[NSThread isMainThread];
    [content ciderUseCounts:counts context:model_use.cid ticket:model_use.ticket];
    f->hook_attached=counts[0];f->hook_ledger=counts[1];f->hook_indexed=counts[2];
    if(f->operation==USE_REMOVE)
    {
        /* Actual ordinary untyped removal requests retirement. Never retain the
         * model/host to hide a missing use pin; relookup after the call. */
        [content removeCALayerHostView:model_use.cid];
        [content ciderUseCounts:counts context:model_use.cid ticket:model_use.ticket];
        f->after_remove_attached=counts[0];f->after_remove_ledger=counts[1];f->after_remove_indexed=counts[2];
        model=[content ciderUseModel:model_use.cid ticket:model_use.ticket];
        f->retire_after_request=model?model->retire_requested:0;
    }
}
void macdrv_model_use_cocoa(void *object,unsigned int cid,uint64_t ticket,struct use_frame *f)
{
    WineWindow *window=object;
    OnMainThread(^{
        WineContentView *content;
        struct remote_host_model *model;
        uint32_t counts[3];
        @try
        {
            NSView *candidate=[window contentView];
            if(![candidate isKindOfClass:[WineContentView class]]||!cid||!ticket)return;
            content=(WineContentView *)candidate;
            model=[content ciderUseModel:cid ticket:ticket];
            if(!model||!model->host||model->installing||model->busy||model->users)return;
            if(model_use.used&&(model_use.window!=window||model_use.cid!=cid||model_use.ticket!=ticket))return;
            if(!model_use.used)
            {
                model_use.used=YES;model_use.initial_mode=model_use.current_mode=retina_on;
                model_use.window=window;model_use.cid=cid;model_use.ticket=ticket;
            }
            f->initial_mode=model_use.initial_mode;
            /* Seed actual geometry through the ordinary update method before
             * each action; the observer is inactive during this preparation. */
            if(f->operation==USE_FLIP||f->operation==USE_RESTORE)
                [content updateCALayerHost:cid generation:ticket frame:CGRectMake(12,18,160,80)
                    clip:CGRectMake(20,22,100,50) z:7 validZ:YES];
            model_use.frame=f;model_use.active=YES;f->op_called=1;
            if(f->operation==USE_UPDATE||f->operation==USE_REMOVE)
                [content updateCALayerHost:cid generation:ticket frame:CGRectMake(12,18,160,80)
                    clip:CGRectMake(20,22,100,50) z:7 validZ:YES];
            else
            {
                BOOL next=f->operation==USE_FLIP?!model_use.current_mode:model_use.initial_mode;
                if(next==model_use.current_mode){f->status=USE_BAD_STATE;return;}
                model_use.current_mode=next; /* latch possible effects before setter */
                [content setRetinaMode:next];
            }
            model_use.active=NO;model_use.frame=NULL;
            f->mode=model_use.current_mode;
            [content ciderUseCounts:counts context:cid ticket:ticket];
            f->attached_after=counts[0];f->ledger_after=counts[1];f->indexed_after=counts[2];
            model=[content ciderUseModel:cid ticket:ticket];
            f->users_after=model?model->users:UINT32_MAX;
            if(model&&model->host)[model->host ciderUseGeometry:f];
            f->cocoa_valid=1;f->status=USE_OK;
        }
        @catch(NSException *exception)
        {f->status=USE_EXCEPTION;f->hook_error=1;ERR(@"model-use fixture exception %@\n",[exception name]);}
        @finally {model_use.active=NO;model_use.frame=NULL;}
    });
}
