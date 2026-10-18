# HY2 dependency review — 2026-10-13

Read-only source audit; only this report was written. Read AGENTS.md, PROJECT_MEMORY and research/28,33,42. No runtime execution, builds, tests, installs, commits, Windows access or network probes. Existing pinned upstream source was reread; one Microsoft contract was consulted to resolve impersonation outputs.

**Conclusion:** the adopted order remains correct: references → requestor/impersonation → capture/lock/unlock/release → privilege checks. There are new actionable findings. The immediate cut must include server token mapping and checked reference acquisition before importing the two primary-token functions. Deferred-reply ownership also needs a more precise design before shared locks are implemented. These are dependency corrections, not a new roadmap.

## Source identity and reused findings

Local root `S=/Users/cervantes/Library/Caches/Cider/engine-build/cider-cx26.3-r1-x86_64/src`. Below, `N=dlls/ntoskrnl.exe/ntoskrnl.c`, `T=server/token.c`, `D=server/device.c`, `R=server/request.c`, `U=dlls/ntdll/unix/server.c`; all paths are relative to S. All ten server-file SHA256 values in report42 still match.

Additional SHA256 anchors: N `562ef9e96648665b8c2ef1b1637f476b06db1d0eeca0a8734a24d6e859e8e79a`; D `4e9717ac0451a0bd5dc0e662bf6183873b696c88f0b781b8c85ccc5ab6fdd635`; U `2164f119b367ee04e2a76e5310606b8a508a6212a128ea0d1e3facb0e0b1b45c`.

Reused: KernelMode's documented success branch; enabled privileges and per-item outputs for UserMode; cross-guest writes require shared token locks; copies cannot replace captured references; token-slot replacement differs from token mutation; cleanup cannot wait for final object destruction; named-pipe impersonation currently copies the server's primary token. Reports28/33/42 already establish these limitations and the pending native evidence.

## New actionable findings

**1. Primary-token reuse omits a server prerequisite.** Local `struct token` has no mapping list (`T:103–118`); `token_ops.get_kernel_obj_list` is `no_kernel_obj_list` (`T:160`), which returns NULL (`D:256–259`). Thus `set_kernel_object` (`D:297–318`) cannot register a token; `set_kernel_object_ptr` reports failure (`D:1092–1111`). Nevertheless, `alloc_kernel_object` (`N:155–178`) logs that failure and returns an allocation. `ObReferenceObject` (`N:222–249`) increments the local count but merely logs a failed server grab; `ObReferenceObjectByHandle` (`N:350–367`) can consequently return success without pinning the token.

The pinned [upstream token source](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/server/token.c) includes `struct token.kernel_object`, `token_get_kernel_obj_list`, its ops entry and initialization in `create_token`. These are missing dependencies of [PsReferencePrimaryToken and PsDereferencePrimaryToken](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntoskrnl.exe/ntoskrnl.c) described in report33. Importing only their bodies/spec would not establish lifetime or alias identity. Registration/acquisition failures must roll back before temporary handles close. Destruction goes through `release_object` (`server/object.c:546–558`), `free_kernel_objects` (`D:860–887`) and `dispatch_free` (`N:837–842`); eventual proxy freeing is distinct from releasing a context's references.

**2. Impersonation references need attachment state, not an effective-token helper.** `thread_get_impersonation_token` (`server/thread.c:1649–1655`) returns primary when there is no client token. Microsoft's [PsReferenceImpersonationToken contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psreferenceimpersonationtoken) instead returns NULL and also reports CopyOnOpen, EffectiveOnly and ImpersonationLevel. Local `server/thread.h:97` and `security_set_thread_token` (`T:179–199`) retain only a token pointer; `get_token_info` (`T:1533–1544`) provides token-level information, not the missing attachment flags. `NtDuplicateToken` explicitly ignores effective-only (`dlls/ntdll/unix/security.c:223–254`). Constants for those outputs would invent state. `open_token` (`T:1216–1247`) also distinguishes absent and anonymous tokens; an opening error must not become primary fallback.

Identity has a concrete idle hazard beyond report33's general warning: `wine_ntoskrnl_main_loop` clears the TEB slot (`N:944`) without clearing `client_tid`, while `KeGetCurrentThread` on the dispatcher reopens that ID (`N:2564–2582`). Worker threads use their own host thread. `get_next_device_request` selects `irp->thread` or server `current` (`D:1020–1022`); cancellation/free requests have no originating thread (`D:699,880`). Capture therefore needs an explicit active-subject lifetime; last-request IDs and cancellation callbacks cannot identify the original requestor automatically.

**3. Deferred writes need caller-specific completion and a deadlock rule.** `call_req_handler` sets global `current`, uses a stack reply and clears current (`R:292–324`). `send_reply`/`write_reply` use that thread's reply fields (`R:225–289`); `read_request` frees variable input (`R:360–362`). Replaying a saved handler from an unlock callback under the unlocking thread would use the wrong request/reply/error state. Store the waiter, output capacity, resolved object and owned input; execute mutation and deliver exactly one terminal result independently of the unlock caller.

`server_call_unlocked` synchronously waits for a reply (`U:276–310`). Therefore postponing a writer from a thread holding a conflicting read lease prevents that thread issuing its unlock. This is a source-derived deadlock scenario, not a reproduced hang. Define self-conflict and overlapping-context nesting before writer preference; never claim an unacquired lock succeeded. `cleanup_thread` (`server/thread.c:610–650`) must cancel the waiter and revoke its leases before reply/input disposal. Driver IRP cancellation (`D:685–701`) is not cancellation of this blocked RPC.

## Minimal dependency cut and evidence still needed

Next: integrate the upstream token mapping prerequisite with checked registration/reference rollback; then establish explicit requestor and impersonation attachment capture. Preserve report42's atomic two-token acquisition, alias deduplication and writer gates at `adjust_token_privileges` (`T:1252`) and `set_token_default_dacl` (`T:1579`), adding the caller-specific deferred completion above. Privilege-check bodies remain downstream.

| Needed evidence | Boundary / adversarial case |
|---|---|
| Token ownership | Alias handles, temporary-handle closure, registration/grab failure, slot replacement and last release; runtime lifetime evidence remains absent. |
| Subject contract | Distinct requestor/host tokens, idle/worker/cancel contexts, absent/anonymous token and attachment outputs; native kernel results still needed. |
| Lock ordering | Cross-process enable/remove/disable/DACL writes during readers; nested overlapping contexts and self-conflicting writer; no scheduling evidence obtained. |
| Terminal cleanup | Cancellation, waiter/holder disconnect, subject exit, stale cookie and delayed reply; exactly one completion and balanced references/leases remain unverified. |

No runtime patch or security-success stub was produced. Wine remains identifiable and game gates are unchanged. HY2 is incomplete; the parent must independently verify and integrate this report.

## Parent integration review

The parent independently matched the three additional SHA256 anchors above, then read the token ops/mapping path, allocator/reference failure handling, dispatcher TEB reset/KeGetCurrentThread, stack reply ownership and synchronous server wait. The Microsoft impersonation contract was independently opened and checked for NULL-on-absence and the three output parameters. This is a targeted source review, not an independent rehash of every report42 file or a native contract run.

Adopt the corrected dependency order for implementation preparation: server token mapping plus checked registration/reference acquisition → explicit request subject and attachment state → shared context lifecycle and caller-owned completion → privilege checks. A token mapping candidate may be prepared and compiled separately; it does not establish the rest of the group, receive default engine adoption, or unblock a game. No Windows access, new native result or scheduling evidence was obtained during integration.
