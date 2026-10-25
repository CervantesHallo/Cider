/* DIAGNOSTIC ONLY: inspect actual Cocoa model, not pixels/GPU retirement. */
@interface WineContentView (CiderRetirementFixture)
- (BOOL)ciderFixtureContext:(unsigned int)contextId generation:(uint64_t)generation;
@end
@implementation WineContentView (CiderRetirementFixture)
- (BOOL)ciderFixtureContext:(unsigned int)contextId generation:(uint64_t)generation
{
    WineRemoteLayerHost *host = [_caLayerHosts objectForKey:@(contextId)];
    return host && host.generation == generation && host.superlayer == self.layer;
}
@end
bool macdrv_fixture_retirement_model(void *object, unsigned int context_id, uint64_t generation)
{
    WineWindow *window = object;
    __block bool found = false;
    OnMainThread(^{
        NSView *content = [window contentView];
        if ([content isKindOfClass:[WineContentView class]])
            found = [(WineContentView *)content ciderFixtureContext:context_id generation:generation];
    });
    return found;
}
