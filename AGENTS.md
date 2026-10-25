# Cider

Open-source macOS app (Apple Silicon) that runs Windows programs and games on Wine — aiming at CrossOver parity or better. The plan lives in `docs/plan/` (start with `00-strategy-and-decisions.md`); research with sources is in `docs/research/`.

## Goals and collaboration

- Final goals include the full Yuzusoft game catalog and local compatibility for the official China-server Windows clients of Genshin Impact, Honkai: Star Rail, and Zenless Zone Zero. Verify each game's login, interactive gameplay, graphics, audio, input, updates, and reliable relaunch. Launcher rendering, downloaded files, and cloud routes are separate milestones.
- Engineering and product constraints may be reconsidered to achieve these goals (user decision, 2026-10-06). Document each concrete tradeoff's benefit, cost, evidence, acceptance criteria, and rollback in the plan; distinguish proposals from adopted changes. Current implementation defaults describe the baseline, not proof that alternatives are impossible.
- Default to doing research and implementation locally. Use subagents only for exceptionally large tasks, with bounded, independent research or code deliverables while doing critical-path work locally. Start with one or two agents, give each a concrete deliverable and disjoint write scope, reuse completed results, and stop unproductive retries. The parent reviews and integrates findings; an agent report alone is not verification. Include adversarial review of material changes.
- Git author and committer must both be CervantesHallo <227578309+CervantesHallo@users.noreply.github.com>. Do not add AI coauthor or generated-by attribution.

## Project memory

- Read `docs/PROJECT_MEMORY.md` when taking over this project. Keep confirmed user decisions, operational lessons, evidence boundaries, and failed experiments current there; source code and the roadmap remain authoritative for implementation state. Do not store credentials or unrelated personal data.
- Development storage: check actual filesystem free space before disk-heavy work and reserve its expected peak plus 20 GiB. Retain the current working lab and necessary qualified references; after confirmed process cleanup and restoration, compact obsolete engine variants to exact component hashes/binaries and metadata, and remove regenerable stages, test prefixes and redundant backups. Keep sources, raw results, game data and saves. Consult the 2026-10-25 cleanup inventory before reusing old paths or downloading removed large packages; a manifest match alone is not byte identity.

## Layout

- `Packages/CiderKit/` — all business logic (SwiftPM, no UI). Modules: CiderCore, CiderSchema, CiderStore (engines, D3DMetal import), CiderRuntime (spawning Wine, preflight gate), CiderBottle, CiderPE, CiderIPC (authenticated command transport), CiderData (compat DB, recipes, red lines), CiderIntegration (Steam, catalog, recipe installer).
- `Tools/ciderctl/` — command client; help/version/parse errors are local, commands go through XPC to the GUI.
- `Tools/CiderCommands/` — original command implementations, invoked by the GUI with a per-request context.
- `Tools/CiderBridge/` — forwarding-only, on-demand user-session Mach service; never spawns Wine.
- `App/` — SwiftUI app (`scripts/build-app.sh` → out/Cider.app). UI design: https://Codex.ai/artifact/CX8EF8Q8pG6EXzbHWxtdJk
- `engine/` — `toolchain.sh`, `deps.sh`, `build.sh <recipe>`, `bundle-gstreamer.sh`, patches (build with a macOS 15 SDK). `data/` — compat entries, profiles, recipes (bundled into the app). `scripts/` — app/DMG build, smoke test, `wine-profile.py` (lldb profiler), Steam cold-start gate.

## Build and run

```sh
scripts/build-app.sh                     # packages GUI, client and XPC bridge
cd Tools/ciderctl && swift build          # optional development client; delegates to packaged app
.build/debug/ciderctl engine list
.build/debug/ciderctl bottle create "Name" --locale zh-Hans   # ja | zh-Hans | zh-Hant | ko | en
.build/debug/ciderctl run -b Name --wait /path/to/setup.exe /S
.build/debug/ciderctl run -b Name 'C:\Program Files (x86)\App\app.exe'
```

`CIDER_HOME=/tmp/x` redirects all data (App Support, Caches, Logs) for experiments. Unit tests need the `Testing` module, which ships with Xcode, not the Command Line Tools.

For incremental engine experiments, reuse recorded compiler arguments and redirect outputs to the private experiment directory. Avoid `make -n -B` / `--always-make`: Makefile regeneration may execute configure even during a dry run. A target-scoped `make -n -W <absolute source> <object target>` is the narrower fallback for argument extraction. Keep ownership updates outside `assert`; `NDEBUG` must not change reference counts or cleanup.

The packaged GUI is the Wine spawner (ADR-007). A standalone development client locates the checkout's `out/Cider.app`, `CIDER_APP`, or `/Applications/Cider.app`; it never falls back to spawning Wine itself. A command already submitted is not automatically replayed after disconnect. Concurrent `run --wait` must retain separate admission/execution capacity for `kill`.

`scripts/build-app.sh` also packages `Cider.app/Contents/MacOS/ciderctl`; that CLI shares the app's resource data independent of cwd. For the standalone development CLI above, run from the checkout root or set `CIDER_DATA` to the checkout's absolute `data` path. A CLI built in isolation does not bundle the compatibility data.

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
