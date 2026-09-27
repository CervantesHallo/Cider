# Cider

Open-source macOS app (Apple Silicon) that runs Windows programs and games on Wine — aiming at CrossOver parity or better. The plan lives in `docs/plan/` (start with `00-strategy-and-decisions.md`); research with sources is in `docs/research/`.

## Layout

- `Packages/CiderKit/` — all business logic (SwiftPM, no UI). Modules: CiderCore, CiderSchema, CiderStore (engines, D3DMetal import), CiderRuntime (spawning Wine, preflight gate), CiderBottle, CiderPE, CiderData (compat DB, recipes, red lines), CiderIntegration (Steam, catalog, recipe installer).
- `Tools/ciderctl/` — CLI on top of CiderKit.
- `App/` — SwiftUI app (`scripts/build-app.sh` → out/Cider.app). UI design: https://claude.ai/artifact/CX8EF8Q8pG6EXzbHWxtdJk
- `engine/` — `toolchain.sh`, `deps.sh`, `build.sh <recipe>`, `bundle-gstreamer.sh`, patches (build with a macOS 15 SDK). `data/` — compat entries, profiles, recipes (bundled into the app). `scripts/` — app/DMG build, smoke test, `wine-profile.py` (lldb profiler), Steam cold-start gate.

## Build and run

```sh
cd Tools/ciderctl && swift build          # builds CiderKit too
.build/debug/ciderctl engine list
.build/debug/ciderctl bottle create "Name" --locale zh-Hans   # ja | zh-Hans | zh-Hant | ko | en
.build/debug/ciderctl run -b Name --wait /path/to/setup.exe /S
.build/debug/ciderctl run -b Name 'C:\Program Files (x86)\App\app.exe'
```

`CIDER_HOME=/tmp/x` redirects all data (App Support, Caches, Logs) for experiments. Unit tests need the `Testing` module, which ships with Xcode, not the Command Line Tools.

Runtime data: `~/Library/Application Support/Cider/{Engines,Bottles}`, logs per session in `~/Library/Logs/Cider/sessions/<bottle>/<time>-<program>/wine.log`, spawn audit in `~/Library/Logs/Cider/audit/spawn.jsonl`.

## Rules

- Engines are x86_64 Wine under Rosetta for now (Engine R); keep code architecture-neutral (`cpu_backend`), Engine A (arm64 + FEX) comes later.
- Only 64-bit (new WoW64) bottles. 32-bit programs run inside them.
- Build Wine environments explicitly; never pass the caller's environment through wholesale (no stray `DYLD_*`, locale).
- Sync is bottle-wide (`SyncMode`, default msync): wineserver fixes it at startup, so never set `WINEMSYNC`/`WINEESYNC` per launch.
- Always set a full `xx_YY.UTF-8` locale for Wine; a bare `UTF-8` silently becomes en-US/1252.
- Apple's D3DMetal is never committed or bundled — users import it from Apple's Game Porting Toolkit.
- Never start Steam in a clone of the user's logged-in bottle (it invalidates the saved login).
- Anti-cheat red line: never tamper with, spoof, hide from, or bypass anti-cheat; no success-faking kernel API stubs; Wine stays identifiable. Unsupported titles get a clear message instead of a launch.
- No legal/licensing sections in docs; mention a license only when it forces a technical design.
