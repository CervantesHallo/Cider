# Wine 上游现状（10.x / 11.x）、新 WoW64、macOS 驱动与 Proton 可移植性 —— Cider 调研报告 03

> 调研日期 2026-09-26 · 置信度说明：**[已证实]** = 直接读到一手来源（源码、tag、release notes、MR/Bug 原文）；**[二手]** = 来自媒体/搜索摘要，未能直接读取原文（CodeWeavers 博客被 Cloudflare 拦截）；**[推断]** = 基于已证实事实的工程推理。版本号与日期均附出处，未能核实的写明“未核实”。本报告与 02（CrossOver 源码差异）、05（D3D12/Vulkan/Metal）、06（Rosetta/ARM64）互补，图形与 CPU 翻译细节以那几份为准。

## 摘要

- **版本现状 [已证实]**：Wine 10.0 于 2025-01-21 发布；Wine 11.0 于 **2026-01-13** 发布（tag `wine-11.0`，tagger 日期 2026-01-13T15:56:08Z，提交 `db11d0fe6a16`）[1][62]，约 6,300 个变更、600+ bug 修复，主打 **NTSync** 与 **新 WoW64 完成**。截至 2026-09-26，最新开发版为 **Wine 11.18（2026-09-18，Alexandre Julliard 打 tag；`wine-11.19` tag 尚不存在）**[5][62]，按双周节奏 11.19 预计在 10 月初发布 [推断]。wine-staging 最新为 v11.18。
- **新 WoW64 [已证实]**：Wine 11.0 宣布新 WoW64“fully supported”，支持 16 位程序；`wine64` 加载器被移除，只剩一个 `wine`；`WINEARCH=win32` 纯 32 位前缀被弃用。它仍不是 configure 的默认项，在 x86_64-darwin 上要显式传 `--enable-archs=i386,x86_64` [8][63]。**在 macOS 上这是运行 32 位 Windows 程序的唯一上游路径**（Catalina 起没有 32 位 Unix 进程）。已知代价：在 macOS 上，32 位 OpenGL 程序做 buffer 映射要走**拷贝路径**，因为零拷贝方案依赖 `GL_EXT_memory_object_fd`，macOS 没有这个扩展。
- **ARM64EC [已证实]**：Wine 10.0 起完整支持 ARM64EC/ARM64X，并提供 x86-64 模拟器接口（FEX）。上游已有一批 `aarch64 && __APPLE__` 代码，但 WineHQ wiki 仍写着 macOS 上“Wine is not ready yet to be build for ARM”。社区 MR !11638 列出的阻碍是：4GB page zero、`KUSER_SHARED_DATA` 固定地址不可用、x18 寄存器。CodeWeavers 于 **2026-07-31** 发布首个 Mac ARM64 原生 CrossOver Preview，用的是**定制版 FEX** [二手]。
- **Rosetta 时间窗 [已证实，但时间点存疑]**：Apple 开发者新闻（2026-09-01）表示 macOS 27 是最后一个通用支持 Rosetta 的版本；支持文章 102527 说 macOS 28 起 Rosetta 只保留给“部分老旧、无人维护的游戏”[53][71]。**存疑**：MacObserver（2026-09-12）报道 Apple 在 2026-09-09 的另一篇开发者文章写的是“macOS 26 是最后一个支持 Intel Mac 和 Rosetta 的版本”，与上述说法矛盾 [72]。Wine 类 x86 进程是否属于游戏子集也没有定义。因此 Cider 必须双轨：v1 用 x86_64 Wine + Rosetta，v2 用 ARM64 Wine + x86 模拟器，并持续跟踪 Apple 的正式措辞。
- **winemac.drv [已证实]**：`MAINTAINERS` 里**没有 Mac 驱动条目**，默认归 Julliard 管。实际主力是 CodeWeavers 的 Brendan Shanks、Rémi Bernon、Tim Clem，以及 Marc-Aurel Zent（隶属未核实）。2025-09 至今标题含 “winemac” 的 MR 共 51 个（41 已合并）。近期工作包括：GL/Vulkan client surface 通用化、基于私有 API `CALayerHost`/`CAContext` 的跨进程 Metal swapchain、HDR/EDID、显示重配置回调。尚未解决的缺口：Steam(CEF) 子窗口黑屏（Bug 60263）、Metal layer ExtEscape（!11058，服务 DXMT）、GCMouse 原始输入（!11799）。
- **图形 [已证实]**：wined3d 默认渲染器仍是 **OpenGL**（`WINED3D_RENDERER_AUTO → OPENGL`），Vulkan 渲染器“尚未与 GL 同等”。winemac 的 Vulkan 优先用 `VK_EXT_metal_surface`，不可用时回退到 `VK_MVK_macos_surface`，找不到 `libvulkan` 时直接 dlopen MoltenVK。vkd3d-shader 从 **vkd3d 1.14（2024 年底）** 起带**实验性 MSL 目标**（`VKD3D_SHADER_TARGET_MSL`），但只有用 `-DVKD3D_SHADER_UNSUPPORTED_MSL` 构建时才启用，普通构建里是关闭的；1.15 至 2.x 持续完善 [65][66]。
- **同步 [已证实]**：NTSync（Linux ≥ 6.14）只在 Linux 上可用。在 macOS 上，NT event/mutex/semaphore 仍要往返 wineserver。上游在 macOS 上只有一条快路径：futex 仿真（`USE_FUTEX`），macOS 14.4+ 用 `os_sync_wait_on_address`，更早版本用 `__ulock_wait`。thread-ID alert、`RtlWaitOnAddress`、临界区和 SRW 都建在这条路径上。msync（Mach semaphore，LGPL-2.1）仍在树外，但 CrossOver 自 **23.7.0（2023-11-27）** 起就内置了它 [52]。**这是 Cider 与 CrossOver 之间最大的 CPU 侧性能差距之一**。
- **上游对 macOS 的保障很弱 [已证实]**：GitLab CI 在 macOS 上只做 64 位构建（`--enable-win64`，ARM runner 上经 `arch -x86_64` 跑），**不跑测试**，也不构建 i386（即不覆盖新 WoW64）；test.winehq.org 没有 macOS 列。Homebrew 的 wine cask 因通不过 Gatekeeper，已于 **2026-09-01 被禁用**（Bug 58946，状态 UNCONFIRMED）[35][73]。
- **Proton [已证实]**：Proton 11.0-1（2026-07-07）基于 Wine 11.0，附带 FEX-2605（ARM64EC 构建）、dxvk 2.7.1-467、vkd3d-proton proton-20260410、dxvk-nvapi v0.9.1、Wine Mono 11.0.0。`ValveSoftware/wine` 的 `proton_11.0` 分支在 wine-11.0 之上约有 **1,453 个提交**，其中大量是按游戏写的 HACK，另有 MF/GStreamer 修复、winex11 fshack、fsync 和 Steam 桥接。移植性判断：ntdll/mscoree/MF 类的 HACK 可以移植；winex11、fsync、lsteamclient、media-converter 不能。
- **按游戏的修复库 [已证实]**：umu-protonfixes 是 **BSD-2-Clause**（可以直接吸收），umu-launcher 与 umu-database 是 **GPL-3.0**（只能作为独立数据或工具使用），Proton 顶层脚本是 BSD-3。

## 详细调研

### 1. Wine 发布状态（2025–2026）与 macOS 相关特性

**1.1 稳定版与开发版时间线**（tag 日期取自 wine-mirror 的 tag 对象 [6][62]）

| 版本 | 日期 | 要点 |
|---|---|---|
| Wine 10.0 | 2025-01-21 | ARM64EC/ARM64X 完整支持、x86-64 模拟接口、Vulkan 1.4.303、HiDPI 自动缩放、opt-in 模式切换模拟、FFmpeg（winedmo）MF 后端 opt-in；**macOS**：用 Xcode ≥ 15.3 构建时不再需要 preloader，Sonoma 起支持直接 NT syscall 的仿真 [3] |
| 10.2 → 10.20 | 2025 年内 | 10.2 线程优先级、`WINEARCH=wow64` 可动态启用新 WoW64；10.5 macOS `%gs` 交换；10.10 macOS CI 切到 Sequoia VM；10.16 NTSync 与 16 位新 WoW64；10.17 EGL 默认（X11）；10.18 WoW64 下用 Vulkan 映射 GL 内存；10.19 reparse points [7] |
| **Wine 11.0** | **2026-01-13** | NTSync、新 WoW64 完成、单一 `wine` 加载器、syscall 编号与 Windows 一致、独占全屏、Vulkan 1.4.335、D3D11 H.264 硬解（经 Vulkan Video，仅 Vulkan 渲染器）、hidraw/力反馈改进 [2] |
| 11.1 → 11.18 | 2026-01-23 → **2026-09-18** | 11.3 vkd3d 1.19、Mono 11.0.0；11.5 C++ 构建支持、Linux Syscall User Dispatch；11.6 游戏 mod 的 DLL 加载顺序启发式、**macOS 64 位强制要求 PE 编译器**；11.8 Mono 11.1.0；11.9 system threads；11.10 **vkd3d 2.0**；11.11 SymCrypt 取代 TomCrypt；11.12 **内置 FFmpeg 8.1.1**（libswresample/libswscale）、Mono 11.2.0；11.14 FreeBSD 新 WoW64；11.15 mingw 模式 ARM64EC；11.16 Mono 11.3.0（含 ARM64）、VA-API；11.17 **vkd3d 2.1**、显示模式模拟初步支持；11.18 NTOSKRNL 驱动支持 [5][7][75][76] |

**1.2 Wine 11.0 中 macOS 专属条目 [已证实][2]**
- 在 macOS 上，syscall dispatcher 会交换 `%gs`，避免 Windows TEB 与 macOS 线程描述符冲突（对应 10.5 Brendan Shanks 的提交“swap GSBASE between the TEB and macOS TSD”）。
- 线程优先级在 Linux 和 macOS 上都已实现（10.2 Marc-Aurel Zent 的 `apply_thread_priority`）。
- 11.x 期间的 macOS 相关提交 [7]：11.1 Julliard 提交“win32u: Add syscall wrappers to work around the macOS ABI breakage on ARM64”，因为 Apple arm64 ABI 在栈上会紧凑打包小于 64 位的参数（见 `dlls/win32u/syscall.c` 中的 `WRAP_FUNC`）[18]。11.14“Hard code the host address space limit on macOS”：原算法返回 `0x7fffffff0000`，但内核实际上限更低。11.17“Spawn a Wine system thread for the macOS main thread on launch”。11.16“Populate the CPU name and vendor on ARM64 macOS”。11.13“Detect ARM64 processor features on macOS”。

**1.3 内置组件版本**（Cider 打包时要对齐）[2][3][61]

| 组件 | Wine 10.0 | Wine 11.0 | master（11.18） |
|---|---|---|---|
| vkd3d | 1.14 | 1.18 | 2.1（11.17） |
| wine-mono | 9.4.0 | 10.4.1 | 11.3.0 |
| wine-gecko | 2.47.4 | 2.47.4 | 2.47.4 |
| FAudio | 24.10 | 25.12 | — |
| FFmpeg（内置 PE） | 无 | 无 | 8.1.1（11.12 导入） |
| Vulkan API | 1.4.303 | 1.4.335 | — |

11.x 期间的升级节点 [已证实][7][61][74][75][76]：wine-mono 在 11.3 升到 11.0.0，11.8 升到 11.1.0，11.12 升到 11.2.0，11.16 升到 11.3.0（含 ARM64）。vkd3d 在 11.3 升到 1.19，11.10 升到 2.0，11.17 升到 2.1。FFmpeg 8.1.1 在 11.12 导入。wine-11.0 的 `addons.c` 为 `MONO_VERSION "10.4.1"`、`GECKO_VERSION "2.47.4"`，master 为 `MONO_VERSION "11.3.0"`。

### 2. 新 WoW64：状态、macOS 默认、对 32 位游戏的影响

**演进 [已证实]**
- Wine 9.0：所有 Windows→Unix 跳转都走 NT syscall 接口，PE 化完成；用 `--enable-archs=i386,x86_64` 启用新 WoW64，而且它“finally allows 32-bit applications to run on recent macOS versions”[4]。
- 10.2：`WINEARCH=wow64` 可动态强制启用。10.16：支持 16 位（LDT 设置、段寄存器）。
- 11.0：“fully supported”，与旧 WoW64 功能基本对等；`wine64` 被移除，同时存在 32/64 位版本的程序默认用 64 位，32 位版本需给显式路径（如 `c:\windows\syswow64\notepad.exe`）；`WINEARCH=win32` 被弃用，且新 WoW64 不支持这种前缀 [2]。

**macOS 上是否默认**
- `configure.ac` 默认只构建宿主架构，在 `x86_64-darwin` 上还**禁止** `--without-mingw` [8]。所以新 WoW64 不是 configure 的默认项，但**所有能在现代 macOS 上跑 32 位程序的构建都必然是新 WoW64**。WineHQ 官方的 macOS 包（Gcenx）用的就是 `--enable-archs=i386,x86_64`（README 明确列出这个参数），只提供 `osx64`（x86_64）包，在 Apple Silicon 上经 Rosetta 2 运行 [36][63][25]。
- 上游 CI 的 `build-mac` 用 `configure --enable-win64 --with-mingw`，也就是**不构建 i386 PE**，所以 macOS 上的新 WoW64 路径**没有上游 CI 覆盖**。测试二进制只作为 artifact 安装，从不运行 [21]。

**性能与已知问题**
- OpenGL：`opengl32/unix_wgl.c` 的零拷贝 WoW64 buffer 映射要求 `GL_EXT_memory_object_fd` 与 Vulkan placed memory。缺少时会打出 “Doing a copy of a mapped buffer (expect performance issues)”，并禁用 `GL_ARB_buffer_storage`、不支持 `GL_MAP_PERSISTENT_BIT`；WoW64 下 GL 版本上限是 4.3 [14]。**macOS 没有 `GL_EXT_memory_object_fd`**，因此 32 位 GL（以及走 wined3d-GL 的 32 位 D3D≤9 游戏）在 macOS 上必然走拷贝路径 [推断，高置信]。缓解办法：32 位 D3D 游戏走 DXVK/DXMT/D3DMetal，不走 wined3d-GL（见 04/05 报告）。
- 已修复的典型问题：#57444 “pop gs”在兼容模式下行为不同导致多款游戏崩溃（10.15 修复）；#58698 新 WoW64 下死循环（11.16 修复）；#55981 新 WoW64 下 GL 慢（10.18 靠 Vulkan 映射修复，但**这个方案在 macOS 上不可用**）[7]。
- 16 位程序在“macOS + Rosetta + 新 WoW64”下能否工作（涉及 LDT）：**未核实**。

### 3. ARM64EC / ARM64X 与 macOS

**上游 [已证实][3][7]**
- 10.0：ARM64EC 与 ARM64 功能对等；ARM64X 混合模块用 `--enable-archs=arm64ec,aarch64` 构建；x86-64 模拟通过 `HKLM\Software\Microsoft\Wow64\amd64` 指定外部模拟库，i386 用 `...\Wow64\x86`（9.0），FEX 以 PE 形式实现这两个接口。ARM64 要求 4K 页。
- 10.5：支持更大的宿主页（对齐到宿主页）；11.0：在 16K/64K 页宿主上**模拟 4K 页**，但原文说这只对“simple applications”有效，并“strongly recommended”使用 4K 内核 [2]。Apple Silicon 上原生 arm64 进程是 16K 页 [推断，常识]。
- 11.x 的 ARM64EC 进展：cooperative suspend（11.9）、mingw 模式 ARM64EC（11.15）、“native-ready .NET 应用以 ARM64EC 运行”（11.15）、FFmpeg ARM64EC 汇编（11.13）、异常处理改进（11.16）；ARM64EC relay 支持（!12032）仍是开放 MR [26]。

**与 macOS 的关系**
- 上游源码已有 aarch64-darwin 相关代码：`signal_arm64.c` 里的 `__APPLE__` 分支（`uc_mcontext->__ss`、`__es.__esr`）[19]，以及 win32u 的 ABI 包装 [18]。但 WineHQ wiki 仍写“Wine is not ready yet to be build for ARM”，要求在 Apple Silicon 上构建 x86_64 版、用 Rosetta 运行 [24]。
- 社区 MR !11638（2026-08-11，仍开放）列出原生 Apple Silicon 的硬性障碍：内核强制 4GB page zero，loader 只能做成 PIE；`0x7ffe0000` 不可访问，需要动态 `KUSER_SHARED_DATA`（`TPIDRRO_EL0 - 0x1000`）；macOS 12 之后无法使用 x18（Windows ARM64 用 x18 指向 TEB）[29]。
- CodeWeavers：2025-11 Linux ARM64 Preview 集成了 FEX；**2026-07-31 发布 Mac ARM64 Preview**，使用“custom version of FEX”，没有 D3DMetal，很多启动器无法工作，需要新建 bottle；CrossOver 27 预计 2027 年初发布，只支持 Apple Silicon [二手][50][51]。FEX 官方仍定位为 Linux 用户态模拟器，发布说明里没有 macOS 宿主支持 [60]；CodeWeavers 定制的 FEX 源码是否公开**未核实**（FEX 为 MIT 许可，没有开源义务）。
- Apple：macOS 26.4 起会对依赖 Rosetta 的 App 发通知。Apple 开发者新闻（2026-09-01）称 macOS 27 是“Final release to support Rosetta”，之后保留面向“older, unmaintained gaming titles”的子集；支持文章 102527 说 macOS 28 起 Rosetta 只提供给部分老旧、无人维护的游戏 [53][54][71]。基于 Wine 的 x86 进程是否属于这个子集，**未知**。
- **存疑**：MacObserver（2026-09-12）报道，Apple 2026-09-09 的一篇开发者文章写的是“macOS 26 is the final release supporting Intel Mac computers and Rosetta”，与上面两份 Apple 一手来源矛盾 [72]。我的判断 [推断，中置信]：以 Apple 开发者新闻和支持文章 102527 的“macOS 27 最后、macOS 28 收缩”为主线，那篇文章更可能指 Intel Mac 硬件支持的终止，而不是 Apple Silicon 上的 Rosetta。但 2026-09-09 的原文未能直接读取，Cider 应按更早收缩的最坏情况做预案，并持续跟踪 Apple 的措辞。

### 4. winemac.drv、Vulkan、wined3d、OpenGL

**4.1 维护者与活跃度 [已证实]**
- `MAINTAINERS` 里没有 “Mac driver” 条目，只有 `dlls/winemac.drv/ime.c` 归 Input methods（Rémi Bernon）。其余落入 “THE REST”（Alexandre Julliard）[20]。OpenGL、Vulkan、HID、Media Foundation 的维护者都是 CodeWeavers 员工（Rémi Bernon、Jacek Caban、Nikolay Sivov 等）。
- GitLab API：2025-09-01 以来，标题含 “winemac” 的 MR 共 51 个（合并 41、开放 7、关闭 3）。作者分布：Brendan Shanks 14、Rémi Bernon 13、Marc-Aurel Zent 8、Tim Clem 8、Elvin Hayatov 2、Jactry Zeng 2 等 [26]。

**4.2 近期关键变更（10.x–11.18）[7][11]**
- GL 栈改为走 win32u 通用实现：pbuffer、context、`wglSwapBuffers` 都用通用实现；11.13 起所有 core context 用可用的最高 profile（GL3/GL4 Core）；11.18/master 持续清理 `macdrv_context`。
- 10.13：每个 VK/GL surface 创建独立的 client view。11.11：`MetalViewSwapChain`，以及**经 `CALayerHost` 实现的跨进程 swapchain**（提交 `1a63b0d7c431`，Marc-Aurel Zent，作者日期 **2026-05-20**；原稿写的 2026-06-01 至多是提交/合并日期，未核实；在 `cocoa_window.m` 中声明私有的 `CAContext`/`CALayerHost`/`CGSMainConnectionID`）[11][77]。
- 显示：10.17 全屏内容的黑边（letter/pillarbox）；10.20 根据 `NSScreen.maximumPotentialExtendedDynamicRangeColorComponentValue` 报告 HDR 能力，EDID 取自 DCPAVServiceProxy/IODisplayConnect；11.14 用 `CGDisplayRegisterReconfigurationCallback` 检测显示变化。
- 11.17：移除废弃 API（`kUTTypeContent`、`graphicsPort`、`NSWindow.oneShot`）及 `WineDisplayLink`。2026-09-25 master 上 Brendan Shanks 重构 ObjC 头文件（WINBOOL、C99 bool）。

**4.3 尚未解决的缺口（对 Cider 很关键）**
- Bug 60263：“cross-process child window Metal swapchains not implemented”，**导致 Steam 客户端（CEF GPU 进程）黑屏**。附带的参考补丁约 +513/-27，把 Metal 路径接到已有的 CAContext/CALayerHost 路由上 [32]。
- Bug 60262：用 `WM_NCCALCSIZE` 收回标题栏的无边框窗口（Electron/CEF 常见）仍被画上 macOS 标题栏，导致点击坐标错位 [33]。另有 60266（全屏窗口上方的窗口被画到后面）、60282（EA App 白窗）。
- !11058（开放）：ExtEscape 查询，用于创建/释放 Metal layer，“for projects … directly on top of Metal … like DXMT”。**这是 DXMT 类后端的上游接口，目前仍在树外** [26]。
- !11799、!11999、!11880：GCMouse 原始鼠标输入。其中 MR 描述了 **macOS 26 上用 `[NSCursor hide]` 隐藏光标时，移动鼠标会让帧呈现锁定到刷新率**（Overwatch 从 250–300 FPS 掉到 120），改用透明光标规避 [27]。
- !10523：从 Vulkan/MoltenVK 取 GPU VendorId/DeviceId，修复 PlayStation PC SDK 游戏在 Apple Silicon 上 ID 不一致的问题。!11538：`win32u: Enable host Vulkan portability enumeration`，让 MoltenVK 能经 vulkan-loader 使用；Gcenx 的 11.18 包已预先打上这个补丁 [28][36]。

**4.4 Vulkan / wined3d / OpenGL [已证实]**
- winemac Vulkan：检测到 `vkCreateMetalSurfaceEXT` 就走 `VK_EXT_metal_surface`，否则回退 `VK_MVK_macos_surface`，并把 `VK_KHR_win32_surface` 映射过去 [9]。configure 找不到 `libvulkan` 时 `WINE_CHECK_SONAME(MoltenVK, …)` 直接把 MoltenVK 当 `SONAME_LIBVULKAN` [8]。反过来说，只要构建环境里有 vulkan-loader 的 `libvulkan`，它就优先生效。此时 loader 后面的 MoltenVK 需要 portability enumeration，也就是仍开放的 !11538（Dean M Greer，2026-07-31 提交）[28]。Cider 打包时二选一 [推断]：要么不带 loader、让 configure 直接绑定 MoltenVK；要么带 loader 并打上 !11538。win32u 在 swapchain 尺寸与呈现尺寸不一致时用 `VkSwapchainPresentScalingCreateInfoEXT`，专门规避 MoltenVK 返回 `VK_SUBOPTIMAL_KHR` 的问题 [10]。
- wined3d：`wined3d_get_renderer()` 在 AUTO 时返回 OPENGL [15]。Wine 11.0 为 Vulkan 渲染器补齐了点精灵、顶点混合、固定管线 bump、alpha test、用户裁剪面等功能，但“not yet at parity … not yet the default”[2]。开启方式：`WINE_D3D_CONFIG=renderer=vulkan`，或注册表 `HKCU\Software\Wine\Direct3D\renderer`。
- OpenGL on macOS：Apple GL 最高 4.1 Core 且已废弃；Wine 10.17 起 EGL 是 X11 上的默认，与 macOS 无关。vkd3d-shader 的实验性 MSL 目标（`VKD3D_SHADER_TARGET_MSL`）始于 **vkd3d 1.14**（2024 年底），只有用 `-DVKD3D_SHADER_UNSUPPORTED_MSL` 预处理选项构建时才启用；1.15 加入更多 MSL 指令（Feifan He），之后一直完善到 2.x（原稿称 2.0 已支持像素着色器 stencil ref）[65][66][55]。它**不是 2.0 的新功能，默认也不开启**。将来可能成为 wined3d/d3d12 的 Metal 后端基础 [推断]。

### 5. ntsync 与 macOS 同步

- **Linux [已证实]**：NTSync 内核模块随 Linux 6.14 提供；Wine 10.11 开始准备，10.16 启用“Fast synchronization support using NTSync”[64]。实现分两层：`server/inproc_sync.c` 负责 open `/dev/ntsync` 和 ioctl 创建对象；`ntdll/unix/sync.c` 按 handle 缓存 inproc sync，直接 ioctl 等待。没有 ntsync 时，整层编译成 stub（`#else /* NTSYNC_IOC_EVENT_READ */`），**对象模型基于 fd** [12][13]。
- **macOS [已证实]**：没有 ntsync，NT 同步对象走 wineserver 请求。在 `ntdll/unix/sync.c` 中，`inproc_device_fd < 0` 时所有 `inproc_*` 函数返回 `STATUS_NOT_IMPLEMENTED`，`NtWaitForMultipleObjects` 回退到 `server_wait()`，也就是一次 wineserver 往返 [12][13]。上游在 macOS 上**只有一条快路径**：`#elif defined(__APPLE__)` 定义 `USE_FUTEX`。`futex_wait` 在 macOS 14.4+ 用 `os_sync_wait_on_address(_with_timeout)`（`__builtin_available(macOS 14.4)`），更早版本用私有的 `__ulock_wait`/`__ulock_wake`（Marc-Aurel Zent，2024-07）。thread-ID alert（`union tid_alert_entry { LONG futex; }`）建在这条路径上，`RtlWaitOnAddress`、临界区和 SRW 锁又建在 thread-ID alert 上 [12]。**更正**：原稿称“thread-ID alert 用 Mach semaphore（2021）”是另一块优化，这是错的。2021 年的 Mach semaphore 实现已被 futex 路径取代，当前 `sync.c` 里没有 `semaphore_create/wait/signal` 调用 [12]。
- **树外方案**：msync（marzent/wine-msync，LGPL-2.1）用 Mach semaphore、port set、`__ulock_wait2` 加共享内存，环境变量 `WINEMSYNC=1`，有 msync-devel/staging/cx22/cx23 版本补丁，仓库最后推送是 2024-08 [38]。上游 `sync.c` 里没有任何 msync 或 Mach semaphore 代码。CrossOver 的 changelog 在 **23.7.0（2023-11-27）** 写着 “MSync included”，25.1.0（2025-08-12）有 “Fix for Steam downloads with msync enabled.”，说明 CrossOver 自 2023 年底起就在产品中下游集成 msync [52]（原稿把 25.1.0 的条目误引为 “Steam connection fixes with msync enabled”，并漏掉了 2023 年这个首次引入的时间点）。事实核查者无法在 GitLab 上检索是否有进行中的 macOS inproc 后端 MR（被 Anubis 拦截），网页搜索只找到 Linux ntsync inproc MR（!7985、!8435、!9091 等），所以“上游暂无 macOS inproc 后端”是**中高置信**，不是穷尽确认。WFUSync（Alien4042x）是一个实验性的纯用户态 macOS 后端，已在 Wine 11.15 上测试，许可证未声明 [39]。
- Proton 11 仍同时带 fsync 和 ntsync（“ntdll: Add local copy of linux/ntsync.h”、各种 `WINE_FSYNC_*` 游戏开关）[46]。
- 结论 [推断，高置信]：Cider 需要自研或移植一个“macOS inproc sync 后端”，接口对齐 Wine 11 的 inproc_sync 抽象。由于上游抽象基于 fd，而 Mach 原语不是 fd，需要设计 handle→共享内存/Mach port 的映射。上游 macOS 只有 futex（os_sync/ulock）这一条快路径可以复用，NT 对象等待全部要新做。CrossOver 的 msync 已在产品中跑了近三年（自 23.7.0 起），是最现成的参照实现。这是 CPU 侧性能的第一优先级。

### 6. PE 化、win32u、Media Foundation、HID、默认版本、Mono/Gecko

- **PE 化 [已证实]**：9.0 宣告完成 [4]。11.6 起 64 位 macOS 强制要求 PE 编译器；11.16 起非 PE 构建报错，除非显式 `--without-mingw`，而这个选项在 `x86_64-darwin`、`aarch64` 上不被允许 [8]。11.0 开始安装 `wine/unixlib.h`，供第三方模块使用 Unixlib，但官方说明仍是“work in progress”[2]。**开发机注意**：configure 要求 bison ≥ 3.0，本机 CLT 自带 `/usr/bin/bison` 2.3，必须另装 [8]。
- **win32u**：越来越多的 USER32 状态放进共享内存（10.8、11.11），GL/Vulkan client surface 和显示模式模拟（11.17；8 月的“Support OpenGL scaling according to emulated resolution”）都在 win32u 通用层实现，winemac 可以直接受益 [7][23]。
- **Media Foundation [已证实]**：默认后端是 winegstreamer（GStreamer）。10.0 引入 FFmpeg 后端 `winedmo`，属 opt-in，开关是 `HKCU\Software\Wine\MediaFoundation` 下 `DisableGstByteStreamHandler=1` [3]；11.12 内置 FFmpeg 8.1.1。Gcenx 的 macOS 包要求用户以全局方式安装 **GStreamer.framework 1.28.5** [36]。11.x 是否把 winedmo 设为默认：**未核实**。硬件解码方面，Linux 用 VA-API（11.16），D3D11 H.264 走 Vulkan Video（11.0）；macOS 上 VideoToolbox 没有上游实现 [推断]。
- **HID/控制器 [已证实]**：`winebus.sys` 有 SDL、UDEV、IOHID 三条总线，macOS 实际用 SDL + IOHID [17]。configure 检测的是 **SDL2**（`libSDL2`），不是 SDL3 [8]。相关注册表选项：`Enable SDL`、`DisableHidraw`、`Map Controllers`、`Split Controllers`、按 VID/PID 配置的 `EnableHidraw`。DualShock4、DualSense、Joy-Con 及飞行/赛车外设默认偏好 hidraw（在 macOS 上即 IOHID）。11.0 另有 Windows.Gaming.Input 配置页和力反馈改进 [2]。
- **默认 Windows 版本 [已证实]**：`version_init()` 设为 `VersionData[WIN10]`（10.0.19045）；可选 `win11`（build 22000）[16]。9.0 起新前缀默认 Win10 [4]。
- **Mono/Gecko**：见 1.3 表 [61]。Proton 11.0-1 用的是 Mono 11.0.0 [40]。

### 7. Wine-staging

- v11.18，`patches/` 下约 118 个补丁集。esync/fsync 已不在 staging 中 [37]。
- 与游戏/macOS 相关的补丁集：`winemac.drv-no-flicker-patch`（Bug 34166 Mac 全屏闪烁）、`dsound-EAX`、`dinput-joy-mappings`、`dinput-scancode`、`user32-rawinput-keyboard`、`wined3d-SWVP-shaders`、`wined3d-Indexed_Vertex_Blending`、`vkd3d-latest`、`ntdll-WRITECOPY`（Voobly/MSYS2）、`ntdll-Hide_Wine_Exports`（部分反作弊/DRM 会检测 Wine 导出）、`xactengine3_7-PrepareWave`、`d3dx9_36_*`。**默认禁用**的有：`nvapi-Stub_DLL`、`ntdll-ForceBottomUpAlloc`、`mfplat-streaming-support`。
- 建议：以 upstream devel 为基线，只挑选少量 staging 补丁，不整体套用，以降低回归面 [推断]。

### 8. Valve Proton 10/11：结构与可移植性

**8.1 版本 [已证实][40][41][42]**

| 版本 | 日期 | Wine 基线 / 组件 |
|---|---|---|
| 10.0-1b（beta） | 2025-04-29 | wine-10.0，Steamworks SDK 1.62 |
| 10.0-3 / 10.0-4 | 2025-11-13 / 2026-01-26 | — |
| **11.0-1** | **2026-07-07**（GitHub release `published_at` 2026-07-07T20:55:03Z，非 prerelease；tag 创建于 2026-06-18）[68] | wine-11.0；FEX-2605（ARM64EC）；vkd3d 1.19-139；dxvk v2.7.1-467（proton-11 分支）；dxvk-nvapi v0.9.1；Wine Mono 11.0.0；vkd3d-proton proton-20260410 |
| 11.0-1b / 10.0-4b | 日期未核实 | Steamworks SDK 1.65 支持 |
| 11.0-2 | 2026-08-21 | 修复 11 系回归，SteamWorks 1.64 支持在 11.0-1 |

**8.2 结构**：`.gitmodules` 包含 wine（Valve fork）、dxvk、vkd3d-proton、dxvk-nvapi、vkd3d、FEX、gstreamer/gst-plugins-rs/ffmpeg/dav1d、OpenXR/openvr、Vulkan-Loader、glslang、piper/vosk（TTS/语音），外加 `lsteamclient`、`steam_helper`、`vrclient_x64`、`wineopenxr` 以及 Python 启动脚本 `proton`；同时提供 `toolmanifest_arm64.vdf` [43]。许可：顶层是 BSD-3（`LICENSE.proton`），各子项目沿用各自许可（Wine fork 为 LGPL-2.1+）[45][57]。

**8.3 Wine fork 的差量**：GitHub compare API 显示 `proton_11.0` 相对 wine-11.0（提交 `db11d0fe6a16`）**ahead 1453、behind 0**（2026-09-26）[46][69]。对其中 753 条抽样分类：ntdll 69、win32u 31、mf/topology_loader 28、winegstreamer/media-converter 26、winegstreamer 25、kernelbase 21、mf 21、winex11(+drv) 28、fshack 13、fsync 5 等；标题含 “HACK” 的有 232 条。

**8.4 可移植性判断**

| 类别 | 例子 | 对 macOS 的可移植性 |
|---|---|---|
| 按游戏的 ntdll/kernelbase HACK | `WINESTEAMNOEXEC` for Mafia II、heap 标志 | **高**（与平台无关，按 exe/AppID 触发） |
| mscoree/.NET HACK | Bannerlord、Karmaflow | 高 |
| MF / topology loader / wm_reader 修复 | Persona 4 Golden、KiriKiri 系 | 中高（依赖 winegstreamer 行为，需实测） |
| dinput 映射、winebus HACK | Logitech G920 映射 | 中（底层从 udev 换成 IOHID/SDL） |
| winex11、fshack、Wayland 相关 | WM_CLASS、fullscreen hack | **不可移植**（macOS 用 win32u 显示模拟替代） |
| fsync / ntsync | 各种 `WINE_FSYNC_*` | 不可移植（需 macOS 同步后端） |
| Steam 桥接 | lsteamclient、`HACK: steam` | 不可移植（macOS 上 CrossOver 跑的是 Windows 版 Steam） |
| media-converter（fozdb 转码） | protonvideoconv | 不需要（Valve 的离线转码体系） |
| `proton` 脚本 `default_compat_config` | 按 AppID 设置 `nomfdxgiman`、`noopwr`、`heapzeromemory`、`hidenvgpu` 等 | **高**（BSD-3，可转成 Cider 的数据库字段）[44] |

**8.5 umu / protonfixes [已证实]**（三个仓库的许可证均经 GitHub license API 核实 [70]）
- umu-launcher（GPL-3.0，只支持 Linux，基于 Steam Linux Runtime/pressure-vessel），用 `GAMEID`、`STORE`、`PROTONPATH`、`WINEPREFIX` 驱动 [47]。
- umu-protonfixes（**BSD-2-Clause**，版权 2018 Chris Simons）：按商店分目录（steam、gog、egs、battlenet、ea、ubisoft、humble、amazon、itchio、zoomplatform、umu），每个修复是一个带 `main()` 的 Python 模块。工具函数包括 `protontricks()`（winetricks verb）、`set_environment()`、`append_argument()`、`winedll_override()`、`regedit_add()`、`set_dxvk_option()`、`disable_nvapi()`、`disable_ntsync()`、`set_cpu_topology_limit()`、`install_eac_runtime()` 等 [48]。
- umu-database（**GPL-3.0**）是一个 CSV，约 1,200 行，把各商店 ID 映射到 `umu-<SteamID>` [49]。
- 对 Cider 的含义：protonfixes 的逻辑和数据可以直接吸收，按 BSD-2 保留版权声明；umu-database 只能作为独立数据文件分发，并随附 GPL-3 文本，不能与闭源部分混编 [推断，需律师确认]。

### 9. 上游 macOS 支持的健康度

- **贡献者**：CodeWeavers 员工（Brendan Shanks、Tim Clem、Rémi Bernon、Zhiyi Zhang、Jactry Zeng、Huw Davies）、Marc-Aurel Zent（IME、Metal、同步、写监视）、Dean M Greer（Gcenx，负责打包与文档），以及少量社区作者 [7][26]。
- **CI**：`build-mac` 在 Tart VM 上运行（MR 用 `winehq-sequoia-pristine`，每日构建用 `winehq-sonoma-pristine`），ARM runner 上用 `arch -x86_64` 加 Xcode SDK；`build-mac` 只在 `merge_request_event` 触发，只构建，`make install-lib install-test` 装好的测试二进制从不运行 [21]。`tools/gitlab/test.yml` 只有 linux 和 win10 两类测试 job [67]。test.winehq.org 的列只有 Win8…Win11 和 Linux，近期整体失败率约 4.4–4.8% [23]。
- **macOS 15/26 相关回归与问题**：Bug 59595（macOS 26 上 `wineboot --init` 卡在 `rundll32 setupapi` 的可见窗口，UNCONFIRMED）[34]；#58008（wine-10.4 在 Rosetta 2 上挂起，10.5 修复）；#58816（窗口焦点/激活，10.18）；macOS 26 隐藏光标导致的帧率问题（!11799/!11880）；CrossOver 25.1.1（2025-09-15）有 “macOS Intel Tahoe fix”，26.0 做了 Tahoe UI 调整 [52]。
- **仍开放的 macOS 性能 MR**：!9090 用 Mach COW 实现写监视（M2 Max + Rosetta 实测：平均页写入 371ns，fallback 为 66,202ns；代价是 `GetWriteWatch` 变慢到约 12.6ms）[30]；!9857 用 `TASK_VM_INFO.internal` 加 swapped 重写 `fill_vm_counters`，已在 macOS 26.2 测试 [31]。
- **分发**：Homebrew 的 wine-stable/devel/staging 以及 gstreamer-runtime cask 因“does not pass the macOS Gatekeeper check”于 2026-09-01 被禁用 [35]，但 WineHQ 的 MacOS wiki 仍推荐 `brew install --cask --no-quarantine` [25]。`Casks/w/wine-stable.rb`（版本 `11.0_1`，Gcenx osx64 tarball）里写着 `disable! date: "2026-09-01", because: :fails_gatekeeper_check`。Bug 58946 “Homebrew packages disabled as of Q3 2026” 创建于 2025-11-10，状态 UNCONFIRMED [35][73]。注意 `11.0_1` 是 Gcenx 打包修订号，不是上游的 wine-11.0.1 tag。
- **CrossOver 源码**：`crossover-sources-26.0.0`、`26.1.0`、`26.2.0`、`26.3.0.tar.gz` 均可获取（HEAD 请求约 149MB，最后修改时间与发布日一致）。这是获取 CodeWeavers 下游 macOS 补丁的合法渠道（LGPL）[56]。

### 10. Wine 测试套件用于 Cider CI

- `winetest.exe` 由上游每日构建产出（`build-daily-winetest` 生成 winetest.exe/winetest64.exe）[21]。常用参数：`-c`（控制台）、`-q`、`-o FILE`（只输出报告，不提交）、**`-J FILE`（JUnit XML）**、`-t TAG`、`-n`（排除指定测试）、`-w SECS`（单测超时，默认 120）、`-x DIR`（仅解包）[22]。
- 构建树内可用 `make test` 或按模块 `make -C dlls/<mod>/tests test`；上游设置了 `RUNTESTFLAGS="-q -P wine"` [8]。
- 建议：Cider 自建 macOS CI（Apple Silicon 自托管 runner，或 Tart），每次提交跑一个子集：ntdll、kernel32、user32、win32u、d3d8/9/10/11、dxgi、opengl32、vulkan、mfplat、dinput、xinput、winmm、dsound，分别在 64 位和 WoW64 下运行。输出 `-J` 结果与 test.winehq.org 的 Linux 基线比对（只看新增失败）。**也可以匿名 tag 提交结果，补上上游缺失的 macOS 列**（需评估隐私后决定是否提交）[推断]。

## 对 Cider 的启示与建议（按优先级）

**P0（立项即做）**
1. **基线策略**：以 Wine upstream devel（11.x，每两周 rebase）为主干，维护一个像 Proton 那样的“Cider 补丁队列”分支，每个补丁标注 upstream 状态（MR 号 / 树外 / Cider 专有）。参考源依次为：①上游开放 MR（!11058、!11538、!11799、!9090、Bug 60263/60262 附带的补丁）；②CrossOver 源码 tarball（LGPL，详见 02 报告）；③Proton fork 中可移植的 HACK；④少量 staging 补丁。坚持 upstream-first，否则长期 rebase 成本会失控。
2. **运行时架构双轨**：v1 使用 x86_64 Wine（`--enable-archs=i386,x86_64`，新 WoW64）+ Rosetta 2，覆盖 macOS 26/27。同时立刻开启 v2 预研：aarch64-darwin Wine + ARM64EC/WoW64 + x86 模拟器，逐项验证 !11638 的三个阻碍，以及 16K 宿主页下 4K 页模拟的兼容性。在 Rosetta 2 于 macOS 28 缩减之前（约 2027 年秋，参照 Apple 年度节奏 [推断]）必须有可用的 v2。**存疑**：有二手报道称 Apple 另有一篇文章把最后支持 Rosetta 的版本写成 macOS 26 [72]，所以计划要按“可能更早”的最坏情况留缓冲，并把跟踪 Apple 措辞（开发者新闻、支持文章 102527）列为固定任务。
3. **macOS 同步后端**：在 Wine 11 的 inproc_sync 抽象下实现 “macsync”，可移植 msync（LGPL-2.1）或参考 WFUSync，并用 `WINEMSYNC=1` 式开关灰度发布。上游 macOS 只有 futex（os_sync/ulock）一条快路径，没有可复用的 Mach semaphore 代码。CrossOver 自 23.7.0（2023-11）起就在产品中内置 msync，所以移植时优先比对 crossover-sources 里的 msync 版本（及其 Steam 下载等修复），再对照 marzent 的 cx23 补丁。性能基准要覆盖多线程引擎的等待密集场景。
4. **winemac 补齐**：Metal layer ExtEscape（DXMT/D3DMetal 集成）、跨进程 Metal 子窗口（Steam/CEF/EA App/Epic）、无边框窗口标题栏、GCMouse 原始输入、透明光标（macOS 26 帧率）、Vulkan portability enumeration。这些是“能用”和“好用”的分水岭。
5. **构建与签名**：开发机需要安装 Rosetta 2、x86_64 版依赖（bison ≥ 3.0、pkg-config、MoltenVK、SDL2、freetype、gnutls、FFmpeg/GStreamer），以及 PE 交叉编译器（llvm-mingw 或 mingw-w64，11.6 起强制）。当前 CLT 的 bison 2.3 不满足要求。configure 的 `-Wl,-no_huge` 需要 Xcode 15.3+ 的链接器，用 CLT 需先验证。产品从第一天起就要做 Developer ID 签名、Hardened Runtime 例外（JIT/未签名可执行内存）和公证，避免重蹈 Homebrew 下架的覆辙。

**P1（首个公开版本前）**
6. **游戏修复数据库**：以 umu-protonfixes 的模型为蓝本，设计 Cider 的声明式 schema（env、DLL override、winetricks verb、注册表、启动参数、同步/图形后端选择），导入 Proton `default_compat_config` 的 AppID 列表（BSD-3）和 protonfixes（BSD-2）。umu-database（GPL-3）作为独立下载数据使用。
7. **媒体栈**：评估用 LGPL 配置的 FFmpeg（winedmo）替代要求用户安装 GStreamer.framework 的方案；准备一套 MF 视频游戏回归集（Proton 11 changelog 中的视频修复可作为清单）。
8. **CI**：按第 10 节建设 winetest 与游戏冒烟测试（启动、渲染一帧、截图对比）；覆盖新 WoW64 32 位路径（上游完全没有覆盖这部分）。
9. **32 位游戏**：32 位 D3D8/9/10/11 默认路由到 DXVK/DXMT/D3DMetal，不走 wined3d-GL，以规避 WoW64 GL 拷贝路径；纯 GL 的 32 位游戏标为“性能受限”。

**P2（持续）**
10. 合并或推进 !9090（写监视，对 .NET/Unity GC 类负载有意义）、!9857（内存计数）、!10523（GPU ID）。跟踪 vkd3d-shader 的 MSL 目标与 wined3d Vulkan 渲染器，把它们作为未来的“纯开源 Metal 路径”。MSL 目标从 vkd3d 1.14 起就存在，但要用 `-DVKD3D_SHADER_UNSUPPORTED_MSL` 构建才启用。Cider 可以现在就在内部实验构建里打开它，跑着色器覆盖率测试，不必等上游默认启用。
11. 向上游回馈 macOS 修复和 CI 结果，争取让 Cider 成为 macOS 上游的事实测试者，从而降低自身补丁队列的规模。

## 风险

- **Rosetta 退场**：macOS 27 之后只保留“老游戏子集”，x86_64 Wine 进程是否在支持范围内未知；另有二手报道称 Apple 的另一篇文章写的是 macOS 26 为最后版本，时间点**存疑**，可能更早 [72]；而 ARM64 路径在上游尚不可用，FEX 也没有官方 macOS 宿主支持（CodeWeavers 用的是定制版）。
- **ARM64 macOS 的结构性障碍**：16K 页（4K 模拟只适用于“simple applications”）、x18、page zero、`KUSER_SHARED_DATA`，可能需要长期维护深度 ntdll 补丁。
- **上游 macOS 资源薄弱**：没有 Mac 驱动维护者，CI 不跑测试也不构建 i386；CodeWeavers 的部分修复先进 CrossOver，Cider 从上游拿到修复会有滞后。
- **win32u 重构的波动**：GL/Vulkan client surface 仍在频繁改动（11.18 修复了多个回归，如 #60290/#60327/#60337），双周 rebase 会带来回归。
- **私有 API**：`CALayerHost`/`CAContext`/`CGSMainConnectionID`、`__ulock_*` 可能随 macOS 更新失效，并排除上架 App Store 的可能。
- **许可**：GPL-3 的 umu-launcher/umu-database 不能与闭源组件混编；Wine 与 vkd3d-proton 的 LGPL 要求提供修改后的源码；D3DMetal 的专有许可见 05 报告。
- **反作弊/DRM**：syscall 编号对齐和 `ntdll-Hide_Wine_Exports` 只能缓解，内核级反作弊不可解。
- **开发机限制**：8GB 内存同时构建 Wine（双架构 PE 加 Unix）和跑游戏会很吃紧，建议用 ccache 并控制 `-j` 并行度。

## 未解问题

1. CodeWeavers 定制的 macOS 版 FEX 是否开源、是否会上游到 FEX-Emu？（FEX 为 MIT 许可，没有公开义务。）
2. Apple 在 macOS 28 保留的“游戏用 Rosetta 子集”技术边界是什么？是否覆盖 Wine/CrossOver 这类非游戏主程序？Apple 2026-09-01 的开发者新闻和支持文章 102527 写的是“macOS 27 最后”，据 MacObserver 报道 2026-09-09 的文章却写“macOS 26 最后”，以哪一份为准？
3. 在新 WoW64、Rosetta 2、macOS 26 组合下，16 位程序（LDT）能否工作？
4. winedmo（FFmpeg）是否会在 Wine 12.0（预计 2027-01）成为默认 MF 后端？
5. Wine 11.0.x 稳定维护版本是否存在、由谁发布：wine/wine 的 `stable` 分支仍停在 11.0 发布提交，未找到 11.0.1 tag。Homebrew/Gcenx 的 `11.0_1` 是打包修订号，不是上游 11.0.1。
6. crossover-sources-26.x 中 msync 与 winemac 补丁的具体范围？需下载源码比对（本次按规则未下载，见 02 报告）。
7. !11058 与 Bug 60263 的补丁是否会被上游接受？私有 CoreAnimation API 在上游的接受度如何？

## 参考来源

1. https://www.winehq.org/news/2026011301 — WineHQ 新闻：Wine 11.0 发布（页面返回 403，经搜索结果确认）
2. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — Wine 11.0 完整发布说明（WoW64、NTSync、macOS `%gs`、图形、组件版本）
3. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.0/ANNOUNCE.md — Wine 10.0 完整发布说明（ARM64EC、macOS preloader、syscall 仿真、FFmpeg 后端）
4. https://raw.githubusercontent.com/wine-mirror/wine/wine-9.0/ANNOUNCE.md — Wine 9.0 说明（新 WoW64 引入、PE 化完成、默认 Win10）
5. https://raw.githubusercontent.com/wine-mirror/wine/master/ANNOUNCE.md — Wine 11.18 发布说明与变更日志
6. https://github.com/wine-mirror/wine/tags — tag 对象日期（经 GitHub API `git/tags` 读取：10.0=2025-01-21、11.0=2026-01-13、11.18=2026-09-18）
7. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.N/ANNOUNCE.md（N=1…18）及 wine-10.N — 各开发版要点与逐条提交（用于 winemac/ARM64/WoW64 统计）
8. https://raw.githubusercontent.com/wine-mirror/wine/master/configure.ac — darwin 分支、`--enable-archs`、MoltenVK SONAME、bison/flex 版本要求、SDL2
9. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/vulkan.c — `VK_EXT_metal_surface`/`VK_MVK_macos_surface` 逻辑
10. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/win32u/vulkan.c — MoltenVK `VK_SUBOPTIMAL_KHR` 规避、present scaling
11. https://github.com/wine-mirror/wine/commit/1a63b0d7c431 — “winemac: Implement cross-process MetalViewSwapChain via CALayerHost”（作者日期 2026-05-20，见 [77]）
12. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/sync.c — ntsync ioctl、`inproc_*` 回退 `server_wait()`、macOS `USE_FUTEX`（`os_sync_wait_on_address`/`__ulock_wait`）、`tid_alert_entry`（无 Mach semaphore）
13. https://raw.githubusercontent.com/wine-mirror/wine/master/server/inproc_sync.c — inproc sync 服务器端（`/dev/ntsync`，非 ntsync 时为 stub）
14. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/opengl32/unix_wgl.c — WoW64 GL buffer 映射需 `GL_EXT_memory_object_fd`，否则走拷贝
15. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/wined3d/wined3d_main.c — 默认渲染器 AUTO→OpenGL
16. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/version.c — 默认 WIN10（19045），WIN11=22000
17. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winebus.sys/main.c — SDL/UDEV/IOHID 总线与注册表选项
18. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/win32u/syscall.c — `aarch64 && __APPLE__` 栈 ABI 包装
19. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/signal_arm64.c — macOS arm64 信号上下文分支
20. https://raw.githubusercontent.com/wine-mirror/wine/master/MAINTAINERS — 无 Mac 驱动条目；各子系统维护者
21. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/gitlab/build.yml 与 …/tools/gitlab/build-mac — macOS CI 仅构建（`--enable-win64`，`arch -x86_64`）
22. https://raw.githubusercontent.com/wine-mirror/wine/master/programs/winetest/main.c — winetest 参数（含 `-J` JUnit）
23. https://test.winehq.org/data/ — 测试结果矩阵（无 macOS 列，失败率约 4.5%）
24. https://gitlab.winehq.org/wine/wine/-/wikis/MacOS-Building — macOS 构建指南（“not ready yet to be build for ARM”）
25. https://gitlab.winehq.org/wine/wine/-/wikis/MacOS — macOS 安装说明（Catalina 10.15.4+、Rosetta2、brew cask）
26. https://gitlab.winehq.org/wine/wine/-/merge_requests/11058 — winemac Metal layer ExtEscape（DXMT）；另经 GitLab API 统计 winemac/ARM64EC MR
27. https://gitlab.winehq.org/wine/wine/-/merge_requests/11799 （及 !11880、!11999）— GCMouse 原始输入、macOS 26 隐藏光标帧率问题
28. https://gitlab.winehq.org/wine/wine/-/merge_requests/11538 — Vulkan portability enumeration（MoltenVK 经 loader）
29. https://gitlab.winehq.org/wine/wine/-/merge_requests/11638 — 社区 Apple Silicon 原生支持尝试及其阻碍说明
30. https://gitlab.winehq.org/wine/wine/-/merge_requests/9090 — Mach COW 写监视及基准数据
31. https://gitlab.winehq.org/wine/wine/-/merge_requests/9857 （及 !10523）— macOS VM 计数与 GPU ID
32. https://bugs.winehq.org/show_bug.cgi?id=60263 — 跨进程子窗口 Metal swapchain 未实现（Steam 黑屏）
33. https://bugs.winehq.org/show_bug.cgi?id=60262 — 无边框窗口标题栏与输入错位
34. https://bugs.winehq.org/show_bug.cgi?id=59595 — macOS 26 上 wineboot 卡死
35. https://bugs.winehq.org/show_bug.cgi?id=58946 — Homebrew wine cask 于 2026-09-01 被禁用（Gatekeeper）
36. https://github.com/Gcenx/macOS_Wine_builds/releases — 官方 macOS 包：11.18（2026-09-25）、gecko 2.47.4、mono 11.3.0、GStreamer 1.28.5、`--enable-archs=i386,x86_64`
37. https://github.com/wine-staging/wine-staging/tree/master/patches — staging v11.18 补丁集及 definition 文件
38. https://github.com/marzent/wine-msync — msync（LGPL-2.1，`WINEMSYNC=1`）
39. https://github.com/Alien4042x/Wine-NTsync-Userspace-macOS-backend — WFUSync 实验后端
40. https://github.com/ValveSoftware/Proton/releases/tag/proton-11.0-1 — Proton 11.0-1 组件清单（2026-07-07）
41. https://github.com/ValveSoftware/Proton/releases/tag/proton-11.0-2 — Proton 11.0-2（2026-08-21）
42. https://github.com/ValveSoftware/Proton/releases/tag/proton-10.0-1b — Proton 10 基于 wine-10.0（2025-04-29）
43. https://github.com/ValveSoftware/Proton/blob/proton_11.0/.gitmodules — Proton 子模块结构
44. https://github.com/ValveSoftware/Proton/blob/proton_11.0/proton — `default_compat_config` 按 AppID 的标志
45. https://github.com/ValveSoftware/Proton/blob/proton_11.0/LICENSE.proton — Proton 顶层 BSD-3
46. https://github.com/ValveSoftware/wine/tree/proton_11.0 — Valve Wine fork（compare API：ahead 1453 / behind 0）
47. https://github.com/Open-Wine-Components/umu-launcher — umu-launcher（GPL-3.0，仅 Linux）
48. https://github.com/Open-Wine-Components/umu-protonfixes — protonfixes（BSD-2-Clause）与 `util.py` API
49. https://github.com/Open-Wine-Components/umu-database — umu-database CSV（GPL-3.0，约 1,200 行）
50. https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac — Mac ARM64 CrossOver Preview（被 Cloudflare 拦截，内容来自搜索摘要）
51. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — ARM64 Preview 的限制与 CrossOver 27 时间
52. https://www.codeweavers.com/crossover/changelog — CrossOver 变更日志（23.7.0 “MSync included”，2023-11-27；25.1.0 “Fix for Steam downloads with msync enabled.”，2025-08-12；Tahoe 修复、组件版本）
53. https://developer.apple.com/news/?id=w5ngl9k2 — Apple：Rosetta 支持变更（macOS 27 为最后版本）
54. https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/ — WWDC25 Rosetta 退场表述（搜索摘要）
55. https://gitlab.winehq.org/wine/vkd3d/-/raw/vkd3d-2.0/ANNOUNCE — vkd3d 2.0 说明（MSL 目标的后续改进；该目标始于 1.14，见 [65]）
56. https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz — CrossOver 源码 tarball（仅 HEAD 确认存在，约 149MB）
57. https://raw.githubusercontent.com/wine-mirror/wine/master/LICENSE — Wine：LGPL-2.1-or-later
58. https://lwn.net/Articles/1055001/ — LWN 转载的 Wine 11.0 发布公告
59. https://www.phoronix.com/news/CrossOver-26 — CrossOver 26 基于 Wine 11.0（搜索摘要）
60. https://github.com/FEX-Emu/FEX — FEX（MIT；发布说明中没有 macOS 宿主支持）
61. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/appwiz.cpl/addons.c — `GECKO_VERSION` 与 `MONO_VERSION`（另查 wine-11.0、wine-10.0 tag）
62. https://api.github.com/repos/wine-mirror/wine/git/tags/ce295733f9a67970b7f60d7af201f2ac16441a50 与 https://api.github.com/repos/wine-mirror/wine/git/tags/ccf4f04c3d4b58856e9e6db6e202409622420d5e — wine-11.0 / wine-11.18 的 tag 对象（tagger 日期 2026-01-13T15:56:08Z / 2026-09-18T20:36:23Z）
63. https://raw.githubusercontent.com/Gcenx/macOS_Wine_builds/master/README.md — Gcenx 官方 macOS 包 README（`--enable-archs=i386,x86_64`）
64. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.16/ANNOUNCE.md — Wine 10.16（“Fast synchronization support using NTSync”）
65. https://gitlab.winehq.org/wine/vkd3d/-/raw/vkd3d-1.14/ANNOUNCE — vkd3d 1.14：首次加入 MSL 输出（`VKD3D_SHADER_TARGET_MSL`，需 `-DVKD3D_SHADER_UNSUPPORTED_MSL`）
66. https://gitlab.winehq.org/wine/vkd3d/-/raw/vkd3d-1.15/ANNOUNCE — vkd3d 1.15：更多 MSL 指令
67. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/gitlab/test.yml — 上游 CI 测试 job（仅 linux 与 win10）
68. https://api.github.com/repos/ValveSoftware/Proton/releases/tags/proton-11.0-1 — Proton 11.0-1 发布元数据（published_at 2026-07-07T20:55:03Z）
69. https://api.github.com/repos/ValveSoftware/wine/compare/db11d0fe6a169c457e23d007e20404643d067aa8...proton_11.0 — Valve Wine fork 相对 wine-11.0 的 compare（ahead 1453 / behind 0）
70. https://api.github.com/repos/Open-Wine-Components/umu-protonfixes/license 、https://api.github.com/repos/Open-Wine-Components/umu-launcher/license 、https://api.github.com/repos/Open-Wine-Components/umu-database/license — 许可证（BSD-2-Clause / GPL-3.0 / GPL-3.0）
71. https://support.apple.com/en-us/102527 — Apple 支持文章：macOS 28 起 Rosetta 仅用于部分老旧游戏
72. https://www.macobserver.com/news/apple-rosetta-end-date-contradiction-macos-26-27-28/ — MacObserver（2026-09-12）：Apple 关于 Rosetta 终止版本的表述矛盾（二手）
73. https://raw.githubusercontent.com/Homebrew/homebrew-cask/master/Casks/w/wine-stable.rb — wine-stable cask（`11.0_1`，`disable! date: "2026-09-01", because: :fails_gatekeeper_check`）
74. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/dlls/appwiz.cpl/addons.c — wine-11.0 的 `MONO_VERSION "10.4.1"`、`GECKO_VERSION "2.47.4"`
75. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.3/ANNOUNCE.md — Wine 11.3（Mono 11.0.0、vkd3d 1.19）
76. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.12/ANNOUNCE.md — Wine 11.12（FFmpeg 8.1.1、Mono 11.2.0）
77. https://api.github.com/repos/wine-mirror/wine/commits/1a63b0d7c431 — CALayerHost 提交元数据（作者日期 2026-05-20）

## 事实核查记录

以下为独立事实核查的结论（2026-09-26），已据此修订正文。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| Wine 11.0 于 2026-01-13 发布（tag wine-11.0），新 WoW64 “fully supported”，移除 wine64、弃用 `WINEARCH=win32`；最新开发版 11.18（2026-09-18） | 证实 | tag 对象 tagger 日期 2026-01-13T15:56:08Z（提交 db11d0fe6a16）；11.18 由 Julliard 于 2026-09-18T20:36:23Z 打 tag，`wine-11.19` 返回 404。补充：新 WoW64 仍不是 configure 默认，x86_64-darwin 需 `--enable-archs=i386,x86_64`（Gcenx README 同）。已在摘要与 §1.1 补充 [1][2][5][62][63] |
| NTSync 仅 Linux（/dev/ntsync，内核 ≥ 6.14）；macOS 上 NT 对象等待仍走 wineserver，只有 futex 路径用 os_sync/ulock；msync 不在上游 | 证实 | `inproc_*` 在无 ntsync 设备时返回 `STATUS_NOT_IMPLEMENTED`，回退 `server_wait()`；msync 用 Mach semaphore、port set、`__ulock_wait2`，`WINEMSYNC=1`，另有 msync-devel 补丁。GitLab MR 检索被 Anubis 拦截，未能穷尽确认是否有进行中的 macOS inproc 后端。已更新 §5 [12][13][38][64] |
| wined3d 默认 OpenGL，Vulkan 渲染器“not yet at parity/default”；winemac 优先 `VK_EXT_metal_surface`，否则 `VK_MVK_macos_surface`；configure 回退 MoltenVK 作 `SONAME_LIBVULKAN` | 证实 | 补充：若找到 vulkan-loader 的 libvulkan 则优先使用，此时 MoltenVK 需 portability enumeration（!11538，Dean M Greer，2026-07-31，仍开放）。已在 §4.4 加入打包二选一建议 [8][9][15][28] |
| 上游 GitLab CI 的 macOS job 只构建（`--enable-win64 --with-mingw`，`arch -x86_64`，Tart VM），不构建 i386、不跑测试；test.winehq.org 无 macOS 列 | 证实 | 补充：test.yml 只有 linux 与 win10 job；测试二进制只作为 artifact 安装、从不运行；winetest 支持 `-J`（JUnit）。已更新 §2、§9 [21][22][23][67] |
| Proton 11.0-1（2026-07-07）基于 Wine 11.0，含 FEX-2605、dxvk 2.7.1-467、vkd3d-proton proton-20260410、dxvk-nvapi 0.9.1、Mono 11.0.0；proton_11.0 ahead 1453；umu-protonfixes BSD-2，umu-launcher/umu-database GPL-3 | 证实 | published_at 2026-07-07T20:55:03Z，tag 创建于 2026-06-18。另有 11.0-1b、10.0-4b（Steamworks SDK 1.65，日期未核实）与 11.0-2（2026-08-21）。已补入 §8.1 表 [40][68][69][70] |
| macOS 27 是最后一个通用支持 Rosetta 的版本；上游 Wine 尚不能在 arm64 macOS 原生构建/使用（wiki、!11638）；CodeWeavers 2026-07-31 的 ARM64 Preview 用定制 FEX、无 D3DMetal | 证实（Rosetta 时间点**存疑**） | Apple 开发者新闻（2026-09-01）与支持文章 102527 一致，但 MacObserver（2026-09-12）报道 Apple 2026-09-09 的一篇文章写“macOS 26 为最后支持 Rosetta 的版本”。判断：以两份一手来源为主线，按可能更早收缩做预案。上游已有部分 arm64-macOS 代码（11.1/11.13/11.16），“不可原生使用”仍准确。已更新摘要、§3、P0-2、风险、未解问题 2 [29][50][51][53][71][72] |
| §5：除 futex 路径外，上游第二块 macOS 同步优化是 thread-ID alert 用 Mach semaphore（2021） | **推翻** | 更正：macOS 上 thread-ID alert 同样走 `USE_FUTEX`（ulock/os_sync）路径，`RtlWaitOnAddress`、临界区、SRW 也建在其上；当前 `sync.c` 中已无 Mach semaphore 代码。上游在 macOS 只有一条快路径。已改写 §5 与摘要，并更新 P0-3 [12] |
| §4.4 与摘要：vkd3d-shader 自 vkd3d 2.0 起有实验性 MSL 目标 | **推翻** | 更正：MSL 目标自 vkd3d 1.14（2024 年底）起就有，需用 `-DVKD3D_SHADER_UNSUPPORTED_MSL` 构建才启用，默认关闭；1.15 起持续完善至 2.x。已改写摘要、§4.4，并更新 P2-10 [65][66] |
| §5：CrossOver 25.1.0（2025-08-12）changelog 提到 “Steam connection fixes with msync enabled”，说明 CrossOver 使用 msync | 部分属实 | 结论成立但引文不准：25.1.0 原文为 “Fix for Steam downloads with msync enabled.”；更有力的证据是 23.7.0（2023-11-27）“MSync included”。已更正 §5、摘要、P0-3 与参考 [52] |
| §9：Homebrew 的 wine-stable/devel/staging cask 于 2026-09-01 因 Gatekeeper 检查失败被禁用（Bug 58946） | 证实 | cask 版本为 `11.0_1`（Gcenx 打包修订号，非上游 11.0.1）；Bug 58946 创建于 2025-11-10，状态 UNCONFIRMED。已补入 §9 与未解问题 5 [35][73] |
| §1.3/§4.2：11.0 带 vkd3d 1.18、mono 10.4.1；master mono 11.3.0；vkd3d 1.19 于 11.3、2.0 于 11.10、FFmpeg 8.1.1 于 11.12、vkd3d 2.1 于 11.17；CALayerHost 提交日期 2026-06-01 | 部分属实 | 版本号均正确，但时间线漏了 Mono 升级：11.3 → 11.0.0、11.8 → 11.1.0、11.12 → 11.2.0、11.16 → 11.3.0。CALayerHost 提交（1a63b0d7c431）作者日期为 2026-05-20，2026-06-01 至多为提交/合并日期。已更新 §1.1、§1.3、§4.2 与参考 [11][61][74][75][76][77] |
