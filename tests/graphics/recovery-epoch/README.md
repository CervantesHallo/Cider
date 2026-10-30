# Recovery epoch fixture v1

Private diagnostic fixture on Wine11.18 plus patches through0025. The exact
Native/PE source was independently reviewed, built and run through Cider GUI/XPC
in an isolated bottle. See docs/research/109-r-v1-recovery-epoch-20261031.md and
its evidence for actual results and binary identities. This is not renderer,
Windows-native, performance, game or default-engine qualification.

Apply diagnostic.patch to the ordinary group through0025; add this directory
to the explicit include path. Keep the matched watch2 Native/PE initialization
and both Unix arrays at8 entries. Use the supplied spec with -Fwinemac.drv.
The fixed/before Native module and client are identical. Apply before.patch
only to the diagnostic PE source and relink both PE drivers for the control.
Client builds with MinGW GCC, -std=gnu11 -O2 -Wall -Wextra -Werror, user32/ntdll
and the ordinary status_cell.h. Never launch Wine directly or use a logged-in
Steam bottle copy.

## Narrow schedule and oracle

Run the same client and Native module in one fresh isolated process per PE variant
and PE width, without arguments. The intended fixed result is **29 checks / 0
failures**. The single primary is **CHECK20**, whose only predicate is the frozen
selected callback's actual `decision_needed == 1`. CHECK19 first requires the
entire qualifying schedule, actual old zero return, high epochs, real protected
retained Native setup, exact target-root absence and timely event release.

1. ARM_EMPTY registers and ends an ordinary empty `producer_setup`. Its real
   synchronous RECOVERY_ARM starts the ordinary timer and advances its epoch.
   End frees that actual empty setup; no NT handles were opened. Native pending
   becomes zero through ordinary state, not an injected recovery result.
2. Exactly one timer invocation with an actual successful Native recovery return
   and pending0 is selected after MACDRV_CALL returns. It captures its real old
   epoch and waits on an owned event before entering the ordinary equality guard.
   It never waits while holding watch_lock. Other timer callbacks are untouched.
3. ACQUIRE invokes ordinary `producer_acquire` for the actual root epoch and
   current PID/TID, generation2^33. Parent's real protected-temporary-thread fault
   from107 makes the checked initial NtClose fail c0000235. The ordinary setup
   FIFO owns that protected H; no producer/status cell or PE process watch exists.
   Its actual RECOVERY_ARM advances the epoch once more and sets needed=true.
4. Client verifies retention and the still-held selected tick, destroys the real
   Win32 root, then verifies absence of that exact HWND+stored root epoch while
   the Native setup remains. It signals the owned event outside watch_lock.
5. Immediately after the ordinary equality guard/body, under the same exclusive
   lock, the selected invocation records actual current epoch, actual needed and
   active_timer presence once. A later tick cannot overwrite that decision.
   Post-unlock code marks that invocation done and checks its actual QPC span.
6. Client clears the actual protected H using real NtQuery/NtSet. Thereafter only
   pure SNAPSHOT and Sleep pacing observe Native owners and PE watch/timer owners
   zero. CLEANUP then closes the fixture's owned gate event; it never recovers
   Native resources. A final pure snapshot confirms observed owners zero.

The **before** PE differs only by removing `observed_epoch == recovery_epoch`
around the ordinary assignment. Its selected stale zero must record needed0,
failing CHECK20 even if later callbacks restore live needed. Do not require
decision_timer0 or infer actual timer retirement from this observation. A later
tick may salvage recovery; alternatively the broken guard may cancel the only
timer and prevent drain. In the latter schedule CHECK23/cleanup can also fail;
that is not an accepted fixed run or evidence of actual process cleanup. This
fixture does not re-ARM, call recover manually, or repair the broken state to
force a 29/1 negative. Parent must classify the bound primary separately from
secondary liveness consequences and independently terminate/clean the process.

## Parent-owned integration points

Basis is ordinary producer-status-source-v6 coordinator and watch version2.
Read source hashes at delivery; the inspected basis was:

```text
remote_coordinator.c f6eaa887cf6c3a565cd99e317d00a93189a426a6e961949262b71934802dc4b6
remote_watch_pe.c    317a9060b2beb646556204bd7e3a2de80d345990357e958cb88a65a83bb33aa0
remote_watch.h       6ce4ad72554f14b45167bb7a25490098c21cb0fe727c0359889c929d40e6efc9
```

- Keep the real107 thread fault after actual NtOpenThread and locked publication
  into setup.thread, before the original NtClose. Query/set actual
  ObjectHandleFlagInformation protection; preserve real return statuses and the
  ordinary checked close/end-setup/FIFO code. Do not stub an NT result. There is
  no section-failure mode here. Before/fixed Native code and fault are identical.
- In the private group only, initialize both Native `next_identity` and PE
  `recovery_epoch` to `((UINT64)1 << 33)`. Client requires actual high root/setup
  IDs and epochs, exact two ordinary increments, and intact64-bit observations
  on both widths. These constants test transport and actual epoch state; they
  are not a coordinator integration/progress claim. The fixture never writes
  needed or the ordinary epoch to manufacture a result.
- Include native.inc after ordinary coordinator definitions. Export the normal
  Unix entry `NTSTATUS macdrv_recovery_epoch_fixture(void *)` as appended slot7
  in both normal/WoW64 arrays, total8. Preserve slot6's pure probe. Add prototypes
  and the matching Unix enum to the private headers; do not alter watch version2
  or USER96. The frame is **208 bytes/version1**, pointer-free and offset-asserted.
- Include pe.inc at the end of the pinned remote_watch_pe.c. Remove the old107
  slot7 PE wrapper/native.inc inclusion from the private group, retaining its
  real fault and read-only probe. fixture.h carries the probe's existing48-byte
  declaration. Export `cider_recovery_epoch_fixture(ptr)` with the supplied spec
  fragment. Client uses actual ntdll/user32 plus ordinary status_cell.h NT types.
- Retain the107 actual `fixture_live_watches` and `fixture_live_timers` diagnostic
  counters, accessible in that PE translation unit. Watch counter spans allocation
  through actual final free; timer counter increments BEFORE CreateTimer and
  decrements only on confirmed create failure or successful/IO_PENDING deletion.
  In-flight deletion stays counted. Existing DLL pinning is required: callbacks
  keep NULL immutable context, pinned code and globals. No caller frame is held.

Before recovery_tick, declare:

```c
static BOOL recovery_epoch_fixture_after_recover(UINT64, NTSTATUS, UINT32);
static void recovery_epoch_fixture_decision_locked(BOOL);
static void recovery_epoch_fixture_after_decision(BOOL);
```

Add a local `BOOL fixture_selected;` and these exact hook placements:

```c
recovered = MACDRV_CALL(remote_watch_recover, &recovery); /* ordinary actual call */
fixture_selected = recovery_epoch_fixture_after_recover(observed_epoch, recovered, recovery.pending);
AcquireSRWLockExclusive(&watch_lock);
if (observed_epoch == recovery_epoch)                    /* ONLY before delta */
    native_recovery_needed = recovered || recovery.pending;
recovery_epoch_fixture_decision_locked(fixture_selected); /* same lock, AFTER */
ReleaseSRWLockExclusive(&watch_lock);
recovery_epoch_fixture_after_decision(fixture_selected);  /* outside lock */
/* all remaining ordinary recovery_tick code stays in order */
```

The recording hook does not reproduce or gate the ordinary predicate. Selection
uses a per-invocation Boolean carried on that actual callback's stack; matching
an epoch on another callback cannot supply the decision. No NT call, QPC call,
SetEvent, event wait or private control lock acquisition occurs in the recording
hook while watch_lock is held. QPC is sampled outside it. Binding must include
effective includes, hooks, fault, counters, high seeds, ABI/arrays, driver pinning,
client/header/spec and resulting Native plus both PE modules. Source hashes of
the ordinary coordinator/PE file alone do not bind these includes.

## Ownership, statuses and bounds

SNAPSHOT and Native RELEASE_TICK/CLEANUP only copy remote_lock-protected counts.
The main setup list remains counted during ordinary FIFO pop/NT close. Native
counts include producers, whole watch registry, all setups, both cleanup FIFOs,
hosts and pending model retirements. Root absence is HWND+epoch only: Wine can
retain other internal roots, and roots==0 is never required.

Native CLEAR copies only the exact captured setup's thread H under remote_lock
when inactive and not busy. It does not retain/dereference a setup pointer after
unlock. Actual protection keeps H valid through the first successful clear;
clearing/cleared latches and the serialized PE control stream prevent replay.
After actual NtSet success it never uses H again, even if ordinary recovery closes
it immediately. Native commits the effect before returning; client commits its
effect latch before deadline rejection. Only explicitly nonacting IO_PENDING can
retry CLEAR. A definitive/uncorrelated failure is logged and not replayed.

This borrow rule requires the real fault to have protected that exact H and no
other actor clearing/reusing it. For a wider contract, a concrete stronger
private-fixture pin is to remove the inactive setup from its cleanup FIFO under
remote_lock, keep its counted main-list owner with busy=true across flag calls,
then requeue/release under lock. Merely setting busy while still queued would
not exclude the ordinary pop. This fixture does not change that ownership policy.

PE private controls serialize on a separate SRW lock never used by timer hooks.
The event is owned globally, pinned across wait and signals, and detached for
CloseHandle only after the selected hook has left it. Failed close restores its
owner. Cleanup first disables new selection and signals outside watch_lock;
IO_PENDING means a hook/control still owns it. It cannot strand a deliberately
blocked callback while waiting for Native drain. `event_owners` counts the
stored event owner plus any in-flight control borrow/close owner; hook_users is
the actual wait pin. This gate is independent of ordinary timer ownership.

Every public operation/poll has strict pre/post four-second QPC checks, including
after JSON output and Sleep. The selected callback has an actual4000ms event
wait and pre-wait/post-decision QPC qualification; `wait_end` is the latter
sample when HOOK_DONE is set. Timeout, WAIT_FAILED or a late decision cannot pass
CHECK19. A scheduling delay can fail this bounded experiment; no sleep supplies
an ordering condition. Unselected overlapping callbacks are not frozen: an
unrelated old zero can invalidate the pre-release needed=true qualification in
the before variant. Such an early failure is not admitted as the selected
CHECK20 regression. The old empty timer may legitimately retire while its one
selected callback is held; pre-acquire acceptance does not require it live.
The new actual ARM must supply live timer ownership while the setup is pending.
Logs record full frames, actual NT/PE operation status,
each check, held/primary/final tuples, PID/TID/birth and actual cleanup outcome.
Native and PE snapshots are separately locked, not falsely one transaction.
Only the selected decision tuple is frozen under the actual ordinary lock.
Final drain requires actual Native/PE owner counts zero and no active/held
timer/gate use; live needed is logged separately and is not treated as an owner
or substituted for the frozen primary decision.

After an earlier failure, cleanup always attempts gate release/close, destroys
the root before flag clear, respects a late applied clear, and uses pure
snapshots to observe drain. No retry of ACQUIRE/ARM, manual recovery call or
unrelated private USER message drives progress. If ownership cannot be confirmed
zero it logs cleanup=false and exits nonzero. Synchronous calls and lock
acquisition are not cancelled by QPC deadlines; parent needs an overall watchdog
and actual process/kernel-handle cleanup evidence. Observed owner counts are
not a complete process leak census or proof of timer callback stack quiescence.

## Delivered source SHA-256

These bind the five source/spec inputs; README's own hash is computed separately
to avoid a self-referential digest. Parent integration edits require fresh hashes.

```text
fixture.h             206e83bb88501ea1b187546e4bc0c34633b19736fb0d46b01d3a0e469f4fe41b
native.inc            8459d28351874cc4eec9224bc652989475a45e73ecb20aed840b0133f8b6c788
pe.inc                ebd7e13e5dba4022d0a0c8041ce150ca84b9ebcc4650120defa7ff64a13f5b70
client.c              148e4d291bd660bc29e60fe34299dcd99b9c191377dbd9d2c3fcd33759d59189
winemac-fixture.spec  9646068e57a06c5acdc434eed8ea5bf336a9ef5997bede14fe62ce66ad2f931f
```
