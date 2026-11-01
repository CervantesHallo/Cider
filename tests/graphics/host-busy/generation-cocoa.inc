/* One actual context, two caller-assigned unmanaged Cocoa generation labels.
 * Reenter the ordinary model method directly on Cocoa, not OnMainThread again.
 * No Native ticket/job/commit result is fabricated. */
#include "generation.h"
static struct
{
    BOOL used,armed,active;
    WineWindow *window; /* synchronous/lifecycle borrow from original caller */
    unsigned int cid;
    uint64_t old_generation,new_generation;
    struct generation_frame witness;
} generation_cocoa;
@interface WineContentView (CiderGenerationFixture)
- (void)ciderGenerationCounts:(uint32_t *)counts context:(unsigned int)cid generation:(uint64_t)generation;
- (uint32_t)ciderGenerationManaged:(unsigned int)cid generation:(uint64_t)generation;
- (struct remote_host_model *)ciderGenerationFind:(unsigned int)cid generation:(uint64_t)generation;
@end
@implementation WineContentView (CiderGenerationFixture)
- (void)ciderGenerationCounts:(uint32_t *)counts context:(unsigned int)cid generation:(uint64_t)generation
{
    struct remote_host_model *model;struct remote_host_selection *selection;
    counts[0]=counts[1]=counts[2]=0;
    for(CALayer *layer in [self.layer sublayers])
        if([layer isKindOfClass:[WineRemoteLayerHost class]]&&[(WineRemoteLayerHost *)layer generation]==generation)++counts[0];
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==generation)++counts[1];
    for(selection=_remoteHostSelections;selection;selection=selection->next)
        if(selection->context_id==cid&&selection->current&&selection->current->generation==generation)++counts[2];
}
- (uint32_t)ciderGenerationManaged:(unsigned int)cid generation:(uint64_t)generation
{
    struct remote_host_model *model;
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==generation)return model->managed;
    return UINT32_MAX;
}
- (struct remote_host_model *)ciderGenerationFind:(unsigned int)cid generation:(uint64_t)generation
{
    struct remote_host_model *model;
    for(model=_remoteHostModels;model;model=model->next)
        if(model->context_id==cid&&model->generation==generation)return model;
    return NULL;
}
@end
static void generation_nested_insert(id content,struct remote_host_model *model)
{
    uint32_t a[3],b[3];unsigned int cid=model->context_id;
    if(!generation_cocoa.armed||cid!=generation_cocoa.cid||model->generation!=generation_cocoa.old_generation||
       content!=[generation_cocoa.window contentView])return;
    generation_cocoa.armed=NO;
    ++generation_cocoa.witness.hook_seen;generation_cocoa.witness.hook_main=[NSThread isMainThread];
    [(WineContentView *)content ciderGenerationCounts:a context:cid generation:generation_cocoa.old_generation];
    generation_cocoa.witness.before_old_attached=a[0];generation_cocoa.witness.before_old_ledger=a[1];
    generation_cocoa.witness.before_old_indexed=a[2];
    generation_cocoa.witness.inner_called=1; /* before possible import effects */
    generation_cocoa.witness.inner_result=[(WineContentView *)content addCALayerHostViewWithContextId:cid
        generation:generation_cocoa.new_generation frame:CGRectMake(12,18,160,80) clip:CGRectMake(20,22,100,50)
        z:7 validZ:YES commit:NULL context:NULL];
    /* Never dereference the old model pointer after nested Cocoa work. */
    [(WineContentView *)content ciderGenerationCounts:a context:cid generation:generation_cocoa.old_generation];
    [(WineContentView *)content ciderGenerationCounts:b context:cid generation:generation_cocoa.new_generation];
    generation_cocoa.witness.after_old_attached=a[0];generation_cocoa.witness.after_old_ledger=a[1];
    generation_cocoa.witness.after_old_indexed=a[2];generation_cocoa.witness.after_new_attached=b[0];
    generation_cocoa.witness.after_new_ledger=b[1];generation_cocoa.witness.after_new_indexed=b[2];
}
static void generation_busy_hook(id object)
{
    if(!generation_cocoa.active||!generation_cocoa.armed)return;
    WineContentView *content=(WineContentView *)[generation_cocoa.window contentView];
    struct remote_host_model *model=[content ciderGenerationFind:generation_cocoa.cid generation:generation_cocoa.old_generation];
    if(!model||model->host!=object)return;
    generation_cocoa.witness.hook_users=model->users;
    generation_cocoa.witness.retire_before=model->retire_requested;
    generation_nested_insert(content,model);
    model=[content ciderGenerationFind:generation_cocoa.cid generation:generation_cocoa.old_generation];
    generation_cocoa.witness.retire_after=model?model->retire_requested:0;
}
bool macdrv_generation_busy_update(void *object,unsigned int cid,uint64_t generation)
{
    WineWindow *window=object;__block bool invoked=false;
    OnMainThread(^{
        @try
        {
            NSView *candidate=[window contentView];
            if(![candidate isKindOfClass:[WineContentView class]])return;
            WineContentView *content=(WineContentView *)candidate;
            struct remote_host_model *model=[content ciderGenerationFind:cid generation:generation];
            if(!model||model->users||model->installing||model->busy)return;
            generation_cocoa.active=YES;
            [content updateCALayerHost:cid generation:generation frame:CGRectMake(12,18,160,80)
                clip:CGRectMake(20,22,100,50) z:7 validZ:YES];
            invoked=true;
        }
        @catch(NSException *exception)
        {generation_cocoa.witness.hook_error=1;ERR(@"busy generation update exception %@\n",[exception name]);}
        @finally{generation_cocoa.active=NO;}
    });return invoked;
}
bool macdrv_generation_arm(void *object,unsigned int cid,uint64_t older,uint64_t newer)
{
    WineWindow *window=object;__block bool armed=false;
    OnMainThread(^{
        if(!generation_cocoa.used&&cid&&older&&newer>older&&[[window contentView] isKindOfClass:[WineContentView class]])
        {
            generation_cocoa.used=generation_cocoa.armed=YES;generation_cocoa.window=window;
            generation_cocoa.cid=cid;generation_cocoa.old_generation=older;generation_cocoa.new_generation=newer;armed=true;
        }
    });return armed;
}
void macdrv_generation_disarm(void *object)
{
    OnMainThread(^{
        if(generation_cocoa.window==object)
        {generation_cocoa.armed=NO;generation_cocoa.window=nil;}
    });
}
void macdrv_generation_observe(void *object,unsigned int cid,uint64_t older,uint64_t newer,struct generation_frame *f)
{
    WineWindow *window=object;
    f->cocoa_valid=0;f->final_old_attached=f->final_old_ledger=f->final_old_indexed=UINT32_MAX;
    f->final_new_attached=f->final_new_ledger=f->final_new_indexed=UINT32_MAX;
    OnMainThread(^{
        @try
        {
            NSView *candidate=[window contentView];uint32_t a[3],b[3];
            if(![candidate isKindOfClass:[WineContentView class]])return;
            WineContentView *content=(WineContentView *)candidate;
            [content ciderGenerationCounts:a context:cid generation:older];
            [content ciderGenerationCounts:b context:cid generation:newer];
            f->hook_users=generation_cocoa.witness.hook_users;f->retire_before=generation_cocoa.witness.retire_before;
            f->retire_after=generation_cocoa.witness.retire_after;
            f->inner_called=generation_cocoa.witness.inner_called;f->inner_result=generation_cocoa.witness.inner_result;
            f->hook_seen=generation_cocoa.witness.hook_seen;f->hook_main=generation_cocoa.witness.hook_main;
            f->hook_error=generation_cocoa.witness.hook_error;
            f->before_old_attached=generation_cocoa.witness.before_old_attached;f->before_old_ledger=generation_cocoa.witness.before_old_ledger;
            f->before_old_indexed=generation_cocoa.witness.before_old_indexed;f->after_old_attached=generation_cocoa.witness.after_old_attached;
            f->after_old_ledger=generation_cocoa.witness.after_old_ledger;f->after_old_indexed=generation_cocoa.witness.after_old_indexed;
            f->after_new_attached=generation_cocoa.witness.after_new_attached;f->after_new_ledger=generation_cocoa.witness.after_new_ledger;
            f->after_new_indexed=generation_cocoa.witness.after_new_indexed;
            f->final_old_attached=a[0];f->final_old_ledger=a[1];f->final_old_indexed=a[2];
            f->final_new_attached=b[0];f->final_new_ledger=b[1];f->final_new_indexed=b[2];
            f->old_managed=[content ciderGenerationManaged:cid generation:older];f->new_managed=[content ciderGenerationManaged:cid generation:newer];
            f->cocoa_valid=1;
        }
        @catch(NSException *exception)
        {f->hook_error=1;f->cocoa_valid=0;ERR(@"generation fixture observer exception %@\n",[exception name]);}
    });
}
