# Isolated real product acceptance

These are narrow regression probes for the GUI/XPC/Wine execution path. The controller operates only in a marked, dedicated `runtime-acceptance-*` directory containing `owner.json`, an exact copied Cider.app, an engine copy, freshly initialized test bottle, and compiled owned probes. Never copy a logged-in bottle. It is a test scenario, not a general process sandbox or Windows watchdog.

The retained local experiment is `/Users/cervantes/Cider/out/runtime-acceptance-20261016-84kr3z6f`. Its owner metadata, private binaries, prefix and complete logs remain there. The committed results keep both failures and final confirmations. The controller may be invoked against that explicit owned root:

```sh
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-root --resume
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-root --resume --early-stop
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-root --resume --only-ui
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-root --resume --only-ui --32
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-root --resume --detach
```

For a new experiment, copy a built, strictly verified candidate App and **engine code only** into a new directory, compile `probe.c` with MinGW `-municode`, compile `window-audio.c` for x86_64/i686 with `-lwinmm -lgdi32 -luser32`, and build `host-control.swift` with the installed macOS SDK. Use only a new private CIDER_HOME. `owner.json` must identify `root`, `home`, `engine_copy`, `engine_id`, and a `protected` map of explicitly selected file hashes. All executable helpers belong inside that root. The first run without `--resume` creates the bottle through the actual product GUI; later runs reuse only that known account-free test prefix. Existing/default user data is never a test fixture.

The WinMM candidate module check intentionally expects the distinct `winmm-repair/src/dlls/winmm/waveform.c` build-source marker in the loaded module. When reproducing, retain that separate build directory and record the linked PE hashes. Do not infer the loaded module from the prefix DLL's disk hash: Wine builtin redirection may load the staged engine module instead. The probe reads only its own loaded module.

The native WindowServer check records published window bounds, not screenshots or an assertion that a human saw an unobscured window. GDI surface pixels and silent waveOut buffer completion are separate evidence. Silent output is not an audible-quality or microphone test. No camera, microphone, account login, Steam clone, protected game, Windows connection, or kernel driver is part of these scenarios. Each completed batch verifies its own jobs and processes exited and preserves the private root on a failure.

The detach case ends only the owned waiting CLI, confirms the guest remains live and listed by the real product, then uses ordinary bottle cleanup. It does not signal the guest as a side effect of disconnect. `product-detach-first-cleanup-failure.json` retains the initial cleanup result; `product-detach-final.json` references its SHA and the independent final absence check. Console Wine had not registered its private engine App (`lsregister -u` returned -10814). The controller now accepts that specific not-found result only after a successful database read confirms the exact private path is absent. Engine bundle identifiers may be shared with production copies; never unregister other paths by identifier.
