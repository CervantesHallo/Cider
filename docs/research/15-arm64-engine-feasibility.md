# ARM64 引擎（Engine A）可行性核实：低 4GB、4K 页、TSO、entitlement 与 FEX Darwin 移植范围

> 调研日期 2026-09-26（本机实验于 2026-09-27 完成；独立事实核查于 2026-09-27 并入，见文末“事实核查记录”）· 调研机器：Apple M3 / macOS 26.5 (25F71) / 内核 `xnu-12377.121.6~2`，只装了 CLT，未装 Rosetta。
> 置信度说明：**[已证实]** = 读过一手源码、官方页面，或在本机复现过；**[较可信]** = 多个独立的二手来源互相印证，或来自第三方源码中的明确陈述；**[推断]** = 基于证据的工程判断，需要原型验证；**[未证实]** = 只有单一间接来源。
> 本文的任务是裁决 02、06、07、14 四份报告之间的矛盾，并给出 Engine A 的 go/no-go 结论和原型计划。所有 xnu 源码都读自 `apple-oss-distributions/xnu` 的 GitHub 标签；本机实验程序只在 scratchpad 中编译运行。

---

## 摘要

- **07 号报告的“arm64 进程永远拿不到低 4GB”结论已过时**。它引用的是 xnu `main` 分支，而 `main` 实际上是 **xnu-12377.1.9（macOS 26.0，2025-10-16 导入）**，比本机内核还旧。在 **xnu-12377.101.15（26.4）** 和 **xnu-12377.121.6（26.5）** 中，`load_machfile()` 新增了一段逻辑：只要 `ml_satisfies_x86_64_requirements(check_ent)` 为真，就把 `enforce_hard_pagezero` 置为 false（源码注释是 “entitled to a soft page zero”）。随后 `load_segment()` 只把 `min_offset` 提高到 **1 页**，`__PAGEZERO` 的其余部分改成一段可以释放的 “soft PAGEZERO” 映射 [1][2][3]。**[已证实]** 标签与 macOS 版本的对应关系（1.9=26.0、81.4=26.3、101.15=26.4、121.6=26.5）已由 `distribution-macOS` 各版本的 `release.json` 证实，不再是推断（核查更正）[41]。因此，带 entitlement 的 arm64 Wine 进程可以映射 0x10000–0x7fffffff。
- **能满足 `ml_satisfies_x86_64_requirements` 的是 `com.apple.developer.cross-architecture-support`（以及 `-unmanaged` 变体）**。开源 xnu 里这个函数只是一个恒返回 false 的桩，但头文件注释点名了这个 entitlement；xnu 自带测试把两个变体都称为 “x86-64 emulation entitlements”，并说明它们能在 hardened runtime 下授予 `MAP_JIT` [6][7]。**[已证实]** 本机实测：ad-hoc 签名的二进制带上任一变体，exec 时都会被 AMFI 杀掉（退出码 137）[13]。Apple 的公开文档、entitlement 索引和 capability 列表里都查不到它 [36][40]。
- **4K 页**：公开 API `posix_spawnattr_set_4k_page_size_np()`（设置 `psa_4k`）以及内核对 `psa_4k` 的处理，**在开源树中第一次出现是 26.5（xnu-12377.121.6）**，尽管 SDK 头文件把它标为 `macos(26.0)`。26.4 只支持私有标志 `_POSIX_SPAWN_FORCE_4K_PAGES`（0x1000）；26.3 及更早的正式版内核则根本没有这条路径（该标志只在 DEBUG/DEVELOPMENT 构建中定义）[1][4][5]。无论走哪条路径，都需要 `com.apple.private.4k-pages` 或 cross-architecture entitlement，否则返回 `EBADMACHO`(88)。本机用公开 API 和私有标志分别复现了 88 [13]。**[已证实]**
- **x18**：06 号报告说“非 hardened 二进制可正常切换 x18”，这个说法有误导性。本机实测：不带 entitlement 时，`os_set_custom_x18_abi_enabled(true)` 能调用成功，但**每次上下文切换内核都会清掉这个标志并清零 x18**（2000 次里丢失 2000 次）。带上 **`com.apple.security.custom-x18-abi-toggle`** 后 x18 全部保留（2000/2000），而且**这个 entitlement 可以 ad-hoc 签名**，不需要 Apple 批准。一次开/关的开销约 4–9 ns，没有进入内核 [8][13]。**[已证实]** 补充：xnu 的 `tests/x18-toggle-entitlements.plist` 还带了 `com.apple.private.4k-pages`，但本机复测表明切换 x18 不需要它 [8][13]。
- **macOS 26.5 这个下限有了源码层面的解释**。26.4 的 libsyscall 里 x18 API 叫 `os_custom_x18_abi()`/`os_custom_x18_abi_get()`，到 26.5 才改成现在 SDK 里的 `os_set_custom_x18_abi_enabled()`/`os_custom_x18_abi_enabled()`。公开的 4K spawn API 也是 26.5 才落地 [5][9]。CodeWeavers 博文的搜索摘要写的是 “ARM64 pieces require macOS 26.5 or higher” [24]。**[较可信]** **核查补充**：26.4（xnu-12377.101.15）已经可以通过私有标志 0x1000 走完整的授权 4K 路径，soft pagezero 也已存在，所以源码并不严格要求 26.5。26.5 下限更合理的解释是 26.5 才有的公开 4K spawn API 和改名后的 x18 API，而不是内核机制首次出现 [3][4][9]。
- **TSO**：Mach trap `thread_set_x86_64_compat`（-108）在开源 xnu 里是 `kern_invalid`，本机未授权调用返回 `KERN_FAILURE`(5)，说明正式版内核中有真实实现并拒绝了调用 [10][13]。Asahi 开发者 Hoshino Lina 在 2026-04-21 的公开帖子中称，这个调用会“动态开关 TSO 和其他 Apple 私有的 x86-64 兼容特性”，并且受私有 entitlement 保护 [39]。**[较可信]** 拿不到授权时，只能用 FEX 的 LRCPC/LRCPC2 软件内存序；M3 有 FEAT_LRCPC2，没有 LRCPC3，向量访问的 TSO 仍然很贵 [31][32]。
- **CrossOver Preview 20260821（CFBundleVersion 27.0.0.40921）的实际结构**：NotProton（`main` @ `c09e0c4`，2026-09-26）的 `SupportedRunner.swift` 为同一版本登记了两个构建，`flavor: nil`（Rosetta）和 `flavor: "fex"`。其中 fex 构建的 cleanNtdll/patchedNtdll 哈希表只列了 **i386-windows 和 aarch64-windows** [20][43]。`resolve.py` 注明 aarch64 那份 ntdll “carries the loader twice, once for the native side and once for the emulated guest”，这符合 ARM64X/ARM64EC 的特征。NotProton 把 `com.apple.developer.cross-architecture-support` 当作 Wine loader 上的受限 entitlement，loader 带有它就跳过重签名 [21][22]。**核查更正**：这张哈希表只说明 NotProton 给哪些 ntdll 打补丁，**不能证明** CX 的 fex 构建里没有其他 ntdll（例如 x86_64-windows）。“Preview” 这个词来自 README，不在 `SupportedRunner.swift` 里 [19]。**[较可信]** 由此推断（不是确认）：CX 走的是“授权 + ARM64EC + WoW64（libwow64fex）”这条路。CodeWeavers 博文摘要说 Mac ARM64 构建用的是定制版 FEX，这与该推断一致 [24]。
- **FEX 的“Darwin 移植”范围远小于想象**。在 Wine 模式下，FEX 以 MinGW PE DLL 的形式运行（`libarm64ecfex.dll`/`libwow64fex.dll`）。Linux 专有的部分（`Source/Tools/LinuxEmulation` 下的 syscalls、SignalDelegator、seccomp、FEXServer 等）完全用不到。宿主侧只有一个 189 行的 `Source/Windows/UnixLib/FEXUnixLib.cpp`，编译成 `libwow64fex.so` 和 `libarm64ecfex.so` 并链接 `rt` [29]。**核查更正**：它导出的 unixcall 不止 TSO 一个，一共 **7 个**：`SetHardwareTSOControl`（`prctl(PR_SET_MEM_MODEL_TSO)`）、`SetKernelUnalignedAtomicControl`（`PR_ARM64_SET_UNALIGN_ATOMIC`）、`Madvise`、`SetVMAName`（`PR_SET_VMA`）、`GetSHMStatsVMA`（`shm_open`/`mmap`）、`DeleteSHMStatsFile` 和 `MapFile`。每一个都需要 Darwin 等价实现或桩 [29]。此外，FEX-2609 的 `CMakeLists.txt` 会拒绝 Linux/WIN32 以外的宿主 [29]。即便如此，真正的工作量仍在 **Wine 的 arm64-macOS unix 侧**。
- **结论**：Engine A **可以作为研发方向 GO，但作为产品引擎只能“有条件 GO”**。条件是 Cider 拿到 `com.apple.developer.cross-architecture-support`，或者 Apple 的政策发生变化。没有授权时，上游 Wine 在 arm64 macOS 上连 `KUSER_SHARED_DATA`（固定在 0x7ffe0000）都映射不了，32 位程序的兼容性损失是 100%，因此**不能作为产品引擎（NO-GO）**，只能做“原生 ARM64 Windows 程序”这一小众模式 [28][27]。

---

## 详细调研

### 1. `enforce_hard_pagezero` 由谁设置：26.0 与 26.5 的逐行对比

**01 版本对应关系**：`apple-oss-distributions/xnu` 的 `main` 头提交为 `f6217f8`，提交信息是 “xnu-12377.1.9”，导入时间 2025-10-16 [2][11]。12377 系列的标签依次为 .1.9 / .41.6 / .61.12 / .81.4 / .101.15 / .121.6。本机 26.5 的 `uname -v` 显示 `xnu-12377.121.6~2`。原文按 15.x 系列（.101=x.4、.121=x.5）的惯例把 .101.15 推断为 26.4。**核查更正：现已 [已证实]**。`apple-oss-distributions/distribution-macOS` 各版本的 `release.json` 给出的对应关系是 macos-260 → xnu-12377.1.9、macos-263 → xnu-12377.81.4、macos-264 → xnu-12377.101.15、macos-265 → xnu-12377.121.6 [41]。

**对比表**（行号均来自各标签的 `bsd/kern/mach_loader.c`）：

| 逻辑 | main = 12377.1.9（26.0） | 12377.81.4（26.3） | 12377.101.15（26.4） | 12377.121.6（26.5） |
|---|---|---|---|---|
| `enforce_hard_pagezero` 初值 TRUE | L700 | 有 | 有 | L743 |
| 唯一的置 FALSE 路径：`#if __x86_64__` 且为 32 位二进制 | L849 | 有 | 有 | L1033（Intel 内核专用，与 Apple silicon 无关） |
| **授权进程置 false**：`if (ml_satisfies_x86_64_requirements(check_ent)) enforce_hard_pagezero = false; /* entitled to a soft page zero */` | 无 | 无 | **有** | **L1082–1085** |
| ARM64 的 4GB 硬 pagezero 检查 `vm_map_has_hard_pagezero(map, 0x100000000)` | L897–903 | 有 | 有（位于上一行之后） | L1087–1093 |
| `load_segment()`：授权时 “use only a 1-page hard PAGEZERO … and a soft PAGEZERO for the rest” | 无（直接 `vm_map_raise_min_offset(vm_end_aligned)`） | 无 | 有 | L2604–2637 |
| 4K spawn：`deferred_4k_check`，检查 `com.apple.private.4k-pages` 或 `ml_satisfies_x86_64_requirements`，fatal 模式返回 `LOAD_BADMACHO` | 无 | 无 | 有（只认 `_POSIX_SPAWN_FORCE_4K_PAGES`） | L781 同时认 `psa->psa_4k`；L986–1025 |
| `fourk_fatal_mode_enabled()`：正式版内核恒为 true，development 内核读 NVRAM `x86-64-compat-dev` | 无 | 无 | 有 | L704–720 |

来源：[1][2][3][4]。**[已证实]**

对问题 (1) 的回答：
- 在 Apple silicon 内核上，**只有 `ml_satisfies_x86_64_requirements()` 为真**时，`enforce_hard_pagezero` 才会被清除。4K spawn 路径本身**不会**清除它：一个只带 `com.apple.private.4k-pages` 的进程能拿到 4K 页，但仍然有 4GB 硬 pagezero。反过来，soft pagezero 也不依赖 4K 模式，授权进程在 16K 模式下同样可以拿到。
- `vm_map_has_hard_pagezero()` 的实现就是 `map->min_offset >= pagezero_size`。`vm_map_raise_min_offset()` 不允许把 `min_offset` 往回调（源码原话是 “Can't move min_offset backwards”）；`kern_mman.c` 中也没有其他 4GB 检查 [1]。所以授权进程只要 `mach_vm_deallocate` 掉 soft pagezero，就能在 0x4000（16K 模式）或 0x1000（4K 模式）以上映射任意低地址，其中包括 0x10000–0x7fffffff 和 `KUSER_SHARED_DATA` 的 0x7ffe0000。**[已证实：源码；本机无授权，未能实跑]**
- dappermint/winecx 的 `arm64` 分支提交 `9cc1f74`（2026-08-26，“ntdll: release the pagezero on macOS arm64”）正是这样做的：它在 `virtual_init()` 里遍历主映像的 `LC_SEGMENT_64`，对 `__PAGEZERO` 调用 `mach_vm_deallocate` [14]。同一分支的提交 `40ce8f4` 写明：没有 entitlement 时 loader 能启动，但 “cannot host a windows process yet” [15]。
- 07 号报告在本机做的实验（先释放 pagezero，再在 0x10000 做 FIXED 分配，得到 `KERN_INVALID_ADDRESS`）本身没有错，只是它测的是**未授权**进程。本机复测结果相同：`dealloc kr=0`，但 `alloc 0x10000 kr=1` [13]。

**设计推论**：soft pagezero 会一直占着低 4GB，直到 Wine 自己释放它。所以 dyld、libSystem 和 malloc 在 `main` 之前都不会占用低地址。Wine 应该在 loader 的 `main` 最早期释放 pagezero，然后立即按 x86_64 版 `WINE_RESERVE` 的布局重新 reserve 低区。arm64 可执行文件必须是 PIE，不能照搬 x86_64 用 `-segaddr` 固定地址的做法。**[推断]**

### 2. `com.apple.developer.cross-architecture-support(-unmanaged)` 是什么，能否申请

**一手证据** [6][7]：
- `osfmk/arm64/x86_64_compat.h` 注释说明：`ml_satisfies_x86_64_requirements()` 通过回调检查 “the necessary entitlements for x86-64 emulation features”，并以 `"com.apple.developer.cross-architecture-support"` 为例。开源版的 `x86_64_compat.c` 只有 `return false; /* Not supported. */`，真实实现不在开源树里。
- `mach_loader.c` 的遥测代码（仅 DEBUG 构建）区分了 `has_restricted_x86_64_entitlement`（对应 `cross-architecture-support`）和 `has_unrestricted_x86_64_entitlement`（对应 `-unmanaged`）[1]。
- `tests/map_jit_x86_64_compat.c` 的注释说明：带任一变体时，hardened runtime 下的 `MAP_JIT` 应当成功，并注明 “enforced by AMFI”。`tests/entitlements/mixed_pagesize_x86_{restricted,unrestricted}.entitlements` 则说明这两个变体也用于 4K 混合页测试 [7]。

**公开渠道核查**：Apple 的 entitlement 文档索引 JSON 里没有 cross-architecture、x86、x18、4k 等字样 [40]。“Supported capabilities (macOS)” 列表中也没有相关条目 [36]。Apple 论坛搜索 `cross-architecture-support` 没有相关帖子，WWDC26 也没有公开相关 session（只有 Rosetta 退役和 macOS 27 的 `game-test-tool`）[37][38]。**唯一的正式申请入口**是 Certificates, Identifiers & Profiles → App ID → **Capability Requests** 标签页，必须由 Account Holder 提交 [36]。这个入口需要登录，本次无法确认列表里是否有 “Cross-Architecture Support”。

**免费开源项目能否申请**：
- 从流程上说可以，前提是加入 Apple Developer Program（个人或组织均可，年费 99 美元）。费用减免只面向非营利组织、认证教育机构和政府实体，**不接受个人或单人企业** [36]。
- 批准与否由 Apple 自行裁量。dappermint 的原话是 “an entitlement apple grants at its own discretion. crossover's arm64 build carries it” [18]。
- `-unmanaged` 与普通变体的区别不明。从命名推测，“managed” 指需要申请的 managed capability，“unmanaged” 可能是自助可用的，或者不受某些使用场景限制 **[推断]**。本机已确认 ad-hoc 签名时两者都会被 AMFI 拒绝，所以至少需要团队证书加上 provisioning profile。
- **对开源项目的后果**：entitlement 绑定 Team ID 和 profile，**用户自己从源码构建的 Cider 无法获得 Engine A**，只有官方签名的发行版可以。**[推断，高置信]**

### 3. CrossOver Mac ARM64 Preview 如何运行 32 位和低地址程序；26.5 下限是否属实

- **版本**：NotProton README 要求 “CrossOver Preview 2026082”，“Preview” 这个词只出现在 README 里。`SupportedRunner.swift`（`main` @ `c09e0c4`，2026-09-26）给出对应关系 `releaseVersion "20260821"` = `bundleVersion "27.0.0.40921"`，并按 loader SHA256 区分 `flavor: nil`（Rosetta）和 `flavor: "fex"` 两个构建 [19][20][43]。
- **FEX 构建的 DLL 组成**：NotProton 为 fex 构建登记的 cleanNtdll/patchedNtdll 表只有 `.i386Windows` 和 `.aarch64Windows`，Rosetta 构建登记的则是 `.x86_64Windows` 和 `.i386Windows` [20]。**核查更正**：原文写的是“没有 x86_64-windows”，这说过头了。这张表是第三方的补丁表，只说明 NotProton 给哪些 ntdll 打补丁，不能证明 CX 的 fex 构建里没有其他 ntdll。`resolve.py` 对 aarch64 ntdll 的注释是 “carries the loader twice, once for the native side and once for the emulated guest”，并区分了 “guest” 版的 `LdrLoadDll` 和 `NtProtectVirtualMemory` [22]。据此推断，x64 程序走 **ARM64X ntdll + ARM64EC**，32 位程序走 **WoW64 + i386 ntdll**（其导出地址在 0x7bc0xxxx，位于低 2GB）。**[较可信]** 06 号报告把 ARM64EC 列为“强推断”，现在可以升级为 [较可信]，但证据仍是第三方补丁表加注释，不是 CX 的构建清单。
- **低地址从哪里来**：FEX 的 WoW64 模块在 `BTCpuProcessInit` 里明确 “Allocate the syscall/unixcall trampolines in the lower 2GB”（`Source/Windows/WOW64/Module.cpp` L573–576，`NtAllocateVirtualMemory` 的 ZeroBits 为 `(1U<<31)-1`）[29]。i386 guest 的映像和栈也必须在 4GB 以下。所以 CX 能跑 32 位程序，本身就证明它的进程有 soft pagezero，而这只能来自 cross-architecture entitlement。NotProton 的 `RunnerPatcher.swift` 定义了 `restrictedEntitlement = "com.apple.developer.cross-architecture-support"`，loader 带有这个 entitlement 时，NotProton 会跳过重签名（重签会让这个 entitlement 失效）[21]。**[较可信]**
- **26.5 下限**：CodeWeavers 2026-07-31 的博文本身返回 403，搜索摘要写的是 “The ARM64 pieces require macOS 26.5 or higher… if you are running an older OS, you will be running Intel Wine and Rosetta” [24]。AppleInsider 和 TUAW 的报道没有提到这个版本号 [25]。源码层面的解释是 26.5 才有公开的 `posix_spawnattr_set_4k_page_size_np`/`psa_4k`，以及改名后的 x18 API（26.4 的 `libsyscall/os/x18.c` 导出的还是 `os_custom_x18_abi`）[5][9]。soft pagezero 在 26.4 就已经存在，26.4 通过私有 `_POSIX_SPAWN_FORCE_4K_PAGES`（0x1000）也已经具备完整的授权 4K 路径。**核查补充**：源码本身并不严格要求 26.5，下限应理解为“公开 API 可用、x18 API 改名”，而不是“内核机制从 26.5 起才有”[3][4]。**[较可信]** CodeWeavers 博文（搜索摘要）还说 Mac ARM64 Preview 用的是定制版 FEX [24]。CX 是否确实用了 4K 模式（而不是 16K 加 Wine 模拟），还没有直接证据。
- **Wine MR !11638**（标题 “Mac: Support Apple silicon mac for running native windows on arm softwares”；作者 cqwrteur，GitLab 用户名 `trcrsired`；2026-08-11 开启，状态 opened，`has_conflicts` 为 true，未合并）走的是另一条路：不申请 entitlement，把 loader 做成 PIE，改用“动态 KUSER_SHARED_DATA（TPIDRRO_EL0−0x1000）”，并链接到 macOS 12 以便沿用旧版 x18 行为，只用来跑原生 WoA 程序 [27]。这不符合 Windows ABI，大量直接读 0x7ffe0000 的代码会出错，被上游接受的可能性低。**[推断]**
- **CX 27 源码**：截至 2026-09-26，CodeWeavers 源码页只列出 `crossover-sources-26.3.0.tar.gz`，Preview/27 的源码没有公开 [26]。Highball #6 称 Wine 部分会随 CX 27（2027 年初）的源码一起发布 [23]。dappermint 的 `arm64`/`arm64-1117` 分支是社区自行探索的结果，不是 CX 27 源码 [17]。

### 4. TSO：能否用 `thread_set_x86_64_compat` 拿到，拿不到的代价

- **接口**：SDK 26.5 的 `mach/mach_traps.h` 声明了 `kern_return_t thread_set_x86_64_compat(uint32_t enable)`，`libsystem_kernel.tbd` 中有导出。`Kernel.framework` 的 `kern/task.h` 在 `CONFIG_X86_64_COMPAT` 下声明了 `task_is_x86_64_compat()` [12]。开源 `syscall_sw.c` 中第 108 项是 `kern_invalid`（返回 4），本机调用得到 5，说明正式版内核有一个闭源的处理函数 [10][13]。从名字看它按**线程**生效。**[推断]**
- **需要什么授权**：大概率与 `ml_satisfies_x86_64_requirements` 同源，即 cross-architecture entitlement。Lina 的帖子称其“动态开关 TSO 及其他 x86-64 兼容特性” [39]。**[较可信，但未经一手验证]**
- **FEX 侧的改动**：`TSOHandlerConfig` 在构造时调用一次 `UnixLib::TryEnableHardwareTSO()`，Linux 下对应 `prctl(PR_SET_MEM_MODEL, PR_SET_MEM_MODEL_TSO)`，作用于整个进程 [29]。移植到 macOS 后要改成**在每个线程的 `ThreadInit`/`BTCpuThreadInit` 中调用 trap**。成功后调用 `CTX.SetHardwareTSOSupport(true)`，FEX 就不再生成屏障或 LRCPC 指令。
- **没有 TSO 的代价**（Apple 核心上）：
  - FEX 博客 2026-09-17 的微基准：M1 上的 acquire/LRCPC load “quite a bit lower than the baseline”；开启 TSO 模式后，store 只有普通 store 的 **76%**，load “basically matches” [31]。
  - FEX-2404（2024-04-05）：即使有 LRCPC/LRCPC2，内存模型模拟 “can have near a 10x performance hit”，主要出现在向量 load/store 上；推荐配置是开启 `TSOEnabled`、关闭 Vector/Memcpy TSO [32]。
  - 本机 M3：`FEAT_LRCPC2=1`，没有 LRCPC3（06 号报告实测）。
  - **没有找到“Apple 芯片、有无 TSO 对比”的游戏级数据**。
  - 结论：没有 TSO 时，要么牺牲正确性（关闭向量 TSO），要么牺牲性能，比 Rosetta 慢是可以预期的。**[推断]**

### 5. FEX Darwin 移植的具体范围与工作量

**上游现状（FEX-2609，注释标签 `587235f`，由 Ryan Houdek 于 2026-09-08 打标签）**：`CMakeLists.txt` 在第 64–66 行规定 “FEX only supports Linux and Windows”（`elseif (NOT (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))` 后接 `FATAL_ERROR`），并且明确拒绝 GCC 和 MSVC，所以 Windows 指的是 Clang/MinGW（llvm-mingw）构建 [29][42]：`ARCHITECTURE_arm64ec` 构建 `Source/Windows/ARM64EC`，普通 arm64 构建 `Source/Windows/WOW64` [29][30]。

| 模块 | 是否依赖 Linux | macOS 上要做什么 |
|---|---|---|
| FEXCore（MinGW 构建） | 否。`AllocatorHooks.h` 在 `_WIN32` 下用 `VirtualAlloc2(PAGE_EXECUTE_READWRITE, MEM_EXTENDED_PARAMETER_EC_CODE)` 分配代码缓存 | 依赖 Wine 在 macOS 上正确实现 RWX（见下面的 W^X） |
| `Source/Tools/LinuxEmulation/*`（syscalls、`SignalDelegator`、seccomp、`/proc` 仿真、futex、eventfd）、FEXServer、FEXInterpreter | 是 | **Wine 模式完全不用** |
| `Source/Windows/UnixLib/FEXUnixLib.cpp`（189 行）+ `UnixLib/CMakeLists.txt`（从同一源文件构建 `libwow64fex.so` 和 `libarm64ecfex.so` 两个 SHARED 库，私有链接 `rt`） | 是。**核查更正**：`__wine_unix_call_funcs` 共有 **7 个** unixcall，不只是开 TSO：① `SetHardwareTSOControl`（`prctl(PR_GET/SET_MEM_MODEL…TSO)`）② `SetKernelUnalignedAtomicControl`（`PR_ARM64_SET_UNALIGN_ATOMIC`）③ `Madvise` ④ `SetVMAName`（`PR_SET_VMA_ANON_NAME`）⑤ `GetSHMStatsVMA`（`shm_open`+`ftruncate`+`mmap(MAP_SHARED\|MAP_FIXED)`）⑥ `DeleteSHMStatsFile`（`shm_unlink`）⑦ `MapFile`（`mmap(PROT_READ, MAP_SHARED\|MAP_NORESERVE)`）[29] | 需要逐个提供 Darwin 实现或桩：① 改为按线程调用 `thread_set_x86_64_compat`，失败时返回错误，FEX 回退到 LRCPC ② macOS 没有等价接口，返回 NOT_SUPPORTED ③ Darwin 的 `madvise` 存在，但没有 `MADV_HUGEPAGE` 等 Linux 专有 advice，需要转换或忽略 ④ 做成 no-op ⑤⑥ 接口在 Darwin 上都存在，但 shm 名称受 `PSHMNAMLEN`=31 字节限制，需要缩短名称 ⑦ 可以直接用（SDK 26.5 定义了 `MAP_NORESERVE`）。构建时去掉 `rt`（macOS 没有 librt），并绕过顶层 CMake 的宿主检查：由 Cider 单独编译这一个文件，或者给上游提交 Darwin 分支。**[推断]** 其中 ② 失败后 FEX 的具体回退路径还需要读源码确认 |
| `Common/CPUFeatures.cpp` | 间接依赖：从注册表 `CentralProcessor\N` 的 `CP 4030` 等键读取 ID 寄存器 | 上游 Wine 的 `get_core_id_regs_arm64` 在非 Linux 平台是 `FIXME("stub")`，需要根据 `hw.optional.arm.FEAT_*` 合成这些值 [28] |

**Wine 侧（arm64-macOS unix 侧）是主要工作量**：
1. arm64 loader：修改 configure（参考 winecx `40ce8f4`），签名时加入 entitlement，最早期释放 pagezero，在 0x7ffe0000 映射 `KUSER_SHARED_DATA`（上游硬编码为 `user_shared_data = (void *)0x7ffe0000`）[14][15][28]；
2. 可选的 4K 模式：由启动器通过 `posix_spawnattr_set_4k_page_size_np` spawn loader；
3. **x18**：PE 侧的 Windows ABI 用 x18 存 TEB，unix 侧遵循 Apple ABI。在 `__wine_syscall_dispatcher`、`__wine_unix_call_dispatcher`、用户回调和信号处理入口，都要成对调用 `os_set_custom_x18_abi_enabled(false/true)`。这个函数“严格切换，重复设置同一状态会 abort”，关闭时 x18 会被销毁，TEB 必须另外保存 [8][12]；
4. 信号：上游 `signal_arm64.c` 已经有 `__APPLE__` 的寄存器宏和 FPU 保存/恢复 [28]，还需要补上 x18 状态处理和 ARM64EC 上下文转换；
5. **W^X**：本机实测，没有 `MAP_JIT` 时 RWX 映射返回 EACCES，RW 改成 RX 的 mprotect 可以成功。在 hardened runtime 加 `allow-jit` 的条件下，本机**能同时创建 8 个 1MB 的 MAP_JIT 区**，而 Apple 文档写的是 “can only create one memory region” [13][35]。FEX 代码缓存有两种方案：(a) 走 MAP_JIT，在 Emitter 写入时切换 per-thread 写权限；(b) 走 winecx 的“缺页翻转”方案（`6baebec`：按缺页类型在 RW 和 RX 之间切换）[16]，这种方案实现简单，但对 JIT 密集的负载很慢；
6. 同步：Wine 在 macOS 上已经用 `os_sync_wait_on_address` 实现进程内 futex 语义，不需要 eventfd（07 号报告）。

**工作量估算**（1 名资深 Wine 开发者 + 1 名 JIT/FEX 开发者，目标是“CX Preview 同级”的 alpha）：FEXUnixLib 与 TSO 0.5–1 PM（核查更正：原为 0.5 PM，未计入另外 6 个 unixcall 的 Darwin 实现或桩，以及绕过 CMake 宿主检查的工作）；Wine arm64-macOS 宿主（loader、pagezero、4K、x18、信号、ID 寄存器）3–4 PM；W^X/JIT 适配 1–2 PM；ARM64EC 在 macOS 上的异常、展开和上下文问题 1–2 PM；winemac.drv arm64 与 DXMT arm64x 集成 1–2 PM；签名、公证、profile 和 CI 0.5–1 PM；兼容性回归 2–3 PM。**合计约 9–15 人月，日历时间 5–7 个月**。如果 CX 27 源码在 2027 年初公开，Wine 侧可以减少约 40–60%。**[推断]**

### 6. ARM64 DXMT 与 wine-mono 11.3 arm64

- **DXMT**：`main` 分支有 `build-arm64ec.txt`：交叉编译器为 `arm64ec-w64-mingw32-*`，参数 `-marm64x`，`cpu_family='aarch64'`。该文件首次提交于 2026-03-09，最近一次改动是 2026-09-17。`meson.build` 对 aarch64 把 PE 文件装到 `aarch64-windows`，unix 侧用 `-arch arm64` 装到 `aarch64-unix` [33]。最新发布的 v0.80（2026-04-23，从这一版起改为 LGPL）的发布说明没有提到 arm64 [33]。**可用性判断**：ARM64X 的 `d3d11.dll` 可以同时服务原生 ARM64 程序和 ARM64EC 下的 x64 程序，翻译层以原生速度运行，这正是 CX Preview “含 ARM64 DXMT、DX11 可用”的来源 [23][24]。**32 位 D3D11 游戏**只能加载 i386 版 DXMT，由 libwow64fex 模拟执行，CPU 开销明显更高 **[推断]**。D3D12：D3DMetal 只有 x86_64 版本（NotProton PR #4），在 Engine A 上暂时没有方案 [19]。
- **wine-mono 11.3.0（2026-08-17）**：发布说明原文是 “An ARM64 build has been added. This includes a new port of Mono to ARM64 Windows. Several components, not including the Mono runtime itself, are also built as arm64ec”。附件里有 `wine-mono-11.3.0-arm64.msi`，同时还有 `-arm64.tar.xz` 和 `-dbgsym-arm64.tar.xz`。核查人员没能加载发布页的附件列表，本次改用 GitHub Releases API 核实了这些文件名（`published_at` 2026-08-17T18:24:41Z）[34]。AnyCPU 的 .NET 程序可以原生运行在 arm64 Mono 上；x64-only 的混合模式程序集仍有问题。在 macOS 上，Mono JIT 依赖 Wine 正确模拟 RWX（winecx 提交说明中写到，不处理的话 “a JIT never runs a single block”）[16]。**在 macOS 上的可用性未经验证**，需要放进原型第 3 步。

### 7. 原生 Windows ARM64 程序能否直接在 Engine A 上运行

技术上可以，Wine 在 Linux ARM64 上早已支持。macOS 上的前提条件：
- x18 必须能当 TEB 用。`com.apple.security.custom-x18-abi-toggle` **可以自签**，本机已验证；
- `KUSER_SHARED_DATA` 位于 0x7ffe0000，需要 cross-architecture entitlement，或者 MR !11638 那样的非标准改动 [27]；
- ARM64 PE 默认 4K 节对齐，需要 4K 模式或 Wine 的 4K 模拟；
- 程序自带 JIT（V8、.NET）时依赖 RWX 模拟。

**无授权**时，只有“不碰 0x7ffe0000、不需要低地址”的原生 ARM64 程序能跑，可以作为小众模式。**有授权**时，可以把它作为 Engine A 的附带能力。

### 8. 四份报告的矛盾裁决

| 论点 | 原报告 | 裁决 |
|---|---|---|
| arm64 无法使用低 4GB，加载器中没有 entitlement 例外 | 07 | **部分错误**：依据的 `main` 是 26.0 的代码；26.4+ 有 soft pagezero 例外 [1][3] |
| 需要 entitlement 才能释放 pagezero（dappermint 的说法，标为 [低]） | 02 | **升级为 [已证实]**（源码）+ [较可信]（CX 带有该 entitlement）[1][6][18][21] |
| `ml_satisfies_x86_64_requirements` 的实现未公开 | 06 | 更正：开源版是恒 false 的桩，头文件注明了它检查哪个 entitlement [6] |
| x18 在非 hardened 二进制中可以正常切换 | 06 | **错误**：不带 toggle entitlement 时 x18 在上下文切换中丢失；该 entitlement 可以自签 [8][13] |
| hardened runtime 下只能有 1 个 MAP_JIT 区 | 06/07 | 文档如此，但 26.5 本机实测未强制执行（8/8 成功）；不要依赖这一行为 [13][35] |
| 26.5 下限只见于第三方转述 | 14 | 有 CW 博文摘要，并有 xnu 源码中 26.5 新增 API 作为解释 [5][9][24]；核查补充：26.4 的内核机制已经齐备（私有 0x1000 标志），所以源码并不严格要求 26.5 [3][4] |
| CX Preview 使用 ARM64EC 是推断 | 02/06/14 | 升级为 [较可信]（NotProton 对 ARM64X ntdll 的注释）[22]；核查更正：NotProton 的补丁表不能证明构建里没有 x86_64-windows ntdll [20] |
| FEX 宿主侧只需开 TSO | 本文初稿 | **核查更正**：FEXUnixLib 有 7 个 unixcall，都需要 Darwin 实现或桩，CMake 也要绕过宿主检查 [29] |
| 4K spawn 自 26.0 可用 | 06 | 更正：SDK 头文件的标注与实现不符；实现出现在 26.5，26.4 只有私有标志 [4][5] |

---

## 对 Cider 的启示与建议

### 决策表

| 场景 | 前提 | x64 程序 | 32 位 x86 程序 | 需要低地址的 x64 程序（LuaJIT、部分 DRM 等） | 原生 ARM64 PE | 性能 | 结论 |
|---|---|---|---|---|---|---|---|
| **A1：拿到授权** | Developer ID + 带 `cross-architecture-support` 的 profile；macOS ≥ 26.5 | 可运行（ARM64EC+FEX），目标与 CX Preview 同级 | 可运行（WoW64+libwow64fex），但 i386 系统 DLL 和 i386 DXMT 全部被模拟，开销更大 | 可运行（soft pagezero，最低地址 1 页） | 可运行 | 有望使用硬件 TSO（待验证）；4K 页为原生 | **GO** |
| **A2：无授权** | 只用公开能力和可自签的能力（x18 toggle、MAP_JIT） | 上游 Wine 无法建立进程（0x7ffe0000）；大改后只有部分程序可跑；4K 模拟只适合“简单程序”；只有软件 TSO | **完全不可用**（WoW64 与 FEX trampoline 都要求 <2–4GB） | 不可用 | 部分可用 | 明显慢于 Rosetta | **产品 NO-GO**；只作为研发或小众模式 |
| **A3：开发者降级环境** | 测试机关闭 SIP，并设置 `amfi_get_out_of_my_way=1` 后自签受限 entitlement | 预期与 A1 相同 **[推断，待验证]** | 同 A1 | 同 A1 | 同 A1 | 同 A1 | 只用于原型，不能分发 |
| R：Rosetta 引擎 | macOS ≤ 27；macOS 28 起只保留“老游戏”子集 | 现有方案 | 可运行 | 可运行 | 不适用 | 最好 | 过渡方案 |

**最低 macOS**：Engine A 定为 **26.5**。26.4 理论上可以用私有 4K 标志和旧名 x18 API 实现，但不建议。核查补充：26.4 的授权 4K 路径和 soft pagezero 在源码中都已完整，所以定 26.5 的理由是公开 API（`posix_spawnattr_set_4k_page_size_np`）、x18 API 名称与 SDK 一致，并与 CX 公开的门槛对齐，而不是内核能力的硬限制 [3][4][24]。26.3 及更早在机制上不可能，因为加载器没有 soft pagezero，4K 标志也只存在于 DEBUG/DEVELOPMENT 内核 [4][41]。

### 优先级建议

1. **P0（本周）：申请 entitlement，这是整个方向的前提。** Account Holder 加入 ADP，在 App ID 的 Capability Requests 标签页查找 Cross-Architecture Support。如果列表里没有，就通过 DTS 或 Feedback Assistant 提交用例说明：开源 Windows 兼容层、Developer ID 分发、Rosetta 退役后的替代方案。同时评估与 Highball、dappermint 等项目联合提出申请。
2. **P0：把 Engine A 的状态写进路线图。** 在授权结果出来之前，Engine R（Rosetta）仍然是 macOS ≤ 27 上的主力；V 后端（虚拟机）作为 macOS 28 的兜底方案，需要提前到与 Engine A 并行开展。
3. **P1：按下文的三步原型推进。** 能力探针工具 `cider-probe` 要作为产品组件保留，在运行时决定可用的后端。
4. **P1：FEX 以上游 FEX-2609 的 PE 构建为基础**，不要 fork Linux 部分。macOS 相关改动集中在 `FEXUnixLib`。核查更正后的范围是：7 个 unixcall 全部要有 Darwin 实现或桩，其中 TSO 按线程开启，unaligned-atomic 和 VMA 命名做成桩，`madvise` 转换 advice，shm 名称控制在 31 字节以内；构建时去掉 `rt`，并绕过顶层 CMake 的宿主检查。尽量把 Darwin 版 unixlib 提交给上游。
5. **P2：在 CX 27 源码发布（预计 2027 年初）后与其对齐**，避免重复实现 Wine 侧的 arm64-macOS 改动。

### 三步原型计划（在 M3 / 26.5 开发机上执行）

- **第 1 步（约 1 周）：能力探针和授权自检。** 把本次 scratchpad 中的实验整理成 `cider-probe`（C 语言，只需 CLT），检测以下项目并输出 JSON：
  - 4K spawn：公开 API 与 0x1000 私有标志，区分 88 和成功；
  - soft pagezero：释放后在 0x10000 和 0x7ffe0000 做 FIXED 分配；
  - `thread_set_x86_64_compat`：区分返回 4、5 和 0；
  - x18 是否保留：分别测带和不带 `custom-x18-abi-toggle`；
  - hardened runtime 下的 MAP_JIT 数量；
  - `hw.optional.arm.FEAT_*`。

  另外安装完整 Xcode 26.x，用个人或付费团队尝试生成包含 `…cross-architecture-support-unmanaged` 的开发 profile，确认 `-unmanaged` 是否可以自助获得。
- **第 2 步（2–3 周）：无授权的 “Engine A-lite”。**
  - 用上游 Wine 11.18 加 llvm-mingw（aarch64、arm64ec）构建 aarch64-apple-darwin 版本，移植 winecx 的 `40ce8f4`（configure）和 `6baebec`（W^X 翻转）；
  - 加入 x18 分发器切换（自签 toggle entitlement）和 sysctl→ID 寄存器；
  - 临时绕过 0x7ffe0000（仅限原型）；
  - 跑通原生 ARM64 PE 控制台程序，测量 syscall 和 unixcall 的往返开销。

  这一步用来验证工具链、x18、16K 页模拟和信号路径，与授权无关。
- **第 3 步（3–5 周）：授权路径端到端验证。** 需要 Apple 签发的 profile，或者在**专用测试机或 macOS VM** 中由用户手动关闭 SIP 并设置 `amfi_get_out_of_my_way=1`。不建议在主力机上这样做，而且这一做法本身有待验证。
  - loader 签名时加入 `cross-architecture-support`、`custom-x18-abi-toggle` 和 `allow-jit`；
  - 通过 `posix_spawnattr_set_4k_page_size_np` 启动 loader，释放 pagezero 后映射 `KUSER_SHARED_DATA`；
  - 接入 FEX-2609 的 `libarm64ecfex.dll` 和 `libwow64fex.dll`，以及移植后的 FEXUnixLib。TSO 按线程开启，失败时回退到 LRCPC2；其余 6 个 unixcall 做成 Darwin 实现或桩。验收标准：7 个 unixcall 都有单元测试，并确认 `SetKernelUnalignedAtomicControl` 返回 NOT_SUPPORTED 后 FEX 仍能正确处理非对齐原子操作；
  - 依次跑 x64 和 i386 控制台测试、DXMT arm64x 的 D3D11 示例、wine-mono arm64 的 WinForms 示例；
  - 对比 TSO 开和关、Engine A 与 Engine R（后者需要用户先安装 Rosetta）的 CPU 基准。

---

## 风险

1. **授权风险（最高）**：Apple 可能不向免费或开源项目发放 cross-architecture entitlement，或者只发给商业合作伙伴。若如此，Engine A 无法成为产品。macOS 28 以后，Cider 只能依靠 VM，或者停留在 macOS 27。
2. **语义变动**：restricted 与 unrestricted 两个变体的差异、TSO trap 的授权条件都属于私有实现，macOS 27 及以后可能改变。26.6 和 27 的 xnu 源码尚未公开。
3. **开源分发**：entitlement 绑定官方签名，社区自行构建的版本拿不到 Engine A；profile 和证书的管理成为单点。
4. **性能**：没有 TSO 时 FEX 会明显慢于 Rosetta；32 位程序需要模拟整个 i386 系统 DLL 栈；8GB 内存下 JIT 缓存与 4K 页表的开销叠加。
5. **正确性**：x18 严格切换（重复设置会 abort）、信号嵌套、per-thread 的 MAP_JIT 语义与 Windows 的全局 RWX 语义不一致，都容易出现难以复现的崩溃。
6. **降级环境方案**（关闭 SIP 加 AMFI boot-arg）只能用于开发。如果在文档中引导普通用户这样做，会带来安全和支持上的风险。

## 未解问题

1. Capability Requests 中是否列有 Cross-Architecture Support？`-unmanaged` 能否自助获得，是否适用于 Developer ID？
2. `thread_set_x86_64_compat` 需要哪个 entitlement？除 TSO 外还开启了哪些特性？每次调用的开销是多少？
3. CX Preview 实际运行在 4K 页还是 16K 页（加模拟）？它的 FEX 代码缓存用的是 MAP_JIT 还是缺页翻转？fex 构建除了 i386 和 aarch64 ntdll，是否还带 x86_64-windows ntdll（NotProton 的补丁表回答不了这个问题）？
4. 26.5 上 MAP_JIT “单区限制”未被执行，这在 Developer ID 签名或公证后是否依然成立？
5. macOS 27 的 `game-test-tool` 所说的 “new underlying system behavior” 是否会影响 Wine 进程？
6. 在 SIP 关闭的 VM 中，`amfi_get_out_of_my_way` 能否让 `IOVnodeHasEntitlement` 对自签的受限 entitlement 返回真？

## 参考来源

1. xnu-12377.121.6 `bsd/kern/mach_loader.c`（L704–720 fatal 模式、L743/753/781、L986–1025 4K 检查、L1082–1093 soft pagezero 豁免、L2604–2637 1 页硬 pagezero 与 soft 映射）：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/kern/mach_loader.c ；`vm_map.c`（`vm_map_has_hard_pagezero`、`vm_map_raise_min_offset`）、`kern_mman.c` 位于同一标签
2. xnu `main`（= xnu-12377.1.9，2025-10-16 导入）的 `mach_loader.c`（L700、L849、L874–903）：https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/mach_loader.c
3. xnu-12377.101.15（26.4）的 `mach_loader.c`：已有 soft pagezero 和 4K 授权检查，但没有 `psa_4k`：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.101.15/bsd/kern/mach_loader.c
4. xnu-12377.81.4 与 xnu-12377.101.15 的 `bsd/sys/spawn.h`：`_POSIX_SPAWN_FORCE_4K_PAGES` 在 26.3 仅定义于 DEBUG/DEVELOPMENT，26.4 起为 PRIVATE：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.101.15/bsd/sys/spawn.h
5. xnu-12377.121.6 的 `libsyscall/wrappers/spawn/posix_spawn.c`（`posix_spawnattr_set_4k_page_size_np` 设置 `psa_4k`）和 `bsd/sys/spawn_internal.h`（“Force 4k address space”）：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/libsyscall/wrappers/spawn/posix_spawn.c
6. xnu-12377.121.6 的 `osfmk/arm64/x86_64_compat.c`（恒 false 的桩）与 `x86_64_compat.h`（entitlement 回调说明）：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/osfmk/arm64/x86_64_compat.h
7. xnu-12377.121.6 的 `tests/map_jit_x86_64_compat.c`、`tests/entitlements/{map_jit,mixed_pagesize}_x86_*`、`tests/Makefile`：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/tests/map_jit_x86_64_compat.c
8. xnu-12377.121.6 的 x18 相关代码：`libsyscall/os/x18.c`、`tests/x18_toggle.c`、`tests/x18_unentitled.c`、`tests/x18-toggle-entitlements.plist`（同时含 `custom-x18-abi-toggle` 与 `com.apple.private.4k-pages`：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/tests/x18-toggle-entitlements.plist ）、`osfmk/arm64/pcb.c`（`preserve_x18_entitled`）、`machine_machdep.h`、`locore.s`：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/osfmk/arm64/pcb.c
9. xnu-12377.101.15 的 `libsyscall/os/x18.c`（旧名 `os_custom_x18_abi`/`os_custom_x18_abi_get`）：https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.101.15/libsyscall/os/x18.c
10. xnu-12377.121.6 的 `osfmk/mach/syscall_sw.h`（`thread_set_x86_64_compat,-108`）与 `osfmk/kern/syscall_sw.c`（`/* 108 */ kern_invalid`）
11. xnu 标签列表与 main 头提交：https://api.github.com/repos/apple-oss-distributions/xnu/tags
12. 本机 SDK 26.5 头文件：`usr/include/spawn.h`（标注 macos(26.0)）、`os/arch/arm64.h`（macos(26.4)，含详细注意事项）、`mach/mach_traps.h`、`Kernel.framework/Headers/kern/task.h`（`CONFIG_X86_64_COMPAT`）、`usr/lib/system/libsystem_kernel.tbd`
13. 本机实验（2026-09-27，M3 / 26.5 25F71）：4K spawn（公开 API 与私有标志均返回 88）；释放 pagezero 后 FIXED 分配返回 1；trap 返回 5；两个 cross-architecture 变体在 ad-hoc 签名下退出码均为 137；x18 无 entitlement 0/2000、带 toggle entitlement 2000/2000，一次开关约 4–9 ns；MAP_JIT：hardened 无 entitlement 0/8、带 allow-jit 8/8；RWX 不带 MAP_JIT 返回 EACCES
14. dappermint/winecx `9cc1f74`（release the pagezero）：https://github.com/dappermint/winecx/commit/9cc1f7435e
15. dappermint/winecx `40ce8f4`（experimental arm64-native build support）：https://github.com/dappermint/winecx/commit/40ce8f4602
16. dappermint/winecx `6baebec`（W^X 翻转）：https://github.com/dappermint/winecx/commit/6baebec954
17. dappermint/winecx 分支列表（`arm64`、`arm64-1117`，2026-08-26/27 的提交）：https://github.com/dappermint/winecx/branches
18. dappermint/winecx-gptk README（entitlement “grants at its own discretion… crossover's arm64 build carries it”）：https://github.com/dappermint/winecx-gptk
19. NotProton README（CrossOver Preview 2026082；FEX 构建处于早期状态）：https://github.com/NotProtonNot/NotProton （Gcenx/NotProton 同样可访问）
20. NotProton `SupportedRunner.swift`（20260821 = 27.0.0.40921；NotProton 只为 fex 构建登记 i386 与 aarch64 两份 ntdll 的补丁哈希，Rosetta 构建登记的是 x86_64 与 i386；不代表 CX 构建的完整文件清单）：https://github.com/NotProtonNot/NotProton/blob/main/app/Sources/NotProtonApp/Model/SupportedRunner.swift
21. NotProton `RunnerPatcher.swift`（`restrictedEntitlement`）：https://github.com/NotProtonNot/NotProton/blob/main/app/Sources/NotProtonApp/Model/RunnerPatcher.swift
22. NotProton `ntdll-patch/resolve.py`（“carries the loader twice”）：https://github.com/NotProtonNot/NotProton/blob/main/ntdll-patch/resolve.py
23. Highball issue #6（2026-08-24）：https://github.com/gauthierpiarrette/highball/issues/6
24. CodeWeavers 博文 2026-07-31（403，采用搜索摘要）：https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac
25. AppleInsider 2026-07-31 与 TUAW 2026-08-02（均未提及 26.5 和 FEX）：https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears ；https://www.tuaw.com/2026/08/02/crossover-goes-native-on-apple-silicon/
26. CodeWeavers 源码页（只有 26.3.0）：https://www.codeweavers.com/crossover/source
27. Wine MR !11638（作者 GitLab 用户名 `trcrsired`，即 cqwrteur；2026-08-11T11:42Z 开启，opened，`has_conflicts` true）：https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/merge_requests/11638 ；https://gitlab.winehq.org/wine/wine/-/merge_requests/11638
28. Wine master（VERSION 11.18）：`dlls/ntdll/unix/virtual.c`（`user_shared_data = 0x7ffe0000`）、`signal_arm64.c`（`__APPLE__` 分支）、`system.c`（`get_core_id_regs_arm64` 在非 Linux 平台为桩）：https://github.com/wine-mirror/wine/tree/master/dlls/ntdll/unix
29. FEX-2609 源码：`CMakeLists.txt` L56–66（拒绝 GCC/MSVC；L64–66 拒绝非 WIN32/Linux 宿主）：https://raw.githubusercontent.com/FEX-Emu/FEX/FEX-2609/CMakeLists.txt ；`Source/Windows/UnixLib/FEXUnixLib.cpp`（189 行，7 个 unixcall）：https://raw.githubusercontent.com/FEX-Emu/FEX/FEX-2609/Source/Windows/UnixLib/FEXUnixLib.cpp ；`UnixLib/CMakeLists.txt`（`wow64fex`/`arm64ecfex` SHARED `.so`，链接 `rt`）：https://raw.githubusercontent.com/FEX-Emu/FEX/FEX-2609/Source/Windows/UnixLib/CMakeLists.txt ；`Common/TSOHandlerConfig.h`；`Common/CPUFeatures.cpp`；`WOW64/Module.cpp`（L573–576，低 2GB trampoline）：https://github.com/FEX-Emu/FEX/blob/FEX-2609/Source/Windows/WOW64/Module.cpp ；`FEXCore/include/FEXCore/Utils/AllocatorHooks.h`：https://github.com/FEX-Emu/FEX/tree/FEX-2609/Source/Windows
30. FEX 发布页（FEX-2609，2026-09-08）：https://github.com/FEX-Emu/FEX/releases
31. FEX 博客 The scourge of x86 emulation（2026-09-17）：https://fex-emu.com/Scourge-of-emulation/ ；Hackaday 转载报道（2026-09-19）：https://hackaday.com/2026/09/19/emulating-memory-access-how-hard-can-it-be/
32. FEX 2404 Tagged!（2024-04-05，“near a 10x performance hit”）：https://fex-emu.com/FEX-2404/
33. DXMT：`build-arm64ec.txt`、`meson.build`、发布页（v0.80，2026-04-23）：https://github.com/3Shain/dxmt
34. wine-mono 11.3.0（2026-08-17，arm64 MSI）：https://github.com/wine-mono/wine-mono/releases/tag/wine-mono-11.3.0 ；附件清单：https://api.github.com/repos/wine-mono/wine-mono/releases/tags/wine-mono-11.3.0
35. Apple：Porting just-in-time compilers to Apple silicon（“only create one memory region with the MAP_JIT flag”）：https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon
36. Apple 开发者账户帮助：Capability requests、Supported capabilities (macOS)、Membership fee waiver：https://developer.apple.com/help/account/capabilities/capability-requests/ ；https://developer.apple.com/help/account/reference/supported-capabilities-macos/ ；https://developer.apple.com/support/membership-fee-waiver/
37. Apple Developer News 2026-09-01（Rosetta 变更；老游戏继续支持；未提及模拟器授权）：https://developer.apple.com/news/?id=w5ngl9k2
38. macOS 26.4 与 macOS 27 发布说明（Rosetta 通知；`game-test-tool`）：https://developer.apple.com/documentation/macos-release-notes/macos-26_4-release-notes ；https://developer.apple.com/documentation/macos-release-notes （27）
39. Hoshino Lina 在 X 上的帖子（2026-04-21，由雪花 ID 换算；采用搜索摘要）：https://x.com/Lina_Hoshino/status/2046437088997130488
40. Apple entitlement 文档索引 JSON（未检索到 cross-architecture）：https://developer.apple.com/tutorials/data/documentation/bundleresources/entitlements.json
41. `apple-oss-distributions/distribution-macOS` 的 `release.json`（macos-260 → xnu-12377.1.9、macos-263 → xnu-12377.81.4、macos-264 → xnu-12377.101.15、macos-265 → xnu-12377.121.6）：https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-265/release.json ；https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-264/release.json ；https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-263/release.json ；https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-260/release.json
42. FEX-2609 注释标签对象（`587235f`，tagger Ryan Houdek，2026-09-08）：https://api.github.com/repos/FEX-Emu/FEX/git/tags/587235f431cecd7492f683c68a2eb8faf2df07e1
43. NotProton `main` 头提交 `c09e0c4`（2026-09-26）：https://api.github.com/repos/NotProtonNot/NotProton/commits/main

## 事实核查记录

> 独立核查于 2026-09-27 完成（核查人员读取了 xnu 各标签的源码并在本机 M3 / macOS 26.5 25F71 上复测；本次修订另外通过 GitHub API 和 distribution-macOS 补查）。表中的更正以本节为准，正文相关位置已标注“核查更正/核查补充”。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| xnu-12377.121.6（26.5）`load_machfile()` 在 ARM64 4GB `vm_map_has_hard_pagezero` 检查之前，对 `ml_satisfies_x86_64_requirements(check_ent)` 为真的进程置 `enforce_hard_pagezero = false`（约 L1082–1085）；`load_segment()` 使用 1 页硬 PAGEZERO 加 soft PAGEZERO（约 L2604–2637）。`main`（= 12377.1.9）和 81.4 没有这段逻辑，101.15 有 | 证实 | 四个标签逐一 grep 过：121.6 的 L1082–1083 是豁免逻辑，L1089 是 4GB 检查，L2608–2609 是 “use only a 1-page "hard" PAGEZERO”。101.15 的逻辑和行号都相同，两版 `mach_loader.c` 只在 L781（`psa_4k`）有差异。main 与 81.4 只有普通 4GB 检查。main HEAD `f6217f8` 就是 xnu-12377.1.9（2025-10-16）。**附带更正**：§1 中“.101.15 = 26.4”原标为 [推断]，现已由 distribution-macOS 的 `release.json` 证实为 [已证实]（260→1.9、263→81.4、264→101.15、265→121.6）[1][2][3][41] |
| 开源 `x86_64_compat.c` 中的 `ml_satisfies_x86_64_requirements()` 是恒返回 false 的桩；`x86_64_compat.h` 点名 `com.apple.developer.cross-architecture-support`；xnu 测试把两个变体都当作在 hardened runtime 下授予 `MAP_JIT` 的 x86-64 emulation entitlement；26.5 上 ad-hoc 签名带任一变体都会在 exec 时被杀 | 证实 | 函数体是 `/* Not supported. ... */ return false;`。`tests/Makefile` 的 `_restricted`/`_unrestricted` 目标都用 `-o runtime` 签名，并期望 `MAP_JIT` 成功。本机复测：带任一 entitlement 时退出码 137，不带时正常返回 7。注意：正式版内核的真实实现是闭源的，它在这两个 entitlement 之外还检查什么，无法看到 [6][7][13] |
| `posix_spawnattr_set_4k_page_size_np()`（`psa_4k`）和内核里的 `psa->psa_4k \|\|` 检查首次出现在 121.6（26.5），尽管 SDK 标注为 `macos(26.0)`；101.15 只认私有 `_POSIX_SPAWN_FORCE_4K_PAGES`（0x1000）；没有授权时返回 `EBADMACHO`(88) | 证实 | 121.6：`posix_spawn.c` L2653 是 API 定义，`mach_loader.c` L781 与 `kern_exec.c` L7280 是检查。fatal 路径在 L990–1025，正式版内核上 `fourk_fatal_mode_enabled()` 恒为真。本机复测返回 88。**核查补充**：26.4 已经能通过 0x1000 私有标志走完整的授权 4K 路径，soft pagezero 也已具备，所以源码并不严格要求 26.5。26.5 下限更合理的解释是公开 API 加上 x18 API 改名；摘要、§3、决策表下的“最低 macOS”和 §8 都已相应补充 [3][4][5] |
| 26.5 上，不带 entitlement 调用 `os_set_custom_x18_abi_enabled(true)` 时 x18 在上下文切换中不保留（0/2000）；ad-hoc 签名带 `com.apple.security.custom-x18-abi-toggle` 后保留（2000/2000）；101.15 中的函数名是 `os_custom_x18_abi()`/`os_custom_x18_abi_get()` | 证实 | 本机复测结果一致，未授权时内核还会清掉标志（enabled 变回 0）。`pcb.c machine_switch_cpu_data()` 只有在 `task->preserve_x18_entitled` 时才保存 PRESERVE_X18 位。旧符号在 26.5 上无法解析。补充：`tests/x18-toggle-entitlements.plist` 同时带 `com.apple.private.4k-pages`，但切换 x18 不需要它（已写入摘要）[8][9][13] |
| NotProton `SupportedRunner.swift` 固定了 CrossOver Preview releaseVersion 20260821 = bundleVersion 27.0.0.40921 的 Rosetta 和 fex 两个构建；fex 构建只有 i386-windows 和 aarch64-windows 两份 ntdll；`resolve.py` 写明 aarch64 ntdll “carries the loader twice”；`RunnerPatcher.swift` 定义了 `restrictedEntitlement` 且不重签带有它的 loader | 部分属实 | **更正**：NotProton（`main` @ `c09e0c4`）确实登记了两个构建（27.0.0.40921 / 20260821，flavor nil 和 `"fex"`），fex 构建的 cleanNtdll/patchedNtdll 表也确实只有 i386-windows 和 aarch64-windows。但这只说明 NotProton 给哪些 ntdll 打补丁，与 ARM64X/ARM64EC 相符，**不能证明构建里没有其他 ntdll**。“Preview” 一词来自 README，不在 `SupportedRunner.swift` 中。`resolve.py` 的引文和 `RunnerPatcher` 的行为（L240 定义、L144 跳过）逐字核对无误。CodeWeavers 2026-07-31 博文摘要说 Mac ARM64 Preview 使用定制 FEX，并要求 26.5+。摘要中的“由此可以确认”已改为“由此推断”，§3、§8、参考 [20] 和未解问题 3 已同步修改 [19][20][21][22][24][43] |
| FEX-2609（2026-09-08 打标签）的 `CMakeLists.txt` 只接受 Linux 或 Windows（MinGW）宿主；Wine 模式下唯一的宿主原生组件是 189 行的 `FEXUnixLib.cpp`，它通过 `prctl(PR_SET_MEM_MODEL_TSO)` 开启硬件 TSO；WOW64 模块把 syscall/unixcall trampoline 分配在低 2GB | 部分属实 | **更正**：标签 `587235f` 由 Ryan Houdek 于 2026-09-08 打出；L64–66 拒绝非 WIN32/Linux 宿主，同时拒绝 MSVC 和 GCC，所以 Windows 指 Clang/MinGW。`FEXUnixLib.cpp` 确实是 189 行，编译为 `libwow64fex.so` 和 `libarm64ecfex.so` 并链接 `rt`，但它导出 **7 个 unixcall**，不只是 TSO：`SetHardwareTSOControl`、`SetKernelUnalignedAtomicControl`（`PR_ARM64_SET_UNALIGN_ATOMIC`）、`Madvise`、`SetVMAName`（`PR_SET_VMA`）、`GetSHMStatsVMA`（`shm_open`/`mmap`）、`DeleteSHMStatsFile`、`MapFile`，每个都需要 Darwin 实现或桩。`WOW64/Module.cpp` L573–576 用 ZeroBits `(1U<<31)-1` 分配 trampoline，属实。已修改摘要、§5 的上游现状和表格（逐项给出 Darwin 映射）、工作量（FEXUnixLib 由 0.5 PM 改为 0.5–1 PM，合计由 9–14 改为 9–15 人月）、P1 建议第 4 条和原型第 3 步（增加 7 个 unixcall 的验收标准）[29][42] |
| 26.3 及更早的正式版内核完全没有 4K spawn 路径；`_POSIX_SPAWN_FORCE_4K_PAGES` 只在 DEBUG/DEVELOPMENT 构建中定义 | 证实 | 81.4 的 `spawn.h` 把 `#define _POSIX_SPAWN_FORCE_4K_PAGES 0x1000` 包在 `#if (DEBUG \|\| DEVELOPMENT)` 中；`mach_loader.c` L732 和 `kern_exec.c` L6844 的使用处受 `_POSIX_SPAWN_FORCE_4K_PAGES && PMAP_CREATE_FORCE_4K_PAGES` 保护，在正式版中会被编译掉。101.15 中该标志在 PRIVATE 下无条件定义 [4][41] |
| Mach trap `thread_set_x86_64_compat`（-108）在开源 xnu 中是 `kern_invalid`；SDK 26.5 的 `mach_traps.h` 有声明，`libsystem_kernel.tbd` 有导出；本机未授权调用返回 `KERN_FAILURE`(5)，说明存在闭源的真实处理函数 | 证实 | `syscall_sw.c` 中是 `/* 108 */ MACH_TRAP(kern_invalid, 0, 0, NULL)`；SDK 声明位于 L305。本机返回 5，不是 `kern_invalid` 应返回的 4。它需要哪个 entitlement 仍未证实，§4 的 [较可信，但未经一手验证] 保持不变 [10][12][13] |
| xnu 测试包含 `tests/entitlements/mixed_pagesize_x86_{restricted,unrestricted}.entitlements`，`map_jit_x86_64_compat.c` 注明 “enforced by AMFI” | 证实 | 121.6 的 GitHub tree API 列出了这两个文件；测试文件头注释原文为 “The entitlement check is enforced by AMFI (AppleMobileFileIntegrity).” [7] |
| wine-mono 11.3.0（2026-08-17）的发布说明写有 “An ARM64 build has been added…also built as arm64ec” | 证实 | 引文逐字一致。核查人员未能加载附件列表，本次修订通过 GitHub Releases API 补充核实：附件中有 `wine-mono-11.3.0-arm64.msi`、`-arm64.tar.xz` 和 `-dbgsym-arm64.tar.xz`，`published_at` 为 2026-08-17T18:24:41Z [34] |
| Wine MR !11638（cqwrteur，2026-08-11 开启，opened，有冲突）把 loader 做成 PIE，使用动态 `KUSER_SHARED_DATA` = `TPIDRRO_EL0 - 0x1000`，并链接到 macOS 12 以沿用旧的 x18 行为，只面向原生 WoA 程序，不需要 entitlement | 证实 | 标题 “Mac: Support Apple silicon mac for running native windows on arm softwares”；GitLab 用户名是 `trcrsired`（与 cqwrteur 是同一开发者的两个账号名）；创建时间 2026-08-11T11:42Z，state opened，`has_conflicts` true。已在 §3 和参考 [27] 中补充用户名与标题 [27] |
| FEX 博客 2026-09-17：M1 上 Acquire/LRCPC load “quite a bit lower than the baseline”；TSO 模式下 store 为普通 store 的 76%，load “basically match” | 证实 | 博文 “The scourge of x86 emulation” 的日期和引文都相符；Hackaday 于 2026-09-19 做了转载报道 [31] |
