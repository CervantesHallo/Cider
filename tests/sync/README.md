# macOS WaitAll acceptance fixtures

These programs exercise the Wine 11.18 candidate chain recorded in
`engine/porting/r-v1-inputs.json`. They do not select a product engine or change a
user bottle. See research77 for the measured build, source hashes and results.

Optional candidate0110 also has `msync-waiter-hint.c`. Build it against the exact
applied `wine/msync_waiter_hint.h` and paired coordinator header using the native
flags below. It exercises real shared Darwin ulock park/wake after a forced
payload ABA, observes the exact reader's kernel wait state, checks concurrent
single/multi hint balance and sticky fallback, and checks cleanup. Run it in an
owned temporary directory with an outer deadline; its own 20-second alarm kills
only its test process. Mutations that erase active counts, erase singles while
clearing MANY, or lose sticky fallback must fail. This is helper/kernel evidence,
separate from Wine/Windows API equivalence and the candidate's own runtime hour.

## Programs and evidence boundaries

| File | Scope |
| --- | --- |
| `waitall.c` | Actual Win32 calls in PE32/PE32+: not-ready/ready mixed objects, 8/64-object waits, APC priority, abandoned mutexes, named and duplicated handles, concurrent permit accounting, fixed-duration random pressure, and benchmarks. |
| `waitall-lifecycle.c` | Actual suspend/context/resume/terminate, owned child creation/reaping, and repeated killed WaitAny waiters. |
| `event-lifecycle.c` | Reuses one manual-reset event across bounded owned-thread termination and fresh-worker wake rounds. Confirms reaping and closes owned handles; ready/live does not prove internal wait registration. |
| `waitall-processes.c` | Four actual self child Wine processes with four role threads each, named shared objects, atomic counters and final permit accounting; bounded owned Job cleanup. |
| `msync-coordinator.c` | Includes the **production** Darwin coordinator header. Tests interrupted commits, notification recovery, stale helpers, stopped live owners, killed contenders and thread/process concurrency. This is not Wine API or Windows equivalence evidence. |
| `msync-single-waiter.c` | Candidate0118's exact production hint helpers: collision, saturation, fork, conservative process-death residue, owned cleanup and a real Darwin ulock payload ABA. Native helper evidence, separate from Wine and Windows equivalence. |
| `stats-snapshot.c` | Reads the actual pump's seqlock counters and kernel process resource observations. A missing/disabled snapshot is unavailable, not zero occupancy. |
| `owned-processes.swift` | Uses production process identity and prefix checks to observe/stop only the owned laboratory. Never spawns Wine. |
| `policy-probe.c` | Negative child-launch check using a copy of this harmless fixture named `C:\CiderTests\yuanshen.exe` in an otherwise empty laboratory. Never point it at an installed game. |
| `run-mac.py` | Invokes the laboratory's packaged `ciderctl` through its actual GUI/XPC spawner, records exact native-module and EXE hashes, validates completion, and performs bounded timeout cleanup. |

## Build

Use the project's existing GCC mingw-w64 frontends, separately for `i686` and
`x86_64`, with `-O2 -g -Wall -Wextra -Werror`. Output names expected by the driver
are `waitall-{arch}.exe` and `waitall-lifecycle-{arch}.exe`. No new compiler is
required. `policy-probe.c` is an independent PE executable.

Compile `event-lifecycle.c` with those same per-architecture flags into
`event-lifecycle-{arch}.exe`. Select `--suite single-event --mode correctness
--iterations 64 --stats`. The supported bound is 1..256 rounds. The driver
requires all requested terminate/wake/context rounds, exact created/reaped counts
and no emergency cleanup termination. Each API join retains its 5000 ms bound.

For optional candidate0118, build `msync-single-waiter.c` against its exact
magic7 `wine/msync_coord.h`, separately for native arm64 and x86_64. Like the
coordinator it needs a fresh owned directory and process session, plus an outer
deadline; its own watchdog is 120 seconds. Usage is `fixture <owned-dir> all`.
Never test mutations in an installed engine. The additional hash buckets are
conservative hints, not kernel waiters or object references. `stats-snapshot.c`
compiled with this header explicitly marks its bucket aggregates as noncoherent;
a missing mapping is unavailable. An old-format reader must reject this layout.

Build `waitall-processes.c` with the same per-architecture C flags into
`waitall-processes-{arch}.exe`. Invoke it through the driver with
`--suite processes --mode correctness --iterations 100000`. This checks actual
Wine client/server sharing; it is separate from native-header process tests.

Build the native helpers with the pinned macOS 15.4 SDK and the candidate source
include directory. `msync-coordinator.c` and `stats-snapshot.c` use
`-arch x86_64 -O2 -g -Wall -Wextra -Werror`; run under Rosetta on Apple Silicon.
The coordinator executable **requires a fresh process session**, for example
Python `Popen(..., start_new_session=True)`. Its independent 120-second watchdog
can kill only that verified private session/group. The outer runner must retain
a bounded deadline, kill/reap that same owned group on failure, and reject a
missing completion marker. All normal/failure teardown precedes its final JSON.

When validating optional candidate0106, add `-DCIDER_TEST_MSYNC_SNAPSHOT` when
building `msync-coordinator.c` against its exact paired header. This enables
snapshot owner/state/sequence/epoch rejection cases alongside the recovery and
concurrency cases. Omit the define for older headers. Separately mutated header
copies must be rejected; never mutate the installed candidate to test a negative.

For optional candidate0108, use `-DCIDER_TEST_MSYNC_DEFER_NOTIFY` instead. It
records whether any notification callback ran under its executor's owner and
requires actual notifications. The original under-owner header is a negative
control for this **new scheduling invariant**, not evidence that its Windows
semantics were incorrect. Also reject isolated headers omitting recovered wakes
or the stale-helper sequence guard; retain native teardown evidence.

Compile `owned-processes.swift` against the same laboratory app's CiderKit
modules. Do not replace it with process-name matching or a global `wineserver -k`.

For optional candidate0109's version5 header, combine
`-DCIDER_TEST_MSYNC_TAGGED_DECISION -DCIDER_TEST_MSYNC_DIRECT_FINISH`
with `-DCIDER_TEST_MSYNC_DEFER_NOTIFY`. The native harness reads the actual tagged
phase/serial, invokes the production direct finish, and ends with a constructed
maximum-serial input in a real child. It requires fatal rejection before payload
write or wrap and never resets that serial. Omit the tagged/direct defines for
older headers. Reject isolated variants that clear before notifying, omit the
version guard or callbacks, or allow serial wrap. The delivery-entry crash runs
even if a broken header already cleared its phase, exposing lost notifications.

## Owned laboratory

Prepare a fresh `waitall-mac-*` directory with an `owner.json` whose `root` is its
absolute canonical path. Inside it keep:

- `source/out/Cider.app`: a separately signed copy of the app built from copied
  sources, with a distinct bundle identifier/preferences domain;
- `private-home`: the sole `CIDER_HOME`, containing only empty test bottles;
- the compiled helpers and fixtures named above;
- `runs/`: immutable per-run result records and references to raw session logs.

The private app binds only explicitly recorded native ntdll/wineserver pairs and
loader hashes, additionally restricted to this laboratory root. Production
`EnginePolicy` stays unchanged. Package complete, matching Wine runtimes; mixing
old CrossOver native modules with new Wine PE modules is invalid. Record source,
compiler/SDK, artifacts and initial user-app/bottle/budget hashes. The engine id
must start with `cider-lab-`. Never clone a logged-in Steam bottle. Provision
Wine Mono before bootstrap so its installer dialog cannot masquerade as a wait
deadlock. The actual accepted laboratory and private binding are recorded in the
research evidence; this driver deliberately does not create trust bindings.

`sync` is a bottle setting fixed before server launch. The server comparison must
use the **same native pair** as the candidate, in a separate empty bottle with
`settings.sync = "server"`; do not toggle it per command. Statistics must likewise
be enabled on the first server launch, then verified from the real shared file.

## Run

```sh
python3 tests/sync/run-mac.py --root "$LAB" --bottle "$BOTTLE" --arch x86_64 --mode correctness
python3 tests/sync/run-mac.py --root "$LAB" --bottle "$BOTTLE" --arch i686 --suite lifecycle --mode correctness --iterations 64 --stats
python3 tests/sync/run-mac.py --root "$LAB" --bottle "$BOTTLE" --arch x86_64 --mode stress --iterations 100000 --threads 8 --stats
python3 tests/sync/run-mac.py --root "$LAB" --bottle "$BOTTLE" --arch x86_64 --mode duration --iterations 3600 --threads 8 --stats --timeout 3720
python3 tests/sync/run-mac.py --root "$LAB" --bottle "$BOTTLE" --arch x86_64 --mode bench --iterations 250000 --threads 4
```

Every run compares the host activity clock with wall time. Sleep or an
unexplained clock gap in either direction invalidates the run and stops only the
owned laboratory, including short lifecycle runs whose guest checks passed.
The record preserves guest checks separately from timing validity; a clock gap
does not establish an engine deadlock. An elapsed guest hour alone is
insufficient for continuous pressure. Locking the screen is
compatible with this command path. Closing a MacBook lid can suspend it despite
an idle-sleep assertion, so a continuous run requires an awake Mac.

Formal performance may use either AC or battery power with low-power mode off.
Declare one power condition for the whole cohort and check it before/after every
run. Do not append battery measurements to an incomplete AC cohort. Preserve
interrupted/invalid trials and their original files.

The current benchmark source uses acquire-only reads for its cancellation flag;
it does not change the stress or lifecycle programs. To retain an already frozen
v3 EXE, compile the new benchmark into `waitall-bench-v4-{arch}.exe` and select
`--benchmark-fixture v4`. Default `v3` selects the legacy `waitall-{arch}.exe`
filename in the prepared lab. These labels select artifact slots: bind the exact
EXE/source hashes for every comparison rather than infer a version from a name.

Run serially. Do not compile, profile or run pressure workloads alongside a
performance comparison. Freeze repetition count/order before measurement and
retain all results. Benchmark records distinguish:

- actual sampled **event signal-to-peer-return latency**, including timestamp
  handoff overhead, with no subtraction; the peer is not guaranteed blocked;
- actual sampled **four-API round-trip latency**;
- per-batch timing distributions, which are **not** individual latency tails;
- aggregate operation counts, including the longer private-CS attribution loop.

Process handle counts alone are not pump occupancy/leak evidence. Check the pump
allocation/free balance and baseline across termination rounds, long-run current
occupancy, fallback use, producer identity/resources, and final owned-process
cleanup. Neither these fixtures nor a performance threshold passing establishes
full M1, Windows kernel equivalence, or a Steam/game compatibility verdict.

The v3 benchmark uses a fixed 16,000,000 private-CS warmup iterations per worker
and 1024 times the requested measurement iterations for this attribution control.
Other workloads retain 1024 warmup iterations (or fewer for a smaller workload).
These actual counts are emitted in the benchmark metadata. Retain earlier v2
control/latency results separately; a longer control is not a threshold change.
### Payload-store interruption for cached-slot candidates

The coordinator fixture maps each slot on a distinct real shared page. Only the
owned child protects slot2 as read-only. The production header's actual second
payload store faults through SIGSEGV/SIGBUS after the first store; the parent
keeps its writable mapping and checks recovery. The handler checks the fault
address/phase and uses only async-signal-safe operations. It never logs,
allocates, waits, recovers, or returns to the faulting store. No production-header
hook or repeated slot callback is needed.

The filtering case checks payload/notification order after unchanged entries
are removed, retains an unchanged entry that owes a notification, and rejects a
new journal serial for an entirely unchanged non-notifying commit. Candidate0111
only caches normally pinned slot addresses inside one owner-held commit;
recovery resolves its own mappings. Native checks do not replace Wine runtime,
continuous-pressure, or performance acceptance. The candidate remains optional.

The count1 fixtures protect slot1 for a real payload-store fault and kill at
notification entry. They also construct an unpublished CLEAN metadata input
(explicitly not an observed interrupted copy), exercise queued delivery, and
check unchanged/empty/same-value-notifying commits. Both COMMITTED and APPLIED
recovery are permitted at notification interruption; required effects/wakes
must actually complete. Candidate0115 keeps the multi-entry implementation and
recovery unchanged. Candidate0116 reuses the existing native thread identity;
its header is identical, so header evidence can be reused only with that exact
hash, while its actual caller/runtime validation remains separate.

For failure diagnosis only, compile `waitall-lifecycle.c` into the separate
`waitall-lifecycle-diag-{arch}.exe` slot and select
`--suite lifecycle --mode correctness --lifecycle-fixture timeout-diagnostic`.
The original 5000 ms child-readiness check fails before an optional 10 second
capture window begins. Late readiness never changes that failed check; the
standard fixture and its frozen executable remain unchanged. Diagnostic marker
rows are retained in the run record. Neither a diagnostic pass nor a later
retry replaces an earlier acceptance failure.

The monitor only reports interruption; the main thread owns submission and
cleanup. On a timeout or clock gap it first reaps the CLI, then uses the
helper's `stop-runtime` mode: confirm the exact private laboratory GUI has
terminated, drain its Wine processes, and require two empty observations. This
prevents a request paused in the GUI from spawning after a cleanup snapshot.
The helper only accepts the owned app path and a laboratory bundle identifier;
it does not stop the user's normal Cider app. Check
`interruptionCleanup.confirmed`; errors do not establish that cleanup finished.
Each request has a fresh `WAITALL_RUN_ID`, matched against the exact bottle and
engine in its session manifest. This binds the log even if the interrupted
`--wait` client never printed its final log path. Partial client output and
failure markers remain available; a missing end marker is explicitly recorded
as an incomplete diagnostic capture.
