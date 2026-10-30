/* Include after WineContentView implementation in cocoa_window.m. All state
 * here is main-thread-only. The Native fixture owns/pins target_window until
 * disarm; no caller frame or remote_host_model pointer survives a hook call. */
#include "fixture.h"
static struct
{
    bool used, armed;
    WineWindow *target_window; /* borrowed from explicit Native fixture owner */
    unsigned int context_id;
    uint64_t ticket, serial, witness_ticket, witness_serial;
    uint32_t attach_faults, rollback_faults;
    bool witness_seen, witness_removed, witness_valid;
    uint32_t witness_attached, witness_indexed, witness_ledger, witness_error;
    uint32_t witness_faults, witness_rollback_faults;
} partial_model_cocoa;

@interface WineContentView (CiderPartialModelFixture)
- (BOOL)ciderPartialIsCurrentContext:(unsigned int)cid ticket:(uint64_t)ticket;
- (void)ciderPartialProbe:(struct partial_model_fixture *)frame context:(unsigned int)cid ticket:(uint64_t)ticket;
@end
@implementation WineContentView (CiderPartialModelFixture)
- (BOOL)ciderPartialIsCurrentContext:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_selection *selection;
    for (selection = _remoteHostSelections; selection; selection = selection->next)
        if (selection->context_id == cid) break;
    return selection && selection->current && selection->current->generation == ticket;
}
- (void)ciderPartialProbe:(struct partial_model_fixture *)f context:(unsigned int)cid ticket:(uint64_t)ticket
{
    struct remote_host_model *model;
    CALayer *parent = self.layer;
    uint32_t attached = 0, ledger = 0;
    if (!parent || !cid || !ticket) return;
    /* Independent of the dictionary AND ledger: an orphan is still visible. */
    for (CALayer *layer in [parent sublayers])
        if ([layer isKindOfClass:[WineRemoteLayerHost class]] &&
            [(WineRemoteLayerHost *)layer generation] == ticket && attached != UINT32_MAX) ++attached;
    for (model = _remoteHostModels; model; model = model->next)
        if (model->context_id == cid && model->generation == ticket && ledger != UINT32_MAX) ++ledger;
    f->attached = attached;
    f->indexed = [self ciderPartialIsCurrentContext:cid ticket:ticket];
    f->ledger = ledger;
    f->cocoa_error = 0;
    f->cocoa_valid = 1;
}
@end

static void partial_cocoa_probe(WineWindow *window, unsigned int cid, uint64_t ticket,
                                struct partial_model_fixture *f)
{
    f->cocoa_valid = 0;
    f->attached = f->indexed = f->ledger = UINT32_MAX;
    f->cocoa_error = 1; /* private observer unavailable, not a fabricated NT result */
    @try
    {
        NSView *content = [window contentView];
        if (window == partial_model_cocoa.target_window && cid == partial_model_cocoa.context_id &&
            ticket && [content isKindOfClass:[WineContentView class]])
            [(WineContentView *)content ciderPartialProbe:f context:cid ticket:ticket];
    }
    @catch (NSException *exception)
    {
        f->cocoa_valid = 0; f->cocoa_error = 2;
        ERR(@"partial-fixture observer exception %@\n", [exception name]);
    }
}

/* Hook A: immediately AFTER the REAL [parent addSublayer:host], before any
 * geometry or dictionary publication, INSIDE the ordinary install @try. */
static void partial_model_fixture_after_attach(id content, struct remote_host_model *model)
{
    if (!partial_model_cocoa.armed || partial_model_cocoa.attach_faults ||
        content != [partial_model_cocoa.target_window contentView] ||
        model->context_id != partial_model_cocoa.context_id) return;
    partial_model_cocoa.ticket = model->generation;
    ++partial_model_cocoa.attach_faults;
    @throw [NSException exceptionWithName:@"CiderPartialModelAfterAttach"
        reason:@"diagnostic actual partial layer after addSublayer" userInfo:nil];
}

/* Hook B: inside removeRemoteHostModel's @try, after host=model->host and
 * BEFORE actual removeFromSuperlayer. The first call is the failed-install
 * @finally rollback. The ordinary method catches this real exception. */
static void partial_model_fixture_before_detach(id content, struct remote_host_model *model)
{
    if (!partial_model_cocoa.armed || partial_model_cocoa.attach_faults != 1 ||
        partial_model_cocoa.rollback_faults || model->generation != partial_model_cocoa.ticket ||
        model->context_id != partial_model_cocoa.context_id ||
        content != [partial_model_cocoa.target_window contentView]) return;
    ++partial_model_cocoa.rollback_faults;
    @throw [NSException exceptionWithName:@"CiderPartialModelRollback"
        reason:@"diagnostic rollback throws before actual model detach" userInfo:nil];
}

/* Hook C: in checked retirement's @finally, immediately BEFORE its existing
 * completion(context, removed). This observer never changes removed, calls a
 * remove method, or calls Native completion. No exception escapes the probe. */
static void partial_model_fixture_retirement_observed(WineWindow *window, unsigned int cid,
                                                       uint64_t ticket, bool removed)
{
    struct partial_model_fixture sample = {0};
    if (!partial_model_cocoa.armed || window != partial_model_cocoa.target_window ||
        cid != partial_model_cocoa.context_id || ticket != partial_model_cocoa.ticket) return;
    if (partial_model_cocoa.serial != UINT64_MAX) ++partial_model_cocoa.serial;
    if (partial_model_cocoa.witness_seen) return;
    partial_cocoa_probe(window, cid, ticket, &sample);
    partial_model_cocoa.witness_ticket = ticket;
    partial_model_cocoa.witness_serial = partial_model_cocoa.serial;
    partial_model_cocoa.witness_removed = removed; /* exact ordinary result */
    partial_model_cocoa.witness_valid = sample.cocoa_valid != 0;
    partial_model_cocoa.witness_attached = sample.attached;
    partial_model_cocoa.witness_indexed = sample.indexed;
    partial_model_cocoa.witness_ledger = sample.ledger;
    partial_model_cocoa.witness_error = sample.cocoa_error;
    partial_model_cocoa.witness_faults = partial_model_cocoa.attach_faults;
    partial_model_cocoa.witness_rollback_faults = partial_model_cocoa.rollback_faults;
    partial_model_cocoa.witness_seen = true;
}

bool macdrv_partial_model_fixture_arm(void *object, unsigned int cid)
{
    WineWindow *window = object;
    __block bool armed = false;
    OnMainThread(^{
        if (!partial_model_cocoa.used && window && cid &&
            [[window contentView] isKindOfClass:[WineContentView class]])
        {
            partial_model_cocoa.used = partial_model_cocoa.armed = true;
            partial_model_cocoa.target_window = window;
            partial_model_cocoa.context_id = cid;
            armed = true;
        }
    });
    return armed;
}
void macdrv_partial_model_fixture_observe(void *object, unsigned int cid, uint64_t ticket,
                                        struct partial_model_fixture *f)
{
    WineWindow *window = object;
    OnMainThread(^{
        partial_cocoa_probe(window, cid, ticket, f);
        f->attach_faults = partial_model_cocoa.attach_faults;
        f->rollback_faults = partial_model_cocoa.rollback_faults;
        f->witness_seen = partial_model_cocoa.witness_seen;
        f->witness_serial = partial_model_cocoa.witness_serial;
        f->witness_ticket = partial_model_cocoa.witness_ticket;
        f->witness_removed = partial_model_cocoa.witness_seen ? partial_model_cocoa.witness_removed : UINT32_MAX;
        f->witness_valid = partial_model_cocoa.witness_seen && partial_model_cocoa.witness_valid;
        f->witness_attached = partial_model_cocoa.witness_seen ? partial_model_cocoa.witness_attached : UINT32_MAX;
        f->witness_indexed = partial_model_cocoa.witness_seen ? partial_model_cocoa.witness_indexed : UINT32_MAX;
        f->witness_ledger = partial_model_cocoa.witness_seen ? partial_model_cocoa.witness_ledger : UINT32_MAX;
        f->witness_faults = partial_model_cocoa.witness_faults;
        f->witness_rollback_faults = partial_model_cocoa.witness_rollback_faults;
        f->witness_error = partial_model_cocoa.witness_seen ? partial_model_cocoa.witness_error : UINT32_MAX;
    });
}
void macdrv_partial_model_fixture_disarm(void *object)
{
    OnMainThread(^{
        if (partial_model_cocoa.target_window == object)
        { partial_model_cocoa.armed = false; partial_model_cocoa.target_window = nil; }
    });
}
