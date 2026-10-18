# macOS WaitAll acceptance fixtures

These programs exercise the Wine 11.18 candidate chain recorded in
`engine/porting/r-v1-inputs.json`. They do not select a product engine or change a
user bottle. See research77 for the measured build, source hashes and results.

## Programs and evidence boundaries

| File | Scope |
| --- | --- |
| `waitall.c` | Actual Win32 calls in PE32/PE32+: not-ready/ready mixed objects, 8/64-object waits, APC priority, abandoned mutexes, named and duplicated handles, concurrent permit accounting, fixed-duration random pressure, and benchmarks. |
| `waitall-lifecycle.c` | Actual suspend/context/resume/terminate, owned child creation/reaping, and repeated killed WaitAny waiters. |
| `waitall-processes.c` | Four actual self child Wine processes with four role threads each, named shared objects, atomic counters and final permit accounting; bounded owned Job cleanup. |
| `msync-coordinator.c` | Includes the **production** Darwin coordinator header. Tests interrupted commits, notification recovery, stale helpers, stopped live owners, killed contenders and thread/process concurrency. This is not Wine API or Windows equivalence evidence. |
| `stats-snapshot.c` | Reads the actual pump's seqlock counters and kernel process resource observations. A missing/disabled snapshot is unavailable, not zero occupancy. |
| `owned-processes.swift` | Uses production process identity and prefix checks to observe/stop only the owned laboratory. Never spawns Wine. |
| `policy-probe.c` | Negative child-launch check using a copy of this harmless fixture named `C:\CiderTests\yuanshen.exe` in an otherwise empty laboratory. Never point it at an installed game. |
| `run-mac.py` | Invokes the laboratory's packaged `ciderctl` through its actual GUI/XPC spawner, records exact native-module and EXE hashes, validates completion, and performs bounded timeout cleanup. |

## Build

Use the project's existing GCC mingw-w64 frontends, separately for `i686` and
`x86_64`, with `-O2 -g -Wall -Wextra -Werror`. Output names expected by the driver
are `waitall-{arch}.exe` and `waitall-lifecycle-{arch}.exe`. No new compiler is
required. `policy-probe.c` is an independent PE executable.

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

Compile `owned-processes.swift` against the same laboratory app's CiderKit
modules. Do not replace it with process-name matching or a global `wineserver -k`.

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

The duration driver compares the host activity clock with wall time. Sleep or
an unexplained clock gap invalidates the continuous run and stops only the owned
laboratory; an elapsed guest hour alone is insufficient. Locking the screen is
compatible with this command path. Closing a MacBook lid can suspend it despite
an idle-sleep assertion, so a continuous run requires an awake Mac.

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
