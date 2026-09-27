# macOS 底层集成调研：地址空间、同步原语、信号、签名/公证/权限、窗口系统与 Tahoe / macOS 27 变化

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 一手源码、Apple 官方文档/发布说明，或本机实测；**[中]** = 可信的二手报道、搜索摘要（原页面 403 无法直接抓取），或一手但只能间接印证；**[低/推断]** = 基于上述材料的工程推断，尚未验证。
> 本机实测环境：Apple M3（4P+4E）、8 GiB、macOS 26.5（25F71，xnu-12377.121.6）、Apple clang 21.0.0、SDK 26.5，只装了 CLT，SIP 开启，**未装 Rosetta**。实测代码都在 scratchpad 里临时编译，没有安装任何东西。
> 2026-09-26 已按独立事实核查修订（其中一条"部分属实"，其余均"属实"并补充了细节），详见文末"事实核查记录"。

## 摘要

- **x86_64 Wine 的地址空间布局已经不再依赖 preloader。** 上游 `configure.ac` 的逻辑是：链接器支持 `-no_huge`（Xcode 15.3+）时，x86_64 不再使用 preloader。此时 `wine` loader 本身以 `-no_pie,-image_base,0x200000000,-no_huge,-no_fixup_chains` 链接，并用 zerofill 段 `WINE_RESERVE`（0x1000 起，约 8 GB）和 `WINE_TOP_DOWN`（0x7ff000000000 起）占住低端和高端地址 [1][2]。这一改动来自 Brendan Shanks 的提交 "loader: Use zerofill sections instead of preloader on macOS when building with Xcode 15.3"（2024-06-21）[71]。preloader 只在 i386 或旧链接器上使用 [1][3]；其他架构（包括 aarch64）走 `*) wine_use_preloader=no` 分支，既没有 preloader，也没有这套地址保留 [1]。**[高：已独立核查]**
- **本机实测：SDK 26.5 的 ld 已经链接不了传统 preloader。** 按上游 `WINEPRELOADER_LDFLAGS`（`-nostartfiles -nodefaultlibs`）链接会报错 `ld: dynamic executables or dylibs must link with libSystem.dylib`。zerofill 方案可以正常链接出 x86_64 可执行文件（4 KB `__PAGEZERO`，外加从 0x1000 开始的 `WINE_RESERVE`）。加 `-static` 虽然能链接出不依赖任何 dylib 的 MH_EXECUTE，但 macOS 26 的 dyld 在运行时会拒绝"没有 LC_LOAD_DYLIB"的二进制，Tahoe beta 期间已经有 Wine 封装器因此失效，所以 `-static` 不是出路 [46][47]。**[高：本机实测已独立复现 + 报告]**
- **原生 arm64 进程根本无法使用低 4 GB 地址，这是 Rosetta 之后最硬的约束。** 本机实测：arm64 可执行文件把 `__PAGEZERO` 设成 16 KB，exec 时直接被 SIGKILL；用默认 4 GB pagezero 时，即使先 `mach_vm_deallocate` 掉 pagezero，`mach_vm_allocate(VM_FLAGS_FIXED)` 在 0x10000、0x7ff00000 和 0x100000000 仍然返回 `KERN_INVALID_ADDRESS`。XNU 源码 `bsd/kern/mach_loader.c` 对 64 位 ARM64 程序强制要求覆盖低 32 位地址空间的 4 GB 硬 pagezero，否则返回 `LOAD_BADMACHO` [72]。Apple DTS 也明确表示 arm64 上"修改 pagezero_size 不受支持"[61]。加上 16 KB 页（本机 `hw.pagesize=16384`）和 TSO 只能通过私有接口开启，arm64 原生 Wine 在 32 位程序和低地址假设上有结构性缺口。**[高]**
- **Rosetta 时间线（官方）**：Rosetta 作为通用工具提供到 macOS 27 为止；从 macOS 28 起，只为"依赖 Intel 框架、无人维护的老游戏"保留一个子集 [53][54]。macOS 27 Golden Gate 发布说明另有三条：升级后**不自动恢复 Rosetta**（163213094）；"macOS 28.0 起所有 Intel 软件不再兼容，legacy games 除外"（176042635）；beta 版提供 `sudo game-test-tool enable` 来试用新的 legacy games 机制，**它会同时禁用 Rosetta**（166398727）[44][73]。同一份发布说明还有三条相关内容：用户设为"使用 Rosetta 打开"的 app 现在会以原生方式启动（168097174）；"设置 › 通用"会列出将与 macOS 28 不兼容的 Intel app（175697313）；"显示简介"窗口也会给这类 app 加标签（169548657）[73]。macOS 27 于 2026-09-14 正式发布（9to5Mac、MacRumors 等第三方报道）[45][74]。**[高/中]**
- **Wine 11.0（2026-01-13）**：在 macOS 上改为"在 syscall dispatcher 中切换 `%gs`"，以避免 Windows TEB 和 macOS 线程描述符冲突；新 WoW64 模式正式可用，`wine64` loader 被移除；纯 32 位的 `WINEARCH=win32` 前缀已被弃用，新 WoW64 不支持；ARM64 上可以在 16K/64K 主机页上模拟 4K 页，但只适合简单程序 [10][11][75]。**[高：已独立核查]**
- **同步原语方面，macOS 上没有 futex 系统调用、eventfd，也没有 ntsync。** 因此上游 Wine 在 macOS 上**没有 msync 这类针对 NT 内核对象的快速路径**：事件、mutex、信号量等通过 `NtWaitForSingleObject`/`NtWaitForMultipleObjects` 等待时，仍要和 wineserver 往返。但**进程内的用户态同步原语不走 wineserver**：上游 Wine 11.0 的 `dlls/ntdll/unix/sync.c` 在 `__APPLE__` 下定义了 `USE_FUTEX`，优先用公开的 `os_sync_wait_on_address_with_timeout`/`os_sync_wake_by_address_any`（macOS 14.4+，标志为 `OS_SYNC_WAIT_ON_ADDRESS_NONE`，即仅限本进程），不可用时退回私有的 `__ulock_wait`/`__ulock_wake`。这套机制支撑 `NtWaitForAlertByThreadId`/`NtAlertThreadByThreadId`，进而支撑 `RtlWaitOnAddress`、SRW 锁、条件变量和临界区竞争 [77][78]。**[高：更正]** marzent 的 **msync**（LGPL-2.1）用 Mach semaphore、`__ulock_wait2`/`__ulock_wake`、POSIX shm 和 bootstrap 端口实现，通过 `WINEMSYNC=1` 开启 [12][13]。公开仓库最后一次提交是 2024-08-13（wine-staging 9.15）[14]。CrossOver 从 23.7 起把 MSync 作为瓶子选项提供，26 中仍在 [15][16][76]。上游 Wine 11.0 没有 msync（`dlls/ntdll/unix/msync.c` 不存在，esync.c、fsync.c 也没有）。跨进程场景下，msync 需要的是 `os_sync_wait_on_address` 的 `OS_SYNC_WAIT_ON_ADDRESS_SHARED` / `OS_SYNC_WAKE_BY_ADDRESS_SHARED` 变体（SDK 26.5 头文件中标注 `API_AVAILABLE(macos(14.4))`），可以用来替代私有的 `__ulock_*` [17][18][79]。**[高]**
- **文件系统方面**：wineserver 在 macOS 上**没有 FSEvents/kqueue 的目录变更通知后端**，inotify 分支在 macOS 上是空桩 [21]，所以 `ReadDirectoryChangesW` 基本不工作。大小写敏感性通过 `getattrlist(VOL_CAP_FMT_CASE_SENSITIVE)` 探测并缓存 [20]；APFS 默认不区分大小写，且对 Unicode 规范化不敏感 [23]。**[高]**
- **签名与公证**：公证强制要求 Hardened Runtime、Developer ID、secure timestamp，且不得带 `get-task-allow` [29][30]。Apple Silicon 上所有原生进程都强制 W^X，与是否启用 Hardened Runtime 无关 [27]。Rosetta 路线至少需要 `allow-unsigned-executable-memory`（Whisky 先例 [32]）；想让 Windows 程序用麦克风或摄像头，还需要 `device.audio-input`、`device.camera`。Homebrew 已于 2026-09-01 禁用没通过 Gatekeeper 的 `wine-stable` cask（cask 文件中为 `disable! date: "2026-09-01", because: :fails_gatekeeper_check`）[26][80]。**[高]**
- **TCC**：Wine 子进程的权限请求会记到"负责进程"（Cider.app）名下，所以用途说明字符串必须写在 Cider.app 的 Info.plist 里 [34][36]。macOS 15 起新增的**本地网络隐私**同样按负责进程归属，会影响局域网联机和 Steam 局域网传输 [36]。**[高]**
- **Tahoe 实际发生过的破坏**：一是 dyld/链接对 libSystem 的强制 [46][47]；二是一个 Rosetta 2 死锁，让 Blizzard 系列游戏（D2R、Overwatch 2、Diablo IV）从 2026 年 1 月到 5 月不可用。Diablo IV 和 Overwatch 最终要 **macOS 26.5 的 Rosetta 修复加 CrossOver 26.1**（2026-05-18）一起才恢复可玩 [49][50][51]。**D2R 并未完全修好**：CodeWeavers 称它只在不丢失焦点时可玩，切换焦点导致的崩溃截至 2026-05-18 仍在调查 [49]。这说明 Cider 的关键路径受制于闭源的 Rosetta。**[中/高]**
- **内存（本机实测，已独立复现）**：M3 8 GB 的 `recommendedMaxWorkingSetSize` = 5,726,633,984 B（约 5.33 GiB，占 66.7%；2/3 这个比例是实测值，Apple 文档没有写明），`maxBufferLength` = 4 GiB；空闲时 swap 已用 863.56 MB / 2 GB。winemac 本身不报告显存，DXVK 用 `dxgi.maxDeviceMemory` 覆盖 [41][66]。**[高]**

## 详细调研

### 1. Wine 在 macOS 上的底层运行机制

#### 1.1 地址空间布局：preloader、`__PAGEZERO`、固定地址

**x86_64（Rosetta 或 Intel）现行做法**：上游 `configure.ac` 的 darwin 分支会先用 `WINE_TRY_CFLAGS([-Wl,-no_huge])` 探测链接器。支持该选项时设 `wine_use_preloader=no`，并给 loader 加上下列链接参数：
`-Wl,-no_pie,-image_base,0x200000000,-no_huge,-no_fixup_chains,-segalign,0x1000,-segaddr,WINE_RESERVE,0x1000,-segaddr,WINE_TOP_DOWN,0x7ff000000000`。
基础的 `WINELOADER_LDFLAGS` 中还有 `-pagezero_size,0x1000` 和 `-sectcreate,__TEXT,__info_plist,loader/wine_info.plist` [1]。
`loader/main.c` 用 `.zerofill WINE_RESERVE`（`static char __wine_reserve[0x1fffff000]`）和 `.zerofill WINE_TOP_DOWN`（`0x001ff0000`）声明保留区，源码注释写的是："Not using the preloader on x86_64: Reserve the same areas as the preloader does, but using zero-fill sections" [2]。这些保留只占虚拟地址，不占物理内存。

| 区域 | 用途 | 机制 | 来源 |
|---|---|---|---|
| 0x0–0x1000 | NULL 页 | `-pagezero_size 0x1000` | [1] |
| 0x1000–0x200000000（约 8 GB） | Win32/WoW64 低地址、DOS 区、32 位堆、低于 4 GB 的映像 | zerofill 段 `WINE_RESERVE` | [2][3] |
| 0x200000000 | loader 自身的 `__TEXT` | `-image_base`、`-no_pie` | [1] |
| 0x7ff000000000–0x7ff001ff0000 | top-down 分配与虚拟堆 | zerofill 段 `WINE_TOP_DOWN` | [2][3] |

**preloader（i386 或旧 ld）**：`preloader_mac.c` 在 i386 上保留 0–0x1000、0x1000–0xf000、0x10000–0x110000（DOS 区）、0x110000–0x67ef0000、0x7f000000–0x82000000，并额外保留 0x7a000000–0x7c000000 给内置 DLL。源码注释说明，低 64 KB 分两步分配，"because PAGEZERO might not always be available"[3]。2023-07 的提交加入了 `__program_vars` 段，因为 "dyld4 (starting in Monterey) does not like it to be missing"，缺了会在 Sonoma 上崩溃 [3][4]。该文件 2024–2026 年没有新提交 [4]。使用 preloader 时，`ntdll/unix/loader.c` 通过 `posix_spawn` 加 `POSIX_SPAWN_SETEXEC | _POSIX_SPAWN_DISABLE_ASLR`（私有标志）重新 exec [8]。

**本机实测（SDK 26.5 ld）**：
1. 按上游的 `WINEPRELOADER_LDFLAGS`（`-nostartfiles -nodefaultlibs -ldylib1.o …`，上游至今仍带 `-ldylib1.o` 和 `-mmacosx-version-min=10.7`）链接 x86_64，失败，报错 `ld: dynamic executables or dylibs must link with libSystem.dylib`（ld-1267，事实核查已独立复现）。加 `-lSystem` 后可以产出 `LC_UNIXTHREAD`、4 KB `__PAGEZERO` 的二进制。加 `-static` 也能链接成功，产物是不依赖任何 dylib 的 MH_EXECUTE。但 Sikarugir #130（2025-08-18，macOS 26 beta，引擎 WS12WineCX24.0.7_4）显示 dyld 会在运行时以 "missing LC_LOAD_DYLIB (must link with at least libSystem.dylib)" 拒绝这类二进制，所以 `-static` 不能作为规避手段 [46]。**[中]** 本机没有 Rosetta，`-static` 产物没有实际运行过，这一点靠 #130 间接印证。
2. 按 zerofill 方案的参数链接 x86_64 测试程序，成功，`otool -l` 显示 `__PAGEZERO` 为 0x1000、`WINE_RESERVE` 为 0x1000–0x200000000。注意保留数组不能被代码以 RIP 相对方式引用，否则会触发 "32-bit RIP-relative reference out of range"。
3. 由于本机没有 Rosetta，x86_64 产物**没有实际运行**。

→ Cider 的 x86_64 Wine 必须走"无 preloader + zerofill"这条路线。确实需要 preloader 时（例如 i386 宿主，基本用不到），链接参数要补上 libSystem，不能用 `-static` 规避。**[高]**

**arm64 原生（Rosetta 之后的路线）**：
- 本机实测：`-Wl,-pagezero_size,0x4000` 链接出的 arm64 程序在 exec 时被杀（退出码 137）。用默认 4 GB pagezero 时，`mmap(MAP_FIXED)` 映射 0x10000 失败；`munmap` 和 `mach_vm_deallocate(0, 4GB)` 都返回成功，但之后 `mach_vm_allocate(FIXED)` 在 0x10000、0x7ff00000 和 0x100000000 仍然是 `KERN_INVALID_ADDRESS`（事实核查在同一台机器上独立复现）。**[高：实测]**
- 内核源码印证了这一点：XNU `bsd/kern/mach_loader.c` 中有 `enforce_hard_pagezero`，对 64 位 `CPU_TYPE_ARM64` 程序要求 `vm_map_has_hard_pagezero(map, 0x100000000)`，注释为 "64 bit ARM binary must have hard page zero of 4GB to cover the lower 32 bit address space"，不满足就返回 `LOAD_BADMACHO` [72]。已抓取的加载路径里看不到基于 entitlement 的例外。**[高：一手源码]**
- Apple DTS（2021-04）："Modifying pagezero_size isn't a supportable option in the arm64 environment. arm64 code must be in an ASLR binary…"[61]。
- 页大小：本机 `hw.pagesize`、`vm.pagesize` 都是 16384。FEX 维护者指出，Apple Silicon 上只有在 Rosetta 下运行 x86_64 二进制的进程才拿到 4 KB 页 [60]。Wine 11 的 4K 页模拟"works for simple applications… Using a 4K-page kernel is strongly recommended"[10]。
- 2020 年 Martin Storsjö 提交 macOS/arm64 补丁时已经列出三点：低 4 GB 不可映射、W^X、16 KB 页 [62]。**[中：搜索摘要]**
- 影响：32 位 x86 程序（WoW64）、`/LARGEADDRESSAWARE:NO` 的 x64 程序、依赖低 2 GB 的 JIT（例如非 GC64 的 LuaJIT）、指针压缩或 DRM，这些在 arm64 原生 Wine + FEX 上**无法用常规方式映射**。CrossOver 27 的 ARM64 预览如何处理这一点，公开资料没有说明（见"未解问题"）。**[推断]**
- `virtual.c` 在 macOS 上用 `mach_vm_map(..., VM_FLAGS_FIXED, ...)` 做保留，用 `mach_vm_region` 扫描空洞。x86 上 `host_page_size` 固定为 0x1000，只有 `__aarch64__` 才在运行时探测 [5]。

#### 1.2 TEB / GS base（x86_64 与 Rosetta）

- Windows x64 通过 `gs:[0x30]` 访问 TEB，而 macOS 把 `%gs` 用作 pthread TSD 基址。Wine 使用私有 API `_thread_set_tsd_base`，源码注释："private API for setting GSBASE, added in macOS 10.12"。在 user-mode callback 的汇编里，Wine 直接用 `movl $0x3000003,%eax; syscall` 调用它 [6]。
- Wine 11.0 的说法是："On macOS, the `%gs` register is swapped in the syscall dispatcher. This avoids conflicts between the Windows TEB and the macOS thread descriptor."[10]
- Intel 与 Rosetta 的差别：在 Intel 上，内核处于 syscall 中时 CS 为 0x07（SYSCALL_CS），信号处理代码会把它修正为 `cs64_sel`，注释写明 "Only applies on Intel, not under Rosetta"。在 Rosetta 下设置调试寄存器会失败，并打印 "Setting debug registers is not supported under Rosetta" [6]。wineserver 也写明跨进程读写被翻译进程的调试寄存器不受支持 [9]。
- 32 位代码：新 WoW64 通过 LDT 安装 32 位代码段。装了自定义 LDT 之后，mcontext 会变成更大的 `_STRUCT_MCONTEXT64_FULL` / `AVX64_FULL` / `AVX512_64_FULL`，Wine 只能靠比较 `uc_mcsize` 来判断 [6]。在 Rosetta 下，64 位 x86 进程可以给自己创建 32 位代码段（`i386_set_ldt`），CrossOver 20.0.2 在 Big Sur 11.1 上借此首次在 Apple Silicon 上跑起 32 位程序 [63]。**[中]**
- Wine 11 的 new WoW64 已"fully supported"，`wine64` loader 被移除，统一为单个 `wine` loader；纯 32 位的 `WINEARCH=win32` 前缀已被弃用，new WoW64 下不支持 [10]。Gcenx 的官方 macOS 构建使用 `--enable-archs=i386,x86_64` [67]。

#### 1.3 主线程、信号与 Mach

- `ntdll/unix/loader.c` 的 `apple_main_thread()` 把进程主线程停在 CFRunLoop 里留给 Cocoa，Wine 的初始化在另一个 pthread 上进行 [8]。每个 Windows 进程对应一个独立的 macOS 进程，各自是一个 Cocoa 应用（见第 5 节）。
- 异常处理用 POSIX 信号，基于 Darwin 的 `uc_mcontext->__ss/__ns/__es` 读取寄存器、FPU 和 ESR [6][7]。**[推断/中]** XNU 会先把硬件异常作为 Mach exception 分发，没有 task 或 thread 端口处理时再转成 BSD 信号。所以调试器或第三方崩溃拦截工具挂上 task exception port 时，会改变 SEH 路径；频繁触发缺页或异常的程序（守护页、反调试）在 macOS 上的开销也比 Linux 高。
- wineserver 对 Mach 的依赖很深 [9]：
  - 用 `bootstrap_register2` 注册接收端口，客户端通过 `mach_msg` 主动把自己的 task port 发过去，而不是用 `task_for_pid`；
  - 用 `thread_get_state`/`thread_set_state`（`x86_DEBUG_STATE`）读写上下文和调试寄存器，注释写明 "Mac OS doesn't allow setting the global breakpoint flags"，因此要屏蔽 DR7；
  - 用 `mach_vm_read_overwrite`/`mach_vm_write` 读写进程内存，写失败时临时 `mach_vm_protect`；
  - 注释还写着 "Rosetta can turn RWX pages into R-X pages during execution"。
- 对 Cider 的影响：这套"客户端交出 task port"的模型与 App Sandbox 不兼容，因为沙盒限制 bootstrap 注册。**[推断/中]** 如果 Apple 以后继续收紧 Mach IPC（例如不可移动的 task control port），这套模型会首当其冲。**[低：未验证]**

#### 1.4 系统调用分发与内核限制

- Linux 上 Wine 用 Syscall User Dispatch 截获 PE 代码里直接执行的 `syscall`。macOS 没有对应机制。Wine 11 让 NT 系统调用号与近期 Windows 一致，以支持硬编码调用号的程序 [10]，但在 macOS 上，PE 代码里的 `syscall` 指令会进入 XNU，无法被截获。**[推断/高]** 某些 DRM 或反作弊的直接 syscall 路径在 Rosetta 方案上无解。在 FEX 方案上，模拟器可以在 JIT 中把 `syscall` 改路由到 Wine 的 dispatcher，这是 arm64 路线的一个潜在优势。**[推断]**

| 限制 | macOS 现状 | 对 Wine/Cider 的影响 | 来源 |
|---|---|---|---|
| 页大小 | arm64 原生 16 KB；Rosetta 进程 4 KB | 原生路线需要模拟 4K 页 | 实测、[10][60] |
| 低 4 GB 地址 | x86_64 可用（小 pagezero）；arm64 硬性禁止 | 原生路线下的 32 位与低地址问题 | 实测、[61] |
| fd 上限 | `kern.maxfilesperproc=10240`、`kern.maxfiles=30720` | esync 这类"每个对象一个 fd"的设计会受限 | 实测 |
| 线程上限 | `kern.num_taskthreads=2048` | 极端程序可能触顶 | 实测 |
| RLIMIT_NOFILE | Big Sur 之前 `rlim_max` 大于 maxfilesperproc 时会失败，Wine 做了特判 | — | [8] |
| futex / eventfd / inotify / ntsync / MAP_32BIT / SUD | 都没有；futex 的公开替代是 `os_sync_wait_on_address`（14.4+） | 上游 Wine 已用 `os_sync_*`（退回 `__ulock_*`）实现进程内的 futex 语义；跨进程的 NT 对象快速路径（msync 类）仍需自己实现 | [17][21][77] |
| TSO | 只对 Rosetta 进程开启；第三方原生进程没有公开接口（`com.apple.private.oahd` 为私有 entitlement） | FEX 需要软件实现内存序 | [64]；另见 06 报告 |
| AVX | Rosetta 翻译 AVX/AVX2，不支持 AVX-512 | 游戏的 CPUID 路径 | [54] |
| 混合架构 | 同一进程内不能混用 arm64 与 x86_64 代码 | x86 PE 程序里的原生插件只能是 x86_64 | [54] |

### 2. 同步原语：esync / fsync / ntsync / msync

- 背景：esync 依赖 eventfd，fsync 依赖 futex，ntsync 是 Linux 6.14+ 的内核驱动。Wine 11 只在 Linux 上使用 ntsync（发布说明称其为 "Linux kernel module"；"仅限 Linux"是据此的推断，但准确）[10]。上游 Wine 11.0 中，esync.c 和 fsync.c 都不存在（核查时请求 wine-11.0 tag 下的 raw 地址返回 404）。
- **上游 Wine 在 macOS 上的现状（已更正）**：
  - **NT 内核对象**（事件、mutex、信号量，通过 `NtWaitForSingleObject`/`NtWaitForMultipleObjects` 等待）没有 msync 式的快速路径，每次等待或唤醒都要和 wineserver 往返。wineserver 是单线程，用 kqueue 作主循环 [22]。
  - **进程内的用户态原语不走 wineserver**。`dlls/ntdll/unix/sync.c`（wine-11.0 约第 139–201 行，master 相同）在 `#elif defined(__APPLE__)` 下定义 `USE_FUTEX` 并包含 `<os/os_sync_wait_on_address.h>`。它优先调用 `os_sync_wait_on_address_with_timeout(..., OS_SYNC_WAIT_ON_ADDRESS_NONE, OS_CLOCK_MACH_ABSOLUTE_TIME, ...)` 和 `os_sync_wake_by_address_any`（macOS 14.4+），否则退回 `__ulock_wait(UL_COMPARE_AND_WAIT, ...)`/`__ulock_wake` [77][78]。
  - 这套 shim 用在 `NtWaitForAlertByThreadId`（`futex_wait`）和 `NtAlertThreadByThreadId`（`futex_wake_one`）上，它们又支撑 `RtlWaitOnAddress`、SRW 锁、条件变量和临界区竞争。所以这些原语在 macOS 上**不需要**和 wineserver 往返。**[高：一手源码]**
  - 修正前本报告写的是"每次等待或唤醒都要和 wineserver 往返"，这个说法过宽，只适用于 NT 内核对象。
- **msync 的设计**（来自补丁本身 [13]）：
  - 新增 `dlls/ntdll/unix/msync.c`（1689 行）和 `server/msync.c`（992 行），共改动 54 个文件；
  - 共享内存：`shm_open("/wine-{configdir inode}-msync")`，按 16 字节槽位存放对象状态，例如信号量是 `{count,max}`，mutex 是 `{tid,count}`；
  - 跨进程：客户端用 `bootstrap_look_up` 找到 wineserver 的端口。多对象等待时，从 `semaphore_pool_alloc()` 取一个 Mach semaphore，通过 `mach_msg2()`（优先用 `mach_msg2_trap`）以 `MACH_MSG_PORT_DESCRIPTOR` 注册给服务器，由服务器端的消息泵在对象变为 signaled 时 signal 它；
  - 单对象快速路径：`__ulock_wait2`（运行时 dlsym）和 `__ulock_wake(UL_COMPARE_AND_WAIT_SHARED)`，拿不到 `__ulock_wait2` 时退回纯信号量；
  - 已知语义缺陷：
    - WaitAll 是"依次等待，再紧循环一次性获取，失败就回滚"；
    - 回滚时不能正确处理被遗弃的 mutex，源码标注为 "HACK"；
    - `PulseEvent` 可能丢唤醒，用 `sched_yield()` 缓解；
    - 不能同时等待 msync 对象和服务器对象（"Can't wait on msync and server objects at the same time!"）。
- 配置：`WINEMSYNC=1`、`WINEMSYNC_QLIMIT=50`、`WINEDEBUG=+msync`。作者给出的数据（M2 Max、CX23）：竞争等待 1000 万次，msync 3.79 s，esync 7.42 s；FFXIV 室内 CPU 瓶颈场景 219 FPS 对 145 FPS [12]。**[中：作者自测]**
- 状态：
  - 公开仓库最后提交 2024-08-13（"Add staging patch for 9.15"）[14]，**没有 Wine 10/11 的版本**；
  - CrossOver Mac 23.7–24 起提供 MSync 选项（"Some applications … broken by MSync"）。CrossOver 23.7 的更新日志称其"now ships with MSync"。CX26 的高级设置页仍列出 MSync，但已不再列出 ESync [15][16][76]；
  - 姊妹报告 02 核对过 CX 25/26 源码包：msync 存在，CX26 已删除 esync（见 02 报告，[高]）；
  - 上游 Wine 11.0 没有 msync：请求 wine-11.0 tag 下的 `dlls/ntdll/unix/msync.c` 返回 404。发布说明中也没有 macOS 同步相关的新机制 [10]。
- 新机会（已更正）：`os_sync_wait_on_address` 和 `os_sync_wake_by_address_any/all` 是 macOS 14.4 起的公开 API [17]。**上游 Wine 已经在进程内场景下完成了从 `__ulock_*` 到 `os_sync_*` 的迁移**（`OS_SYNC_WAIT_ON_ADDRESS_NONE`，私有 API 只作回退，见上文 [77]），Cider 可以直接照搬这套模式。msync 的跨进程路径还需要 `OS_SYNC_WAIT_ON_ADDRESS_SHARED` / `OS_SYNC_WAKE_BY_ADDRESS_SHARED` 这对标志（两边必须配对使用），本机 SDK 26.5 的 `os/os_sync_wait_on_address.h` 中已有这两个标志，标注 `API_AVAILABLE(macos(14.4))` [79]。Rust 标准库已经在 Apple 平台采用它，并用 `__ulock` 作旧版本回退 [18]。实验项目 WFUSync 用这套 API 实现了用户态 NT 同步，声称比 CrossOver MSync 每次操作快 0.58–1.19 µs（合成基准）[19]。**[中]**
- 这对 msync 收益的估计有影响。**[推断]** 由于 SRW 锁、临界区和 `WaitOnAddress` 在上游已经不走 wineserver，msync 的收益主要来自**大量使用 NT 内核对象**（事件、信号量、mutex、`WaitForMultipleObjects`）的程序，这正是很多游戏引擎和 D3D 翻译层线程同步的典型模式。所以 msync 仍值得做，但评估基准应当分开测"NT 对象等待"和"用户态锁"两类。
- wineserver 瓶颈：除同步外，窗口消息、文件句柄、注册表都要经过服务器。msync 只解决等待和唤醒这部分。

### 3. 文件系统

| 主题 | 现状 | 影响与建议 | 来源 |
|---|---|---|---|
| 大小写 | APFS 默认不区分大小写；Wine 用 `getattrlist(ATTR_VOL_CAPABILITIES)` 检查 `VOL_CAP_FMT_CASE_SENSITIVE`/`CASE_PRESERVING`，按 dev/fsid 缓存；退路是看 `statfs.f_fstypename` | bottle 应放在默认（不区分大小写）的卷上；放在区分大小写的卷上需要逐目录扫描，更慢，还可能出现同名冲突 | [20][23] |
| Unicode 规范化 | APFS 保留原始规范化形式，查找时不敏感 | 没有 HFS+ 的 NFD 问题 | [23] |
| DOS 属性 / xattr | 用 `XATTR_USER_PREFIX`（"user."）存 DOSATTRIB；macOS 上用 `listxattr` 判断存在，因为 "getxattr() is significantly slower" | 复制 bottle 时要保留 xattr（`cp -p` 或 `ditto`） | [20] |
| 可用空间 | 用 `kCFURLVolumeAvailableCapacityForImportantUsageKey`（包含可清理空间） | 安装器的空间检查更准确 | [20] |
| 字节范围锁 | wineserver 自己维护 Windows 锁语义；`fcntl(F_SETLK)` 只用于和原生程序互斥；遇到 `ENOTSUP` 等错误就关闭 fs 锁 | 网络卷或 exFAT 上的行为要测试 | [22] |
| 目录变更通知 | **只有 Linux 的 inotify/dnotify**；macOS 上是空桩 | `ReadDirectoryChangesW` / `FindFirstChangeNotification` 实际无效，影响启动器、IDE、热重载、同步盘客户端。**建议给 `server/change.c` 实现 FSEvents 或 kqueue 后端** | [21] |
| 符号链接与重解析点 | Wine 11 实现了 mount point 和 symlink 两类 NT reparse point | 可以让 bottle 内的 `mklink` 类行为正常 | [10] |
| 长路径 | **[中/背景知识]** Darwin `PATH_MAX`=1024 字节，`NAME_MAX`=255 字节（UTF-8） | 深层中日韩路径可能超限，要纳入测试 | — |
| quarantine | 浏览器和 Mail 会给下载文件打 `com.apple.quarantine`，解压时该属性会传给解出的文件 | **[推断/中]** Gatekeeper 只评估 Mach-O 和 app bundle，不评估 PE，所以瓶内的 EXE/DLL 不会被拦截；但 Cider **自己下载的 Mach-O 组件**（Wine 引擎、MoltenVK、DXMT 的 unix 库）如果带 quarantine 且没有公证，就会被拦截 | [24][26] |
| Gatekeeper 绕过 | Sequoia 取消了"按住 Control 点击打开"，只能去"系统设置 › 隐私与安全性"点"仍要打开"；Homebrew 已于 2026-09-01 禁用 `wine-stable` 等没通过 Gatekeeper 的 cask | 不公证就几乎没法分发；也不能指望 `--no-quarantine` | [25][26] |
| App Translocation | **[中/背景知识]** 带 quarantine、没被用户移动过的 app 会从只读的随机路径运行 | Cider 首次启动时应检测自己是否被转移，并提示用户移到 /Applications | — |
| macOS 27 | launchd 不再加载带 quarantine 属性的 plist（166415497） | 如果 Cider 安装 LaunchAgent，要清掉这个属性 | [44] |

### 4. 安全模型：SIP、Hardened Runtime、公证、TCC、为什么不能上 App Store

**SIP**：本机已开启。Wine 本身不需要关闭 SIP，Cider 也绝不能引导用户关闭。

**Apple Silicon 的 W^X**："Apple silicon enables memory protection for all apps, regardless of whether they adopt the Hardened Runtime." 在启用 Hardened Runtime 并带 `allow-jit` 时，"it can only create one memory region with the MAP_JIT flag set"[27]。

| Entitlement | Rosetta x86_64 路线 | arm64 原生 + FEX 路线 | 说明 | 来源 |
|---|---|---|---|---|
| `cs.allow-unsigned-executable-memory` | **需要** | 可能需要 | 允许不受 MAP_JIT 限制地创建可写且可执行的内存。PE 映像和运行时生成的代码都需要。Whisky 在用 | [28][32] |
| `cs.allow-jit` | 不需要 | **需要** | FEX 的代码缓存要用 MAP_JIT，并配合 `pthread_jit_write_protect_np` | [27][29] |
| `cs.disable-library-validation` | 尽量不用 | 尽量不用 | 只有加载其他团队签名的 dylib 时才需要，例如用户自带的插件 | [29] |
| `cs.allow-dyld-environment-variables` | 不要用 | 不要用 | 改用 `@rpath`/`@loader_path`，不要依赖 `DYLD_*` | [29] |
| `cs.disable-executable-page-protection` | 禁止 | 禁止 | 会关闭全部代码签名保护 | [28] |
| `device.audio-input` / `device.camera` | 需要（语音、视频） | 需要 | Hardened Runtime 下的资源访问开关 | [29][32] |
| `automation.apple-events` | 可选 | 可选 | 只在需要驱动 Finder 等应用时 | [29][32] |
| `get-task-allow` | **不得带** | 不得带 | 带了无法公证 | [30] |
| `cs.debugger` | 不需要 | 不需要 | wineserver 的 task port 是客户端主动交出的 | [9] |

**公证要求**：
- 所有可执行文件用 Developer ID Application 签名；
- 带 secure timestamp；
- 启用 Hardened Runtime；
- 不带 `get-task-allow`；
- SDK 不低于 10.9 [30]。

**bundle 布局**：
- Mach-O 应放在 `Contents/MacOS`、`Helpers`、`Frameworks`、`PlugIns` 等代码位置；
- 每个代码位置应是"flat list"，Apple 警告嵌套目录"might fail later in hard-to-debug ways"，目录名也不要含点 [31]；
- Wine 的 `lib/wine/x86_64-unix/*.so` 就是嵌套目录里的 Mach-O，需要由内向外逐个签名；
- CrossOver 把 Wine 放在 bundle 内的嵌套目录中，并且通过了公证，可见这是可行的。**[中]**

**TCC**：Wine 进程由 Cider 启动时，会继承 Cider 作为负责进程。TCC 会查负责进程 Info.plist 里的用途说明，把授权记在 Cider 名下 [34]。私有的 `responsibility_spawnattrs_setdisclaim` 可以让子进程自己负责 [34]。另外有报告称，在 macOS 26 上，用 `login(1)` 包一层已经不能再切断归属（tccd 会穿透到 app）[35]。

| 权限 | 触发场景 | Cider 需要准备的 | 来源 |
|---|---|---|---|
| 麦克风 | 游戏语音、会议软件 | `NSMicrophoneUsageDescription` 加 `device.audio-input` | [29][34] |
| 摄像头 | 视频会议 | `NSCameraUsageDescription` 加 `device.camera` | [29] |
| 本地网络（macOS 15+） | 局域网联机、广播和组播发现、`.local` 解析、连接局域网 IP | `NSLocalNetworkUsageDescription`（按需再加 `NSBonjourServices`）；子进程归到负责代码名下 | [36] |
| 文件与文件夹（桌面、文稿、下载、可移除卷、网络卷） | Wine 默认把 Z: 映射到 `/`，shell folder 链接到 `~/Documents` 等目录 | **[推断]** 默认不映射受保护目录，改为由用户显式选择挂载，避免一连串授权弹窗 | — |
| 屏幕录制 | Windows 程序截取整个桌面 | winemac 没用 `CGWindowListCreateImage` 或 ScreenCaptureKit [41]，所以默认用不到 | [41] |
| 输入监控 / 辅助功能 | CGEventTap 方式的光标限制 | winemac 优先用 confinement 方式，事件 tap 只是退路 [39]。**[推断]** 退路可能触发授权 | [39] |

**为什么不能用 App Sandbox 或上 App Store**：
- 审核条款 2.5.2 和 2.4.5(iv) 禁止下载或执行会改变功能的代码；
- 2.4.5(vii) 要求只能通过 App Store 更新；
- 4.7 虽然允许"PC emulator apps can offer to download games"[37]，但技术上沙盒和 Wine 冲突：wineserver 要 `bootstrap_register2`，msync 要 `bootstrap_look_up`，POSIX shm 命名受限，Z: 需要任意访问文件系统，下载的引擎又必须能执行 [9][13]。**[推断/中]**
- 结论：与 CrossOver 一样，走 Developer ID 签名加公证的站外分发。

### 5. 窗口系统：winemac.drv

- **进程与线程模型**：
  - loader 嵌入的 Info.plist 中是 `CFBundleIdentifier=org.winehq.wine`、`NSPrincipalClass=WineApplication`、`LSUIElement=1` [42]；
  - 每个 Windows 进程都是一个独立的 NSApplication；
  - Wine 线程通过 `OnMainThread` / `OnMainThreadAsync` 往主线程投递 block（用 CFRunLoopSource 实现），事件通过各线程自己的 `WineEventQueue` 分发 [39]；
  - Sonoma 起，多个 Wine 进程之间用 `yieldActivationToApplication:` / `activateFromApplication:` 协作交接前台 [39]；
  - Ventura 上 `-setLevel:` 有 bug，改用 `-orderFront:` 规避 [39]。
- **渲染**：
  - Vulkan 优先用 `VK_EXT_metal_surface`，否则退回 `VK_MVK_macos_surface`；CAMetalLayer 通过 `macdrv_client_surface_acquire_metal_swapchain` 获取 [40]；
  - OpenGL 走 CGL，由 `OpenGLSurfaceMode` 控制，默认 opaque 置前 [38]；
  - **[推断]** D3DMetal 和 DXMT 也渲染到 winemac 提供的 CAMetalLayer 上。
- **全屏与 Spaces**：
  - `CaptureDisplaysForFullscreen` 默认关闭，开启时用 `CGCaptureAllDisplays`；
  - 通过监听 `NSWorkspaceActiveSpaceDidChangeNotification` 调整窗口层级；
  - 分辨率切换用 `CGDisplaySetDisplayMode`，并缓存原始模式；
  - `AllowSetGamma` 默认开启 [38][39]。
- **多显示器**：`CGGetOnlineDisplayList` 加 `NSScreen` 枚举，主屏由 `CGDisplayIsMain` 判定。GPU 信息优先从 Metal（`MTLCopyAllDevices`、`registryID`、GPU family）获取，**不报告显存** [41]。
- **Retina**：
  - `RetinaMode` 是全局设置（按 prefix 生效，不能按程序单独设），开启后鼠标增量乘 2 [38][39]；
  - CrossOver 把它包装成 "High Resolution Mode"：关闭像素加倍，并报告 192 DPI [15][16]。
- **光标**：
  - 优先用 `WineConfinementClipCursorHandler`，退路是 `WineEventTapClipCursorHandler`；
  - `CGWarpMouseCursorPosition` 之后会有 0.25 s 鼠标与光标脱钩，Wine 先调 `CGSetLocalEventsSuppressionInterval(0)` 规避，再调 `CGAssociateMouseAndMouseCursorPosition` [39]。
- **键位与其他选项**：`LeftOptionIsAlt`、`RightOptionIsAlt`、`LeftCommandIsCtrl`、`RightCommandIsCtrl`、`UsePreciseScrolling`、`EnableAppNap`（默认 false）等，读取路径是 `HKCU\Software\Wine\Mac Driver`，也支持 AppDefaults 按程序覆盖 [38]。
- **菜单栏**：winemac 只提供 macOS 的 app 菜单，Windows 程序窗口内的菜单保持原样。
- **Game Mode**：
  - macOS 26 新增 `LSSupportsGameMode`（Apple 文档标注 macOS 26.0 引入）；Game Mode 要求 `LSApplicationCategoryType=public.app-category.games`，并在全屏时才启用 [43][69]；
  - 26.0 发布说明："Game Mode will not activate for application binaries spawned directly from Terminal"（153127050），给出的规避办法是改用 `open` 启动 [43][82]。**[推断]** Cider 直接 `posix_spawn` wine loader，是否也算"直接派生的二进制"需要实测；如果算，就要考虑经由 LaunchServices（`open` 或 `NSWorkspace`）启动游戏进程；
  - Apple DTS 表示，由启动器派生的子进程窗口"unlikely to work"[68]；
  - Wine 嵌入的 plist 里没有游戏分类。**[推断]** 可以在 loader 的 `__info_plist` 中加上这三个键，再实测是否生效。

### 6. macOS 26 Tahoe 与 macOS 27 Golden Gate 的变化

| 时间 / 版本 | 变化 | 对 Wine / Cider 的影响 | 来源 |
|---|---|---|---|
| 2025-06 WWDC25 | 宣布 Rosetta 通用支持到 macOS 27 | 决定了 Cider 的整体路线 | [56][54] |
| 26 beta（2025-08） | dyld 报 `missing LC_LOAD_DYLIB (must link with at least libSystem.dylib)`，Sikarugir 的 WS12WineCX24.0.7 封装完全不能用 | 所有 Mach-O 都要显式链接 libSystem；本机实测 ld 已直接拒绝链接 | [46][47]、实测 |
| 26.0 | 最后一个支持 Intel 的版本；新增 `nox86exec=1` boot-arg（`sudo nvram boot-args="nox86exec=1"`，136764433），设置后任何原本要经 Rosetta 运行的进程都会在启动时崩溃，用于测试不依赖 Rosetta；新增 `LSSupportsGameMode`；Metal 4；Game Mode 相关修复（从终端直接派生的二进制不会激活 Game Mode，153127050）；已知问题：关闭"显示器具有单独的空间"会导致 WindowServer 在登录时崩溃（153570422）；全屏布局问题已修复（151266898） | 可以用 `nox86exec` 验证 arm64 路线。**[推断，未验证]** Apple Silicon 上设置自定义 nvram boot-args 可能需要先在恢复模式降低"启动安全性"，CI 采用前要先确认流程。全屏和 Spaces 测试矩阵要覆盖"单独的空间"开关 | [43][82][69] |
| 2025-09-15 | CrossOver 25.1.1 "Tahoe is a go"，包含 Intel Tahoe 修复 | — | [48] |
| 26.2–26.4（2026-01 至 05） | Rosetta 2 死锁：D2R（2026-01 更新后）、Overwatch 2、Diablo IV S12（2026-03）无法运行。FB15880492、FB21763885、FB21838832 | 闭源依赖。Diablo IV 和 Overwatch 需要 macOS 26.5 加 CrossOver 26.1 两边的改动才恢复可玩（2026-05-18）。**D2R 只在不丢失焦点时可玩**，切换焦点时的崩溃截至该日仍在调查，不能算已修复 | [49][50][51] |
| 26.4 | 启动使用 Rosetta 的 app 时弹出停止支持提醒 | 用户会看到"这个 app 以后不能用"的提示，需要在 Cider 里提前解释 | [52][55] |
| 26.x | TCC 归属变化（`login` 包装不再切断归属） | 负责进程策略要以实测为准 | [35] |
| 2026-02-10 | CrossOver 26 基于 Wine 11.0，附带 D3DMetal 3.0 和 DXMT 0.72 | Cider 的 Wine 基线应与之对齐（11.0） | [58][81] |
| 2026-07-31 | CrossOver ARM64 预览：macOS 上用定制 FEX；没有 D3DMetal，很多启动器不能用，旧 bottle 不能转换 | 厂商也刚开始走 arm64 路线 | [58][59] |
| 2026-06-11 | CrossOver 27（AppleInsider 2026-07-31 报道称"penciled in"2027 年初发布）只支持 Apple Silicon 和 Sonoma+，不再运行 32 位 bottle | Cider 的最低系统版本可以参考 | [57][58]（CodeWeavers 原文为搜索摘要，另有 AppleInsider、GIGAZINE 报道） |
| macOS 27（2026-09-14 发布，第三方报道） | 只支持 Apple Silicon；升级后不自动恢复 Rosetta（163213094）；macOS 28 起除 legacy games 外不再兼容 Intel 软件（176042635）；beta 版 `game-test-tool enable` 启用新的 legacy 游戏机制，同时禁用 Rosetta，非游戏进程可能崩溃，且仅在 beta 版可用（166398727）；设为"使用 Rosetta 打开"的 app 改为原生启动（168097174）；"设置 › 通用"列出将与 macOS 28 不兼容的 Intel app（175697313），"显示简介"中也会加标签（169548657）；launchd 拒绝带 quarantine 的 plist；修复了全屏时 Dock 残留（174992242） | Cider 必须检测 Rosetta 并引导安装；**要尽早用 game-test-tool 实测 Wine 进程能否在 legacy 机制下运行**。**[推断，未验证]** Cider.app 若在 bundle 内携带 x86_64 的 Wine 引擎，可能被列入"不兼容"清单或被加上标签，需要在 macOS 27 上实测，并准备面向用户的说明 | [44][45][73][74] |

### 7. 内存：统一内存、8 GB、swap、显存报告

- 本机实测（M3、8 GiB、26.5）：
  - `hasUnifiedMemory=1`；
  - `recommendedMaxWorkingSetSize=5,726,633,984 B`（5461 MiB，占 `hw.memsize=8,589,934,592` 的 66.7%；事实核查用独立的 Objective-C 程序复现了同样的数值）。**注意**：2/3 这个比例只是本机实测结果，Apple 没有文档说明，其他内存档位不能直接套用，要在运行时读取；
  - `maxBufferLength=4 GiB`；
  - `iogpu.wired_limit_mb=0`（默认，由系统动态决定）；
  - `vm.swapusage` 总计 2048 MB，空闲时已用 863.56 MB，已有压力；
  - wired 约 106545 × 16 KB ≈ 1.6 GB。
- 显存报告：
  - winemac 不报告显存 [41]；
  - DXVK 可以用 `dxgi.maxDeviceMemory` / `dxgi.maxSharedMemory` 覆盖，`d3d9.maxAvailableMemory` 默认 4096 MB [66]；
  - D3DMetal 如何报告 `DedicatedVideoMemory` 是闭源的，未知；
  - Metal 文档把 `recommendedMaxWorkingSetSize` 定义为"不影响性能的情况下可分配的大致上限"[65]。
- **[推断]** 8 GB 机器上，建议给游戏报告约 4 GiB 的专用显存。报告 5.3 GiB 以上会诱使游戏装载更高档的纹理，与系统和 Wine 进程争内存，引发 swap。16 GB 及以上的机器可以按运行时读到的 `recommendedMaxWorkingSetSize` 的 70–75% 计算，不要假设它恒为物理内存的 2/3。
- zerofill 保留（8 GB 低端和约 32 MB 高端）只占虚拟地址，不增加 RSS。Rosetta 翻译缓存和 FEX JIT 缓存会额外占用内存，在 8 GB 机器上要控制。**[推断]**

## 对 Cider 的启示与建议（按优先级）

**P0（立即）**
1. **双轨架构，从第一天起就规划 Rosetta 之后。**
   - 轨道 R：x86_64 Wine 11（新 WoW64），在 Rosetta 下运行，采用上游的 zerofill 布局，不用 preloader。这是 macOS 26/27 上唯一成熟的路线。
   - 轨道 A：arm64 Wine、ARM64EC/WoW64 加 FEX。要接受 16 KB 页上模拟 4K、没有低 4 GB、TSO 只能软件实现这三个约束，并把它们写入兼容性声明（32 位程序、低地址 x64 程序可能无法运行）。
   - macOS 28 预计 2027 年秋发布。每个 beta 都要用 `game-test-tool` 实测 Wine 进程能否走 legacy 机制，但不要把它当作依赖。与 06 报告一致。
   - 轨道 A 的回归测试可以借助 26.0 起的 `nox86exec=1` boot-arg，确认没有任何进程偷偷走 Rosetta。**[推断，未验证]** Apple Silicon 上设置自定义 boot-args 可能需要降低启动安全性，所以先在专用测试机上确认流程，再决定是否放进 CI。
   - 轨道 A 的低 4 GB 限制已由 XNU 源码证实是加载器的硬性检查（`enforce_hard_pagezero`），不是可调参数。兼容性声明里应直接写明"无法通过配置解决"。
2. **工具链**：
   - 在 SDK 26+ 上构建时，确保所有 Mach-O 都显式链接 libSystem，不要用 `-static` 规避（dyld 会在运行时拒绝缺 `LC_LOAD_DYLIB` 的二进制）；
   - CI 中加入 `otool -l` 检查，要求有 `LC_LOAD_DYLIB` 和 `LC_BUILD_VERSION`；x86_64 loader 还要检查 `__PAGEZERO=0x1000`、`WINE_RESERVE@0x1000`；
   - 分别在 26.5 和 27 上做冒烟测试。
3. **签名和公证流水线一开始就建好**：
   - Developer ID 签名、`--options=runtime`、`--timestamp`、由内向外逐个签名、notarytool 公证加 staple；
   - entitlement 取最小集合：R 轨道用 `allow-unsigned-executable-memory`，A 轨道用 `allow-jit`，另加 `device.audio-input` 和 `device.camera`；
   - 不用 `disable-library-validation`，不用 `DYLD_*` 环境变量；
   - 如果引擎是单独下载的，下载的引擎包也要签名和公证。
4. **同步**（已按事实核查修订）：
   - 以 CX26 源码中的 msync 为基础（见 02 报告）移植到 Wine 11 基线；
   - msync 的目标范围限定在 **NT 内核对象**（事件、信号量、mutex、多对象等待）。SRW 锁、临界区、条件变量和 `WaitOnAddress` 在上游 Wine 11 的 macOS 构建中已经走 `USE_FUTEX` shim，不经过 wineserver，不需要 msync 处理；
   - 迁移 `__ulock_*` 时直接复用上游 `dlls/ntdll/unix/sync.c` 的模式：14.4+ 用 `os_sync_*`，更早的系统退回 `__ulock_*`。区别在于 msync 的跨进程共享内存路径必须使用 `OS_SYNC_WAIT_ON_ADDRESS_SHARED` 和 `OS_SYNC_WAKE_BY_ADDRESS_SHARED`，两者要配对使用；上游用的是 `OS_SYNC_WAIT_ON_ADDRESS_NONE`，只适用于本进程；
   - 建立正确性测试（WaitAll、遗弃 mutex、PulseEvent，加上 Wine 的 ntdll sync 测试）和微基准。基准要把"NT 对象等待"和"用户态锁"分开测，否则会高估 msync 的收益；
   - 做成按 bottle 可开关的选项；测试充分后，游戏类 bottle 默认开启。
5. **Rosetta 生命周期**：
   - 检测 `sysctl.proc_translated` 和 Rosetta 是否已安装；
   - 用 `softwareupdate --install-rosetta` 引导安装（macOS 27 升级后不会自动恢复）；
   - 为 26.4+ 的弃用提醒写说明；macOS 27 的"设置 › 通用"不兼容清单和"显示简介"标签也要写进说明，并实测 Cider.app 是否会被列入；
   - 维护一个按 macOS build 号索引的已知问题库，例如 Rosetta 死锁要求 26.5 以上，且 D2R 在 26.5 + CX26.1 上仍有切换焦点崩溃。

**P1（第一个公开版本之前）**
6. **winemac 增强**：
   - Game Mode：在 loader 的 plist 中加 `LSApplicationCategoryType`、`LSSupportsGameMode`、`GCSupportsGameMode` 并实测；同时对比 `posix_spawn` 直接启动和经由 LaunchServices（`open`/`NSWorkspace`）启动两种方式，因为 26.0 已知从终端直接派生的二进制不会激活 Game Mode；
   - RetinaMode 支持按程序设置；
   - 加强全屏和 Spaces、多显示器、光标捕获的测试矩阵，覆盖"显示器具有单独的空间"开关。
7. **给 wineserver 实现 FSEvents 或 kqueue 的变更通知后端**。这是上游缺失的功能，也可以回馈上游。
8. **TCC 体验**：
   - Cider.app 的 Info.plist 写全麦克风、摄像头、本地网络的用途说明，负责进程保持为 Cider；
   - 默认不映射 Z: 到 `/`，也不把 shell folder 链接到受保护目录，改为由用户挑选要挂载的文件夹。
9. **内存策略**：按内存档位设置显存报告（8 GB 约 4 GiB），计算基数取运行时的 `recommendedMaxWorkingSetSize`，不要硬编码 2/3；8 GB 机器上默认关闭 Retina 渲染；提示用户关闭其他占内存的应用。

**P2**
10. 检测 App Translocation 并引导用户移动 app；清理 LaunchAgent plist 上的 quarantine 属性（macOS 27 要求）。
11. 诊断包：收集 macOS build、`proc_translated`、Rosetta 状态、entitlement 自检结果、msync 开关状态。

## 风险

1. **Rosetta 终止（高）**：macOS 28 起失去通用 Rosetta，轨道 R 会整体失效；legacy games 子集是否覆盖 Wine 不确定。
2. **arm64 的结构性限制（高）**：低 4 GB 不可用、16 KB 页、TSO 只有私有接口，导致兼容性和性能都不如 Rosetta 路线。
3. **依赖私有 API（中）**：`_thread_set_tsd_base`、`__ulock_*`、`mach_msg2_trap`、`bootstrap_register2`、`_POSIX_SPAWN_DISABLE_ASLR`、`responsibility_*`、CGS 光标限制。任何一个系统更新都可能让它们失效。
4. **闭源回归（中）**：Rosetta 死锁从 2026-01 拖到 05 才大部分修好，D2R 的切换焦点崩溃截至 2026-05-18 仍未解决。Cider 无法自行修复，只能靠反馈加临时规避。
5. **分发门槛（中）**：公证需要付费的开发者账号；不公证的话，Sequoia 之后用户体验极差，Homebrew 也不会收录。
6. **msync 语义缺陷（中）**：WaitAll、PulseEvent、遗弃 mutex 的处理可能导致个别程序出错，需要按程序关闭。
7. **TCC 弹窗过多（低到中）**：本地网络、文件夹、麦克风的授权会集中出现在 Cider 名下。
8. **Mach IPC 收紧（低，未验证）**：例如不可移动的 task port，会破坏 wineserver 的模型。

## 未解问题

1. macOS 28 的 legacy games 机制如何识别"游戏"：白名单、链接的框架，还是签名？Wine 或 Cider 进程能否被认定？`game-test-tool` 背后是什么技术？
2. CrossOver ARM64 预览在 macOS 上如何处理低 4 GB、32 位 WoW64 和 TSO？（CodeWeavers 博客对本工具返回 403，只拿到搜索摘要。）是否用了 Apple 发放的 entitlement？
3. 26.4+ 的 Rosetta 弃用提醒，会不会因为 arm64 的 Cider.app 启动了 x86_64 的 wine 子进程而触发？如果触发，会提示几次？
4. CX26 的 msync 是否已经换成 `os_sync_*`？默认是否开启？
5. Rosetta 对 PE 代码（非 Mach-O）的翻译能否跨次运行缓存？首次运行卡顿有多大？
6. D3DMetal 向游戏报告的显存和 `recommendedMaxWorkingSetSize` 是什么关系？
7. 在 Rosetta 下，x86_64 Wine 进程在 Hardened Runtime 下的最小 entitlement 集合是什么？需要实测，因为本机还没装 Rosetta。
8. 有没有任何 entitlement 能降低 arm64 进程的最小映射地址？（部分解答：XNU `mach_loader.c` 的 `enforce_hard_pagezero` 路径中看不到基于 entitlement 的例外 [72]，基本可以认为第三方无路可走；但未排除其他代码路径或私有 entitlement。）
9. macOS 27 的"设置 › 通用"不兼容清单和"显示简介"标签如何判定：只看主可执行文件的架构，还是会扫描 bundle 内的所有 Mach-O？arm64 的 Cider.app 如果携带 x86_64 Wine 引擎，会不会被标记？

## 参考来源

1. Wine `configure.ac`（master，darwin loader/preloader 参数、`-no_huge` 探测）：https://raw.githubusercontent.com/wine-mirror/wine/master/configure.ac
2. Wine `loader/main.c`（zerofill `WINE_RESERVE`/`WINE_TOP_DOWN`）：https://raw.githubusercontent.com/wine-mirror/wine/master/loader/main.c
3. Wine `loader/preloader_mac.c`（保留区表、`__program_vars`）：https://raw.githubusercontent.com/wine-mirror/wine/master/loader/preloader_mac.c
4. preloader_mac.c 提交历史（2022–2023，之后没有更新）：https://github.com/wine-mirror/wine/commits/master/loader/preloader_mac.c
5. Wine `dlls/ntdll/unix/virtual.c`（`mach_vm_map`、`host_page_size`）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/virtual.c
6. Wine `signal_x86_64.c`（`_thread_set_tsd_base`、mcontext FULL、Rosetta 调试寄存器）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/signal_x86_64.c
7. Wine `signal_arm64.c`（Darwin arm64 mcontext）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/signal_arm64.c
8. Wine `ntdll/unix/loader.c`（`apple_main_thread`、`_POSIX_SPAWN_DISABLE_ASLR`）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/loader.c
9. Wine `server/mach.c`（bootstrap_register2、task port、Rosetta 注释）：https://raw.githubusercontent.com/wine-mirror/wine/master/server/mach.c
10. Wine 11.0 ANNOUNCE（%gs 切换、WoW64、4K 页模拟、NTSync）：https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md
11. WineHQ News：Wine 11.0 Released（2026-01-13）：https://www.winehq.org/news/2026011301
12. marzent/wine-msync README（设计、环境变量、基准、LGPL-2.1）：https://github.com/marzent/wine-msync
13. msync-staging.patch（实现细节与局限）：https://raw.githubusercontent.com/marzent/wine-msync/main/msync-staging.patch
14. wine-msync 提交历史（最后一次 2024-08-13）：https://github.com/marzent/wine-msync/commits/main
15. CodeWeavers：Advanced Settings in CrossOver Mac 23.7–24（ESync/MSync/高分辨率模式）：https://support.codeweavers.com/advanced-settings-in-crossover-235
16. CodeWeavers：Advanced Settings in CrossOver Mac 26：https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26
17. Apple：`os_sync_wait_on_address`（及 `OS_SYNC_WAIT_ON_ADDRESS_SHARED`）：https://developer.apple.com/documentation/os/os_sync_wait_on_address
18. rust-lang/rust PR #122408（Apple 平台改用 futex 式 API，14.4 与 `__ulock` 回退）：https://github.com/rust-lang/rust/pull/122408
19. WFUSync（macOS 用户态 NT 同步实验）：https://github.com/Alien4042x/Wine-NTsync-Userspace-macOS-backend
20. Wine `ntdll/unix/file.c`（大小写探测、xattr、可用空间）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/file.c
21. Wine `server/change.c`（只有 inotify/dnotify，macOS 上是空桩）：https://raw.githubusercontent.com/wine-mirror/wine/master/server/change.c
22. Wine `server/fd.c`（锁语义、kqueue 主循环）：https://raw.githubusercontent.com/wine-mirror/wine/master/server/fd.c
23. Apple APFS Guide FAQ（规范化不敏感、默认不区分大小写）：https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/APFS_Guide/FAQ/FAQ.html
24. Eclectic Light：quarantine xattr（2017-08-15）：https://eclecticlight.co/2017/08/15/quarantined-more-about-the-quarantine-extended-attribute/
25. Apple Developer News：Updates to runtime protection in macOS Sequoia：https://developer.apple.com/news/?id=saqachfa
26. fleetdm #43484（Homebrew 于 2026-09-01 禁用 wine-stable）：https://github.com/fleetdm/fleet/issues/43484
27. Apple：Porting just-in-time compilers to Apple silicon：https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon
28. Apple：`com.apple.security.cs.allow-unsigned-executable-memory`：https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.allow-unsigned-executable-memory
29. Apple：Hardened Runtime（entitlement 列表、公证要求）：https://developer.apple.com/documentation/security/hardened-runtime
30. Apple：Resolving common notarization issues：https://developer.apple.com/documentation/security/resolving-common-notarization-issues
31. Apple：Placing content in a bundle：https://developer.apple.com/documentation/bundleresources/placing-content-in-a-bundle
32. Whisky.entitlements：https://raw.githubusercontent.com/Whisky-App/Whisky/main/Whisky/Whisky.entitlements
33. Whisky 仓库（2025-05-11 归档，不再维护）：https://github.com/Whisky-App/Whisky
34. Qt Blog：The Curious Case of the Responsible Process（2022-02-04）：https://www.qt.io/blog/the-curious-case-of-the-responsible-process
35. stablyai/orca #12971（macOS 26 TCC 归属变化）：https://github.com/stablyai/orca/issues/12971
36. Apple TN3179：Understanding local network privacy：https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy
37. App Store Review Guidelines（2.5.2、2.4.5、4.7）：https://developer.apple.com/app-store/review/guidelines/
38. Wine `winemac.drv/macdrv_main.c`（注册表选项）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/macdrv_main.c
39. Wine `winemac.drv/cocoa_app.m`（主线程、全屏、光标、Sonoma 激活）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_app.m
40. Wine `winemac.drv/vulkan.c`（Metal surface）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/vulkan.c
41. Wine `winemac.drv/cocoa_display.m`（显示器与 GPU 枚举，无显存报告）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_display.m
42. Wine `loader/wine_info.plist.in`：https://raw.githubusercontent.com/wine-mirror/wine/master/loader/wine_info.plist.in
43. Apple：macOS Tahoe 26 Release Notes：https://developer.apple.com/documentation/macos-release-notes/macos-26-release-notes
44. Apple：macOS 27 Golden Gate Release Notes：https://developer.apple.com/documentation/macos-release-notes/macos-27-release-notes
45. Blake Crosley：macOS 27 Golden Gate Is Out（2026-09-16，称 2026-09-14 发布，build 26A428）：https://blakecrosley.com/blog/macos-27-golden-gate-release
46. Sikarugir #130（Tahoe 下 LC_LOAD_DYLIB 问题，2025-08-18）：https://github.com/Sikarugir-App/Sikarugir/issues/130
47. Lucas Colley：macOS 26 要求链接 libSystem（2025-09-26）：https://lucascolley.github.io/blog/2025-09-26-macos-26-c-compiler/
48. CodeWeavers 博客：Tahoe is a go with CrossOver 25.1.1（2025-09-15，搜索摘要）：https://www.codeweavers.com/blog/mjohnson/2025/9/15/tahoe-is-a-go-with-crossover-2511
49. CodeWeavers 博客：Diablo IV 和 Overwatch 在 CrossOver 26.1 + macOS 26.5 上可玩（2026-05-18，搜索摘要）：https://www.codeweavers.com/blog/mjohnson/2026/5/18/finally-diablo-iv-and-overwatch-are-playable-with-crossover-261-macos-265
50. Apple Developer Forums：Rosetta 2 Deadlock（FB 编号）：https://developer.apple.com/forums/thread/814383
51. Blizzard 论坛：Diablo IV S12 在 CrossOver 下无窗口：https://us.forums.blizzard.com/en/d4/t/diablo-iv-invisibleno-game-window-after-season-12-update-macos-via-crossover/242939
52. Apple Developer News：Upcoming changes to Rosetta support：https://developer.apple.com/news/?id=w5ngl9k2
53. Apple Support 102527（macOS 28 起只保留给老游戏）：https://support.apple.com/en-us/102527
54. Apple：About the Rosetta translation environment（AVX/AVX2、无 AVX-512、`proc_translated`、不能混合架构）：https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment
55. MacRumors：26.4 的 Rosetta 提醒（2026-02-16）：https://www.macrumors.com/2026/02/16/macos-tahoe-26-4-rosetta-2-warnings/
56. MacRumors：WWDC25 宣布逐步停用 Rosetta（2025-06-10）：https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/
57. CodeWeavers 博客：What's in and what's out for CrossOver 27（2026-06-11，搜索摘要）：https://www.codeweavers.com/blog/mjohnson/2026/6/11/whats-in-and-whats-out-for-crossover-27
58. AppleInsider：首个 Apple Silicon 原生 CrossOver 构建（2026-07-31）：https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears
59. CodeWeavers 博客：CrossOver Preview ARM64 on Mac（2026-07-31，搜索摘要）：https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac
60. FEX Discussion #3267（macOS 与页大小）：https://github.com/FEX-Emu/FEX/discussions/3267
61. Apple Developer Forums 655950（arm64 不支持自定义 pagezero）：https://developer.apple.com/forums/thread/655950
62. Phoronix：Wine macOS ARM64 初始补丁（2020，搜索摘要）：https://www.phoronix.com/news/Wine-ARM64-macOS-Initial-Patch
63. neugierig.org：Emulating x86 on x64 on aarch64（2023-08，搜索摘要）：https://neugierig.org/software/blog/2023/08/x86-x64-aarch64.html
64. UTM #5460（TSO 与私有 entitlement `com.apple.private.oahd`）：https://github.com/utmapp/UTM/issues/5460
65. Apple：`recommendedMaxWorkingSetSize`：https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize
66. DXVK `dxvk.conf`（`dxgi.maxDeviceMemory` 等）：https://raw.githubusercontent.com/doitsujin/dxvk/master/dxvk.conf
67. Gcenx/macOS_Wine_builds（`--enable-archs=i386,x86_64`）：https://github.com/Gcenx/macOS_Wine_builds
68. Apple Developer Forums 787702（子进程窗口与 Game Mode）：https://developer.apple.com/forums/thread/787702
69. Apple：LSSupportsGameMode：https://developer.apple.com/documentation/bundleresources/information-property-list/lssupportsgamemode
70. Ferrum（第三方闭源 FEX-on-macOS 产品，佐证业界方向）：https://pyrosoft.pro/ferrum/
71. Wine `loader/main.c` 提交历史（"loader: Use zerofill sections instead of preloader on macOS when building with Xcode 15.3"，Brendan Shanks，2024-06-21）：https://github.com/wine-mirror/wine/commits/master/loader/main.c
72. XNU `bsd/kern/mach_loader.c`（`enforce_hard_pagezero`，ARM64 要求 4 GB 硬 pagezero）：https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/mach_loader.c
73. Apple：macOS 27 Release Notes（DocC JSON，含 163213094、176042635、166398727、168097174、175697313、169548657）：https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-27-release-notes.json
74. 9to5Mac：Apple confirms macOS 27 Golden Gate launch date September 14（2026-09-09）：https://9to5mac.com/2026/09/09/apple-confirms-macos-27-golden-gate-launch-date-september-14/
75. Help Net Security：Wine 11 released（2026-01-14）：https://www.helpnetsecurity.com/2026/01/14/wine-11-released/
76. CodeWeavers：CrossOver 更新日志（23.7 起附带 MSync）：https://www.codeweavers.com/crossover/changelog
77. Wine `dlls/ntdll/unix/sync.c`（wine-11.0，`__APPLE__` 下的 `USE_FUTEX`、`os_sync_*` 与 `__ulock_*` 回退）：https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/dlls/ntdll/unix/sync.c
78. Wine `dlls/ntdll/unix/sync.c`（master，同上）：https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/sync.c
79. 本机 SDK 头文件 `/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk/usr/include/os/os_sync_wait_on_address.h`（`OS_SYNC_WAIT_ON_ADDRESS_SHARED`、`OS_SYNC_WAKE_BY_ADDRESS_SHARED`，`macos(14.4)`）
80. Homebrew `Casks/w/wine-stable.rb`（`disable! date: "2026-09-01", because: :fails_gatekeeper_check`）：https://raw.githubusercontent.com/Homebrew/homebrew-cask/master/Casks/w/wine-stable.rb
81. Phoronix：CrossOver 26（基于 Wine 11.0）：https://www.phoronix.com/news/CrossOver-26
82. Apple：macOS 26 Release Notes（DocC JSON，含 `nox86exec=1` 136764433、Game Mode 153127050）：https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26-release-notes.json

## 事实核查记录

> 核查日期 2026-09-26。本机复核使用同一台 M3 / macOS 26.5（25F71）/ SDK 26.5（ld-1267）开发机，临时程序只在 scratchpad 中编译运行。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 上游 `configure.ac`：x86_64 在链接器支持 `-no_huge`（Xcode 15.3+）时设 `wine_use_preloader=no`，loader 以 `-no_pie,-image_base,0x200000000,-no_huge,-no_fixup_chains,-segalign,0x1000,-segaddr,WINE_RESERVE,0x1000,-segaddr,WINE_TOP_DOWN,0x7ff000000000` 和 `-pagezero_size,0x1000` 链接；`loader/main.c` 声明 `__wine_reserve[0x1fffff000]` 和 `__wine_top_down[0x001ff0000]`；preloader 只用于 i386 或不支持 `-no_huge` 的链接器 | 属实 | 核对了 master 的 `configure.ac` 约第 984–1012 行和 `loader/main.c`。补充两点：`*)` 分支设 `wine_use_preloader=no`，所以 aarch64 既没有 preloader 也没有地址保留；这一改动来自 Brendan Shanks 2024-06-21 的提交 [1][2][71] |
| Apple Silicon macOS 26.5 上原生 arm64 进程无法映射低 4 GB；`-pagezero_size 0x4000` 在 exec 时被杀；默认 pagezero 下释放后 FIXED 分配仍返回 `KERN_INVALID_ADDRESS`；DTS 655950 称 arm64 不支持修改 pagezero；主机页 16 KB | 属实 | 在同一台机器上独立复现，并补测 0x100000000 同样失败。XNU `mach_loader.c` 的 `enforce_hard_pagezero` 对 ARM64 强制要求 4 GB 硬 pagezero，否则返回 `LOAD_BADMACHO`。已抓取的加载路径中看不到 entitlement 例外。结论从"推断"升级为"一手源码证实" [61][72] |
| Apple Support 102527 与 macOS 27 发布说明中的 Rosetta 时间线（163213094、176042635、166398727） | 属实 | 原文逐条核对无误。补充了报告漏掉的三条：168097174（"使用 Rosetta 打开"改为原生启动）、175697313（"设置 › 通用"列出与 macOS 28 不兼容的 Intel app）、169548657（"显示简介"加标签）。macOS 27 发布日 2026-09-14 另有 9to5Mac 佐证 [53][73][74] |
| Wine 11.0（2026-01-13）：macOS 上在 syscall dispatcher 中切换 `%gs`；新 WoW64 完全支持，移除 `wine64`；ARM64 4K 页模拟只适合简单程序；NTSync 仅限 Linux（6.14+） | 属实 | 已核对 ANNOUNCE.md 原文。补充：纯 32 位 `WINEARCH=win32` 前缀被弃用，新 WoW64 不支持。"NTSync 仅限 Linux"是依据"Linux kernel module"的推断，但准确 [10][11][75] |
| marzent/wine-msync（LGPL-2.1，`WINEMSYNC=1`，Mach semaphore、`__ulock_wait2`/`__ulock_wake`、POSIX shm、`bootstrap_look_up`）最后更新 2024-08-13；上游 Wine 11.0 没有；CrossOver 23.7 起提供，CX26 仍在 | 属实 | 补充：CX26 高级设置页仍列出 MSync，但不再列出 ESync；上游 wine-11.0 中 msync.c、esync.c、fsync.c 都不存在。附带更正：上游已经在进程内使用 `os_sync_*`，见下一条 [12][14][15][16][76] |
| M3 8 GB 上 `recommendedMaxWorkingSetSize` = 5,726,633,984 B（约 5.33 GiB，66.7%），`maxBufferLength` = 4 GiB | 属实 | 用独立的 Objective-C 程序复现了同样的数值。补充说明：2/3 比例是实测值，Apple 文档没有说明，因此建议改为运行时读取，不要硬编码比例 [65] |
| §2 与摘要："macOS 上没有 eventfd、futex……上游 Wine 在 macOS 上只能走 wineserver 往返"；"每次等待或唤醒都要和 wineserver 往返" | **部分属实，已更正** | 只有 NT 内核对象的等待（`NtWaitForSingle/MultipleObjects`）仍要和 wineserver 往返。上游 Wine 11.0 的 `sync.c` 在 `__APPLE__` 下定义了 `USE_FUTEX`：14.4+ 用 `os_sync_wait_on_address_with_timeout`/`os_sync_wake_by_address_any`（`OS_SYNC_WAIT_ON_ADDRESS_NONE`），否则退回 `__ulock_wait`/`__ulock_wake`。这套 shim 支撑 `NtWaitForAlertByThreadId`，进而支撑 `RtlWaitOnAddress`、SRW 锁、条件变量和临界区竞争，这些都不走 wineserver。据此修订了摘要、§1.4 表格、§2 以及 P0 第 4 条建议：msync 的范围限定在 NT 对象，迁移时复用上游模式，跨进程路径改用 `_SHARED` 标志 [77][78][79] |
| SDK 26.5 的 ld 已无法链接传统 preloader（`-nostartfiles -nodefaultlibs` 报 "must link with libSystem.dylib"）；Tahoe beta 拒绝没有 `LC_LOAD_DYLIB` 的二进制（Sikarugir #130） | 属实 | 本机用 ld-1267 独立复现了同样的报错。补充：加 `-static` 可以链接成功，但 #130 显示 dyld 会在运行时拒绝缺 `LC_LOAD_DYLIB` 的二进制，因此 `-static` 不是规避手段（本机没有 Rosetta，未实际运行 `-static` 产物）。上游 preloader 参数仍带 `-ldylib1.o` 和 `-mmacosx-version-min=10.7` [1][46] |
| Rosetta 2 死锁让 Blizzard 游戏（D2R、Overwatch 2、Diablo IV）在 2026 年 1–5 月不可用，需要 macOS 26.5 加 CrossOver 26.1（2026-05-18）才修好 | 属实，措辞已收紧 | CodeWeavers 原文确认需要两边同时改动。但 D2R 只在不丢失焦点时可玩，切换焦点时的崩溃仍在调查。已把摘要、§6 表格、风险第 4 条和已知问题库建议中的"D2R 已修复"改为"部分修复" [49][50] |
| Homebrew 于 2026-09-01 以未通过 Gatekeeper 为由禁用 `wine-stable` cask | 属实 | cask 文件原文为 `disable! date: "2026-09-01", because: :fails_gatekeeper_check`，已补为一手来源 [80] |
| CrossOver 27（计划 2027 年初）只支持 Apple Silicon 和 Sonoma+，不再运行 32 位 bottle；CrossOver 26（2026-02-10）基于 Wine 11 | 属实 | 补充：CX26 基于 Wine 11.0，附带 D3DMetal 3.0 和 DXMT 0.72；AppleInsider（2026-07-31）称 CX27"penciled in"2027 年初 [57][58][81] |
| macOS 26.0 新增 `nox86exec=1` boot-arg；`LSSupportsGameMode` 为 26 新增；从终端直接派生的二进制不会激活 Game Mode（153127050） | 属实 | 补充了命令 `sudo nvram boot-args="nox86exec=1"`（136764433）和发布说明里给出的 `open` 规避办法。**[推断，未验证]** Apple Silicon 上设置自定义 boot-args 可能需要降低启动安全性，CI 采用前要先确认；Cider 自己 `posix_spawn` 游戏进程时是否也会遇到 Game Mode 不激活，需要实测 [43][69][82] |
