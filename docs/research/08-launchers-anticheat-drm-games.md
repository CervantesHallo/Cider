# 游戏生态调研：Steam 与各启动器、反作弊、DRM、逐游戏修复机制、热门游戏兼容现状

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 本次直接读取了一手来源（官方 changelog 或支持页、源码文件、GitHub README、issue、recipe JSON）；**[中]** = 来自可信第三方媒体、社区数据库，或搜索摘要转述的官方博客（codeweavers.com 博客与兼容库页面、winehq.org、PCGamingWiki 对自动抓取返回 403，只能通过转述获取）；**[低]** = 推断、单一社区报告，或无法核实。文中说“推断”的地方都是作者的工程判断，不是来源里的原话。本次没有克隆仓库，也没有下载二进制。2026-09-26 已按独立事实核查结果修订，见文末“事实核查记录”。

---

## 摘要

- **Steam（Windows 版，跑在 Wine 里）仍是最大的入口，也最容易被上游更新打坏。**
  - Valve 2025-12-19 的客户端更新说明：Steam 客户端在 Windows 11 和 64 位 Windows 10 上已改为 64 位；32 位客户端只在 32 位 Windows 上继续更新到 2026-01-01 [76][77][4][5]。因此 Cider 的 Steam bottle 必须是 64 位前缀（win64）；新 WoW64 或旧式 WoW64 都能拿到 64 位客户端，真正要避免的是纯 32 位前缀（推断）。过去常用的 `-cef-force-32bit` 之类参数应视为失效（推断）。
  - steamwebhelper（CEF/Chromium）是主要故障点，具体包括：GPU 进程通过 ANGLE→D3D11 画到浏览器进程的子窗口；sandbox；同步原语（msync/esync 会让 CEF 卡死）；winemac 的 hosted-layer 缺陷 [6][8]。
  - Steam 客户端自己也会回归。例如 2026-09-03 的客户端更新（buildid 1788400362）后，Linux 原生客户端启动 15–20 秒即因 `BMainLoop stalled` 崩溃 [7]。Highball 在 Mac/Wine 上记录的是另一种模式（首次自更新后约 3 分钟退出，登录一次后消失）[6]，两者是否同源未经证实。
- **“原生 macOS Steam + 兼容工具 + steamclient 桥”是 2026 年出现的新路线**，已有三个开源实现：macos-steam（AGPL-3.0，v0.1.0 于 2026-08-26 打 tag，v0.5.0 于 2026-09-05；只有 tag，没有 Release 二进制）、ullage、kaon [18][83][19][20]。
  - 做法是翻转 macOS Steam 客户端里一个休眠的 `m_bCompatEnabled` 开关，或者改写 `appinfo.vdf`。然后用 Proton 的 lsteamclient 思路，把 Windows 游戏的 Steamworks 调用桥接到 macOS 原生 `steamclient.dylib`。
  - 这条路线能绕开“CEF 跑在 Wine 里”的整类问题，但依赖未公开的 Valve 内部行为，很脆弱。
- **启动器（2025–2026 年）在 CrossOver 上大多能用，但几乎每个版本都要修。**
  - CrossOver 25.0（2025-03-11）起官方支持 Epic 和 GOG Galaxy；25.0.1 和 25.1 修 EA app 与 Ubisoft；26.0（2026-02-10）基于 Wine 11.0，带 D3DMetal 3.0、DXMT v0.72、vkd3d 1.18；26.1（2026-04-09）修 Battle.net 安装；26.2（2026-06-09）修 Helldivers 2 并增加 32 位 bottle 警告；26.3（2026-07-21）修 Epic 下载、GOG Galaxy 和 Diablo IV。截至 2026-09-26，26.3.0 仍是最新版 [1][85][86]。
  - 开源侧 Highball 的 recipe 显示：在 Highball 自己的 Sikarugir Wine 10 引擎上，Epic 官方启动器的安装流程被 ACL 审计（DP-07）卡死；GOG Galaxy 在 Wine 10 上黑屏；在 Highball 用 CX 26.3 源码自行打包的 Wine 11 引擎（`x64-crossover26.3-r8`～`r10`）上，GOG Galaxy 崩溃、Battle.net 登录区黑屏；Ubisoft Connect 需要 Wine 11 + DXMT [22]–[27]。这些是单个 DIY 引擎的结果，不是 CrossOver 产品的结果，CrossOver 26.x 对这几个启动器都有官方支持和修复 [1]。
  - 建议 Epic、GOG、Amazon 默认走 Legendary、gogdl、nile 这类开源客户端 [28][29]。这是产品取舍，不是因为官方启动器不可能跑通；官方 Epic 启动器应作为兼容选项保留（见 P1 第 6、7 项）。
- **反作弊的硬边界**：
  - EAC 和 BattlEye 的 Wine/Proton 支持需要开发者逐个游戏主动开启，而且只提供 Linux 原生模块（`easyanticheat_x64.so` / Proton BattlEye Runtime），对 macOS 上的 Wine 无效 [34][35][36][74][75]。EAC 虽有原生 macOS 模块，但只服务原生 Mac 版游戏，不服务 Mac 上的 Wine。
  - Vanguard（Valorant）、Ricochet（CoD）、Javelin（Battlefield 6，要求 Secure Boot）、ACE 这类内核级反作弊，在任何 Wine 上都不可行 [37][38]。
  - 例外属于“偶然能用”：CrossOver 26 上 Helldivers 2（nProtect GameGuard）可以联机，但会随游戏补丁反复坏掉 [1][41][42]。
  - CodeWeavers 官方立场是不支持、不尝试修复、也不绕过反作弊（支持页原文未点名 EAC 或 BattlEye）[33]；2026-08-31 的博文另说 EAC/BattlEye 的 Linux 选项帮不了 CrossOver Mac 用户 [34]。
- **DRM**：
  - Denuvo 自 CrossOver 23.5（2023-09）起可在 Apple Silicon 上运行 [44]。
  - GamingOnLinux 2025-05-15 报道：切换 Proton 版本、修改 `WINE_CPU_TOPOLOGY` 可能被 Denuvo 当成“新机器”，24 小时内 5 台就锁 24 小时 [43]。这是作者的观察，没有引用 Denuvo/Irdeto 的官方说法。2026-04 Pragmata 首发时仍有 Linux/Steam Deck 玩家被锁 [46][80]。
  - 2026-08-27 Irdeto（Denuvo 产品经理）的官方博客承认不同 Proton 版本“could be interpreted differently”，并称 “Denuvo has since addressed this behavior” [79]。博客没给修复日期，没说剩余限制，也没提 CrossOver、macOS、Rosetta 或 FEX。因此该行为在 Proton 上可能已缓解，在 CrossOver/Cider 上仍未知。[中]
  - Cider 仍应把影响指纹的设置按游戏锁定（稳妥做法，而非已证实的必要条件）。部分 Denuvo 或新游戏还需要 `ROSETTA_ADVERTISE_AVX=1` [42][45]。
  - SafeDisc、SecuROM、StarForce 依赖内核驱动，基本无解 [47][48]。
- **逐游戏修复机制**：业界已有成熟的“数据 + 少量引擎 hack”两层结构。
  - 数据层：Proton 的 appid 列表加 `hack_append_command_line`、umu-protonfixes（Python，BSD-2）加 umu-database（CSV，GPL-3）、Lutris YAML、CrossTie XML、CrossOver 25 起的私有 per-game 数据库，以及 Highball 的 JSON recipe（CC0）[1][13][14][50]–[54][21]。
  - 引擎层：CrossOver Wine 里按 exe 名写死的 `CW HACK`，例如 Epic 的 winstation、Ubisoft 的 swiftshader ICD、对 `libcef.dll` 与 `Qt5WebEngineCore.dll` 的加载期二进制补丁 [10][11]。
- **热门游戏兼容现状（2026-09）**：
  - 原生 macOS 版：Cyberpunk 2077（2025-07-17，要求 16GB 以上内存）、BG3、Wuthering Waves（Mac App Store，2025-03-27）[59][65]。
  - CrossOver 可玩：Black Myth: Wukong、Hogwarts Legacy、Starfield、Diablo IV、Helldivers 2，以及 RDR2（Vulkan→MoltenVK）。
  - 部分可玩：Elden Ring（只能离线）、GTA V Enhanced（只有故事模式，D3DMetal 路径据报告需要 M3 或更新的芯片）[55]–[58]。
  - 开发机是 **8GB 内存**，RDR2 这类 3A 游戏会直接因内存不足崩溃 [57]。

---

## 详细调研

### 1. Windows Steam 在 macOS/Wine 下的运行

#### 1.1 steamwebhelper（CEF）的故障面与解决手段

| 故障面 | 现象 | 已知对策 | 来源与置信度 |
|---|---|---|---|
| CEF sandbox | 旧版客户端启动 steamwebhelper 时崩溃或白屏 | CrossOver 早期 hack：在 `kernel32/process.c` 的 `create_process_impl()` 中检测到 `steamwebhelper.exe` 时追加 ` --no-sandbox`；环境变量 `STEAM_DISABLE_CEF_SANDBOX=1` / `CEF_DISABLE_SANDBOX=1`；启动参数 `-no-cef-sandbox` | [9][8] 高/中 |
| GPU 进程 | UI 黑屏、白屏或空白菜单 | `-cef-disable-gpu`、`-cef-in-process-gpu`、`-cef-disable-gpu-compositing`（部分环境下会反而全黑）；在 Steam 设置里关闭 “GPU accelerated rendering in web views” | [17][6] 中/高 |
| 跨进程 swapchain | CEF 的 GPU 进程需要把画面画进浏览器进程的窗口 | DXMT：`DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1`（Highball 在 Battle.net、EA、Ubisoft、Rockstar 的 recipe 里都开启了）| [23]–[26] 高 |
| winemac hosted-layer | Steam 菜单是空白的 2×1px 窗口；Retina 下 UI 按 2× 绘制、鼠标错位 | `dlls/winemac.drv/cocoa_window.m` 的 `CAContextSwapChain setContainer:frame:` 没有在无 runloop 的线程上 flush `CATransaction`，也没有做 points/pixels 换算。修法是改 3 行（`cgrect_mac_from_win()` 加显式 flush）| [8] 高（2026-09-19 的 issue） |
| 同步原语 | 开启 msync/esync 后登录窗口不出现，webhelper.txt 显示 “killing unresponsive browser” | Steam UI 以 `WINEMSYNC=0 WINEESYNC=0` 启动。游戏用 `-applaunch` 静默会话时再开 msync（Highball 称游戏帧率提升约 40%，属自报数据）。wineserver 的同步模式在启动时固定，切换时必须重启 wineserver | [6] 高（recipe 原文），性能数字为 [低] |
| 客户端自更新 | 32 位 bootstrapper 在 WoW64 + Rosetta 下要 15–25 分钟，偶尔报 “nested exception on signal stack”；下载卡在约 215/235MB | 重启后会续传 | [6] 高 |
| 上游回归（Linux） | steam-for-linux #13576（2026-09-05 开）：2026-09-03 更新（buildid 1788400362）后，**Linux 原生客户端**启动约 15–20 秒报 `CSteamEngine::BMainLoop appears to have stalled > 15 seconds without event signalled` 并崩溃；删除 Steam 配置目录也无效（报告者环境 PikaOS + RX 9070） | issue 中未见可用对策 | [7] 高 |
| 自更新后退出（Mac/Wine） | Highball recipe：首次自更新后约 3 分钟、登录前，客户端因 “fatal stalled cross-thread pipe” 退出（recipe 最后验证 2026-08-24，Sikarugir Wine 10 引擎） | 再按一次 Play；登录过一次的前缀就稳定了 | [6] 高。与上一行是否同源未证实 [低] |
| steamservice | “Steam Client Service failed to start (GLE 126)” | 对大多数游戏无害，只影响少数驱动或反作弊的安装 | [6] 高 |
| 与原生 Steam 冲突 | 登录后立刻显示 NO CONNECTION | 退出 macOS 原生 Steam（两个客户端会争抢同一会话）| [6] 高 |

- **CrossOver 的做法**：
  - 25.0 的 changelog 称 Steam 启动明显加快，并且支持当时最新的 Steam 更新；25.1.0 修复了“开启 msync 时 Steam 下载失败”和“Steam 常规连接问题” [1][3]。[高]
  - 在 CX 26.3 源码镜像的 `dlls/kernelbase/process.c`、`dlls/ntdll/loader.c` 里没有找到 steamwebhelper 专用分支，旧的 `--no-sandbox` hack 看起来已被移除或改写（与本系列 02 号报告的结论一致）[10][11]。[中]
  - 但 26.3 的 `dlls/ntdll/loader.c` 的 `build_module()` 会在加载 `libcef.dll` 或 `Qt5WebEngineCore.dll` 时调用 `apply_binary_patches()`（位于 `#if defined(__i386__) || defined(__x86_64__)` 内），加载 `cohtml_Unity3DPlugin.dll` 时调用 `apply_fuzzy_binary_patches()`（仅 `__x86_64__`，带通配字节模式的补丁）；同文件还有 CW HACK 22434（`unix_pe_module_loaded`）[11]。[高：代码已由独立核查复读确认。注意 dappermint/winecx 是第三方镜像，不是 CodeWeavers 官方仓库]
  - 补丁表的具体内容没能读全（核查也未验证），推测和 Rosetta 或 Wine 下 Chromium 的 syscall、sandbox 行为有关。[低]
  - 这说明 CodeWeavers 对嵌入式浏览器做的是加载时字节补丁，而不仅仅是改参数。
- **Steam 64 位化**：
  - Valve 2025-12-19 的 Steam 客户端更新说明（GamingOnLinux 引用原文）：“The Steam client is now 64-bit on Windows 11 and Windows 10 64-bit”；32 位 Windows 上的 32 位客户端继续更新到 2026-01-01 [76][77]。Valve 支持 FAQ 称，32 位 Windows 10 上的现有安装仍能用，但之后不再有任何更新（搜索摘要）[78]。多家媒体报道一致 [4][5]。[高]
  - Highball 2026-08 的 recipe 显示，首次启动会下载约 235MB 的 64 位客户端 [6]。[高]
  - 对 Cider 的影响：
    1. Steam bottle 必须是 64 位（win64）前缀。新 WoW64 或旧式 WoW64 前缀都会拿到 64 位客户端；Steam 的硬性要求只是不能用纯 32 位前缀。“必须是新 WoW64”是 Cider 的架构选择，不是 Steam 的要求（推断）；
    2. CrossOver 26.2.0（2026-06-09）的 changelog 写有 “Added additional warnings for 32-bit bottles” [1]；CX 27 不再运行 32 位 bottle，同时只支持 Apple Silicon、要求 Sonoma 及以上，这是 CodeWeavers 博客 “What's in and what's out for CrossOver 27”（2026-06-11）宣布的 [82]；
    3. 2024 年 Whisky 或 CrossOver 用户常用的 `-allosarches -cef-force-32bit` 权宜之计 [16] 在纯 64 位客户端上应已无意义（推断）[低]。
  - 2024-11 的 Steam “游戏录制”更新曾让基于 Wine 7.7 的 Whisky/GPTK 1.x 直接失效（`chrome_elf.dll failed to initialize`）[15]，说明 CEF 升级会淘汰旧 Wine 基线。
- **Steamworks DRM（SteamStub）与 Overlay**：
  - 在 bottle 方案下，SteamStub 包壳的游戏需要同一前缀里有正在运行的 Windows Steam。这点属于常识，本次未单独验证。[中]
  - Overlay（`GameOverlayRenderer64.dll`）在 Mac + D3DMetal 下不稳定，Highball 建议关闭 [6]。
  - macos-steam 的做法是：对 DRM 包壳的游戏改用 “Valve's own signed client DLL” 启动，overlay 调用原生 macOS Steam 的 Metal hook 安装器（v0.5.0 的 commit 记录），也就是用原生 overlay 替代 Wine 内的 D3D overlay [18]。[高：commit 标题；实现细节未读]

#### 1.2 新路线：原生 macOS Steam 桥接

| 项目 | 机制 | 状态（日期） | 说明 |
|---|---|---|---|
| **macos-steam** (Superd22) [18][83] | 用注入的 dylib 翻转 macOS Steam 里已锁定（latched）的休眠开关 `m_bCompatEnabled`，注册为兼容工具（`toolmanifest.vdf` / `compatibilitytool.vdf`）。替换 CrossOver bottle 内的 `steamclient64.dll` 和 `steamclient.dll`，新 DLL 把每个调用 “across the Wine unix seam” 封送给一个原生 `.so`，由它承载 Valve 真正的 macOS `steamclient.dylib`。由一个未签名强化的 `.app` 把注入库放进 `DYLD_INSERT_LIBRARIES` 再启动 `steam_osx` | “working beta”；tag 从 v0.1.0（2026-08-26）到 v0.5.0（2026-09-05），最新 commit 为 2026-09-05 的 “chore(release): 0.5.0”，共 85 个 commit，只有 tag、没有 GitHub Release 二进制；已测试 CrossOver 25.1.1 与 26.2；Apple Silicon + macOS 14 及以上，需要已登录的原生 Steam；AGPL-3.0 | 项目约一个月大，整套机制依赖一个未公开的 Steam 内部开关，很脆弱；还没有覆盖全部 Steam API（issue #45：好友、服务器浏览器、创意工坊）；启用 overlay 时反作弊大概率失败 |
| **ullage** [19] | 改写 `appinfo.vdf` 中 Windows 启动项，指向仓库外的小 launcher；用 lsteamclient.dll 加 .so 与原生会话握手；通过监听 content log 的 Terminating 事件实现 Stop | 2026-08-27/28 仍在密集提交，处于实验阶段 | 明确声明不做 Wine 分支，也不做运行时发行 |
| **kaon** (natbro) [20] | 在 macOS Steam 的 `steam_dev.cfg` 中写入 `@sSteamCmdForcePlatformType windows`，让原生客户端下载 Windows depot；改 `libraryfolders.vdf` 与 CrossOver 中的 Windows Steam 共享库 | Apache-2.0；lsteamclient 的 macOS 版“未完成” | 两个客户端需要同时运行 |

- **steamcmd**：macOS 原生 steamcmd 用 `+@sSteamCmdForcePlatformType windows` 可以直接下载 Windows depot [72][20]。适合做 Cider 的无头下载器，也适合 CI 拉取测试游戏。[中]

### 2. 其他启动器与商店（2025–2026）

| 启动器 | CrossOver 官方状态 | 开源侧实测（Highball recipe，2026-08/09） | 关键技巧 | 建议 |
|---|---|---|---|---|
| Epic Games Launcher | CX 25.0（2025-03-11）起官方支持；26.3.0（2026-07-21）修复 “Epic Games Launcher downloads not working on Mac after update” [1]；源码 `CW Hack 24938` 把 `EpicGamesLauncher.exe` 移到 `winsta0\Default` [10] | 以下均为 Highball 自己的引擎（recipe 最后验证 2026-08-25，`x64-sikarugir10.0_6-r0`，macOS 14.6，M1 Pro），**不是 CrossOver**：UI 在 DXMT 下全黑，改用 DXVK 后正常；`-SkipBuildPatchPrereq` 可避免前置依赖死循环；自更新后需要手动重开；**安装任何游戏都报 DP-07**（“You do not have permission to install to...”，启动器审计 Wine 不持久化的 Windows ACL），被标为 blocked [22]。DP-07 是这个引擎的 ACL 持久化缺口，不是普遍的墙：CrossOver 上官方启动器可用 [1] | MSI 静默安装 | 游戏库默认走 **Legendary**（0.21.1，2026-09-08；0.21.0 支持 ChunksV5 加密 manifest、Ubisoft 通过 uplay 协议启动，0.21.1 恢复 “EpicGamesLauncher.exe” wrapper 支持）[28]；官方启动器作为兼容选项保留，并补 ACL 持久化 |
| GOG Galaxy | CX 25.0 起支持；26.3.0 修复 “GOG Galaxy client not loading after update” [1] | Galaxy 2.1.6.29 离线安装包加 `mfc140`（2026-08-25 在 `sikarugir-10.0_6` 上验证安装）；该 Wine 10 引擎下登录窗黑屏；在 Highball 用 CX 26.3 源码自行打包的 `x64-crossover26.3-r9`（2026-09-16）与 `r10`（2026-09-18）上，显示登录表单约 2 秒后因 `Qt6WebEngineCore.dll` 异常崩溃（highball#149）→ blocked [27]。这些不是 CrossOver 26.3 产品的结果 | `/runWithoutUpdating /deelevated` | 走 **gogdl** 或离线安装包；Heroic 2.22.x 持续维护（2026-09-16）[29] |
| EA app | 25.0.1 “Fix for latest EA App update”，25.1.0 “Fix for EA app issue” [1] | Burn bootstrapper 等待一个永远不显示的窗口；Wine 10 下 MSI 返回 1627/1603（服务加 .NET 自定义动作失败），需要 Wine 11（CX 26.3）；**NTFS junction 被 Wine 存成空的 “EA Desktop?” 桩** → 手动 `ln -s` [24] | `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1` | 引擎层需要补 junction/reparse 语义 |
| Battle.net | 26.1.0（2026-04-09）“Fix for Battle.net not installing for some users” [1]；26.3.0 修 Diablo IV 更新后无法启动 [1] | Highball 的 Wine 10 引擎正常（最后验证 2026-09-18，`x64-sikarugir10.0_6-r5`）；在 Highball 用 CX 26.3 源码自行打包的 `x64-crossover26.3-r8`～`r10`（DXMT，Highball 0.9.12）上，登录窗保持黑屏，并有 libcef 触发的 int3 崩溃（highball#119）[23]。这说明“DIY 的 Wine 11 + DXMT”会在 CEF 启动器上回归，**不能**说明 Wine 11 基线或 CrossOver 26.3 产品本身有问题 | `--lang=enUS --installpath=...`，sync=none | 纳入回归测试集 |
| Ubisoft Connect | 25.1.0 “Fix for Ubisoft issue” [1]；源码 `CW HACK 19252`：当 `upc.exe`/`UplayWebCore.exe` 设置 `VK_ICD_FILENAMES` 时自动生成 `vk_swiftshader_icd.json` [10] | 2025 年以后的客户端 CEF GPU 初始化失败，软件回退在 Apple Silicon 上崩溃；Wine 11 + DXMT 可以渲染登录窗（2026-09-05）[25] | 启动时设 `WINE_SIMULATE_WRITECOPY=1`；关闭 overlay | 同上 |
| Rockstar Games Launcher | 24.0.5 修复最新 RGL 更新 [1] | 注册表 `ServicesPipeTimeout=60000`；IFEO `Rockstar-Games-Launcher.exe\GlobalFlag=16`；时区不对会报 `#1.000.7`；安装卡在 “Preparing to Install”（Sikarugir#258）[26] | Epic 版 RDR2 需要一个名为 `EpicGamesLauncher.exe` 的替身进程才会被判定有授权 [57] | Legendary 或替身进程 |
| Riot Client | 无 | — | LoL 有原生 macOS 客户端，2025-01-23 的 25.S1.2 版本在 Mac 上启用嵌入式 Vanguard（mVG）[32]；Valorant 使用内核 Vanguard | 不支持 Valorant |
| Xbox app / MS Store / Game Pass | 不可用 | — | GDK 运行时、Xbox 身份认证、MSIXVC 加密三重壁垒；开源 **Xodus**（2026-08）已实现登录、下载、授权和解密，运行层仍未解决，宣称面向 Linux 和 macOS [31] | 观望 |
| HoYoPlay | 24.0.4 “New HoYoPlay launcher now works” [1] | — | 游戏受 mhyprot 影响 | 按游戏标注 |

- **共性结论**：启动器故障几乎都出在嵌入式浏览器上（CEF、QtWebEngine、WebView2），集中在 GPU 进程、跨进程呈现、sandbox 和同步原语这几处。其次是服务、ACL 和 junction 这类 NT 语义缺失。[高：来自 7 份 recipe 的归纳]。注意这些 recipe 都是在 Highball 自有引擎（Sikarugir Wine 10，或自行打包的 CX 26.3 源码）上测的，“blocked” 不等于 CrossOver 产品上也不可用。
- **Whisky 已停止维护**：2025-04 宣布结束，仓库于 2025-05-11 归档 [30][15]。社区有 frankea/Whisky 等分支，以及用 CX 26.3 差异构建的 winecx-gptk [70]。

### 3. 反作弊：能做什么，不能做什么

CodeWeavers 在 2026-08-31 的博文中总结：Wine 完全运行在用户态，无法影响内核；极少数内核级反作弊恰好能在某些 Wine 版本上工作，是 “very happy accident”；EAC 和 BattlEye 的 Linux 选项既是 Linux 专属，又取决于游戏开发者，帮不了用 CrossOver 的 Mac 玩家 [34]。官方支持页说明他们 “do not support or attempt to fix issues with anti-cheat technology”，也 “cannot legally work around” 反作弊，只有游戏开发商修改才可能让游戏可用；但支持页本身没有点名 EAC 或 BattlEye [33]。[中：博文 403，经搜索摘要转述；支持页 高]

| 反作弊 | 机制 | Proton/Linux | macOS + Wine | 代表游戏 |
|---|---|---|---|---|
| **EAC** | 开发者在 EAC 后台的 SDK Configuration 里启用 Linux 客户端平台，在 Client Module Releases 下激活 Unix 模块，把 `Client/Assets/Plugins/x86_64/libeasyanticheat.so` 改名为 `easyanticheat_x64.so`，随 depot 放在 `EasyAntiCheat_x64.dll` 旁；Wine/Proton 玩家使用这个 Linux 模块，纯用户态运行 [74]。自 2022-01 起 Proton 支持 EAC “without requiring any recompilation”，不需要更新 SDK [75]；“SDK ≥ 1.14” 只是 2021-09 最初 EOS 路径的要求，Valve 现行文档没有 SDK 下限 [35][74] | 由开发者选择是否开启；AWACY 显示约 66% 的 EAC 游戏可玩（第三方统计）[37] | **不可用**（没有 Mach-O 模块）。部分游戏可以离线玩：Elden Ring 离线可玩，联网报错 [55] | Fortnite（Denied）、Apex（2024-10-31 起关闭 Linux 支持）、Rust（Denied）[37] |
| **BattlEye** | 开发者按游戏发邮件给 Valve 或 BattlEye 的联系人开启，“No additional work is required”；玩家通过独立的 Proton BattlEye Runtime 获得支持（2021-11）[36][74] | 需要开发者开启 | **不可用**；GTA Online 加入 BattlEye 后，CrossOver 上只能玩故事模式（需在 RGL 中关闭 BattlEye）[64][58] | PUBG（Broken）、R6 Siege、Destiny 2（Denied）[37] |
| **Vanguard** | 内核驱动；LoL 在 macOS 上是嵌入式 mVG [32] | Denied | 不可用 | Valorant |
| **Ricochet / Javelin** | 内核级；Javelin 要求 Secure Boot [38] | Denied；EA 2026-03 的招聘启事提到未来可能支持 Linux/Proton [39] | 不可用 | CoD 系列、Battlefield 6、EA FC |
| **ACE（腾讯）** | 内核级 | 基本不可用 | 不可用（推断）[低] | 三角洲行动；Wuthering Waves PC 版（Mac 已有原生版 [65]）|
| **nProtect GameGuard** | Windows 上有内核组件 | — | **CX 26 上能联机，但会随游戏补丁坏掉又恢复**（26.2 修复更新后无法启动）[1][41][42] | Helldivers 2 |
| **NEAC（网易）** | 多数网易游戏是用户态 | Naraka 在 Linux 上开箱可玩 [37] | 有封号风险：Marvel Rivals 曾误封 Mac 和 Linux 玩家，2025-01 撤销 [40]；CX 26.3 源码里有 `CW Hack 24920/24557`，对 SteamGameId 2767030（Marvel Rivals）阻止 `powershell.exe` 启动 [10] | Marvel Rivals |
| **mhyprot（米哈游）** | 内核驱动 | Genshin 为 Running，崩坏：星穹铁道为 Broken（AWACY）[37] | 未验证 [低] | 原神、星铁、绝区零 |
| **VAC** | 用户态、服务端 | 可用 | 一般可用（推断）[中] | CS2、Dota 2 |

- **硬边界**：
  1. 内核级反作弊在 Wine 下一律不可行。Wine 11.18（2026-09）加强了 ntoskrnl 驱动宿主，但仍在用户态，没有 ring-0 [48]。
  2. EAC 和 BattlEye 在技术上可以为 macOS/Wine 提供一个 `.dylib` 模块。EAC 已有原生 macOS 模块，但只供原生 Mac 版游戏使用，不供 Mac 上的 Wine 使用。要让 Wine 可用，需要 Epic 或 BattlEye 发布这样的模块，并由每个游戏的开发者开启。Cider 无法单方面做到，CrossOver 至今也没有做到。
  3. 任何伪造或绕过反作弊的做法都有封号风险，不应做成产品功能。
- **规模参考**：AWACY 统计 1167 款游戏，其中 Supported 196、Running 276、Broken 640、Denied 53 [37]。这是 Linux 口径，macOS 的可玩集合只会更小，因为“Supported”几乎都依赖 Linux 模块。

### 4. DRM

| DRM | 性质 | Wine/macOS 状态 | 对 Cider 的要求 |
|---|---|---|---|
| **Denuvo Anti-Tamper** | 用户态加固，在线激活，硬件指纹 | CX 23.5（2023-09）起 “Denuvo games are now playable”，需要 Sonoma 或更新 [44]。GOL 2025-05-15 报道：切换 Proton 版本、修改 `WINE_CPU_TOPOLOGY` **可能**被计为新机器，24 小时内 5 台的上限，超限锁 24 小时 [43]；该文是作者观察，未引用 Denuvo/Irdeto 官方说法。2026-04 Pragmata 首发时仍有 Linux/Steam Deck 玩家被锁 [46][80]。2026-08-27 Irdeto 官方博客称 “Denuvo has since addressed this behavior”，但没给修复日期、剩余限制，也没提 CrossOver、macOS、Rosetta 或 FEX [79]。结论：Proton 上可能已缓解，CrossOver/Cider 上未知 [中] | 每个 Denuvo 游戏锁定引擎版本、CPU 拓扑、AVX 广告、机器名、MachineGuid 等（稳妥做法）；换引擎前提示“可能”消耗激活次数 |
| AVX 需求 | 游戏自检 | Rosetta（macOS 15 起）支持 AVX/AVX2，但默认不广告，需要 `ROSETTA_ADVERTISE_AVX=1` [45]。Helldivers 2 不加这个变量会报 “Incompatible CPU detected” [42]。CrossOver 论坛有用户称 M1 上的 Denuvo 游戏不加该变量会报激活错误 [中] | **推断**：切换 AVX 广告会改变 CPUID，可能消耗 Denuvo 激活次数；从 Rosetta 迁到 FEX（CX 27 路线）也会改变指纹 [低] |
| **Arxan / GuardIT** | 反调试和完整性校验（FromSoftware 自 DS2:SOTFS 起使用）[49] | 大多可运行，但对 hook 和代码补丁敏感 | Cider 对游戏代码做二进制补丁或注入 overlay 时要避开受保护模块 |
| **SteamStub** | Steam 包壳 | 需要同前缀的 Steam，或桥接到签名的 client DLL [18] | 见 §1.2 |
| **SafeDisc / SecuROM** | 内核驱动（secdrv.sys 等） | 微软 2015 年起在 Win10 禁用，并用 KB3086255 回移到 Vista–8.1 [47]；Wine 下常见 `ZwLoadDriver failed` | 不投入；引导用户使用 GOG/Steam 的无 DRM 重发版 |
| **StarForce** | 内核驱动 | Wine 社区建议删除其驱动 [中] | 同上 |

### 5. 逐游戏修复机制的设计参考

**现有方案对比**

| 方案 | 格式 | 键 | 能力 | 值得借鉴之处 |
|---|---|---|---|---|
| Proton 脚本 [14] | Python 内置 appid 列表 | `SteamGameId` | `default_compat_config()` 按 appid 开启 `gamedrive`、`heapdelayfree`、`nomfdxgiman`、`forcelgadd`（→`WINE_LARGE_ADDRESS_AWARE=1`）等；用户可用 `STEAM_COMPAT_CONFIG`/`PROTON_*` 覆盖 | 用“命名 flag”封装复杂行为 |
| Proton Wine 的 `hack_append_command_line` [13] | C 表 `{exe_name, append, steamgameid}` | exe 名，可附加 appid | 给子进程追加参数，例如 `Paradox Launcher.exe` 加 ` --use-angle=gl`，`UnrealCEFSubProcess.exe` 加 ` --use-gl=swiftshader`（appid 2316580）| **对 CEF 子进程注入参数**，launch options 做不到这一点 |
| CrossOver `CW HACK nnnnn` [10][11][12] | C 代码，带 bug 号 | exe 名、模块名、SteamGameId | Epic winstation（24938）；Ubisoft ICD（19252）；Marvel Rivals 阻止 powershell（24920/24557）；`libcef.dll`、`Qt5WebEngineCore.dll` 二进制补丁；`simulate_writecopy`（22996/24067）| 为每个 hack 编号和注释，方便日后移植 |
| CrossOver per-game DB（CX 25 起）[1][2][3] | 私有 | 游戏 | Graphics=Auto 时按数据库选择 wined3d、DXMT、DXVK 或 D3DMetal，没有条目时用 wined3d；bottle 级开关包括 DLSS→MetalFX、MSync、High Resolution Mode | 默认“开箱即用”，用户不必手动选后端 |
| CrossTie（.tie，C4 XML）[54] | XML | C4 app id、Steam Id、安装包 glob | 依赖（Pre/Post-Dependencies）、Installer Environment（如 `WINE_WAIT_CHILD_PIPE_IGNORE`）、Installer DLL Overrides、Pre-Install Registry、PreRmFakeDlls、静默参数、快捷方式 | 覆盖“安装期”配置 |
| umu-protonfixes [50] | 每个游戏一个 Python 文件，`main()` | `gamefixes-{steam,egs,gog,umu,...}/<id>.py` | `util.*` 约 40 个函数：`protontricks`、`regedit_add`、`replace_command`、`append_argument`、`set_environment`、`winedll_override`、`disable_esync/fsync/ntsync`、`set_ini_options`、`set_xml_options`、`set_dxvk_option`、`set_cpu_topology_*`、`create_dos_device`、`install_eac_runtime` 等。例如 Elden Ring（1245620）会 touch 空的 `DLC.bdt`/`DLC.bhd` 以避免 EAC 误报 | 类型化原子操作的清单 |
| umu-database [51][52] | CSV：`TITLE,STORE,CODENAME,UMU_ID,COMMON ACRONYM,NOTE,EXE_STRINGS` | 跨商店统一的 `umu-<steamappid>` | 约 1700–1800 行；umu-launcher 用 `GAMEID`/`STORE` 查询 | **跨商店 ID 映射**，可以直接复用 |
| Lutris [53] | YAML：`game/files/installer/wine/system` | slug | `task`（wineexec、winetricks、set_regedit、create_prefix）、`extract/move/merge/write_config/write_json/input_menu/insert-disc` | 安装流程可声明 |
| **Highball**（macOS，GPL-3 程序 + CC0 数据）[21] | JSON recipe：`steps[]`（note、sync、installer、pin、env、registry、winetricks……）、`knownIssues[{symptom,cause,fix}]`、`lastVerified{date,engine,macos,chip,result}`、`blocked{reason,tracking}`；游戏库记录 `status`（verified-local、reported-upstream、community、blocked-anticheat）、`provenance`、`anticheat.macVerdict` | 按 id 与 steam_appid | 按 pin 设置 renderer 和 env（例如 Epic 启动器用 DXVK，游戏用 DXMT）；按 pin 设置 sync | **与 macOS 最贴近的现成模板**，含 CC0 数据 |
| Wine AppDefaults [73] | 注册表 `HKCU\Software\Wine\AppDefaults\<exe>\{DllOverrides,Mac Driver,Direct3D,...}` | exe 名 | Wine 原生的按程序覆盖 | 落地层，零成本 |

**常见修复类型**（归纳自以上来源）：环境变量；DLL override（`native,builtin` 等）；winetricks verbs（vcrun、dotnet、mfc140、字体）；启动参数和子进程参数追加；注册表（含 IFEO、服务超时）；文件操作（touch、复制 DLL，如把 RDR2 的 `vulkan-1.dll` 放到 exe 旁 [57]；symlink 修 junction [24]）；INI/XML 配置编辑（分辨率、窗口化、跳过 intro）；渲染后端选择；同步模式；CPU 拓扑；Windows 版本；AVX 广告；Retina/DPI；进程替身（EpicGamesLauncher.exe）；阻止特定子进程（powershell）；加载期二进制补丁。

### 6. 热门游戏兼容现状（2025–2026）

| 游戏 | 最佳路径 | 状态与关键点 | 日期 / 来源 | 置信度 |
|---|---|---|---|---|
| Elden Ring | CrossOver + D3DMetal（DX12）+ ESync/MSync | 离线可玩，EAC 联网不可用；AGW 最近的 M 系列报告（CX 23.7.1，macOS 15.4.1）；M3 Max 1080p 约 53fps | 2025-05-12 [55]；Highball 社区报告 2026-09-22 在 M1 上连续玩 231 分钟 [56] | 中 |
| Cyberpunk 2077 | **原生 macOS** | 2025-07-17 发布，Apple Silicon，**要求 16GB 或以上** | [59] | 高 |
| GTA V Enhanced | CrossOver，仅故事模式 | 需要在 RGL 中关闭 BattlEye；Online 不可用。D3DMetal 在 M1/M2 上报 `ERR_GFX_D3D_NOD3D12`，现场报告显示 M3 及以上可用（疑与硬件光追有关）；vkd3d→MoltenVK 会 GPU device lost（42 个图形管线、27 个计算管线失败）| 2026-09-08 [58][64] | 中/低 |
| Baldur's Gate 3 | **原生 macOS** | — | [umu 行示例 51] 仅佐证 ID，原生版本为常识 | 中 |
| Hogwarts Legacy | CrossOver + D3DMetal + ESync | MacGamingDB 10 份报告，中位数 40fps；社区称中画质至少需要 24GB；含 Denuvo | [60][61] | 中 |
| Black Myth: Wukong | CrossOver + D3DMetal + MSync | Tahoe + GPTK 3 下 M1 Max 约 60fps（开 DLSS→MetalFX 约 70）；官方 tips 页 | 2025-06 [61][62] | 中 |
| Red Dead Redemption 2 | CrossOver 25 起支持；游戏使用 **Vulkan→MoltenVK** | 峰值占用 13.7GB，16GB 机器会交换；**8GB 的 iMac M3 在片头崩溃**；MoltenVK 线性平铺 3D 纹理问题在 Highball r6 修复；D3DMetal 的 DX12 路径约 30 秒后报 “Not implemented” | 2025-03-11 [1][63]；2026-09-11/25 [57] | 高/中 |
| Starfield | CrossOver 26 | 26.0 修复列表中 | 2026-02-10 [1] | 高 |
| Diablo IV | CrossOver + Battle.net | 26.3 修复“游戏更新后无法启动”，说明游戏更新会反复打坏 | 2026-07-21 [1] | 高 |
| Helldivers 2 | CrossOver 26.3 + D3DMetal + `ROSETTA_ADVERTISE_AVX=1` | CodeWeavers 评级 “Runs Well”；GameGuard 可联机，但随补丁时好时坏；135GB 磁盘 | 2026-02/06 [1][42][71] | 中 |
| 中国热门游戏 | 视情况 | Wuthering Waves 已上架 Mac App Store（2025-03-27）[65]；黑神话见上；HoYoPlay 启动器可用（CX 24.0.4）[1]；原神在 Linux 上为 Running、星铁为 Broken（AWACY，Linux 口径）[37]；Naraka（NEAC）在 Linux 上开箱可玩 [37]；三角洲行动（ACE）推断不可用；Marvel Rivals 有封号先例 [40] | — | 低/中 |

- 另有 GPTK 4 / D3DMetal 4（WWDC26，2026-06，首个 beta，只支持 Apple Silicon，基于 Metal 4）：AppleInsider（2026-06-17）引用创作者用 beta 做的测试，M3 Max 上 Cyberpunk 2077 的 DX12 路径帧数约 +10%，RDR2 约 +25%（+7fps）[68]。“需要 macOS 27” **未经核实**：该文只提到测试跑在 macOS 27 beta 上，没有说 macOS 27 是最低要求，也没找到 Apple 写明最低系统版本的 GPTK 4 发布说明 [68][84]。截至 2026-09-26 最新的 CrossOver 26.3.0 changelog 尚未提到 D3DMetal 4 [1]。

---

## 对 Cider 的启示与建议（按优先级）

**P0（立刻做，第 0–2 个月）**

1. **建立启动器和游戏的回归测试平台（每日跑）。**
   - 覆盖 Steam、EA app、Battle.net、Ubisoft Connect、Rockstar、Epic（仅登录）、GOG（仅登录），加上 10–20 款测试游戏（D3D9/11/12、Vulkan 各若干，尽量用免费或 DRM-free 的，通过 steamcmd `+@sSteamCmdForcePlatformType windows` 拉取）。
   - 每天自动检测 Steam 客户端的 buildid 变化。CrossOver 的 changelog 显示几乎每个小版本都要修启动器 [1]；Steam 2026-09-03 的更新让 Linux 原生客户端崩溃 [7]，Highball 也在 Mac/Wine 上记录了自更新后退出的问题 [6]（两者是否同源未证实）。
2. **Steam in bottle 基线（与 CrossOver 对齐）。**
   - 只做 64 位 bottle。Steam 的硬性要求只是“不能是纯 32 位前缀”[76]；Cider 统一用新 WoW64 是自己的架构选择（与 CX 27 放弃 32 位 bottle 的方向一致 [82]），不是 Steam 强制的。
   - Steam UI 进程固定使用 `WINEMSYNC=0 WINEESYNC=0`，默认关闭 web views GPU 加速和 overlay；游戏会话用 msync。
   - 实现“按进程组选择同步模式”，或在切换时自动重启 wineserver，这是首要的架构问题。
   - 移植 winemac hosted-layer 修复（`setContainer:frame:` 的坐标换算和 `CATransaction` flush）[8]，并开启 DXMT 的 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1`。
3. **做一个 Chromium-in-Wine 专项。**
   - 把 CEF、QtWebEngine、WebView2 的 GPU 进程、sandbox、跨进程 swapchain 问题当作一个整体来处理。
   - 在 Wine 中实现类似 Proton `hack_append_command_line` 的数据驱动机制：按 exe 名或 appid 给子进程追加 `--in-process-gpu`、`--use-angle=...`、`--no-sandbox` 等参数 [13]。
   - 评估 CrossOver 对 `libcef.dll`/`Qt5WebEngineCore.dll` 的加载期二进制补丁 [11]。
4. **设计 per-game profile 数据库（Cider Profiles）。**
   - 格式：JSON（加 JSON Schema 校验），或更适合人写的 TOML，由 CI 统一编译成签名的 JSON 包。
   - 键：规范 ID 沿用 `umu-<steamappid>`，另外映射 `steam_appid`、`egs_codename`、`gog_id`、`amazon_id`，以及 `exe_name + PE VersionInfo + sha256` 用于识别游戏。直接导入 umu-database 的映射 [51]。
   - 条件（`when`）：引擎版本区间、macOS 版本、芯片代际（M1/M2 与 M3+ 光追）、内存（例如 `ram_gb < 16` 时警告）、翻译层（Rosetta 或 FEX）、渲染后端可用性。
   - 动作（`actions`，类型化，全部幂等）：`env`、`dll_overrides`、`registry`（含 IFEO）、`verbs`、`args`、`child_args`（按子进程 exe）、`files`（touch、copy、symlink、replace，带 sha256）、`ini`/`xml`、`renderer`、`sync`、`cpu_topology`、`winver`、`rosetta.advertise_avx`、`retina`、`stand_in_process`（如 EpicGamesLauncher.exe）、`block_process`。
   - 元数据：`anticheat`（导入 AWACY 数据 [37]，再加一个 Cider 自己的 `mac_verdict`）、`drm.denuvo=true` 时锁定指纹、`known_issues[{symptom,cause,fix}]`、`provenance`、`last_verified{date,engine,macos,chip,ram}`。这部分直接借鉴 Highball 的结构 [21][6]。
   - 分发：Git 仓库 → CI 校验 → 签名包（例如 minisign）走 CDN，客户端离线缓存，用户可本地覆盖。复杂逻辑留一个受限脚本钩子，但默认不启用。
   - 数据来源：Highball recipe 和游戏库为 CC0，可直接吸收；umu-protonfixes 可作为动作清单参考（其中 Linux 专属项需要剔除）。
5. **反作弊与 DRM 守门。**
   - 下载或安装前先查表：内核级反作弊游戏标记为“不支持联机”，给出明确说明；有 EAC 或 BattlEye 的游戏同样不能联机（它们只有 Linux 模块 [74]），开发者支持离线模式的显示“仅离线”。
   - Denuvo 游戏：首次成功启动后冻结引擎版本、CPU 拓扑、AVX 广告和机器标识；用户切换引擎前弹出“可能消耗激活次数”的警告 [43]。这是稳妥做法：Irdeto 2026-08-27 称已处理 Proton 版本切换被当成新机器的问题 [79]，但没说对 CrossOver/macOS 是否适用，Cider 上的实际行为要实测，不能把 “5 次/24 小时锁定” 当成已确认的现行规则。

**P1（3–6 个月）**

6. **Epic、GOG、Amazon 默认走开源客户端**（Legendary 0.21.x、gogdl、nile），Cider 用原生 UI 包装 [28]。这是产品取舍（原生 UI、少一个 CEF 进程、维护面小），不是因为官方启动器跑不起来：DP-07 只在 Highball 的 Sikarugir Wine 10 引擎上出现 [22]，CrossOver 25.0 起官方支持 Epic 启动器，26.3.0 还修了下载问题 [1]。因此仍把官方 Epic 启动器作为兼容选项保留（部分游戏或用户需要），并纳入回归测试集。同时提供 `EpicGamesLauncher.exe` 替身，以满足 Rockstar 等游戏的授权检查 [57]。
7. **补 NT 语义缺口**：junction/reparse point 持久化（EA app）[24]；服务启动、`ServicesPipeTimeout`（Rockstar）[26]；ACL 持久化（Epic DP-07，官方 Epic 启动器装游戏要用；CrossOver 上官方启动器可用，推断它已用某种方式解决或绕开，Cider 需要对齐）[22][1]。这些会让大量启动器受益。
8. **原生 macOS Steam 桥接，作为 R&D 支线**，参考 macos-steam、ullage [18][19]。
   - 收益：Steam UI、下载、云存档和 overlay 全部原生，免去“CEF 在 Wine 里”的维护负担。
   - 代价：依赖 `m_bCompatEnabled` 注入或 `appinfo.vdf` 改写，Valve 一次更新就可能失效；SteamStub 需要 Valve 签名的 client DLL；Steam API 覆盖不全。
   - 建议保留两条路线并行：bottle Steam 作为兼容性兜底，原生桥接作为可选的体验模式。

**P2（6–12 个月）**

9. **跟进 FEX/ARM64 迁移对游戏生态的影响**。CX 27 ARM64 预览版（2026-07-31）使用为 macOS 定制的 FEX 移植，没有 D3DMetal（“Direct3D 12 support coming soon”），“Many game launchers do not function at all”，也不能转换现有 bottle；CodeWeavers 预计在 CrossOver 27（计划 2027 年初）之前解决 [67][81]。Cider 在切换到 FEX 之前，必须把第 1 项的测试平台跑通；Denuvo 游戏也要做迁移提示（从 Rosetta 换到 FEX 是否会被 Denuvo 当成新机器，目前无公开信息）。
10. **8GB 开发机的限制**：3A 游戏（RDR2、Hogwarts、Cyberpunk 原生版）的验证需要 16–32GB 设备，或者靠社区报告。建议接入类似 Highball 的社区上报流程（issue → report → 数据库）[21]。
11. **Wine 11 升级要单独验证 CEF 启动器**。Highball 用 CX 26.3 源码自行打包的 Wine 11 + DXMT 引擎上，Battle.net 和 GOG Galaxy 都回归了 [23][27]，而 CrossOver 26.3 产品本身支持这两个启动器 [1]。推断 CrossOver 在公开源码之外还有构建、配置或 per-game 数据库层面的差异 [低]。Cider 自建 Wine 11 基线时，要把这两个启动器放进升级前的必过用例。

---

## 风险

- **上游更新打坏兼容性**：Steam 客户端（2024-11 的 CEF 升级；2026-09-03 更新后 Linux 原生客户端的主循环卡死）、各启动器的自更新、游戏补丁（Diablo IV、Helldivers 2）都会不可预期地破坏兼容 [1][7][15]。维护成本是持续性的，没有“一次做完”的阶段。
- **反作弊是硬墙**：大量热门多人游戏（Fortnite、Apex、Valorant、CoD、BF6、PUBG、R6、Destiny 2）在可见的未来都无法在 Mac/Wine 上联机 [37][38]。用户期望如果管理不好，会直接拉低口碑；另有误封风险 [40]。
- **Denuvo 锁定**：引擎升级（尤其从 Rosetta 迁到 FEX）或配置变动可能让用户被锁 24 小时 [43][46]。Irdeto 2026-08-27 称已处理 Proton 上的这类误判 [79]，但对 CrossOver/macOS 是否适用未知，风险降级为“可能”，仍需实测。
- **Rosetta 退场**：Apple 开发者新闻称 macOS 27 是 “Final release to support Rosetta”，之后 Intel 专用应用将无法运行，只保留面向 “older, unmaintained gaming titles that rely on Intel-based frameworks” 的 Rosetta 功能 [66]（抓取时页面显示 2026-09-01，原始公告可能更早）。x86 Windows 游戏走 Rosetta 这条路线在 macOS 28 上能否继续使用尚不确定，需要 FEX 作为后备 [67][81]。
- **原生 Steam 桥接的脆弱性**：依赖 DYLD 注入和 Valve 未公开的内部开关，随时可能被关闭 [18]。
- **D3DMetal 是闭源组件**：GTA V Enhanced 等游戏在 D3DMetal 中遇到 “Not implemented” 时无法自行修复 [57][58]；GPTK 4 的最低系统版本未经核实（媒体测试跑在 macOS 27 beta 上，但未见 Apple 写明最低要求）[68][84]。
- **开发机只有 8GB 内存**，覆盖不到主流 3A 游戏的内存需求 [57][59]。

## 未解问题

1. CrossOver 26 让 Helldivers 2 的 GameGuard 可联机，具体做了哪些改动？（源码中只看到图标 hack；CodeWeavers 博客 403，未能读取。）
2. CX 26.3 中 `apply_binary_patches()` 对 `libcef.dll` 和 `Qt5WebEngineCore.dll` 具体打了哪些字节补丁、原因是什么？需要全文读取 `dlls/ntdll/loader.c`。
3. macOS Steam 客户端里的 `m_bCompatEnabled` 兼容通道，Valve 会继续保留还是移除？SteamStub 在桥接模式下是否 100% 可用？
4. Epic 或 BattlEye 是否可能为“macOS 上的 Wine”提供 Mach-O 模块？（EAC 已有原生 macOS 模块，但只供原生 Mac 版游戏使用，技术上可行，但没有任何公开计划为 Wine 提供。）
5. Denuvo 在 Rosetta 下读到的 CPUID，是否会因 `ROSETTA_ADVERTISE_AVX` 的开关而变化，并被计入激活？迁移到 FEX 时会发生什么？Irdeto 2026-08-27 所说的“已处理”是否也覆盖 CrossOver/macOS？（本报告此处为推断，需要实测。）[79]
6. EA Javelin 的 Linux/Proton 支持到底进展如何？（2026-03 的招聘启事可以确认；“2026-07 已支持”的说法没有核实。）[39]
7. 中国热门游戏（原神、星铁、绝区零、三角洲行动、燕云十六声、永劫无间）在 CrossOver 26 上的实测状态缺少一手数据；本次的搜索预算已用完。
8. Xodus 在 macOS 上的实际进度，以及 Game Pass 运行层能否复用 Wine 与 D3DMetal [31]。
9. Steam overlay 能否统一改用原生 Metal hook（macos-steam 的做法），同时不触发 Arxan 等反篡改机制？
10. Highball 的 Mac/Wine “自更新后约 3 分钟退出”与 steam-for-linux #13576 的 `BMainLoop stalled` 是否同源？（前者 recipe 最后验证于 2026-08-24，早于 2026-09-03 的更新，目前没有证据表明两者是同一个 bug。）[6][7]
11. GPTK 4 / D3DMetal 4 的最低 macOS 版本是多少？是否只在 macOS 27 上可用？[68][84]

## 参考来源

1. https://www.codeweavers.com/crossover/changelog — CrossOver 24.0–26.3.0 changelog 原文（日期、Wine/D3DMetal/DXMT 版本、启动器与游戏修复）
2. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26 — CX Mac 26 Advanced Settings：Auto、DXMT、D3DMetal、DXVK、wined3d、DLSS、MSync、High Resolution
3. https://www.codeweavers.com/blog/mjohnson/2025/3/11/experience-next-level-gaming-on-mac-with-crossover-25 — CX 25 博文（403，经搜索摘要转述：新配置系统，无需 CrossTie）
4. https://videocardz.com/newz/steam-for-windows-moves-to-64-bit-32-bit-updates-end-on-january-1-2026 — Steam 64 位化，32 位 2026-01-01 停更（搜索摘要）
5. https://www.tomshardware.com/video-games/pc-gaming/steam-begins-64-bit-transition-on-windows-as-32-bit-support-enters-final-countdown — 同上（搜索摘要）
6. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/steam.json — Steam recipe：sync、CEF、64 位客户端、已知问题（2026-08/09 实测）
7. https://github.com/ValveSoftware/steam-for-linux/issues/13576 — 2026-09-05 开的 issue：2026-09-03 更新（buildid 1788400362）后 Linux 原生客户端 BMainLoop stall 崩溃
8. https://github.com/dappermint/winecx-gptk/issues/11 — winemac hosted-layer 导致 Steam 菜单空白、2× 绘制的诊断与修复（2026-09-19）
9. https://github.com/PlayOnLinux/wine-patches/blob/master/custom/steam_crossoverhack/crossover_hack_52560.patch — 旧 CrossOver steamwebhelper `--no-sandbox` hack
10. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/kernelbase/process.c — CX 26.3：CW Hack 24938、19252、24920/24557、CX Hack 20810
11. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/loader.c — CX 26.3：libcef、Qt5WebEngineCore、cohtml 二进制补丁；CW HACK 22434（第三方镜像，非 CodeWeavers 官方仓库）
12. https://raw.githubusercontent.com/dappermint/winecx/master/dlls/ntdll/unix/loader.c — CW Hack 24067、22144、22996、22434、CX HACK 20810
13. https://raw.githubusercontent.com/ValveSoftware/wine/proton_10.0/dlls/kernelbase/process.c — Proton `hack_append_command_line` 表
14. https://raw.githubusercontent.com/ValveSoftware/Proton/proton_10.0/proton — Proton 脚本 `default_compat_config()` 按 appid 的 flag
15. https://github.com/Whisky-App/Whisky/issues/1200 — 2024-11 Steam 更新导致 steamwebhelper 在 Whisky 中失效；仓库 2025-05-11 归档
16. https://github.com/Whisky-App/Whisky/issues/1202 — 同期问题；`-allosarches -cef-force-32bit` 权宜之计（搜索摘要）
17. https://github.com/ValveSoftware/steam-for-linux/issues/10561 — `-cef-disable-gpu` 相关（搜索摘要）
18. https://github.com/Superd22/macos-steam — 原生 macOS Steam 兼容通道加 steamclient 桥（AGPL-3.0，“working beta”，v0.5.0，2026-09-05；已测 CrossOver 25.1.1 与 26.2）
19. https://github.com/xXJSONDeruloXx/ullage — appinfo.vdf 启动映射加 lsteamclient 桥（2026-08）
20. https://github.com/natbro/kaon — `steam_dev.cfg` 强制 Windows 平台加 CrossOver（Apache-2.0）
21. https://github.com/gauthierpiarrette/highball 与 https://github.com/gauthierpiarrette/highball-db — 开源 macOS 游戏层，JSON recipe 与 CC0 兼容库
22. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/epic-games.json — Epic：DXVK pin、DP-07 阻塞（最后验证 2026-08-25，`x64-sikarugir10.0_6-r0`，macOS 14.6，M1 Pro）
23. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/battle-net.json — Battle.net：Sikarugir Wine 10 引擎可用（2026-09-18）；Highball 自打包的 `x64-crossover26.3-r8`～`r10` + DXMT 上黑屏、int3 崩溃（highball#119）
24. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/ea-app.json — EA app：Burn、MSI、junction 问题
25. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/ubisoft-connect.json — Ubisoft：CEF GPU、`WINE_SIMULATE_WRITECOPY=1`
26. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/rockstar.json — Rockstar：注册表、时区、安装卡住
27. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/recipes/launchers/gog-galaxy.json — GOG Galaxy：Wine 10 引擎黑屏；Highball 自打包的 `x64-crossover26.3-r9`/`r10` 上 Qt6WebEngine 崩溃（highball#149；2026-09-26 复读确认）
28. https://github.com/derrod/legendary/releases — Legendary 0.21.0/0.21.1（0.21.1 于 2026-09-08 发布；ChunksV5、wrapper EXE、macOS symlink）
29. https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/releases — Heroic 2.22.3（2026-09-16）
30. https://appleinsider.com/articles/25/04/16/whisky-development-ends-on-macos-to-help-wine-flourish — Whisky 停止开发
31. https://www.gamingonlinux.com/2026/08/xbox-pc-and-game-pass-coming-to-linux-with-the-xodus-project/ — Xodus（Game Pass 逆向，2026-08-11）
32. https://wiki.leagueoflegends.com/en-us/Riot_Vanguard — LoL macOS 嵌入式 Vanguard（25.S1.2，2025-01-23；搜索摘要）
33. https://support.codeweavers.com/anti-cheat — CodeWeavers 反作弊支持政策（不支持、不修复、不能合法绕过；未点名 EAC/BattlEye）
34. https://www.codeweavers.com/blog/mjohnson/2026/8/31/why-do-most-games-with-anti-cheat-not-work-with-crossover-mac — 2026-08-31 反作弊博文（403，搜索摘要转述）
35. https://www.gamingonlinux.com/2021/09/epic-games-announce-full-easy-anti-cheat-for-linux-including-wine-a-proton/ — 2021-09 EAC Wine/Proton 公告：Linux 模块；“SDK 1.14” 仅属当时的 EOS 路径，现已不需要（见 [74][75]；搜索摘要）
36. https://www.gamingonlinux.com/2021/11/supporting-linux-proton-and-the-steam-deck-with-battleye-is-just-an-email-away/ — BattlEye 靠邮件开启，独立 Proton 运行时
37. https://areweanticheatyet.com/ 与 https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/HEAD/games.json — 反作弊兼容统计与条目
38. https://www.techspot.com/news/108925-battlefield-6-anti-cheat-system-requires-secure-boot.html — BF6 Javelin 要求 Secure Boot（搜索摘要）
39. https://www.gamingonlinux.com/2026/03/ea-javelin-anticheat-job-listing-mentions-future-support-for-linux-and-proton/ — EA 招聘启事提及 Linux/Proton（搜索摘要）
40. https://appleinsider.com/articles/25/01/04/netease-reverses-bans-on-macos-linux-players-of-marvel-rivals — Marvel Rivals 撤销对 Mac/Linux 玩家的封禁（搜索摘要）
41. https://wineformac.org/news/blog-crossover-26-anti-cheat-2026.html — CX 26 与 Helldivers 2、Darktide（二手来源）
42. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/helldivers-2.json — HD2：GameGuard、AVX2、`ROSETTA_ADVERTISE_AVX=1`
43. https://www.gamingonlinux.com/2025/05/denuvo-will-lock-you-out-of-games-on-linux-steamos-steam-deck-if-you-keep-changing-proton-versions/ — Denuvo 激活计数（2025-05-15）
44. https://www.codeweavers.com/blog/mjohnson/2023/9/27/crossover-235-is-a-real-game-changer — CX 23.5 支持 Denuvo（搜索摘要）
45. https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/4193 — `ROSETTA_ADVERTISE_AVX`（搜索摘要）
46. https://www.notebookcheck.net/Denuvo-blocks-Pragmata-Steam-Deck-and-Linux-players-as-reviews-slam-anti-piracy-DRM.1276439.0.html — Pragmata 首发时 Denuvo 锁定（搜索摘要）
47. https://www.guru3d.com/story/microsoft-disables-securom-and-safedisc-drms-and-thus-their-games/ — SafeDisc/SecuROM 被禁用（搜索摘要）
48. https://www.gamingonlinux.com/2026/09/wine-11-18-released-with-more-ntoskrnl-support-for-kernel-drivers/ — Wine 11.18 NTOSKRNL 改进
49. https://me3.help/en/latest/blog/posts/arxan-reversing-1/ — Arxan/GuardIT 在 FromSoftware 游戏中的使用（搜索摘要）
50. https://github.com/Open-Wine-Components/umu-protonfixes — README、util.py、LICENSE（BSD-2）、gamefixes-steam/1245620.py
51. https://github.com/Open-Wine-Components/umu-database — CSV 结构与 ID 规则（GPL-3.0）
52. https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/README.md — GAMEID、STORE、PROTONPATH
53. https://raw.githubusercontent.com/lutris/lutris/master/docs/installers.rst — Lutris 安装脚本格式
54. https://support.codeweavers.com/crosstie-data-startpage/an-intermediate-guide-on-what-the-crosstie-editor-options-mean — CrossTie 字段说明
55. https://www.applegamingwiki.com/wiki/Elden_Ring — Elden Ring on Mac（EAC 离线、D3DMetal）
56. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/elden-ring.json — 社区报告（2026-09-22）
57. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/red-dead-redemption-2.json — RDR2：MoltenVK、内存、8GB 崩溃、Epic 替身
58. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/grand-theft-auto-v-enhanced.json — GTA V Enhanced：D3DMetal、vkd3d、BattlEye
59. https://appleinsider.com/articles/25/07/15/cyberpunk-2077-ultimate-edition-coming-to-apple-silicon-macs-on-july-17 — Cyberpunk 原生版（搜索摘要）
60. https://macgamingdb.app/games/990080 — Hogwarts Legacy 社区性能数据
61. https://www.notebookcheck.net/Black-Myth-Wukong-Hogwarts-Legacy-and-the-Witcher-3-run-at-60-FPS-on-macOS-Tahoe-26-with-Metal-4.1040850.0.html — Tahoe + GPTK 3 实测（搜索摘要）
62. https://www.codeweavers.com/compatibility/crossover/tips/black-myth-wukong/tweak-crossover-to-run-the-game-on-mac — 黑神话 CrossOver tips（搜索摘要）
63. https://9to5mac.com/2025/03/11/crossover-25-red-dead-redemption-2-macos/ — CX 25 支持 RDR2（搜索摘要）
64. https://www.codeweavers.com/support/forums/general/?t=27&forumc__=&forumcurPos=0&msg=346704 — GTA Online BattlEye 在 Mac 上不可用（搜索摘要）
65. https://www.siliconera.com/wuthering-waves-launch-trailer-and-mac-store-release-shared/ — Wuthering Waves Mac App Store 版（搜索摘要）
66. https://developer.apple.com/news/?id=w5ngl9k2 — Apple：macOS 27 是 “Final release to support Rosetta”，面向较老、无人维护游戏的 Rosetta 功能会保留（抓取时页面日期 2026-09-01）
67. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — CX 27 ARM64 预览版的限制（2026-07-31）
68. https://appleinsider.com/articles/26/06/17/apples-game-porting-toolkit-4-is-a-big-improvement-for-modern-game-coders — GPTK 4 / D3DMetal 4 首个 beta（2026-06-17；创作者实测；未写明 macOS 27 为最低要求）
69. https://github.com/Sikarugir-App/Sikarugir — Wineskin 后继，引擎与渲染器选项
70. https://github.com/dappermint/winecx-gptk — CX 26.3 差异前移到 Wine 11.17 的 GPTK 运行时
71. https://appleinsider.com/articles/26/02/10/crossover-26-update-adds-compatibility-for-blockbuster-expedition-33-and-helldivers-2 — CX 26 发布报道（搜索摘要）
72. https://swissmacuser.ch/download-install-steam-game-files-on-mac-os/ — macOS steamcmd 下载 Windows depot（搜索摘要）
73. https://gitlab.winehq.org/wine/wine/-/wikis/Useful-Registry-Keys — Wine AppDefaults（抓取被 Anubis 拦截，内容为既有知识）[中]
74. https://partner.steamgames.com/doc/steamdeck/proton — Valve Steamworks 文档：EAC（启用 Linux、激活 Unix 模块、改名 `easyanticheat_x64.so`）与 BattlEye（邮件联系 Valve 或 BattlEye）在 Proton 下的开启步骤，无 SDK 下限
75. https://www.gamingonlinux.com/2022/01/easy-anti-cheat-gets-much-simpler-for-proton-and-steam-deck/ — 2022-01：Proton 支持 EAC “without requiring any recompilation”
76. https://www.gamingonlinux.com/2025/12/latest-steam-stable-update-is-live-as-windows-gets-64-bit/ — 引用 Valve 2025-12-19 客户端更新说明：Windows 版 Steam 改为 64 位
77. https://steamcommunity.com/games/593110/announcements/detail/528740542771627405 — Steam 客户端更新公告（2025-12）
78. https://help.steampowered.com/faqs/view/49A1-B944-48B8-FF00 — Steam FAQ：32 位 Windows 上的现有安装不再获得任何更新（搜索摘要）
79. https://irdeto.com/blog/denuvo-anti-piracy-proton-linux-steam-deck — Irdeto 官方博客 “Denuvo Anti-Piracy for Proton, Linux and Steam Deck”（2026-08-27，Denuvo 产品经理撰写）：称已处理 Proton 版本切换被误判的问题
80. https://x.com/SteamDeckHQ/status/2045192293813203341 — SteamDeckHQ：Pragmata 在 Steam Deck/Linux 上被 Denuvo 锁定（2026-04）
81. https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac — CodeWeavers 博客：CX 27 ARM64（FEX）预览版，无 D3DMetal、多数启动器不可用、不能转换 bottle（2026-07-31）
82. https://www.codeweavers.com/blog/mjohnson/2026/6/11/whats-in-and-whats-out-for-crossover-27 — CodeWeavers 博客：CX 27 仅 Apple Silicon、需 Sonoma 及以上、不再运行 32 位 bottle（2026-06-11；搜索摘要）
83. https://github.com/Superd22/macos-steam/tags 与 https://github.com/Superd22/macos-steam/commits/main — macos-steam 的 tag（v0.1.0 2026-08-26 至 v0.5.0 2026-09-05）与 commit 记录（85 个）
84. https://developer.apple.com/wwdc26/guides/games/ — WWDC26 游戏开发指南（未找到写明 GPTK 4 最低系统版本的说明）
85. https://www.omgubuntu.co.uk/2026/02/crossover-26-released — CrossOver 26 发布报道（2026-02）
86. https://en.wikipedia.org/wiki/CrossOver_(software) — CrossOver 条目：稳定版本线 26（2026-02-10）

## 事实核查记录

> 2026-09-26 对本报告中的 12 条关键声明做了独立核查。下表记录结论，正文已按“更正”一栏就地修改。本轮没有出现两名核查者结论相互矛盾的情况，因此没有标“存疑”的条目；证据不足的点在正文中标为“未经核实”或“未证实”。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| CrossOver 26.0.0 于 2026-02-10 发布（Wine 11.0、D3DMetal 3.0、DXMT v0.72、vkd3d 1.18）；截至 2026-09-26 最新为 26.3.0（2026-07-21），修复 Diablo IV、Epic 下载、GOG Galaxy | 属实 | 官方 changelog 逐字确认，并列出 Wine Mono 10.4.1；26.3.0 之后没有新版本，也没搜到 26.4。中间版本 26.1.0（2026-04-09，修 Battle.net 安装）和 26.2.0（2026-06-09，修 Helldivers 2、增加 32 位 bottle 警告）也核对无误。唯一的小出入：搜索摘要把 26.3.0 论坛公告日期写成 2026-06-16，但该页 403，以 changelog 的 2026-07-21 为准 [1][85][86] |
| EAC 的 Wine/Proton 支持需要开发者开启 Linux 模块（SDK ≥ 1.14）并发布 `easyanticheat_x64.so`；BattlEye 需联系开启、走 Proton BattlEye Runtime；两者只有 Linux 模块，CodeWeavers 明确说对 CrossOver Mac 无效、也不绕过 | 部分属实 | 结论（Mac 上 EAC/BattlEye 无法联机）成立。更正：“SDK ≥ 1.14” 只属于 2021-09 最初的 EOS 路径；2022-01 起 Proton 支持 EAC 无需重编译或更新 SDK，Valve 现行文档没有 SDK 下限。具体步骤为启用 Linux、激活 Unix 模块、把 `libeasyanticheat.so` 改名为 `easyanticheat_x64.so` 放在 `EasyAntiCheat_x64.dll` 旁。BattlEye 是给 Valve 或 BattlEye 发邮件。“对 Mac 无效”出自 CodeWeavers 2026-08-31 博文（403，经摘要）；支持页只说不支持、不修复、不能合法绕过反作弊，没有点名 EAC 或 BattlEye。EAC 的原生 macOS 模块只服务原生 Mac 版游戏 [33][34][35][36][74][75] |
| Windows Steam 在 64 位 Win10/11 上已改为 64 位，32 位客户端 2026-01-01 起不再更新 | 属实 | Valve 2025-12-19 更新说明原文确认；2026-01-01 的截止只针对 32 位 Windows 上的 32 位客户端。更正报告里的推论：“bottle 必须是新 WoW64”是推断，任何 64 位前缀（含旧式 WoW64）都会拿到 64 位客户端，真正要避免的是纯 32 位前缀。置信度由 [中] 提到 [高] [76][77][78][4] |
| Denuvo 会把切换 Proton 版本、修改 `WINE_CPU_TOPOLOGY` 计为新硬件激活；24 小时约 5 次上限，超限锁 24 小时（GOL 2025-05-15） | 部分属实 | GOL 的报道属实，但只是作者观察，没有引用官方说法；2026-04 Pragmata 仍有锁定报告。2026-08-27 Irdeto 官方博客称 “Denuvo has since addressed this behavior”，没给修复日期、剩余限制，也没提 CrossOver、macOS、Rosetta 或 FEX。改为：Proton 上可能已缓解，CrossOver/Cider 上未知。P0 第 5 项“锁定指纹设置”仍保留为稳妥做法，但不再把 5 次/24 小时锁定当成已确认的现行规则 [43][46][79][80] |
| Apple：macOS 27 是最后支持 Rosetta 的版本，但为较老、无人维护的 Intel 游戏保留 Rosetta 功能；CX 27 ARM64（FEX）预览版（2026-07-31）没有 D3DMetal，很多启动器无法运行 | 属实 | Apple 开发者新闻原文确认（抓取时页面日期 2026-09-01，原始公告可能更早）。CodeWeavers 博客与 AppleInsider 同日报道：定制 FEX 移植、无 D3DMetal（DX12 “coming soon”）、“Many game launchers do not function at all”、不能转换现有 bottle；预计 CX 27（计划 2027 年初）前解决。补充引用 CodeWeavers 博客 [66][67][81][82] |
| macos-steam（Superd22，AGPL-3.0，v0.5.0，最新 commit 2026-09-05）注入 dylib 翻转 `m_bCompatEnabled`，注册为兼容工具，并用替换的 `steamclient64.dll` 桥接原生 `steamclient.dylib` | 属实 | README、tag 页、commit 页确认；补充细节：v0.1.0（2026-08-26）至 v0.5.0（2026-09-05）只有 tag、没有 Release 二进制，共 85 个 commit；已测 CrossOver 25.1.1 与 26.2（原文“依赖 25.1.1 及以上”改为“已测试”）；桥接方式是经 Wine unix 边界封送到一个承载 `steamclient.dylib` 的原生 `.so`。项目约一个月大，依赖未公开的内部开关，很脆弱 [18][83] |
| Epic 官方启动器的安装被 ACL 审计（DP-07）卡死，建议 Epic/GOG/Amazon 走 Legendary、gogdl、nile，不跑官方启动器 | 部分属实 | DP-07 只出现在 Highball 自己的引擎上（最后验证 2026-08-25，`x64-sikarugir10.0_6-r0`，macOS 14.6，M1 Pro），不是 CrossOver；CrossOver 25.0 起官方支持 Epic，26.3.0 还修了下载。DP-07 是单个引擎的 ACL 持久化缺口，不是普遍的墙。已修改摘要、§2 表格和 P1 第 6、7 项：默认用 Legendary（0.21.1，2026-09-08）是产品取舍，官方 Epic 启动器作为兼容选项保留，ACL 持久化要补 [1][22][28] |
| Battle.net 在 Wine 10 正常、Wine 11 下登录区黑屏并有 libcef int3 崩溃；GOG Galaxy 在 Wine 11 崩溃，暗示 Wine 11 基线对启动器是回归 | 部分属实 | Wine 11 的失败来自 Highball 用 CX 26.3 源码自行打包的引擎（`x64-crossover26.3-r8`～`r10`，DXMT，Highball 0.9.12；Battle.net 见 highball#119），不是 CrossOver 26.3 产品；后者官方支持 Battle.net（26.1.0 修安装）和 GOG Galaxy（26.3.0 修复）。GOG recipe 经 2026-09-26 复读，同样是 `x64-crossover26.3-r9`/`r10`（highball#149）。只能说明“DIY 的 Wine 11 + DXMT”会在 CEF 启动器上回归。新增 P2 第 11 项 [1][23][27] |
| 2026-09-03 客户端（buildid 1788400362）启动 15–20 秒后报 `BMainLoop stalled` 并退出，Linux 原生客户端也中招；首次更新后再按一次 Play、登录过一次的前缀就稳定 | 部分属实 | steam-for-linux #13576（2026-09-05 开）确认 Linux 原生客户端的崩溃和时间，删配置目录也无效。Highball 的 Mac/Wine recipe（最后验证 2026-08-24，Sikarugir Wine 10）描述的是另一种模式：首次自更新后约 3 分钟、登录前退出，登录一次后消失。原文把 Linux 的时间和 Mac 的对策混成一条，已拆成两行，并注明两者是否同源未证实 [6][7] |
| CrossOver 26.2 增加“32 位 bottle 警告”，CX 27 计划去掉 32 位 bottle [1] | 属实（来源有误） | changelog [1] 只包含 26.2.0（2026-06-09）“Added additional warnings for 32-bit bottles”。CX 27 去掉 32 位 bottle（同时只支持 Apple Silicon、需 Sonoma 及以上）出自 CodeWeavers 博客 “What's in and what's out for CrossOver 27”（2026-06-11），已改引 [82] |
| CX 26.3 `dlls/ntdll/loader.c` 的 `build_module()` 在加载 `libcef.dll`/`Qt5WebEngineCore.dll` 时调用 `apply_binary_patches()`，加载 `cohtml_Unity3DPlugin.dll` 时调用 `apply_fuzzy_binary_patches()` | 属实 | 核查者复读了源码并确认条件编译范围（前者为 i386 或 x86_64，后者仅 x86_64）和 CW HACK 22434。补充说明：dappermint/winecx 是第三方镜像，不是 CodeWeavers 官方仓库；补丁表内容未核实 [11] |
| GPTK 4 / D3DMetal 4（WWDC26，2026-06，beta，需要 macOS 27，只支持 Apple Silicon）：DX12 约 +10%，RDR2 约 +25% | 部分属实 | AppleInsider（2026-06-17）确认首个 beta 只支持 Apple Silicon（Metal 4），以及创作者用 beta 测得的数字：M3 Max 上 Cyberpunk 2077 DX12 约 +10%，RDR2 约 +25%（+7fps）。“需要 macOS 27” 未经核实：该文只说测试跑在 macOS 27 beta 上，也没找到 Apple 写明最低系统版本的说明。已改 §6 与“风险”，并加入未解问题第 11 项 [68][84] |
