# 嵌入式 Chromium（CEF / WebView2 / QtWebEngine）在 winemac 上的专项：呈现、沙箱、DComp、IME

> 调研日期 2026-09-26（部分核对在 2026-09-27 凌晨完成）。
>
> **置信度标注**
> - **[高]**：本次直接读过一手来源，包括源码文件、提交元数据、bug 页面、MR API 和官方文档。
> - **[中]**：来自可信二手来源，或只核实了一部分。
> - **[低]**：推断，或无法核实。
>
> **来源说明**：凡写"CX 26.3 源码"的地方，读的都是第三方镜像 `dappermint/winecx` 的 `crossover-26.3.0` 分支。这个镜像与官方 tarball 是否逐文件一致，本文**未核实**。
>
> 本文汇总 03、08、09、10 号报告中与嵌入式 Chromium 相关的零散结论，重新核对后整理成行动方案。
>
> **修订说明**：本文已按 2026-09-27 的独立事实核查结果修订。正文中标"核查更正"的段落就是改动过的地方，逐条结论见文末"事实核查记录"。

## 摘要

- **CX 26.3 的二进制补丁表，本质是在弥补 CrossOver 自己的 %gs 模型** [高]
  - x86_64 下，`apply_binary_patches()` 把 libcef 72.0.3626.96/.121、85.3.9.0、85.3.11 和 Qt5WebEngineCore 5.15.2.0 中的 `mov rax, gs:[0x8]`（读 `NT_TIB.StackBase`）改写成两步：先 `gs:[0x30]` 取 TEB，再 `[rax+8]` [1]。
  - 原因：CX 26.3 在 macOS 上**没有**让 GSBASE 指向 TEB。它只把 `Tib.Self`、`ThreadLocalStoragePointer`、`Peb` 三个字段镜像进 macOS 的 pthread TSD 槽位 [2]。
  - 上游从 **Wine 10.5 起**（!6866，tag 2025-04-04）已经用 `_thread_set_tsd_base` 在进出 PE 代码时切换 GSBASE，按理这整类补丁都不再需要 [5][6][7]。
  - **建议**：Cider 采用上游的 swap 模型，不移植 %gs 字节补丁。
- **同一张表里还有另一类补丁：把 `cef_settings_t.command_line_args_disabled` 强制清零** [高]
  - 覆盖 x64 libcef 90.6.7（Epic，CW HACK 23854），以及 32 位 111.2.7 / 135.0.20（Ubisoft，19252/25737）[1]。
  - 目的是让 Chromium 开关能从命令行注入。
  - 补丁里的偏移（0x68 / 0x3c / 0x38）与 CEF 各分支头文件中的结构体布局完全吻合 [90]。
  - **核查更正**：`chrome_runtime` 字段在 CEF 6478（M126）中仍在，6723（M130）/7049（M135）已无；本次补查确认 6533（M127）中它被 `#if !defined(DISABLE_ALLOY_BOOTSTRAP)` 包着，6613（M128）已移除 [90][91]。原文"在 111 到 135 之间移除"的范围过宽。
  - Cider 可以在 `cef_initialize` 处统一改写，不必逐版本打字节补丁。但布局要按 libcef 版本加 `settings->size` 一起判断，不能只看 size（见 §1.2、§5）。
- **跨进程"子窗口" Metal swapchain 仍是全链路最大的缺口** [高]
  - 上游 1a63b0d7c431 只实现了跨进程**顶层**窗口：!10935，作者日期 2026-05-20，合入 2026-06-01，随 wine-11.11 发布（2026-06-12）[13][14][15]。
  - master 对子窗口仍报 `FIXME("Cross-process child window Metal swapchains are not implemented")`，随后在 vulkan.c 返回 `VK_ERROR_INCOMPATIBLE_DRIVER` [16]。
  - Bug 60263 于 2026-08-31 提交，状态 UNCONFIRMED，附带补丁 82030（针对 11.16，+513/−27）[18]。
  - dappermint（d81a288，+202/−22）和 Highball（0007，回移植到 11.0）各有一套实现 [20][24]。
  - dappermint 的实现存在"不 flush CATransaction"和"像素当点用"两个缺陷，补丁 3 行 [19]。
- **CX 26.3 公开源码里完全没有 CAContext / CALayerHost 代码** [高]
  - winemac 中找不到这两个类，因为它基于 Wine 11.0，早于 11.11 [1]。
  - CX 在产品层如何让 Steam 的 CEF 画出来，公开源码解释不了。可能落在闭源的 `cxcompatdb.so`（CW Hack 24067）或未公开的 DXMT 改动里 [3][23]。[低]
- **上游 DXMT 直接拒绝跨进程 swapchain**（代码事实 [高]；它是否真的挡住 Chromium [中]，待验证）
  - `CreateSwapChain` 检测到 HWND 属于别的进程就返回 `E_FAIL` [26]。
  - Highball 的 fork 加了 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` 作为 opt-in [27][84]。
  - **核查更正**：标准 Chromium 路径下，呈现用的子 HWND 由 GPU 进程自己创建并持有。浏览器进程先校验"子窗口属于 GPU 进程"，然后才 `SetParent` [85][86]。所以 DXMT 的 PID 比较多半相等，这项检查**未必**是阻塞点；真正缺的是 winemac 的每进程 NSView 模型（C1）。
  - 反证：Highball 的 Battle.net recipe 说这个开关能消除 Wine 11 引擎下的崩溃 [37]。两边冲突，C4 降为"先用日志验证"。
  - 上游 winemac 没有 CX 那套 `macdrv_functions`。可替代的 ExtEscape 接口 !11058 自 2026-06-03 起一直是 open [31]。
- **WebView2"只画第一帧"的起因，与原报告的归因不同** [中]
  - #5720 把问题归到 runtime 自己追加的 `--use-gl=swiftshader` [52]。
  - 但宿主应用 IAGD 在 PR #294（2026-07-28 合入）中，检测到 Wine 就会传 `--no-sandbox --disable-gpu --disable-gpu-compositing --single-process` [54]。
  - Chromium 在 `--single-process`/`--in-process-gpu` 下会**就地改写本进程命令行**，追加 GPU 相关开关 [55][56]。
  - 所以更可能的触发链是"应用在 Wine 下自带的开关 + 单进程 GPU + 软件 GL 值失效"。
  - 结论：Cider 的 hook 必须能**删除**参数，而且要在子进程启动时生效。
- **macOS 上没有任何已验证可用的 WebView2 Fixed Version** [高]
  - Linux 上，151.0.4129.78 配合 win7 版本号（可再加 staging dcomp）有成功报告 [58][59]。
  - macOS 上唯一的数据点是 M4、staging 11.14、runtime 150 的组合：Wine 自己的 d2d1 在 wined3d 上调用 `CreateDeviceContextState(FL10.0)` 被拒绝，渲染失败 [58]。
- **DComp**：staging 的 `dcomp-DCompositionCreateDevice2` 在 master 上已有 67 个补丁加 definition [60]。
  - 合成方式是 D2D 渲染后 blit 到 `GetDCEx`；shared visual 只支持同进程。
  - 仍无法覆盖 `UpdateLayeredWindow` 型嵌入面板 [58]。
  - 上游 dcomp 仍是 stub [62]。
  - **不作为 Cider 默认**，默认走 win7 版本号绕开 DComp。
- **沙箱在 macOS/Rosetta 上"能跑"但不提供隔离** [中]
  - 上游 x86_64 syscall thunk 的字节形态与 Windows 一致，注释明确写着是为了 Chromium，macOS 共用这套 thunk [41]。
  - 但 Wine 不按 token 约束 Unix 文件访问，"开沙箱"几乎没有安全收益。
  - `SetThreadpoolTimerEx` 在 wine-11.17（2026-09-04）才实现 [44][45]；CX 26.3 里仍是注释掉的 stub，调用即 abort [46]。
  - 未找到任何"Chromium 沙箱在 macOS + Rosetta + Wine ≥ 11.17 上验证通过"的一手记录。
- **IME** [高/中]
  - Chromium **M122 起**在 Windows 上只构造 `InputMethodWinTSF`（headless 和测试分支除外）[67][88]。
  - 它的 `OnUntranslatedIMEMessage` 只处理 `WM_IME_REQUEST` 和 `WM_CHAR`/`WM_SYSCHAR` [68]。
  - Wine 的 IME 走的是 imm32 路线，所以在 M122+ 的窗口模式 CEF 里，上屏大概率可用（经 `WM_IME_CHAR`→`WM_CHAR` 回落），但预编辑和候选窗位置会偏 [推断]。
  - **核查更正**：M121 及以前仍保留 `InputMethodWinImm32`，可用 `--disable-features=TSFImeSupport` 选中；M110 及以前在 Windows 版本 ≤ Win7 时还会自动选 IMM32 [88][89]。
    - 所以 Epic（CEF 90）、Ubisoft 旧版（111）、Rockstar（85）、BeamNG（72）都能用 argv 或 winver 规则切到 IMM32 路径。
    - Steam（CEF 126）和 Ubisoft 新版（135）只有 TSF 路径。
    - OSR（离屏渲染）宿主自己处理 IME，不适用上述分析。
  - 上游相关 MR !12164、!12074、!12077 均在 2026-09 下旬提交，仍为 open [70][71]。
- **Steam 策略**：默认"bottle 内 Windows Steam"（P0）；原生 Steam 桥（macos-steam / NotProton）作为 P2 可选集成。
  - 理由：EA、Ubisoft、Battle.net、Epic、Rockstar、GOG 都必须在 Wine 里跑 CEF 或 QtWebEngine。桥只能省掉 Steam 这一个，省不掉整条技术线。

## 详细调研

### 1. CX 26.3 的 `apply_binary_patches()` / `apply_fuzzy_binary_patches()` 到底改了什么

#### 1.1 挂载点 [高]

`dlls/ntdll/loader.c` 的 `build_module()` 在 import 修正完成、`LoadCount` 置位之前，依次执行三件事 [1]：
1. **CW HACK 22434**：对每个非 builtin PE 模块做 unixcall `unix_pe_module_loaded`。Unix 侧会 dlopen `$CX_APPLEGPTK_LIBD3DSHARED_PATH`，调用 `register_non_native_code_region(start,end)`，只在 Sonoma 及以上生效 [3]。这是给 D3DMetal/Rosetta 用的，与 Chromium 无关，但会作用于每一个 PE 模块。
2. 若 `BaseDllName` 恰为 `libcef.dll` 或 `Qt5WebEngineCore.dll`，调用 `apply_binary_patches()`（i386 或 x86_64）。
3. 若为 `cohtml_Unity3DPlugin.dll`，调用 `apply_fuzzy_binary_patches()`（仅 x86_64）。

`apply_binary_patches()` 的匹配方式：按"固定 RVA + `memcmp` 原字节"匹配，命中后 `NtProtectVirtualMemory(PAGE_EXECUTE_READWRITE)`，`memcpy`，再恢复保护。它带一个 `stop_patching_after_success` 标志，用来表示"同一版本的一组补丁打完即停"。

`apply_fuzzy_binary_patches()` 则在整个映像里线性扫描带通配字节（0xff）的模式。

#### 1.2 完整补丁表 [高]

| CW HACK | 模块 / 版本（注释所述用途） | 架构 | RVA | 改写内容 |
|---|---|---|---|---|
| 18582 | libcef 85.3.9.0（Rockstar Social Club/Launcher） | x64 | 0x28c4b30 | `mov rax,gs:[0x8]; ret; int3×6; …; mov rax,gs:[0x8]` → `mov rax,gs:[0x30]; mov rax,[rax+8]; ret` 利用 int3 填充区，第二处改成 `call` 回这个函数 |
| 22584 | libcef 85.3.11（Rockstar 更新版） | x64 | 0x28c5190 / 0x28c521a | 第一处同 85.3.9；第二处把 `mov rsi,gs:[0x8]; test rsi,rsi; jz` 改成 `mov rsi,gs:[0x30]; mov rsi,[rsi+8]; nop` |
| 19114 | libcef 72.0.3626.121（BeamNG.drive） | x64 | 0x23bb2ad / 0x23bb329 / 0x23bb369 | 在 `IMMEDIATE_CRASH` 的 `int3; ud2; push 0x1c; ud2…` 死代码区**新写一个"取 StackBase"小函数**，两处 gs:[0x8] 改成 `call` 它 |
| 16900 | libcef 72.0.3626.96（Wizard101） | x64 | 0x23bb82d / 0x23bb8a9 / 0x23bb8e9 | 同 19114，偏移不同 |
| 21548 | Qt5WebEngineCore 5.15.2.0（EA Launcher，注释称"基于 CEF 83.0.4103.122"；核查更正：QtWebEngine 不是 CEF，实为 Chromium 83） | x64 | 0x2810f10 / 0x2810f8d | 同类 %gs 改写；第二处位于 `call [VirtualQuery]` 之后 |
| 23854 | libcef 90.6.7（Epic，数组名 `epic_cmd_line_args_90_6_7`） | x64 | 0x3807 | `mov eax,[rdi+0x68]; mov [rbx+0x68],eax` → `xor eax,eax; nop; mov [rbx+0x68],eax`，即忽略 `command_line_args_disabled`。清零的是 `cef_initialize` 内部拷贝的那份 `cef_settings_t`，不是调用者的结构体 |
| 19252 | 32 位 libcef 111.2.7（Ubisoft Connect） | x86 | 0x114a43 | 同上，字段偏移 0x3c |
| 25737 | 32 位 libcef 135.0.20（更新后的 Ubisoft Connect） | x86 | 0x11e92d | 同上，字段偏移 0x38 |
| 22901 | cohtml_Unity3DPlugin.dll（Cities: Skylines II） | x64 | 模糊匹配 | 模式 `int3; push r; sub; cmp; mov; jz` → 只把 `0f 84`（jz）改成 `0f 85`（jnz） |

**为什么要改 %gs** [高]
- CX 26.3 的 `signal_x86_64.c` 在 macOS 上只做这几件事 [2]：
  - 用 `movq …,%gs:0x30/0x58/0x60` 写 `Tib.Self`、`ThreadLocalStoragePointer`、`Peb`；
  - 其余 TEB 字段不做镜像；
  - syscall dispatcher 里也写成 `movq %gs:0x30,%rcx`。
- 因此 Chromium/V8 中 MSVC 内联生成的 `__readgsqword(0x08)`（`NT_TIB64.StackBase`）读到的是 macOS 的 TSD 槽位 1，不是栈底。
- 补丁在补丁点改成先经 `gs:0x30` 取 TEB 再解引用。具体是哪个 Chromium 函数（V8 栈边界或 `base::debug` 一类），本文**未核实**。

**同一问题还在别处冒头** [高]
- Highball 的引擎（同样基于 CX 26.3 源码）在 2026-09 又加了几个补丁 [9]：
  - 0008/0009 镜像 `FiberData`；
  - 0011 让 `%gs:0x68`（`LastErrorValue`）与 TEB 同步。
- 0011 的实测：Unity 的 `mono-2.0-bdwgc.dll` 有 130 处直接读 `%gs:0x68`。不同步时 `GetLastError()` 得到的是一个堆指针。
- 换句话说，CX 式"只镜像部分槽位"的方案需要不断补洞。

**上游的做法** [高]
- Wine 10.5 的 ANNOUNCE 头条写着 "%GS register swapping on macOS"。对应提交是 "On macOS x86_64, swap GSBASE between the TEB and macOS TSD when entering/leaving PE code"（Brendan Shanks，!6866）[6][7]。
  - 时间线：!6866 于 2024-11-21 开启，2025-04-02 合入；wine-10.5 的 tag 提交日期是 2025-04-04 [7][80]。
- master 的做法 [5]：
  - `init_handler()` 调用 `_thread_set_tsd_base(pthread_teb)`；syscall/unixcall dispatcher 直接执行 `movl $0x3000003,%eax; syscall`；
  - `leave_handler()` 在不处于信号栈、也不在 syscall 中时，调用 `_thread_set_tsd_base(data->teb)`；
  - 效果：进入 Unix 侧时切回 pthread TSD，返回 PE 时切到 TEB。于是原生代码读 `%gs:0x8`、`%gs:0x68` 都能拿到真实 TEB 字段 [推断，由设计得出，未实测]。
- CX 26.3 中搜不到 `tsd_base`。也就是说，**CodeWeavers 在 CX 26.3（Wine 11.0 基线）里没有用上游从 10.5 起的 swap 实现**，原因未公开 [2]。
- 可能的原因有两个 [低]：
  - D3DMetal 的兼容性：MetalSharp #608 专门"做了一个带开关的 ntdll.so：DXMT 走 swap，D3DMetal 保留 legacy 行为"[10]；
  - Rosetta 下每次切换多一次 syscall 的开销。
- 反例：dappermint 的 `wine1117` 分支采用了上游 swap（`_thread_set_tsd_base` 在第 800/833 行）[8]，其用户用 D3DMetal 4.0b2 也能运行 Steam [19]。**两方说法冲突，需要自测。**

**`command_line_args_disabled` 的偏移可以交叉验证** [高]

（核查更正：原文只引 CEF master 的字段顺序 [11]，但 master 已经没有 `chrome_runtime`；下面改用各分支头文件。）

- CEF 分支 4430（M90）、5563（M111）、6478（M126）中，`cef_settings_t` 的字段顺序为 `size, no_sandbox, browser_subprocess_path, framework_dir_path, main_bundle_path, chrome_runtime, multi_threaded_message_loop, external_message_pump, windowless_rendering_enabled, command_line_args_disabled` [90]。
- 分支 6723（M130）、7049（M135）和 master 去掉了 `chrome_runtime`，其余顺序不变 [11][90]。

据此计算：
- x64、带 `chrome_runtime` 字段：8 + 4（加 4 字节对齐）+ 3×24 + 4×4 = **0x68**；
- x64、不带 `chrome_runtime`：**0x64**；
- x86、带 `chrome_runtime`：4 + 4 + 3×12 + 4×4 = **0x3c**；
- x86、不带 `chrome_runtime`：**0x38**。

补丁表中的三个值（0x68、0x3c、0x38）都对得上。CX 135.0.20 的补丁先读 `[edi+0x34]` 再读 `[edi+0x38]`，与无 `chrome_runtime` 的 x86 布局一致 [1]。

`chrome_runtime` 被移除的时间 [高]：
- 核查者确认它在 6478（M126）仍在，6723（M130）已无 [90]；
- 本次补查进一步收窄：6533（M127）中它被 `#if !defined(DISABLE_ALLOY_BOOTSTRAP)` 包着，布局取决于构建配置；6613（M128）已移除 [91]。
- 原文"在 CEF 111 到 135 之间移除"的推断方向对，但范围过宽。

对 C9 shim 的影响：同一 `size` 值在不同版本里可能对应不同布局，其他字段也在各版本间增删。所以选布局要以 libcef 版本为主（PE VERSIONINFO 或导出的版本查询函数），`settings->size` 只作为校验 [推断]。

**cohtml 补丁** [低]
- 这是 Coherent Gameface 的 HTML UI，不是 Chromium。补丁只翻转了一个条件分支，源码没有注释说明检查的是什么。
- macgameport/cities-skylines-2-macos 在 Wine 11 上不再需要"某个签名/授权相关的绕过补丁"[12]。两者是否是同一件事，未能确认。

**CX 里其他与 CEF 相关、但不在这张表中的改动** [高]
- `WINE_SIMULATE_WRITECOPY`（CW Hack 22996，只由环境变量开启）[3][4]
- Ubisoft 的 `vk_swiftshader_icd.json` 自动生成（19252）[47]
- 把 `EpicGamesLauncher.exe` 挪到 `winsta0\Default`（24938）[47]
- 闭源 `cxcompatdb.so`（24067），会在每个进程中被 dlopen [3]
- 旧版"给 steamwebhelper 追加 `--no-sandbox`"的 hack [51]，在 26.3 的 kernelbase 中已**不存在** [47]

### 2. 跨进程子窗口 Metal swapchain：现状与各方实现

#### 2.1 为什么 macOS 特别难 [中]

Chromium 的 GPU 进程会把 ANGLE→D3D11 的 swapchain 画到一个子 HWND 上，而这个子窗口的根窗口属于浏览器进程。

**核查更正** [高/中]：这个子 HWND 本身是 **GPU 进程**创建和持有的，不属于浏览器进程。
- `ui/gl/child_window_win.cc` 在 GPU 进程的专用线程里，先建一个隐藏的 popup，再在它下面 `CreateWindowEx(WS_CHILDWINDOW|WS_DISABLED|WS_VISIBLE …)` [85]。
- 浏览器侧的 `RenderingWindowManager::RegisterChild()` 用 `GetWindowThreadProcessId` 校验子窗口属于 GPU 进程（不符就打 "Child HWND not owned by GPU process." 并返回），然后才 `::SetParent(child, parent)` [86]。
- Wine 里跨进程 `SetParent` 不改变窗口的所属线程。所以 ANGLE 在 GPU 进程里对这个 HWND 建 swapchain 时，所属 PID 就是当前进程 [推断，中]。
- 结论：难点在于子窗口的根 NSWindow 在另一个进程里，而不是"swapchain 的 HWND 属于别的进程"。

- **X11**：窗口是服务端对象，跨进程绘制是自然支持的。
- **winemac**：NSView 和 `macdrv_win_data` 是**每进程私有**的，`get_win_data()` 看不到别的进程的 HWND [18]。

唯一可行的系统机制是私有 API `CAContext`/`CALayerHost`：渲染进程把 layer 导出为 contextId，宿主进程用 CALayerHost 引用它。Chromium 自己在 macOS 上从 2014 年起就用这套 API（`ui/base/cocoa/remote_layer_api.h`）[35]。所以它虽然是私有 API，但 Apple 悄然移除它的风险相对可控。

#### 2.2 各实现对比

| 实现 | 基线 / 日期 | 能力 | 状态 |
|---|---|---|---|
| 上游 1a63b0d7c431（!10935，Marc-Aurel Zent）[13][14][15][17][81] | author 2026-05-20，merge 2026-06-01，wine-11.11（2026-06-12；GitLab API 复核，11.11 是最早包含该提交的 tag） | 跨进程**顶层**窗口：`CAContextSwapChain` 创建时用 `cgrect_mac_from_win()` 定尺寸；**之后不再跟踪尺寸**；只有 vulkan.c 会走到这条路径 | 已合入 |
| 上游 master，子窗口分支 [16][82] | 2026-09 | `macdrv_client_surface_acquire_metal_swapchain()` 中 `get_win_data()` 失败且 `NtUserGetAncestor(hwnd,GA_ROOT)!=hwnd` → FIXME 并返回 false → vulkan.c 返回 `VK_ERROR_INCOMPATIBLE_DRIVER` | **未实现** |
| Bug 60263 附件 82030 [18][83] | Wine 11.16，2026-09-03（取代附件 82021；补丁头声明由 AI 编写，从未提交为 PR/MR；按标题/关键字搜索 GitLab MR 也没有子窗口方向的 MR） | 接到已有的 CAContext 通路；在入口处做像素→点换算；按 Win32 paint order 叠放；区分空矩形与"无矩形"；新层创建时退役旧层；不扩展 `macdrv_functions_t`（因为它有 `C_ASSERT` 尺寸检查），改为导出独立符号 | UNCONFIRMED，作者声明是"参考实现"，未提 MR |
| dappermint d81a288 + e590b06 [20][21] | 2026-08-13 / 2026-09-21 | container layer 包住 CAMetalLayer；`remote_layers` 数组映射 contextId→子 HWND；在 WindowPosChanged 时做 DFS，重新推导可见性和 z-order；"每个子窗口只显示最新的 hosted layer" | fork 内 |
| dappermint 的缺陷与修复（issue #11）[19] | 2026-09-19，Steam CEF Chrome/126.0.6478.183 | ① 呈现线程没有 runloop，隐式 CATransaction 永远不提交，菜单停在 2×1 px 显示为空白；② 把 Win32 像素当作 CALayer 的点，Retina 下 UI 放大 2 倍、点击错位。修复是 3 行：`cgrect_mac_from_win` 加 `[CATransaction flush]` | 未合入，fork 维护者无回应 |
| Highball 0007 [24] | 回移植到 CX 26.3（11.0） | 11.11 的 CAContext 通路，加上 60263 的几何、z-order、0×0 和生命周期处理，接进 `macdrv_functions` shim，使 D3DMetal/DXMT"无需改动"；新增 `WM_MACDRV_{CREATE,RELEASE,UPDATE}_REMOTE_LAYER` | 自述"只做了编译与链接检查，未实际运行"；取代了此前的 0004 overlay 窗口方案 |
| adurham/winecx [22] | 2026-09-22～24 | VRR：`0188ae9` 用载体 `CAMetalDisplayLink` 驱动外接 VRR 面板；`52e3c7b`/`474c902` 做帧率 broker（离屏 D3DMetal surface，display link 挂在呈现 layer 上）；`d42b09e` 新增 `WHISKY_EXTERNAL_MODE_CONTROL` | fork 内，与子窗口托管共用 CAContext 基础设施 |
| CX 26.3 [1] | Wine 11.0 | winemac 中**没有** CAContext/CALayerHost；只有 `WineMetalLayer`（CW HACK 22435，D3DMetal client surface） | 如何支撑 Steam，公开源码无法解释 |

**相关回归：flush 时隐藏 client_view** [高]
- 上游 1a1d1f3f3820（Zhiyi Zhang，作者日期 2026-08-04，2026-08-11 合入）让 window surface flush 时隐藏 client_view [32][34][92]。
- 后果：同时用 GDI 绘制又有活跃 swapchain 的窗口会反复闪白，EA App 和 Steam 都受影响（bug 60282，11.16，UNCONFIRMED）。
- Zhiyi Zhang 本人评论说，彻底修好需要"实现合成"或重做 GL/GDI 呈现。
- 该改动截至 2026-09-27 **仍在 master 的 `surface.c` 里**（第 122–133 行，经事实核查复核）。dappermint 的 f77c272 用 +6/−13 行删掉了这段隐藏逻辑 [33]。

#### 2.3 DXMT 这一侧 [高]

- **上游 DXMT 的限制**：`D3D11SwapChain` 创建时，`GetWindowThreadProcessId(hWnd)` 若不等于当前进程，直接报 `"cross-process swapchain not supported yet"` 并返回 `E_FAIL` [26]。
- **Highball 的 fork**（main 与 highball 分支相同）：设了 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` 就放行。注释说窗口的 Cocoa view 在创建窗口的那个进程里，只有在能为外部 HWND 提供 surface 的驱动上才该打开，点名的是"Highball's winemac.drv overlay window"[27][84]。Highball 的 recipe 给 Battle.net、EA、Ubisoft 开启此项 [37][38][39]。
- **核查更正：这项检查是否真的挡住 Chromium，尚未确定** [中]
  - 按 Chromium 源码，标准路径下 swapchain 的子 HWND 属于 GPU 进程（§2.1）[85][86]，PID 比较应当相等，检查不会触发。
  - 但 Highball 的 Battle.net recipe 写着"GPU 进程画进客户端的窗口"，并称这个开关能消除 Wine 11 引擎下的崩溃（登录窗仍是黑的）[37]。
  - 可能的解释包括：另一条 HWND 路径、`--in-process-gpu`，或 recipe 的因果判断本身有误。Highball 为什么需要这个开关，没有文档说明。
  - 要先抓 `+dxgi` 和 DXMT 日志，记录传给 `CreateSwapChain` 的 HWND 所属 PID 与当前 PID，再决定 C4 的优先级。
- **DXMT 取 view 的方式**：winemetal 的 Unix 侧优先 `dlsym("macdrv_functions")`（CX 的 ABI），从中取 `get_win_data()->client_cocoa_view`。回退路径只 dlsym 了创建 view 的函数，**拿不到 `get_win_data`** [28]。
- **上游缺接口**：上游 winemac 既不导出 `macdrv_functions`，也没有 Metal 专用的 ExtEscape；后者 !11058 仍是 open [31]。所以"上游 Wine + 上游 DXMT"的组合，在 winemac 这一侧本身就缺一个稳定接口。
- **API 缺口**：上游 DXMT 的 `SwapDeviceContextState` 仍是 `UNIMPLEMENTED` [30]，而 Wine 的 d2d1 和 staging dcomp 都依赖它。Highball 的 DXMT 发布说明写着 "SwapDeviceContextState implemented" [27]。
- **驱动类型**：DXMT 的 `D3D11CreateDevice` 在 adapter 为空时忽略 DriverType（只打 WARN），WARP 会被当作硬件处理 [29]。

### 3. Chromium 沙箱：macOS/Rosetta + Wine ≥ 11.17 是否真的可用

**已证实的部分**

1. **syscall thunk 形态满足 Chromium 的拦截要求** [高]
   - `include/wine/asm.h` 的 x86_64 thunk 字节序列是 `4c 8b d1 / b8 id / f6 04 25 08 03 fe 7f 01 / 75 03 / 0f 05 / c3 / eb 01 / c3 / ff 14 25 00 10 fe 7f / c3`。
   - 注释原文："Chromium depends on syscall thunks having the same form as on Windows" [41]。
   - PE 侧 ntdll 在 macOS 与 Linux 上共用这套 thunk，所以 sandbox 的 service-call interception 在 macOS 上**结构上可行**。
2. **`SetThreadpoolTimerEx`** [高]
   - wine-11.17（tag 2026-09-04）中已由 8fc5b439ff6a 实现（Nikolay Sivov：`kernel32: Add SetThreadpoolTimerEx() implementation.`）；bug 57980 于 2026-09-18 以 "fixed in 11.18" 关闭 [43][44][45]。
   - bug 中的说法是 2025 年年中 Chromium 经 nearby-connections 引入了这个硬依赖 [43]。
   - CX 26.3 的 `kernelbase.spec` 仍是 `# @ stub SetThreadpoolTimerEx`，kernel32 没有导出 [46]。Wine 对缺失导出的处理是"调用时才 abort"，所以影响**只在走到这条代码路径时出现**：Adobe CC 会崩，WebView2 基本页面未必会。
   - 注意：它与沙箱**无关**，属于一般性 API 缺口。10 号报告把它放在"沙箱"语境里，本文更正这一点。
   - 事实核查补充：
     - 提交作者日期和提交日期都是 2026-09-04，所以代码随 11.17 发布 [87]。
     - bug 57980 在 2026-09-04 19:26Z（11.17 打 tag 之后）标记 RESOLVED FIXED，所以出现在 11.18 的修复清单里，2026-09-18 由 Julliard 批量关闭。
     - 报告者日志里的 "unimplemented function KERNEL32.dll.SetThreadpoolTimerEx, aborting"，证实了"调用即 abort"。
3. **bug 56378（Edge/WebView2 必须加 `--no-sandbox`）** [高]：列入 11.1 修复清单，但没有关联提交，只是报告者复测后标记为 resolved，而且**只在 Linux 上复测过** [42]。

**推断的部分** [中]
- Wine 的 NT 安全模型在文件访问上不受 token 约束：ntdll 按 Unix 权限打开文件。restricted token、integrity level、lowbox/AppContainer 最多在 wineserver 对象层面产生一些效果。
- 所以 Chromium 沙箱在 Wine 里即使"不崩"，**也不提供实质隔离**。
- Cider 需要的隔离应来自 macOS 层面（按 bottle 分用户或容器、App Sandbox），而不是 Chromium 沙箱。
- 由此得出的策略：**对已知启动器默认关闭 Chromium 沙箱**。这样做安全上几乎没有损失，同时少了一整类失败模式。

**各方实际强制的配置**

- **CrossOver 26.3（公开源码）**：没有找到命令行注入 [47]，只找到"让命令行重新生效"的字节补丁（§1）。writecopy 只由环境变量控制。其余配置可能在闭源的 `cxcompatdb.so` 或 bottle 模板里 [低]。
- **Highball**（recipe，2026-08～09）[36]–[40]：
  - 所有 CEF 启动器 `sync: none`。Steam recipe 原话：CEF 界面在 msync/esync 下会挂起，其中"killing unresponsive browser"一条明确归因于 msync/esync。
  - Battle.net、EA、Ubisoft 设 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1`。
  - Ubisoft 的 pin 设 `WINE_SIMULATE_WRITECOPY=1`。
  - Steam：在界面里关闭"GPU accelerated rendering in web views"和 overlay。
  - 所有 recipe 都**没有**加 `--no-sandbox`。
  - 引擎补丁 0006 引入 `HKCU\Software\Wine\AppDefaults\<exe>\CommandLineAppend`；0005 是 msync 的"auto event 置位后 yield"[23][25]。
- **Proton**：
  - 硬编码的 `hack_append_command_line` 表只能追加，按 `wcsstr(cmd, exe_name)` 匹配，可以附带 SteamGameId [48]；
  - 对 `UplayWebCore.exe` 和 `Battle.net.exe` 自动开启 `simulate_writecopy` [49]。
- **应用自带**：IAGD 在检测到 Wine 时传 `--no-sandbox --disable-gpu --disable-gpu-compositing --single-process` [54]。

**与沙箱相邻的崩溃：`PAGE_WRITECOPY` 语义** [高]
- wiesson 的逆向（Battle.net 32 位 libcef）[50]：某处先 `VirtualProtect(page, PAGE_READONLY, &old)`，再 `CHECK(old == PAGE_READWRITE)`。
- Wine 对未写过的映像页返回 8（`PAGE_WRITECOPY`），导致 `int3; ud2`。
- 设 `WINE_SIMULATE_WRITECOPY=1` 后返回 4，可以登录，Diablo IV 也能完成 SSO。
- 教训：**"libcef 里出现 int3"至少有三种互不相关的原因**，需要反汇编触发 CHECK 的上下文才能分辨：
  - writecopy 语义；
  - GPU 进程反复崩溃后放弃（Highball 在 Wine 11 引擎、DXMT、未设跨进程开关时观察到）[37]；
  - %gs 读取错误（CX legacy 模型）。

### 4. WebView2 on Mac

#### 4.1 "只画第一帧"（WebView2Feedback #5720）的根因复核

报告内容 [52]：
- 环境：CX 26.3、macOS 26.7、M1 Pro；runtime 153.0.4234.46（Evergreen）和 149.0.4022.98（Fixed）。
- 观察到的命令行尾部是 `--in-process-gpu --use-gl=swiftshader`。
- v153 报 `gl_factory.cc:110 Requested GL implementation (gl=none,angle=none) not found`；v149 报 `eglInitialize D3D11Warp failed`。
- `requestAnimationFrame` 在 6 秒内一次都没有触发。

本文的复核：
- 同一命令行里还有 `--no-sandbox --disable-gpu-compositing --single-process`。它们来自 IAGD 的 PR #294，该 PR 在检测到 Wine 时注入，2026-07-28 合入 [54]。
- Chromium main 的 `GpuDataManagerImplPrivate` 构造函数写着：`kSingleProcess || kInProcessGPU` 时，对**当前进程**的 `CommandLine` 调用 `AppendGpuCommandLine()`；`SOFTWARE_GL` 模式下再调用 `SetSoftwareWebGLCommandLineSwitches()` [55]。
- 当前 main 的这个函数追加的是 `--use-gl=angle --use-angle=swiftshader-webgl`，或经策略允许的 `d3d11-warp-webgl` [56]。Edge 分支追加旧值 `swiftshader` 只能解释为 Edge 侧的行为差异 [低]。

**结论** [中]：触发链是"应用在 Wine 下自加的 `--single-process`/`--disable-gpu`"→"Chromium 就地给本进程追加软件 GL 开关"→"该值在新版无效，或 WARP 初始化失败"。它**不是**"runtime 检测到 Wine 后主动追加"。

- 这类追加发生在进程内，外部 hook 无法删除追加结果。
- 能做的是**在 `msedgewebview2.exe` 启动时删掉 `--single-process`、`--in-process-gpu`、`--disable-gpu`**，让 GPU 走正常的独立进程路径。
- 前提是 §2 的跨进程子窗口呈现已经可用。

**v149 路径的补充**：`D3D11Warp` 失败时，Wine 自带的 d3d11 对 WARP 会 `FIXME` 后回落到硬件 [57]。DXMT 忽略 DriverType [29]。D3DMetal 的行为未知。

**Retina 下内容"正好一半"（iagd #304）**：innerWidth=526 对应 1052pt 的控件，报告者认为 DSF 被除了两次 [53]。两种候选成因：
- (a) 宿主进程与 `msedgewebview2.exe` 的 DPI 感知方式不一致，跨进程 DPI 虚拟化出错。win32u 在 2026-07/08 仍在改这部分，例如"Allow creating client surfaces with raw physical coordinates"和"Always update window monitor DPI from its parent"[21]。
- (b) 与 dappermint #11 同类的像素/点混用 [19]。

根因**未确认**。可以用 `BoundsMode=UseRasterizationScale`、固定 `RasterizationScale=1` 做对照实验来区分。

#### 4.2 哪个 Fixed Version 可用

| 版本 | 平台 | 结果 |
|---|---|---|
| 149.0.4022.98 Fixed | CX 26.3 / macOS | 只画第一帧（带 IAGD 的开关）[52] |
| 150.0.4078.105 | staging 11.14 / macOS M4 / wined3d | 初始化成功但无渲染；d2d 的 `CreateDeviceContextState(FL10.0)` 被 wined3d 拒绝，dcomp 报 "Failed to create a D2D device context"，另有 `GL_INVALID_FRAMEBUFFER_OPERATION`（58921 c6，2026-07-28）[58] |
| 151.0.4129.78 | Linux（11.15 / staging 11.17） | win7 版本号可用，或 staging dcomp 可用（58921 c9；60318 c1，2026-09-20）[58][59] |
| 153.0.4234.32（2026-09-11） | Linux + NVIDIA / 11.16 | 空白（60318）[59][64] |
| 154 preview（2026-09-10） | — | 无数据 [64] |

**结论**：**macOS 上没有已验证可用的 Fixed Version。** 候选从 151.0.4129.78 开始实测，实测前不对外宣称可用。

#### 4.3 ANGLE D3D11 在 DXMT 还是 wined3d 上

| 维度 | DXMT | wined3d（macOS GL 4.1） |
|---|---|---|
| 跨进程子窗口 swapchain | HWND 属于别的进程时上游拒绝（`E_FAIL`）[26][27]；标准 Chromium 路径下子 HWND 属于 GPU 进程，可能不触发（核查更正，待日志验证）[85][86]；缺的主要是 §2 的驱动支持 | 经 win32u 的 GL client surface；跨进程子窗口在 winemac 上同样没有通路 [16] |
| WARP | 当作硬件 [29] | FIXME 后回落硬件 [57] |
| d2d1 / dcomp | `SwapDeviceContextState` 未实现（上游）[30] | `CreateDeviceContextState(FL10.0)` 被拒（macOS 实测）[58] |

**建议**：WebView2 默认走"win7 版本号（关掉 DComp）+ DXMT + §2 驱动托管"。wined3d 只作为诊断用的备选。

（核查更正：原建议写的是"DXMT（Cider fork 放行跨进程）"。DXMT 的跨进程放行（C4）是否必要，要等日志确认，所以不再作为默认配置的前提。）

#### 4.4 DirectComposition 的 staging 补丁集 [高]

- **规模**：master 上的 `patches/dcomp-DCompositionCreateDevice2` 有 67 个补丁加 definition [60]。
  - 0001–0065 是 Zhiyi Zhang 的系列，补丁头为 2026-03-13 的 `[PATCH nn/65]`。
  - 0066 "Always use the front buffer" 由 Alistair Leslie-Hughes 于 2026-05-19 加入。
  - 0067 "Allow IDCompositionDevice3" 由 Stian Low 于 2026-06-08 加入。
- **修复范围**：修复 bug 54968、58315。definition 写明完整实现"需要 dwm.exe 与图形驱动集成"。代码来自 `zhiyi/wine` 的 `bug-23698-react-native` 分支 [60]。
- **实现方式与局限**：
  - 0001 是 HACK：给 dxgi 加 `CreateSwapChainForComposition`；
  - 合成走 D2D，再 blit 到 `GetDCEx(hwnd)`；
  - 0062 的 shared visual handle **只支持同进程**，在 wineserver 的 d3dkmt 中新增了对象 [61]；
  - `UpdateLayeredWindow` 驱动的嵌入面板仍然是黑的（58921 c12，2026-09-25）[58]。
- **上游状态** [62][63]：
  - dcomp 仍是 1540 字节的 stub；
  - 6ca41b6f0b82（2026-05-27）取消了 prefer-native；
  - !10875（IDCompositionDevice3 头文件）已合入；
  - Zhiyi Zhang 的旧 draft !2243 于 2026-08-11 关闭；
  - !10180（DWM overlay，Linux 方向）仍是 open。

**结论**：不作为默认，只作为跟踪项。在 macOS 上还额外受 d2d/wined3d 与 DXMT API 缺口的双重制约。

### 5. 数据驱动的命令行改写 hook 与 per-exe profile

**现有机制的局限**：
- **Proton**：硬编码表，只能追加，匹配方式是子串 `wcsstr`，在父进程的 `CreateProcessInternalW` 中执行 [48]。
- **Highball 0006**：registry 驱动，但同样只能追加，也在父进程侧 [25]。
- **CX**：没有注入机制，改为用字节补丁"让 CEF 接受命令行"[1]。
- **四个共同问题**：
  1. 无法删除或替换参数；
  2. 绕过 kernelbase 的创建路径覆盖不到，例如直接调用 `NtCreateUserProcess`，或从宿主侧 `wine start`；
  3. 对 `command_line_args_disabled=1` 的 CEF 无效；
  4. 不懂 Chromium 的语义：同名开关以最后一个为准，`--enable-features`/`--disable-features` 需要合并而不是覆盖。

**设计（Cider）**：

1. **在子进程侧改写，作为主机制。**
   - 位置：ntdll 的 `LdrInitializeThunk` → `loader_init()` 早期，kernelbase 缓存 `GetCommandLineW()` 之前。
   - 做法：直接改写 `PEB->ProcessParameters->CommandLine`，同时重建 `ImagePathName` 之外的 argv 视图。
   - 效果：覆盖所有创建方式。
   - 规则来源：沿用 Wine 已有的 `AppDefaults` 机制，`loadorder.c` 早已在 ntdll 层读取 `HKCU\Software\Wine\AppDefaults\<exe>`。
   - 规则存放：Cider 的 profile 编译成 `HKCU\Software\Wine\AppDefaults\<exe>\Cider\ArgRules`（REG_MULTI_SZ，每行一条 JSON）。
2. **在父进程侧保留一个兼容点**：给需要父进程语义的规则使用，例如按父 exe 或 SteamGameId 过滤。
3. **按 Windows 规则解析参数**：分词用 `CommandLineToArgvW` 规则，重新拼接时按同一规则转义引号和反斜杠。遇到 Chromium 开关时按 `--name[=value]` 建模。
4. **CEF settings 覆盖，取代字节补丁。**
   - 在 loader 解析 import 以及 `LdrGetProcedureAddress` 时，把 `libcef.dll!cef_initialize`（必要时加上 `cef_execute_process`）重定向到 Cider 的 shim。
   - shim 先复制一份 `cef_settings_t`，选定字段布局，改写 `command_line_args_disabled` 和 `no_sandbox`，再调用原函数。
   - 布局选择（核查更正）：共有四种偏移：x64 为 0x68 或 0x64，x86 为 0x3c 或 0x38，取决于有没有 `chrome_runtime`。M127 还取决于构建配置（§1.2）。
     - 先按 libcef 版本选布局表，再用 `settings->size` 校验；
     - 版本未知或 size 对不上时不改写，只打日志 [90][91]。
   - 这对 Epic、Ubisoft 这类显式禁用命令行的应用是**唯一**不依赖版本字节的做法 [推断]。
5. **各框架原生开关优先。**
   - QtWebEngine：`QTWEBENGINE_CHROMIUM_FLAGS`、`QTWEBENGINE_DISABLE_SANDBOX=1` [65][66]。
   - WebView2：`WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS` 和策略键（见 10 号报告），再加 hook 删除参数。
   - Steam：`-cef-*` 启动参数。
6. **可观测。** 每次改写都打 `FIXME("CIDER: argv rewrite …")`，内容包括规则 id、改写前后的 argv。

**profile 中新增的字段**（schema 片段）：

```jsonc
"processes": [{
  "match": { "exe": "msedgewebview2.exe", "argsContain": ["--embedded-browser-webview"],
             "parentExe": null, "steamGameId": null, "fileVersion": ">=149 <155" },
  "args": {
    "remove":  ["--single-process", "--in-process-gpu", "--disable-gpu", "--use-gl"],   // 按开关名删除，不管取值
    "replace": [{ "from": "--use-angle=*", "to": "--use-angle=d3d11" }],
    "append":  ["--disable-direct-composition"],
    "features": { "disable": ["CalculateNativeWinOcclusion"] }   // 与已有列表合并
  },
  "cef":  { "commandLineArgsDisabled": false, "noSandbox": true }, // 走 cef_initialize shim
  "env":  { "WINE_SIMULATE_WRITECOPY": "1" },
  "winver": "win7", "sync": "none", "d3d11": "dxmt",
  "dxmtCrossProcess": false   // 核查更正：默认关闭；只有日志证实 swapchain HWND 跨 PID 时才打开（见 C4）
}]
```

匹配粒度支持：`exe`、路径 glob、参数子串或正则（例如 `--type=gpu-process`）、父 exe、SteamGameId、模块文件版本。按这个粒度，Proton 表中 `UnrealCEFSubProcess.exe`+appid 这类条目，以及 CX 的 exe 名 hack，都能无损迁移进来 [48][47]。

### 6. CEF 和 TSF 启动器中的 IME 与文本输入

**Chromium 侧（已证实）** [高]：
- main 的 `input_method_factory.cc` 在 `IS_WIN` 下返回 `InputMethodWinTSF`；在此之前只有 headless（`MockInputMethod`）和测试分支 [67]。
- **核查更正：TSF-only 是从 M122 开始的，不是所有版本都如此。** 按 tag 核对 `input_method_factory.cc` [88]：

| Chromium 版本（tag） | Windows 分支 | 典型宿主 |
|---|---|---|
| 85 / 90 / 100 / 108–110 | `kTSFImeSupport` 开启**且** `GetVersion() > WIN7` → TSF，否则 `InputMethodWinImm32` | Rockstar（85）、Epic（90）；BeamNG（72）推断相同，72 未按 tag 核对 |
| 111–121 | 只看 `kTSFImeSupport`，否则 `InputMethodWinImm32` | Ubisoft 旧版（111） |
| 122 / 124 / 126 | 无条件 TSF，文件里已不再引用 `InputMethodWinImm32` | Steam（126）、Ubisoft 新版（135） |

- `kTSFImeSupport` 的特性名是 `"TSFImeSupport"`，默认开启（121 的 `ui_base_features.cc`）[89]。所以对 ≤M121 可以用 `--disable-features=TSFImeSupport` 选 IMM32；对 ≤M110，win7 版本号就会自动选 IMM32。
- 以上只适用于窗口模式（Aura）的 CEF/Chromium。OSR 宿主自己处理 IME。QtWebEngine 走 Qt 自己的输入处理，不经过这个 factory。
- `TSFBridgeImpl::Initialize()` 依次调用 `CoCreateInstance(CLSID_TF_InputProcessorProfiles)`、`CLSID_TF_ThreadMgr`、`Activate`，然后建立 document map [69]。
- `OnUntranslatedIMEMessage` 只处理 `WM_IME_REQUEST` 和 `WM_CHAR`/`WM_SYSCHAR`，注释说明这是 TIP 未激活时的回落路径 [68]。
- 光标位置只通过 TSF 的 `OnTextLayoutChanged` 通知，不调用 `ImmSetCompositionWindow` 或系统 caret。

**Wine 侧**：
- macOS 输入法经 winemac 进入 imm32 的内置 IME，生成 `WM_IME_*` 消息（见 09 号报告）。
- Wine 的 msctf 没有 TIP，也没有"TSF 文本存储 ↔ IMM"的桥；2026 年上游 msctf 只有 stub 级别的改动（!10996）[72]。

**推断的行为** [中，需要实测]：
- **M122+（TSF-only，含 Steam CEF 126、Ubisoft 135）**：
  - Chromium 不处理 `WM_IME_COMPOSITION`，消息落到 DefWindowProc，由默认 IME UI 窗口画预编辑。`CFS_DEFAULT` 时位置在窗口左下。
  - 最终结果经 `WM_IME_CHAR` → `WM_CHAR` 送到 `OnChar` 上屏。
  - 也就是说，**能输入但预编辑不在行内，候选窗位置偏移**。
- **≤M121（Epic 90、Ubisoft 111、Rockstar 85、BeamNG 72）**：切到 `InputMethodWinImm32` 后，Chromium 自己处理 `WM_IME_COMPOSITION`。Wine 的 imm32 IME 应当能得到正常的行内组合 [推断]。
  - 注意：Epic 90 和 Ubisoft 111 设了 `command_line_args_disabled`。所以 `--disable-features=TSFImeSupport` 要等 C9 shim（或等价的 CX 字节补丁）让命令行生效后才起作用 [推断]。
- QtWebEngine（EA、GOG）的输入由 Qt 的 `QWindowsInputContext`（IMM32）处理，表现可能更好 [低]。
- （核查更正：原文写"CEF 126 等较旧版本是否已经是 TSF-only，未核实"。现已按 tag 核实：126 是 TSF-only，≤121 仍有 IMM32 回退 [88]。）

**上游进展**：
- 已合入：!12027（切换 hkl 时取消组合，2026-09-17）、!11193（`WM_IME_ENDCOMPOSITION` 一致性）[72]。
- 未合入：
  - !12164（Marc-Aurel Zent，2026-09-25）：winemac 从 Cocoa 主线程直接投递 IME 更新；
  - !12074 和 !12077（Masahito Suzuki，2026-09-21）：imm32 向宿主报告组合串位置，`CFS_DEFAULT` 时显示在系统 caret 处 [70][71]。
- !12077 依赖系统 caret，**帮不到 Chromium**，因为 Chromium 不创建 Win32 caret。

**Cider 要补的部分**：

0. **先做便宜的一步（核查后新增）**：给 <M122 的启动器下发 per-exe 规则，把它们切到 IMM32 路径。
   - M111–M121：argv 追加 `--disable-features=TSFImeSupport`，按 §5 的 `features.disable` 合并语义处理；
   - ≤M110：`winver=win7` 即可；
   - Epic、Ubisoft 111 这类显式禁用命令行的，依赖 C9；
   - 验收：LRS 的 IME 子项在这些启动器上预编辑位于行内。

1. 对 M122+（Steam、Ubisoft 135），在 msctf 中实现最小的 "TSF-aware composition rect"。
   - 当线程上有已 Push 的 `ITextStoreACP` context，且 imm32 开始组合时，取当前选区的 `GetTextExt()` 屏幕矩形；
   - 把它交给 `NtUserCallTwoParam(SetIMECompositionRect)`，并在 `OnLayoutChange` 时更新；
   - 这样 macOS 候选窗能跟随光标。

## 对 Cider 的启示与建议

### A. 优先级补丁清单（截至 2026-09-26 的上游状态）

| ID | 工作项 | 优先级 | 上游状态 | 参考 | Cider 动作 |
|---|---|---|---|---|---|
| C1 | winemac 跨进程**子窗口** Metal swapchain | P0 | 未实现（FIXME）；bug 60263 为 UNCONFIRMED，没有 MR [16][18] | 82030、dappermint d81a288/e590b06、Highball 0007 [18][20][24] | 以 11.11+ 的 CAContext 通路为底，合并 82030 的几何与生命周期处理和 dappermint 的可见性镜像；向上游提 MR |
| C2 | CAContextSwapChain 尺寸跟踪 + `CATransaction flush` + `cgrect_mac_from_win` | P0 | 上游顶层路径建好后不再更新 [17] | dappermint #11 [19] | 并入 C1；只在尺寸变化时 flush，并测量开销 |
| C3 | 撤销"flush 时隐藏 client_view"（1a1d1f3f3820） | P0 | 仍在 master；bug 60282 [32][34] | dappermint f77c272 [33] | 采用 f77c272；另起 MR 讨论上游方案 |
| C4 | DXMT 跨进程 swapchain 放行 | P1，先验证（核查更正：原为 P0） | HWND 跨进程时上游 DXMT 拒绝 [26]；标准 Chromium 路径下子 HWND 属于 GPU 进程，可能不触发 [85][86]；Highball 的 Battle.net recipe 报告相反的实测 [37] | Highball fork 的环境变量 [27][84] | 先抓 `+dxgi` 和 DXMT 日志，记录 `CreateSwapChain` 的 HWND 所属 PID。确认跨 PID 后，再在 Cider 的 DXMT fork 中按"驱动能力查询"放行（改动只有几行），不依赖环境变量；此时恢复为 P0 |
| C5 | winemac Metal layer 的 ExtEscape | P0 | !11058 open（2026-06-03）[31] | Marc-Aurel Zent | 合入；DXMT 改走这个接口，不再 dlsym `macdrv_functions` |
| C6 | 采用上游 GSBASE swap，不带 CX legacy 槽位镜像 | P0 | 10.5 起已在上游 [6][7] | — | 基线 ≥ 11.17 自动具备；专项验证 D3DMetal [8][10] |
| C7 | `SetThreadpoolTimerEx` | P0 | 11.17 已有 [44] | — | 基线 ≥ 11.17 |
| C8 | 子进程侧 argv 改写 hook + profile 的 `processes[]` 字段 | P0 | 无上游对应；Proton 和 Highball 都只能追加 [48][25] | — | 新写（§5） |
| C9 | `cef_initialize` settings shim（按 libcef 版本选布局，用 size 校验） | P1 | 无 | 替代 CW 23854/19252/25737 [1]；布局见 [90][91] | 新写；四种偏移（0x68/0x64/0x3c/0x38）加 M127 构建配置差异；版本未知时不改写。C14 的 IMM32 规则在 Epic、Ubisoft 111 上依赖它 |
| C10 | `WINE_SIMULATE_WRITECOPY` | P1 | 不在上游；CX 只认环境变量；Proton 对两个 exe 自动开启 [3][49] | CW 22996 | 移植，由 profile 开启 |
| C11 | msync/esync 下 CEF 挂起 | P1 | — | Highball 0005 [23] | 定位根因；在此之前 CEF 类 exe 默认 `sync=none` |
| C12 | 无边框窗口标题栏（bug 60262） | P1 | 无补丁 [73] | 60 行复现程序 | 自己实现并上游 |
| C13 | WebView2 默认值：win7 版本号、EdgeUpdate 设为手动、Fixed Version 固定、删除 `--single-process`/`--disable-gpu` | P1 | Proton 的 wine.inf 有 win7 行，上游没有 [79] | Proton、winetricks | wine.inf 加 profile |
| C14 | IME：<M122 启动器切 IMM32 + !12164、!12074、!12077 + msctf composition rect 桥 | P1 | 前三个 open [70][71] | Chromium 各 tag 的 `input_method_factory.cc` [88][89] | 核查后新增的第一步：<M122 用 `--disable-features=TSFImeSupport`（≤M110 用 win7）切到 IMM32；然后合入前三个 MR；桥自己写，只针对 M122+（Steam 126、Ubisoft 135） |
| C15 | DXMT `SwapDeviceContextState` | P2 | 上游未实现 [30] | Highball DXMT [27] | 合入（d2d1 和 dcomp 依赖它） |
| C16 | staging dcomp（67 个补丁） | P2 | 只在 staging [60] | Zhiyi Zhang 等 | 不默认启用；每月跟踪 |
| C17 | CX 的 Ubisoft ICD（19252）、Epic winstation（24938） | P2 | CX 独有 [47] | — | 做成 profile 动作，默认关闭 |
| C18 | CX 的 %gs 字节补丁、cohtml 模糊补丁 | 不移植 | — | [1] | 只有 C6 决定保留 legacy 模式时才考虑 |

### B. 启动器回归套件（LRS）定义

**运行矩阵**：
- d3d11 = {DXMT(Cider), D3DMetal, wined3d-GL}；
- RetinaMode = {n, y}；
- sync = {none, msync}；
- macOS 26.5，M3/8 GB。受内存限制**串行**执行，每项测完清理 wineserver。

**用例**：
- **L01 Steam**（64 位客户端，固定 build，CEF 126）：登录窗；商店、库；菜单下拉（2×1 创建路径）；窗口缩放；聊天框中文输入。
- **L02 Epic Games Launcher**：登录、库页面；winstation。
- **L03 EA app**（Qt/CEF 混合）：登录、首页。
- **L04 Battle.net**（32 位 CEF）：登录表单绘制；writecopy 开/关对照。
- **L05 Ubisoft Connect**（32 位 CEF 135）：登录窗。
- **L06 GOG Galaxy 2.1.x**（Qt6WebEngine）：登录后 2 秒崩溃的回归点（`Qt6WebEngineCore+0x292ada`）[40]。
- **L07 Rockstar Games Launcher**（CEF 85.3.x）。
- **W01–W04 WebView2**（Fixed 151.0.4129.78 起）：官方 Win32 与 WinForms 示例、MT5 Marketplace、IAGD。
- **C01 cefclient 多版本**：72 / 85 / 90 / 111(x86) / 135(x86) / 最新稳定版，用来覆盖 §1 的每个补丁点，以及 C6 与 C9 的效果。
- **Q01 Qt simplebrowser**（5.15.2 和 6.x）。
- **E01 Electron 无边框窗口**（对应 60262）。

**判定条件**（全部自动化）：
- 用 hook 给目标注入 `--remote-debugging-port`（Steam 用它的 CEF 调试面 [中]）；
- 从 macOS 侧经 CDP 统计 5 秒内 rAF 次数 ≥ 50；
- `Page.captureScreenshot` 3 秒内返回；
- 用 ScreenCaptureKit 截取宿主窗口，判断画面不是纯色；
- `--type=gpu-process` 的进程创建次数 ≤ 1；
- 记录 DXMT `CreateSwapChain` 收到的 HWND 所属 PID 与当前 PID，用来判定 C4 是否必要（核查后新增）；
- 记录 `chrome://gpu` 和 `chrome://sandbox`（Edge 为 `edge://`）的快照；
- 峰值 RSS 不超过预算；
- IME 子项：拼音、日文、韩文三种输入法，检查上屏正确、预编辑位置、候选窗与光标的距离，以及按住 WASD 时不丢键。
  - 对 <M122 的用例（C01 的 90/111、L02、L07），分别在 TSF（默认）和 IMM32（`--disable-features=TSFImeSupport`）两条路径上各测一次（核查后新增）。

**触发时机**：每次 Wine rebase、DXMT 或 D3DMetal 升级、WebView2 通道更新，以及每周的启动器自更新之后。

### C. Steam 策略决策

- **默认：bottle 内 Windows Steam（P0）。**
  - C1–C4 要支持的 CEF 启动器有六七个，Steam 只是其中之一；原生桥并不能省掉这条技术线。
  - bottle Steam 与 CrossOver 的产品形态一致，覆盖 CEG、Steamworks、DRM 和 overlay 的全部行为。
- **可选：原生 macOS Steam 桥（P2）。**
  - macos-steam 自称 "working beta"：只有源码，Steamworks API 覆盖不全（#45），开 overlay 时反作弊大概率失效，并且需要 CrossOver 25.1.1/26.2 [75]。
  - NotProton 需要 Steam build 1788652215 或 1790121765 加 CrossOver Preview 2026082，会 patch runner 的 ntdll 以加载 lsteamclient [74]。
  - 两者都依赖对 `m_bCompatEnabled` 这类内部状态的逆向。
- **Cider 的动作**：
  1. 提供稳定的 "runner" 接口：bottle 创建 CLI、ntdll 的 lsteamclient 加载点、环境约定，让 NotProton 和 macos-steam 能以 Cider 代替 CrossOver；
  2. 不自研桥。
- **重新评估的条件**：C1 到期仍无法稳定 Steam 的 CEF；或 Valve 官方在 macOS 上开放 Steam Play。

## 风险

1. **私有 API**：CAContext、CALayerHost、`_thread_set_tsd_base`（`0x3000003` trap）和 `CGSMainConnectionID` 都可能随 macOS 27/28 变化。Chromium 自己也依赖前两者 [35]，风险中等。
2. **上游化失败**：60263 的作者只提供参考实现，没有 MR；私有 CoreAnimation 代码在上游的接受度未知。Cider 可能要长期维护一个较大的 winemac 分叉。
3. **GSBASE 抉择**：如果 D3DMetal 必须用 legacy 模式（MetalSharp 的说法 [10]），Cider 就要同时支持两种模式，并按 exe 或路由切换。legacy 模式会带回 %gs 字节补丁和槽位镜像这一整类维护工作。
4. **WebView2 与 CEF 每月更新**：微软和各启动器按月推 Chromium，Steam 客户端也会自行更新（2026-09-03 的 "BMainLoop stalled" 回归）[36]，能用的组合随时可能失效。
5. **诊断误判**：int3 或黑屏这类症状对应多个根因（§3）。单用户、AI 辅助写出的诊断（60263、60262、#5720、#11 都有这种情况）要独立复现后再采信。
6. **8 GB 开发机**：多进程 CEF、DXMT、Rosetta 叠在一起内存压力大，回归套件只能串行跑，覆盖速度受限。

## 未解问题

1. CX 26.3 产品在没有 CAContext 代码的情况下，靠什么让 Steam、EA 的 CEF 画出来？`cxcompatdb.so` 做了什么？CX 的 DXMT 有没有未公开的跨进程补丁？
2. 上游的 GSBASE swap 与 D3DMetal（GPTK 3.x / 4.0b）是否兼容？dappermint 与 MetalSharp 的说法冲突；另外，swap 在 Rosetta 下的每次调用开销有多大？
3. Chromium 沙箱在 macOS + Rosetta + Wine ≥ 11.17 下各进程类型的实际状态（`chrome://sandbox`），以及 lowbox 和 mitigation 相关调用的表现。
4. Edge 153 为什么追加 `--use-gl=swiftshader`，而 Chromium main 已改为 `--use-angle=swiftshader-webgl`？
5. iagd #304 的"一半尺寸"，根因是跨进程 DPI 虚拟化还是 winemac 的像素/点混用？
6. msctf 桥能否让 M122+（Steam CEF 126 等）的候选窗跟随光标？对 ≤M121 的启动器，切到 IMM32 后 Wine 的 imm32 能否给出行内预编辑？（核查更正：原问题"CEF 126 是否已经是 TSF-only"已按 tag 核实，答案是"是"，M122 起就是 [88]。）
7. msync/esync 让 CEF 挂起的具体原语是什么？Highball 0005（yield after auto event set）是否就是修复？
8. cohtml 补丁（CW HACK 22901）检查的到底是什么，Wine 11 下是否已不再需要？
9. 各启动器传给 DXMT `CreateSwapChain` 的 HWND 是否跨进程？Chromium 源码表明标准路径是同进程 [85][86]，而 Highball 的 Battle.net recipe 报告开关能消除崩溃 [37]。这个问题决定 C4 做不做（核查后新增）。

## 参考来源

1. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/loader.c — CX 26.3 的 `build_module()`、完整补丁表和模糊补丁（第三方镜像）
2. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/signal_x86_64.c — CX 26.3 的 legacy %gs：只镜像 Self/TLS/PEB，没有 tsd_base
3. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/loader.c — CW HACK 22434（`pe_module_loaded`）、22996（writecopy）、24067（cxcompatdb.so）
4. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/virtual.c — `NtProtectVirtualMemory` 中的 writecopy 模拟
5. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/signal_x86_64.c — 上游用 `_thread_set_tsd_base` 做 GSBASE swap
6. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.5/ANNOUNCE.md — 10.5 的 "%GS register swapping on macOS"
7. https://gitlab.winehq.org/wine/wine/-/merge_requests/6866 — swap GSBASE 的 MR（Brendan Shanks）
8. https://raw.githubusercontent.com/dappermint/winecx/wine1117/dlls/ntdll/unix/signal_x86_64.c — dappermint 11.17 采用上游 swap
9. https://raw.githubusercontent.com/gauthierpiarrette/highball-engine/main/patches/0011-macos-lasterror-in-gs-slot.patch — legacy 模型下 `%gs:0x68` 问题的实测
10. https://github.com/metalsharp/MetalSharp/pull/608 — 带开关的 GSBASE：DXMT 用 swap，D3DMetal 用 legacy
11. https://raw.githubusercontent.com/chromiumembedded/cef/master/include/internal/cef_types.h — `cef_settings_t` 字段布局（master，已无 `chrome_runtime`；各分支布局见 [90][91]）
12. https://github.com/macgameport/cities-skylines-2-macos — CS2 on Mac、Wine 11 补丁数，以及 winemac 跨进程补丁的出处
13. https://api.github.com/repos/wine-mirror/wine/commits/1a63b0d7c431 — 提交元数据：author 2026-05-20，commit 2026-06-01，+224 行
14. https://gitlab.winehq.org/wine/wine/-/merge_requests/10935 — "winemac: Add cross-process MetalViewSwapChain"
15. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.11/ANNOUNCE.md — 11.11 更新日志中的 CALayerHost 条目
16. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/window.c — 子窗口 FIXME
17. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_window.m — 上游 `CAContextSwapChain`，无尺寸更新
18. https://bugs.winehq.org/show_bug.cgi?id=60263 — Bug 60263 与附件 82030（+513/−27）
19. https://github.com/dappermint/winecx-gptk/issues/11 — flush 与像素/点两个缺陷，含 3 行补丁
20. https://github.com/adurham/winecx/commit/d81a288 — dappermint 的跨进程子窗口实现（+202/−22）
21. https://github.com/dappermint/winecx/commits/wine1117 — f77c272、c033557、11.17 merge 等提交
22. https://github.com/adurham/winecx/commits/ — VRR、broker、`WHISKY_EXTERNAL_MODE_CONTROL` 提交
23. https://raw.githubusercontent.com/gauthierpiarrette/highball-engine/main/README.md — 引擎补丁 0005–0007 与 2026-09-04 的状态
24. https://raw.githubusercontent.com/gauthierpiarrette/highball-engine/main/patches/0007-winemac-cross-process-child-swapchains.patch — 回移植到 11.0 的 60263 实现
25. https://raw.githubusercontent.com/gauthierpiarrette/highball-engine/main/patches/0006-kernelbase-per-exe-command-line-append.patch — `CommandLineAppend`
26. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d11/d3d11_swapchain.cpp — 上游 DXMT 拒绝跨进程
27. https://raw.githubusercontent.com/gauthierpiarrette/dxmt/main/src/d3d11/d3d11_swapchain.cpp 与 https://github.com/gauthierpiarrette/highball-engine/releases/tag/dxmt-highball-20260904T194518Z-7421db2 — opt-in 放行与 SwapDeviceContextState
28. https://raw.githubusercontent.com/3Shain/dxmt/main/src/winemetal/unix/winemetal_unix.c — 通过 `macdrv_functions` 和 `get_win_data` 取 view
29. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d11/d3d11.cpp — 忽略 DriverType
30. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d11/d3d11_context_impl.cpp — `SwapDeviceContextState` 未实现
31. https://gitlab.winehq.org/wine/wine/-/merge_requests/11058 — Metal layer ExtEscape（open）
32. https://bugs.winehq.org/show_bug.cgi?id=60282 — EA App 白窗，由 1a1d1f3f3820 引起
33. https://github.com/dappermint/winecx/commit/f77c272 — 不在 flush 时隐藏 client_view
34. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/surface.c — master 中仍有隐藏逻辑
35. https://raw.githubusercontent.com/chromium/chromium/main/ui/base/cocoa/remote_layer_api.h — Chromium 自己使用 CAContext/CALayerHost
36. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/steam.json — Steam recipe（sync、GPU webview、已知问题）
37. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/battle-net.json — Battle.net（跨进程开关、Wine 11 下黑屏）
38. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/ea-app.json — EA app
39. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/ubisoft-connect.json — Ubisoft（writecopy、跨进程开关）
40. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/gog-galaxy.json — GOG（Qt6WebEngineCore 在 0x292ada 崩溃）
41. https://raw.githubusercontent.com/wine-mirror/wine/master/include/wine/asm.h — 与 Windows 同形的 syscall thunk（"Chromium depends on…"）
42. https://bugs.winehq.org/show_bug.cgi?id=56378 — Edge/WebView2 的 `--no-sandbox` 问题（只在 Linux 复测）
43. https://bugs.winehq.org/show_bug.cgi?id=57980 — `SetThreadpoolTimerEx`，2026-09-18 关闭
44. https://github.com/wine-mirror/wine/commit/8fc5b439ff6a19de46c794032cf89e9d96662f25 — 实现提交
45. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.17/ANNOUNCE.md 与 https://raw.githubusercontent.com/wine-mirror/wine/wine-11.18/ANNOUNCE.md — 11.17 的实现条目、11.18 的修复清单
46. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/kernelbase/kernelbase.spec — CX 26.3 中 `# @ stub SetThreadpoolTimerEx`
47. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/kernelbase/process.c — CW Hack 24938、24920/24557、19252
48. https://raw.githubusercontent.com/ValveSoftware/wine/proton_11.0/dlls/kernelbase/process.c — `hack_append_command_line` 表
49. https://raw.githubusercontent.com/ValveSoftware/wine/proton_11.0/dlls/ntdll/unix/loader.c — 对 UplayWebCore/Battle.net 自动开启 writecopy
50. https://raw.githubusercontent.com/wiesson/wine-cef-writecopy/main/docs/technical-notes.md — Battle.net libcef 的 writecopy 逆向
51. https://github.com/PlayOnLinux/wine-patches/blob/master/custom/steam_crossoverhack/crossover_hack_52560.patch — 旧版 steamwebhelper `--no-sandbox` hack
52. https://github.com/MicrosoftEdge/WebView2Feedback/issues/5720 — 只画第一帧（2026-09-18）
53. https://github.com/marius00/iagd/issues/304 — 冻结与 Retina 下一半尺寸
54. https://github.com/marius00/iagd/pull/294 — IAGD 在 Wine 下注入的开关（2026-07-28 合入）
55. https://raw.githubusercontent.com/chromium/chromium/main/content/browser/gpu/gpu_data_manager_impl_private.cc — 单进程时就地 `AppendGpuCommandLine`
56. https://raw.githubusercontent.com/chromium/chromium/main/ui/gl/gl_implementation.cc — `SetSoftwareWebGLCommandLineSwitches`
57. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/d3d11/d3d11_main.c — WARP 回落到硬件
58. https://bugs.winehq.org/show_bug.cgi?id=58921 — c6（macOS M4 失败）、c9、c11、c12（UpdateLayeredWindow）
59. https://bugs.winehq.org/show_bug.cgi?id=60318 — MT5，153 失败，151 配合 win7/staging 可用
60. https://gitlab.winehq.org/wine/wine-staging/-/tree/master/patches/dcomp-DCompositionCreateDevice2 — 67 个补丁加 definition
61. https://gitlab.winehq.org/wine/wine-staging/-/raw/master/patches/dcomp-DCompositionCreateDevice2/0062-dcomp-Implement-shared-visual-handle-for-the-same-proc.patch — shared visual 只支持同进程
62. https://api.github.com/repos/wine-mirror/wine/commits?path=dlls/dcomp — 上游 dcomp 历史（6ca41b6f0b82）
63. https://gitlab.winehq.org/wine/wine/-/merge_requests/10875 、!2243、!9839、!10180 — dcomp 和 DWM 相关 MR
64. https://learn.microsoft.com/en-us/microsoft-edge/webview2/release-notes/runtime/ — 153.0.4234.32（2026-09-11），154 preview（2026-09-10）（依据搜索摘要）
65. https://doc.qt.io/qt-6/qtwebengine-debugging.html — `QTWEBENGINE_CHROMIUM_FLAGS`
66. https://doc.qt.io/qt-6/qtwebengine-platform-notes.html — `QTWEBENGINE_DISABLE_SANDBOX`
67. https://raw.githubusercontent.com/chromium/chromium/main/ui/base/ime/init/input_method_factory.cc — Windows 上只用 TSF
68. https://raw.githubusercontent.com/chromium/chromium/main/ui/base/ime/win/input_method_win_tsf.cc — 只处理 `WM_IME_REQUEST` 和 `WM_CHAR`
69. https://raw.githubusercontent.com/chromium/chromium/main/ui/base/ime/win/tsf_bridge.cc — TSF 初始化流程
70. https://gitlab.winehq.org/wine/wine/-/merge_requests/12164 — winemac IME 从主线程投递（open）
71. https://gitlab.winehq.org/wine/wine/-/merge_requests/12074 与 https://gitlab.winehq.org/wine/wine/-/merge_requests/12077 — 组合串位置与 caret（open）
72. https://gitlab.winehq.org/wine/wine/-/merge_requests/10996 、!11193、!12027 — msctf stub 与 imm32 修复
73. https://bugs.winehq.org/show_bug.cgi?id=60262 — 无边框窗口标题栏与点击错位
74. https://raw.githubusercontent.com/Gcenx/NotProton/main/README.md — NotProton 的前提条件
75. https://raw.githubusercontent.com/Superd22/macos-steam/main/README.md — macos-steam 的现状与限制
76. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/repository/tags?search=wine-11 — Wine 11.x 各 tag 日期（11.11 为 2026-06-12，11.17 为 2026-09-04，11.18 为 2026-09-18）
77. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.10/ANNOUNCE.md — 11.10 中 "dcomp: No longer prefer native"
78. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/cocoa_window.m — CX 26.3 中没有 CAContext/CALayerHost
79. https://raw.githubusercontent.com/ValveSoftware/wine/proton_11.0/loader/wine.inf.in — Proton 的 `msedgewebview2.exe` win7 默认值
80. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/repository/tags/wine-10.5 — wine-10.5 tag 提交日期 2025-04-04（事实核查来源）
81. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/repository/commits/1a63b0d7c431 — 1a63b0d7c431 的 GitLab 提交元数据（author 2026-05-20，commit 2026-06-01）
82. https://gitlab.winehq.org/wine/wine/-/raw/master/dlls/winemac.drv/vulkan.c — 取 swapchain 失败时返回 `VK_ERROR_INCOMPATIBLE_DRIVER`（第 44 行）
83. https://bugs.winehq.org/attachment.cgi?id=82030 — 附件 82030：2026-09-03，取代 82021，+513/−27，补丁头声明 AI 编写、未提 MR
84. https://raw.githubusercontent.com/gauthierpiarrette/dxmt/highball/src/d3d11/d3d11_swapchain.cpp — Highball DXMT highball 分支：opt-in 放行及"winemac.drv overlay window"注释
85. https://raw.githubusercontent.com/chromium/chromium/main/ui/gl/child_window_win.cc — GPU 进程内在隐藏 popup 下创建 `WS_CHILDWINDOW` 呈现窗口
86. https://raw.githubusercontent.com/chromium/chromium/main/ui/gfx/win/rendering_window_manager.cc — `RegisterChild()` 校验子 HWND 属于 GPU 进程后 `SetParent`
87. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/repository/commits/8fc5b439ff6a19de46c794032cf89e9d96662f25 与 https://gitlab.winehq.org/wine/wine/-/raw/wine-11.17/ANNOUNCE.md — `SetThreadpoolTimerEx` 提交元数据（2026-09-04，tag 11.17/11.18）
88. Chromium 各 tag 的 `ui/base/ime/init/input_method_factory.cc`：https://raw.githubusercontent.com/chromium/chromium/90.0.4430.212/ui/base/ime/init/input_method_factory.cc 、https://raw.githubusercontent.com/chromium/chromium/100.0.4896.127/ui/base/ime/init/input_method_factory.cc 、https://raw.githubusercontent.com/chromium/chromium/110.0.5481.178/ui/base/ime/init/input_method_factory.cc 、https://raw.githubusercontent.com/chromium/chromium/121.0.6167.184/ui/base/ime/init/input_method_factory.cc 、https://raw.githubusercontent.com/chromium/chromium/122.0.6261.128/ui/base/ime/init/input_method_factory.cc 、https://raw.githubusercontent.com/chromium/chromium/126.0.6478.183/ui/base/ime/init/input_method_factory.cc — ≤110 带 WIN7 条件，111–121 只看特性开关，122+ 只用 TSF
89. https://raw.githubusercontent.com/chromium/chromium/121.0.6167.184/ui/base/ui_base_features.cc — `BASE_FEATURE(kTSFImeSupport, "TSFImeSupport", FEATURE_ENABLED_BY_DEFAULT)`
90. CEF 各分支的 `include/internal/cef_types.h`：https://raw.githubusercontent.com/chromiumembedded/cef/4430/include/internal/cef_types.h 、https://raw.githubusercontent.com/chromiumembedded/cef/5563/include/internal/cef_types.h 、https://raw.githubusercontent.com/chromiumembedded/cef/6478/include/internal/cef_types.h 、https://raw.githubusercontent.com/chromiumembedded/cef/6723/include/internal/cef_types.h 、https://raw.githubusercontent.com/chromiumembedded/cef/7049/include/internal/cef_types.h — `chrome_runtime` 在 4430/5563/6478 中存在，6723/7049 中已无
91. https://raw.githubusercontent.com/chromiumembedded/cef/6533/include/internal/cef_types.h 与 https://raw.githubusercontent.com/chromiumembedded/cef/6613/include/internal/cef_types.h — 6533（M127）中 `chrome_runtime` 位于 `#if !defined(DISABLE_ALLOY_BOOTSTRAP)` 内，6613（M128）已移除（2026-09-27 修订时补查）
92. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/repository/commits/1a1d1f3f3820 — "Hide client_view when flushing window surfaces" 的提交元数据（author 2026-08-04，commit 2026-08-11）

## 事实核查记录

核查日期 2026-09-27，共 10 条：
- 8 条确认。其中 3 条（DXMT 检查、Chromium IME factory、`cef_settings_t` 布局）附带措辞收窄或推论更正。
- 2 条部分正确。
- 没有被推翻或无法核实的条目。

修订时另外补查了 CEF 6533、6613 分支，Chromium 90/110 tag 和 `ui_base_features.cc`，以及 Highball 的 Battle.net recipe。下表只记录结论和改动，证据见对应的参考来源。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| CX 26.3 镜像 `loader.c` 的 `build_module()` 对 libcef/Qt5WebEngineCore 调用 `apply_binary_patches()`、对 cohtml 调用 `apply_fuzzy_binary_patches()`；补丁表中各 %gs 改写与 `command_line_args_disabled` 清零的 RVA 和偏移（§1.2） | 确认 | 行号 2358–2377，表内各 CW HACK、RVA、字段偏移均一致 [1]。细节补充：①部分补丁点加载到 `rsi` 而非 `rax`；②72.x 在 `IMMEDIATE_CRASH` 死代码区新写 helper；③命令行补丁清零的是 `cef_initialize` 内部拷贝的 settings；④21548 注释里的"CEF 83.0.4103.122"实为 Chromium 83，QtWebEngine 不是 CEF。表格已补注。镜像未与官方 tarball 比对 |
| 上游 Wine 10.5 起用 `_thread_set_tsd_base`（0x3000003）在进出 PE 时切换 GSBASE（!6866，tag 2025-04-04）；CX 26.3 没有 tsd_base，只镜像 Self/TLS/PEB | 确认 | !6866 于 2024-11-21 开启、2025-04-02 合入 [7][80]；master 的 `init_handler`/`leave_handler`/dispatcher 行为已补进 §1.2 [5]；CX 镜像第 3076–3078 行只写三个槽位 [2]。"原生 `%gs:0x8`/`0x68` 能读到真实 TEB"是由设计推出的结论，未实测，已标 [推断]。D3DMetal 与 swap 模型的兼容性仍未验证（未解问题 2） |
| 1a63b0d7c431 只处理跨进程顶层窗口（!10935，wine-11.11）；master 对子窗口 FIXME 后返回 `VK_ERROR_INCOMPATIBLE_DRIVER`；bug 60263 UNCONFIRMED，附件 82030（+513/−27）无 MR | 确认 | 提交与 tag 日期经 GitLab API 复核 [81]；子窗口分支的函数名和 vulkan.c 第 44 行已补进 §2.2 [82]。附件 82030 取代 82021，补丁头声明由 AI 编写，GitLab 上搜不到子窗口方向的 MR [83] |
| 上游 DXMT 在 HWND 属于别的进程时返回 `E_FAIL`；Highball fork 用 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` 放行 | 确认（代码事实）；推论未成立 | 代码事实正确 [26][27][84]。但"Chromium 的 ANGLE-D3D11 必须放宽此检查"没有得到证实：标准路径下子 HWND 由 GPU 进程创建并持有 [85][86]，PID 比较多半相等。修订时补查到，Highball 的 Battle.net recipe 称该开关能消除 Wine 11 下的崩溃 [37]，与源码分析冲突。已改摘要、§2.1、§2.3、§4.3 和 profile 示例（`dxmtCrossProcess` 默认 false），C4 从 P0 降为"P1，先验证"，LRS 增加 PID 记录，并新增未解问题 9 |
| `SetThreadpoolTimerEx` 在 wine-11.17（2026-09-04）由 8fc5b439ff6a 实现；bug 57980 于 2026-09-18 以 fixed in 11.18 关闭；CX 26.3 仍是 `# @ stub` | 确认 | 代码随 11.17 发布；bug 在 11.17 打 tag 之后才标 FIXED，所以列在 11.18 的修复清单里 [87][43]。CX 镜像 `kernelbase.spec` 第 1549 行仍是 stub，kernel32 无导出 [46]。报告者日志证实"调用即 abort"。已在 §3 补注 |
| Chromium main 的 `input_method_factory.cc` 在 Windows 上无条件返回 `InputMethodWinTSF`；`OnUntranslatedIMEMessage` 只处理 `WM_IME_REQUEST` 和 `WM_CHAR`/`WM_SYSCHAR` | 确认（附范围更正） | main 与 M122+ 如此，但前面还有 headless 和测试分支 [67][68]。≤M121 仍有 IMM32 回退，≤M110 在 ≤Win7 时自动选 IMM32 [88]。"只能靠 `WM_IME_CHAR` 回落"的结论只适用于 M122+ 的窗口模式 CEF，OSR 宿主不适用。已改摘要和 §6 |
| §6 称"CEF 126 等较旧版本是否已经是 TSF-only，未核实"，以及"Wine 的 imm32 IME 只能经 `WM_IME_CHAR`→`WM_CHAR` 到达 CEF" | 部分正确 | 已按 tag 核实：M122/124/126 只用 TSF；M111–121 可用 `--disable-features=TSFImeSupport` 选 IMM32（特性名与默认值见 [89]）；≤M110 在 win7 版本号下自动选 IMM32 [88]。Epic 90、Ubisoft 111、Rockstar 85 可切到 IMM32 路径，Steam 126 和 Ubisoft 135 不行。§6 已改为分版本的表格和推断；C14 新增第一步"<M122 切 IMM32"，并注明 Epic 和 Ubisoft 111 依赖 C9；未解问题 6 已改写；LRS 增加 TSF/IMM32 对照 |
| §2.1 和 C4 称 GPU 进程把 ANGLE→D3D11 swapchain 画到根窗口属于浏览器进程的子 HWND 上，所以上游 DXMT 的跨进程拒绝必须放宽（P0） | 部分正确 | 根窗口确实属于浏览器进程。但子 HWND 由 GPU 进程创建，浏览器校验归属后才 `SetParent` [85][86]；Wine 跨进程 `SetParent` 不改变所属线程。所以 DXMT 检查通常看到的是同一进程，真正的阻塞点是 winemac 的每进程 NSView（C1）。这是推断，中等置信度。§2.1 已补"核查更正"，C4 已改为"先用日志验证" |
| §1.2 称 0x68/0x3c/0x38 与带/不带 `chrome_runtime` 的 `cef_settings_t` 布局吻合，`chrome_runtime` 在 CEF 111 到 135 之间被移除（推断） | 确认（范围收窄） | 布局吻合，CX 135 补丁先读 `[edi+0x34]` 再读 `[edi+0x38]` 也与之一致。核查者确认它在 6478（M126）仍在，6723（M130）/7049（M135）已无 [90]。修订时补查到：6533（M127）中它受 `DISABLE_ALLOY_BOOTSTRAP` 条件编译控制，6613（M128）已移除 [91]。§1.2 已改用分支头文件，补上 x64 无该字段时的 0x64；C9 和 §5 改为"按版本选布局，size 只作校验" |
| §2.2、§4.4、§6 的上游状态：1a1d1f3f3820 仍在 master；bug 60282 UNCONFIRMED；!11058、!12164、!12074、!12077 open；!12027 已合入；staging dcomp 67 个补丁加 definition；DXMT `SwapDeviceContextState` 未实现 | 确认 | 截至 2026-09-27 全部与 GitLab API 一致。另确认 !11193（2026-06-22）、!10875（2026-05-12）已合入，!2243 于 2026-08-11 关闭，!10180 仍 open，asm.h 的 syscall thunk 与注释属实 [30][31][32][34][41][60][70][71][92]。§2.2 已补 1a1d1f3f3820 的作者日期和 `surface.c` 行号 |
