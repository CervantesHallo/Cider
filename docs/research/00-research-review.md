# 调研总评与跨报告矛盾裁定

> 由调研阶段的完整性评审代理生成（2026-09-27）。制定计划时，矛盾以本文“较可能的事实”为准，并以各报告末尾的“事实核查记录”为最终依据。

## 总体评价

总体评价：14 份报告覆盖面很广，质量整体较高。大多数关键结论都有一手来源（源码行号、tag 日期、Apple 文档 JSON、GitHub API、本机实测），并且逐份做过独立事实核查。已经足够支撑以下几项决策：Engine R（x86_64 Wine 11 + 新 WoW64 + Rosetta）的构建、DXMT 作为 D3D10/11 主后端、D3DMetal 由用户自行导入、配方与兼容库的数据格式、签名与公证流水线、诊断与 QA 框架，以及 CJK、输入、音视频方面的落地清单。

剩余的不确定性集中在少数几个会决定路线的点上，而且这些点在不同报告之间互相矛盾：
（1）Engine A（arm64 Wine + ARM64EC + FEX）在 macOS 上到底能不能做成。涉及低 4GB 硬 pagezero、4K 页和 TSO 所需的受限 entitlement、CrossOver Preview 的 26.5 门槛和 FEX 移植范围，目前只有第三方转述，各报告的置信度标注也不一致。
（2）进程模型与代码签名身份：引擎放在 bundle 外、不签名，还是放进签名 bundle。这一点直接影响 TCC、本地网络、Game Mode 和 Metal 缓存归属，12 号报告与 07/09 号报告的结论相反。
（3）Wine 基线与 msync 在 Wine 11 上的集成方式，以及 Steam CEF 与 msync 的冲突。
（4）Chromium/CEF/WebView2 在 winemac 上的呈现问题。这是启动器和应用兼容性最大的单点，但目前的知识分散在 03、08、10 三份报告里，没有形成可以执行的方案。
（5）DX12 的开源路线和 D3DMetal 4 的架构与系统要求：都依赖实测，或依赖 9 月 29 日 XDC 2026 之后才会出现的信息。
（6）32 位和 DX9 老游戏在 macOS 新 WoW64 下的实际性能。这是 Cider 的核心价值区，但目前全部是推断。

另有几处事实层面的冲突需要统一，例如 06 号报告中的 GPTK 发布日期整体早了一年、macOS 最低版本定为 14 还是 15、winedmo 能否替代 GStreamer。

codeweavers.com 在整个调研中持续返回 403，所以 CrossOver 27 和 ARM64 Preview 的细节仍然只来自媒体和搜索摘要。开发机没有 Rosetta 和 Xcode，大量结论（Game Mode、TCC、缓存路径、hardened runtime 对 Wine 的影响）还停留在“需实测”。

建议：下一步只做下面 6 个聚焦课题，每个课题都产出明确结论或原型验证计划。完成后就可以冻结架构，进入 P0 实施。

## 跨报告矛盾与裁定

### 1. 涉及：01-crossover-product.md、14-landscape-2025-2026.md、02-crossover-wine-source-delta.md、06-cpu-translation-rosetta-arm64.md

CrossOver Mac ARM64 Preview details (custom macOS FEX, universal build, ARM64 part requires macOS 26.5+, ARM64 DXMT included). 01 and 14 mark these unverified [低], coming only from search snippets and highball #6. 02 marks them [中] and its fact-check says 'confirmed by both checkers'. 06 states the 26.5+ floor in its summary as fact and builds recommendations on it.

**较可能的事实 / 裁定：** The FEX usage is very likely: the Linux ARM64 Preview used FEX, and Gcenx/NotProton describes FEX and Rosetta builds of CrossOver Preview 2026082. The 26.5 floor, the universal packaging and the ARM64 DXMT remain unverified by any primary or independent source (AppleInsider does not mention them). Treat them as hypotheses and do not use 26.5 as a planning floor until prototyped.

### 2. 涉及：07-macos-system-integration.md、06-cpu-translation-rosetta-arm64.md、02-crossover-wine-source-delta.md

Low-4GB address space for arm64 Wine. 07 says XNU's enforce_hard_pagezero makes low 4GB unmappable for ARM64 with no entitlement exception, so there is no path for third parties. 02 relays dappermint's claim that the cross-architecture-support entitlement solves it and that CrossOver's ARM64 build carries it. 06 shows a separate entitlement-gated 4K-page spawn path and treats the entitlement as the key.

**较可能的事实 / 裁定：** Unresolved. The XNU main source fetched during this critique shows the ARM64 hard-pagezero check is conditional on an `enforce_hard_pagezero` flag, and the 4K/x86-compat path may clear it. 07's 'no path at all' is likely too strong for entitled processes, but without the entitlement the low 4GB is indeed unavailable. Needs the arm64-engine-feasibility follow-up.

### 3. 涉及：06-cpu-translation-rosetta-arm64.md、05-graphics-d3d12-vulkan-metal.md、11-prior-art-open-source.md

GPTK/Gcenx release dates. 06 says GPTK 2.1 was 2024-03-12, GPTK 3.0 was 2024-12-05 and 3.0-3 was 2025-03-03. 05 and 11 give 2.1 as 2025-03-12, 3.0 as 2025-12-05 and 3.0-3 as 2026-03-03.

**较可能的事实 / 裁定：** 05 and 11 are correct: the GitHub releases API for Gcenx/game-porting-toolkit shows 2.1 on 2025-03-12, 3.0 on 2025-12-05 and 3.0-3 on 2026-03-03, with 3.0 betas from 2025-06-12. GPTK 3 was announced at WWDC25, so 06's timeline and its fact-check row ('GPTK 3.0 于 2024-12 发布') are off by one year.

### 4. 涉及：03-wine-upstream-proton.md、14-landscape-2025-2026.md、06-cpu-translation-rosetta-arm64.md、07-macos-system-integration.md

Last macOS with Rosetta. 03 and 14 report that Apple's 2026-09-09 developer news says 'macOS 26 is the final release supporting Intel Mac computers and Rosetta', and treat the timeline as partly doubtful. 06 and 07 state that Apple's documents are consistent (macOS 27 is the last general release, macOS 28 keeps a legacy-games subset).

**较可能的事实 / 裁定：** macOS 27 is the last general-purpose Rosetta release and macOS 28 keeps a subset for older games (About Rosetta page, Developer News 2026-09-01, Support 102527 updated 2026-09-21). The 09-09 wording is most likely a slip conflating Intel-Mac support with Rosetta. Plan for autumn 2027 while tracking Apple's wording.

### 5. 涉及：07-macos-system-integration.md、06-cpu-translation-rosetta-arm64.md、13-performance-qa-compatdb.md、11-prior-art-open-source.md

Whether upgrading to macOS 27 removes Rosetta. 07 cites Apple's macOS 27 release notes (163213094: Rosetta not automatically restored after upgrade) as [高]. 06 says the claim is only second-hand (MacRumors). 13 rates it [低], citing blogs that say later builds restored the install prompt. 11 relies on a hedged Porting Kit observation.

**较可能的事实 / 裁定：** 07 is likely right that Apple's own release notes document it, since it read the DocC JSON. In any case Cider must check for Rosetta on every launch and guide reinstallation; the auto-prompt behaviour in later 27.x builds needs verification.

### 6. 涉及：12-build-packaging-distribution.md、07-macos-system-integration.md、09-multimedia-input-cjk-desktop.md

Engine packaging and signing. 12 recommends keeping Wine out of the app bundle as a separately downloaded engine that is ad-hoc signed or unsigned, without hardened runtime, with integrity protected by an Ed25519 manifest. 07 recommends signing and notarizing engines with hardened runtime and allow-unsigned-executable-memory/allow-jit. 09 recommends running all Wine processes inside a signed .app with a unique LC_UUID so that TCC and local-network privacy attribute correctly.

**较可能的事实 / 裁定：** Both concerns are real. A likely compromise: downloadable engines that are Developer ID-signed (needed anyway for Engine A's restricted entitlement), launched through signed per-app shim bundles with Cider.app as the responsible process. Must be settled by prototype (see the process-model-signing-identity gap).

### 7. 涉及：01-crossover-product.md、12-build-packaging-distribution.md、14-landscape-2025-2026.md、11-prior-art-open-source.md

Minimum macOS. 01 and 12 recommend macOS 14.0, aligned with CrossOver 27, DXMT and Wine direct syscall emulation. 14 recommends macOS 15 as the hard floor with 14 best-effort, because Rosetta AVX needs 15 and GPTK 3 needs 15. frankea/Whisky requires 15.

**较可能的事实 / 裁定：** This is a product decision rather than a factual conflict. Technically 14 works for Engine R with DXMT/wined3d, while AVX-dependent titles and D3DMetal/GPTK 3 need 15, and KosmicKrisp needs 26. A reasonable choice is floor 14 with feature gating, recommended 15+, and CI on 26/27.

### 8. 涉及：03-wine-upstream-proton.md、09-multimedia-input-cjk-desktop.md、12-build-packaging-distribution.md

Whether FFmpeg/winedmo can replace GStreamer. 03 (P1-7) and 12 suggest evaluating winedmo with the bundled FFmpeg (Wine 11.12) to avoid shipping GStreamer. 09 shows that winedmo is demux-only and links the host FFmpeg, that the bundled FFmpeg 8.1.1 compiles only libavutil, libswresample and libswscale with no libavcodec, and that modern decoders (H.264, AAC, WMA, WMV) live in winegstreamer.

**较可能的事实 / 裁定：** 09 is correct, based on source it read: GStreamer (with gst-libav, and vtdec for H.264/HEVC/VP9/AV1) must still be bundled. winedmo only helps with demuxing and would still need a host FFmpeg.

### 9. 涉及：03-wine-upstream-proton.md、07-macos-system-integration.md、08-launchers-anticheat-drm-games.md、01-crossover-product.md

Using msync by default. 03 and 07 recommend porting msync and enabling it by default for game bottles. 08 reports (Highball recipes) that msync or esync makes Steam's CEF hang, so the Steam UI must run with WINEMSYNC=0, and that wineserver's sync mode is fixed at startup, making per-process mixing impossible without restarting it. 01/03 note that CrossOver 25.1.0 fixed Steam downloads with msync.

**较可能的事实 / 裁定：** msync is valuable for games, but Cider needs either msync robust enough for CEF (as CrossOver apparently achieved) or a per-session/per-process-group sync-mode design. This belongs in the wine-baseline-and-sync follow-up.

### 10. 涉及：11-prior-art-open-source.md、02-crossover-wine-source-delta.md

Whether D3DMetal can only run on CrossOver-derived Wine. 11 states [高], citing the dappermint README, that D3DMetal only runs on CX-derived Wine because it patches CX unixcall internals. 02's fact-check calls this overstated: utmapp/d3dmetal-native (MIT) implements the GFXT host interface outside Wine, and only 22434, macdrv_functions/d3dmetal_objc.m and 22435 are confirmed D3DMetal-specific.

**较可能的事实 / 裁定：** 02 is more accurate. As shipped, D3DMetal expects a CX-style host, but the contract is replicable. Cider must port the confirmed CX hooks, or implement them against GFXT semantics, and verify the libd3dshared imports with nm/dyld_info.

### 11. 涉及：02-crossover-wine-source-delta.md、03-wine-upstream-proton.md、12-build-packaging-distribution.md、10-apps-runtimes-installers.md、08-launchers-anticheat-drm-games.md

Recommended Wine baseline. 02 recommends upstream plus the CX 26.3 diff split into topic patches, building CX 26.3 as an oracle first. 03 recommends upstream devel with biweekly rebases, upstream-first. 12 proposes parallel engine lines (wine-11.0-stable, devel, cx-26.3). 10 says a pure 11.0 base breaks modern Chromium without backporting SetThreadpoolTimerEx (only in 11.17+). 08 shows DIY CX26.3→Wine 11 builds regress Battle.net and GOG where CrossOver 26.3 does not.

**较可能的事实 / 裁定：** No report is wrong, but they are not reconciled. A probable answer is an upstream snapshot ≥11.17 (moving to 12.0 in Jan 2027) plus a curated CX patch queue, with CX 26.3 built only as a comparison oracle. This needs the wine-baseline-and-sync follow-up to confirm with regression evidence.

### 12. 涉及：01-crossover-product.md、02-crossover-wine-source-delta.md

Whether new bottles in CrossOver 26 default to 64-bit. 01 lists it as a fact ([高], from the user guide preferences). 02's fact-check says the 26.0.0 changelog does not mention it and marks it unverified [低].

**较可能的事实 / 裁定：** Probably true, since the user guide describes the 'Enable deprecated 32-bit bottles' preference, but it is immaterial for Cider, which will only create 64-bit (new WoW64) bottles.

## 已追加的补充调研（15–20）

- 15-arm64-engine-feasibility.md — ARM64 引擎（Engine A）在 macOS 上的可行性核实：低 4GB、4K 页、TSO、entitlement 与 FEX Darwin 移植范围：Rosetta is guaranteed only through macOS 27 (autumn 2027). Engine A is Cider's survival path, but the reports contradict each other: 07 says the low 4GB can never be mapped, 06 and 02 say entitlements might allow it, and 01/14 call the 26.5 floor unverified while 02/06 mark it confirmed. Every roadmap date depends on this.
- 16-process-model-signing-identity.md — Cider 进程模型与代码签名身份：引擎放置、per-app .app shim、TCC/本地网络/Game Mode/Metal 缓存归属：This decision fixes the app layout, the update pipeline and the user-facing permission UX at once, and two groups of reports recommend opposite designs. Getting it wrong means TCC/local-network breakage or notarization rework after launch.
- 17-wine-baseline-and-sync.md — Wine 基线与补丁队列定案，及 msync 在 Wine 11 inproc_sync 架构下的集成：The Wine baseline and the sync backend are the first engineering commitment and the biggest CPU-side performance gap. The reports give three incompatible recommendations, and the one empirical data point (Highball) shows that naive CX→Wine 11 forward-ports regress key launchers.
- 18-chromium-embedded-browsers-on-winemac.md — Chromium/CEF/WebView2/QtWebEngine 在 winemac 上的专项：呈现、沙箱、DComp、IME：CrossOver's changelog is dominated by 'launcher broken after update' fixes, and WebView2 is described as the largest app-compat risk. There is no consolidated technical plan yet, even though this is the most visible quality gap users will judge Cider on.
- 19-dx12-open-path-and-d3dmetal4.md — DX12 路线定案：D3DMetal 4 架构/系统要求、DXMT d3d12 成熟度、vkd3d-proton+KosmicKrisp 阻塞项：D3DMetal is the only proprietary bottleneck and exists only as x86_64. If it never ships for arm64, DX12 disappears for every free solution after Rosetta. Reports 04, 05 and 14 leave its architecture and OS requirements unverified, and the open-path blockers are identified but not sized.
- 20-legacy-32bit-dx9-perf-macos.md — 32 位/DX9/OpenGL 老游戏在 macOS 新 WoW64 下的实际表现与优化路线：Reports 03 and 04 flag severe but unmeasured risks (GL copy path, FL 9_3 caps, x87 cost, OpenGL deprecation) that affect a large part of the catalog Cider would actually serve. Planning DX9/legacy investment without data is guesswork.
