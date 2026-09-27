# Patch series

Applied in lexical order on top of the pinned Wine source (`inputs.json`). Each file starts with a
header stating what it fixes, how it was verified (game, machine, macOS, engine id, date), and the
upstream status (link to the merge request or issue). A patch that upstream ships is deleted here in
the same change that bumps the pinned source.

Current series:

- `0001-use-the-real-user-name.patch`: CrossOver Hack 12735 names the Windows user "crossover" and hardcodes `C:\users\crossover`; Highball bottles carry `C:\users\<macOS user>`, and a bottle moved to this engine would get a second profile directory. Restores upstream Wine's behaviour in advapi32 and shell32. Verified 2026-09-04 (M1 Pro, macOS 26.6.2).

- `0002-wined3d-auto-renderer-opengl-first.patch`: CrossOver hack 18311 tries the Vulkan wined3d adapter first on macOS; with MoltenVK present it is created and then fails Direct3D 11 feature checks (Warframe's launcher, Rockstar's launcher). Upstream's OpenGL-first choice, which Sikarugir's engine shows in practice, restored; renderer=vulkan stays selectable.
- `0003-ntdll-winedllpath-prepend.patch`: WINEDLLPATH_PREPEND support (MacPorts GPTK patch 1005), without which none of Highball's renderer overlays applied on this engine.

- `0008-macos-mirror-teb-fiberdata.patch`: on macOS the %gs base is the host thread block, so signal_init_process() mirrors Tib.Self, ThreadLocalStoragePointer and Peb into it but not Tib.FiberData. GetCurrentFiber() is an intrinsic reading %gs:0x20 directly, so a fiber-based job system reads a host value (measured: a constant 0x8ff on every thread) and hands it back to SwitchToFiber, which faults. Mirrors the field at thread start and in the three places kernelbase changes it. Measured on M1 Pro, macOS 26.6.2, engine x64-crossover26.3-r4, 2026-09-09; reproducers need no game. NOT yet verified to make Marvel's Guardians of the Galaxy run, and does not explain why CodeWeavers' own testers report that game working on M1 from this same tree. Upstream: not yet reported.
- `0010-mfreadwrite-video-processor-sample-allocator.patch`: Proton 164af86d; the source reader lets the video processor's allocator create sample textures, so they carry the shared flags a DXGI device manager asks for and DXMT can hand a game (Unity, Unreal media players) a real shared handle instead of null (The Last Flame's menu video, highball#99; 3Shain/dxmt#135).

- `0011-macos-lasterror-in-gs-slot.patch`: %gs:0x68 is where MSVC-built code (Unity's Mono, 130 sites) reads GetLastError(); on macOS that is libc's TSD slot 13, so it is swapped at the 0009 crossings and kept in step with TEB->LastErrorValue by RtlSetLastWin32Error, and the readers use it. File.Delete on a missing save no longer throws "Unknown error (0x80f13910)" (The Last Flame, highball#99).

- `0012-ntdll-x87sidecar-cooperative-attach.patch`: athei's a4e40c6. Two things. get_alternate_wineloader() now checks the loader it names with access(X_OK): our builds are wow64-only, the i386-unix loader does not exist, and without the check env.c's build_initial_params() took the phantom path as a reason to relaunch every 32-bit program through `start.exe /exec`, so each ran as a child of a detached start.exe (measured 2026-09-24: two processes without WINEARCH, one with). And with ROSETTA_X87_PATH set to athei's x87sidecar, an i386 program re-execs through it in cooperative mode, no entitlement needed: fsin/fsqrt/fdiv loop 1554 ms to 20 ms, Half-Life 2 timedemo 131 to 174 fps on wined3d (M1 Pro, macOS 27.0). Off unless the variable is set. Upstream: CrossOver-only hack by athei (github.com/athei/wine), not for WineHQ.
- `0013-ntdll-nx-compat-from-main-image-permanent-under-rosetta.patch`: athei's 539aa62. Wine turned data execution prevention off for the whole process as soon as any loaded module lacked IMAGE_DLLCHARACTERISTICS_NX_COMPAT, which sets force_exec_prot and adds PROT_EXEC to every readable mapping; under Rosetta a writable and executable page costs a Mach round trip on first touch, so an old game without the flag turns every allocation into a fault storm (athei/wine-build#3 measured a 190x frame time difference from one LoadLibrary). Now only the main executable's flag counts, as on Windows, and under Rosetta DEP is permanently on (ProcessExecuteFlags reports MEM_EXECUTE_OPTION_PERMANENT, SetProcessDEPPolicy off fails with STATUS_ACCESS_DENIED). WINE_DISABLE_NX_COMPAT=0 restores the old behaviour for a program that really executes from data pages. Suggested by athei on highball#165 (2026-09-25); dry-run applies on the pinned source, none of 0001-0012 touch loader.c or unix/process.c. Trial build 2026-09-25 (run 36125101280): applies and builds; a 32-bit program without the flag reads ProcessExecuteFlags 0x9 on it against 0x2 on r11. No cost found to remove on M1 Pro / macOS 27.0: fresh-page touch, LoadLibrary/FreeLibrary, VirtualProtect churn and VirtualAlloc churn time the same with no-exec on and off, on r11 and on the trial. Waiting on athei's measurement setup (highball#165) before it ships. Upstream: athei's tree, not WineHQ.
Candidates, in order, from the 2026-09 investigation:

1. Rockstar Games Launcher installer: the service start that the CrossOver tree completes and the
   Sikarugir tree parks on (see Highball's tracking notes and Sikarugir-App/Sikarugir#258). Needed
   only if the diff between the two trees is not already in this base.
2. CS:GO Legacy: a wait-completion boost emulation for Source's thread pool hand-off (lost wake-up
   race measured 2026-09-02). Experimental.

## 0004-foreign-client-surface-overlay.patch

winemac.drv: a client surface for a window another process owns draws into a borderless,
mouse-transparent overlay window of ours, framed on the foreign window's client rect and kept
above it on every present. This is the driver half of cross-process presenting for CEF-based
launchers (Steam's browser, Rockstar, Ubisoft) under DXMT; the DXMT half is the
DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN opt-in in github.com/gauthierpiarrette/dxmt. Unverified until
build 12 and the Steam-on-r2 test.
