# Wine 基线与补丁队列定案，及 msync 在 Wine 11 inproc_sync 架构下的集成

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 本次直接读取源码或官方文件（raw 源文件、ANNOUNCE、SDK 头文件、官方目录列表）核实；**[中]** = 第三方仓库 README、issue、作者自述数据，或搜索摘要转述；**[低]** = 推断。CX 源码核验依赖三个第三方镜像，均未与官方 tarball 逐字节比对：`dappermint/winecx` 的 `crossover-26.3.0` 分支（CX 26.3.0，VERSION 为 Wine 11.0）、`PhoenicisOrg/winecx` 的 master（CX 25.1.0，VERSION 为 Wine 10.0）、`MythicApp/wine` 的 `mythic-crossover-24.0.7-stable`（CX 24.0.7，VERSION 为 Wine 9.0）。本文不涉及许可证分析。本文已按独立事实核查结果修订，逐条结论见文末“事实核查记录”。

## 摘要

- **基线定案**：建议现在以**上游 wine-11.18**（2026-09-18）为开发基线，叠加 CX 26.3 补丁队列和社区的 msync 修复。之后跟随每个 devel tag 变基，但每两个 tag 才发布一次引擎。12.0-rc 期间按周跟进 rc，**Wine 12.0（预计 2027 年 1 月中下旬，属推断）作为 Cider 1.0 的引擎基线**。原样构建的 CX 26.3 只作对照 oracle 和回退引擎，不再维护独立的 “11.0-stable” 线。选 11.18 而不是 11.17，是因为 11.17 已导出 `SetThreadpoolTimerEx`，但它的 ntdll 初始化重构引入了 #60327、#60331、#60337 三个启动回归，这三个在 11.18（dl.winehq.org 时间戳 2026-09-18 22:55）才修掉 [30][34][35][59]。另外（**核查更正**，原文写作“上游 x.0 版本没有维护版”）：**上游自 9.0.1 之后就没有再出过稳定维护版**：10.0 目录下只有 rc1–rc6 和 10.0，oldstable 分支的 VERSION 仍是 10.0；11.0 目录下只有 rc1–rc5 和 11.0，stable 分支 2026-01-13 之后没有新提交（截至 2026-09-26）[36][37][58]。这不是一贯的规律，8.0.1、8.0.2、9.0.1 都发布过 [56][57]。不过从 10.0 以来的情况看，不能指望出现 11.0.x，所以“11.0-stable”实际上等于 Cider 要自己维护全部回移植 [40]。
- **Highball 回归不是变基造成的**：Highball 的 `x64-crossover26.3-r8…r12` 是用**未经变基**的 `crossover-sources-26.3.0.tar.gz`（Wine 11.0 底座）加 Highball 自己的 0001–0013 补丁构建的，PE 部分已经用 mingw-w64 gcc 编译 [23][24]。两个启动器用例都跑在 `sync=none` 下 [18][21][22]，所以 msync、变基和 llvm-mingw 都可以排除。但这些引擎带着 Highball 自己的补丁（0007 跨进程子窗口 swapchain、0008/0009 改写 TEB FiberData/QoS 槽位），这一因素**没有排除** [23][24]。Battle.net 的 libcef int3（#119）在设置 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` 后消失，属于图形栈和跨进程 swapchain 问题 [18][21]。GOG Galaxy 的问题是 `Qt6WebEngineCore.dll` 在偏移 0x292ada 处稳定触发的访问违例（r9 2026-09-16、r10 2026-09-18 是同一条指令），与 DXMT 开关和 `--disable-gpu` 都无关，根因未明 [22]。**核查更正**：0x292ada 这一细节出自 highball-db 的 gog-galaxy recipe，不在 #149 正文里；#149 正文报告的是 Highball 0.9.19、macOS 27.2 上登录窗黑屏 [19][22]。这些崩溃是否就是 08 号报告所说的 “DIY 回归”，核查者没有独立确认。CrossOver 产品在公开源码之外还有闭源层：每个进程都会 dlopen 的 `cxcompatdb.so`、按应用的设置、alt loader 服务，以及它自己的图形后端选择。DIY 构建和 CX 产品行为不等价，这是结构性原因 [28]。
- **msync 在 CX 26.3 中的集成方式**：它不是一条独立路径，而是 **Wine 11 inproc_sync 框架在 macOS 上的后端**。具体做法如下 [1]–[6]：
  - 服务器端 `get_inproc_device_fd()` 在设置了 `WINEMSYNC` 时打开 `/dev/null` 充当“设备”；
  - `get_inproc_sync_fd` 的应答新增 `shm_idx` 字段；
  - ntdll 中的 `linux_*_obj()` 在 `__APPLE__` 下转调 `msync_*()`。

  同步模式在 wineserver 启动时就固定下来。客户端如果没开 `WINEMSYNC`，连上 msync 服务器后会直接 `exit(1)`。
- **msync 的演进（源码核实）**：CX 24.0.7 用“每次等待一个 Mach semaphore”，池上限 1024。CX 25.1.0（2025-08-12）改为每线程一个 tid 槽，共 64 MB 共享内存，等待一律走 ulock（`UL_COMPARE_AND_WAIT_SHARED`），Mach semaphore 全部去掉。准确地说，`__ulock_wait2` 以 `weak_import` 声明，缺失时回退到超时以微秒计的 `__ulock_wait`，所以是“始终用 ulock”，不是“始终用 `__ulock_wait2`”。CX 26.3 又把共享内存从 POSIX `shm_open` 改为 Mach memory entry [2][3][9][10]。25.1.0 的源码只来自第三方镜像 PhoenicisOrg/winecx，未与官方 tarball 比对 [9]。marzent 的公开分支显示，这次重写发生在 2024-12-22 到 2025-08-12 之间 [11]。它与 25.1.0 的 “Fix for Steam downloads with msync enabled” 时间吻合，但无法证明就是那一项修复 [14]。
- **CX 26.3 msync 有真实缺陷**，已被 dappermint 修复并测量 [16][17]。**核查更正**：补丁作者日期是 2026-08-21，GitHub 上 `wine1117` 分支显示的提交/推送日期是 2026-08-25。
  - 服务器泵的清理条件写成 `if (i > 1)`，应为 `i > 0`，会留下陈旧注册；
  - 端口队列上限 50，队列满时发送线程（包括 wineserver 主线程）被阻塞；
  - 注册等待期间无界空转，与泵线程争抢 CPU 核（307f90f 并非直接改成睡眠，而是先做最多 256 次带 pause/yield 提示的有界空转，再把槽写成 3，并以 1 ms 为上限睡眠）；
  - 争用退出路径没有注销注册。

  我在独立阅读代码时也发现了前两个问题。
- **Steam CEF 与 msync**：Highball “Steam UI 在 msync/esync 下卡死”的结论，是在 Sikarugir **Wine 10** 引擎上得出的（2026-08-24）。该引擎的 msync 来源未核实 [20]。上面这些 msync 缺陷（空转、队列阻塞、陈旧唤醒）足以解释 CEF 看门狗报出的 “killing unresponsive browser”，但这只是推断。dappermint 的 README 把 Steam UI 和 msync 都列为可用 [15]。建议：先把 msync 修稳，并把“Steam UI 在 msync 下冷启动 20 次零看门狗”设为发布门禁。按进程组混用同步模式只能在同一个 wineserver 里实现，因为 Steamworks 依赖同一 prefix 的命名对象。这需要让服务器能够等待 msync 对象（见 §3）。
- **os_sync 可行**：CX msync 的等待和唤醒已经完全基于 ulock，可以一对一换成 macOS 14.4+ 的公开 API：`os_sync_wait_on_address_with_timeout`/`os_sync_wake_by_address_{any,all}` 加 `OS_SYNC_*_SHARED` 标志 [44]。仍然依赖的私有接口是 `bootstrap_register2` 和 `mach_msg2_trap`。上游 wineserver 本来就在用 `bootstrap_register2` [46]。WFUSync（2026-08-12 更新，基于 Wine 11.15）已经证明 “os_sync SHARED + 普通 wineserver 请求”这条路走得通。它自报的微基准比 CX msync 快约 8–15%，但属于合成测试，未被复现 [43]。
- **Wine 12.0 与 CX 27**：11.0 的节奏是 rc1 2025-12-05，rc1–rc4 每周一个，rc4→rc5 跨新年隔了 14 天，2026-01-13 发布；10.0 则用了 6 个 rc [36][37][38]。**核查更正**：原文写“每周一个 rc”，并不严格。据此推断 12.0-rc1 约在 2026-12-04，正式版约在 2027 年 1 月中下旬，误差约 ±1–2 周。这是推断，没有找到官方日程。CX 24/25/26 分别基于 Wine 9.0/10.0/11.0，CX 27 计划 2027 年初发布，大概率基于 12.0 [推断][41][42]。

## 详细调研

### 0. 五份报告的基线建议对账

| 报告 | 原建议 | 本次核实 | 定案 |
|---|---|---|---|
| 02 | 上游 + CX 26.3 补丁队列，原样构建 CX 26.3 作为 oracle | CX 26.3 差异足够紧凑。dappermint 和 marzent 两条独立的 “CX26.3→11.17” 树都已跑通，dappermint 称“大部分变基改动是机械性的”[11][15] | **采纳**。oracle 按 Highball 的做法固定 tarball 的 sha256：`ac99c8ca…6872`，149,054,023 字节 [23] |
| 03 | 跟 upstream devel，每两周变基 | 11.x 期间 inproc_sync 接口稳定，只有机械性改名（见 §2）[7][8] | **采纳**，但“变基”和“发布”分开：每个 tag 都变基，每两个 tag 发布一次 |
| 12 | 三条引擎线：11.0-stable、devel、cx-26.3 | 10.0 和 11.0 都没有维护版 [36][37][40][58]（**核查更正**：原文写“上游 x.0 没有维护版”，但 8.0.1、8.0.2、9.0.1 存在，准确说法是 9.0.1 之后再没出过 [56][57]）。11.0 缺 `SetThreadpoolTimerEx`、CALayerHost 跨进程 swapchain 和显示模式模拟等 [30][34][47] | **改为两条**：`cider/devel`（默认）加 `cx-26.3-oracle`（回退，仅内部或高级用户使用）。放弃 11.0-stable |
| 10 | 11.0 必须回移植 `SetThreadpoolTimerEx`，否则用 ≥11.17 | wine-11.0 和 CX 26.3 的 `kernelbase.spec` 都是注释掉的 `# @ stub SetThreadpoolTimerEx`，**根本不导出**。wine-11.17 为 `@ stdcall SetThreadpoolTimerEx(ptr ptr long long) ntdll.TpSetTimerEx` [30] | 满足：基线 ≥11.18 |
| 08 | DIY 的 CX26.3→Wine 11 构建上 Battle.net 和 GOG 回归，而 CX 26.3 产品可用 | Highball 并没有变基，用的是原树加 Highball 自己的补丁（§1）。Highball 的这些崩溃是否就是 08 号报告所指的回归，核查者未独立确认 | 结论修正为：回归来自配置、闭源层差异或 Highball 自身补丁，**不能**据此否定上游基线 |

### 1. Highball/dappermint 回归的根因

**事实层 [高]**
- 引擎构成：Highball 的 CX 引擎清单写的是 “Built from CodeWeavers' crossover-sources-26.3.0.tar.gz … with patches 0001…0009”。r12 另加 0010、0011，外加 x87sidecar 试验补丁 [24]。highball-engine 在 `macos-15-intel` runner 上构建，PE 部分用 mingw-w64 gcc。inputs.json 明确写着 “llvm-mingw is known to break Steam's login (frankea, 2026-08-04)” [23]。也就是说，**这个引擎没有经过任何变基**，但带有 Highball 自己的补丁（见下文根因表第 3 行）。
- #119 Battle.net（2026-09-16 开，Highball 0.9.12，M5 Max，r8，DXMT，`sync=none`）：崩溃点是 `0x6d3c00e1 libcef+0x16d00e1: int3`。模块地址在 4 GB 以下，是 WoW64 下的 32 位进程 [18]。int3 是 Chromium `CHECK`/`IMMEDIATE_CRASH` 的典型形态 [推断]。维护者给出的结论是 “the embedded browser's GPU process under DXMT … without the cross-process swapchain flag”。加上 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` 后不再崩溃，但登录区域变黑。能否正常绘制“尚未定论”，可能只是子窗口截图造成的假象。Steam 主窗口在 Wine 11 上加了该开关后能正常绘制（2026-09-19）[21]。
- GOG Galaxy（highball-db 的 gog-galaxy recipe，knownIssues）：`EXCEPTION_ACCESS_VIOLATION in Qt6WebEngineCore.dll at offset 0x292ada`，在 r9（2026-09-16）和 r10（2026-09-18）上是同一条指令。有没有跨进程开关、加不加 `--disable-gpu` 都一样 [22]。**核查更正**：原文把这段细节归到 #149，其实它只出现在 recipe 里。#149（“Install GOG Blocked”，2026-09-17 开）正文报告的是 Highball 0.9.19、macOS 27.2、M4 Pro 上登录窗黑屏，日志中有 “D3D11CoreCreateDevice: Adapter is not a DXVK adapter” 警告，没有提到 Qt6WebEngineCore 或 0x292ada [19]。
- 构建工具链：frankea/Whisky v4.0.0-beta.2 说明，beta.1 的 Steam 登录卡死通过逐模块二分定位到 llvm-mingw 编译出的 `kernelbase.dll`，之后 PE 部分改用 mingw-w64 gcc [26][15]。Highball 已经采用 gcc，所以这一项不是 #119 和 #149 的原因。

**根因归纳（按可能性排序）**

| 候选 | 证据 | 判断 |
|---|---|---|
| 图形栈配置：DXMT（Highball 分支）加跨进程 swapchain，以及 CEF GPU 进程画进别的进程的子窗口 | #119 加开关后 int3 消失 [21]。Wine bug 60263（UNCONFIRMED，参考补丁基于 11.16）[32] | **#119 的主因** [高] |
| CX 产品的闭源运行层缺失：CW Hack 24067 在 `start_main_thread` 中 dlopen `ntdll_dir/cxcompatdb.so`，并以默认可见性导出 `KeServiceDescriptorTable` 和 `prepend_dll_path` [28]。另有 bottlewrapper 的按应用设置、`CX_ALT_LOADER_SOCKET` alt loader、Graphics=Auto 按数据库选择后端 | 源码中只有加载点，没有实现 | **结构性差异** [高]；具体影响了哪个用例 [低] |
| Highball 自己的补丁：0002 撤销了 CX hack 18311（wined3d 优先用 Vulkan），0008/0009/0011 改写 macOS 上 `%gs` 槽位里的 FiberData、QoS 和 LastError | 这些补丁和回归出现在同一时间窗口（r4→r12）[23][24]。独立核查确认：变基和 msync 已被排除，但 Highball 补丁这一项没有排除 | 需要二分排除 [中] |
| macOS 上 TEB 只做了部分镜像：`%gs` 基址是 pthread TSD，只镜像 `Tib.Self`、`ThreadLocalStoragePointer`、`Peb`（CX 26.3 `signal_x86_64.c` 第 3075–3078 行）[29] | CX 的公开二进制补丁表只覆盖特定 libcef（72.x、85.x 等）和 `Qt5WebEngineCore 5.15.2`，内容是把 `mov rax, gs:[0x8]` 改成先读 `gs:[0x30]` 再加 8 [27]。**没有覆盖 Qt6WebEngine** | **#149 的首要假设** [低，可测] |
| 变基错误 | Highball 没有变基 | **排除** [高] |
| msync | 两个用例都是 `sync=none` | **排除** [高] |

**验证方法**：
1. 反汇编 `Qt6WebEngineCore.dll` 在 RVA 0x292ada 处的指令，确认是不是 `gs:[off]` 读取、off 是否属于未镜像的 TEB 字段。
2. 同一个 Galaxy 版本分别跑在 CX 26.3 试用版、不带任何 Highball 补丁的原样 CX 26.3 构建、以及 Cider 11.18 构建上。
3. 用 `WINEDEBUG=+seh,+loaddll,+module` 对比三者的差异。

### 2. CX 26.3 中 msync 与 Wine 11 `server/inproc_sync.c`、`ntdll/unix/sync.c` 的共存方式

**接入点逐一核对 [高]**

| 位置 | 上游 11.0 | CX 26.3 改动 |
|---|---|---|
| `server/inproc_sync.c` | 只有 `#ifdef NTSYNC_IOC_EVENT_READ` / `#else` stub（307 行）[7] | 新增 `#elif defined(__APPLE__)` 分支（全文件 499 行）。`get_inproc_device_fd()` 只在 `do_msync()` 为真时 `open("/dev/null")`。`struct inproc_sync` 持有 `struct msync *`。`get_inproc_sync_fd()` 返回的是 `shm_idx`，不是 fd。`abandon_inproc_mutexes()` 调用 `msync_abandon_mutexes()` [4] |
| `server/protocol.def` | — | `get_inproc_sync_fd` 应答新增 `unsigned int shm_idx; /* optional for index-based sync implementations */` [6] |
| `server/thread.c` | `init_first_thread` 用 `send_client_fd` 发送 ntsync 设备 fd | 设备改为 /dev/null。`get_inproc_alert_fd` 在 msync 下把 alert 对象的 shm 索引放进 `reply->handle` 返回，不传 fd [5] |
| `server/main.c` | — | 在 `open_master_socket()` 前调用 `msync_init_shm()`，之后调用 `msync_init()` [5] |
| `ntdll/unix/sync.c` | 没有 ntsync 设备时，所有 `inproc_*` 返回 `STATUS_NOT_IMPLEMENTED`，回退到 `server_wait()` | `#elif defined(__APPLE__)` 下 `linux_{release_semaphore,set_event,reset_event,pulse_event,release_mutex,query_*,wait_objs}_obj` 转调 `msync_*`。`get_server_inproc_sync`、`release_inproc_sync`、`get_inproc_alert_fd` 三处加 `if (do_msync())` 分支，把 “fd” 当作 shm 索引使用。另外 CW Hack 23015 为 `NtCreateSemaphore`、`NtReleaseSemaphore`、`NtCreate/Set/Reset/Clear/PulseEvent` 生成 `GPT_ABI_WRAPPER`，供 libd3dshared 以 ms_abi 调用 [1] |
| `ntdll/unix/loader.c` | — | `start_main_thread()` 中依次调用 `server_init_process()` → `hacks_init()` → `msync_init()` [28] |

**模式如何选择 [高]**：服务器端和客户端各自在 `do_msync()` 里缓存 `getenv("WINEMSYNC") && atoi(...)` 的结果 [2][3]。服务器在启动时注册 `wine-<config_dir inode>-msync` 这个 bootstrap 服务，用的是私有接口 `bootstrap_register2`。CX 的 `server/msync.c` 把它声明为 `int flags`，上游 wine-11.18 的 `server/mach.c` 声明为 `uint64_t flags`，两者 ABI 略有出入，移植时要统一成上游的声明 [3][46]。消息泵线程设为 latency/throughput QoS tier 0、`timeshare=0`、importance 63。端口队列 `mpl_qlimit` 默认 50，可用 `WINEMSYNC_QLIMIT` 覆盖 [3]。客户端两种不一致的情况都会退出：
- 客户端没开 msync：调用一次 `get_inproc_alert_fd`，如果返回值不是 `STATUS_INVALID_PARAMETER`，就报 “Server is running with WINEMSYNC but this process is not…” 并 `exit(1)`；
- 客户端开了 msync 而服务器没开：`bootstrap_look_up` 失败，同样 `exit(1)` [2]。

**因此，msync 是由 wineserver 启动时的环境变量决定、整个 prefix 统一生效的一种 inproc 后端，不是可以按进程选择的路径。**

**对象覆盖面 [高]**：进程、线程、job、消息队列、APC、上下文和计时器都通过 `create_internal_sync()` 获得 inproc sync（在 msync 下类型为 `MSYNC_*_SERVER`），由 wineserver 主线程调用 `msync_set_event()` 触发 [5]。inproc 对象的 `add_queue` 是 `no_add_queue`，会返回 `STATUS_OBJECT_TYPE_MISMATCH`，所以**服务器无法代替客户端等待 msync 对象**。如果一次等待里混了尚未转换成 internal sync 的对象，就会失败。这一限制与 Linux 上的 ntsync 相同 [4]。

**等待机制 [高]**：
- 对象状态放在 16 字节的共享槽里：`{low, high, type:16, refcount:16, multiple_waiters}`。
- 单对象等待直接在对象字上执行 `__ulock_wait2(UL_COMPARE_AND_WAIT_SHARED)`。`__ulock_wait2` 以 `weak_import` 声明，缺失时回退到 `__ulock_wait`（超时单位为微秒）[2]。
- 多对象或可警报等待的流程是：
  1. 把自己的 tid 槽写成 2；
  2. 用 `mach_msg2` 发送注册消息，`msgh_id = tid<<8 | count`；
  3. **空转等待**泵线程把槽改成 1；
  4. 在 tid 槽上 ulock 等待。
- 唤醒方先执行 `__ulock_wake(…|ULF_WAKE_ALL)`；如果 `multiple_waiters > 0`，再给泵线程发消息，由泵线程逐个 `wake_tid()` [2][3]。
- 语义上的已知缺口（代码注释中自己承认）：PulseEvent 不精确；WaitAll 先逐个等待，再在紧凑循环里一起抢占，**不是原子的**；放弃互斥锁的处理是 “HACK”。ntsync 规范要求 WaitAll “atomic and totally ordered”[48]。

**11.x 变基成本 [高]**：从 11.0 到 master（11.18），`inproc_sync.c` 和 `sync.c` 只有机械性改动：
- `object_ops` 改为指定初始化器，`inproc_sync.c` 同时去掉了 `WIN32_NO_STATUS`（307 行→290 行）；
- `ntdll_get_thread_data()` 改为 `get_thread_data()`；
- `alloc_object_attributes` 改为 `wine_server_alloc_object_attributes`；
- `sync.c` 中 `make_client_id` 的调整；
- 新增 `NtOpenPrivateNamespace` stub [7][8]。

dappermint 的 `wine1117` 和 marzent 的 `ff-wine-11.17` 都已经带着 msync 跑在 11.17 上。marzent 版的客户端 msync.c 与 CX 26.3 逐字相同 [11][16]。

**25.1.0 “Fix for Steam downloads with msync enabled” 改了什么** [14]：

| 版本 | 共享内存 | 多对象等待 | 单对象等待 | 与服务器的关系 |
|---|---|---|---|---|
| CX 24.0.7（Wine 9.0） | POSIX `shm_open` | 每次等待从池里取一个 Mach semaphore（`MAX_POOL_SEMAPHORES 1024`），用 `semaphore_timedwait` 等待 | 通过 dlsym 取到 `__ulock_wait2` 时使用 | msync 是一条独立路径，通过 object_ops 的 `get_msync_idx` 接入 |
| CX 25.1.0（Wine 10.0） | POSIX shm，另加 64 MB 的 `-tid` 映射 | tid 槽加 ulock SHARED，**不再使用 Mach semaphore** | 始终使用 ulock | 同上，并与 esync 并存 |
| CX 26.3.0（Wine 11.0） | Mach memory entry（`mach_make_memory_entry_64`，按页请求） | 同 25.1 | 同 25.1 | 作为 inproc_sync 后端，esync 已删除 |

来源：[9][10][2][3]。25.1 和 26.3 的“始终使用 ulock”是指 `__ulock_wait2`（weak_import）或回退的 `__ulock_wait`。25.1.0 一行的依据是 PhoenicisOrg/winecx master（VERSION 10.0，顶端提交 “winecx-25.1.0”，2025-09-13），属第三方镜像，未与官方 tarball 比对 [9]。

marzent 的公开分支里，`ff-wine-9.19` 和 `ff-wine-10.0-rc3`（2024-12-22）仍使用 `semaphore_create`，`ff-wine-10.0`（2025-12-10）已改为 `shm_tid_map` [11]。独立核查复验了代码内容（rc3 有 3 处 `semaphore_create`、0 处 `shm_tid_map`；10.0 为 0 处和 4 处），但**括号里的分支日期没有复验** [中]。所以这次重写落在 2024-12-22 到 2025-08-12 之间。因为拿不到 25.0.x 的源码，**“25.1.0 的那项修复就是这次重写”只能算中低置信度的推断**。可能的机理是：Steam 下载阶段多对象等待密集，semaphore 的创建和销毁、1024 的池上限，以及 `KERN_TERMINATED` 路径都会成为瓶颈或出错点 [推断]。Marc-Aurel Zent（msync 作者）自 2024 年起受雇于 CodeWeavers [13]。

### 3. Steam CEF 在 msync/esync 下卡死；能否按进程组混用同步模式

**现有证据**
- Highball 的 Steam recipe 写道：“Steam's own CEF interface hangs under msync/esync, but GAMES gain ~40% frame rate from msync. Wine's sync mode is fixed when the prefix's wineserver starts…”。症状是登录窗不出现，webhelper.txt 里出现 “killing unresponsive browser”。验证环境为 `x64-sikarugir10.0_6-r0`、M1 Pro、macOS 14.6、2026-08-24 [20]。这是 Wine 10 引擎，msync 实现来源未核实。
- dappermint README 把 “steam's ui end to end … msync” 列为可用（M5）[15]，但没有明确说是“UI 本身跑在 msync 下”。

**CX 26.3 msync 中可能拖垮 CEF 的机制**（代码级，[推断]，但每一项都有对应的修复提交）[16][17]：
1. **注册空转**：CEF 的 UI 线程通过 `MsgWaitForMultipleObjectsEx` 等待“句柄 + 消息队列”，至少两个对象，必然走多对象路径。每次都要一次 Mach 往返，再空转等待泵线程确认。多个进程的几十个线程同时这样做，会在 8 核 M 系列上与泵线程抢核（修复提交 307f90f：最多 256 次带 pause/yield 提示的有界空转，之后把槽写成 3，以 1 ms 为上限睡眠）。
2. **队列阻塞**：泵端口队列上限 50。队列满时 `mach_msg` 发送方阻塞，而 wineserver 主线程也会通过 `signal_all()` 给自己的端口发消息，因此整个 wineserver 可能被卡住（修复提交 3a7a712，改为 `MACH_PORT_QLIMIT_MAX`，即 `MACH_PORT_QLIMIT_LARGE` = 1024，并对 `WINEMSYNC_QLIMIT` 覆盖值做钳位 [45]）。
3. **陈旧注册**：`i > 1` 这个差一错误，加上争用退出路径不注销，会让已离开的线程被错误唤醒，节点池（`MAX_POOL_NODES 0x80000`）泄漏（修复提交 8df1826、9be392b）。
4. **语义**：没有等待唤醒后的优先级提升，“连续两次 set 丢掉一次唤醒”。Highball 为 CS:GO 加了 0005 补丁，在 set 之后执行 `sched_yield()` [23]。

esync 在 macOS 上依赖 fd（每个对象占一个或多个描述符），CX 自己的 README.esync 就以 fd 耗尽为头号已知问题 [54]。CEF 多进程、多对象的负载下更容易撞到上限 [推断]。

**混用同步模式的可行性分析**

| 方案 | 可行性 | 说明 |
|---|---|---|
| 每个 bottle 会话一个 wineserver，Steam UI 与游戏分开 | **对 Steam 不可行** | wineserver 与 prefix 一一对应。Steamworks IPC 依赖同一 wineserver 里的命名对象，Highball 也只能靠“重启 wineserver 切换模式” [20] |
| 同一个 wineserver 内按进程放弃快路径 | **可行，但要改服务器** | 当前客户端不一致会直接 `exit(1)` [2]。需要实现“服务器可等待的 msync 对象”：inproc 对象的 `add_queue` 通过泵线程通知主循环（kqueue `EVFILT_USER` 或管道）；由服务器对共享内存执行 CAS 来满足等待；由服务器代为执行 set/release。这样还能顺带实现原子的 WaitAll。WFUSync 的 `try_wait_all_wfusync` 和 `WFUSYNC_SERVER_WAITERS` 是现成参考 [43] |
| 把 msync 修稳，Steam UI 也跑 msync | **首选** | 先移植 dappermint 的 8 个 msync 提交，再用门禁来证明 |

结论：短期保留 bottle 级的 `sync=server` 开关和自动重启 wineserver 作为兜底，这和 Highball 的做法一致。主线则是修稳 msync，再补上“服务器可等待对象”，从而支持按进程放弃快路径，**不再依赖 `WINEMSYNC=0` 来保住 Steam UI**。

### 4. 基于 `os_sync_wait_on_address` SHARED 重做 msync；WFUSync

**API 事实 [高]**（取自 MacOSX26.5.sdk 的 `os/os_sync_wait_on_address.h`）[44]：
- 可用性为 `macos(14.4)`；`size` 只能是 4 或 8；
- `OS_SYNC_WAIT_ON_ADDRESS_SHARED` 与 `OS_SYNC_WAKE_BY_ADDRESS_SHARED` 用于跨进程共享内存，等待和唤醒两侧必须使用相同的地址、大小和 shared 标志，否则返回 EINVAL；
- `_with_timeout` 的超时值不能为 0，否则 EINVAL；
- 需要处理 EINTR、EFAULT、ENOMEM 后重试；
- 头文件明确说明该 API 不提供优先级反转规避，锁类原语应优先用 `os_unfair_lock`。

**映射方式**：
- `ulock_wait(UL_COMPARE_AND_WAIT_SHARED|ULF_NO_ERRNO, addr, v, ns)` 对应 `ns ? os_sync_wait_on_address_with_timeout(addr, v, 4, OS_SYNC_WAIT_ON_ADDRESS_SHARED, OS_CLOCK_MACH_ABSOLUTE_TIME, ns) : os_sync_wait_on_address(addr, v, 4, OS_SYNC_WAIT_ON_ADDRESS_SHARED)`；
- `__ulock_wake(…SHARED|ULF_WAKE_ALL)` 对应 `os_sync_wake_by_address_all(…, OS_SYNC_WAKE_BY_ADDRESS_SHARED)`，单个唤醒对应 `_any`；
- 返回值从 `-errno` 风格改为读取 errno。

上游 Wine 的进程内 futex 路径早已由 Zent 迁移到 os_sync（`OS_SYNC_WAIT_ON_ADDRESS_NONE`，并保留 `__ulock` 回退）[1]。**Cider 最低系统版本定为 macOS 14.4 即可去掉 `__ulock_wait2`**，CX 27 本身也只支持 Sonoma+ [42]。

迁移后仍然剩下的私有依赖：
- `bootstrap_register2`：上游 `server/mach.c` 本来就在用 [46]，不构成新的暴露面。公开的 `bootstrap_register` 自 10.5 起已弃用 [45]。注意上游声明的 flags 是 `uint64_t`，CX msync 是 `int`，移植时统一为上游声明（见 §2）。
- `mach_msg2_trap`：通过 dlsym 获取，取不到时回退到公开的 `mach_msg`。

**WFUSync [中]** [43]：
- Radim Vesely，2026，LGPL-2.1+。v5 为测试版，针对 WineForge/Wine 11.15，最后提交于 2026-08-12，用 `WINEWFUSYNC=1` 开启。
- 设计：同样挂在 inproc_sync 上，同样新增 `shm_idx` 应答字段。共享内存用 `shm_open` 后立即 `shm_unlink`。等待和唤醒用 os_sync SHARED，旧系统回退到 `__ulock`。
- 多对象等待通过**普通 wineserver 请求**完成：`get_wfusync_wait_slot`、`register/unregister_wfusync_wait`、`wake_wfusync_waiters`、`try_wait_all_wfusync`。不使用 Mach IPC。对象上有 `STATE_LOCKED`、`MULTI_WAITERS`、`SERVER_WAITERS` 标志位和 generation 计数。
- 在不支持或敏感的路径上（例如 Semaphore query、有争用的 WaitAll）刻意回退到服务器。
- 自报的微基准（每个场景 100 万次操作）：

| 场景 | WFUSync（ms/op） | CX MSync（ms/op） |
|---|---|---|
| Auto Event set/wait | 0.000581 | 0.000666 |
| Semaphore | 0.000597 | 0.000683 |
| Mutex | 0.000625 | 0.000686 |
| WaitAny | 0.000627 | 0.000704 |
| WaitAll | 0.001186 | 0.001291 |

  压力测试：WaitAll 注册 1000 次零失败，竞争 WaitAll 200 轮零失败。作者本人声明这些是合成数据，不代表帧率。

**评价**：WFUSync 证明“公开 API + 无 Mach 泵”在语义上可行。但多对象注册要占用 wineserver 的单线程主循环，而 msync 用的是独立的高 QoS 泵线程，两者在高并发下孰优孰劣需要实测。它测的大概率是无争用的往返，不能回答 CEF 或游戏在负载下的尾延迟问题 [推断]。

### 5. Wine 12.0 的时间与内容；CrossOver 27

- **时间 [高→推断]**：
  - 11.0：rc1 2025-12-05、rc2 12-12、rc3 12-19、rc4 12-26、rc5 2026-01-09，正式版 2026-01-13 [36][38]。rc4→rc5 相隔 14 天（跨新年），不是每周；
  - 10.0：rc1 2024-12-06，共 rc1–rc6，2025-01 中旬发布 [37][39]；
  - 11.x 每两周一个 devel 版，11.18 为 2026-09-18 [35]。

  据此推算：11.19 约 10-02、11.20 约 10-16、11.21 约 10-30、11.22 约 11-13（可能还有 11.23，约 11-27）；**12.0-rc1 约 2026-12-04（周五）**，之后大致每周一个 rc。**核查更正**：原文写“每周一个 rc”，但节假日会出现两周间隔，rc 的个数也在 5 到 6 个之间变化，所以 **12.0 约在 2027 年 1 月中下旬**（中心估计 2027-01-12 至 01-19，误差约 ±1–2 周）[推断]。截至 2026-09-27 的检索，仍没有找到 Julliard 公布的 12.0 日程 [36][37]。
- **10.0 和 11.0 没有维护版**：dl.winehq.org 的 10.0 目录没有 10.0.1，11.0 目录只有 rc 和 11.0 [36][37]。GitHub 镜像上 `oldstable` 分支的 VERSION 仍是 10.0，`stable` 分支 2026-01-13 之后没有新提交（截至 2026-09-26）[58]。搜索摘要称稳定分支没有维护者 [40][中]。**核查更正**：这不是历来如此，dl.winehq.org 上有 wine-8.0.1、wine-8.0.2、wine-9.0.1 [56][57]。准确的说法是“9.0.1 之后再没出过稳定维护版”。“不要指望 11.0.x”这个结论不变。
- **内容（据 11.x 实际合入推断）**：vkd3d 2.x、FFmpeg 版 winedmo、Mono 11.3（含 ARM64）、SymCrypt、显示模式模拟（11.17）、NTOSKRNL 与 PnP 驱动支持（11.18）、ARM64EC 的 mingw 模式、C++ 构建支持 [34][35]。macOS 方面包括：
  - marzent 的 CALayerHost 跨进程 `MetalViewSwapChain`，**只支持根窗口**，子窗口仍未实现，即 bug 60263 [47][32]；
  - 11.17 让 macOS 主线程以 system thread 身份启动，并移除 `WineDisplayLink` 等废弃 API [34]。

  **未发现 macOS inproc 后端或 msync 的上游合并迹象** [推断]。所以 12.0 之后，msync、D3DMetal ABI 和 Rosetta hack 仍需要 Cider 自己携带。
- **CX 27 [中]**：2026-06-11 宣布只支持 Apple silicon 和 Sonoma+，并删除 32 位 bottle。2026-07-31 发布 ARM64 预览版（没有 D3DMetal，许多启动器不可用），计划 2027 年初正式发布 [41][42]。以 CX 24/25/26 对应 Wine 9.0/10.0/11.0 的规律推断，CX 27 大概率基于 12.0，源码约在 2027 年 2–3 月公开 [推断]。届时 CW HACK 20810（32 位 bottle 模拟）会从队列中消失，FEX 和 ARM64 相关的胶水会增加。

### 6. 补丁队列的分类、工具、节奏与 CI 门禁

**分类（目录前缀即类别，提交尾注必填）**

| 类别 | 内容 | 退出条件 |
|---|---|---|
| `up/` | 从上游 master 提前摘取的提交 | 变基时 `git cherry`/patch-id 命中即自动丢弃 |
| `mr/` | 上游待合 MR 或 bug 附带的补丁，例如 60263 | 被合并即丢弃 |
| `cx-sync/` | msync 及其修复 | 长期携带 |
| `cx-abi/` | D3DMetal ABI：`macdrv_functions`（192 B）、22434、22435、23015，以及经 `nm -u libd3dshared.dylib` 确认需要的 24067 子集 | 跟随 GPTK 版本 |
| `cx-rosetta/` | 24256、23427、20186、22131、24265 | 切换到 FEX 后重评估 |
| `cx-mac/` | 10912、14364、18896、22310、22144 | 尽量上游化 |
| `cx-app/` | CEF/Qt 二进制补丁（16900、18582、19114、21548、22584、23854、19252、25737、22901）、24938、19252 ICD、24920/24557、22996 | 逐步改成数据驱动 |
| `cider/` | Cider 自有：按应用追加命令行（参考 Highball 0006）、TEB/`%gs` 镜像（参考 0008/0009/0011，只在有测试时引入）、SEH personality 修复（dappermint）、服务器可等待的 msync 对象 | — |
| `drop/`（不移植） | 24067 的 cxcompatdb dlopen 块、10523/23741 alt loader、12735 用户名、20810 32 位 bottle；18311 待评估 | — |

尾注字段：`Cider-Class:`、`Upstream-Status: Backport <sha> | MR !n | Pending | Cider-only | CX-Hack <id>`、`Owner:`、`Test:`、`Drop-When:`。这与 Proton 在提交中使用 `CW-Bug-Id: #nnnnn` 尾注的做法一致 [51]。

**工具**：以普通 git 线性分支为真源（`cider/devel`），并启用 `git rerere`。
- 变基用 `git rebase --onto wine-11.19 wine-11.18`；
- 用 `git range-diff` 审查新旧两版补丁栈的差异；
- 用 `git cherry` 找出已经上游化的补丁；
- 在 CI 中用 `git rebase --exec` 逐个提交构建，上游 build-mac 也是这样做的（见 12 号报告）。

另写一个小脚本 `tools/queue.py`，负责校验尾注、按类别排序、导出 `patches/` 和 `series` 供外部审阅。它参考 wine-staging 的 `staging/upstream-commit` 加 `patches/*/definition`（含 Fixes/Depends）结构 [50]。stgit 适合个人本地使用 [53]，git-series 可以用来保存补丁系列的历史和 cover letter [52]，但都不作为强制工具。不用 quilt。

CX 源码导入流程：
1. 固定 tarball 的 sha256 并自行归档；
2. 把 CX 树覆盖到对应的 `wine-X.0` tag 上，提交；
3. 用 `grep -rniE "(CW|CX|CrossOver) ?HACK"` 生成 hack 清单；
4. 新 CX 版本发布时做差异的差异（例如 26.3→27）。

**节奏**：
- 每个 devel tag 发布后 48 小时内完成变基并通过 G0–G3；每两个 tag 发布一次引擎（G0–G6 全绿）。
- 12.0-rc 期间每周变基。
- 12.0 发布后切出 `cider/12` 维护分支（回移植由 Cider 自己负责），`cider/devel` 继续跟 12.x。
- CX 每个源码包发布后，两周内完成分诊。

**CI 门禁**：

| 门禁 | 内容 |
|---|---|
| G0 | 逐提交构建，保证可二分 |
| G1 | Wine 一致性测试子集：`ntdll/tests/sync.c`（event、mutant、semaphore、keyed_events、wait_on_address、tid_alert、completion_port、barrier）[49]、`kernel32/tests/sync.c`、`kernel32/tests/thread.c`、`user32/tests/msg.c`。在 server 和 msync 两种模式下各跑一遍，只看新增失败 |
| G2 | msync 语义与压力套件（§建议 3） |
| G3 | 打包检查，参考 winecx-gptk：可重定位、所有 dylib 都能 dlopen、i386 PE 部分非空、PE 已 strip、能开窗口、MF 有解码器 [15] |
| G4 | 启动器冒烟：Steam（安装、登录窗、商店页）、Battle.net、GOG Galaxy、EA、Rockstar、WebView2 与 cefclient 的固定版本；每一项都与 CX 26.3 oracle 做 A/B |
| G5 | 性能回归：同步微基准，加 3–5 个游戏的固定场景 |
| G6 | 工具链：PE 部分用 mingw-w64 gcc；另跑一次 llvm-mingw 构建，只对比不发布 |

## 对 Cider 的启示与建议

**P0（2026-10）**
1. **基线**：`cider/devel` 等于 wine-11.18 加 CX 26.3 队列。移植时以 dappermint/`wine1117` 和 marzent/`ff-wine-11.17` 两棵树交叉核对 [11][16]。11.19 发布（约 10-02）后立刻变基。
2. **oracle 与回退**：原样构建 CX 26.3（固定 tarball sha256，PE 用 gcc），作为 A/B 对照和用户可选的回退引擎。不做 11.0-stable 线。
3. **msync 移植（阶段 M0–M1）**：
   - 按 §2 的清单搬运接入点，外加 11.x 的机械性改名，并把 `bootstrap_register2` 的声明统一为上游的 `uint64_t flags`；
   - 先移植 dappermint 的 8 个 msync 提交（8df1826 `i > 0`、3a7a712 QLIMIT 改为 1024 并钳位覆盖值、307f90f（256 次有界空转后以 1 ms 为上限睡眠）和 6d31614 去掉无界空转、9be392b 争用退出时注销、620d8c5 无等待者时跳过唤醒、a7ef7b3 唤醒一个、ef72fdb 已置位时跳过 exchange）；
   - 新增计数器：注册次数、陈旧唤醒、队列深度、泵延迟直方图，由 `WINEMSYNC_STATS=1` 打开；
   - 默认对 bottle 开启 msync；Steam UI 暂时保留 `sync=server` 兜底开关，并在切换时自动重启 wineserver。
4. **启动器回归清零**：先在 oracle 上确认 CX 26.3 本身能跑 Battle.net 和 GOG。然后按 §1 的方法二分：图形后端 → Highball 类补丁 → TEB 镜像。Highball 自身补丁是独立核查唯一没有排除的因素，二分时不能跳过。GOG 的复现要同时核对 #149 报告的登录窗黑屏和 recipe 记录的 0x292ada 崩溃，这可能是两个现象 [19][22]。先移植 bug 60263 的子窗口跨进程 swapchain 补丁。Highball 的 0007 和 dappermint 的“CAContext 托管 + win32 状态镜像”补丁都实现了这一功能，可以互相对照 [15][23][32]。

**P1（2026-11 → 2027-01）**
5. **M2：换用公开 API**。最低系统定为 macOS 14.4，用 os_sync SHARED 取代 `__ulock_wait2`/`__ulock_wake`；`mach_msg2_trap` 保留 dlsym 加回退。
6. **M3：让服务器可以等待 msync 对象**。实现 inproc 对象的 `add_queue`/`satisfied`，由服务器代为执行 set/release，并用服务器辅助实现原子 WaitAll。之后开放按进程放弃快路径（`WINEMSYNC_CLIENT=0`），不再需要重启 wineserver。
7. **12.0 跟进**：rc 期间每周变基；12.0 发布后作为 Cider 1.0 引擎。12.0 日期是推断（2027 年 1 月中下旬，±1–2 周），Cider 1.0 的排期要留出至少 2 周余量。

**P2（2027 Q1 以后）**
8. 用实验开关 `WINEMSYNC=2` 尝试去掉泵：共享内存中的等待者列表加对象锁位，直接用 os_sync 唤醒（WFUSync 式），与 M3 对比尾延迟。
9. CX 27 源码公开后做差异的差异，重点看 ARM64/FEX 胶水、D3DMetal（GPTK 4）ABI，以及 msync 是否有新改动。
10. 把 msync 以“macOS inproc 后端”的形式提交上游 MR，降低长期携带成本。成败取决于上游是否接受 Mach 泵这类私有接口，概率不高 [推断]。

**必须携带的 CX hack 与负责人**（团队规模未知，这里按角色分配，初期可由同一人兼任）：

| 组 | 条目 | 负责人 | 备注 |
|---|---|---|---|
| 同步 | msync 全套 + protocol `shm_idx` + inproc 接入点 | **S**（同步） | 加上社区修复和 M2/M3 |
| D3DMetal ABI | `d3dmetal.c`/`d3dmetal_objc.m`、22434（PE 与 unix 两侧）、22435、23015 `GPT_ABI_WRAPPER`、`hook(localtime)`（PEB 放在 pthread TLS 0x60 处）、24067 子集 | **G**（图形/ABI） | 用 `nm -u` 确认后再决定是否携带 24067 |
| Rosetta | 24256、23427、20186、22131、24265（M3 专用的 MXCSR 问题） | **C**（CPU） | 开发机就是 M3，必须携带 |
| macOS 集成 | 10912、14364、18896、22310、22144 | **M**（mac 驱动） | 争取上游化 |
| 应用兼容 | CEF/Qt 二进制补丁表、24938、19252、24920/24557、22996 | **A**（应用兼容） | 改造成规则引擎 |
| 构建 | gcc PE、oracle 构建、G0–G6 | **B**（构建/CI） | — |

**测试与基准计划：把 NT 对象等待与用户态锁分开**
- **U 组（用户态锁，不受 msync 影响）**：CRITICAL_SECTION 争用乒乓、SRWLOCK 读写、CONDITION_VARIABLE 生产者消费者、`WaitOnAddress`/`WakeByAddress*`、InitOnce。它们在上游走的是 futex（os_sync）路径 [1]。**server 和 msync 两种模式下的差异应在 ±3% 以内**。超出说明测量有误或存在干扰，这组用作“归因校验”。
- **N 组（NT 对象，受 msync 影响）**：
  - 各类对象的单对象乒乓：auto/manual event、semaphore、mutex；
  - WaitAny，对象数 2、8、64；WaitAll，对象数 2、8；
  - `MsgWaitForMultipleObjectsEx`；可警报等待加 `QueueUserAPC`；`SignalObjectAndWait`；
  - 跨进程场景：命名对象、`DuplicateHandle` 给子进程；
  - 超时精度：0、1、15.6、100 ms；
  - 线程或进程被杀后的放弃互斥锁；等待期间关闭句柄；PulseEvent。

  语义以 ntsync 文档为规范 [48]，每项都要求与 server 模式结果一致，WaitAll 需在 M3 之后达到原子。
- **S 组（服务器对象）**：进程和线程句柄、计时器、IOCP、管道重叠 IO，以及与 N 组混合的等待，用来验证 internal sync 的覆盖面。
- **指标**：每秒操作数；唤醒延迟 p50/p99/p99.9；每次操作的 user+sys CPU；wineserver 和泵线程的 CPU 占用；上下文切换次数（`proc_pid_rusage`）；能耗（powermetrics）。分别以 2、4、8 线程运行（M3 为 4P+4E）。
- **应用级**：
  - Steam UI 冷启动 20 次，要求零次 “killing unresponsive browser”；
  - Steam 下载 5 GB，吞吐量与 server 模式持平；
  - CS:GO legacy 地图加载（丢失唤醒问题）；
  - 一款 D3DMetal DX12 游戏和一款 DXMT 游戏的帧时间 p99；
  - Battle.net、GOG、cefclient。
- **门槛**：N 组单对象乒乓的 p50 至少比 server 模式快 5 倍；应用级不出现回归；1 小时随机压力测试无挂起、无泄漏（泵节点池计数不增长）。

## 风险

- **私有接口**：`__ulock_wait2`/`__ulock_wake`（M2 后移除）、`bootstrap_register2`、`mach_msg2_trap`，以及 winemac 的 `CAContext`/`CALayerHost` 都可能随 macOS 更新失效 [3][46][47]。
- **Rosetta 时间线**：macOS 28（2027）将大幅停用 Rosetta。x86_64 引擎和只有 x86_64 版的 D3DMetal 都在倒计时，msync 将来还要在 FEX 上的 arm64 Wine 里验证 [15][42]。
- **devel 回归**：例如 11.17 的初始化回归要到 11.18 才修掉 [35]；11.x 中 winemac 的 ObjC 重构还会反复打乱 CX 的 winemac 补丁 [34][35]。
- **镜像失真**：CX 源码全部取自第三方镜像，之前已有镜像 404 的情况。必须自行归档 tarball（02 号报告）。
- **msync 语义缺口**：WaitAll 不是原子的，PulseEvent 不精确，放弃互斥锁有近似处理，可能导致个别应用异常 [2]。“唤醒一个”的优化（a7ef7b3）如果与超时或 APC 交互不当，会引入丢失唤醒，需要 G2 覆盖 [16]。
- **8 GB 开发机**：完整构建加上 CEF 多进程测试会导致内存交换，基准噪声大。需要固定测试条件，或者用 CI 机器跑基准。

## 未解问题

1. CX 25.0.x 的 msync 仍是 semaphore 版还是已经换成 tid 槽版？这决定 25.1.0 的 Steam 下载修复是否就是那次重写。需要 25.0.1 的 tarball。
2. `Qt6WebEngineCore.dll` 在 RVA 0x292ada 处的指令到底是什么？它与 #149 报告的登录窗黑屏（Highball 0.9.19，macOS 27.2）是不是同一个问题？CX 26.3 产品为什么能跑 GOG Galaxy（26.3.0 changelog：“GOG Galaxy client not loading after update”）？修复在公开源码里，还是在 cxcompatdb.so 里？
3. Sikarugir Wine 10 引擎的 msync 是 marzent 的旧 semaphore 版吗？它上面 Steam UI 卡死的具体机理是什么？
4. libd3dshared 实际导入了哪些 ntdll 和 winemac 符号？24067 子集是否需要携带？
5. llvm-mingw 编出的 `kernelbase.dll` 究竟是哪个函数被“误编译”？是编译器缺陷，还是 Wine 代码里的 UB？
6. 上游是否会接受 macOS inproc 后端（Mach 泵或 os_sync 方案）？12.0 之前会不会出现相关 MR？（本次 GitLab 检索受 Anubis 拦截，未能穷尽。）
7. CX 27 的 Wine 基线，以及它的 ARM64 版是否继续使用 msync。

## 参考来源

1. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/sync.c：CX 26.3 的 inproc 包装、msync 分支、macOS futex（os_sync/ulock）、CW Hack 23015
2. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/msync.c：客户端 msync（tid 槽、注册空转、`WINEMSYNC` 检查、不一致时 exit）
3. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/server/msync.c：服务器泵、QLIMIT 50、`bootstrap_register2`、QoS、`i > 1` 缺陷
4. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/server/inproc_sync.c：`#elif defined(__APPLE__)` 后端、/dev/null 设备、`shm_idx`
5. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/server/thread.c 和 .../server/main.c：设备 fd 下发、`get_inproc_alert_fd`、初始化顺序、internal sync 的使用者
6. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/server/protocol.def：`shm_idx` 应答字段
7. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/server/inproc_sync.c：上游 11.0（仅 ntsync 与 stub）
8. https://raw.githubusercontent.com/wine-mirror/wine/master/server/inproc_sync.c 和 .../dlls/ntdll/unix/sync.c：master（11.18）的机械性变更
9. https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/server/msync.c 和 .../dlls/ntdll/unix/msync.c：CX 25.1.0 msync（tid 槽、POSIX shm）
10. https://raw.githubusercontent.com/MythicApp/wine/mythic-crossover-24.0.7-stable/dlls/ntdll/unix/msync.c：CX 24.0.7 msync（Mach semaphore 池）
11. https://github.com/marzent/winecx/branches/all 以及 `ff-wine-9.19`、`ff-wine-10.0-rc3`、`ff-wine-10.0`、`ff-wine-11.17` 的 msync.c：重写时间窗口与 11.17 移植
12. https://github.com/marzent/wine-msync：原始 msync 仓库（最后推送 2024-08）
13. https://www.codeweavers.com/about/people/mzent/：Marc-Aurel Zent 于 2024 年加入 CodeWeavers（搜索摘要）
14. https://www.codeweavers.com/crossover/changelog：23.7.0 “MSync included”，25.1.0 “Fix for Steam downloads with msync enabled.”，26.0.0 “NTsync for kernels that support it”
15. https://raw.githubusercontent.com/dappermint/winecx-gptk/main/README.md：CX26.3→11.17、gcc 编 PE、发布门禁、SEH 修复、Rosetta 与 entitlement
16. https://raw.githubusercontent.com/dappermint/winecx/wine1117/dlls/ntdll/unix/msync.c 与 https://github.com/dappermint/winecx/commits/wine1117/server/msync.c：msync 修复（补丁作者日期 2026-08-21，GitHub 提交/推送日期 2026-08-25），含空转测量数据
17. https://github.com/dappermint/winecx/commit/3a7a712 、/commit/307f90f 、/commit/8df1826（核查者读取的是对应的 `.patch`，如 https://github.com/dappermint/winecx/commit/307f90f.patch ）：QLIMIT 改为 `MACH_PORT_QLIMIT_MAX` 并钳位、256 次有界空转后以 1 ms 为上限睡眠、`i > 0` 修复的提交说明
18. https://github.com/gauthierpiarrette/highball/issues/119：Battle.net libcef int3（r8，DXMT，sync=none）
19. https://github.com/gauthierpiarrette/highball/issues/149：“Install GOG Blocked”（2026-09-17，Highball 0.9.19，macOS 27.2），登录窗黑屏；正文未提 Qt6WebEngineCore 或 0x292ada（核查更正）
20. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/steam.json：Steam CEF 在 msync/esync 下卡死（Sikarugir Wine 10，2026-08-24）
21. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/battle-net.json：int3 的成因与 DXMT 跨进程开关
22. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/gog-galaxy.json：`Qt6WebEngineCore.dll` 偏移 0x292ada 处的访问违例
23. https://github.com/gauthierpiarrette/highball-engine（README、inputs.json、patches/README.md、0005 补丁）：CX 26.3 原样构建、gcc、tarball 的 sha256
24. https://raw.githubusercontent.com/gauthierpiarrette/highball/main/spike/engines/x64-crossover26.3-r8.json 和 -r12.json：引擎清单
25. https://github.com/gauthierpiarrette/highball/issues/5：Wine 11 引擎进展与 `macdrv_functions` 导出
26. https://newreleases.io/project/github/frankea/Whisky/release/v4.0.0-beta.2：llvm-mingw 编出的 `kernelbase.dll` 导致 Steam 登录卡死
27. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/loader.c：CEF/Qt5 二进制补丁表（`%gs:0x8` 与 cmd_line_args）
28. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/loader.c：`hacks_init`、`msync_init` 调用顺序、24067 的 cxcompatdb.so dlopen
29. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/signal_x86_64.c：macOS `%gs` 只镜像 Self、TLS 指针和 PEB
30. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.17/dlls/kernelbase/kernelbase.spec 与 CX 26.3、wine-11.0 同名文件：`SetThreadpoolTimerEx`
31. https://bugs.winehq.org/show_bug.cgi?id=57980：`SetThreadpoolTimerEx`（提交 8fc5b439ff6a，2026-09-18 关闭）
32. https://bugs.winehq.org/show_bug.cgi?id=60263：winemac 跨进程子窗口 Metal swapchain（UNCONFIRMED）
33. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md：11.0 发布说明（macOS 在 syscall dispatcher 中交换 `%gs`）
34. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.17/ANNOUNCE.md：11.17（显示模式模拟、macOS 主线程改为 system thread、winemac API 清理）
35. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.18/ANNOUNCE.md：11.18（修复 #60327、#60331、#60337、#57980）
36. https://dl.winehq.org/wine/source/11.0/：rc1–rc5 与 11.0 的日期
37. https://dl.winehq.org/wine/source/10.0/：只有 rc1–rc6 和 10.0，没有 10.0.x 维护版
38. https://www.winehq.org/news/2025120501：Wine 11.0-rc1（2025-12-05）
39. https://www.gamingonlinux.com/2024/11/windows-to-linux-compatibility-layer-wine-10-0-planned-for-mid-january-2025/：10.0 日程（Julliard 邮件转述）
40. https://forum.winehq.org/viewtopic.php?p=151318：关于 11.0.1 的讨论（403，来自搜索摘要：稳定分支无维护者）
41. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears：CX 27 预览版，计划 2027 年初发布
42. https://www.codeweavers.com/blog/mjohnson/2026/6/11/whats-in-and-whats-out-for-crossover-27（403）与 https://appleinsider.com/articles/26/06/11/crossover-a-windows-to-mac-gaming-tool-goes-apple-silicon-only：CX 27 的范围
43. https://github.com/Alien4042x/Wine-NTsync-Userspace-macOS-backend：WFUSync README、v5 补丁（协议与 os_sync SHARED）、提交记录
44. Apple SDK 头文件 `MacOSX26.5.sdk/usr/include/os/os_sync_wait_on_address.h`（本机 Command Line Tools），对应文档 https://developer.apple.com/documentation/os/os_sync_wait_on_address
45. `MacOSX26.5.sdk/usr/include/mach/port.h`（`MACH_PORT_QLIMIT_MAX = 1024`）与 `servers/bootstrap.h`（`bootstrap_register` 自 10.5 弃用，头文件中没有 `bootstrap_register2`）
46. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.18/server/mach.c：上游使用 `bootstrap_register2`
47. https://github.com/wine-mirror/wine/commit/1a63b0d7c431：marzent 的 CALayerHost 跨进程 `MetalViewSwapChain`（仅根窗口）
48. https://docs.kernel.org/userspace-api/ntsync.html：ntsync 语义（WaitAll 原子性、PulseEvent、放弃互斥锁）
49. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.18/dlls/ntdll/tests/sync.c：一致性测试用例
50. https://raw.githubusercontent.com/wine-staging/wine-staging/master/staging/patchinstall.py 与 patches/*/definition：staging 的补丁队列结构
51. https://github.com/ValveSoftware/Proton/issues/7605：Proton/Wine 提交中使用 `CW-Bug-Id` 尾注（搜索摘要）
52. https://github.com/git-series/git-series：补丁系列版本管理工具
53. https://github.com/stacked-git/stgit：Stacked Git
54. https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/README.esync：esync 的 fd 耗尽等限制
55. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26：CX 26 高级设置中的 MSync 说明
56. https://dl.winehq.org/wine/source/8.0/：存在 wine-8.0.1、wine-8.0.2（核查新增）
57. https://dl.winehq.org/wine/source/9.0/：存在 wine-9.0.1（核查新增）
58. https://github.com/wine-mirror/wine/commits/stable 与 https://raw.githubusercontent.com/wine-mirror/wine/oldstable/VERSION：stable 分支 2026-01-13 后无提交，oldstable 的 VERSION 仍为 10.0（核查新增）
59. https://dl.winehq.org/wine/source/11.x/：wine-11.18.tar.xz 时间戳 2026-09-18 22:55（核查新增）

## 事实核查记录

独立事实核查于 2026-09-26/27 完成，共 11 条。结论为 confirmed 的条目，正文只补充了核查者给出的细节；partially_true 的条目已在正文中标“核查更正”并改写。本节的更正优先于正文。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| CX 26.3 中 msync 是 Wine 11 inproc_sync 的 macOS 后端：`server/inproc_sync.c` 有 `#elif defined(__APPLE__)` 分支，设置 `WINEMSYNC` 时打开 `/dev/null`；`protocol.def` 新增 `shm_idx`；模式在 wineserver 启动时固定；不一致的客户端 `exit(1)`（§摘要、§2） | 属实 | 核查者读取原文件：CX 版 `inproc_sync.c` 共 499 行，上游 11.0 为 307 行；第 244 行 `#elif defined(__APPLE__)`；`protocol.def` 第 4174 行 `shm_idx`。上游 wine-11.18 和 master 的 `inproc_sync.c` 都没有 `__APPLE__` 分支 [1][2][4][6][7] |
| msync 演进：24.0.7 为 Mach semaphore 池（1024）；25.1.0（2025-08-12）改为 64 MB tid 槽 + `__ulock_wait2(UL_COMPARE_AND_WAIT_SHARED)`，不再用 semaphore；26.3 改用 `mach_make_memory_entry_64`（§摘要、§2） | 属实（有补充） | 补充两点：① 25.1 和 26.3 中 `__ulock_wait2` 为 `weak_import`，缺失时回退 `__ulock_wait`（微秒超时），准确说法是“始终用 ulock”；② 25.1 源码来自第三方镜像 PhoenicisOrg，未与官方 tarball 比对。正文已补充 [2][9][10][14] |
| CX 26.3 服务器泵的 `if (i > 1)` 差一错误与 `mpl_qlimit = 50`；dappermint 于 2026-08-25 以 8df1826、3a7a712（→`MACH_PORT_QLIMIT_MAX` = 1024）修复，307f90f 把无界空转改为睡眠（§摘要、§3、建议 3） | 属实（日期与细节更正） | 代码事实无误。**更正**：补丁作者日期为 2026-08-21，2026-08-25 是 GitHub 上的提交/推送日期；307f90f 不是直接睡眠，而是最多 256 次带 pause/yield 提示的有界空转，之后把槽写成 3，并以 1 ms 为上限睡眠；3a7a712 还对 `WINEMSYNC_QLIMIT` 做钳位 [3][16][17][45] |
| Highball `x64-crossover26.3` 引擎用未变基的 `crossover-sources-26.3.0.tar.gz` + mingw-w64 gcc 构建；Battle.net #119 在 `sync=none`+DXMT 下 int3，加 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` 后消失；GOG #149 为 `Qt6WebEngineCore.dll` 0x292ada 访问违例，r9/r10 相同，与开关和 `--disable-gpu` 无关（§摘要、§0、§1） | 属实（有保留） | **更正/保留**：① 0x292ada 细节在 highball-db 的 gog-galaxy recipe 中，不在 #149 正文；#149 正文是 Highball 0.9.19、macOS 27.2 上的登录窗黑屏；② 引擎带 Highball 自己的补丁（0007、0008/0009 等），变基和 msync 已排除，Highball 补丁没有排除；③ 是否就是 08 号报告所说的 “DIY 回归”，未独立核实。正文 §摘要、§0、§1、建议 4 已改 [19][22][23][24] |
| wine-11.0 和 CX 26.3 的 `kernelbase.spec` 中 `SetThreadpoolTimerEx` 是注释掉的 stub；wine-11.17 导出 `ntdll.TpSetTimerEx`；wine-11.18（2026-09-18）修复 #60327、#60331、#60337（§摘要、§0） | 属实 | wine-11.16 也仍是 stub；11.18 ANNOUNCE 同时列出 #57980；dl.winehq.org 上 wine-11.18.tar.xz 时间戳 2026-09-18 22:55 [30][35][59] |
| Wine 11.0 rc1–rc5 为 2025-12-05、12-12、12-19、12-26、2026-01-09，正式版 2026-01-13；10.0 目录无 10.0.x（§5） | 属实 | 同时指出：rc4→rc5 间隔 14 天；10.0 用了 6 个 rc；oldstable 的 VERSION 仍是 10.0，stable 分支 2026-01-13 后无提交 [36][37][58] |
| “上游 x.0 版本没有维护版”，所以稳定线等于 Cider 自己负责全部回移植（§摘要、§0、§5） | 部分属实 | **更正**：只对 10.0 和 11.0（截至 2026-09-26）成立；8.0.1、8.0.2、9.0.1 都存在。改为“9.0.1 之后再没出过稳定维护版”。“不指望 11.0.x、放弃 11.0-stable 线”的结论不变 [56][57][58] |
| 11.0 的 rc “每周一个”，据此推算 12.0 ≈ 2027-01-12 至 01-19（§摘要、§5） | 部分属实 | **更正**：rc1–rc4 是每周一个，但 rc4→rc5 跨新年隔了 14 天，10.0 有 6 个 rc。12.0 正式版应表述为“2027 年 1 月中下旬，±1–2 周，属推断”；没有找到官方日程。建议 7 已加排期余量 [36][37] |
| marzent `ff-wine-11.17` 的客户端 msync.c 与 CX 26.3 逐字相同；`ff-wine-10.0-rc3` 仍用 `semaphore_create`，`ff-wine-10.0` 改用 `shm_tid_map`（§2） | 属实 | 1162 行 diff 为空；rc3 有 3 处 `semaphore_create`、0 处 `shm_tid_map`，10.0 为 0 处和 4 处。分支日期（2024-12-22、2025-12-10）**未复验**，正文已标 [中] [11] |
| 上游 11.0→11.18 的 `inproc_sync.c`、`ntdll/unix/sync.c` 改动是机械性的；上游 futex 走 `os_sync_wait_on_address`（`OS_SYNC_WAIT_ON_ADDRESS_NONE`）并保留 `__ulock` 回退；上游 `server/mach.c` 用 `bootstrap_register2`；os_sync 需要 macOS 14.4+（§2、§4） | 属实（有补充） | 补充：`inproc_sync.c` 还去掉了 `WIN32_NO_STATUS`（307→290 行），`sync.c` 还有 `make_client_id` 的调整；上游 `bootstrap_register2` 的 flags 是 `uint64_t`，CX msync 声明为 `int`，属轻微 ABI 不一致，移植时统一。正文 §2、§4、建议 3 已加 [1][3][7][8][44][46] |
| CX 26.3 msync 的语义缺口在代码注释中自认：PulseEvent 不精确、WaitAll 非原子、放弃互斥锁为 HACK；ntdll 的 `linux_*_obj` 在 `__APPLE__` 下转调 `msync_*`（§2） | 属实 | 注释原文分别位于 `msync_pulse_event_obj`、第 951 行附近的 WaitAll 说明和第 1106 行；`sync.c` 第 454 行 `#elif defined(__APPLE__)`，`do_msync()` 分支位于第 689、734、950 行 [1][2] |
