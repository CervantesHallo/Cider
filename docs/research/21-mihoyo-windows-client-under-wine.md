# 米哈游三游戏（原神 / 崩坏：星穹铁道 / 绝区零）Windows 客户端在 Wine/macOS 下的技术现状与反作弊机制

> 调研日期 2026-09-27 · 置信度说明：**[高]** 一手来源（源码、官方商店页或公告、项目 README 或 issue 原文、GitHub API 时间戳）；**[中]** 可信二手来源，或社区用户贴出的一手日志；**[低]** 搜索摘要、SEO 站点或推断。codeweavers.com、support.hoyoverse.com、PCGamingWiki 在本次调研中均返回 403，相关内容只来自搜索摘要，一律按 [中]/[低] 处理。
> 范围：只讨论 Windows PC 客户端，官方云游戏只作为兜底。按用户底线，iOS/iPadOS 客户端和任何 iOS 应用运行器都不在范围内，本报告不评估。社区项目只做高层描述，用于评估风险，**不含任何绕过步骤**。
> 核查更新（2026-09-27）：已并入独立事实核查的结论。正文中凡标 **【核查更正】** 的地方都已按核查结果改写，完整记录见文末“事实核查记录”，有冲突时以该表为准。

## 摘要

- **三款游戏都是 Unity + D3D11 的 64 位游戏，反作弊由两层组成**：进程内的用户态模块 `mhypbase.dll`，加上内核驱动。内核驱动早期是 `mhyprot2.sys`，现在是 `HoYoKProtect.sys`，注册为服务 `HoProtect`，依赖 KMDF 的 `WDFLDR.SYS` [1][23][30][43]。此外还有一层**服务端策略闸门**：同一个客户端能否在 Wine/Proton 下登录，随版本和维护窗口变化 [6][10][22]。
- **Linux 上已经出现“开发商默许”的先例**。绝区零 Steam 版于 2026-06-16 上线，Steam 页面明确写着使用内核级反作弊 HoYoKProtect，但在 Proton 下开箱可玩 [1][4]。HoYoverse 在 2026-04-28 对媒体表示 “players are welcome to run the game on Steam Deck” [2]。原神在 AWACY 上标为 Running：3.8 版（2023-07-05）起“反作弊在全新安装上放行 Proton”[8]，所以绝区零并不是唯一的先例。星铁仍标为 Broken [8]。**【核查补充】** 有三点需要注意。①这句表态只针对 Steam Deck，原话同时说明首发不做专门优化，收集反馈是为了以后的更新；它不是对 Linux 或 Wine 整体的承诺，更不涉及 macOS [2]。②可玩并不稳定：ProtonDB 用户报告称 3.2 版本初期反作弊会在启动时崩溃，有时要重试多次甚至等几天 [5] [低：搜索摘要]；Intel LGA1700 平台登录时卡死（Proton #9288，重复于 #8951）[66][7]。③ProtonDB 2026-09-27 的汇总为 gold，共 138 份报告，最佳单份报告为 platinum [5]。
- **macOS 上没有已知的“零修改”配置**（截至 2026-09-27）。这一点没有找到反例，但也无法证明不存在。社区方案 yaagl 给三款游戏推荐的设置都包含修改性手段。**【核查更正】** 这些推荐设置出自 issue #551（2025-04-14 创建，此后多次更新），与 0.3.20 这个版本无关（0.3.20 只修复了 “invalid version” 错误）[22][20]。原神和绝区零是 “Wine 11.0-1 Crossover + Steam + timeout fix”；星铁是 “Wine 11.0-1 Crossover + launch fix”，或者断网启动、看到 HoYoverse 标志后再联网。FAQ 中另有 “AC Patch” 开关，关闭后网页功能不可用 [19][21][22]。CrossOver 原版在几次更新后都失效过：原神 6.4 因 `ndis.sys` 未实现的函数崩溃，绝区零国服 2.8.0 因 `mhypbase` 递归崩溃 [29][30]。CodeWeavers 官方的说法是原神因反作弊无法运行 [33]。
- **国服最难**。2026-09-22 的 yaagl #763 中，国服原神 7.1.0 在 Wine 下加载 `HoYoKProtect.sys` 时找不到 `WDFLDR.SYS`，`ZwLoadDriver`（服务 `HoProtect`）返回 `c0000142`，`driverError.log` 中为 `initDriver Failed: Error [4,1114,0]`，游戏在渲染前退出 [23]。**【核查更正】** 这是单一用户的报告，而且环境已被修改：开着 yaagl 的 “Steam Patch”，Wine 为 `11.0-1-crossover-signed-experimental`，不是原版 Wine [23]。完全相同的签名在 2026-03-01 的 yaagl #653 中就出现过（原神 6.5，macOS 26.3，wine11.0 DXMT signed，区服未能确认）[67]。#653 和 #763 都作为 #551 的重复关闭。因此它既不是国服独有，也不是 7.1.0 才开始的，而是 6.5 起在 macOS/yaagl 上反复出现的问题。至于“不欺骗反作弊就无解”，那是推断。
- **在上游 Wine/CrossOver 中，驱动无法“按设计运行”；社区 Wine 已经能让它加载，但 Cider 按政策不采用这条路**。上游 Wine master（2026-09-27）中，`ObRegisterCallbacks` 直接返回 `STATUS_SUCCESS` 和假句柄 `0xdeadbeaf`；`PsSetCreateProcessNotifyRoutine(Ex)` 和 `PsSetCreateThreadNotifyRoutine` 是 FIXME 空桩，同样返回成功；`KeStackAttachProcess`/`KeUnstackDetachProcess` 只打印 FIXME。`PsSetLoadImageNotifyRoutine` 则是有实现的 [38]。**【核查更正】** `ndis.sys.spec` 实际只有 276 个导出，不是原稿写的 389 个：其中 271 个是 `@ stub`，只有 5 个 stdcall，而且其中的 `NdisRegisterProtocol` 本身也是 stub 函数 [40]。上游没有 `wdfldr.sys` [41]。**【核查更正】** 原稿说“社区驱动路线不存在”，这不符合事实。Linux 社区的 spritz-wine 已经带有 wdfldr 实现，在绝区零 3.2.0 之前能加载 `HoProtect`，3.2.0 起改为报 `c0000355` [68]。jadeite 的弃用说明也写着带内核驱动的版本 “can now run on Wine” [16]。同一批维护者在 2026-09 还向上游提交了一批 ntoskrnl `Ps*/Mm*/Se*` 实现，并附带 tests [69]。所以技术上并非不可能。但如果安全 API 空转却返回成功，驱动“加载成功”只会让反作弊误以为保护已经生效。Cider 把这种做法视为欺骗，这是 **Cider 的政策判断**，不是技术结论（见 §4.2、P1-3）。
- **合法的兼容工作只有两类**：①修复 Windows API 行为偏差。先例是 Wine `crypt32` 签名属性排序的缺陷：它导致国服原神的 `MHYPBase.dll` 在进入大世界 30–60 秒后访问 `0x1000` 崩溃，社区 Wine 构建（spritz-wine-cachyos 10.0-11+、spritz-wine-tkg 11.9-1+、dwproton 11.0-2+）已修复 [11]。**【核查更正】** 修复时间是 2026 年上半年，不是 2025-10；#572 虽然创建于 2025-10-12，但 crypt32 根因大约在 2026-05 至 06 才写进正文。说这是“与 Windows 行为对齐”的修复属于推断，issue 并没有给出与 Windows 的对照 [11][18]。②把 Wine 环境透明、可识别地暴露给反作弊，由 HoYoverse 自己决定是否放行。能不能玩，最终是 HoYoverse 的策略决定。
- **图形不是瓶颈**。三款游戏都走 D3D11，DXMT 是首选：2024-07 起 DXMT 用特例实现了 GPU skinning 所需的 GS stream-output，原神和绝区零在 DXMT 下可以“开箱即用”（仅指图形层，不代表能过反作弊）[24]，完整的 SO 是 DXMT 1.0 的计划项 [25][26]。**【核查更正】** “M1 8GB 在 1080p 低/中画质下 55–60 fps”出自 macgamerhq 2024-03-26 的文章，当时的 yaagl 走的是 GPTK/D3DMetal 路线，DXMT 还不可用，而且只测了原神 [57]。这组数据不能作为 DXMT 路线或星铁、绝区零的性能依据。文章中确有“游戏锁 60 帧”和“8GB 会卡顿”（文中说卡顿很快消失）；yaagl 建议 16GB [21][57]。
- **结论**：短期内唯一有希望、又不越红线的路线，是**在 Cider 里用 Windows 版 Steam 运行绝区零 Steam 版**（待实测：HoYoverse 为 Proton 开的路径是否也对 macOS Wine 生效，而且不需要伪装 SteamOS。注意 HoYoverse 的公开表态只覆盖 Steam Deck [2]）。原神、星铁以及国服，都需要 HoYoverse 主动配合。Cider 应该做到三件事：体验上先做预检、不让用户撞上反作弊报错；对 HoYoverse 走合作渠道；预检不通过时引导到官方云游戏。

## 详细调研

### 1. 三款游戏的技术画像

| 项目 | 原神 Genshin Impact | 崩坏：星穹铁道 HSR | 绝区零 ZZZ |
|---|---|---|---|
| 当前版本（2026-09） | 7.0（2026-08-12 上线，核查确认）[49][72]；国服 7.1.0 已在 2026-09-22 的报告中出现 [23] | 4.5，4.6 于 2026-09-28 上线 [50] | 3.2（2026-09-09）[51] |
| 引擎 | Unity 2017.4.30f1（崩溃报告标题）[46] [中] | Unity 2019.4 系，小版本未经一手核实 [低] | Unity 2019.4.40f1（CX 论坛贴出的崩溃对话框）[30] [中] |
| 图形 API | D3D11 [中] | D3D11（官方要求）；第三方启动器 Collapse 能切到 DX12，但“新关卡可能崩溃”[48] [低] | D3D11（Steam 页面）[1] [高] |
| 反作弊组件 | 用户态 `mhypbase.dll`（进程内）[11]；内核驱动：2020 年为 `mhyprot2.sys` [43][44]。2026 年的日志中是 `C:\windows\system32\HoYoKProtect.sys`，服务名 `HoProtect`，导入 `WDFLDR.SYS`：原神 6.5（2026-03，#653）[67] 和国服 7.1.0（2026-09，#763）[23] 都出现过 [高：日志] | AWACY 记为 “miHoYo Protect”[8]；有社区资料称还含 HoYoKProtect 和腾讯 ACE [低，未核实]；Linux 上的现行方案依赖“签名相关 workaround”或 signed 构建，由此推断存在模块签名校验 [17][18] | Steam 页面披露 “Uses Kernel Level Anti-Cheat / HoYoKProtect”[1] [高]；`mhypbase` 自带 SEH 处理器，会在 Unity 报错前拦截崩溃 [30] [中] |
| 最低配置 | 5.0 官方公布：GT 1030、8GB 内存；推荐 GTX 1060 6GB、16GB；100GB 存储 [47] [中] | i5、8GB、GTX 650、Win7 64 位；推荐 i7、GTX 1060 6GB（首发期数据）[48] [中] | Win10 64 位、i5 第 7 代、8GB、GTX 970、DX11、75GB；推荐 i7 第 10 代、GTX 1660 [1] [高] |
| 分发渠道 | HoYoPlay（国际服）/ 米哈游启动器（国服）；Epic；**尚未上 Steam**。2026-01 beta 被数据挖掘出 Steam API DLL [52] [低] | HoYoPlay / 米哈游启动器；Epic；PS5 | HoYoPlay / 米哈游启动器；**Steam（2026-06-16）**[1] |

**反作弊加载链（按已公开日志归纳）**：
1. 游戏进程加载 `mhypbase.dll`。该模块会用 `crypt32` 解析数字签名：AAGL #572 的根因分析显示，它在初始化时读取签名属性 [11]。
2. 在需要内核保护的版本或区服上，游戏把驱动放到 `system32`，注册服务 `HoProtect`，再调用 `ZwLoadDriver`（需要管理员权限；HoYoPlay 会请求提权）[23]。
3. 驱动基于 KMDF，导入 `WDFLDR.SYS`。原稿写的是“国服原神 7.0.0 日志中无驱动痕迹，7.1.0 起强制加载”。**【核查更正】** 这只对 #763 那一位用户的国服环境成立。同样的 `WDFLDR.SYS ... not found` 加 `initDriver Failed: Error [4,1114,0]` 早在原神 6.5（2026-03-01，#653）就出现过 [67][23]。可以确定的是：自 6.5 起，macOS/yaagl 上反复出现驱动加载失败。驱动是否加载可能随版本、区服和启动路径变化 [推断]。
4. 另有服务端闸门。社区长期观察到：大版本更新后 Wine/Proton 客户端会有一段时间无法进入，维护结束或数周后恢复。原神 3.5 时期是“等 2 周”[10]；绝区零 Steam 首发当天 Linux 上崩溃，“维护结束后就好了”[6]。yaagl 把这类现象概括为 “Hoyo changed something on their side”[22]。**事实**：这些时间上的规律都有记录。**推断**：HoYoverse 在服务端或配置层控制对 Wine 环境是否放行 [中]。

**官方表态**：2020-09-28，miHoYo 声明 `mhyprot2` 不收集或上传数据，并把“游戏关闭后驱动仍在运行”修正为随游戏退出而卸载 [44] [中]。2022-08，Trend Micro 披露 `mhyprot2.sys` 在签名仍有效的情况下被勒索软件用作杀软终结器（BYOVD）[43] [高]。这一点在 macOS 上没有内核影响，只说明该驱动的能力边界，即任意内核/用户内存读写和 ring-0 结束进程。

### 2. 在 Wine 下的现状（2025–2026）

#### 2.1 Linux

| 游戏 | 状态 | 关键事实（日期） |
|---|---|---|
| 原神 | AWACY：**Running** [8] | 3.5（2023-03-03）“在受限场景放行 Proton”；3.8（2023-07-05）“在全新安装上放行”；备注“可能出现 100% CPU 占用”[8]。GloriousEggroll 2023-03 发现 wine-ge 7-41 未打补丁也能运行 [9]。Wine `crypt32` 的缺陷会导致国服原神进入大世界 30–60 秒后 `MHYPBase.dll` 访问违例，spritz-wine-cachyos 10.0-11+、spritz-wine-tkg 11.9-1+ 和 dwproton 11.0-2+ 已修复 [11][18] [高]。**【核查更正】** 该问题只在国服报告过。#572 创建于 2025-10-12，但修复在 2026 年上半年，不是 2025-10。“每次大版本后封锁 Wine 1–2 周”的说法只见于 SEO 站 [65] [低]，但与 2023 年“等 2 周”的观察 [10] 一致 |
| 星铁 | AWACY：**Broken**，“需要 workaround”，可用 GeForce NOW [8] | jadeite 在 2026-05 前后归档，弃用说明称“目标反作弊的大多数游戏（包括带内核驱动的版本）现在已能在 Wine 上运行”，但 “does not apply to SR yet”[16]。dwproton 11.0-10（2026-08-04）加入 “signature workaround for HSR”，此后 “works out of the box”。维护者说明这是他们的启动器三年来一直在用的方法，“technically a less proper solution than signed”；另一条路线是锁定到他们签名密钥的 signed 构建 [17][18]。**【核查更正】** 原稿写“需要 dwproton 11.0-10”过于绝对。准确说法是：目前 Linux 上的星铁方案都依赖某种签名相关的 workaround 或 signed 构建 |
| 绝区零 | Steam 版：**可玩但不稳定**（ProtonDB gold，138 份报告，最佳单份报告 platinum，2026-09-27 快照）[5] | 2025-08-05：vanilla Wine/Staging 的 CPU 占用约 30%，和 Windows 相当，Proton Experimental 偏高 [7]。2026-06-16 Steam 首发，GOL 测试 Proton 11 “everything works”，推测 “they appear to have enabled support for Linux”[4]。Valve Deck 评级为 Playable [3] [中]。**【核查补充】** ProtonDB 用户报告称 3.2 版本初期反作弊在启动时崩溃，有时要重试多次或等几天 [5] [低：搜索摘要]。Intel LGA1700 平台登录时卡死，见 Proton #9288（重复于 #8951）[66][7]。spritz-wine 此前能加载 `HoProtect`，3.2.0 起改报 `c0000355` [68] |

#### 2.2 macOS

| 方案 | 结论 | 事实（日期） |
|---|---|---|
| CrossOver（原版） | 三款都不稳定：更新后反复坏，官方不修复反作弊问题 | CX 24.0.4 “New HoYoPlay launcher now works”（08 号报告 [1]）；2023-12 原神 4.3 在 CX 上报 “yuanshen.exe has encountered a serious problem”，4.2 正常 [32]；2024-07 绝区零首发期可在 CX 24.0.4+ 运行 [35]；2025-11-28 起绝区零在 CX 上“更新后无法启动”[31]；2026 年初原神 6.4（2026-02-25 上线）国服和国际服都无法启动，CX、yaagl、Heroic 表现相同，CX 下是 `ndis.sys` 未实现函数触发硬崩溃 [29]；2026-05-06 绝区零国服 2.8.0 在 CX 26.1 上启动即崩，D3DMetal 和 DXVK 都一样，`mhypbase` 递归，Unity 崩溃对话框一闪而过，工单 #1544104 [30]。CodeWeavers 兼容库为三款游戏的国际版和中国版都建了页面，但评级无法获取（403）[28]。官方博文（2026-08-31）称原神因反作弊无法运行，个别内核反作弊能用只是 “very happy accident”[33] |
| Whisky（2025-05 已归档） | 文档评级 Silver，但原神启动器需要额外参数 | 文档（2024-11-01）：启动器黑屏，需在 bottle 参数中加 `--in-process-gpu`；纳塔地面出现彩虹纹理；“Some users have reported their accounts getting banned”[27] |
| yaagl（macOS，活跃） | 三款均“可玩”，但**依赖修改性手段** | **【核查更正】** 0.3.20（2026-09-25T02:49Z）只修复了 “invalid version” 错误；同一天发布的 0.3.19 修复了 “HSR model issues”，并把 DXMT 升级到 654f547 [20]（原稿称 654f547 修复了 macOS 27 HUD 问题，核查未能复核）。推荐设置出自 #551，与具体版本无关 [22]。0.3.18（2026-06-17）支持绝区零 3.0+ 和星铁 4.3+；0.3.15（2026-05-28）新增 “wine 11.0 signed”（原神用）、“wine 11.0-1 crossover”（星铁、绝区零用）、“通过 steam stub 启动绝区零”、“timeout fix”、“让国服重新可玩的 wine 补丁（might break anytime）”[20]。FAQ 写明有 “AC Patch” 开关，关闭后网页功能不可用；“there's always a risk of ban”，但尚未观察到 Mac 玩家被封 [21] |
| GPTK / D3DMetal | 只作为星铁的替代渲染器 | hsr-wine-d3dmetal：Wine 11.17 + GPTK 4.0b2 D3DMetal，面向 yaagl 星铁国际服，要求 macOS 26.4+，“只在 macOS 27 上实测过”[37] |
| ForgePlay（Steam 游戏套壳） | 绝区零 Steam 版失败（**非干净环境**） | 2026-09-20：HoYoPlay（`HYP.exe`）在 D3DMetal 下一直白屏，不会交给 `ZenlessZoneZero.exe`；直接启动 exe 则出现 Unity 崩溃；已标记 DEFERRED [36]。**【核查更正】** 这次尝试启用了 ForgePlay 的 “NVIDIA identity injection”，也就是 GPU 身份伪装（issue 标题为 “HoYoPlay white screen under D3DMetal NVIDIA”）。因此它不能作为无伪装 Wine 环境的基线，也不能直接说明失败原因是 CEF [36] |
| Heroic / Sikarugir / Highball | 没有 HoYo 专项数据 | Highball DB 目录抽查未见 HoYo 条目 [低]；Heroic 在 Linux 上可用 Proton 10 运行 HoYoPlay 版绝区零 [6] |

#### 2.3 失败模式分类（Cider 预检和诊断要识别的就是这些）

| 阶段 | 现象或报错 | 技术原因 | 性质 | 来源 |
|---|---|---|---|---|
| 启动器 | HoYoPlay 白屏或顶部白条；旧启动器报 `QtWebEngineProcess.exe` 错误 | 推断为 CEF/QtWebEngine 在 winemac 下的 GPU 进程与跨进程呈现问题（见 18 号报告）。**【核查更正】** [36] 是在 GPU 身份伪装下得到的结果，不能单独证明原因是 CEF | Wine 兼容性，可以正当修复 | [4][27][36] |
| 反作弊初始化（驱动） | `import_dll Library WDFLDR.SYS ... not found` → `ZwLoadDriver ... Services\HoProtect: c0000142` → `driverError.log` 中 `initDriver Failed: Error [4,1114,0]`，渲染前退出。**【核查更正】** 原神 6.5（2026-03，#653）起在 macOS/yaagl 上反复出现，不只是国服 7.1.0 | 上游 Wine 和 CrossOver 系没有 KMDF（`wdfldr.sys` 不存在）；1114 即 `ERROR_DLL_INIT_FAILED`。社区的 spritz-wine 已有 wdfldr 实现 [68] | 在上游 Wine 中属于内核能力缺失；用 KMDF 宿主让驱动“加载成功”，按 Cider 政策视为红线（见 §4.2） | [23][67][41][68] |
| 反作弊初始化（驱动，社区 Wine） | `ZwLoadDriver failed to create driver ...: c0000355`，同时出现 `fixme:wdfldr:WdfVersionUnbind`（绝区零 3.2.0，spritz-wine-tkg-staging-wow64-11.14-1；核查转述的服务名有 `HoProtect` 和 `HoYoProtect` 两种写法，以原日志为准） | 社区 KMDF 宿主遇到新版驱动 | 只用于识别用户自带的第三方 Wine，Cider 不采用 | [68] |
| 反作弊初始化（驱动） | 点击“开始游戏”后无反应或硬崩溃（原神 6.4） | `ndis.sys` 未实现的导出（上游 276 个导出中 271 个是 `@ stub`） | 内核能力缺失；只有不涉及安全语义的导出才可以诚实实现 | [29][40] |
| 反作弊初始化（用户态） | Unity 崩溃对话框一闪即退，`error.log` 为空（绝区零国服 2.8.0） | `mhypbase` 递归，其 SEH 抢先接管了崩溃 | 原因不明，需要 +seh 日志 | [30] |
| 游戏内 30–60 秒（国服原神） | 静默退出，没有对话框 | Wine `crypt32` 对签名属性排序的缺陷，使 `MHYPBase.dll` 内部指针变成 `0x1000`，在 `mov [rcx], rdi` 处访问违例 | **Wine 缺陷，可以正当修复**（“与 Windows 行为不一致”属于推断，#572 没有给出 Windows 对照）【核查更正】 | [11] |
| 登录 / 更新日 | 客户端显示“Game is running”后立即关闭；维护后仍有数小时不可用 | 服务端 API 未开放，或对 Wine 环境临时不放行 | HoYoverse 策略，只能等待或沟通 | [6][22] |
| 渲染 | 角色、宝箱不渲染（在缺少 transform feedback 的后端上）；星铁 DXMT 下刃等角色模型错误 | GPU skinning 需要 GS stream-output，Metal 没有对应功能 | 图形层，可以正当修复 | [24][20] |

### 3. 社区项目简史与玩家风险（只做高层概述）

| 项目 | 平台与时期 | 解决了什么 | 是否改动反作弊或欺骗环境 | 现状 |
|---|---|---|---|---|
| dawn 补丁（Krock） | Linux，约 2021–2023 | 原神在 Wine 下的反作弊不兼容 | 是（补丁） | 3.5/3.8 以后原神基本不再需要 [8][9] |
| an-anime-game-launcher / the-honkers-railway-launcher / sleepy-launcher | Linux | 下载、Wine 管理、关闭遥测；星铁需要外部补丁 | 部分是 | 维护模式，只做 bug 修复和新版本适配 [13][14][15] |
| jadeite | Linux/macOS，约 2023–2026 | 星铁、崩坏 3 等的“loader-autopatcher” | 是 | 2026-05 前后归档；自述“pre-v1.1.0 SR 的旧补丁 did cause many bans”[16] |
| dwproton / spritz-wine | Linux，2025–2026 | 游戏专项 Wine 修复；一部分已上游到 Wine 11.18（dwproton 11.0-13，2026-09-20）；维护者 NelloKudo 等在 2026-09 向上游提交 ntoskrnl 实现 | 修复类：crypt32（是否属于忠实性修复是推断）；规避类：星铁“签名 workaround”；spritz-wine 还带 wdfldr/KMDF 宿主，能加载 `HoProtect` 【核查补充】 | 活跃 [11][17][18][68][69] |
| yaagl | macOS，2023 至今 | DXMT + 定制 Wine，一键运行三款游戏 | 是：AC patch、Steam 模式、timeout fix、国服补丁 | 活跃 [19][20][21] |
| Twintail | Linux/Windows（macOS 计划中） | 统一启动器，零遥测 | 未核实 | 活跃 [61] |

- 上表中，“Steam 模式 / steam stub”让非 Steam 安装走 Steam 启动路径；“timeout fix / 网络延迟恢复”在启动阶段拦截网络调用，AAGL #509 用 `LD_PRELOAD` 实现，属于规避检查而不是 Wine 修复 [12]；此外还有“签名 workaround”。这三类都属于**欺骗或规避**，Cider 一律不采用。
- **HoYoverse 的执法记录**：2024-01-08 的 DMCA 针对 GI-Download-Library 及其 111 个 fork，理由是分发游戏包内容，没有涉及 Linux 启动器 [45]。截至 2026-09-27，未找到 HoYoverse 对 Wine/Linux/macOS 启动器发出下架通知或官方表态的一手记录。
- **封号风险**：有据可查的是 2023 年星铁旧补丁引发的封号潮 [16]。Whisky 文档转述“有用户称被封”[27]；yaagl 称“未观察到 Mac 封号，但始终有风险”[21]。可以类比的先例：网易 Marvel Rivals 曾误封 Mac/Linux 玩家，2025-01 撤销（08 号报告 [40]）。**评估**：只用官方客户端、不做修改的玩家风险低，但没有保障；使用改动反作弊或伪装环境的方案，风险明显更高。

### 4. 不改动反作弊能否运行？合法兼容工作的边界

**4.1 今天的答案**
- Linux：**可以，但不稳定**。绝区零 Steam 版 + Proton [4]；HoYoverse 公开表示欢迎的只是 Steam Deck，而且说明首发不做专门优化 [2]。3.2 初期反作弊启动崩溃、LGA1700 登录卡死等问题都有报告 [5][66]。原神 + 近期 Proton，据 AWACY [8]，但受服务端闸门影响。
- macOS：**没有已记录的零修改配置**，同样没有找到反例，但也无法证明不存在。yaagl 的推荐设置（#551）都包含修改性手段 [22]。CrossOver 原版在原神 6.4+、绝区零国服 2.8+ 和星铁上都失败 [29][30][32]。原神 6.5 起 yaagl 上反复出现 `WDFLDR.SYS` 缺失，而且报告者用的就是 signed/crossover 系的修改版 Wine [67][23]。

**4.2 三层分析**

| 层 | 需要什么 | 能否“忠实实现” | 结论 |
|---|---|---|---|
| 用户态（`mhypbase.dll` 等） | Win32/NT API 与 Windows 行为一致：crypt32/wintrust、SEH、线程与内存语义 | **能**。例如 crypt32 签名属性问题，可以先写 Wine conformance test 对照 Windows 验证 [11]。#572 本身没有给出 Windows 对照，是否属于忠实性修复，要等对照测试确认 | 正当，Cider 应当做，并优先提交上游 |
| 内核驱动（`HoYoKProtect.sys`） | KMDF 运行时（`WDFLDR.SYS`、`Wdf01000`）、ndis、以及对象回调、进程/线程/映像通知、跨进程内存、物理内存等**安全语义** | **上游 Wine 做不到；社区 Wine 能让驱动加载，但安全语义能否忠实实现未经证实**。Wine 的驱动只作为普通用户态进程（winedevice）里的代码运行，没有 ring-0（CodeWeavers [33]，Wine 11.18 仍是如此 [42]）。上游现状（2026-09-27）：`ObRegisterCallbacks` 返回成功和假句柄 `0xdeadbeaf`；`PsSetCreateProcessNotifyRoutine(Ex)` 和 `PsSetCreateThreadNotifyRoutine` 是返回成功的空桩；`KeStackAttachProcess`/`KeUnstackDetachProcess` 是空操作；`MmCopyVirtualMemory` 返回 `STATUS_NOT_IMPLEMENTED`；`MmGetPhysicalAddress` 直接把虚拟地址当作物理地址；`PsSetLoadImageNotifyRoutine` 有实现 [38]。`ndis.sys` 共 276 个导出，271 个是 `@ stub` [40]。**【核查更正】** 社区方面，spritz-wine 已有 wdfldr 实现，在绝区零 3.2.0 之前能加载 `HoProtect` [68]；jadeite 称带内核驱动的版本 “can now run on Wine” [16]。上游 2026-09 合入了 `PsGetProcessPeb`、`PsGetContextThread`、`PsReferencePrimaryToken`、`MmGetPhysicalMemoryRanges`、`SeLocateProcessImageName`、`PsGetProcessImageFileName` 等实现，并附带 tests [69]。这些桩的状态会变，要按日期持续跟踪 | 如果只是补上 KMDF 让驱动“加载成功”，安全 API 却空转并返回成功，反作弊得到的就只是空壳保护。**Cider 把这视为欺骗，属于红线。这是政策判断，不是技术不可能。** 如果将来上游提供了带 tests、语义诚实的实现，再按 P1-3 重新评估 |
| 服务端策略 | HoYoverse 对 Wine 环境放行 | 不适用 | 只能透明标识并寻求合作 |

**4.3 红线清单（写进 Cider 的规则）**

| 允许 | 禁止 |
|---|---|
| 有 Windows 对照测试的 API 行为修复，通用、不按反作弊模块名分支，先提交上游 | 修改游戏或反作弊文件，打内存补丁，“AC patch” |
| 保留 `wine_get_version` 等可识别特征，并附带 Cider 构建标识 | 隐藏 Wine 导出（HideWineExports 一类），伪造硬件或机器指纹 |
| 对无法兑现的安全 API 返回诚实的失败码；跟进上游带 tests 的 ntoskrnl 忠实实现 | 为反作弊驱动提供 KMDF 宿主，让安全 API 空转却返回成功（社区 spritz-wine 已有此类宿主 [68]，Cider 不采用） |
| 运行**真正的** Steam 版（在 Windows 版 Steam 中安装和启动） | 让非 Steam 安装伪装成 Steam 启动；设置 `SteamOS=1`、`SteamDeck=1` 之类的伪装变量 |
| 网络按正常状态运行 | 启动时阻断或延迟网络（timeout fix / 离线启动） |
| 失败时给出明确说明，并引导到官方云游戏 | 引导用户使用第三方补丁或启动器 |

**4.4 现实性判断**：技术上，社区已经用 KMDF 宿主让驱动在 Wine 中加载 [68][16]。但在 Cider 的红线之内，让反作弊在 Wine 下“按设计运行”，唯一可接受的路径仍然是 HoYoverse 为 Wine 环境提供专门的运行模式或放行策略【核查更正：原稿写“唯一现实”，现改为“红线内唯一可接受”】。Linux 上这件事看起来已经发生：绝区零 Steam 版可玩，原神“放行 Proton”[4][8]。不过 HoYoverse 的公开表态只覆盖 Steam Deck [2]。Cider 能做的是把自己做成一个忠实、可识别、易于测试的 Wine 环境，让这套策略也能覆盖 macOS。

### 5. 图形与性能可行性（假设反作弊问题已解决）

- **后端选择**：三款游戏都是 D3D11，因此以 **DXMT 为首选**。DXMT 开源可随 Cider 分发，已有 arm64x 构建（PR #209，2026-09-17 合入）[73]（19 号报告）。**【核查更正】** arm64x 只解决 Wine 和 DXMT 这一侧的原生 ARM 问题，游戏本体仍是 x86_64，仍然需要 Rosetta 或 FEX（见下文 CPU 路线），所以不能说它能让这三款游戏“跨越 Rosetta 退场”。星铁把 D3DMetal 作为备选：yaagl 0.3.16 记录了 DXMT 下刃等角色的模型问题，0.3.19（2026-09-25）又修复了“HSR model issues”[20][37]。D3DMetal 只能由用户从 GPTK 自行导入（许可证限制，不随 Cider 分发）。
- **关键特性**：Unity 的 GPU skinning 依赖 GS stream-output，Metal 没有对应功能。DXMT 从 2024-07 起针对“关闭光栅化、非 strip 图元”的情况做了特例实现，原神和绝区零因此开箱即用（仅指图形层，不代表能过反作弊），DXMT 给出的说法是 “consistent 60fps” [24]。完整的 SO 列在 DXMT 1.0 计划中（2026-04-21），issue #28 仍为 open [25][26]。缺少 transform feedback 的后端（DXVK→MoltenVK）会出现角色和宝箱不渲染的问题（CX 论坛摘要）[低]。
- **性能数据**：M1 8GB，1080p 低/中画质，55–60 fps；M1 Max，4K 高画质，60 fps [57] [中]。**【核查更正】** 这组数据只测了原神，来源 2024-03-26 的文章写明当时的 yaagl “uses the Apple Game Porting Toolkit”，即 GPTK/D3DMetal 路线，早于 DXMT 可用。它**不能**作为 DXMT 路线或星铁、绝区零的性能依据，DXMT 路线目前只有 “consistent 60fps” 这一定性说法 [24]。游戏锁 60 帧，解锁帧率需要修改游戏，yaagl FAQ 说明这会增加封号风险，所以 Cider 不提供 [21]。8GB 机型初期会卡顿，文章称卡顿很快消失；yaagl FAQ 建议 16GB [21][57]。安装时需要约 2 倍于游戏大小的空间（原神 100GB 以上）[21][47]。
- **M3 8GB 预期（推断 [低]）**：原神和绝区零在 1080p 低/中画质下有望达到 45–60 fps；星铁和绝区零战斗场景偏重。这一推断没有 DXMT 路线的定量基线，只能靠 P2-1 在测试机上实测。内存压力（统一内存加 Rosetta 加 Wine）是主要风险，必须实测。
- **CPU 路线**：这三款是 x86_64 游戏。macOS 28 之后 Rosetta 只保留“老游戏子集”，届时需要 Engine A（FEX）。`mhypbase` 在 FEX 下的行为，以及 CPU 指纹变化会不会被判为异常，都还未知 [低]。

### 6. 先例与官方渠道

- **EAC/BattlEye**：由开发者主动开启。EAC 需要启用 Linux/Unix 模块，BattlEye 由开发者发邮件开启。两者都只有 Linux 模块，对 Mac 上的 Wine 无效（08 号报告，[53][54][55]）。
- **库洛/ACE**：鸣潮 Steam 版上线约 4 个月后，开发商为 Steam Deck 开放了支持，并据报道只放行 Deck、过滤桌面 Linux [56]。这说明国内厂商愿意为 Wine 环境单独开口子，但开口子的依据是平台身份。
- **HoYoverse**：①绝区零 Steam Deck 表态（2026-04-28）[2]，只针对 Deck，并说明首发不做专门优化，不是对 Linux/Wine 整体的承诺，更不涉及 macOS；②原神 3.5/3.8 期间在 Proton 上悄然放行 [8]；③原神客户端中出现 Steam API DLL（2026-01 beta 数据挖掘，未官宣）[52] [低]。目前**没有公开的反作弊或平台合作计划**。
- **联系渠道**：HoYoverse Help Center（support.hoyoverse.com）、游戏内客服、原神客服邮箱 `genshin_cs@hoyoverse.com` [60] [中]、HoYoLAB；国服走米哈游客服和米游社。尚未找到面向开发者或平台的公开合作入口 [低]。
- **官方云游戏（兜底）**：云·原神已开放网页版桌面端 [58]；云·星穹铁道 2023-11 测试时加入网页端，覆盖 PC 和 Mac [59]；云·绝区零详见 22 号报告。

### 7. 可行性结论（2026-09-27）

| 游戏 × 区服 | Cider 不越红线的可行性 | 依据 | 路线 |
|---|---|---|---|
| 绝区零 · 国际服 · **Steam 版** | **中**（待实测） | Linux 上 Proton 可玩但不稳定 [4][5][66]；HoYoverse 的表态只针对 Steam Deck [2]；macOS 上还没有成功记录。**【核查更正】** 唯一的尝试（ForgePlay #22）开了 GPU 身份伪装，表现为 HoYoPlay 白屏加 Unity 崩溃，不能当作干净基线 [36] | Windows 版 Steam（Cider）+ 绝区零 Steam 版 + DXMT。先在无伪装环境下复现，确认 HoYoPlay 白屏的真实原因，是 CEF 还是其他问题，再针对性修复 |
| 绝区零 · 国际服 · HoYoPlay 版 | 低–中 | Linux 上 Heroic+Proton 可玩 [6]；macOS 上 CX 自 2025-11 起不稳定 [31] | 同一套修复，实测 |
| 原神 · 国际服 | 低（取决于 HoYoverse） | Linux 上放行 Proton [8]；macOS 上 6.4 起失败 [29]，6.5 起 yaagl 上反复出现 `WDFLDR.SYS` 缺失（#653 区服未确认）[67]；社区依赖 Steam 模式 [22] | 忠实性修复（crypt32、ndis 的诚实实现）+ 合作；若上 Steam，比照绝区零 |
| 星铁 · 国际服 | 很低 | Linux 上的现行方案都依赖签名相关的 workaround 或 signed 构建 [17][18]【核查更正】；AWACY Broken [8] | 只走合作 + 云游戏 |
| 三款 · 国服 | 很低 | 原神国服 7.1.0 驱动加载失败（单一用户报告，而且是在 Steam Patch + crossover-signed Wine 的修改环境下）[23]；绝区零 2.8.0 崩溃 [30]；crypt32 缺陷只在国服报告过 [11]；社区依赖“国服补丁”[20] | 云游戏为主，并与米哈游沟通 |

## 对 Cider 的启示与建议

按优先级排列。工作量以“会话”计，1 个会话约等于一个 5 小时配额窗口。

**P0-1 反作弊政策落地（1 个会话）**
- 新建 `docs/policy/anticheat.md`，写入 §4.3 的红线表。在 `recipes/` 的 schema 里加入必填字段 `anticheat.policy`，取值只有 `run-as-designed`。
- 加一个 CI lint（GitHub Actions）：凡是 `anticheat.vendor ∈ {hoyoverse}` 的配方，出现以下任何内容就拒绝合并：`env` 含 `SteamOS`、`SteamDeck`、`SteamAppId` 伪装；`LD_PRELOAD`/`DYLD_INSERT_LIBRARIES`；`network.block`；`HideWineExports`；指向游戏目录的 `file.patch`/`file.replace`。
- 验收：lint 对一组故意违规的样例配方全部报错。

**P0-2 HoYo 预检器 `CiderPreflight/hoyo`（2–3 个会话）**
- 识别：通过 exe（`GenshinImpact.exe`/`YuanShen.exe`、`StarRail.exe`、`ZenlessZoneZero.exe`）和 `config.ini` 或版本文件判断游戏、区服（`os`/`cn`）、版本和渠道（`hoyoplay`/`mihoyo-launcher`/`steam`/`epic`）。
- 状态库条目示例（CC0 数据仓库）：
```json
{
  "id": "zzz-os-steam",
  "match": {"exe": "ZenlessZoneZero.exe", "channel": "steam", "steam_appid": 4162040, "region": "os"},
  "anticheat": {"vendor": "hoyoverse", "kernel_driver": "HoYoKProtect.sys", "policy": "run-as-designed"},
  "status": "experimental",
  "verified": [{"date": "2026-10-xx", "game_version": "3.2", "engine": "cider-wine-11.x", "renderer": "dxmt", "macos": "27.0", "chip": "M3", "ram_gb": 8, "result": "pending"}],
  "on_block": {"message_key": "hoyo.blocked.server_policy", "fallback": "cloud:zzz"}
}
```
- UI 状态分为四档：可玩、实验、暂不可用（版本刚更新，等待验证）、不支持（改用云游戏）。游戏版本号与库里记录不一致时，默认降为“实验”，并提示“新版本通常需要若干天验证”。
- 验收：在 `status ∈ {blocked, unsupported}` 时不启动游戏进程，直接展示说明和云游戏入口。用户不会看到 `initDriver Failed` 这类报错。
- 【核查更正】`WDFLDR.SYS` 缺失和 `initDriver [4,1114,0]` 这组驱动签名，匹配范围要覆盖原神 **6.5 及以后的所有版本、所有区服**（#653、#763）[67][23]，不能只绑定国服 7.1.0。在没有 wdfldr 的 Cider Wine 中，凡是检测到会加载 `HoYoKProtect.sys` 的版本，默认都判为 `blocked`。

**P0-3 诊断采集 `cider diag hoyo`（1 个会话）**
- 只收集日志，不做任何修改。开启 `WINEDEBUG=+module,+ntoskrnl,+seh,+loaddll`，按 §2.3 的规则分类：`DRIVER_IMPORT_MISSING(WDFLDR.SYS)`、`ZWLOADDRIVER_FAIL(c0000142)`、`ZWLOADDRIVER_FAIL(c0000355)`、`INITDRIVER_FAILED`、`UNITY_CRASH_HANDLER`、`SILENT_EXIT_AFTER_WORLD`、`LAUNCHER_CEF_WHITE`。
- 【核查补充】报告里要记录环境是否被修改过：第三方 Wine（`*-signed*`、`*crossover*`、spritz 等）、启动器补丁（Steam Patch、AC Patch）、GPU 身份伪装。#763 和 ForgePlay #22 这两个公开样例都是修改过的环境 [23][36]，不能直接当作 Cider 原版 Wine 的基线。
- 输出脱敏报告，去掉账号和路径。验收：用 §2.3 的公开日志样例（#653、#763、spritz-wine #16、CX 论坛）回放，分类准确率 100%。

**P1-1 绝区零 Steam 路线实验（2 个会话，需要测试机）**
- 步骤：Cider 创建 64 位 bottle，安装 Windows 版 Steam（CEF 设置参考 17、18 号报告），用 Steam 安装绝区零 4162040，渲染器用 DXMT，不设置任何伪装变量，也不做 GPU 身份伪装，记录 P0-3 的日志。【核查更正】macOS 上目前没有干净的基线，ForgePlay #22 开了 NVIDIA identity injection [36]，所以本实验本身就是第一份无伪装基线。
- 需要回答三个问题：①`HoYoKProtect` 服务是否被创建；②是否进入 HoYoPlay 并交给游戏进程；③能否登录并进入游戏。
- 验收：在 M3 8GB 上完成 30 分钟游戏，没有反作弊报错。若失败，形成可复现的报告，作为 P1-4 的材料。

**P1-2 忠实性修复清单（逐项 1 个会话，先提交上游）**
- ① `crypt32` 签名属性排序：先在测试机上用**上游 Wine 11.18 原版**复测国服原神的“进入大世界 30–60 秒后退出”问题。上游在 2026-09 已合入 Dmitry Timoshkov 的一组 PKCS signed message attributes 提交（tests 2026-09-03；`CryptMsgGetParam(CMSG_SIGNER_AUTH/UNAUTH_ATTR_PARAM)` 2026-09-02；83567b4de865 `CMSG_ENCODED_MESSAGE` 2026-09-07）[70]，dwproton 11.0-13（2026-09-20）也称游戏补丁已上游到 Wine 11.18 [18]，但没有来源把它们和 #572 直接对应起来。如果仍然复现，再写 Wine conformance test，在 Windows CI（GitHub Actions windows-latest）上对照，然后修复 [11]。【核查补充】
- ② 如果 ndis 缺失的导出能被诚实实现（不涉及安全语义）就补上，否则保持失败 [29][40]。
- ③ HoYoPlay 和米哈游启动器的 CEF 呈现（白屏、白条），沿用 18 号报告的方案。
- ④ 国服登录页的 WebView 验证码交互。
- 所有修复都不得按反作弊模块名或 exe 名分支。

**P1-3 内核驱动宿主政策（0.5 个会话）**：【核查更正】这是政策选择，不是因为技术上做不到。社区 spritz-wine 已经带有 wdfldr 实现，能加载 `HoProtect` [68]，上游也在陆续合入带 tests 的 ntoskrnl `Ps*/Mm*/Se*` 实现 [69]。Cider 的默认做法是不自行实现或引入 `wdfldr.sys`/KMDF 宿主，只有两种情况例外：①HoYoverse 书面认可；②上游 Wine 合入 wdfldr，而且驱动依赖的安全 API（对象回调、进程/线程通知、`KeStackAttachProcess` 等）都是带 tests 的诚实实现，不再“空转却返回成功”。在 Cider 补丁队列中记录一项 `ntoskrnl-honest-security-apis`，评估把 `ObRegisterCallbacks` 和进程/线程通知改为返回诚实的失败码。启用前必须回归其他依赖这些桩的游戏，例如 GameGuard 下的 Helldivers 2，影响范围确认之前不改动默认行为。每次 Wine 同步时，用脚本重新核对 `ntoskrnl.c` 中这些函数的实现状态，并写入带日期的记录。

**P1-4 与 HoYoverse 的合作包（1 个会话写材料，时机为 P1-1 有结果之后）**：一页中英文技术说明，内容包括：Cider 是什么；环境如何识别（`wine_get_version` 加 Cider 构建号，不隐藏）；我们不做的事（红线表）；请求（把 Proton 的放行策略扩展到可识别的 Cider/macOS 环境，并提供一个测试联系人）；我们能提供的（每个版本的自动化回归、问题 48 小时内响应）。渠道：Help Center 工单、`genshin_cs@hoyoverse.com`、HoYoLAB 开发者反馈帖、米哈游客服（国服）。同时抄送 CodeWeavers，推动与 CrossOver 共用同一套策略。

**P2-1 渲染与性能配置（1–2 个会话）**：三款游戏默认 DXMT，星铁可选 D3DMetal（用户导入）。给 8GB 机型的预设为 1080p、低/中画质、关闭高内存特性，预检时检查可用磁盘空间是否达到游戏大小的 2 倍。验收：M3 8GB 上原神蒙德城区 60 秒平均帧率 ≥ 45 fps，以测试机实测为准。【核查更正】目前没有 DXMT 路线的定量公开数据，2024-03 的 55–60 fps 来自 GPTK/D3DMetal 路线 [57]。这项阈值是目标值，不是已知基线，第一次实测后要校准；星铁和绝区零要分别建立自己的基线。

**P2-2 版本节奏运维**：三款游戏大约每 6 周各更新一次，再乘以两个区服，一年约 50 次变更。状态库要在每次更新后 72 小时内复核，在此之前自动降为“实验”。游戏无法在 CI 中运行，需要人工在测试机上验证（推荐 16GB 以上的测试 Mac）。

**P3 持续跟踪**：原神和星铁是否上 Steam；DXMT 1.0 的 SO；Wine 的 KMDF/ntoskrnl 进展（上游 `dlls/ntoskrnl.exe` 的提交记录 [69]、上游是否出现 `dlls/wdfldr.sys` [41]、spritz-wine 的 wdfldr 进展 [68]）；上游 crypt32 的 signed message 提交与 #572 的关系 [70]；HoYoverse 关于 Linux 或 Deck 的新表态；CodeWeavers 兼容库评级（需要人工在浏览器中查看）。

## 风险

1. **封号风险无法归零**：即使不修改任何东西，HoYoverse 也可能调整策略。产品文案不能承诺“安全”，只能说明“未修改反作弊，风险由 HoYoverse 策略决定”。
2. **策略不确定**：服务端闸门可能只放行 Linux/SteamOS，不放行 macOS Wine，类似库洛只放行 Deck 的做法 [56]。如果是这样，P1-1 会失败，只能依靠合作。
3. **更新频繁导致回归**：一年约 50 次变更，每次都可能坏掉（原神 4.3、6.4，绝区零 2.8 都有先例）。单人维护压力大，预检器和状态库必须默认保守。
4. **国服额外限制**：驱动加载失败（原神 6.5 起在 macOS/yaagl 上反复出现，并不限于国服 [67][23]）、crypt32 缺陷（只在国服报告过 [11]）、实名和验证码流程，短期内基本不可行。
5. **内存和磁盘**：8GB 机型体验上限低；原神 100GB 以上，安装期约需 2 倍空间。
6. **用户流失**：社区方案（yaagl，以及 Linux 上带 KMDF 宿主的 spritz-wine [68]）“能玩”，但依赖修改。Cider 坚持红线，可能在短期内显得“不如社区方案”，需要靠预检的透明度和云游戏兜底来弥补体验。
7. **Rosetta 退场**：2027 年秋以后，x86_64 游戏在 FEX 下的反作弊行为未知。DXMT 的 arm64x 构建 [73] 只覆盖 Wine 和 DXMT 这一侧，不能消除这项风险。

## 未解问题

1. 绝区零 Steam 版在 Proton 下的放行条件：是检测 Steam 启动、检测 Wine、检测 SteamOS/Deck，还是由服务端下发？这决定了 macOS 上的 Cider 能否在不伪装的前提下走通（P1-1）。
2. 原神国际服当前版本是否也加载 `HoYoKProtect.sys`？【核查更正】原稿假设这是国服 7.1.0 起才有的强制加载。实际上 #653（原神 6.5，2026-03-01）就出现了相同的驱动签名，但区服无法确认（API 返回 403，页面没有写明）[67]。需要在测试机上分别对国际服和国服、HoYoPlay 和 Epic 渠道核实。
3. `crypt32` 修复是否已进入上游 Wine？**无法证实**。上游 2026-09 有 Dmitry Timoshkov 的一组 signed message attributes 提交（2026-09-02、09-03、83567b4de865 于 09-07）[70]；dwproton 11.0-13（2026-09-20）称 “Imported all games patches that have now been upstreamed to Wine 11.18”[18]；另见 wine-cachyos PR #136 [71]。这些线索很可能相关，但没有任何来源把它们和 #572 直接对应起来。处理方式：用 Wine 11.18 原版在测试机上复测（P1-2 ①）。
4. 星铁的“签名 workaround”到底针对什么签名校验？dwproton 维护者说它 “less proper than signed”，另一条路线是锁定到其签名密钥的 signed 构建 [18]，由此推断与 Wine 自身模块的签名有关 [推断]。如果是这样，可以通过 HoYoverse 白名单正当解决；但它也可能属于欺骗。没有公开细节，需要谨慎。
5. CodeWeavers 兼容库对三款游戏的当前评级，以及 CrossOver 27 的 FEX 构建下的表现（403，无法获取）。
6. 星铁是否确实同时带有腾讯 ACE 组件 [低]。
7. HoYoverse 是否存在面向平台方的正式合作渠道。
8. 【核查新增】spritz-wine 的 wdfldr 实现如何处理对象回调、进程通知等安全 API，是诚实实现，还是空转并返回成功？这决定了它在 Cider 政策下的定性 [68]。

## 参考来源

1. https://store.steampowered.com/app/4162040/Zenless_Zone_Zero/ — 绝区零 Steam 页面：2026-06-16 发售，披露 “Uses Kernel Level Anti-Cheat / HoYoKProtect”，DX11，配置要求
2. https://www.rpgsite.net/news/20250-zenless-zone-zero-steam-deck-support-zzz-optimization-hoyoverse-launch — 2026-04-28 HoYoverse 表态 “players are welcome to run the game on Steam Deck”（同时说明首发不针对 Deck 专门优化，收集反馈用于后续更新；只针对 Steam Deck）
3. https://www.gamingonlinux.com/2026/04/zenless-zone-zero-is-heading-to-steam-in-q2-2026/ — 2026-04-28；Deck 评级 Playable；HoYoKProtect
4. https://www.gamingonlinux.com/2026/06/zenless-zone-zero-has-arrived-on-steam-and-works-on-linux-steamos/ — 2026-06-17；Proton 11 实测可玩，“appear to have enabled support for Linux”
5. https://www.protondb.com/app/4162040 （API：/api/v1/reports/summaries/4162040.json）— 2026-09-27 快照：gold、138 份报告、bestReportedTier platinum
6. https://steamcommunity.com/app/4162040/discussions/0/561408124904276711/ — 首发日 Linux 崩溃，维护后恢复；Heroic + Proton 10 可运行 HoYoPlay 版
7. https://github.com/ValveSoftware/Proton/issues/8951 — 2025-08-05 绝区零 CPU 占用（vanilla Wine 约 30%）
8. https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/HEAD/games.json — AWACY：原神 Running（3.5/3.8 更新记录）、星铁 Broken
9. https://twitter.com/GloriousEggroll/status/1640571147719954433 — 2023-03 原神在 wine-ge 7-41 上未打补丁可运行（搜索摘要）
10. https://www.gamingonlinux.com/2023/09/ge-proton-8-16-released-with-tweaks-for-genshin-impact-resident-evil-and-wine-upgrades/ — 2023-09-24；评论区“3.5 起等 2 周后放行，3.8 起更新当天放行”
11. https://github.com/an-anime-team/an-anime-game-launcher/issues/572 （API：https://api.github.com/repos/an-anime-team/an-anime-game-launcher/issues/572 及 /comments?per_page=100）— 创建于 2025-10-12（国服原神），updated_at 2026-06-14；正文被改写为 “[Resolved]”，把根因归到 crypt32 签名属性排序 → `MHYPBase.dll` 访问违例；34 条评论（2025-10-18 至 2026-05-17）均未提到 crypt32；修复构建为 spritz-wine-cachyos 10.0-11+、spritz-wine-tkg 11.9-1+、dwproton 11.0-2+（2026 年上半年）
12. https://github.com/an-anime-team/an-anime-game-launcher/issues/509 — 2025-05-30 启动期网络拦截（规避类手段，仅用于风险分类）
13. https://github.com/an-anime-team/an-anime-game-launcher — 维护模式声明
14. https://github.com/an-anime-team/the-honkers-railway-launcher — 星铁 Linux 启动器 README
15. https://github.com/an-anime-team/sleepy-launcher — 绝区零 Linux 启动器 README
16. https://codeberg.org/mkrsym1/jadeite — 弃用说明（“including versions with the kernel driver, can now run on Wine”；“does not apply to SR yet”；旧补丁 “did cause many bans”）
17. https://www.gamingonlinux.com/2026/08/dwproton-11-0-10-released-with-a-fix-for-honkai-star-rail/ — 2026-08 星铁 “signature workaround”
18. https://github.com/dawn-winery/dwproton-mirror/releases （API：https://api.github.com/repos/dawn-winery/dwproton-mirror/releases?per_page=15）— dwproton 11.0-2（crypt32，CN GI/ZZZ）；11.0-10（2026-08-04T20:16Z，HSR signature workaround，“less proper than signed”）；11.0-13（2026-09-20，“Imported all games patches that have now been upstreamed to Wine 11.18”）
19. https://github.com/yaagl/yet-another-anime-game-launcher — yaagl README（DXMT、定制 Wine、风险声明）
20. https://github.com/yaagl/yet-another-anime-game-launcher/releases （API：https://api.github.com/repos/yaagl/yet-another-anime-game-launcher/releases?per_page=8）— 0.3.11–0.3.20（2026-03-24 至 2026-09-25）；0.3.20 只修复 “invalid version”，同日 0.3.19 修复 HSR model issues、DXMT 升级到 654f547
21. https://github.com/yaagl/yet-another-anime-game-launcher/wiki/FAQ — AC Patch 开关、封号风险、16GB 建议、磁盘 2 倍
22. https://github.com/yaagl/yet-another-anime-game-launcher/issues/551 — 创建于 2025-04-14，更新于 2026-09-23；各游戏推荐设置（原神/绝区零 “Wine 11.0-1 Crossover + Steam + timeout fix”，星铁 “Wine 11.0-1 Crossover + launch fix”，与 0.3.20 无关）；“Hoyo changed something on their side”；#653、#763 均作为其重复关闭
23. https://github.com/yaagl/yet-another-anime-game-launcher/issues/763 — 2026-09-22 国服原神 7.1.0（单一用户；环境为 yaagl Steam Patch + Wine 11.0-1-crossover-signed-experimental；作为 #551 的重复关闭）：`WDFLDR.SYS` 缺失、`ZwLoadDriver` c0000142、`initDriver Failed: Error [4,1114,0]`
24. https://github.com/3Shain/dxmt/discussions/9 — 2024-07-30 DXMT：原神、绝区零开箱即用；SO 特例实现
25. https://github.com/3Shain/dxmt/issues/151 — 2026-04-21 DXMT 1.0 计划（SO from GS）
26. https://api.github.com/repos/3Shain/dxmt/issues/28 — SO issue，open，2026-03-10 更新
27. https://docs.getwhisky.app/game-support/genshin-impact.html — Whisky 原神文档（2024-11-01）
28. https://www.codeweavers.com/compatibility/crossover/genshin-impact 、…/genshin-impact-china-edition 、…/honkai-starrail-11 、…/honkai-star-rail-china-edition 、…/zenless 、…/zenless-zone-zero-china-edition — CodeWeavers 兼容库页面（403，评级未取得）
29. https://www.codeweavers.com/compatibility/crossover/forum/genshin-impact?msg=347523 — 原神 6.4 无法启动，`ndis.sys` 未实现函数（搜索摘要）
30. https://www.codeweavers.com/compatibility/crossover/forum/zenless?msg=351959 — 2026-05-06 绝区零国服 2.8.0 在 CX 26.1 上崩溃，`mhypbase` 递归，Unity 2019.4.40f1（搜索摘要）
31. https://www.codeweavers.com/support/forums/general?t=27&forumcurPos=1&msg=340634 — 2025-11-28 起绝区零在 CX 上无法启动（搜索摘要）
32. https://www.codeweavers.com/support/forums/general?t=27;forumcurPos=155;msg=294546 — 2023-12 原神 4.3 在 CX 上报 “serious problem”（搜索摘要）
33. https://www.codeweavers.com/blog/mjohnson/2026/8/31/why-do-most-games-with-anti-cheat-not-work-with-crossover-mac — 2026-08-31 CodeWeavers 反作弊博文（搜索摘要）
34. https://support.codeweavers.com/anti-cheat — CodeWeavers 反作弊支持政策
35. https://www.dexerto.com/tech/how-to-play-zenless-zone-zero-on-macos-m-series-intel-2809003/ — 2024-07-08 绝区零可在 CX 24.0.4+ 上运行（本报告只采用其中 Windows 客户端部分）
36. https://github.com/Facta-Leopard/ForgePlay/issues/22 — 2026-09-20 绝区零 Steam 版在 macOS 上 HoYoPlay 白屏、直接启动 exe 时 Unity 崩溃（D3DMetal，启用了 NVIDIA identity injection，非干净环境）
37. https://github.com/dbc-hbin/hsr-wine-d3dmetal — Wine 11.17 + GPTK 4.0b2 D3DMetal（星铁）
38. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntoskrnl.exe/ntoskrnl.c — 2026-09-27 读取（共 5125 行）：`ObRegisterCallbacks`（第 3382 行，`UlongToHandle(0xdeadbeaf)` + `STATUS_SUCCESS`）、`PsSetCreateProcessNotifyRoutine(Ex)`/`PsSetCreateThreadNotifyRoutine`（FIXME stub 返回成功）、`KeStackAttachProcess`/`KeUnstackDetachProcess`（只打印 FIXME）、`MmCopyVirtualMemory`（`STATUS_NOT_IMPLEMENTED`）、`MmGetPhysicalAddress`（虚拟地址当作物理地址）、`PsSetLoadImageNotifyRoutine`（有实现，调用 Ex 版本）
39. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntoskrnl.exe/ntoskrnl.exe.spec — 导出表
40. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ndis.sys/ndis.sys.spec — 【核查更正】276 个导出（原稿误作 389）：271 个 `@ stub`，5 个 stdcall（NdisAllocateMemoryWithTag、NdisAllocateSpinLock、NdisInitUnicodeString 转发给 ntdll、NdisRegisterProtocol、NdisSystemProcessorCount），其中 NdisRegisterProtocol 本身也是 stub 函数；文件最后修改于 2016 年
41. https://api.github.com/repos/wine-mirror/wine/contents/dlls/wdfldr.sys — 404（上游没有 wdfldr.sys）
42. https://www.gamingonlinux.com/2026/09/wine-11-18-released-with-more-ntoskrnl-support-for-kernel-drivers/ — 2026-09-21 Wine 11.18
43. https://www.trendmicro.com/en_us/research/22/h/ransomware-actor-abuses-genshin-impact-anti-cheat-driver-to-kill-antivirus.html — 2022-08 `mhyprot2.sys` 的能力与 BYOVD 滥用
44. https://www.dualshockers.com/genshin-impact-spyware-controversy-mihoyo-explained/ — 2020-09 miHoYo 声明及驱动随游戏卸载的修正（二手）
45. https://github.com/github/dmca/blob/master/2024/01/2024-01-08-hoyoverse.md — 2024-01-08 HoYoverse DMCA（GI-Download-Library，111 个仓库）
46. https://learn.microsoft.com/en-us/answers/questions/3845733/genshin-impact-(version-unity-2017-4-30f1-(0))-uni — 原神 Unity 2017.4.30f1（用户崩溃报告）
47. https://finance.sina.com.cn/tech/roll/2024-08-26/doc-inckyerc6403375.shtml — 原神 5.0 官方配置要求
48. https://www.pcgamesn.com/honkai-star-rail/system-requirements — 星铁配置要求（搜索摘要）；另见 https://github.com/CollapseLauncher/Collapse （DX12 选项）
49. https://store.steampowered.com/news/group/6070552/view/668372420911957899 — 原神 7.0 于 2026-08-12 上线
50. https://www.rpgsite.net/news/21440-honkai-star-rail-version-4-6-update-release-date-zzz-collaboration-razer — 星铁 4.6 于 2026-09-28 上线
51. https://www.notebookcheck.net/Zenless-Zone-Zero-Version-3-2-launches-Sept-9-with-new-Armorer-class.1391587.0.html — 绝区零 3.2 于 2026-09-09 上线
52. https://x.com/Pirat_Nation/status/2048782219217711319 — 原神客户端出现 Steam API DLL（数据挖掘，未官宣）[低]
53. https://partner.steamgames.com/doc/steamdeck/proton — Valve：EAC/BattlEye 在 Proton 下的开启方式
54. https://www.gamingonlinux.com/2022/01/easy-anti-cheat-gets-much-simpler-for-proton-and-steam-deck/ — 2022-01 EAC 支持 Proton 无需重新编译
55. https://www.gamingonlinux.com/2021/11/supporting-linux-proton-and-the-steam-deck-with-battleye-is-just-an-email-away/ — BattlEye 只需一封邮件
56. https://www.dbltap.com/news/kuro-games-confirms-working-on-wuthering-waves-steam-deck-compatibility — 库洛为鸣潮开启 Steam Deck 支持（报道称只放行 Deck）
57. https://www.macgamerhq.com/virtualization/genshin-impact-on-mac/ — 2024-03-26 M1/M1 Max 原神帧率（当时的 yaagl “uses the Apple Game Porting Toolkit”，即 GPTK/D3DMetal 路线；只测了原神，不适用于 DXMT）
58. https://ys.mihoyo.com/main/m/news/detail/28832 — 云·原神网页版桌面端开放
59. https://www.gamersky.com/news/202311/1668414.shtml — 2023-11-09 云·星穹铁道测试新增网页端（PC/Mac）
60. https://genshin.hoyoverse.com/m/en/news/detail/19431 — 原神客服邮箱变更公告（搜索摘要）
61. https://twintaillauncher.app/ — Twintail 启动器
62. https://appleinsider.com/articles/25/01/04/netease-reverses-bans-on-macos-linux-players-of-marvel-rivals — 网易撤销对 Mac/Linux 玩家的误封（同 08 号报告 [40]）
63. https://steamcommunity.com/app/4162040/discussions/0/561408124904296075/ — 玩家对 HoYoKProtect 行为的讨论（随游戏运行）[低]
64. docs/research/08-launchers-anticheat-drm-games.md、18-chromium-embedded-browsers-on-winemac.md、19-dx12-open-path-and-d3dmetal4.md — 内部报告（反作弊总表、CEF、DXMT arm64x）
65. https://caniplayonlinux.com/games/genshin-impact/ — “更新后封锁 1–2 周”的说法（SEO 站，未引用官方来源）[低]
66. https://github.com/ValveSoftware/Proton/issues/9288 — 绝区零在 Intel LGA1700 平台登录时卡死（重复于 #8951）【核查新增】
67. https://github.com/yaagl/yet-another-anime-game-launcher/issues/653 — 2026-03-01 原神 6.5（macOS 26.3，wine11.0 DXMT signed）：HoYoKProtect.sys 加载失败（`WDFLDR.SYS` 缺失）、`initDriver Failed: Error [4,1114,0]`；区服未确认；作为 #551 的重复关闭【核查新增】
68. https://github.com/NelloKudo/spritz-wine/issues/16 — 2026-09-09 绝区零 3.2.0 在 spritz-wine-tkg-staging-wow64-11.14-1 下 `ZwLoadDriver ... c0000355`，日志含 `fixme:wdfldr:WdfVersionUnbind`；报告人称此前一直可用（spritz-wine 带 wdfldr 实现）【核查新增】
69. https://github.com/wine-mirror/wine/commits/master/dlls/ntoskrnl.exe — 2026-09-04 至 09-15 NelloKudo、BananaWorks07 的 ntoskrnl 提交（PsGetProcessPeb、PsGetContextThread、PsReferencePrimaryToken、MmGetPhysicalMemoryRanges、SeLocateProcessImageName、PsGetProcessImageFileName 等，附 tests）【核查新增】
70. https://github.com/wine-mirror/wine/commits/master/dlls/crypt32 — 2026-09 Dmitry Timoshkov 的 PKCS signed message attributes 提交（tests 2026-09-03；CryptMsgGetParam CMSG_SIGNER_AUTH/UNAUTH_ATTR_PARAM 2026-09-02；83567b4de865 CMSG_ENCODED_MESSAGE 2026-09-07）；与 #572 的关系未证实【核查新增】
71. https://github.com/csdivad/wine-cachyos/pull/136 — wine-cachyos 相关 PR（crypt32 上游化线索，与 #572 的对应关系未证实）【核查新增】
72. https://beebom.com/genshin-impact-7-0-release-date-and-time-countdown-timer/ — 原神 7.0 “Everwinter Without Mercy” 于 2026-08-12 上线【核查新增】
73. https://github.com/3Shain/dxmt/pull/209 — DXMT ci/arm64x，2026-09-17 合入【核查新增】

## 事实核查记录

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 绝区零 Steam 版 2026-06-16 发售，披露 HoYoKProtect 内核反作弊 | 属实 | Steam 页面 [1]；个别媒体写 06-17，是时区差异 |
| HoYoverse 称欢迎玩家在 Steam Deck 上运行绝区零 | 属实 | RPG Site 2026-04-28 原文引语 [2]；GOL 转述 [3] |
| 国服原神 7.1.0 在 Wine 下报 `initDriver Failed: Error [4,1114,0]` | 日志属实；定性部分属实 | yaagl #763 原始日志 [23]，只有一份报告。环境与“首次出现”的定性见下方独立核查第 2、7 行 |
| 上游 Wine `ObRegisterCallbacks` 返回成功和假句柄 | 属实 | 2026-09-27 直接读取 master 源码 [38] |
| `wdfldr.sys` 不在上游 Wine 中 | 属实 | GitHub contents API 返回 404 [41] |
| dwproton/yaagl 等发布日期 | 更正 | WebFetch 渲染的 GitHub release 页年份显示为 2024，与内容（Wine 11、Proton 11.0-20260910）矛盾。以 GOL 报道日期（2026-08）和 GitHub API 时间戳为准 |
| 原神“每次大版本后封锁 1–2 周” | 未证实 [低] | 只见于 SEO 站 [65]；与 2023 年 GOL 评论中的“等 2 周”一致 [10]，但没有 2025–2026 年的一手记录 |
| 星铁 Unity 版本为 2019.4.34f1 | 未证实 | 只有搜索摘要，正文降级为“2019.4 系” |
| （独立核查 1）绝区零 Steam 版（4162040，2026-06-16）披露 HoYoKProtect，但在 Linux/Proton 下可玩；HoYoverse 2026-04-28 表示欢迎在 Steam Deck 上运行 | 属实（补充上下文） | Steam 页（2026-09-27）、RPG Site、GOL 2026-06-17、ProtonDB API（gold/138/best platinum）均确认 [1][2][4][5]。补充三点：①表态只针对 Steam Deck，并说明首发不做专门优化，不是对 Linux/Wine 整体的承诺，更不涉及 macOS；②可玩性不稳定：3.2 初期反作弊启动崩溃（ProtonDB 用户报告，搜索摘要）、Intel LGA1700 登录卡死（Proton #9288，重复于 #8951）[5][66][7]；③“唯一先例”言过其实，原神 3.8（2023-07-05）起已 “allowing Proton on clean installs” [8]。已改：摘要、§2.1、§4.1、§4.4、§6、§7 |
| （独立核查 2）国服原神 7.1.0 在 Wine 下 HoYoKProtect.sys 找不到 WDFLDR.SYS → ZwLoadDriver(HoProtect) c0000142 → initDriver [4,1114,0]，渲染前退出 | 部分属实 | 日志逐行属实 [23]。更正：①#763 是单一用户报告，环境为 yaagl “Steam Patch” + Wine 11.0-1-crossover-signed-experimental，属于修改过的环境；②同一签名在 #653（2026-03-01，原神 6.5，macOS 26.3，wine11.0 DXMT signed）就已出现 [67]，#653 和 #763 都作为 #551 的重复关闭；③spritz-wine 已有 wdfldr 实现，3.2.0 之前能加载 HoProtect [68]，jadeite 称带内核驱动的版本 “can now run on Wine” [16]；“被内核驱动硬性卡死”只对上游 Wine 和 CrossOver 系成立；“不欺骗就无解”是推断。已改：摘要、§1、§2.3、§4.1、§7、P0-2、P0-3、风险 4 |
| （独立核查 3）上游 Wine：ObRegisterCallbacks 返回成功 + 0xdeadbeaf；Ps*Notify 是空桩；KeStackAttachProcess 是空操作；无 wdfldr.sys；ndis.sys 有 389 个导出，约 5 个已实现 | 部分属实 | ntoskrnl 部分全部属实（ntoskrnl.c 共 5125 行，ObRegisterCallbacks 在第 3382 行；KeUnstackDetachProcess 同为空操作；MmCopyVirtualMemory 返回 NOT_IMPLEMENTED；MmGetPhysicalAddress 虚实等同）[38]；无 wdfldr.sys [41]。**更正**：ndis.sys.spec 共 276 个导出，不是 389 个；其中 271 个 `@ stub`，5 个 stdcall，NdisRegisterProtocol 本身也是 stub 函数 [40]。PsSetLoadImageNotifyRoutine 有实现，不是空桩。上游 2026-09 由 NelloKudo 和 BananaWorks07 合入了一批 Ps*/Mm*/Se* 实现并附 tests [69]，桩的现状需要按日期跟踪。已改：摘要、§4.2、参考 [38][40]、P1-3、P3 |
| （独立核查 4）macOS 无已记录的零修改方案；yaagl 0.3.20 推荐设置为原神/绝区零 Steam 模式 + timeout fix、星铁 launch fix，另有 AC Patch；星铁在 Linux 上需要 dwproton 11.0-10 的 signature workaround | 部分属实 | 0.3.20（2026-09-25T02:49Z）只修复了 “invalid version”；推荐设置出自 #551（2025-04-14 起），与 0.3.20 无关 [20][22]。原文：原神/绝区零 “Wine 11.0-1 Crossover + Steam + timeout fix”，星铁 “Wine 11.0-1 Crossover + launch fix (or cut Internet...)”；AC Patch 开关见 FAQ [21]。dwproton 11.0-10（2026-08-04）之后星铁 “works out of the box”，维护者称 “less proper than signed”，另有锁定其签名密钥的 signed 构建路线 [17][18]，所以“需要 dwproton 11.0-10”过于绝对。“macOS 零修改方案不存在”没找到反例，但无法证明。已改：摘要、§1、§2.1、§2.2、§4.1、§7、未解问题 4 |
| （独立核查 5）Wine crypt32 签名属性排序与 Windows 不一致，导致 MHYPBase.dll 进入大世界 30–60 秒后访问 0x1000 崩溃；已在 spritz-wine、dwproton 11.0-2+ 修复（2025-10）；属于 Windows 行为忠实性修复 | 部分属实 | #572 创建于 2025-10-12，国服原神（YuanShen.exe）；正文后来改写为 “[Resolved]”，把根因归到 crypt32 “sorting signing attributes”，时间点在进入大世界 30–60 秒后，`mov [rcx], rdi` AV [11]。**更正**：修复构建为 spritz-wine-cachyos 10.0-11+、spritz-wine-tkg 11.9-1+、dwproton 11.0-2+，时间在 2026 年上半年，不是 2025-10（34 条评论 2025-10-18 至 2026-05-17 均未提 crypt32，updated_at 2026-06-14）[11][18]。“与 Windows 行为对齐”是推断，issue 没有 Windows 对照。只在国服报告过。已改：摘要、§2.1、§2.3、§3、§4.2、§7、P1-2 ①、参考 [11] |
| （独立核查 6）三款都走 D3D11；DXMT 自 2024-07 起特例实现 GS SO，原神和绝区零开箱即用；完整 SO 在 DXMT 1.0 计划中（#28 open）；M1 8GB 1080p 低/中 55–60 fps（锁 60 帧） | 部分属实 | DXMT 部分属实：#9（2024-07-30）特例 hack，“just work out-of-the-box”、“consistent 60fps”，仅指图形层 [24]；#151（2026-04-21）列出 SO from GS；#28 open（2026-03-10 更新）[25][26]。**更正**：55–60 fps 出自 macgamerhq 2024-03-26，当时 yaagl “uses the Apple Game Porting Toolkit”（D3DMetal 路线），早于 DXMT 可用，而且只测了原神，不能作为 DXMT 路线或星铁、绝区零的性能依据 [57]；“锁 60 帧”和“8GB 卡顿”原文确有（文中称卡顿很快消失）。已改：摘要、§5、P2-1、参考 [57] |
| （独立核查 7）国服原神 7.0.0 日志无驱动痕迹，7.1.0 起强制加载 HoYoKProtect；WDFLDR 缺失 → initDriver [4,1114,0] 是国服 7.1.0 的特征 | 部分属实 | 相同签名在 #653（2026-03-01，原神 6.5，macOS 26.3，wine11.0 DXMT signed）就已出现 [67]，所以不是国服 7.1.0 首次强制加载，而是 6.5 起在 macOS/yaagl 上反复出现的问题。#653 区服无法确认（API 403）。预检签名要覆盖 6.5 及以后的所有版本。已改：§1 加载链第 3 条、§2.3、P0-2、未解问题 2 |
| （独立核查 8）“让驱动在 Wine 里按设计运行不现实”；上游无 wdfldr，社区驱动路线不存在 | 部分属实 | 上游确实没有 wdfldr.sys [41]。**更正**：spritz-wine（NelloKudo）已有 wdfldr 实现，日志出现 `fixme:wdfldr:WdfVersionUnbind`，绝区零 3.2.0 之前能加载 HoProtect，3.2.0 起改报 c0000355（#16，2026-09-09，spritz-wine-tkg-staging-wow64-11.14-1）[68]；jadeite 称 “including versions with the kernel driver, can now run on Wine” [16]；同批维护者 2026-09 向上游提交 ntoskrnl Ps*/Mm*/Se* 实现 [69]。技术上并非不可能；是否视为“欺骗”是 Cider 的政策判断。已改：摘要、§2.3、§3、§4.2、§4.3、§4.4、P1-3、P3、风险 6、未解问题 8 |
| （独立核查 9）ForgePlay #22：macOS 上绝区零 Steam 版唯一的尝试卡在 HoYoPlay CEF 白屏，可作为 P1-1 基线 | 部分属实 | #22（2026-09-20）使用 D3DMetal，并启用了 ForgePlay 的 “NVIDIA identity injection”（GPU 身份伪装）；标题为 “HoYoPlay white screen under D3DMetal NVIDIA; Unity crash when launching the game EXE” [36]。这不是干净环境，不能作为无伪装基线，也不能直接说明原因是 CEF。已改：§2.2、§2.3、§7、P0-3、P1-1、参考 [36] |
| （独立核查 10）crypt32 修复是否已进入上游 Wine 11.18 | 无法证实 | 上游 crypt32 2026-09 有 Dmitry Timoshkov 的 signed message attributes 提交（CryptMsgGetParam CMSG_SIGNER_AUTH/UNAUTH_ATTR_PARAM 2026-09-02、tests 2026-09-03、83567b4de865 CMSG_ENCODED_MESSAGE 2026-09-07）[70]；dwproton 11.0-13（2026-09-20）称 “Imported all games patches that have now been upstreamed to Wine 11.18” [18]；另见 [71]。很可能相关，但没有来源与 #572 直接对应。建议用 Wine 11.18 原版在测试机上复测。已改：未解问题 3、P1-2 ①、P3 |
| （独立核查 11）原神 7.0 于 2026-08-12 上线；星铁 4.6 于 2026-09-28 上线；DXMT arm64x 构建 2026-09-17 合入 | 属实 | Beebom、Game8 等 [49][72]；RPG Site 2026-09-21 [50]；DXMT PR #209 [73]。补充：arm64x 只解决 Wine 和 DXMT 侧的原生 ARM 问题，游戏本体仍是 x86_64，仍需 Rosetta 或 FEX。原稿说 arm64x “能跨越 Rosetta 退场”，已据此更正。已改：§1、§5、风险 7 |
