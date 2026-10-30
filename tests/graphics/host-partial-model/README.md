# Real partial-model exception fixture v1

The exact source has been independently reviewed, built for both PE widths,
and run through Cider GUI/XPC in an isolated bottle. Actual results and scope:
docs/research/110-r-v1-partial-host-model-20261031.md and its evidence.
Ordinary candidate: patch0026 after0025, Native source host-job-fence-v8.
Diagnostic integration-v2/build-v4; watch2, both Unix arrays8, frame256/version1.
Apply diagnostic.patch, use this directory as an explicit include path and the
provided complete private spec with -Fwinemac.drv. Then apply before.patch only
to the Cocoa source for the control. Native coordinator, PE and client stay the
same. The original author-only delivery and failed build records remain private;
their descriptions below document the source contract, not extra runtime proof.

The private protocol is fixed **256 bytes/version1**, with asserted offsets.
Run the same PE client/source in a fresh isolated process per candidate and PE
width, without arguments. Intended fixed result: **30 checks/0 failures**.
The qualified old-policy negative stops at **19 checks/1 failure, CHECK19 only**,
then makes one honest cleanup attempt and returns nonzero. It reports actual
retained context/window/producer owners and `internal_cleanup=false`; the parent
must confirm actual process/kernel cleanup externally. No timeout alone is an
accepted negative. This fixture does not exercise a foreign renderer, USER96
marshalling, per-host status protocol, pixels, GPU retirement or game behavior.

## Real resources and deterministic ownership

Client creates a real same-process Win32 root and bordered child, verifies their
actual PID/TID/ancestry and process birth, and keeps them alive through primary.
SETUP retains that root's actual WineWindow, calls ordinary producer_acquire with
generation2^33, retains one independent producer observation reference, and
creates an actual exported Metal/CAContext using the child's real bounds. The
actual context ID is obtained from that exported object. Parent seeds the private
Native identity allocator to2^33; root/lease/ticket must have real high bits.
No dummy context ID, process watch, pending count, NT result or success stub is
supplied by the fixture.

The Native reference is a lifetime pin, **not** a request_inflight guard. It lets
the negative inspect the actual finalized producer after its watch registry
entry disappears. SETUP directly calls actual host_install with the real lease.
Frame `install_result` is its actual uint64 return; expected0 means this import
failed. SETUP status0 means the harness call returned, not INSTALL success.
For an earlier setup failure that never invoked host_install, cleanup may release
its never-imported context once Native owners drain; the private install_started
latch distinguishes that case. Once import was attempted, actual model absence
is required. A failed/missing probe never supplies absence for an attempted import.

Two Cocoa hooks raise real NSException objects inside the ordinary catches:

1. Immediately after actual parent addSublayer, before geometry and current-index
   publication, raise CiderPartialModelAfterAttach once for the armed context.
2. In the failed-install rollback's removeRemoteHostModel @try, after obtaining
   its host and before actual removeFromSuperlayer, raise CiderPartialModelRollback
   once for the same actual generation. The ordinary catch returns failure and
   keeps the ledger/object. Neither hook fabricates the method return.

The candidate then retires its actual failed Native host. Its normal model job
fence defers removal while model_jobs is nonzero. **After** that deferral test,
when the last real job transfers its retirement owner to submit_retirement,
the diagnostic Native hook takes exactly that owner into `held`. It adds no
retirement/ref count and performs no Cocoa/NT work under remote_lock. Expected
SETUP observation is actual install_result0, attached1, index0, ledger1,
pending1, retirement_total1, held1 and model_jobs0. Both actual exception counters
must be1. The independent producer reference and exported context stay owned.

REVOKE calls ordinary producer_revoke/real STOP while the root stays live. Pure
snapshots wait for actual completion_requested1, process/saved handles gone,
cleanup_inflight0, pending1, watched1 and finalized0. Actual PE watch/timer owners
are sampled, not invented. SUBMIT_HELD transfers the same owner back to ordinary
submit_retirement outside remote_lock. No user callback holds a caller frame.

## Actual checked-removal witness and primary

The third Cocoa hook is a **pure observer**, inserted in the checked retirement
wrapper immediately before its existing completion(context, removed). It receives
the actual ordinary `removed` Boolean, performs no removal or Native completion,
and catches observer exceptions. On Cocoa's main thread, before that callback,
it freezes the first matching ticket's actual:

- successful/failed checked remove result and invocation serial;
- parent sublayer count by real WineRemoteLayerHost generation, independent of
  both the ledger and selection index;
- exact ledger count in _remoteHostModels;
- actual C selection->current membership for that context/ticket;
- both actual exception counts, ticket and probe validity/error.

Later probes cannot overwrite this witness. Live Cocoa and Native snapshots are
separately synchronized and are not claimed to be one transaction. The primary
uses only the frozen same-main-thread removal observation. Unavailable/throwing
probes have valid0 and UINT32_MAX counts; they cannot pass as model absence.

CHECK18 requires real prequalified partial state from CHECK11/14, a live root and
child, witness serial1, exact immutable ticket, both exception counts1, actual
removed1, index0 and a valid observation. Witness attached/ledger must be either
0/0 (fixed) or1/1 (target negative). **CHECK19 rejects removed1/attached1.** The
negative's real object therefore survives a real claimed-success removal; an
arbitrary error, timeout, missing callback or unqualified probe is not accepted.

The fixture deliberately omits optional RO producer-cell mapping: adding reader
NT ownership would widen this task. Actual Native watched/finalized/completion
fields are observed through the explicit producer reference. A frozen
checked-removal violation is sufficient; it is not claimed to be PE receipt of a
terminal ACK or direct RO SAFE observation. Parent may bind later Native SAFE
logs separately without changing this primary.

## Parent hook placements and private glue

Include native.inc at end of the private coordinator, retaining ordinary code.
Before submit_retirement, declare:

```c
static BOOL partial_model_fixture_hold_retirement(struct remote_host *);
```

In submit_retirement, retain its lock/deferral code exactly, then:

```c
if (deferred) return;
if (partial_model_fixture_hold_retirement(host)) return; /* transferred owner */
macdrv_window_release_remote_host_deferred_checked(/* ordinary arguments */);
```

Include cocoa.inc after WineContentView's implementation (or at translation-unit
end). After remote_host_model's definition, declare the following before their
uses; types already exist there:

```objc
static void partial_model_fixture_after_attach(id, struct remote_host_model *);
static void partial_model_fixture_before_detach(id, struct remote_host_model *);
static void partial_model_fixture_retirement_observed(WineWindow *, unsigned int, uint64_t, bool);
```

Place the two faults exactly inside ordinary @try regions:

```objc
[parent addSublayer:host];
partial_model_fixture_after_attach(self, model);
/* ordinary geometry and later selection->current publication */
```

```objc
host = model->host;
if (host)
{
    partial_model_fixture_before_detach(self, model);
    [host removeFromSuperlayer];
    /* rest of exact-model removal unchanged */
}
```

The second call must remain inside removeRemoteHostModel's own @try, so the
ordinary catch/finally clears busy and returns NO. Do not inject outside it or
skip the candidate's model-job settlement.

In macdrv_window_release_remote_host_deferred_checked's @finally:

```objc
partial_model_fixture_retirement_observed(window, context_id, generation, removed);
if (completion) completion(context, removed); /* ordinary callback unchanged */
```

The supplied category's ciderPartialIsCurrentContext reads the actual v3
_remoteHostSelections list and its current model using C fields, on the main
thread. It does not equate a highwater node with a current model. Ledger and
parent sublayer probes remain independent. If parent changes selection names,
adapt **only this accessor** to equivalent actual state; never return a fixed0,
consult Native installed flags, or infer absence from failed host_install.
There is no _caLayerHosts dependency in this deliverable.

Include pe.inc at end of the private diagnostic remote_watch_pe.c and append the
normal Unix entry macdrv_partial_model_fixture at slot7 in both arrays, count8.
Parent adds private enum/prototypes and the supplied PE export/spec entry,
replacing any previous slot7 fixture. Preserve slot6 with its own existing probe
declarations. Keep watch protocol version2, existing DLL pinning and the actual
fixture_live_watches/fixture_live_timers counters. The latter counts timer creation
before CreateTimer and retains in-flight deletion until successful/IO_PENDING
cancellation. Do not substitute active_timer presence for the live owner count.
All driver callbacks use pinned code; the private wrapper's serialization lock
is never taken by ordinary ARM/STOP/timer callbacks. No source edits outside the
parent's private integration are performed by this deliverable.

## Exact negative policy

Fixed and before use the same Native helper, real faults, model ledger, selection
highwater, observers, PE and client. Change **only** the versioned Cocoa
removeCALayerHostView:contextId generation:generation policy to treat absence
from the **actual current selection** as success. The minimal v3/v4 negative
is to put this ordinary-state check before its existing exact-ledger loop:

```objc
struct remote_host_selection *selection;
for (selection = _remoteHostSelections; selection; selection = selection->next)
    if (selection->context_id == contextId) break;
if (!selection || !selection->current || selection->current->generation != generation)
    return YES; /* old index-only absence policy, intentionally defective */
/* unchanged exact-ledger loop for a currently selected model */
```

This does not invoke a diagnostic predicate or alter the post-removal observer.
For the target partial import, current is genuinelyNULL although the actual
ledger/object remains. It returns the real defective YES without detaching it.
Do not change removeRemoteHostModel: the first rollback must still raise/catch
the same actual exception in fixed and before. Do not use the old unversioned
remove helper recursively, clear the ledger, or destroy the root before primary.

## Cleanup, errors and bounds

The fixed client waits for actual model/index/ledger0 and Native producer/watch/
setup/cleanup/retirement plus PE watch/timer owners0 while windows remain alive.
CLEANUP independently reprobes and requires actual finalized Native quiescence
and actual model absence before destroying the exported context. Native records
release_attempted before the call, and marks context_released only after its
real void destructor returns. An interrupted/unknown destruction is not replayed.
The PE wrapper may then report IO_PENDING if actual PE owners remain; client
records the applied context release **before** timing/status rejection. No status
or boolean is upgraded to claim that an attached model went away.

Only after actual context release does fixed destroy child/root. FINISH requires
the exact root epoch absent and no Native resource guards, disarms the Cocoa
fixture on the main thread, then releases the independent producer/window pins.
Those final unavailable producer/model observations are explicitly invalid, not
fake zeros; summary retains the earlier actual settled and primary tuples.

CLEANUP always transfers any held retirement owner back first and revokes a
still-active owned producer once. It does not manually drive retry/finalize from
SNAPSHOT. Failed cleanup is explicit. In the target negative, actual attached1/
ledger1 prevents context release even if ordinary Native finalized became true.
Client keeps the context, producer/window/module references and live windows
through process exit, logs internal_cleanup=false and external_cleanup_required,
and returns nonzero. Root/process teardown is the parent's separate evidence;
it cannot retroactively turn internal cleanup into success.

Every control/poll uses pre/post four-second QPC checks, including after message
output/flush and pacing. Effects and identities are recorded before rejecting a
late return. Sleep5 only paces real pure observations; no sleep/spin supplies the
fault ordering or primary. A synchronous Cocoa/NT call or lock acquisition cannot
be canceled by QPC: parent needs an outer watchdog and confirmed actual process
termination/restoration. No late readiness, lack of callback or timeout may be
accepted as the targeted negative.

Effective build binding must include candidate versions, both includes, Native
hold/three Cocoa hook placements, actual C selection accessor, only the stated
negative delta, high identity seed, wrapper/counters, slot arrays/header/spec,
client/frame and resulting Native/both PE modules. Parent must source-review and
qualify the actual candidate; this fixture does not resolve the separate update
UAF/reentrant-selection issues that motivated v3, nor claim compositor/ABA proof.

## Current source binding

Actual admission uses ordinary host-job-fence-v8, diagnostic-source-v2 and
before-source-v2. The historical author delivery targeted v3/v4; it is retained
privately and is not used as a substitute for these current source hashes.
The public spec includes the retained private probe export as well as this
fixture. Current exact file/patch and compiled-input bindings are in research110
and its evidence. This fixture does not resolve the separately unexecuted update
reentry/selection, legacy handoff or general Retina acceptance cases.

```text
fixture.h 54eca6f411e9905a2d36310a8ae90f29f6d528058940257ed7c1145fd596c3d7
native.inc 6530421cd148a7d9699861d821e1b8a133f8e6924cb870ac5f8c518e10003d4a
cocoa.inc 8a586adbf22849c9049f0ebcd0601a6df1e2616128a4f806b9a34ca280faa0f5
pe.inc be5e9dd560e6a60a7db33f265518aaeb312ec5d06dcd7d604bead5f5fda1c6f0
client.c 9a77dc06833b25e6b587b1a233b490da46970dd3d14efca24c1f537b65427ad5
probe.h 6c251cdfc9e4ba34a73231ca34c9497e40e0575248322c7f3fb54eacbf281b02
winemac-fixture.spec 699e363f1888d90b7add9a23565a87e75d8b487ba7271d03efc08c9858e83997
```
