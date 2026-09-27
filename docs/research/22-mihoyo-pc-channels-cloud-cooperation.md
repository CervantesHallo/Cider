# 米哈游三游戏：PC 客户端分发渠道、官方云游戏兜底、原生 Mac 动向与官方合作可能性（不含 iOS 版）

> 调研日期 2026-09-27 · 置信度说明：**[高]** 官方页面、商店页或源码直接确认；**[中]** 可信媒体报道，或多个二手来源一致；**[低]** 单一社区来源、搜索摘要或推断。凡属推断，均标“推断”。
> 范围：按用户底线，官方 iOS/iPadOS 客户端、Mac App Store 的“iPhone 与 iPad App”、PlayCover 以及任何 iOS App 运行器都不在范围内，本报告不研究、不推荐。本报告也不包含任何反作弊或 DRM 规避方法。反作弊机制本身由 21 号报告负责，本报告只记录渠道差异和事实。
> 注：hoyoverse.com、mihoyo.com、miyoushe.com 的多数页面由 JS 渲染，WebFetch 只能取到标题；codeweavers.com 返回 403。凡依赖二手转述的结论，均已降低置信度。

## 摘要

1. **PC 分发渠道（截至 2026-09）**
   - **国际服**：有三条渠道。
     - HoYoPlay 官方启动器。
     - Epic Games Store：原神 2021-06-09 上架，星铁 2023-04-26 上架，绝区零 2024-07-04 首发即上架。
     - **Steam：目前只有绝区零**。2026-06-16/17 上线（App 4162040），与 3.0 版本同日。商店页明示 “Uses Kernel Level Anti-Cheat: HoYoKProtect”，需要绑定 HoYoverse 账号，国区锁区 [12][13][14][15]。
     - 原神、星铁截至 2026-09 **没有 Steam 商店页**，只有数据挖掘线索（KitGuru 2026-01-19）和未确认的 `steam_api` DLL 传闻，没有官方公告 [19][20]。
   - **国服**：国服（服务器）的 PC 版只通过“米哈游启动器”分发，分官服和 B 服。2024-06-17 起旧版启动器停止支持 [1]。**核查更正**：这不等于“中国大陆用户只能用米哈游启动器”。大陆用户个人仍可通过 HoYoPlay 或 Epic 安装国际服，只是 Steam 版绝区零对国区锁区 [15][72][74]。
   - 国服与国际服是两套独立安装的客户端，例如原神的可执行文件分别是 `YuanShen.exe` 和 `GenshinImpact.exe` [26]；账号体系也是两套：国服用米哈游通行证（B 服用 B 站账号），国际服用 HoYoverse ID [27]。（核查更正：[26] 只能证明客户端分离，不涉及账号。）
   - 同一服务器下，各渠道的游戏文件可以互通 [25]。没有证据表明反作弊组件按渠道区分。

2. **与 Mac 最相关的新事实：绝区零 Steam 版在 Linux Proton 下可以运行。** [高/中]
   - GamingOnLinux 的反作弊库在 2026-06-17 把它记为 “Works with Proton”，Steam Deck 评级 Playable [16][17]。
   - HoYoverse 2026-04-28 对 RPG Site 表示：Steam Deck 暂不做专门优化，但欢迎玩家在 Deck 上运行，以便团队收集反馈 [18]。
   - AreWeAntiCheatYet 的记录：原神为 Running，3.5 版起在受限场景放行 Proton，3.8 版起全新安装即放行；星铁为 Broken [21]。**核查补充**：AWACY 还收录了绝区零的非 Steam 版（HoYoPlay 版，反作弊标注为 “miHoYo Protect”），自 2024-07-31 起为 Running。可见绝区零上 Steam 之前就已能在 Proton 下运行 [21]。
   - macOS 上目前没有可信的成功案例。2026-09-20 有人用 ForgePlay（Wine 11.12 + D3DMetal）跑绝区零 Steam 版失败，停在 HYP 白屏或 Unity 崩溃，失败原因与反作弊无直接关系 [9]。
   - **所以对 Cider 来说，核心问题是：同一客户端在 Proton 下被放行，为什么在 macOS 的 Wine 上会弹反作弊错误？** 这个问题只能通过“实验取证加官方沟通”解决，不能靠规避。

3. **官方云游戏**
   - **国服三款都能在 Mac 上用浏览器玩。**
     - 云·原神：有 Windows 客户端，有 Windows 和 macOS 网页端，不提供 macOS 客户端；推荐 Chrome；默认 1080P 60 帧 [31][33][35][36]。
     - 云·星穹铁道：网页端与安卓端 **2024-02-06 正式上线**，支持 Windows 和 macOS；2023-11 起的测试就已覆盖 Mac 网页 [37][38][71]。
     - 云·绝区零：2024-12 上线，网页入口是 `zzz.mihoyo.com/cloud-feat` [40][41]。
   - **国际服（非大陆），核查更正**：HoYoverse 有两款官方云。
     - **Genshin Impact · Cloud**（公测）：开放地区有限，平台为 iOS、Android、PC（Windows），**不包含 macOS** [43][45]。
     - **『ゼンレスゾーンゼロ・クラウド版』（绝区零云版，日本地区）**：2026-02-06 随 Ver.2.6 正式上线，**只能连亚服**。官方公布的平台为 iOS、Android、PC（Windows/Mac含む）；首次登录送 10 小时，30 日通行证 2,600 日元 [75][76][77][79][83]。
     - **绝区零云版的 Mac 形态存疑 [低]**。攻略站列出的 Mac 需求是“M1 以上、macOS 12 以上”[77][83]，这恰好是 Apple 对“在 Apple Silicon Mac 上运行的 iPhone/iPad App”的标准措辞。App Store 搜索摘要显示该 App（id6748930656）为“iPad用に設計。macOSでは未検証”[81][82]。按 App 名称或 “zenless” 在日区 Mac App Store 搜索，也没有找到原生 macOS 版本 [81]。**推断**：所谓 Mac 版很可能就是 iPad App 在 Mac 上运行。按用户底线，**Cider 绝不引导用户走这条路**。只有核实存在官方网站下载的原生 macOS 客户端或网页版后，才能把它作为日本地区的兜底。
     - 没有找到星铁的国际服官方云。
   - 作为补充，GeForce NOW 已收录三款游戏，并有原生 macOS 客户端 [46][47][48]；Xbox Cloud Gaming 收录了原神和绝区零 [28][29]。

4. **原生 Mac**
   - HoYoverse 至今没有宣布任何 macOS 原生版。新作 Nexus Anima、Petit Planet 公布的平台只有 PC 和移动端 [54]。
   - 三款游戏在 Windows on Arm 上都无法启动，原因是内核态反作弊没有 ARM64 驱动 [55]。onarm.net 对原神和星铁点名 mhyprot；对绝区零只写 “HoYoverse kernel anti-cheat with no Arm support”，而绝区零 Steam 版标注的是 HoYoKProtect [12]。这是单一第三方追踪站的结论，不是官方声明 [中]。
   - 需要区分：绝区零日本地区官方云列出了 “Mac” 平台（见第 3 条），但那是云串流客户端，不是原生游戏移植，而且形态可能是 iPad App（不在范围内）。
   - 可参照的先例是库洛《鸣潮》：2025-03-27 在 Mac App Store 上架原生 Mac 版（要求 macOS 12+、M1 及以上），2025-03-25 左右 Tim Cook 到场站台 [85][86]。（核查更正：原引用 [53] 是 2024 年的文章，其中既没有这个日期，也没有提到 Tim Cook。）

5. **合作渠道**
   - HoYoverse 平台扩张的轨迹很清楚：PS、Epic、Xbox、Steam 依次上线。它对 Steam Deck 是“欢迎运行”的态度。
   - EAC 和 BattlEye 的 Proton 支持都需要开发者主动开启 [56]。
   - 网易 2025-01 撤销了对 macOS/Linux 兼容层玩家的误封，是可以引用的先例 [57]。
   - 没有找到 HoYoverse 公开的商务或技术合作入口。可行的路径是：客服工单 + HoYoLAB + 分游戏的媒体联系人 + 行业中间方。

6. **设计结论**
   - Cider 以 Windows PC 客户端为主路线，由“签名裁定库 + 启动前检查”决定能不能启动。**凡是没有验证为可玩的（游戏版本 × 渠道 × 引擎 × macOS）组合，一律不启动游戏 exe**，直接给出云端兜底。兜底分三种情况：
     - 国服：官方网页云。
     - 绝区零的日本地区（亚服）用户：官方云，**但只接受网页版或经过核实的原生 macOS 客户端，不接受 iPad App 形态**。
     - 其他国际服用户：可选的第三方授权云。
   - 这样用户永远不会停在反作弊弹窗上。

## 详细调研

### 1. PC 客户端分发渠道及其对 Wine 的要求

#### 1.1 渠道总表

| 渠道 | 覆盖游戏 | 服务器与账号 | 更新机制 | 反作弊标注 | Mac/Wine 侧的要点 | 置信度 |
|---|---|---|---|---|---|---|
| **HoYoPlay（国际服）** | 原神、星铁、绝区零、崩坏3 [3] | 亚、欧、美、港澳台服；HoYoverse ID | 自带启动器更新，游戏走 Sophon 分块下载 | 与游戏本体相同 | 启动器是自更新的多进程程序（见 1.2）；CrossOver 24.0.4 起支持 [61] | 高/中 |
| **米哈游启动器（国服）** | 同上，另有 B 服 [1][27] | 国服官服 / B 服；米哈游通行证（B 服用 B 站账号，PC 端可扫码登录）[27] | 同上；API 域名为 `hyp-api.mihoyo.com` [5] | 同上 | 国内 CDN 直连，不需要镜像；实名认证和防沉迷由游戏处理 | 中 |
| **Epic Games Store** | 原神 2021-06-09、星铁 2023-04-26、绝区零 2024-07-04 [22][23][24] | 只有国际服（推断，未找到明确说明）；仍需 HoYoverse 账号 | 由 Epic 启动器更新 | 与游戏本体相同 | 需要在 Wine 里跑 Windows 版 Epic 启动器（CrossOver 25.0 起正式支持，见 01 号报告）；与 HoYoPlay 的游戏文件可互通 [25] | 中/低 |
| **Steam** | 三款目标游戏中**只有绝区零**，2026-06-16 上线，App 4162040 [12]（崩坏3 另有 Steam 版 App 1671200，不在范围内 [21]） | 欧、美、亚、港澳台服；**中国大陆锁区** [14][15][74]（Steam appdetails API 用 `cc=cn` 查询返回 `{"success":false}`，`cc=us` 能返回数据 [72]）；首次启动需绑定 HoYoverse ID [14] | 走 Steam depot；不支持经 HoYoPlay 预下载 [14]；下载后游戏内还要再下约 5 GB，并做资源校验 [16] | 商店页：“Uses Kernel Level Anti-Cheat: HoYoKProtect”[12] | 需要在 Wine 里跑 Windows 版 Steam（CEF，见 18 号报告）；运行时仍会拉起 `HYP.exe`、`HYPWorker.exe`、`HYPHelper.exe` [9] | 高/中 |
| Steam（原神、星铁） | 未上架 | — | — | — | 2026-01 起数据挖掘发现 Steam 相关字符串和 `steam_api` DLL，官方未确认 [19][20] | 低 |
| 主机（PS4/PS5/Xbox） | 原神：PS、Xbox（2024-11-20）；星铁：PS5；绝区零：PS5、Xbox（2025-06-06）[28][29][30] | 跨平台存档 | — | — | 不是 PC 渠道；可作为“自有主机串流”的备选（1.4 节） | 高/中 |

**文件布局与标识（按安装形态区分）**

- 国服和国际服是两套可执行文件和数据目录。原神国服是 `YuanShen.exe`，国际服是 `GenshinImpact.exe`。社区工具 GenshinSymlinker 能让三款游戏的国服和国际服共用资源，说明两边的资源大部分相同，差异主要在可执行文件和少量文件上 [26]。该工具只证明客户端分离，与账号体系无关；账号分离的依据见 [27]。
- HoYoPlay 的安装结构：顶层是 `launcher.exe`，负责单实例检查；下面有带版本号的子目录，例如 `HoYoPlay\1.1.4.133\`，里面放 `HYP.exe`、`HYPHelper.exe`（无窗口的后台进程）、`HYPWorker.exe` 和 `crashreport.exe` [8][9]。**推断**：`launcher.exe` 会切换到新的版本子目录，这和 Steam、Battle.net 的自更新方式相同。所以 Cider 应在自更新前给目录做快照，并保留回滚能力。
- **HoYoPlay 的 UI 技术栈（CEF、QtWebEngine、WebView2 还是自研）未确认** [低]。现有线索：Linux 社区反映“装了 WebView2 后黑屏消失”[11]；Whisky 2.3.2（Wine 7.7）在 2024-06-17 出现空白窗口 [10]；ForgePlay（Wine 11.12）在 2026-09 出现白屏 [9]；GamingOnLinux 提到 Proton 下 Steam 版启动器顶部有一条白边 [16]。要在 bring-up 阶段通过查看安装目录确认（见“未解问题”）。

**启动器使用的 API**（公开但未文档化，社区整理）[4][5]

- 国际服基址：`https://sg-hyp-api.hoyoverse.com/hyp/hyp-connect/api/`，launcher_id `VYTpXlbWo8`。
- 国服基址：`hyp-api.mihoyo.com`。
- 接口：`getGames`、`getGameContent`、`getGamePackages`、`getGameConfigs`、`getNotification`、`getGameBranches`。
- 游戏 ID（国际服）：原神 `gopR6Cufr3`，星铁 `4ziysqXOQ8`，绝区零 `U5hbdsT9W7`，崩坏3 `5TIVvvcwtM`。

**Sophon 分块下载**

- 原神从 4.5 版（2024-03）开始使用。
- 下载流程：先调用 `getGameBranches`，拿到 `package_id`、`password`、`tag`；再调用 `getBuild`，拿到清单；清单是 zstd 压缩的 protobuf，里面列出分块；分块逐一下载，按 offset 拼装，每块都有 MD5 校验 [5][6]。
- 大约从 2025 年中起，预下载只提供 Sophon 方式，没有 ZIP 包。第三方 macOS 启动器 YAAGL 因此一度无法更新 [7]。**核查更正**：原文写的是“原神 6.0（2025-05）”，这个组合自相矛盾。YAAGL issue #561 开于 2025-05-07，原文是 “only for Sophon Download system. ZIP is currently not available yet”；而原神 6.0 在 2025-09-10 才上线 [87]。所以 2025-05 那次预下载应该是 5.6 版，issue 标题里的 “6.0” 是后来改的。
- 对 Wine 的要求：大量小文件的并发 IO、zstd 解压的 CPU 开销、先写 staging 目录再 rename 的原子性，以及长路径支持。

**对 Cider 的意义**：`getGameBranches` 返回的 `tag` 就是“当前线上版本号”，可以只读地用于版本监控（第 5 节）。Cider **不自己实现下载器**，安装和更新都交给官方启动器、Steam 或 Epic。理由有两点：一是保证官方完整性校验链完整，二是避免触碰服务条款风险（风险判断属于推断）。

#### 1.2 各组件需要 Wine 提供什么

| 组件 | 需要的 Wine 能力 | Cider 对应项 |
|---|---|---|
| 启动器 UI（HYP.exe，内嵌 Web 视图，技术栈待确认） | 嵌入式浏览器的呈现和 IME，窗口合成，HTTPS/TLS | 18 号报告的启动器回归套件（LRS）加一条 HoYoPlay；渲染器钉在 wined3d 或 DXMT，与游戏分开设置 |
| HYPHelper、HYPWorker 后台进程 | 多进程、命名管道、自更新目录切换 | 进程组管理；退出游戏后回收（防止出现 Whisky/GPTK 那种 wineserver 残留 [W1]） |
| 游戏本体（Unity） | **D3D11**（绝区零 Steam 版的最低和推荐配置都写 “DirectX: Version 11” [12][73]；核查更正：原文误写为 11.1，此更正不影响下一列的结论）；XInput 和 DualSense；音频；IME（聊天） | DXMT 是主后端，**这三款不需要 D3DMetal**（推断，依据 00 号报告的裁定和 DX11 要求） |
| 反作弊组件（mhyprot 系、HoYoKProtect） | 由 21 号报告分析；Cider **原样运行，不修改** | 只做裁定和启动前检查 |
| Steam 或 Epic 客户端（对应渠道） | CEF；sync 模式冲突（00 号报告第 9 条） | 复用 Steam 和 Epic 的现有配方 |

**帧率与 ProMotion**

- 原神 PC 版官方上限 60 fps，一直没有提供 120 fps 选项 [66]。
- 星铁 PC 版的 120 fps 不在菜单里，只能通过改注册表中的游戏自身配置开启（社区资料）[65]。
- 绝区零 PC 版支持不限帧 [64]。
- 因此 ProMotion 实际上只对绝区零（以及可能的星铁）有意义。Cider 应通过 DXMT 的 `d3d11.preferredMaxFrameRate` 统一限帧（见 13 号报告）。**Cider 不自动修改游戏自身的注册表设置**，只在 UI 里给出说明。

#### 1.3 Linux/Proton 与 macOS 的现状对照（只记录事实）

| 游戏 | Linux/Proton 状态 | 依据 | macOS（Wine 系）状态 |
|---|---|---|---|
| 原神 | Running（miHoYo Protect）；3.5（2023-03-03）起在受限场景放行，3.8（2023-07-05）起全新安装放行；AWACY 条目更新于 2024-09-02 | AWACY [21] | 没有可信成功报告；CrossOver 用户普遍遇到反作弊问题（社区转述）[低] |
| 星铁 | Broken（“Workarounds required”）；条目更新于 2025-05-26 | AWACY [21] | 同上 |
| 绝区零（HoYoPlay/非 Steam 版，核查补充） | Running（miHoYo Protect），自 2024-07-31 起 | AWACY [21] | 没有可信成功报告 [低] |
| 绝区零（Steam） | Works with Proton（2026-06-17），Deck 评级 Playable（未获 Verified）；在 Proton 11 上测试可玩；GOL 称 HoYoKProtect “Linux support enabled” | GOL [16][17] | ForgePlay 2026-09-20 失败，停在 HYP 白屏或 Unity 崩溃 [9] |
| 崩坏3 | Broken（ACE）；已上 Steam（App 1671200） | AWACY [21] | 不在范围内 |

社区有两种说法都没有得到核实，只作为待验证假设 [低]：
- “每逢大版本更新后，Linux/Proton 会被限制 1–2 周” [69]；
- “反作弊只在 SteamOS 或 Deck 上放行兼容层”（HoYoLAB 帖子，未能打开核实）。

Cider **绝不能**靠伪装 SteamOS、Deck 或 Proton 来“借用”放行。这是明确的检测规避，违反 R3 的护栏。正确做法是：把差异作为事实记录下来，交给 21 号报告做机制分析，再作为合作请求的核心诉求（第 4 节）。

#### 1.4 其他合法路径（非 iOS、非云）

PS Remote Play 有官方 macOS 客户端，可以串流用户自己的 PS5（原神、星铁、绝区零都有 PS5 版）；9.5.0 版起要求 macOS 12 以上（搜索摘要）[63][低]。它不属于 Cider 的核心路线，只能在路线卡里作为“你有 PS5 时”的外链提示。

### 2. 官方云游戏：可用性、质量与 Cider 封装的可行性

#### 2.1 国服官方云（对 Mac 用户而言最强的兜底）

| 项目 | 云·原神 | 云·星穹铁道 | 云·绝区零 |
|---|---|---|---|
| Mac 上的形态 | **网页端**（macOS 10.10 以上，推荐最新版 Chrome）；**没有 macOS 客户端**，Windows 有客户端 [31][32][36] | 网页端（2023-11 的测试已包含 PC 和 Mac 网页）[38]；网页与安卓账号互通 [39] | 网页端 `https://zzz.mihoyo.com/cloud-feat/` [40]；第三方文章称支持 Chrome 110+、Edge 112+、Safari 17.4+ [41][中] |
| 上线时间 | 安卓 2021-10 [70]；网页桌面端 2023-09-15 [31] | 2023-11 测试；**网页与安卓 2024-02-06 正式上线**，支持 Windows、macOS、安卓、iOS（核查更正，原文写“日期未能取到”）[37][71][88] | 2024-12-18（搜索摘要）[40]；官方网页入口在 2024-12-11 的报道中已出现，但该报道没有写明上线日期 [40] |
| 画质 | 默认“超高清”、60 帧，约 1080P60；码率 2~50 Mbps 自适应（搜索摘要）[34] | 未找到官方数值 | 可按网络情况调整 [41] |
| 免费时长 | 首次登录 5 小时；每日登录 15 分钟；累计上限 600 分钟 [31][34]；有过限时活动（如 2024-08）[35] | **每个版本**登录可得 10 小时，累积上限 10 小时（核查更正：不是“只有首次”）[71] | 首次 10 小时，每日 15 分钟（2026-04 报道）[42] |
| 付费 | 畅玩卡 60 元/30 天 [32][35] | 畅玩卡 30 元/30 天；按时计费为 10 星云币/分钟 [39]；购买“无名勋礼”附赠 42 日畅玩卡 [71] | 未找到 |
| 输入 | 网页端支持键鼠；手柄需要先设置 [33] | — | PC 可直接接手柄 [41] |
| 已知问题 | Safari 可能无法全屏，官方建议换浏览器 [33] | — | — |
| 排队 | 高峰期可能排队（二手）[34] | — | — |

以上价格和时长都来自当期公告或二手转述，经常有活动调整，UI 只展示“以官方为准”并链接官方页面，不写死数字。

#### 2.2 国际服官方云

- **Genshin Impact · Cloud**（公测）的时间线：2023-12-27 在新加坡和马来西亚开放 PC 公测 [44]；2024-06-12/13 开放美国和加拿大公测 [45]。
- 官网列出的地区：美国、加拿大、新加坡、马来西亚、印度尼西亚、泰国、菲律宾、老挝、柬埔寨、孟加拉国、缅甸。平台只有 iOS、Android、PC，**没有 macOS** [43]。
- 价格：Cloud Pass 19.99 美元/30 天；计费 10 Cloud Coins/分钟；免费时长为首次 300 分钟、每日 15 分钟、上限 600 分钟 [44][45]。
- **核查更正：绝区零有非大陆地区的官方云，但只在日本地区** [高/中]。
  - 名称是『ゼンレスゾーンゼロ・クラウド版』，2026-02-06 随 Ver.2.6 正式上线 [75][76][78][79]。
  - **只能连亚服**，不能切换服务器；官方建议在日本国内游玩 [75][83]。
  - 官方 X 账号称 “iOS、Android、PC（Windows/Mac含む）版が正式にリリース” [79]。Google Play 包名是 `com.HoYoverse.cloudgames.Nap` [80]。
  - 首次登录送 10 小时（每个 HoYoverse 账号一次）；30 日通行证 2,600 日元 [77][83]。
- **这个 “Mac 版” 的形态没有得到核实，并且有较强的反面证据** [低]。
  - 攻略站给出的 Mac 需求是 “M1チップ以上、macOS 12以上”[76][77][83]。
  - App Store 上的 iOS 版（id6748930656，COGNOSPHERE PTE. LTD.，2026-02-04 发布，iOS 13.0+）在搜索摘要里显示为 “iPad用に設計。macOSでは未検証”[81][82]。“macOS 12.0 以上、搭载 Apple M1 及以上芯片的 Mac” 正是 Apple 给 iPhone/iPad App 在 Mac 上运行时用的标准措辞。
  - 用 iTunes Search API（`entity=macSoftware`，日区）按 App 名称或 “zenless” 搜索，都找不到原生 macOS 版本 [81]。
  - 有两个来源给出不同说法：note.com 的一篇个人文章（2026-02-06）写的是“从官网下载客户端”或“只用浏览器”[84]；game8 的核查员摘录提到浏览器游玩，但本次复查 game8 页面没有找到浏览器入口 [77]。
  - **推断**：Mac 上目前能找到的主要形态是 iPad App 在 Apple Silicon Mac 上运行。**按用户底线，这条路一律不用、不提。** Cider 只有在核实存在官方原生 macOS 客户端（官网下载、`CFBundleSupportedPlatforms` 为 `MacOSX`）或官方网页版之后，才会把它列为日本地区绝区零用户的官方云兜底。
- 截至 2026-09，**没有找到星铁的国际服官方云** [中，属于“未发现”]。
- 结论（核查更正，按地区区分）：
  - 原神、星铁的国际服 Mac 用户，没有 HoYoverse 官方的云端兜底。
  - 绝区零在日本地区（亚服）有官方云，但它在 Mac 上是否有 Cider 可用的形态，还待核实。
  - 其他地区的绝区零国际服 Mac 用户，没有官方云。

**实验项 E4（推断，值得验证）**：以下几款都是官方程序，本机不运行游戏的反作弊：Genshin Impact · Cloud 的 Windows PC 客户端、云·原神的 Windows 客户端、绝区零云版（日本）的 Windows PC 客户端。它们有可能在 Cider 的 Wine 里跑起来，从而补上国际服 Mac 用户的缺口。原神国际服目前没有其他官方云路线，E4 对它仍是唯一的候选。绝区零云版如果核实后在 Mac 上只有 iPad App 形态（按底线排除），E4 对它同样必要。需要验证的点：视频解码路径（Media Foundation、D3D11 视频解码，还是软件解码；见 09 号报告）、输入延迟、是否检测虚拟化或兼容层。

#### 2.3 发行商授权上架的第三方云（可选，默认折叠）

| 服务 | 收录情况 | macOS 形态 | 限制 |
|---|---|---|---|
| GeForce NOW | 原神 2022-06-23 对全体会员开放（当时 RTX 3080 档在 PC/Mac 应用上可到 4K60）[46]；星铁（Epic 版）2024-05-09 [47]；绝区零 2024-12 [48] | 原生 macOS 应用 | 2026 年起每月 100 小时上限（14 号报告）；中国大陆无服务（推断） |
| Xbox Cloud Gaming | 原神 2024-11-20，Xbox Wire 当时说明云端游玩需要 Game Pass Ultimate [28]；绝区零 2025-06-06 [29] | 浏览器（Safari、Chrome、Edge）[67][中] | 2026 年的订阅档位规则有变化（二手）[67]；中国大陆无服务（推断） |

这些服务运行的都是发行商授权的官方版本，反作弊完整保留，但不属于“米哈游官方云”。按 R3 的要求，只作为可选的第二兜底：默认折叠，由用户在设置中开启。

#### 2.4 Cider 原生云封装的技术可行性

| 方案 | 优点 | 问题 | 结论 |
|---|---|---|---|
| A. 用系统默认浏览器打开官方 URL | 零维护、兼容性最好 | 体验割裂 | **v1 必备的兜底** |
| B. 用已安装的 Chrome/Edge 以 `--app=<url>` 窗口打开 | 云·原神官方推荐 Chrome [31]；无边框、可全屏；Pointer Lock 和 Gamepad 原生可用 | 依赖用户已安装 Chrome/Edge | **v1 首选** |
| C. Cider 内置 WKWebView 窗口 | 与 Cider 界面统一；零下载 | 元素全屏需要 `WKPreferences.isElementFullscreenEnabled`（macOS 12.3+）[50]；**Pointer Lock 只能通过私有委托 `_webViewDidRequestPointerLock:completionHandler:` 实现**（cmux 2026-09-25 PR；如果不实现这个回调，WebKit 会直接拒绝；fused-render-lite PR #20 记录了同一组 `WKUIDelegatePrivate` 选择器，Safari 也用它们）[51][89]；Gamepad API 需要用户手势唤醒 [52]；云·原神 FAQ 提示 Safari 系可能无法全屏 [33] | P2 做原型，只对明确支持 Safari 的云·绝区零 [41] 默认启用 |
| D. 内置 Chromium（CEF/Electron） | 行为一致 | 体积大，需要每月跟进安全更新 | 不做 |

**可接受性边界**（Cider 自己的规则）：

- 只加载官方 URL，原样呈现。
- 不注入脚本，不自动登录，不抓取或存储凭据，不改 UA（除非官方文档要求某个浏览器）。
- 只提供窗口相关能力：全屏、防休眠（`IOPMAssertion`）、网络预检（到云节点的 RTT 和带宽）、“返回 Cider”按钮。

**120 Hz**：国服官方云的上限是 60 帧 [34]，ProMotion 不起作用。Cider 只需保证窗口不强制限帧、不插帧。

### 3. 原生 macOS 版的动向

- **官方现状**：HoYoverse 没有发布或宣布任何原生 macOS 版游戏 [中，属于“未发现”]。
  - 唯一与 Mac 有关的官方产品是绝区零日本地区云版列出的 “Mac” 平台（2026-02-06）[75][79]。它是云串流客户端，不是原生游戏移植。已知的 Mac 形态疑似 iPad App，按底线不在范围内（见 2.2 节）。
- 截至 2026-09，各游戏公布的平台：
  - 原神：PC、PS4/PS5、Xbox Series、移动端 [28]；
  - 星铁：4.6 版本（2026-09-28）写明 PC、PS5、iOS、Android [30]；
  - 绝区零：PC（HoYoPlay、Epic、Steam）、PS5、Xbox、移动端 [12][29]。
- 新作 Honkai: Nexus Anima 和 Petit Planet 公布的平台都是 PC 加移动端 [54]。
- HoYoLAB 上有大量“要 Mac 版”的玩家帖，没有找到官方正面回复。
- **侧面信号**：三款游戏在 Windows on Arm 上都被反作弊拦住 [55]。
  - onarm.net 2026-06-06 的记录：原神、星铁是 mhyprot 内核反作弊，没有 Arm64 驱动；绝区零只写 “HoYoverse kernel anti-cheat with no Arm support”。
  - 绝区零 Steam 版标注的是 HoYoKProtect [12]。所以“mhyprot 系”这个说法对绝区零并不准确（核查更正）。
  - 这是单一第三方追踪站的结论 [中]。**推断**：HoYoverse 目前没有投入非 x86 的 PC 平台。这也意味着 2027 年 Rosetta 收缩后（00 号报告第 4 条），这三款游戏会同时面临 CPU 转译和反作弊架构两道门槛。
- **行业先例**：库洛《鸣潮》2025-03-27 在 Mac App Store 上架原生 Mac 版（macOS 12+、M1 及以上），2025-03-25 左右 Tim Cook 到场站台 [85][86][中]。（核查更正：原引用 [53] 是 2024 年的文章，只说 Mac 版 “on the way”，不能支持这两个日期。）**推断**：如果将来出现米哈游原生 Mac 版，大概率是通过与 Apple 合作（WWDC 或 Apple 发布会）的形式。Cider 在 schema 里预留 `native` 路线，一旦检测到就直接推荐。

### 4. 合作：先例、机制与一份可信请求的样子

#### 4.1 HoYoverse 平台扩张先例

| 时间 | 事件 | 来源 |
|---|---|---|
| 2021-06-09 | 原神上架 Epic | [22] |
| 2022-06-23 | 原神上线 GeForce NOW（经过限量 beta） | [46] |
| 2023-04-26 | 星铁首发 PC、Epic 和移动端，PS5 版 2023-10-11 | [23] |
| 2024-06 | 统一为 HoYoPlay / 米哈游启动器 | [1][10] |
| 2024-07-04 | 绝区零首发 PC、Epic、PS5 和移动端 | [24] |
| 2024-11-20 | 原神上线 Xbox Series 和 Xbox Cloud | [28] |
| 2025-06-06 | 绝区零上线 Xbox | [29] |
| 2026-02-06 | 绝区零云版在日本地区正式上线（亚服；iOS、Android、PC（Windows/Mac含む）） | [75][79] |
| 2026-04-28 | 表态：Steam Deck 不做专门优化，但欢迎玩家运行并反馈 | [18] |
| 2026-06-16/17 | 绝区零上线 Steam；Proton 下可运行 | [12][16][17] |

**解读（推断）**：HoYoverse 的平台策略是“商店和主机优先，兼容层默许”。它对兼容层用户的公开姿态是欢迎和收集反馈，而不是封禁，这是提出请求的基础。

#### 4.2 其他反作弊和发行商如何为 Wine/Proton 开门

- **EAC**：开发者在 EAC 后台启用 Linux 客户端平台，激活 Unix 模块，把 Linux 库改名为 `easyanticheat_x64.so`，放进 depot，与 Windows 库并列，再发布新 build [56]。
- **BattlEye**：每个游戏都要手动配置，由开发者联系 Valve 或 BattlEye [56]。
- Valve 的建议是尽量使用用户态反作弊组件。它对内核态方案的原话是 “not currently supported and are not recommended”；Valve 与多数反作弊厂商有合作 [56]。
  - 可以对照的事实：绝区零 Steam 版的 HoYoKProtect 是内核级反作弊，却能在 Proton 下运行，GOL 称其 “Linux support enabled”[16]。这符合“开发商主动开启”的模式。
- 这两套机制都只有 Linux 模块。CodeWeavers 明确表示对 Mac 上的 CrossOver 没有帮助，并且不修复、也不能合法绕过反作弊 [62]（见 08 号报告）。
- **网易《漫威争锋》**：2025-01 曾把在 macOS/Linux 上通过 CrossOver、Proton、Parallels 游玩的玩家误封。之后网易宣布撤销封禁，并承诺修改安全机制，避免在其他平台上误封 [57]。游戏随后在 Steam Deck 和 Linux 上可玩 [58]。
- **共同点**：打开兼容层的开关都在**开发商**手里，而且通常要有明确的“环境身份”作为依据（Proton 或 SteamOS）。macOS 上的 Wine 没有这样一个被业界认可的身份，这正是 Cider 可以补上的地方。

#### 4.3 联络渠道（截至 2026-09 能找到的）

- **正式记录渠道**：HoYoverse Help Center 或游戏内客服工单 [59]；国服走米哈游客服（游戏内或官网入口）。用途是留档，不指望工单直达技术团队。
- **社区渠道**：在 HoYoLAB 官方论坛发布可复现的技术报告，积累公开讨论的记录。
- **媒体和公关渠道**：HoYoverse 在 Games Press 发布的新闻稿里列有各游戏的媒体联系人 [60]（本报告不转录个人邮箱）。
- **行业中间方（推断）**：Apple 的游戏开发者关系团队（参照库洛的先例 [85][86]）；Valve（绝区零 Steam 的合作方）；Proton 和 Wine 的上游社区。
- **没有找到**：公开的商务合作邮箱、开发者或合作伙伴门户、安全应急响应中心。mihoyo.com 的“联系我们”页面由 JS 渲染，未能取到内容。这一项列入未解问题。

#### 4.4 一份可信请求（“Cider Compatibility Brief”）应该包含什么

形式：中英双语 PDF 加一个公开仓库。先有数据，再发出。

1. **身份与承诺**：开源（仓库链接）；**不修改、不隐藏、不绕过任何反作弊组件**；反作弊文件原样执行；不做 SteamOS、Proton 或硬件伪装；Wine 的标识（例如 ntdll 的 `wine_get_version` 导出）保持可见，另外增加一个稳定、可验证的 Cider 环境标识（Cider 版本、引擎哈希、macOS 版本）。
2. **事实包**：
   - 三款游戏 × 渠道 × 版本的矩阵，逐项给出“出现什么弹窗、在什么时刻出现、错误码是什么”；
   - 用来对照的 Proton 放行事实 [17][21]：
     - 绝区零 Steam 版 Works with Proton（2026-06-17）；
     - AWACY 记录绝区零非 Steam 版（miHoYo Protect）自 2024-07-31 起为 Running；
     - 原神自 3.8 起全新安装即放行。
   - 复现脚本和日志（按 13 号报告的支持包格式，默认脱敏）；
   - 不包含任何逆向反作弊的内容。
3. **分级诉求**，从易到难：
   - (a) 在不支持的环境里，给出明确、可识别的“不支持”返回，不要弹通用的反作弊错误；最好在启动前就可以查询，让 Cider 能提前分流；
   - (b) 承诺不因使用兼容层而封号，参照网易的先例 [57]；
   - (c) 把对 Proton/SteamOS 的兼容层策略扩展到带签名身份的 macOS Wine 环境（Cider）；
   - (d) 提供一个技术联系人，并在版本更新前给出测试窗口；
   - (e) 长期：出原生 Mac 版；或者为 macOS 提供官方云，形式是原生 macOS 客户端或网页版，不是 iPad App。具体请求是把绝区零日本云版的 “Mac” 支持做成原生客户端或网页版，并扩展到原神、星铁以及更多地区。
4. **Cider 能给的回报**：每个版本的回归报告（CI）、问题的最小复现、中文 Mac 玩家社区的反馈汇总、在 UI 中引导用户走官方渠道和官方云。

### 5. “深度支持”的设计含义

#### 5.1 路线与裁定数据（schema 示例，数值为占位）

`compat-db/games/mihoyo.genshin.json`：

```json
{
  "id": "mihoyo.genshin",
  "names": { "zh-Hans": "原神", "en": "Genshin Impact" },
  "editions": [
    { "edition": "cn", "exe": "YuanShen.exe", "account": "mihoyo-passport",
      "channels": ["mihoyo-launcher-cn", "mihoyo-launcher-bilibili"],
      "version_source": { "api": "hyp-api.mihoyo.com", "game_id": "TBD" } },
    { "edition": "global", "exe": "GenshinImpact.exe", "account": "hoyoverse-id",
      "channels": ["hoyoplay", "epic"],
      "version_source": { "api": "sg-hyp-api.hoyoverse.com", "launcher_id": "VYTpXlbWo8", "game_id": "gopR6Cufr3" } }
  ],
  "routes": [
    { "kind": "pc-client", "priority": 1 },
    { "kind": "official-cloud", "priority": 2,
      "targets": { "cn": { "url": "https://ys.mihoyo.com/cloud/", "open_with": ["chrome-app", "edge-app", "default-browser"] },
                   "global": null } },
    { "kind": "authorized-cloud", "priority": 3, "opt_in": true, "providers": ["geforce-now", "xbox-cloud"] },
    { "kind": "native", "priority": 0, "available": false }
  ],
  "verdicts": [
    { "edition": "global", "channel": "hoyoplay", "game_version": "<tag>",
      "engine": "cider-engine-r@<hash>", "macos": "27.0", "cpu": "x86_64/rosetta",
      "result": "unverified", "ac_popup": null, "verified_at": null, "evidence": null }
  ]
}
```

`result` 的取值：`playable`、`playable-caveats`、`blocked-anticheat`、`broken-launcher`、`broken-render`、`unverified`。整个库用 minisign 签名，沿用 13 号报告的做法。

**核查后补充：官方云路线要按地区和客户端形态约束。** 原神的 `global` 云目标确实没有 Mac 可用形态，写 `null` 是对的。绝区零的国际服官方云只在日本地区（亚服）提供，需要在 `official-cloud` 目标里写明地区和形态。`compat-db/games/mihoyo.zzz.json` 的片段如下（数值为占位）：

```json
{ "kind": "official-cloud", "priority": 2,
  "targets": {
    "cn":     { "url": "https://zzz.mihoyo.com/cloud-feat/", "form": "web",
                "open_with": ["chrome-app", "edge-app", "default-browser"] },
    "global": { "regions": ["JP"], "server": "asia",
                "form": "unverified", "url": null,
                "excluded_forms_note": "ios-on-mac: 按用户底线永不启用" } } }
```

校验器规则：

- `form` 只允许取 `web`、`native-macos`、`unverified` 三个值。`ios-on-mac`（Mac App Store 上的 iPhone/iPad App）**不在枚举里**，写进去 CI 直接失败。
- `form` 为 `unverified` 时，路线卡不展示这个目标。
- 把 `form` 改成 `native-macos` 需要附证据：官方下载地址，以及安装包 `Info.plist` 中 `CFBundleSupportedPlatforms` 为 `MacOSX`。

#### 5.2 启动前检查（pre-flight）状态机

```
detect_install ──► resolve_edition/channel ──► read_version(local → HYP API 只读)
      │                                               │
      ▼                                               ▼
 env_check(Rosetta/macOS/engine/DXMT/磁盘/内存)   verdict_lookup(game×channel×version×engine×macOS)
      └──────────────► decide ◄───────────────────────┘
          playable            → 启动（先起启动器，后起游戏；进程组管理）
          unverified          → 不启动游戏 exe；显示“新版本验证中”，给出云端按钮；测试员可选择参与
          blocked-anticheat   → 不启动游戏 exe；显示路线卡（云 / 授权云 / 外链说明）
          broken-launcher     → 允许只打开启动器（更新和校验），游戏按钮置灰
```

关键规则：

1. 游戏版本不在裁定库里时，默认视为 `unverified`，**不启动游戏本体**。启动器本身可以打开，用来更新或修复。
2. “兼容性测试员”模式默认关闭。开启时要逐条勾选风险告知，产生的数据只用于裁定库。
3. 每个游戏大约 6 周一个版本（行业常识，推断）。用 GitHub Actions 每天轮询 `getGameBranches` 的 `tag`：版本一变，就把对应裁定标记为 `unverified`，并自动开 issue，触发实验室复测。

#### 5.3 其他“深度支持”清单

| 维度 | 做法 |
|---|---|
| 启动器 | 在 LRS 中加入 HoYoPlay 和米哈游启动器；自更新前对版本子目录做 APFS 快照；启动器与游戏分别钉渲染器；游戏退出后清理 HYP 后台进程和 wineserver |
| 渠道 | 支持 HoYoPlay、米哈游启动器（官服和 B 服）、Epic、Steam（只有绝区零，国区不可用）；支持“导入已有游戏目录”（外置盘或 Boot Camp），导入后用官方启动器的“校验/修复”功能完成对齐 |
| 输入 | GameController 到 XInput/DualSense 的映射（09 号报告）；按游戏预置手柄提示；键盘布局使用美式物理键位 |
| 显示 | 默认无边框全屏；刘海屏安全区；Retina 缩放；绝区零和星铁开放 120 限帧，原神固定 60 |
| CJK | 国服默认 zh-Hans；补中文字体；聊天 IME 走 09 号报告的 IMM32 路径 |
| 账号和区域 | Cider 不接触凭据，登录在游戏或官方网页里完成；UI 按“国服（通行证/B 站）/ 国际服（HoYoverse ID）”引导选择版本；提示 Steam 和 Epic 只有国际服；提示大陆用户也可以通过 HoYoPlay 或 Epic 安装国际服，但 Steam 版绝区零对国区锁区（核查更正） |
| 网络 | 游戏数据直连官方 CDN，不需要镜像；Cider 自己的引擎和配方走镜像（R4） |
| 资源 | 按每款约 75 GB 以上做磁盘预检（绝区零 Steam 版的数据 [12]）；8 GB 内存机器提示关闭其他程序 |

## 对 Cider 的启示与建议

按优先级排列。每项的工作量以“一个 5 小时 AI 配额窗口”（下称“窗口”）为单位估算。

**P0（MVP 前）**

1. **路线与裁定 schema、校验器、签名**（2 个窗口）。按 5.1 节实现；CI 用 JSON Schema 校验，用 minisign 签名。
   - 验收：三款游戏 × 两个版本 × 各自渠道的样例能通过校验；缺字段时 CI 失败。
2. **Pre-flight 引擎和路线卡 UI**（zh-Hans 和 en，3 个窗口）。
   - 验收：对 `unverified` 或 `blocked-anticheat` 的组合，整个流程中游戏 exe 一次都不被拉起（在进程审计日志中可以验证）；路线卡 3 秒内给出云端入口。
3. **国服官方云入口 v1**（1 个窗口）。优先用 Chrome/Edge 的 `--app` 窗口，其次用默认浏览器；启动前做网络预检。
   - 验收：云·原神、云·星铁、云·绝区零三个入口都能在 macOS 27 上用 Chrome 进入登录页。
4. **版本监控**（GitHub Actions cron，1 个窗口）：只读轮询国服和国际服的 HYP API `tag`，版本变化时开 issue 并把裁定置为 `unverified`。
5. **实验室取证 E1–E4**（4 个窗口，外加人工时间；建议在 16 GB 以上的测试机上做，每款游戏要准备 75 GB 以上磁盘）：
   - E1 绝区零 Steam 版；E2 原神国际服和国服（HoYoPlay / 米哈游启动器）；E3 星铁；E4 官方云的 Windows 客户端（Genshin Impact · Cloud PC、云·原神 PC、绝区零云版（日本）PC）。
   - E1 还要记录一项对照：绝区零 HoYoPlay 版（AWACY 自 2024-07-31 起为 Running）与 Steam 版（HoYoKProtect）在 Cider 下的表现是否不同 [21]。
   - 只记录现象（弹窗时刻、错误码、进程树、Wine 日志），**不修改任何反作弊文件或行为**。结果交给 21 号报告做对照分析。

**P1（Beta）**

6. 在 LRS 中加入 HoYoPlay、米哈游启动器和 Epic 渠道；首先确认 HoYoPlay 的 UI 技术栈（1–2 个窗口）。
7. 提供“授权第三方云”（GeForce NOW / Xbox Cloud）的可选入口，默认折叠（0.5 个窗口）。
7a. **核查后新增：核实绝区零日本云版的 Mac 形态**（0.5 个窗口，只读调研）。
   - 做法：查官方新闻页 [78] 和官网下载入口，确认是否存在官网下载的原生 macOS 客户端或网页版。如果有原生客户端，检查安装包 `Info.plist` 中的 `CFBundleSupportedPlatforms`（原生应为 `MacOSX`，iOS App 为 `iPhoneOS`）。
   - 验收：compat-db 中 `mihoyo.zzz` 的 `global.form` 由 `unverified` 改为 `web` 或 `native-macos`，并附证据；如果只找到 iPad App 形态，保持 `unverified`，路线卡不展示。**无论哪种结果，都不引导用户使用 Mac App Store 上的 iPad App。**
8. 写出 Cider Compatibility Brief v1，在 E1–E4 数据齐全后按 4.3 节的渠道投递；同时在 HoYoLAB 公开发布技术报告（1 个窗口）。
9. 推出兼容性测试员计划，并做回报自动化（1 个窗口）。

**P2**

10. WKWebView 云窗口原型：全屏，加上私有 Pointer Lock 委托（需要评估维护风险），先只对云·绝区零启用（2 个窗口）。
11. 如果 E4 可行：把 Windows 版云客户端做进 Cider，作为以下用户的官方云路线（2 个窗口）：
    - Genshin Impact · Cloud PC：服务原神国际服、所在地区已开放该云服务的 Mac 用户；
    - 绝区零云版（日本）PC：在 7a 没有找到网页版或原生 macOS 客户端时，服务日本地区（亚服）的 Mac 用户。
12. 预留检测原生 Mac 版的逻辑，关注 WWDC27 和 Apple 发布会。

## 风险

1. **官方可能永远不放行 macOS 上的 Wine**，甚至收紧检测。对策：pre-flight 默认阻断，路线卡兜底；不承诺“能玩”，只承诺“不会停在弹窗上”。
2. **更新频繁**：每个版本都可能改变裁定，已验证组合会周期性失效。社区还传言大版本更新后会有 1–2 周的兼容层限制期（未核实）[69]。
3. **国际服缺少 Mac 官方云**（核查更正，按地区区分）：
   - 原神和星铁的国际服，以及日本以外地区的绝区零，兜底只能靠第三方授权云（GeForce NOW、Xbox，需要订阅，且在中国大陆不可用）或 E4 实验的结果。
   - 日本地区的绝区零有官方云，但它的 Mac 形态疑似 iPad App，按底线不能使用；在 7a 核实之前，它不算可用的兜底。
4. **云服务的价格、免费时长和网页兼容性经常变动**：Safari 系的全屏问题 [33]，或站点新增 UA 检查，都可能让 C 方案失效。
5. **启动器自更新导致失效**：这是 CrossOver changelog 中反复出现的模式（01 号报告）。
6. **Rosetta 收缩（2027）**：这三款游戏都是 x86_64，而米哈游的内核反作弊（原神、星铁为 mhyprot，绝区零 Steam 版为 HoYoKProtect）在 Windows 上也没有 ARM64 驱动（依据是单一第三方追踪站）[55]，Engine A 路线对它们的可行性存在双重不确定。
7. **资源压力**：每款游戏 75 GB 以上，8 GB 开发机无法同时跑实验和开发。
8. **渠道差异**：Steam 版国区锁区 [15][72]；国服（服务器）的 PC 版只通过米哈游启动器分发，大陆用户如果玩国际服，仍可用 HoYoPlay 或 Epic（核查更正）；B 服的登录 SDK 窗口在 Wine 下的表现未知。

## 未解问题

1. HoYoPlay 和米哈游启动器的 UI 技术栈。验证方法：在安装目录执行 `find . -iname 'libcef.dll' -o -iname 'Qt6WebEngineCore.dll' -o -iname 'WebView2Loader.dll' -o -iname 'msedgewebview2.exe'`，并查看进程树。
2. 同一客户端在 Proton 下被放行、在 macOS Wine 下弹错，差异点具体在哪里（交给 21 号报告和 E1–E3）。
3. 原神和星铁会不会上 Steam，上架后是否沿用绝区零在 Proton 下的表现。
4. Genshin Impact · Cloud 国际服有没有网页端，能不能在 Mac 的浏览器中使用。
5. 星铁和绝区零 PC 版是否有 B 服，登录窗口使用什么技术。
6. HoYoverse 或米哈游有没有公开的商务或技术合作入口、安全响应渠道。
7. Epic 版是否只有国际服（目前是推断）。
8. 云·星穹铁道网页版的官方画质参数，以及云·绝区零上线日期的一手来源。（云·星铁的正式上线日期已查明：2024-02-06 [71]。）
9. **核查后新增**：绝区零日本云版的 “Mac” 支持，到底是官网下载的原生 macOS 客户端、网页版，还是只是 iPad App 在 Mac 上运行？目前的证据倾向于后者 [81][82]，需要按建议 7a 核实。

## 参考来源

1. https://baike.baidu.com/item/%E7%B1%B3%E5%93%88%E6%B8%B8%E5%90%AF%E5%8A%A8%E5%99%A8/64275479 与 https://sj.qq.com/appdetail/com.mihoyocol.pcapp.mihoyocoltd — 米哈游启动器 2024-06-17 上线，旧版停止支持（搜索摘要）
2. https://www.youxituoluo.com/532226.html — 米哈游启动器正式上线（行业媒体，搜索摘要）
3. https://game8.co/games/Genshin-Impact/archives/456970 — HoYoPlay 整合四款游戏（搜索摘要）
4. https://gist.github.com/DynamiByte/0ad250bbe1930e2736a6d4e6e842bcb2 — HYP API 端点、launcher_id、game ID（2026-03-05）
5. https://github.com/Furina1027/miHoYo-api — 国服和国际服 API 基址、getGameBranches/getBuild、Sophon 流程
6. https://github.com/Scighost/Starward/issues/725 — Sophon 分块模式从原神 4.5 起使用（2024-03-18）
7. https://github.com/yaagl/yet-another-anime-game-launcher/issues/561 — 预下载只提供 Sophon、暂无 ZIP（开于 2025-05-07；标题中的 “6.0” 与原神 6.0 上线日 2025-09-10 不符，见 [87]）
8. https://www.file.net/process/hyphelper.exe.html — HYPHelper 位于版本子目录、无窗口
9. https://github.com/Facta-Leopard/ForgePlay/issues/22 — 绝区零 Steam 版在 macOS（Wine 11.12）上失败，HYP 进程名（2026-09-20）
10. https://github.com/Whisky-App/Whisky/issues/1033 — HoYoPlay 在 Whisky 2.3.2 上空白窗口（2024-06-17）
11. https://lemmy.world/post/28449676 — Linux 社区：装 WebView2 后启动器黑屏消失（社区）
12. https://store.steampowered.com/app/4162040/ — 绝区零 Steam 商店页：HoYoKProtect、第三方账号、系统要求（DirectX: Version 11）、13 种语言、75 GB
13. https://zenless.hoyoverse.com/en-us/news/164753 — 官方公告：6 月 17 日登陆 Steam（只取到标题）
14. https://www.rpgsite.net/news/20713-zenless-zone-zero-steam-preload-download-account-link-faq — Steam 版 FAQ：绑定、服务器、75 GB、不经 HoYoPlay 预下载（2026-06-16）
15. https://www.gamersky.com/news/202604/2130657.shtml — Steam 版锁国区（2026-04-24）
16. https://www.gamingonlinux.com/2026/06/zenless-zone-zero-has-arrived-on-steam-and-works-on-linux-steamos/ — 绝区零在 Linux 和 SteamOS 上可玩，Deck Playable
17. https://www.gamingonlinux.com/anticheat/vendor/hoyokprotect/ — HoYoKProtect：绝区零 “Works with Proton”（2026-06-17）
18. https://www.rpgsite.net/news/20250-zenless-zone-zero-steam-deck-support-zzz-optimization-hoyoverse-launch — HoYoverse 对 Steam Deck 的表态（2026-04-28）
19. https://www.kitguru.net/desktop-pc/mustafa-mahmoud/genshin-impact-will-soon-no-longer-be-exclusive-to-epic-on-pc-according-to-dataminers/ — Steam 相关字符串的数据挖掘（2026-01-19）
20. https://x.com/Pirat_Nation/status/2048782219217711319 — 原神 PC 客户端中出现 steam_api DLL 的传闻（未确认）
21. https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/HEAD/games.json — AWACY：原神 Running（3.5/3.8 的记录，条目更新于 2024-09-02）、星铁 Broken（2025-05-26）、绝区零（非 Steam 版，miHoYo Protect）Running（2024-07-31）、崩坏3 Broken（ACE，Steam App 1671200）
22. https://www.videogameschronicle.com/news/genshin-impact-is-releasing-on-the-epic-games-store-next-week/ — 原神 2021-06-09 上架 Epic（搜索摘要）
23. https://store.epicgames.com/en-US/news/honkai-star-rail-officially-launches — 星铁 2023-04-26 首发（搜索摘要）
24. https://x.com/EpicGames/status/1806372025130160335 — 绝区零 2024-07-04 在 Epic 上线（搜索摘要）
25. https://www.itechguides.com/where-to-download-genshin-impact-on-pc-official-hoyoplay-and-epic-guide/ — Epic 版与官方启动器版游戏文件互通、不能互相关联（搜索摘要）
26. https://github.com/sffxzzp/GenshinSymlinker — YuanShen.exe 与 GenshinImpact.exe；三款游戏的国服和国际服可以共存（只证明客户端分离，不涉及账号）
27. https://jingyan.baidu.com/article/7082dc1c4afe0ca50a89bd81.html — 原神 B 服 PC 端扫码登录（搜索摘要）
28. https://news.xbox.com/en-us/2024/08/20/genshin-impact-coming-to-xbox-on-november-20/ — 原神 2024-11-20 上线 Xbox Series 和 Xbox Cloud；当时说明云端游玩需要 Game Pass Ultimate；支持跨平台存档
29. https://news.xbox.com/en-us/2025/05/23/zenless-zone-zero-xbox-pre-order-starter-pack/ — 绝区零 2025-06-06 上线 Xbox（搜索摘要）
30. https://www.rpgsite.net/news/21440-honkai-star-rail-version-4-6-update-release-date-zzz-collaboration-razer — 星铁 4.6（2026-09-28）的平台（搜索摘要）
31. https://www.163.com/dy/article/IEN77JL20511B8LM.html — 云·原神网页版桌面端开放（2023-09-15）：Win7+/macOS 10.10+、推荐 Chrome、首次 5 小时
32. https://news.mydrivers.com/1/935/935384.htm — 云·原神网页版（2023-09-16）：60 元月卡
33. https://shouyou.3dmgame.com/gl/477142.html — 云·原神网页版官方 FAQ 转载：浏览器、键鼠、Safari 全屏问题
34. https://www.9game.cn/yuanshen/7309438.html 与 https://www.ithome.com/0/569/312.htm — 每日 15 分钟、上限 600 分钟、1080P60、码率 2~50 Mbps（搜索摘要，多源）
35. https://news.qq.com/rain/a/20240802A091KC00 — 2024-08 免费时长活动；平台列表（Windows 客户端、Windows 和 macOS 网页、移动端）
36. https://www.zhihu.com/question/1932923499812463160 — 云·原神没有 macOS 客户端（搜索摘要）
37. https://sr.mihoyo.com/news/122332 — 《云·星穹铁道》网页版与安卓端正式上线（官方，只取到标题）
38. https://www.gamersky.com/news/202311/1668414.shtml — 云·星铁测试覆盖 PC 和 Mac 网页（2023-11-09）
39. https://shouyou.3dmgame.com/gl/492648.html — 云·星铁计费：10 星云币/分钟、畅玩卡 30 元/30 天（2024-02-07）
40. https://m.ali213.net/news/gl2412/1571539.html — 云·绝区零网页入口（2024-12-11）；上线日期 12-18（搜索摘要）
41. https://www.idongdong.com/article/24489.html — 云·绝区零支持的浏览器（Chrome 110+/Edge 112+/Safari 17.4+）和手柄（2026-05-07，第三方）
42. https://m.ali213.net/news/gl2604/1765383.html — 云·绝区零免费时长（2026-04-20）
43. https://cloudgenshin.hoyoverse.com/en-us — Genshin Impact · Cloud：地区列表，平台只有 iOS/Android/PC
44. https://cloudgenshin.hoyoverse.com/en-us/news/114201 — 新加坡和马来西亚 PC 公测（2023-12-27），10 Cloud Coins/分钟
45. https://www.siliconera.com/genshin-impact-cloud-version-open-beta-hits-north-america-costs-money-to-play/ — 美国和加拿大公测与定价（2024-06-13）
46. https://blogs.nvidia.com/blog/2022/06/16/geforce-now-thursday-june-16 — 原神 2022-06-23 登陆 GeForce NOW，含 Mac 应用
47. https://blogs.nvidia.com/blog/geforce-now-thursday-honkai-star-rail — 星铁（Epic 版）登陆 GeForce NOW（2024-05-09）
48. https://finalweapon.net/2024/12/11/zenless-zone-zero-comes-to-geforce-now-next-week/ — 绝区零登陆 GeForce NOW（2024-12）
49. https://cloudbase.gg/g/genshin-impact/ — 云游戏收录聚合（2026-09-25，第三方，只作参考）
50. https://developer.apple.com/documentation/webkit/wkpreferences/iselementfullscreenenabled — WKWebView 元素全屏（macOS 12.3+，经搜索摘要）
51. https://github.com/manaflow-ai/cmux/pull/14518 — WKWebView 的 Pointer Lock 需要私有委托（2026-09-25）；参见 https://github.com/WebKit/WebKit/blob/main/Source/WebKit/UIProcess/API/Cocoa/WKUIDelegate.h
52. https://bugs.webkit.org/show_bug.cgi?id=205448 — WKWebView 中的 Gamepad API（iOS 13+，需要按键唤醒）
53. https://www.siliconera.com/wuthering-waves-launch-trailer-and-mac-store-release-shared/ — 鸣潮 2024 年上线时宣布 Mac App Store 版 “on the way”（核查更正：不含 2025-03-27 日期，也没有提到 Tim Cook；这两项改引 [85][86]）
54. https://en.wikipedia.org/wiki/Honkai:_Nexus_Anima 与 https://en.wikipedia.org/wiki/Petit_Planet — 新作平台为 PC 加移动端（搜索摘要）
55. https://www.onarm.net/anti-cheat — 三款游戏在 Windows on Arm 上被拦截（2026-06-06）
56. https://partner.steamgames.com/doc/steamdeck/proton — Valve：EAC 和 BattlEye 在 Proton 下的开启步骤、对用户态和内核态反作弊的态度
57. https://appleinsider.com/articles/25/01/04/netease-reverses-bans-on-macos-linux-players-of-marvel-rivals — 网易撤销对 macOS/Linux 玩家的误封（2025-01-04）
58. https://www.gamingonlinux.com/2024/12/marvel-rivals-out-now-free-on-steam-and-works-on-steam-deck-linux/ — 漫威争锋在 Deck 和 Linux 上可玩（搜索摘要）
59. https://support.hoyoverse.com/hc/en-us/articles/49686220541721-How-to-contact-support-for-in-game-problems — HoYoverse Help Center 联系方式
60. https://www.gamespress.com/en-US/HoYoverse-Teases-Significant-Updates-and-Crossovers-on-gamescom-2024-O — HoYoverse 新闻稿，附分游戏的媒体联系人（搜索摘要）
61. https://www.codeweavers.com/crossover/changelog — CrossOver 24.0.4 “New HoYoPlay launcher now works”（经 01/08 号报告；本次 403）
62. https://support.codeweavers.com/anti-cheat — CodeWeavers 反作弊政策（经 08 号报告）
63. https://www.playstation.com/en-us/support/games/playstation-remote-play-on-pc-and-mac/ — PS Remote Play 的 Mac 客户端（macOS 版本下限见搜索摘要）
64. https://www.pcgamesn.com/zenless-zone-zero/best-settings — 绝区零 PC 不限帧（搜索摘要）
65. https://gamevika.com/en/hsr/wiki/optimal-settings — 星铁 PC 的 120 fps 隐藏设置（社区，低）
66. https://sportskeeda.com/esports/genshin-impact-2-8-adds-new-fps-option-pc-users-await-high-frame-rate-support — 原神 PC 没有高帧率选项（搜索摘要）
67. https://www.xbox.com/en-US/cloud-gaming — Xbox Cloud Gaming（浏览器与订阅档位，经搜索摘要）
68. https://steamdeckhq.com/news/hoyoverse-welcomes-zenless-zone-zero-steam-deck/ — HoYoverse 欢迎在 Deck 上游玩（转述 [18]）
69. https://caniplayonlinux.com/games/genshin-impact/ — 大版本更新后 1–2 周限制的说法（单一来源，低）
70. https://baike.baidu.com/en/item/Genshin%20Impact%20Cloud/14137 — 云·原神 2021-10 推出安卓版，现覆盖 Windows 客户端、Windows 和 macOS 网页、移动端（搜索摘要）
71. https://news.qq.com/rain/a/20240206A08FUL00 — 《云·星穹铁道》网页版与安卓端正式上线（2024-02-06）：Windows/macOS/安卓/iOS；每个版本登录得 10 小时，累积上限 10 小时（核查补充）
72. https://store.steampowered.com/api/appdetails?appids=4162040&cc=cn — Steam appdetails API：`cc=cn` 返回 `{"success":false}`（2026-09-27 核查）
73. https://store.steampowered.com/api/appdetails?appids=4162040&l=english — 绝区零 Steam：release_date “Jun 16, 2026”、HoYoKProtect、HoYoverse Account、最低和推荐配置均为 “DirectX: Version 11”、75 GB（2026-09-27 核查）
74. https://www.ithome.com/0/961/020.htm — IT之家：绝区零 6 月 17 日登陆 Steam，国区锁区（2026-06-07）
75. https://automaton-media.com/articles/newsjp/zzz-20260130-414736/ — AUTOMATON：绝区零云版日本地区 2026-02-06 上线，只连亚服，建议在日本国内游玩（2026-01-30）
76. https://www.gamespark.jp/article/2026/01/31/162205.html — Game*Spark：绝区零云版平台为 PC、Mac、iOS、Android；Mac 需求 M1 以上、macOS 12 以上（2026-01-31）
77. https://game8.jp/zenless/730264 — game8：云版需求（Mac：“M1チップ以上、macOS 12以上”）、首次 10 小时、30 日 2,600 日元（攻略站）
78. https://zenless.hoyoverse.com/ja-jp/news/162503 — 绝区零云版官方公告（JS 渲染，只取到标题）
79. https://x.com/ZZZ_JP/status/2019591879675838653 — 绝区零日本官方 X：“iOS、Android、PC（Windows/Mac含む）版が正式にリリース”（搜索摘要）
80. https://play.google.com/store/apps/details?id=com.HoYoverse.cloudgames.Nap&hl=en_US — Google Play：ゼンレスゾーンゼロ・クラウド版
81. https://itunes.apple.com/lookup?id=6748930656&country=jp — iTunes Lookup：ゼンレスゾーンゼロ・クラウド版为 iOS App（COGNOSPHERE PTE. LTD.，iOS 13.0+，2026-02-04 发布）；同日用 https://itunes.apple.com/search?term=zenless&country=jp&entity=macSoftware 搜索，没有原生 macOS 版本（2026-09-27 复查）
82. https://apps.apple.com/jp/app/%E3%82%BC%E3%83%B3%E3%83%AC%E3%82%B9%E3%82%BE%E3%83%BC%E3%83%B3%E3%82%BC%E3%83%AD-%E3%82%AF%E3%83%A9%E3%82%A6%E3%83%89%E7%89%88/id6748930656 — App Store 页：搜索摘要显示 “iPad用に設計。macOSでは未検証”、Mac 需要 “macOS 12.0以上とApple M1チップ以降”（页面本身无法直接取回，属于搜索摘要，低）
83. https://gamewith.jp/zenless/550018 — GameWith：云版只连亚服、不能切换服务器；Mac 需求 M1、macOS 12；10 小时试玩；30 日 2,600 日元
84. https://note.com/yoh_kitajima/n/nba917508cf4e — 个人文章：Mac 上玩绝区零云版的说法（2026-02-06；关于“官网客户端”还是“浏览器”，说法不一致，低）
85. https://x.com/Wuthering_Waves/status/1888075281044259062 — 鸣潮官方：3 月 27 日起在 Mac App Store 上线
86. https://www.gamespress.com/WUTHERING-WAVES-VERSION-22-AVAILABLE-NOW-FULL-GAME-LAUNCHES-ON-MACOS — 鸣潮 2.2 版与 macOS 完整版上线新闻稿（Tim Cook 2025-03-25 到场见搜索摘要）
87. https://game8.co/games/Genshin-Impact/archives/537916 — 原神 6.0 于 2025-09-10 上线
88. https://www.miyoushe.com/sr/article/48850926 — 米游社：《云·星穹铁道》正式上线公告（搜索摘要）
89. https://github.com/fusedio/fused-render-lite/pull/20 — 同一组 `WKUIDelegatePrivate` 私有选择器（包括 Pointer Lock），Safari 也使用
W1. https://xr1s.me/2023/07/23/pc-genshin-honkai-on-macos/ — 2023 年用 GPTK 跑原神和星铁的个人记录：存在 wineserver 残留、音频问题（原文含规避内容，本报告只引用其中与反作弊无关的现象）

内部交叉引用：00（裁定：DXMT 为 D3D11 主后端、Rosetta 时间线、msync 与 CEF 冲突）、01（CrossOver 启动器支持史）、08（EAC/BattlEye 机制、CodeWeavers 政策、Highball schema）、09（输入、CJK、视频解码）、13（兼容库、签名、支持包、限帧）、14（GeForce NOW 和 Xbox 云）、18（CEF/WebView2、LRS）、21（米哈游反作弊机制，并行撰写）。

## 事实核查记录

核查日期 2026-09-27。本节的更正优先于正文。表中“再核”一项是编辑本报告时对核查员结论做的补充复查，结果与核查员原结论有出入，已在正文中按更谨慎的一方处理。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 绝区零上架 Steam（App 4162040，2026-06-16/17），标注 “Uses Kernel Level Anti-Cheat: HoYoKProtect”；Linux/SteamOS 上 Proton 可运行（GOL 2026-06-17 “Works with Proton”，Deck Playable）；HoYoverse 2026-04-28 表示欢迎在 Deck 上运行 | 属实 | Steam 商店页和 appdetails API 核实：release_date “Jun 16, 2026”，需要 HoYoverse 账号，13 种语言，75 GB。GOL 称 HoYoKProtect “Linux support enabled”，Proton 11 测试可玩；Deck 评级为 Playable，不是 Verified [12][16][17][18][73] |
| Steam 版绝区零国区锁区；原神和星铁不在 Steam；中国玩家只能用米哈游启动器；国服和国际服的 exe 与账号都分离 | 部分属实 | 锁区属实：游民星空 2026-04、IT之家 2026-06-07 都有报道，Steam API 的 `cc=cn` 返回 success:false。原神和星铁截至 2026-09 没有商店页，只有数据挖掘线索。**更正**：只有国服（服务器）的 PC 版限定米哈游启动器，大陆用户仍可通过 HoYoPlay 或 Epic 装国际服；[26] 只证明客户端分离，账号分离另有依据 [27]。已改摘要第 1 条、风险第 8 条和 5.3 节 [15][19][72][74] |
| 三款游戏都有国服官方云，可在 Mac 浏览器使用；云·原神没有 macOS 客户端 | 属实 | 云·原神网页端支持 macOS 10.10+，推荐 Chrome；云·星铁 2023-11 测试即含 Mac 网页，2024-02-06 正式上线；云·绝区零网页入口为 `zzz.mihoyo.com/cloud-feat` [31][38][40][71] |
| 国际服只有 Genshin Impact · Cloud（没有 macOS），星铁和绝区零都没有国际服官方云 | **不属实** | **更正**：还有『ゼンレスゾーンゼロ・クラウド版』，2026-02-06 在日本地区正式上线，只连亚服，平台为 iOS、Android、PC（Windows/Mac含む），首次 10 小时，30 日 2,600 日元。星铁确实没有国际服云。已按地区改写摘要第 3 条、2.2 节结论、E4 理由、风险第 3 条和建议 11 [75][76][77][79][80][83] |
| （再核）核查员称绝区零日本云版有“原生 Mac 串流客户端（M1+、macOS 12+）” | 存疑 [低] | 与核查员原结论相左，但证据不足以完全否定。App Store 上的 iOS 版 id6748930656 在搜索摘要里显示为 “iPad用に設計。macOSでは未検証”；“macOS 12.0 以上 + Apple M1 以上”正是 iPhone/iPad App 在 Mac 上运行时的标准措辞；日区 Mac App Store 找不到原生版本；note.com 的说法前后矛盾。**处理**：按用户底线，不引导用户使用 iPad App 形态。compat-db 里把 `form` 记为 `unverified`，并且不允许取 `ios-on-mac`。新增建议 7a 和未解问题 9 [81][82][84] |
| HoYoverse 没有宣布原生 macOS 版；三款游戏在 Windows on Arm 上都因反作弊驱动没有 ARM64 版而被拦截（onarm.net 2026-06-06） | 属实 | 这是单一第三方追踪站的结论。onarm.net 对绝区零只写 “HoYoverse kernel anti-cheat with no Arm support”，而 Steam 版标注 HoYoKProtect，所以“mhyprot 系”对绝区零不准确，已修正摘要第 4 条、第 3 节和风险第 6 条。另在第 3 节注明日本云版列出的 “Mac” 平台是云串流客户端，不是原生游戏移植 [55][12] |
| Steam 上 EAC 和 BattlEye 的 Proton 支持要由开发者开启；Valve 表示不支持内核态反作弊 | 属实 | Valve 原话：“Kernel-space solutions are not currently supported and are not recommended”。4.2 节已改用原话，并补充 HoYoKProtect 在 Proton 下可运行的对照事实 [56][16] |
| 1.1 节：“从原神 6.0（2025-05）起，预下载只提供 Sophon” | 部分属实 | YAAGL #561 开于 2025-05-07，原文是 “only for Sophon Download system. ZIP is currently not available yet”；原神 6.0 在 2025-09-10 才上线。已改为“大约从 2025 年中起” [7][87] |
| 1.2 节：“绝区零 Steam 版要求 DirectX 11.1 以上” | 不属实 | Steam 最低和推荐配置都写 “DirectX: Version 11”。已更正；DXMT 为主后端的结论不变 [12][73] |
| 2.1 节：云·星铁上线日期未知；“首次 10 小时，上限 10 小时” | 部分属实 | 网页端与安卓端 2024-02-06 正式上线，支持 Windows、macOS、安卓、iOS；**每个版本**登录可得 10 小时，累积上限 10 小时；另有随“无名勋礼”附赠的 42 日畅玩卡。表格和未解问题 8 已更新 [71][88] |
| 第 3 节：鸣潮 2025-03-27 上架 Mac App Store，2025-03-25 Tim Cook 站台 [53] | 部分属实 | 两个日期都属实（macOS 12+、M1 及以上），但原引用 [53] 是 2024 年的文章，两者都不包含。已改引官方 X 和 Games Press 新闻稿，4.3 节的引用同步修改 [85][86] |
| 1.3 节和摘要：AWACY 原神 Running、星铁 Broken；绝区零的 Proton 状态只引 GOL | 部分属实 | 原神和星铁的记录准确（条目日期分别为 2024-09-02、2025-05-26）。**遗漏**：AWACY 还把绝区零非 Steam 版（miHoYo Protect）记为 Running，自 2024-07-31 起。已补进 1.3 表、摘要第 2 条、4.4 事实包和 E1 对照项；崩坏3 另有 Steam 版 App 1671200 [21] |
| 2.4 节：WKWebView 的 Pointer Lock 只能通过私有委托 `_webViewDidRequestPointerLock:completionHandler:` 实现（cmux 2026-09-25） | 属实 | cmux PR #14518 属实；缺少这个回调时 WebKit 直接拒绝。fused-render-lite PR #20 记录了同一组 `WKUIDelegatePrivate` 私有选择器 [51][89] |
