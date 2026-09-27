# CPU 转译：Rosetta 2 能力与退役时间线、FEX / Box64 / ARM64EC / Hangover 替代路径

> 调研日期 2026-09-26 · 调研机器：Apple M3 / macOS 26.5 (25F71) / 内核 xnu-12377.121.6~2 (T8122)，未安装 Rosetta。
> 置信度说明：**[已证实]** = 有一手来源（Apple / Microsoft 官方文档、源码、SDK 头文件，或在本机做过实验）；**[较可信]** = 可信的二手报道或多个来源互相印证；**[推断]** = 根据证据得出的工程判断，需要原型验证；**[未证实]** = 只见于单一第三方或无法核实。CodeWeavers 官网（codeweavers.com）和 winehq.org 对抓取返回 403，所以 CodeWeavers 博客内容是通过搜索摘要和 AppleInsider 等转述拿到的，文中已标出。Apple 文档页面在客户端渲染，核查时通过其 JSON 端点（`developer.apple.com/tutorials/data/documentation/...json`）读取原文。
> 事实核查：2026-09-26 经两轮独立核查。更正和补充已写入正文，逐条结论见文末「事实核查记录」。

---

## 摘要

- **Rosetta 的时间线已定**：Apple 官方文档写明，Rosetta 作为通用工具提供到 **macOS 27** 为止；之后只保留一个"子集"，**面向依赖 Intel 框架、已无人维护的老游戏**[1][2][3][4]。macOS 27 "Golden Gate" 已于 **2026-09-14** 发布[5]。升级时会**卸载已装的 Rosetta**，但可以重装[5][6]；这一点只见于 MacRumors 等二手报道，没有找到 Apple 原文 **[较可信]**。**macOS 28（预计 2027 年秋）起，通用 Rosetta 不再提供。** Apple 的各个页面都没有说明游戏如何进入子集，Wine / CrossOver 这类用法是否被覆盖也**没有说明**。Cider 不能指望它。
- **CrossOver 已经在转向**：CodeWeavers 在 2026-06-11 宣布，CrossOver 27（计划 2027 年初发布）**只支持 Apple silicon，要求 macOS Sonoma 及以上，不再支持 32 位 bottle**[20]。2026-07-31 推出的 Mac 版 ARM64 Preview 采用"**原生 arm64 Wine + ARM64EC + CodeWeavers 自己移植到 macOS 的 FEX**"方案，**ARM64 部分要求 macOS 26.5+**，更低版本仍跑 Intel Wine + Rosetta；该预览版不含 D3DMetal[21][43]。其中"ARM64EC"没有任何可读页面明说，是根据"Wine 在 ARM64 上模拟 x86-64 只能走 ARM64EC"和 CodeWeavers 时间线得出的**强推断**。
- **Apple 在 macOS 26.x 里悄悄加了一组"兼容层专用"的底层接口**（本机 SDK 和内核验证）[38][39][40]：
  - `posix_spawnattr_set_4k_page_size_np()`，从 macOS 26.0 起可用，作用是强制 4K 地址空间；
  - `os_set_custom_x18_abi_enabled()`，从 macOS 26.4 起可用，允许线程把 x18 当普通寄存器用，比如放 TEB；
  - Mach trap `thread_set_x86_64_compat`（trap -108）。
  - 内核里还有受限 entitlement 字符串 `com.apple.developer.cross-architecture-support(-unmanaged)` 和 `com.apple.private.4k-pages`。

  本机实验结果（核查者在 25F71 上复现）：未授权的二进制用 4K 模式 spawn 会失败，返回 `EBADMACHO`(88)，连 `/usr/bin/true` 这样的系统二进制也一样；内核日志写 "does not satisfy requirements for 4k page size"（核查者用非特权 `log show` 没能复现这行日志，但同样的格式串就在开源 `mach_loader.c` 里）；`thread_set_x86_64_compat(1)` 返回 `KERN_FAILURE`(5)。开源 xnu 的 `bsd/kern/mach_loader.c` 给出了 4K 的门控条件：进程要么有 `com.apple.private.4k-pages`，要么通过 `ml_satisfies_x86_64_requirements()`，否则在正式版内核上一律返回 `LOAD_BADMACHO`[39][40]。所以 **4K 地址空间需要 entitlement 这一点已由源码证实**；`ml_satisfies_x86_64_requirements()` 的实现未公开，**很可能**对应 cross-architecture-support entitlement；TSO 入口同样需要授权属 **[推断，证据较强]**；CrossOver ARM64 为什么要求 26.5+ 仍是 **[推断]**。这是 Cider 最大的外部依赖。
- **上游开源拼图基本齐了**：
  - Wine 10.0（2025-01-21）实现了 ARM64EC 和 x86-64 模拟接口，当时仍要求 4K 主机页[18]；
  - Wine 11.0（2026-01-13）完成了新 WoW64，废弃纯 32 位前缀，移除 `wine64` 加载器，并加入"在 16K/64K 主机页上模拟 4K 页"。发布说明原话是 "more demanding applications may not work correctly"，并强烈建议用 4K 页内核[19]；
  - FEX 提供 `libarm64ecfex.dll`（x86-64）和 `libwow64fex.dll`（i386），实现 Windows 的 `BTCpu64*` / xtajit 接口[24][25]；
  - Hangover 11.x 在上游 Wine 之上只剩约 10 个补丁[23]；
  - Valve 为 Steam Frame 投资 FEX，SteamDB 上出现 `proton-arm64ec` 标签[32]。
  - **但上游 FEX 没有 Darwin 支持**，FEX 维护者表示只支持 Linux[28][43]；Box64 没有 ARM64EC 模块，只能处理 32 位（`wowbox64.dll`），其文档仍把 x64（ARM64EC）部分列为尚未支持[23][57]。
- **Rosetta 目前的能力**：
  - 64 位进程里跑 32 位 x86 代码可行，Wine 在 macOS 上用 `i386_set_ldt` 设置 LDT[17]；
  - 能执行 AVX/AVX2 指令（不论是否设置环境变量），但 CPUID 和 Windows 特性查询 API 默认不报告，要设置 `ROSETTA_ADVERTISE_AVX=1`（macOS 15+；GPTK 4 beta 2 仍这样记载，默认关闭）[1][11][54]；
  - 不支持 AVX-512[1]；
  - x87 用完整 80 位软件模拟，精度高但慢[14][16]；
  - 翻译线程由内核开启硬件 TSO，开销约 5%[14][27]；
  - CPU 密集负载约为原生的 70–80%[31][30]；
  - Wine 在 Rosetta 下不能设置调试寄存器[17]；
  - **同一进程里不能混用 arm64 和 x86_64 代码**[1]。
- **建议**：Cider 做成"**可插拔的 CPU 后端**"。
  - **现在**：用 "x86_64 Wine 11.x（新 WoW64）+ Rosetta" 的 **R 后端**上线，拿到兼容性和用户。
  - **同时**：马上开始 "arm64 Wine + ARM64EC + FEX" 的 **A 后端**原型，在本机 macOS 26.5 上验证 4K 页、x18、TSO、W^X 四个关键点，并**尽早向 Apple 申请跨架构 entitlement**。
  - **兜底**：Virtualization / Hypervisor 虚拟机方案作为 **V 后端**，公开 API 就能拿到 TSO 和 4K 页。
  - **目标**：2027 年 6 月 WWDC（macOS 28 beta）前，A 后端达到默认可用。

---

## 详细调研

### 1. Rosetta 2 中与 Wine 相关的能力

| 能力 | 现状 | 对 Wine / Cider 的意义 | 来源 / 置信度 |
|---|---|---|---|
| x86-64 → arm64 翻译 | AOT + JIT。Mach-O 由 `oahd` 预先翻译并缓存；运行时生成或加载的代码走 JIT | Wine 加载的 PE 镜像对 Rosetta 来说只是 mmap 出来的内存，**[推断]** 主要走 JIT 路径，所以首次运行会卡顿 | [14][15] 已证实；PE 走 JIT 为 [推断] |
| 进程级翻译 | "系统禁止同一进程混用 arm64 与 x86_64 代码" | R 后端必须**整个 Wine 都是 x86_64**：unix 侧、MoltenVK / DXMT / D3DMetal 的 dylib 全部是 x86_64，图形转译层本身也被翻译，有 CPU 开销 | [1] 已证实 |
| 64 位进程内跑 32 位 x86 代码 | 支持。Wine 的 x86_64 unix 代码在 `__APPLE__` 分支用 `i386_set_ldt()` 建 32 位段和 fs 选择子，用私有 `_thread_set_tsd_base()` 切 GSBASE；Wine 11 在 macOS 的 syscall dispatcher 里交换 `%gs`，避免 TEB 和 macOS 线程描述符冲突。源码注释还写明 syscall 的 CS 修正 "Only applies on Intel, not under Rosetta" | 新 WoW64（`WINEARCH=wow64`）在 Rosetta 下可以运行 32 位 Windows 程序。CrossOver 自 2020 年起就在 M1 上这么跑 | [17][19][42 相关报道] 已证实 |
| 调试寄存器 | Wine 源码写明 "Setting debug registers is not supported under Rosetta" | 依赖硬件断点的反作弊 / DRM / 调试器会失败 | [17] 已证实 |
| AVX / AVX2 | macOS 15 起能翻译 AVX/AVX2，不支持 AVX-512[1]。CPUID 默认**不**报告：GPTK 2.1 README 写明 `ROSETTA_ADVERTISE_AVX` 默认 0，设为 1 后才向被翻译程序发布 cpuid 信息[11]；GPTK 4 beta 2 仍这样记载[54]。gamekit PR #22 的探针显示，AVX/AVX2 指令**不论是否设置都能执行**，变量只改变 CPUID 和 Windows 特性查询 API 的结果[54] | 需要 AVX 才能启动的游戏必须设置此变量，例如 D2R 的加载器不设就会卡住[55]。256 位运算在没有 SVE 的 M 系列上只能拆成 128 位 NEON 操作 **[推断]**。"可能比 SSE 路径更慢"只有一个微基准作证：2024-06-11 在 M2、macOS 15.0 开发者 beta 上，AVX2 比 SSE2 慢约 12.9%（12.88%），AVX 与 SSE 大致持平；该作者怀疑的 256 位整数结果错误后来也被质疑[13]。Whisky 的开关提示它 "may significantly impact performance"[12] | 行为 [1][11][54] 已证实；性能结论为单一来源 [未证实] |
| x87 | 完整 80 位软件模拟，每条 x87 指令展开成大约 20 条 ARM 指令，精度对但慢；社区项目 rosettax87 用低精度快速实现换来约 4.7 倍微基准加速（229,731 对 48,517 平均 tick，要求 macOS 15.5 或兼容版本），已于 2026-01-02 归档，由 WineAndAqua 分支继续维护；另有 x87sidecar | 老 32 位游戏（大量 x87 代码）会遇到 CPU 瓶颈。这类项目靠 hook Rosetta 实现，不适合 Cider 正式发布版 | [14][16] 已证实 |
| 硬件 TSO | 内核为翻译线程打开 Apple 私有的 TSO 模式（ACTLR TSOEN），普通 load/store 就有 x86 的内存序。FEX 博客估计开销约 5% | 这是 Rosetta 快的关键原因之一；第三方进程拿不到（见第 4 节） | [14][27][41] 已证实 |
| 标志位硬件扩展 | Apple 加了未公开的 PF/AF 计算支持；公开的 FEAT_AFP / FlagM2 本机为 1 | FEX 能用上 AFP / FlagM / LRCPC，但用不到 Apple 的私有扩展 | [14]；本机 sysctl [40] |
| JIT / 自修改代码 | Apple 文档："Rosetta 可以翻译大多数 Intel 应用，包括含 JIT 编译器的应用" | .NET / LuaJIT / 浏览器内核这类 x86 JIT 可以运行，但 SMC 频繁时会反复重新翻译 | [1] 已证实 |
| AOT 缓存 | macOS 用 `oahd` 预翻译 Mach-O；Linux VM 版在 macOS 14+ 有 `VZLinuxRosettaCachingOptions` | 对 Windows PE 代码作用有限 **[推断]** | [9][15] |
| 性能 | AnandTech SPEC：CPU 密集约为原生 70–80%，内存密集 >90%；M1 上 7-zip 约 71% | 游戏常常 GPU 或驱动受限；但在 R 后端里 D3D→Metal 转译本身也被翻译，CPU 受限场景损失会叠加 **[推断]** | [31][30] 较可信（2020–2022 数据） |
| 反作弊兼容修复 | 有报道称 macOS 26.4 / 26.5 修了与某些反作弊 / 游戏（D4、OW）相关的 Rosetta 问题 | 说明 Apple 仍在为 Wine 游戏场景维护 Rosetta | [未证实]（论坛和搜索摘要） |

### 2. Rosetta 的未来：官方表态与时间线

**Apple 原文（一手来源）**：
- 《About the Rosetta translation environment》[1]：Rosetta "will be available through macOS 27 — as a general-purpose tool for Intel apps… Beyond this timeframe, we will keep a subset of Rosetta functionality aimed at supporting older unmaintained gaming titles, that rely on Intel-based frameworks."
- 同一页还写明："macOS 27 directly integrates support for Intel binary translation, without needing to install Rosetta. This enables support for Intel Linux binaries running in ARM VMs as well as Intel Linux containers."[1][9]
- 《Running Intel Binaries in Linux VMs》[9]：从 macOS 27 起，`VZLinuxRosettaDirectoryShare.availability` 总是返回 `.installed`，`installRosetta` 会立即返回。
- Apple Developer News（2026-09-01）[2]：
  - macOS 26.4 起，启动依赖 Rosetta 的应用时会弹出系统通知；
  - "macOS 27: Final release to support Rosetta — Intel-only apps will no longer run on Mac computers with Apple silicon after this update"；
  - 同时重申游戏子集会继续支持。
- Apple Support 102527（2026-09-21 更新）[3]："Rosetta is available for any Mac with Apple silicon using macOS 27 or earlier"；从 macOS 28 起，"only for certain older, unmaintained games that rely on Intel-based frameworks"。
- 以上三份 Apple 文档截至 2026-09-26 口径一致，但**都没有说明游戏怎样才能进入子集**。

**二手报道（[较可信]，没有找到 Apple 原文）**：MacRumors（2026-09-24）[5] 写明 macOS 27 已于 2026-09-14 发布，"Upgrading to macOS Golden Gate removes Rosetta, but if you have an app installed that requires it, you can reinstall it"；设置 > 通用 > 关于本机里有一个 "Intel-Based Apps" 列表。其他报道补充：第一次启动 Intel 应用时系统会提示重装，也可以手动运行 `softwareupdate --install-rosetta`。9to5Mac（2026-09-09）[46] 只是发布日期的预告，不能证明已经发布。

**时间线**

| 日期 | 事件 | 来源 |
|---|---|---|
| 2020-11 | M1 发布；CrossOver 在 Rosetta 2 下运行 32/64 位 Windows 程序 | [42 相关报道] |
| 2024-06 | macOS 15：Rosetta 支持 AVX/AVX2；GPTK 2 加入 `ROSETTA_ADVERTISE_AVX` | [11][13] |
| 2025-06-10 | WWDC25：宣布通用 Rosetta 提供到 macOS 27 | [4] |
| 2026 春 | macOS 26.4 开始对 Rosetta 应用弹出提示 | [2][7] |
| 2026-06（WWDC26） | 重申时间线；macOS 27 定名 Golden Gate；2026-06-08 发布 GPTK 4 beta（D3DMetal 4），评估环境仍经 Rosetta 翻译 x86 代码 | [6][52][53] |
| 2026-09-14 | macOS 27 发布：升级时移除 Rosetta，可重装（二手报道）；使用 Rosetta 应用时出现警告；Linux VM 的 Intel 翻译改为系统内置（Apple 文档） | [5][6][9]；[46] 为发布前预告 |
| 2027 秋（预计） | macOS 28：通用 Rosetta 停止，只留游戏子集 | [1][3][5] |

**尚未明确的地方**：
- "子集"的判定机制（是否白名单、按 bundle 还是签名、检测哪些"Intel 框架"）完全没有公开。
- MacRumors 2026-02 援引 Apple 的说法，Linux VM 里的 Intel 二进制翻译在 macOS 27 之后仍会支持[7]。这与 [1][9] "系统内置"的说法一致，但 macOS 28 的文档还没出。
- Mac Observer 指出 Apple 各页面措辞前后不一致[8]；不过 Apple 自己的文档 [1][3] 口径一致，以它们为准。
- 第三方项目 gamekit 的评估（2026-09-18）认为，游戏例外 "does not establish coverage"，即不能认定会覆盖 Wine / Steam / D3DMetal 二进制[44]。这是第三方推断，**不是 Apple 政策**，属 [未证实]。

**CodeWeavers 的公开表态**（转述自 [20][21] 和相关报道）：
- "Rosetta 2 will be largely discontinued with macOS 28 in 2027"；
- ARM64 版 CrossOver "will run without Rosetta 2"；
- CrossOver 26 在 CrossOver 27 发布后仍可继续使用。

**结论**：对 Cider 来说，**macOS 28 上不能假设 Rosetta 可用**。最迟在 **2027 年 6 月 WWDC**（macOS 28 beta）之前，arm64 主机路径必须可用。

### 3. Rosetta 之外的替代方案

#### 3.1 上游 Wine 的 ARM64 / ARM64EC 架构（一手来源）
- **Wine 9.0（2024-01）**：ARM64 支持，可运行原生 Windows ARM64 程序和模拟的 i386 程序[22]。
- **Wine 10.0（2025-01-21；WineHQ 新闻 2025012101 和 wine-10.0 标签日期均为 1 月 21 日）**[18]：
  - "The ARM64EC architecture is fully supported"；
  - `--enable-archs=arm64ec,aarch64` 可构建 ARM64X 混合模块；
  - "The 64-bit x86 emulation interface is implemented… only the application's x86-64 code requiring emulation"；
  - 外部模拟器通过注册表 `HKLM\Software\Microsoft\Wow64\amd64` 指定；
  - 当时要求主机 4K 页，"16K or 64K pages is not supported"。
- **Wine 11.0（2026-01-13）**[19]：
  - 新 WoW64 完全可用，支持 16 位程序；
  - `WINEARCH=win32` 纯 32 位前缀**已废弃**；
  - 移除 `wine64` 加载器；
  - ARM64 上"支持在更大主机页（通常 16K/64K）上模拟 4K 页"，但只适合简单程序：原文为 "more demanding applications may not work correctly"，并且 "using a 4K-page kernel is strongly recommended"。
- **2026 年开发版**：
  - 11.9（2026-05-18）：ARM64 模拟代码中的线程挂起[45]；
  - 11.15：支持 Mingw 模式构建 ARM64EC；
  - 11.16（2026-08-21）：改进 ARM64EC 异常处理，Wine Mono 11.3.0 支持 ARM64；
  - 11.17（2026-09-04）和 11.18（2026-09-18）：截至 2026-09-26 的最新开发版，发布说明里没有 ARM64 方面的重点改动[61]；
  - Wine 12 预计 2027-01 发布[45]。

**ARM64EC 的价值**：Wine 自身、DXMT/DXVK 等图形转译层全部原生运行，**只有游戏 / 程序自己的 x86-64 代码被模拟**。R 后端里所有代码都要被翻译，两者 CPU 开销差距在"驱动 / 转译层重"的游戏上可能很明显 **[推断]**。

ARM64EC 的 ABI 细节[35]：
- 与 x64 CONTEXT 对应，x13、x14、x23、x24、x28、v16–v31 不可用；
- 变参函数用 x0–x3，另用 x4/x5 描述栈上参数；
- 通过 entry / exit thunk 与 `__os_arm64x_dispatch_call_no_redirect` 和模拟器交互；
- FEX 的 ARM64EC 模块会把 X18 等"禁用寄存器"清零[25]。

#### 3.2 FEX-Emu（MIT 许可证）
- **构建方式**[24]：
  - ARM64EC 版（`arm64ec-w64-mingw32`）产出 `libarm64ecfex.dll`，负责 x86-64；
  - WOW64 版（`aarch64-w64-mingw32`）产出 `libwow64fex.dll`，负责 i386；
  - 需要 llvm-mingw 的 ARM64EC 工具链；
  - 在 Windows on Arm 上可以作为 xtajit64 的直接替代品。
- 源码 `Source/Windows/ARM64EC/Module.cpp` 实现了以下接口[25]：
  - `BTCpu64FlushInstructionCache`、`BTCpu64NotifyMemoryDirty`、`BTCpu64IsProcessorFeaturePresent`；
  - `NotifyMemoryAlloc/Protect/Free`、`NotifyMapViewOfSection`、`ResetToConsistentState` 等；
  - 通过 `FEXUnixLib` 调用主机侧功能，例如磁盘缓存的文件映射。**这部分正是移植到 macOS 需要改写的地方 [推断]**。
- **近期版本**：
  - FEX-2608（2026-08-04）：修复 win32 下 VirtualProtect 静默失败；Wine 下用 WFE 实现自旋等待[26]；
  - FEX-2609（2026-09-07）：JIT 代码磁盘缓存（FOZ 格式，`FEX_DISKCACHE=1`；目前还没有容量上限，也不会清除过期条目）；移除缺少 unixlib 时的回退逻辑，即 Wine 下 unixlib 变为必需；PMULHRSW 在部分 Geekbench 测试中提速约 2 倍[26]；
  - FEX-2609.1：FEX-2609 之后的小版本标签，是截至 2026-09-26 最新的 FEX 标签，发布日期未核实[56]；
  - 以上发布说明都没有提到 macOS 或 Darwin。
- **macOS 状态**：README 只写 "ARM64 Linux"；FEX 维护者在 Discussion #3267 中表示 FEX 只支持 Linux[28]；第三方汇总称 "upstream FEX has no Darwin support today"[43]。GitHub 上唯一叫 "FEX_MacOs" 的分叉（Jpkovas）是 2025-11 的原样镜像，没有任何 Darwin 改动[58]。CodeWeavers 在 2026-07 "completed work to make a custom version of FEX compatible with macOS"[21]。**FEX 是 MIT 许可，CodeWeavers 没有义务公开这些 Darwin 改动**；Wine 部分是 LGPL，会随 CrossOver 27 源码一起发布（2027 年初）[43]。
- **对 M3 的适配**：本机 M3 有 FEAT_LRCPC/LRCPC2、LSE2、AFP、RPRES、FlagM/FlagM2，没有 SVE/SME[40]。FEX 的 AVX 需要在 128 位 ASIMD 上实现；早期 FEX 认为 AVX 需要 SVE2，2024 年中改为支持 128 位实现[28]。

#### 3.3 Box64 / WowBox64（MIT 许可证）
- 最新版 v0.4.4（2026-08-02，截至 2026-09-26 仍是最新），带默认开启的 DynaCache 和 RC 配置工具 box64-configurator；发布说明没有提到 ARM64EC，文档仍把 x64（ARM64EC）部分列为尚未支持[29][57]。
- Box64 本体是 Linux ELF 用户态模拟器，没有 macOS 主机支持。
- Hangover 的 `box64cpu.dll` 已作为 `wowbox64.dll` 并入上游，**只负责 32 位 WoW64**；64 位只能用 FEX 的 `libarm64ecfex.dll` 或 Wine 的 `xtajit64.dll` 桩[23]。
- 结论：**Box64 最多能作为 i386 模拟的备选**，不能替代 x86-64。

#### 3.4 Hangover
- 用 FEX 或 Box64 配合上游 Wine，**只支持 arm64 Linux**。
- 用 `HODLL`（32 位：`libwow64fex.dll` / `wowbox64.dll` / `wow64cpu.dll`）和 `HODLL64`（64 位：`libarm64ecfex.dll` / `xtajit64.dll`）选择模拟器[23]。
- 11.0（2026-01-13）：移除 QEMU，上游之上只剩约 10 个补丁。
- 11.16（2026-08-30）：配 FEX 2608、Box64 v0.4.4[23]；截至 2026-09-26 仍是最新版。
- 对 Cider 的价值：它是"**在上游 Wine 之上接入 FEX / Box64 的最小补丁集**"，可以直接当作参考实现。

#### 3.5 Valve 的投入
- Steam Frame 使用 Snapdragon 8 Gen 3，运行 SteamOS，用 Proton + FEX 运行 x86 Windows 游戏。
- FEX 和 Lepton 于 2026-08-02/03 在 Steam 上公开[32]。
- SteamDB 出现 `proton-arm64ec-4` / `proton-arm64ec-experimental` 标签，说明 Valve 在做 ARM64EC 版 Proton[32]。
- 媒体称 FEX 开销约 10–20%，Valve 从很早就资助 FEX [较可信，二手]。
- **意义**：FEX + Wine ARM64EC 会得到长期、大规模的投入，Cider 可以搭便车；但 Valve 的重点是 Linux，**Darwin 适配要靠 CodeWeavers 或 Cider 自己**。

### 4. native-ARM64 路径在 macOS 上的障碍（含 macOS 26 新接口）

| 障碍 | Windows / Wine 的期望 | macOS 现状 | 2025–2026 新变化（本机验证） |
|---|---|---|---|
| **页大小** | Windows：4 KB 页、64 KB 分配粒度；Wine 10 要求 4K | arm64 进程 `hw.pagesize=16384`[40]；只有 Rosetta 下的 x86-64 进程拿到 4K 页[28] | ① Wine 11 的 4K 模拟，局限大[19]；② SDK 26.0 起有 `posix_spawnattr_set_4k_page_size_np()`，内部字段 `psa_4k`，注释为 "Force 4k address space"[38][39]；③ 本机用它 spawn 普通 arm64 二进制返回 `EBADMACHO`(88)，内核日志 "`load_machfile: binary '…' does not satisfy requirements for 4k page size (fatal_mode=1)`"[40]。核查者复现了返回值（ad-hoc 签名的测试程序和 `/usr/bin/true` 都返回 88），但用非特权 `log show` 没能看到这行日志；同样的格式串 `%s: binary '%s' does not satisfy requirements for 4k page size (fatal_mode=%d)` 就在开源源码里；④ **门控条件已从源码查明**[39]：xnu-12377.121.6 `bsd/kern/mach_loader.c` 在 `deferred_4k_check && !check_ent("com.apple.private.4k-pages") && !ml_satisfies_x86_64_requirements(check_ent)` 时记录上述日志，fatal 模式下返回 `LOAD_BADMACHO`；`fourk_fatal_mode_enabled()` 只在 development 内核上读 NVRAM `x86-64-compat-dev`，正式版内核恒为 true；另有按 `FOURK_PAGE_MASK` 的段对齐检查，同样返回 `LOAD_BADMACHO`。**所以主要门槛是 entitlement [已证实]**，二进制还须 4K 对齐（常规 16K 对齐的 arm64 Mach-O 天然满足，所以本机失败来自 entitlement 检查 [推断]）。`ml_satisfies_x86_64_requirements` 的实现未公开，**很可能**走 cross-architecture-support entitlement（其字符串就在同一文件）[推断]；⑤ 内核还有 `vm_force_4k_pages=1` 字符串 |
| **x18** | Windows ARM64 用户态 x18 指向 TEB[34] | Apple ABI："The platforms reserve register x18. Don't use this register."[33] | SDK `os/arch/arm64.h` 新增 `os_set_custom_x18_abi_enabled(bool)` 和 `os_custom_x18_abi_enabled()`，`API_AVAILABLE(macos(26.4))`。头文件注释明确提到"为其他 ABI 实现兼容层……例如用作 TSD base"。开启后该线程**不得调用任何 macOS 库**（这两个函数除外），重复设置同一状态会 abort，信号处理需特别小心[38]。本机非 hardened 二进制可正常切换[40]；内核有 `com.apple.security.custom-x18-abi-toggle` 和 `com.apple.private.custom-x18-abi`，**[推断]** hardened runtime 下可能需要前者 |
| **TSO** | x86 程序依赖 TSO | 公开 API 里**没有**给原生 macOS 进程开 TSO 的办法；内核只为 Rosetta 进程开启[41] | ① Hypervisor.framework 的 `HV_SYS_REG_ACTLR_EL1`（macOS 15.0）可以给 vCPU 设 EnTSO 位[38]；② Apple 提供给 Linux 客户机的内核补丁有 `prctl(PR_SET_MEM_MODEL, PR_SET_MEM_MODEL_TSO)`[10]；③ macOS 26 SDK 有 Mach trap `thread_set_x86_64_compat(uint32_t)`，xnu 中是 trap -108，但开源 xnu `osfmk/kern/syscall_sw.c` 里第 108 项是 `MACH_TRAP(kern_invalid, 0, 0, NULL)`，实现没有公开[39]；本机未授权进程调用返回 `KERN_FAILURE`(5)[40]。`kern_invalid` 返回的是 `KERN_INVALID_ARGUMENT`(4)，本机却得到 5，说明正式版内核里有一个未开源的真实处理函数，拒绝了未授权的调用方 **[推断，证据较强]**。**[推断]** 这就是给获授权的跨架构进程开 TSO（以及其他 x86 兼容特性）的入口 |
| **W^X / JIT** | Windows 允许 `PAGE_EXECUTE_READWRITE` | Apple silicon 对所有进程强制 W^X：本机不带 `MAP_JIT` 时 RWX 的 mmap / mprotect 都返回 EACCES；带 `MAP_JIT` 可以，需配合 `pthread_jit_write_protect_np` 和 `sys_icache_invalidate`[36][40]。Hardened runtime 下需要 `com.apple.security.cs.allow-jit`，**且只能有一个 MAP_JIT 区域**；如果用 `jit-write-allowlist`，就不能再调 `pthread_jit_write_protect_np`[36] | FEX 的代码缓存必须放在单个 MAP_JIT 大区里再细分。x86 客户代码的 RWX 页对 FEX 来说只是数据，没有问题；但**原生 ARM64/ARM64EC 的 Windows JIT** 需要 Wine 在 NtProtectVirtualMemory 层模拟 **[推断]** |
| **签名与受限 entitlement** | — | 本机给 ad-hoc 签名的二进制加上 `com.apple.developer.cross-architecture-support` 或 `com.apple.private.4k-pages`，会被 AMFI 直接 SIGKILL，日志为 "adhoc signed but contains restricted entitlements"[40] | **[推断]** 需要 Developer ID 签名 + Apple 发放的 provisioning profile（受管 capability）。`mach_loader.c` 显示 4K 路径接受两类授权：`com.apple.private.4k-pages`（`com.apple.private.*` 是 Apple 内部 entitlement，第三方基本拿不到 [推断]），或 `ml_satisfies_x86_64_requirements` 路径（很可能对应 `com.apple.developer.cross-architecture-support(-unmanaged)`）[39]。Apple 没有公开 cross-architecture-support 的文档，网上搜索也找不到。`-unmanaged` 变体的含义不明 |
| **其他 ABI 差异** | Windows：变参放在 x0–x7 通用寄存器；红区 16 B；被调用方负责扩展窄参数 | Apple：变参全部放栈上，`va_list` 是 `char*`；红区 128 B；调用方负责扩展 <32 位的参数；`char` 有符号；`long double` 等于 `double`[33][34] | Wine 的 PE 侧用 mingw-clang 按 Windows ABI 编译，unix 侧按 Apple ABI 编译，两边只通过 syscall / unixcall 分发器交界，影响可控 **[推断]**。x18 的开关也应放在这个分发器里 |

**CrossOver 为什么要求 26.5**：CodeWeavers 没有公开原因。时间上与上表几个 API 的引入（26.0 / 26.4）吻合，**[推断]** 26.5 修复或完善了相关内核路径。未解。

### 5. 性能对比（公开数据很少，同平台对比基本没有）

| 方案 | 数据 | 条件 / 注意 | 来源 |
|---|---|---|---|
| Rosetta 2 | CPU 密集约 70–80% 原生，内存密集 >90%；7-zip 约 71% | M1，2020–2022 | [31][30] |
| Box64 | 7-zip 约 57% 原生 | M1 上的 Linux（16K 页），2022 | [30] |
| FEX（2022） | Pi 400 上 7-zip 约 25% | 太旧，不能代表现状 | [30] |
| FEX（2026） | 媒体称 Steam Frame 上开销约 10–20%；PMULHRSW 提速 2 倍等单项优化 | 二手 / 单项 | [32][26] |
| TSO 成本 | Apple 硬件 TSO 约 5%；没有硬件 TSO 时，load/store 要换成 acquire/release，代价在不同核心上差别很大（AmpereOne 的 release-store 只有普通 store 的约 8.5%） | FEX 博客 | [27] |
| x87 | Rosetta 默认实现对比 rosettax87 快速实现约 4.7 倍差距 | 微基准 | [16] |
| CrossOver Linux ARM64 | Cyberpunk 2077 约 120 FPS 等 | Ampere Altra + RTX 4060 Ti，GPU 受限，**不能用来衡量 CPU 转译** | [22] |

**结论**：到 2026-09 为止，**没有在同一台 Mac 上公开比较 Rosetta-Wine 和 FEX-ARM64EC-Wine 的数据**，Cider 需要自己测。**[推断]**
- 纯 CPU 计算：FEX 很可能慢于 Rosetta，因为 Rosetta 有 AOT、Apple 私有标志位扩展和硬件 TSO；
- "驱动 / 转译层重"的游戏：ARM64EC 让 Wine 和 D3D→Metal 层原生运行，可能扳回差距；
- **能否用上硬件 TSO**，决定 FEX 在 macOS 上的性能上限。

### 6. CrossOver、Whisky、GPTK 目前的运行方式

- **Apple GPTK**：x86_64 Wine 整体跑在 Rosetta 下，图形由 D3DMetal 负责；AVX 通过 `ROSETTA_ADVERTISE_AVX` 开启[11]。2.x 时代的安装方式是先进入 `arch -x86_64 zsh`，装 x86_64 Homebrew，再 `brew install apple/apple/game-porting-toolkit`。**注意 2.x 已过时**：
  - GPTK 2.1 为 2024-03；GPTK 3.0 于 2024-12 发布（Gcenx 打包的 3.0-3 为 2025-03-03）[51]；
  - GPTK 4 beta 在 WWDC26（2026-06-08）发布，带 D3DMetal 4，评估环境支持 Metal 4，仍处于 beta[52]；
  - 按现有报道，GPTK 4 的评估环境仍通过 Rosetta 翻译 x86 代码[53]，GPTK 4 beta 2 仍记载 `ROSETTA_ADVERTISE_AVX`（默认关闭）[54]；
  - **没有找到 D3DMetal 有 arm64 / ARM64EC 版本的证据**，这直接关系 A 后端的 D3D12 路线。Cider 应跟踪 GPTK 4 / D3DMetal 4，而不是 2.x。
- **Whisky**：WhiskyWine 是基于 CrossOver 源码的 x86_64 Wine 加 GPTK，在 Rosetta 下运行，提供 AVX 开关[12]。原仓库 2025-04 归档，作者建议改用 CrossOver，社区有分支继续维护[42]。
- **CrossOver 26.0.0（2026-02-10）**：基于 Wine 11.0，带 D3DMetal 3.0、DXMT v0.72、vkd3d 1.18、Wine Mono 10.4.1，仍是 **x86_64 Wine + Rosetta**[48][49][50]。之后有 26.1.0（2026-04-09）、26.2.0（2026-06-09），截至 2026-09-26 最新版为 26.3.0（2026-07-21）[49]。
  - **AVX 更正**：自动开启 AVX **不是 CrossOver 26 的新功能**，[48] 也没有提到它。CrossOver 25（2025-03-11）起，CrossOver 会对已知需要 `ROSETTA_ADVERTISE_AVX=1` 的游戏**按游戏**自动开启，不是全局设置；其他游戏由用户在 bottle 里手动设置这个环境变量，CodeWeavers 的 Prey、Death Stranding 等游戏提示页就是这样写的[47]。
- **CrossOver Mac ARM64 Preview（2026-07-31，通向 CrossOver 27）**[21]：
  - universal 包，同时含 **ARM64 Wine** 和 **Intel Wine**；
  - macOS 26.5+ 上用 ARM64 Wine + CodeWeavers 定制的 macOS 版 FEX，更低版本退回 Intel Wine + Rosetta；
  - 使用 ARM64EC：可读的页面都没有明说，但 Wine 在 ARM64 上模拟 x86-64 只能走 ARM64EC，CodeWeavers 的时间线也把 x86-64 模拟归功于 Wine 10 的 ARM64EC，属强推断；
  - 带 ARM64 DXMT；
  - 已知限制：不含 D3DMetal（"Direct3D 12 support coming soon"）、很多游戏启动器无法运行、旧 bottle 不能转换；
  - 截至 2026-09-26 没有找到更新的 ARM64 预览公告。
- **CrossOver 27 正式版（2027 年初）**：只支持 Apple silicon，要求 Sonoma+，不支持 32 位 bottle（"32-bit bottles will no longer run, full stop"）[20][60]。注意：32 位**程序**仍可在 64 位 bottle 里通过新 WoW64 运行，**[推断]** ARM64 路径下由 `libwow64fex.dll` 负责模拟。

---

## 对 Cider 的启示与建议（按优先级）

**P0 — 架构决策（立即）**
1. **CPU 后端做成可插拔**，bottle 元数据记录 `cpu_backend ∈ {rosetta-x86_64, arm64-fex, vm}`。
   - **R 后端**：x86_64 Mach-O Wine 11.x（新 WoW64）+ x86_64 图形栈，在 Rosetta 下运行。
   - **A 后端**：arm64 Wine（unix 侧为 arm64 Mach-O；PE 侧用 `--enable-archs=aarch64,arm64ec,i386` 构建）+ `libarm64ecfex.dll`（通过 `HKLM\Software\Microsoft\Wow64\amd64` 注册）+ `libwow64fex.dll`；图形栈原生运行。
   - **V 后端（兜底）**：Virtualization.framework 跑 Linux 客户机，用 Apple 内置的 Intel 翻译（macOS 27+，文档写明面向 Linux VM 和容器；macOS 27 起 `VZLinuxRosettaDirectoryShare.availability` 恒为 `.installed`，`installRosetta` 立即返回，所以 V 后端不需要引导用户安装 Rosetta[9]），或者 Hypervisor.framework + `HV_SYS_REG_ACTLR_EL1` 开 TSO + 4K 客户页 + FEX；里面运行 Wine/Proton。这条路**只依赖公开 API**，适合办公或非图形重负载的应用，GPU 转发是难点。
2. **只做 64 位 bottle**，一开始就不支持 `WINEARCH=win32`，和 Wine 11、CrossOver 27 保持一致。bottle 目录按"架构无关"设计：`drive_c` 和注册表可以复用，系统 DLL 在切换后端时由 `wineboot -u` 重建。这样可以做到 CrossOver 目前做不到的 **R↔A bottle 迁移**。**[推断，需验证]**

**P1 — R 后端上线（2026 Q4 – 2027 Q2）**
3. 在开发机上装 Rosetta（`softwareupdate --install-rosetta`，由用户手动执行）。基于上游 Wine 11.x 的 x86_64 macOS 构建做基线，验证 32 位 WoW64、LDT 和 GSBASE 路径。
4. **按程序配置 CPU 选项**：`ROSETTA_ADVERTISE_AVX` 默认关闭，按兼容库白名单**逐个程序**开启，UI 里也允许用户按 bottle 或按程序手动打开。这和 CrossOver 25 起的做法一致：只对已知需要的游戏自动开启，不做全局设置[47]。要注意，这个变量不控制 AVX 指令能否执行（不设也能执行），只控制 CPUID 和 Windows 特性 API 是否报告 AVX，从而影响程序选哪条代码路径[54]。"AVX 路径更慢"目前只有一个微基准作证[13]，Whisky 也提示可能明显影响性能[12]，所以默认关闭仍然合理，但应该用 Cider 自己的基准套件（第 9 条）确认。在 UI 中标出"需要调试寄存器 / 硬件断点"的不兼容类别。**不要**把 hook Rosetta 的 x87 补丁放进正式版，可以留作实验开关。

**P1 — A 后端原型（立刻并行开始，本机 macOS 26.5 已满足条件）**
5. **原型 A1 – x18**：在 Wine arm64 unix 侧的 syscall 和 unixcall 分发器里包裹 `os_set_custom_x18_abi_enabled(true/false)`，运行原生 ARM64 的 Windows 控制台程序。**测量每次切换的开销**：如果每次都进内核，会成为 NT syscall 热路径的瓶颈。
6. **原型 A2 – 页大小**：先用 Wine 11 的 4K 模拟跑一组真实程序，记录失败类型。在拿到 entitlement 之前，这是 A 后端**唯一可用**的页大小方案，应作为 A2 的主线。`posix_spawnattr_set_4k_page_size_np` 路径（由 Cider 启动器 spawn Wine 加载器）已从 xnu 源码确认需要 `com.apple.private.4k-pages`，或者通过 `ml_satisfies_x86_64_requirements` 检查[39]，所以现阶段只做能力探测和代码准备，同时确保 Wine 加载器和各 Mach-O 的段满足 4K 对齐。
7. **原型 A3 – FEX on Darwin**：
   - 把 `FEXUnixLib` 移植到 Darwin（文件映射、pid、线程）；
   - 代码缓存用**单个 MAP_JIT 大区**，配合 `pthread_jit_write_protect_np`，满足 hardened runtime 限制；
   - 验证 SMC 检测在 16K 保护粒度下是否正确；
   - TSO 先用 LRCPC/LRCPC2 软件路径，量化与 Rosetta 的差距。
8. **原型 A4 – TSO 探测**：写一个诊断工具，报告 `thread_set_x86_64_compat`、4K spawn、x18 切换的可用性，作为 Cider 的"平台能力探针"和启动时的后端选择依据。返回值要区分：`KERN_FAILURE`(5) 表示"内核有这个 trap 但拒绝了本进程"，`KERN_INVALID_ARGUMENT`(4) 表示"内核没有这个 trap"；4K spawn 返回 `EBADMACHO` 表示"缺少 entitlement 或段不对齐"。
9. **基准套件**：7-zip、Cinebench R23（x86-64）、x87 重的老游戏、AVX 游戏、CPU 受限的 DX11 场景。R 后端和 A 后端在同一台 M3（8 GB 内存）上对比，同时记录内存峰值（JIT 缓存 + AOT 缓存 + 游戏占用，对 8 GB 机器很敏感）。

**P1 — 外部依赖（本季度内行动）**
10. **加入 Apple Developer Program，申请 "Cross-Architecture Support" 一类的受管 capability**。在 Feedback Assistant 或 DTS 中询问 `com.apple.developer.cross-architecture-support`、4K 页和 x86-64 compat 的申请条件。申请目标是这个 developer entitlement；`com.apple.private.4k-pages` 属 Apple 内部 entitlement，不作为申请目标。Apple 没有公开这个 entitlement 的任何文档，所以只能通过 DTS 或 Feedback 询问。**这是 A 后端能否达到 CrossOver 水平的决定性因素。** 如果 Apple 只授权给商业伙伴，就要把 V 后端提前到 P1。
11. **跟踪 CrossOver 27 源码发布**（Wine 部分是 LGPL，2027 年初），吸收其中 macOS arm64 相关补丁；关注 CodeWeavers 是否向上游 FEX 提交 Darwin 支持（MIT 许可，没有公开义务）。考虑直接向 FEX 上游贡献 Darwin 移植，和 FEX / Valve 生态绑定。
12. **跟踪 GPTK 4 / D3DMetal 4，而不是 GPTK 2.x**[52][53]：重点确认 D3DMetal 是否会出现 arm64 版本，这决定 A 后端能否用 D3DMetal 做 D3D12，否则 A 后端的 D3D12 只能靠 vkd3d / DXMT 一类开源路线。D3DMetal 不随 Cider 安装包或仓库分发，安装流程设计为从用户自己下载的 GPTK 中导入。第三方称 GPTK 许可只允许不做修改、整体、非商业的再分发，但 Apple 的许可原文没有读到，未证实[59]。

**P2 — 2027 年里程碑**
13. 2027 年 6 月 WWDC 前，A 后端成为 macOS 26.5+ 上新建 bottle 的**默认后端**；R 后端作为兼容性回退一直保留到 macOS 27。在 macOS 28 beta 首周验证"游戏子集"是否会在 Wine 进程上生效（只作为加分项，不做依赖）。

---

## 风险

1. **受限 entitlement 风险（高）**：4K 地址空间需要 entitlement，这一点已由开源 `mach_loader.c` 证实（`com.apple.private.4k-pages` 或 `ml_satisfies_x86_64_requirements` 路径）[39]；x86-64 compat（TSO）trap 在本机对未授权进程返回 `KERN_FAILURE`，很可能也需要受限 entitlement [推断]。如果 Apple 不给免费或开源项目，A 后端只能退到"16K 页 + 4K 模拟 + 软件 TSO"，兼容性和性能明显不如 CrossOver。
2. **Rosetta 提前失效（中）**：macOS 27 升级时默认卸载 Rosetta，并对使用它的应用弹警告，影响用户体验；macOS 28 上 R 后端基本不可用。游戏子集的机制不透明。
3. **FEX Darwin 分叉维护（中高）**：上游 FEX 没有 Darwin 支持；CodeWeavers 的移植可能不公开；Cider 自己维护的分叉会持续承受上游每月发布的合并压力。
4. **性能不确定（中）**：没有硬件 TSO 时 FEX 在 macOS 上的 CPU 性能可能明显低于 Rosetta；M3 没有 SVE/SME，AVX 只能用 128 位实现；8 GB 内存下 JIT 缓存会带来内存压力。
5. **W^X 和 hardened runtime（中）**：公证发布需要 hardened runtime，而 "只能有一个 MAP_JIT 区域" 的限制会约束 FEX 代码缓存的设计，以及原生 ARM64 Windows JIT 的支持。
6. **x18 切换的开销和正确性（中）**：开启自定义 x18 后不能调用任何 macOS 库，信号处理要特殊处理；如果切换代价高，NT syscall 密集的程序会变慢。
7. **图形栈绑定（中）**：A 后端要求图形转译层有 arm64 / ARM64EC 版本。D3DMetal 是 Apple 的闭源二进制，只随 GPTK 提供；目前没有找到 arm64 版本的证据，截至 2026-07-31 CrossOver ARM64 Preview 也不含它[21][53]。能否随 Cider 打包未证实（见建议 12），所以按"用户从自己的 GPTK 导入"设计[59]。DXMT 是开源的，可以在 arm64 上构建。（详见图形调研报告。）
8. **Apple 文档口径变化（低中）**：Apple 各页面措辞曾不一致[8]，需要持续跟踪官方文档。

## 未解问题

1. `thread_set_x86_64_compat` 的确切语义：只开 TSO，还是连同 AFP / 标志位等 Rosetta 硬件特性一起开？需要什么 entitlement？能否按线程开关？开源 xnu 中该表项是 `kern_invalid`（返回 4），本机却返回 `KERN_FAILURE`(5)，说明正式版内核有未开源的真实实现。
2. ~~`posix_spawnattr_set_4k_page_size_np` 的"requirements"具体是什么~~ **基本已解决**：开源 `mach_loader.c` 显示门槛是 entitlement（`com.apple.private.4k-pages` 或 `ml_satisfies_x86_64_requirements`），另加 4K 段对齐检查[39]。仍未解决的是：`ml_satisfies_x86_64_requirements` 的实现（未公开）是否就是检查 `com.apple.developer.cross-architecture-support`；`-unmanaged` 变体是否意味着可以在 App Store 之外分发。
3. CrossOver ARM64 为什么要求 **26.5** 而不是 26.4？
4. Apple 的"游戏子集"如何判定（白名单、签名、框架检测）？Wine / CrossOver 进程是否可能被覆盖？截至 2026-09-26，Apple 的三份文档[1][2][3] 都没有说明。
5. Rosetta 对 Wine 映射的 PE 代码是否有持久化翻译缓存（JIT 缓存能否跨进程复用）？这决定 R 后端的首次运行卡顿。
6. Rosetta-Wine 与 FEX-ARM64EC-Wine 在同一台 Apple silicon 上的系统性对比，目前还没有公开数据。
7. Wine 11 的 4K 页模拟在 macOS 16K 主机上的真实兼容范围（例如反作弊、`SEC_IMAGE` 映射、64K 对齐的 DLL 重定位）。
8. Valve 的 ARM64EC Proton 是否会给出通用于非 Linux 主机的 FEX 改动，比如磁盘缓存、unixlib 抽象？
9. Apple 是否会给开源兼容层发放跨架构 entitlement，以及具体流程和周期。
10. GPTK 4 / D3DMetal 4 是否有或将有 arm64 版本？没有的话，A 后端的 D3D12 支持只能走开源路线。

## 参考来源

1. Apple Developer — About the Rosetta translation environment（Rosetta 提供到 macOS 27、游戏子集、macOS 27 内置 Intel 翻译、禁止混用架构、支持 AVX/AVX2 不支持 AVX-512）：https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment ；JSON 端点（核查时读取原文用）：https://developer.apple.com/tutorials/data/documentation/apple-silicon/about-the-rosetta-translation-environment.json
2. Apple Developer News，2026-09-01，Upcoming changes to Rosetta support for Intel-based macOS apps：https://developer.apple.com/news/?id=w5ngl9k2
3. Apple Support 102527（2026-09-21 更新）"Rosetta is available… macOS 27 or earlier"：https://support.apple.com/en-us/102527
4. MacRumors 2025-06-10，引用 WWDC25 时 Apple 文档的原文：https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/
5. MacRumors 2026-09-24，macOS Golden Gate 移除的功能（升级时移除 Rosetta，可重装）：https://www.macrumors.com/2026/09/24/macos-golden-gate-features-removed/
6. AppleInsider 2026-06-12，Intel 应用支持何时终止：https://appleinsider.com/articles/26/06/12/how-and-when-macos-will-finally-stop-support-for-intel-apps
7. MacRumors 2026-02-16，macOS 26.4 的 Rosetta 警告，以及 Linux VM 继续支持：https://www.macrumors.com/2026/02/16/macos-tahoe-26-4-rosetta-2-warnings/
8. Mac Observer，Apple 各页面口径不一致：https://www.macobserver.com/news/apple-rosetta-end-date-contradiction-macos-26-27-28/
9. Apple Virtualization — Running Intel Binaries in Linux VMs（macOS 27 内置翻译、`VZLinuxRosettaDirectoryShare.availability` 恒为 `.installed`、AOT 缓存）：https://developer.apple.com/documentation/virtualization/running-intel-binaries-in-linux-vms ；JSON 端点：https://developer.apple.com/tutorials/data/documentation/virtualization/running-intel-binaries-in-linux-vms.json
10. Apple Virtualization — Accelerating the performance of Rosetta（ACTLR.TSOEN、`prctl(PR_SET_MEM_MODEL…)`）：https://developer.apple.com/documentation/virtualization/accelerating-the-performance-of-rosetta
11. GPTK 2.1 README 的第三方副本（`ROSETTA_ADVERTISE_AVX` 默认 0、x86_64 Wine 在 Rosetta 下运行；Apple 原件需开发者登录）：https://gist.github.com/lynkos/3999f629560219a81d4e2c083a4bf5b1
12. Whisky PR #1034，AVX 开关（2024-10-24 合并）：https://github.com/Whisky-App/Whisky/pull/1034
13. rosetta2_avx_dive，macOS 15 下 AVX 的实现与微基准：https://github.com/carsongoodwin32/rosetta2_avx_dive
14. Dougall Johnson，Why is Rosetta 2 fast?（2022-11-09，AOT、TSO、PF/AF、x87）：https://dougallj.wordpress.com/2022/11/09/why-is-rosetta-2-fast/
15. Project Champollion，Rosetta 2 逆向（AOT、oahd）：https://ffri.github.io/ProjectChampollion/part1/
16. rosettax87（约 4.7 倍 x87 加速，2026-01-02 归档）：https://github.com/Lifeisawful/rosettax87 ；x87sidecar：https://github.com/rdbell/x87sidecar
17. Wine 源码 dlls/ntdll/unix/signal_x86_64.c（`i386_set_ldt`、`_thread_set_tsd_base`、Rosetta 下不能设调试寄存器）：https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/unix/signal_x86_64.c ；raw：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/signal_x86_64.c
18. Wine 10.0 发布说明（**2025-01-21**，ARM64EC 与 x86-64 模拟接口、4K 页要求）：https://raw.githubusercontent.com/wine-mirror/wine/wine-10.0/ANNOUNCE.md ；WineHQ 新闻：https://www.winehq.org/news/2025012101 ；标签日期：https://api.github.com/repos/wine-mirror/wine/git/refs/tags/wine-10.0 ；转载：https://www.linuxcompatible.org/story/wine-100-released/
19. Wine 11.0 ANNOUNCE（2026-01-13，新 WoW64、废弃 win32 前缀、移除 wine64、4K 页模拟、macOS `%gs`）：https://github.com/wine-mirror/wine/blob/wine-11.0/ANNOUNCE.md ；raw：https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md ；WineHQ 新闻：https://www.winehq.org/news/2026011301
20. CodeWeavers 博客 2026-06-11，What's in and what's out for CrossOver 27：https://www.codeweavers.com/blog/mjohnson/2026/6/11/whats-in-and-whats-out-for-crossover-27 ；AppleInsider 转述：https://appleinsider.com/articles/26/06/11/crossover-a-windows-to-mac-gaming-tool-goes-apple-silicon-only
21. CodeWeavers 博客 2026-07-31，The right to bear ARM64 on Mac：https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac ；AppleInsider：https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears
22. CodeWeavers 2025-11-06 Linux ARM64 Preview（FEX 集成的时间线）：https://www.codeweavers.com/blog/mjohnson/2025/11/6/twist-our-arm64-heres-the-latest-crossover-preview ；GamingOnLinux：https://www.gamingonlinux.com/2025/11/codeweavers-launch-a-new-crossover-preview-adding-linux-arm64-support/
23. Hangover README 与 Releases（HODLL/HODLL64、wowbox64、11.0 / 11.16）：https://github.com/AndreRH/hangover ；https://github.com/AndreRH/hangover/releases ；Phoronix：https://www.phoronix.com/news/Hangover-11.0-Released
24. FEX Wiki — Development:ARM64EC：https://wiki.fex-emu.com/index.php/Development:ARM64EC
25. FEX 源码 Source/Windows/ARM64EC/Module.cpp（BTCpu64* 接口、FEXUnixLib）：https://github.com/FEX-Emu/FEX/blob/main/Source/Windows/ARM64EC/Module.cpp
26. FEX-2609（2026-09-07，磁盘缓存）：https://fex-emu.com/FEX-2609/ ；FEX-2608（2026-08-04）：https://fex-emu.com/FEX-2608/
27. FEX 博客 The scourge of x86 emulation（TSO 成本；页面标注 2026-09-17）：https://fex-emu.com/Scourge-of-emulation/
28. FEX Discussion #3267，FEX 在 macOS 上的可行性（Rosetta 进程才有 4K 页、SIGILL 实验；维护者表示 FEX 只支持 Linux）：https://github.com/FEX-Emu/FEX/discussions/3267
29. Box64 Releases（v0.4.4，2026-08-02）：https://github.com/ptitSeb/box64/releases
30. box86.org 2022-03-24，Box64 / FEX / QEMU / Rosetta2 基准：https://box86.org/2022/03/box86-box64-vs-qemu-vs-fex-vs-rosetta2/
31. Michael Tsai 汇总的 Rosetta 2 性能（AnandTech SPEC 等）：https://mjtsai.com/blog/2020/11/16/performance-of-rosetta-2-on-apple-m1/
32. GamingOnLinux 2026-08，Steam Frame 的 FEX / Lepton 与 proton-arm64ec 标签：https://www.gamingonlinux.com/2026/08/lepton-and-fex-get-prepared-for-the-steam-frame-release/ ；TechSpot：https://www.techspot.com/news/113337-valve-publicly-releases-lepton-fex-compatibility-tools-power.html
33. Apple — Writing ARM64 code for Apple platforms（x18 保留、红区、变参、参数扩展）：https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
34. Microsoft — Overview of ARM64 ABI conventions（x18 指向 TEB、16 B 红区、变参规则）：https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions
35. Microsoft — Understanding Arm64EC ABI：https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
36. Apple — Porting just-in-time compilers to Apple silicon（MAP_JIT、allow-jit、单一区域限制、jit-write-allowlist）：https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon
37. Apple — Addressing architectural differences in your macOS code（页大小、内存序、W^X）：https://developer.apple.com/documentation/apple-silicon/addressing-architectural-differences-in-your-macos-code
38. 本机 macOS 26.5 SDK 头文件：`/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk/usr/include/spawn.h`（`posix_spawnattr_set_4k_page_size_np`，macos 26.0）、`usr/include/os/arch/arm64.h`（`os_set_custom_x18_abi_enabled`，macos 26.4）、`usr/include/mach/mach_traps.h`（`thread_set_x86_64_compat`）、`System/Library/Frameworks/Hypervisor.framework/Headers/hv_vcpu_types.h`（`HV_SYS_REG_ACTLR_EL1`，macos 15.0）
39. xnu-12377.121.6（2026-06-17 标签）：https://github.com/apple-oss-distributions/xnu/tree/xnu-12377.121.6
    - `osfmk/mach/syscall_sw.h` 中 `kernel_trap(thread_set_x86_64_compat,-108,1)`：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/osfmk/mach/syscall_sw.h
    - `osfmk/kern/syscall_sw.c` 中 `/* 108 */ MACH_TRAP(kern_invalid, 0, 0, NULL)`：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/osfmk/kern/syscall_sw.c
    - `bsd/sys/spawn_internal.h` 中 `psa_4k` "Force 4k address space"：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/sys/spawn_internal.h
    - `bsd/kern/mach_loader.c`：4K 门控条件（`com.apple.private.4k-pages` / `ml_satisfies_x86_64_requirements`）、日志格式串、`fourk_fatal_mode_enabled()` 与 NVRAM `x86-64-compat-dev`、`FOURK_PAGE_MASK` 段对齐检查、cross-architecture-support 字符串：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/kern/mach_loader.c
40. 本机实验（2026-09-26，M3 / macOS 26.5 25F71）：4K spawn 返回 EBADMACHO 及对应内核日志；`thread_set_x86_64_compat`→KERN_FAILURE；x18 切换成功；RWX 不带 MAP_JIT 返回 EACCES；受限 entitlement 被 AMFI 拒绝；内核字符串 `com.apple.developer.cross-architecture-support` 等；sysctl `hw.optional.arm.*`。同日两位核查者各自复现：ad-hoc 测试程序和 `/usr/bin/true` 的 4K spawn 均返回 88，trap 返回 5，`hw.pagesize=16384`、FEAT_AFP=1、FEAT_FlagM2=1、FEAT_LRCPC2=1、FEAT_SME=0；内核日志行未能用非特权 `log show` 复现
41. saagarjha/TSOEnabler（内核通常只为 Rosetta 进程开启 TSO）：https://github.com/saagarjha/TSOEnabler
42. MacRumors 2025-04-23，Whisky 停止开发：https://www.macrumors.com/2025/04/23/whisky-ends-mac-gaming-tool-crossover/
43. highball issue #6（第三方汇总：Wine ARM64EC + 自定义 macOS FEX、26.5+、上游 FEX 无 Darwin 支持）：https://github.com/gauthierpiarrette/highball/issues/6
44. EndofLineTech/gamekit PR #34（第三方的 macOS 28 可行性评估和 API 探针，2026-09-18；结论属第三方推断）：https://github.com/EndofLineTech/gamekit/pull/34
45. GamingOnLinux，Wine 11.9（2026-05-18）：https://www.gamingonlinux.com/2026/05/wine-11-9-released-with-arm64-improvements-initial-support-for-system-threads/
46. 9to5Mac 2026-09-09，Apple 确认 macOS 27 将于 2026-09-14 发布（发布前预告，不是发布证明）：https://9to5mac.com/2026/09/09/apple-confirms-macos-27-golden-gate-launch-date-september-14/
47. CodeWeavers 博客 2025-03-11，CrossOver 25（对需要 `ROSETTA_ADVERTISE_AVX=1` 的游戏自动开启 AVX）：https://www.codeweavers.com/blog/mjohnson/2025/3/11/experience-next-level-gaming-on-mac-with-crossover-25 ；按游戏设置的提示页：https://www.codeweavers.com/compatibility/crossover/tips/prey-2017/avx-capabilities ，https://www.codeweavers.com/compatibility/crossover/tips/death-stranding/enabling-avx-support-for-death-stranding （codeweavers.com 返回 403，内容来自搜索摘要）
48. OMG! Ubuntu，CrossOver 26（2026-02-10，Wine 11.0、D3DMetal 3.0、DXMT v0.72；未提 AVX）：https://www.omgubuntu.co.uk/2026/02/crossover-26-released
49. CrossOver 更新日志（26.0.0 2026-02-10、26.1.0 2026-04-09、26.2.0 2026-06-09、26.3.0 2026-07-21）：https://www.codeweavers.com/crossover/changelog
50. Phoronix，CrossOver 26（组件版本）：https://www.phoronix.com/news/CrossOver-26
51. Gcenx/game-porting-toolkit Releases（2.1 2024-03-12、3.0 2024-12-05、3.0-3 2025-03-03）：https://github.com/Gcenx/game-porting-toolkit/releases
52. AppleInsider 2026-06-08，Game Porting Toolkit 4（beta，评估环境支持 Metal 4）：https://appleinsider.com/articles/26/06/08/game-porting-toolkit-4-ushers-in-support-for-agentic-coding
53. Macworld，GPTK 4 beta 体验（评估环境仍翻译 x86 代码）：https://www.macworld.com/article/3189951/apples-latest-game-porting-toolkit-beta-changed-how-i-think-about-mac-gaming.html
54. EndofLineTech/gamekit PR #22（2026-09-17 合并；AVX 探针：指令总能执行，只有 CPUID / Windows 特性 API 受变量影响；引用 GPTK 4 beta 2 文档）：https://github.com/EndofLineTech/gamekit/pull/22 ；另见 Heroic issue #4193：https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/4193
55. Blizzard 论坛，D2R 在 Apple silicon 上启动卡住的根因（AVX）：https://us.forums.blizzard.com/en/d2r/t/guide-d2r-on-apple-silicon-root-cause-of-the-launch-hang-avx-and-a-free-open-source-wine-setup/177572
56. FEX 标签列表（FEX-2609.1、FEX-2609、FEX-2608）：https://api.github.com/repos/FEX-Emu/FEX/tags
57. Box64 v0.4.4 发布页（DynaCache 默认开启、box64-configurator，未提 ARM64EC）：https://github.com/ptitSeb/box64/releases/tag/v0.4.4
58. Jpkovas/FEX_MacOs（2025-11 的原样镜像，没有 Darwin 改动）：https://github.com/Jpkovas/FEX_MacOs
59. dbc-hbin/d3dmetal-redistributable（第三方再分发仓库，称只允许非商业再分发；Apple 许可原文未核实）：https://github.com/dbc-hbin/d3dmetal-redistributable ；https://github.com/dbc-hbin/d3dmetal-redistributable/releases/tag/gptk-4.0b2
60. GIGAZINE 2026-06-12，CrossOver 27 取消旧平台支持：https://gigazine.net/gsc_news/en/20260612-crossover-27-removes-legacy-support-mac-intel/
61. WineHQ GitLab 标签列表（11.17 2026-09-04、11.18 2026-09-18）：https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/repository/tags

## 事实核查记录

> 2026-09-26，两位核查者各自独立核查。相同声明合并为一行；来源编号对应上方「参考来源」。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| Apple 文档写明 Rosetta 作为通用工具提供到 macOS 27，之后只保留面向老游戏的子集；Support 102527（2026-09-21）写明 macOS 28 起只对部分老游戏可用 | 已证实（两位核查者） | 通过 JSON 端点读到原文，Support 102527 和 Developer News（2026-09-01）口径一致[1][2][3]。三页都没有说明游戏怎样入选。gamekit PR #34 的"不能认定覆盖 Wine"是第三方推断，不是 Apple 政策，正文已注明[44] |
| macOS 27 Golden Gate 于 2026-09-14 发布；升级时移除 Rosetta，可重装；macOS 27 内置 Linux VM / 容器用的 Intel 翻译 | 已证实（附注） | 发布日期和"升级移除、可重装"只来自 MacRumors 等二手报道，没有 Apple 原文，正文标为 [较可信][5]；9to5Mac 那篇只是发布前的预告[46]。补充：macOS 27 起 `VZLinuxRosettaDirectoryShare.availability` 恒为 `.installed`，`installRosetta` 立即返回[9]，已写入 §2 和建议 1 |
| CrossOver Mac ARM64 Preview（2026-07-31）= ARM64 Wine（ARM64EC）+ 定制 macOS 版 FEX，ARM64 部分要求 26.5+；CrossOver 27（2027 年初）只支持 Apple silicon、要求 Sonoma+、不支持 32 位 bottle | 已证实（附注） | "ARM64EC"没有任何可读页面明说，是强推断，摘要和 §6 已标注。补充：预览版不含 D3DMetal；26.5 以下跑 Intel Wine + Rosetta；截至 2026-09-26 最新正式版为 26.3.0（2026-07-21）[21][49][60] |
| macOS 26.5 SDK 声明 `posix_spawnattr_set_4k_page_size_np`（26.0）、`os_set_custom_x18_abi_enabled`（26.4）；xnu 有 trap -108；未授权 4K spawn 返回 EBADMACHO，内核日志提示不满足 4K 要求 | 已证实（附注） | 两位核查者在 25F71 上复现了返回值 88，`/usr/bin/true` 也一样。内核日志行没能用非特权 `log show` 复现，但格式串原样出现在开源 `mach_loader.c` 里[39][40] |
| Wine 10.0（2025-01-22）实现 ARM64EC 和 x86-64 模拟接口；Wine 11.0（2026-01-13）完成新 WoW64、废弃 win32 前缀、支持 4K 页模拟（仅简单程序） | 部分正确 | **Wine 10.0 的发布日期更正为 2025-01-21**（WineHQ 新闻 2025012101、wine-10.0 标签），摘要、§3.1 和参考 18 已改。其余内容正确；补充了 11.0 原话 "more demanding applications may not work correctly"、移除 `wine64` 加载器，以及最新开发版 11.17 / 11.18[18][19][61] |
| Rosetta 能翻译 AVX/AVX2，不支持 AVX-512；`ROSETTA_ADVERTISE_AVX` 默认 0，macOS 15+ 设为 1 才通过 CPUID 报告 AVX | 已证实（附注） | 补充：不论是否设置，AVX 指令都能执行，变量只影响 CPUID 和 Windows 特性 API；GPTK 4 beta 2 仍这样记载[54]。"AVX 更慢"只有一个微基准（M2、macOS 15.0 beta），数字从"约 12.8%"改为 12.88%，并注明该作者的 256 位整数结论受到质疑[13]。建议 4 的理由已相应改写 |
| （§4 / 未解问题 2）4K spawn 的"requirements"不明，需要受限 entitlement 只是推断 | 部分正确 → 基本解决 | 开源 `mach_loader.c` 显示门槛是 `com.apple.private.4k-pages` 或 `ml_satisfies_x86_64_requirements`；正式版内核恒为 fatal 模式；另有 `FOURK_PAGE_MASK` 段对齐检查[39]。"需要 entitlement"升级为 [已证实]；`ml_satisfies_x86_64_requirements` 的实现和 cross-architecture-support 的文档仍未公开。§4、摘要、建议 6 / 10、风险 1 和未解问题 2 已更新 |
| （§4）`thread_set_x86_64_compat` 在开源 xnu 中为 `kern_invalid`，本机返回 `KERN_FAILURE` | 已证实 | 补充：`kern_invalid` 返回 `KERN_INVALID_ARGUMENT`(4)，本机却返回 5，支持"正式版内核有未开源的真实实现"这一推断。已写入 §4、未解问题 1，建议 8 的探针也会区分这两个返回值 |
| （§6）CrossOver 26（2026-02-10）基于 Wine 11.0、D3DMetal 3.0、DXMT v0.72，启动器自动设置 `ROSETTA_ADVERTISE_AVX`[48] | 部分正确 | 版本信息正确，补充了 vkd3d 1.18、Wine Mono 10.4.1。**自动开启 AVX 不是 26 的新功能**：CrossOver 25（2025-03-11）起就按游戏开启，不是全局设置，而 [48] 并未提到 AVX。§6 已更正，新增参考 [47][49][50]；建议 4 的"按程序开启"与这个做法一致 |
| （§4 / V 后端）`HV_SYS_REG_ACTLR_EL1`（macOS 15.0）可设 EnTSO；hardened runtime + allow-jit 只能有一个 MAP_JIT 区域，用 jit-write-allowlist 后不能再调 `pthread_jit_write_protect_np` | 已证实 | 无需更改[36][38] |
| （§1）Wine x86_64 unix 代码在 `__APPLE__` 分支用 `i386_set_ldt()` 和 `_thread_set_tsd_base()`；"Setting debug registers is not supported under Rosetta" | 已证实 | 补充注释 "Only applies on Intel, not under Rosetta"（syscall CS 修正），并给参考 17 加了 raw 链接 |
| （§3）FEX-2608 / 2609、Box64 v0.4.4、Hangover 11.0 / 11.16、上游 FEX 无 Darwin 支持 | 已证实 / 部分正确 | 一位核查者判"已证实"，另一位判"部分正确"，只是因为新出了 FEX-2609.1 标签。两者不矛盾，所以不标存疑。补充：FEX-2609.1（日期未核实）；磁盘缓存暂无容量上限和过期清理；FEX 维护者表示只支持 Linux；Box64 文档仍把 ARM64EC 部分列为未支持；Jpkovas/FEX_MacOs 只是镜像[56][57][58] |
| （§6）把 GPTK 2.x 当作当前版本描述 | 部分正确 | 2.x 已过时。GPTK 3.0 于 2024-12 发布；GPTK 4 beta 于 2026-06-08 发布（D3DMetal 4 / Metal 4），评估环境仍经 Rosetta，beta 2 仍记载 `ROSETTA_ADVERTISE_AVX`[51][52][53][54]。没有 arm64 版 D3DMetal 的证据。§6 和时间线已更新，并新增建议 12 和未解问题 10 |
| （§1）rosettax87 已于 2026-01-02 归档，由 WineAndAqua 分支继续维护，x87 微基准约 4.7 倍加速 | 已证实 | 补充了 tick 数和"要求 macOS 15.5 或兼容版本"。维持"不进正式版"的建议[16] |
| （风险 7）D3DMetal 是 Apple 闭源二进制（隐含：能否随 Cider 打包） | 部分正确 | D3DMetal 只随 GPTK 提供。第三方称只允许不修改、整体、非商业的再分发，但 Apple 许可原文没有读到，未证实[59]。风险 7 和新建议 12 改为"从用户自己的 GPTK 导入"。CrossOver ARM64 Preview 也不含它 |

**分歧说明**：两位核查者对同一声明的结论没有实质冲突。唯一的差异是第 12 行"已证实"对"部分正确"，原因只是新增的 FEX-2609.1 标签，所以本次没有需要标为 **存疑** 的条目。
