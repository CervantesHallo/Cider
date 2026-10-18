# K3 checked reference callers

Candidate `engine/patches/candidates/0012-reference-callers.patch`, based on
`1340a49` and the read-only `hy2-token-map-20261013/src` artifact (originally 0009/0010 applied). The adopted repair queue applies 0009 then 0012; 0010 is now retired, and 0012 has no dependency on its functions. It changes only `dlls/ntoskrnl.exe/{ntoskrnl.c,
ntoskrnl_private.h,sync.c,pnp.c}` when applied. It is not selected by a recipe.
No commits, installed engines, driver execution, Windows connections, or product
sessions are part of this delivery.

## Fixed behavior

- `ObReferenceObjectByName` acquires through the checked helper and publishes
  only after success. `drivers_cs` protects tree lookup through acquisition,
  so `IoDeleteDriver` cannot remove the tree's owning reference in the gap.
  A nonmatching object type returns `STATUS_OBJECT_TYPE_MISMATCH`; UserMode,
  nonzero desired access, access state, parse context, and unsupported flags
  return `STATUS_NOT_IMPLEMENTED`. The supported flags are zero and
  `OBJ_CASE_INSENSITIVE`; a case-sensitive mismatch returns
  `STATUS_OBJECT_NAME_NOT_FOUND`. Unknown namespaces remain not implemented.
- `ObOpenObjectByName` rejects null attributes/output and unsupported relative
  roots before acquiring anything; it already releases its acquired object
  after an open failure. No output is published by the reference call on error.
- All four private constructors return `NTSTATUS` and preserve query and
  allocation/registration errors. Process, thread, and event queries happen
  before proxy registration, so query failure cannot leave a registered partial
  proxy. The thread's temporary process handle is closed after a failed nested
  lookup and before a failed thread allocation/registration. No new strong
  process reference is introduced here; see the lifetime limitation below.
- `IoQueueWorkItem` acquires one reference before submission. A real
  `TrySubmitThreadpoolCallback` false return captures its NT failure status,
  releases that one reference, and raises. In this source, kernelbase forwards
  `TpSimpleTryPost` through `set_ntstatus`, which preserves the last NT status.
  A defensive `STATUS_UNSUCCESSFUL` fallback prevents a zero-status raise.
  No worker runs on a false return. No synchronous callback fallback is used.
- Successful submission transfers that reference to the worker. The worker
  uses the captured device, including when its callback frees the work item,
  and releases through `__FINALLY_CTX` on normal return and unwind.
- PnP `load_function_driver` releases its named driver reference on both
  normal AddDevice return and exception unwind.
- `KeGetCurrentThread` preserves failed client-thread opens, closes a temporary
  handle before raising a mapping/constructor failure, and caches only success.

## Production caller audit

A full source search found these production callers (plus the implementation,
header declarations, and native driver tests). `ObReferenceObjectByPointer` is
an existing explicit `STATUS_NOT_IMPLEMENTED` implementation with no reference
acquisition; K3 does not claim it is implemented.

| Caller | Error/cleanup disposition |
| --- | --- |
| `ObReferenceObjectByHandle` | Existing 0009 checked status path; output only after acquisition. Core map/reference atomicity belongs to K1. |
| `IoQueueWorkItem` | Fixed checked acquisition, failed submission rollback and explicit raise, worker unwind cleanup. |
| `ObReferenceObjectByName` | Fixed checked acquisition, type/mode/input rejection, protected lookup, output publication. |
| `ObfReferenceObject` | Retains explicit raising void wrapper; no prior local resource to unwind. |
| `IoGetAttachedDeviceReference` | Retains explicit raising wrapper; no prior local acquisition or temporary handle. |
| `PsLookupProcessByProcessId` | Already returns `ObReferenceObjectByHandle` status and closes its handle on success or failure; exercised unchanged. Existing open failure normalization to `STATUS_INVALID_PARAMETER` is retained. |
| `PsLookupThreadByThreadId` | Already returns reference status and closes its handle on success or failure; exercised unchanged. |
| `PsReferencePrimaryToken` (retired 0010) | Not in the repair queue; no replacement token/access path was implemented or counted as validation. |
| `ObOpenObjectByName` | Already checks reference status and dereferences after `ObOpenObjectByPointer` failure; strengthened invalid/unsupported input handling. |
| `pnp.c:load_function_driver` | Checks named reference status before use; fixed reference release through AddDevice normal return, error status, missing callback, and unwind. The callback and loader dependencies are host mocks only. |

`IoCreateSynchronizationEvent` and `IoCreateNotificationEvent` already close
the created event handle after mapping failure and publish the handle only on
success. Their failure cleanup is also exercised. These return borrowed proxies
whose lifetime still depends on their open handles.

## Valid void calls and failure preconditions

A live `IoCreateDevice` device starts with a positive local reference; a queued
work item requires its caller to retain a live device and avoid concurrent
queue/free/requeue of the same item. A positive count below `MAXLONG` takes no
backend grab in the existing checked helper. The same applies to a live attached
device under the caller's device-stack lifetime synchronization. This explains
why a backend acquisition failure is impossible on those valid positive-count
paths; resource exhaustion in threadpool submission is still possible and now
raises explicitly after rollback. Count overflow also raises explicitly.

`ObfReferenceObject` accepts other valid mapped objects whose local count can
be zero (for example, a proxy borrowed while a handle keeps the server object
alive). A backend grab can fail there; the existing wrapper raises after helper
locks have been released. Null inputs raise `STATUS_INVALID_PARAMETER`; arbitrary
dangling pointers and unsynchronized deletion violate pointer/lifetime
preconditions and cannot be made safe by a null check. `IoGetAttachedDevice`
itself assumes a valid device before the reference call.

Raising from void/pointer APIs on backend failures is candidate behavior, not a
claim of native failure parity. No failure is converted to successful work.

## Checks and evidence boundaries

`run_checks.py` copies only four source files into a temporary directory,
applies 0012 with zero fuzz/offset, and extracts 20 exact C function definitions
for a host C harness. The dependencies are mocks, not reimplementations of the
function bodies. It checks output sentinels, exact injected statuses, reference
counts, callback-owned frees, submission failures, temporary handles, and cache
publication. Host setjmp models exception control flow: it verifies the cleanup
logic, not Wine's SEH implementation. Five deliberately broken variants must fail
assertions: premature output, missing submission rollback, erased constructor
status, missing temporary handle close, and missing AddDevice unwind cleanup.

`results.json` records the final patch/source/extracted-body hashes, 277 passing
assertions with ASan/UBSan, all five rejected regressions, and successful compile
of all three changed C files for x86_64 and i386. The PE objects use real Wine headers
and `wine/exception.h`; no executable is linked or run. Compile evidence validates
the changed private callback signatures and SEH syntax, not native behavior,
backend transport, concurrency, or whole-engine integration.

Reproduce the host checks from the worktree root:

```sh
python3 tests/kernel/reference-callers/run_checks.py --source /absolute/path/to/patched-0009-0010/src
```

For PE compile checks add `--build /absolute/path/to/configured/build`; pass
`--x64-cc` and `--i686-cc` with compiler paths if they are not on PATH. Optional
`--report /path/to/results.json` writes the machine-readable result.

## Integration and remaining issues

1. Retain K1's map/free locking and atomic reference changes when applying the
   small constructor dispatch hunk: replace the pointer-returning call and
   guessed `STATUS_NO_MEMORY` with `status = type->constructor(handle, &obj)`.
   `alloc_kernel_object_checked` becomes an internal cross-file function for
   `sync.c`, with a private header declaration; no public export is added.
   All four constructor definitions and the private callback signature must
   move together. K3 does not edit the core checked reference helper or
   `ObReferenceObjectByHandle` implementation.
2. K3 adds no `obref_cs` acquisition. Named acquisition starts under
   `drivers_cs`; if K1's helper takes `handle_map_cs` then `obref_cs`, the order
   is `drivers_cs -> handle_map_cs -> obref_cs`. Keep free/release callbacks
   from acquiring `drivers_cs` in the reverse order. This isolated mock harness
   does not establish absence of races in the combined patch queue.
3. **Existing lifetime debt:** `thread->process` and the TEB thread cache still
   contain borrowed proxy pointers. K3 prevents partial initialization but
   does not introduce the shared ownership/destruction protocol needed to
   prove their lifetime across process/thread exit and server free delivery.
   K1's map/free lock alone is not evidence that these persistent links own a
   reference. This must remain an explicit integration review item.
4. **Failure contract/parity:** additional query failures now fail construction
   instead of yielding zero-filled objects. Unsupported named access/relative
   roots now fail instead of being ignored. These are deliberate honest-error
   changes, with a compatibility cost for callers that depended on the old
   incomplete behavior. Rollback is removal of 0012 as a whole; reverting only
   the callback signature or only some constructors is unsafe. Native error,
   object-access, and void API failure contracts remain separately pending.
5. The baseline ByHandle wrapper still ignores handle-information output and
   does not independently validate a null output pointer; K1 owns that wrapper.
   Access-right parity is not established by these KernelMode caller checks.
   K3 does not claim object-manager completeness or repair unrelated existing
   success stubs. Existing work queue priority/requeue semantics and wider
   driver callback exception containment are also not proven here. The two
   callback sites patched here release their reference during unwind; that is
   not a claim that an arbitrary driver exception is otherwise recoverable.
6. Parent should rerun the checks appropriate to the combined K1/K2/K3 source.
   The source fixing work is delivered; native reference and parity remain
   pending separately. No native/environment limitation is used to defer these
   source fixes.

Parent integration supersedes the early K1 lifetime limitation: 0013 adds stable temporary handles and map/FREE synchronization. Server thread objects hold strong process references (thread.c create/destroy); current IRP owns its thread until the next request, before which the TEB borrow is cleared. This bounds the supported borrowed edges; it is not permission to retain borrowed pointers after their owner scope. Final parent source/host evidence is separate from this original 277-assertion harness.

## 最终整合结果（2026-10-15，覆盖前述早期限制）

队列已调整为0009→0012→0013，0010撤出。最终源码的mapper被拆为不自行加锁的locked函数和逐层finally解锁的公开借用包装，ByHandle采用同权限私有稳定句柄；thread构造器的临时进程句柄也有独立finally。HandleInformation非null明确不支持、输出为null明确失败。

使用`--already-applied --source /path/to/repaired/src`可直接验证合并源码，不重复应用0012。最终313条断言及6个故意破坏的变体全部按预期；新增变体单独检查嵌套构造器清理。其backend、句柄、SEH控制转移仍为模型；object-lifetime检查覆盖实际锁包装和双线程，真实i386/x64模块构建另有记录。最新results.json绑定最终源码；早期277条结果保留为历史描述，不冒充最终整合结果。
