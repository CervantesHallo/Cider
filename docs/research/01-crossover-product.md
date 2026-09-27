# CrossOver（CodeWeavers）产品全景拆解：功能 / UX / 版本 / 商业模式 / 痛点 —— 面向 Cider 的对标调研

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 官方一手来源（codeweavers.com / support.codeweavers.com / developer.apple.com / 官方 GitHub）直接核实；**[中]** = 可信二手来源（主流科技媒体、社区 GitHub 仓库）或官方页面仅经搜索摘要间接获得（codeweavers.com/blog 在本次调研中对抓取返回 403，只能通过搜索摘要和媒体转述）；**[低]** = 推断或单一非权威来源。未能核实的内容在“未解问题”中列出，没有编造。
>
> 2026-09-26 已按独立事实核查结果修订：共核查 11 条声明，5 条确认、5 条部分属实、1 条被驳回；部分属实和被驳回的内容已在正文原处更正，明细见文末“事实核查记录”。

---

## 摘要

- **当前版本**：截至 2026-09-26，CrossOver 最新稳定版是 **26.3.0（2026-07-21）**。26.0.0 于 **2026-02-10** 发布，基于 **Wine 11.0**，Mac 端带 **D3DMetal 3.0、DXMT v0.72**，另含 vkd3d 1.18 和 Wine Mono 10.4.1 [1][2]（已独立核实 [高]）。其中 DXMT v0.72 是上游 2025-12-11 打的标签，上游之后又发布了 v0.73、v0.74 和 v0.80（2026-04-23）。**CrossOver 26.x 自带的 DXMT 比上游落后好几个版本**，Cider 可以直接以更新的 DXMT 为基线 [31]。
- **重大转向（CrossOver 27）**：CodeWeavers 在 2026-06-11 宣布，CrossOver 27 **只支持 Apple Silicon 和 macOS 14 Sonoma 及以上**，**32-bit bottle 在 27 中完全不能运行**。官方 26 用户指南也写明，“Enable deprecated 32-bit bottles”选项将在 27 中移除 [5][6][15]。官方博客原文 [4] 抓取返回 403，以上内容依据两家独立媒体和官方用户指南。**“2027 年初发布”不是 6-11 公告中核实到的内容**，这个时间只出现在 AppleInsider 2026-07-31 关于 ARM64 Preview 的报道里（“penciled in for a release in early 2027”）[8][中]。Intel 用户继续用 26，但不会再有更新 [4][5][6][8]。
- **原生 ARM64**：2026-07-31，CodeWeavers 在 Mac 上发布**原生 ARM64（Apple Silicon）CrossOver Preview** [8]。已独立核实的限制有三条：这个构建不含 D3DMetal，官方称 Direct3D 12 支持“即将推出”；许多启动器完全不能用；现有 bottle 无法转换为 ARM64。三条限制都预计在 CrossOver 27（计划 2027 年初）之前解决 [8][中]。**以下四点目前只来自搜索摘要，没能从一手或独立来源核实** [7][低]：x86/x64 代码由为 macOS 定制的 **FEX** 模拟执行；安装包是 universal build；**ARM64 部分要求 macOS 26.5 及以上**；内含 ARM64 版 DXMT。其中“使用 FEX”的可信度较高 [中，推断]，因为 2025-11 的 Linux ARM64 Preview 已经集成 FEX 负责 i386/x86-64 模拟 [9]。
- **外部时间表**：Apple 官方说明 **macOS 27 是最后一个完整支持 Rosetta 2 的版本**。之后 Rosetta 只为“依赖 Intel 框架、已无人维护的老游戏”保留有限功能 [10][11]（已独立核实 [高]）。这就是 CrossOver 转向 ARM64 的直接原因，Cider 也必须照此规划。有两点需要注意：Apple 没有说明 Wine 或 x86_64 的 Wine 宿主是否属于这个老游戏例外；同一公告还说，从 macOS 26.4 起，启动依赖 Rosetta 的应用时系统可能弹出通知，所以 Cider 的 x86_64 Engine A 在 26.4+ 上会触发 Rosetta 弃用提示 [10]。
- **图形后端矩阵（26）**：Auto（由专有的逐游戏数据库决定，没有记录时用 wined3d）、D3DMetal（Apple 专有，支持 DX11/12）、DXMT（开源，DX10/11→Metal）、DXVK（官方文档只写“for Direct3D 10 and 11”，→MoltenVK）、wined3d；另有 DLSS→MetalFX 开关（仅对 D3DMetal 和 DXMT 生效）、MSync 开关、High Resolution Mode（192 DPI）[16]。ESync 在 25 的文档里还是可选项，26 的文档中已经不再列出 [16][17]（已独立核实 [高]）。
- **开放与专有的边界**：CodeWeavers 的源码页提供 `crossover-sources-26.3.0.tar.gz`，并列出 26.3.0 使用的 29 个 FOSS 项目，包括 Wine、vkd3d、DXVK、MoltenVK、wine-mono、FAudio、SDL、LLVM、Sparkle、PyObjC、Samba、GnuTLS、FreeType、UnRAR、cabextract 等 [29]。该页**没有提到 D3DMetal，也没有提到 DXMT 和 ARM64/FEX 的源码**。本次没有下载 tarball，里面的具体内容没有逐项核实。D3DMetal 是 Apple 专有组件，在 CrossOver 中位于 `CrossOver.app/Contents/SharedSupport/CrossOver/lib64/apple_gptk/external` [34][35]。GUI、CrossTie/C4 兼容性数据库与配方、逐游戏自动配置库都是专有的 [16][27]。EULA 条款属于法律范畴，本轮核查按项目约定跳过，没有复核 [30]。
- **商业模式**：CrossOver+ **$74/12 个月**（含 12 个月升级、邮件/电话支持和 Preview 访问权），CrossOver Life **$494**（终身，不退款），14 天全功能试用，不需要信用卡。按人授权，机器数量不限。支持期内得到的版本可永久使用 [12][13][42]。
- **最大的用户痛点（也是 Cider 的机会）**：① Steam、Battle.net、EA、Epic、GOG、Ubisoft、Rockstar 等启动器一更新就坏，只能等点版本修复（24.0.1→26.3.0 的 changelog 大半是这类修复）[1]；② 兼容性库和安装配方老旧、下载链接失效 [44]；③ 反作弊游戏基本不能玩（官方以 DMCA 为由不做绕过）[40][41]；④ 设置按 bottle 生效，不能按应用单独设置，后端需要用户自己反复试 [16][42]；⑤ Metal HUD 等诊断能力没有 GUI 入口 [55]；⑥ 32-bit bottle 被淘汰，x86→ARM64 的 bottle 无法迁移 [7][8]；⑦ 支持响应慢、退款条款严格 [45]。

---

## 详细调研

### 1. 功能清单与版本演进（23 → 24 → 25 → 26 → 27）

**1.1 版本时间线（Mac 视角）** [1][2][7][8]

| 版本 | 日期 | Wine 基线 | Mac 端关键变化 | 置信度 |
|---|---|---|---|---|
| 23.0.0 | 2023-08-16 | Wine 8.0.1 | DX12 初步支持（Diablo II: R / Diablo IV）、geometry shader 和 transform feedback 初步支持、MoltenVK 1.2.3、DXVK 1.10.3、EA App 支持、应用卸载功能 | 高 |
| 23.5.0 | 2023-09-27 | — | **首次集成 GPTK 翻译层（D3DMetal 开关）**、支持 Sonoma、BG3、Denuvo 游戏可在 Sonoma 上运行、GStreamer | 高 |
| 23.6.0 | 2023-10-18 | — | CS2、Warframe；修复 Sonoma 上的打印 | 高 |
| 23.7.0 | 2023-11-27 | — | **引入 MSync**、MoltenVK 性能优化、支持 Stage Manager | 高 |
| 24.0.0 | 2024-02-22 | Wine 9.0 | MoltenVK 1.2.5、vkd3d 1.10、Wine Mono 8.1、多项 UI 改进 | 高 |
| 24.0.1–24.0.7 | 2024-03 → 2025-01-28 | — | 几乎全部是启动器和游戏更新后的修复（Diablo IV、Ubisoft Connect、HoYoPlay、Rockstar、Battle.net） | 高 |
| 25.0.0 | 2025-03-11 | Wine 10.0 | **加入 DXMT**、D3DMetal 2.1、MoltenVK 1.2.10、**逐游戏自动设置数据库（Auto）**、支持 GOG Galaxy 与 Epic Games Store、RDR2 | 高 |
| 25.0.1 / 25.1.0 | 2025-04-23 / 2025-08-12 | — | EA app、Ubisoft、手柄（有线 Xbox One、8BitDo）；MSync 下 Steam 下载失败的修复 | 高 |
| 25.1.1 | 2025-09-15 | — | 修复 Intel Mac 上的 Tahoe 问题；Tahoe 要求 ≥25.1.1 [14] | 高 |
| 26.0.0 | 2026-02-10 | Wine 11.0 | **D3DMetal 3.0、DXMT v0.72**（上游 2025-12-11 的标签，现已落后于上游 v0.80 [31]）、vkd3d 1.18、Wine Mono 10.4.1、适配 Tahoe 的 UI；Linux 端支持 NTSync | 高 |
| 26.1.0 | 2026-04-09 | — | Death Stranding 2 的 workaround、修复多款 Unity 游戏的鼠标输入 | 高 |
| 26.2.0 | 2026-06-09 | — | 修复 Helldivers 2；**增加 32-bit bottle 的弃用警告** | 高 |
| 26.3.0 | 2026-07-21 | — | 修复 Diablo IV、Mac 上 Epic 启动器下载、GOG Galaxy | 高 |
| Preview（ARM64 Mac） | 2026-07-31 | — | 原生 ARM64 Wine。已核实的限制：不含 D3DMetal（DX12 支持“即将推出”）、许多启动器不能用、旧 bottle 不能转换为 ARM64 [8]。“定制版 FEX、universal build、ARM64 部分需 macOS 26.5+、ARM64 DXMT”只来自搜索摘要，**未核实** [7]；其中 FEX 是较可信的推断 [9] | 中（限制部分）/ 低（FEX 等四点） |
| 27（计划） | 2027 年初（这个时间来自 AppleInsider 2026-07-31 的报道 [8]，6-11 公告中没有核实到发布日期） | — | 只支持 Apple Silicon 和 macOS 14+；32-bit bottle 完全不能运行 [5][6][15] | 中（官方博客原文无法抓取，依据两家独立媒体和官方用户指南） |

补充说明：
- CrossOver 25 能对需要 `ROSETTA_ADVERTISE_AVX=1` 的游戏**自动开启 AVX**。Auto 模式会根据数据库在 wined3d、DXMT、DXVK、D3DMetal 之间选择；QA 发现 DXMT 在低配 Mac 上更快 [中，来自搜索摘要引用的 CrossOver 25 博客]。
- 发布节奏：每年 2–3 月出一个大版本，跟随上一年 1 月的 Wine stable；之后大约每 2 个月一个点版本，内容主要是启动器和游戏热修复 [1]。CrossOver Preview 面向有效订阅用户，比正式版早 6–9 周拿到新 Wine 和修复，可与正式版并存安装，不在支持范围内 [50][中]。
- 源码侧：Gcenx 的 `macports-wine` overlay 里有 `CrossOver 26.3.0` port（`emulators/crossover/Portfile`），同时有 `wine-devel 11.18` 和 `sikarugir 1.0.1` [48]。**这个 CrossOver port 不是源码构建**（经核查更正，原先的说法被驳回）。Portfile 写明 `license Commercial`、`supported_archs x86_64`、`use_configure no`，从 `media.codeweavers.com/pub/crossover/cxmac/demo/` 下载官方二进制试用包 `crossover-26.3.0.zip`，然后只做重新打包：删去部分 DXVK DLL，可选改用 GStreamer.framework，去掉 Sparkle 更新 URL，最后用 `codesign --deep --force --sign -` 做 ad-hoc 签名，全过程没有编译 [60]。因此它**不能证明** 26.3.0 的源码可以由第三方完整构建 [高]。能证明源码可以获取的是 `crossover-sources` tarball 本身 [29] 和 winecx 一类的镜像 [49]；用这些源码能否独立构建出与 CrossOver 等价的产物，本次没有验证。

**1.2 核心功能清单（Mac，26.x）** [15][16][22][23]

| 模块 | 功能点 |
|---|---|
| 安装 | 搜索兼容性库 → 点击磁贴 → “Install details” 清单（安装程序、bottle、依赖逐项打勾）→ 自动下载安装程序和依赖；Advanced 中可开调试日志、选语言、管理依赖；“Install an unlisted application” 走手动选择安装程序并新建 bottle 的路径；安装程序缓存（可清理）；打开 .exe 时可自动弹出 Installer Assistant |
| 运行 | Home 视图列出图标，双击运行；Hide from Home；安装后自动在 Launchpad 生成图标，可拖进 Dock；Run Command（运行任意 exe 或 regedit、notepad 等 Wine 工具，并可 **Save Command as Launcher**）；Run with Options（调试日志通道自动填好，可附加参数和环境变量） |
| Bottle 动作 | Open C: Drive、Install Applications Into Bottle、Quit / Force Quit All Applications、Delete Bottle |
| Bottle 菜单 | New / Duplicate / Rename Bottle、Export Bottle to Archive（压缩归档，可用于回滚或迁移机器）/ Import Bottle Archive、Publish Bottle / Update Published Bottle（多用户共享）、Open Shell（打开已设置好 Wine 环境、位于 C: 盘的终端） |
| Advanced Settings | Graphics：Auto / D3DMetal / DXMT / DXVK（官方文档只写 DX10/11）/ Wine(wined3d)；DLSS（通过 MetalFX，仅对 D3DMetal 和 DXMT 生效）；MSync；High Resolution Mode |
| Control Panels | Wine Configuration（winecfg）、Game Controllers（含 **Disable hidraw**，用于有线 Sony 手柄）、Simulate Reboot、Task Manager、Internet Settings |
| Bottle Details | 已安装软件列表，可卸载（23.0 起） |
| Preferences | 在线集成（检查更新、提醒提交评分）、系统集成（启动器图标目录、bottle 目录）、Installer Assistant（自动下载新配方、隐藏未测试或已知不可用的条目、**Enable deprecated 32-bit bottles**，26 起新 bottle 默认 64-bit）、安装程序缓存 |
| 故障排除 | Clear and Rebuild Programs（重建启动器图标）、Repair Bottles（修复被杀毒软件误删的文件）、Check for Updates |
| 注册 | 试用模式；用邮箱和密码或激活码解锁；命令行 `cxregister` [21] |

### 2. Bottle、模板、CrossTie 与兼容性数据库

**2.1 Bottle 与模板** [19][23]
- Bottle 是独立的 WINEPREFIX，包含 C: 盘、注册表、字体、CrossOver 设置、应用和用户数据；默认每个应用单独建一个 bottle，以免互相影响 [高]。
- 模板名：`win98, win2000, winxp, winvista, win7, win8, win10, win11`，64 位版本加 `_64` 后缀（如 `win10_64`、`win11_64`）[高]。26 起默认创建 64-bit bottle，32-bit 需要在偏好设置里手动开启；27 起彻底取消 32-bit bottle，32-bit bottle 在 27 中完全不能运行 [15][5][6]。
- 路径 [19]：用户 bottle 在 `~/Library/Application Support/CrossOver/Bottles`，已发布 bottle 在 `/Library/Application Support/CrossOver/Bottles`；偏好文件 `~/Library/Preferences/com.codeweavers.CrossOver.plist`（键 `BottleDir`、`ManagedBottleDirs`）；hook 脚本放在 `~/Library/Application Support/CrossOver/support/scripts.d` 或 bottle 根目录的 `scripts.d`，命名规则 `nn.name`；可用环境变量 `CX_ROOT`、`CX_BOTTLE`、`WINEPREFIX`。Linux 文档列出的 hook 触发点有 `create`、`restore`、`upgrade-from`、`pre-create-stub`、`create-stub`、`pre-update-stub`、`update-stub` [20]。卷序列号和卷标可以用目录根下的 `.windows-serial` / `.windows-label` 指定 [19]。
- `cxbottle.conf` 的 `[EnvironmentVariables]` 常见键（来自社区仓库，**[中]**）[38][39]：`CX_GRAPHICS_BACKEND` = `d3dmetal|dxmt|dxvk|wined3d`（缺省即 Auto）；`WINEMSYNC`、`WINEESYNC`；`D3DM_ENABLE_METALFX=1`（D3DMetal 的 DLSS→MetalFX）；`DXMT_ENABLE_NVEXT=1`（DXMT 的 DLSS）；`MTL_HUD_ENABLED=1`（Metal HUD）；`ROSETTA_ADVERTISE_AVX=1`（让 Rosetta 向进程报告 AVX/AVX2）。旧键 `WINED3DMETAL`、`WINEDXVK` 会被模板迁移。High Resolution Mode 对应注册表 `HKCU\Software\Wine\Mac Driver` 下的 `RetinaMode="Y"` [中]。

**2.2 CrossTie 配方（.tie / C4P）格式与依赖解析** [24][25][26]
- 格式：XML。根结构是 `<c4p><applications><app appid="…">…</app></applications></c4p>`，可以用 `autorun` 指定首选 app。`<cxversion product="cxoffice|cxgames">` 按产品和版本过滤。`appid` 必须永久不变、删除后也不能复用 [高]。
- 每个 `app` 最多一个 AppProfile，可以有零到多个 InstallProfile 和 CdProfile。安装分三段：**PreInstallation**（依赖、支持的 bottle 类型、下载信息）、**Installation**（与依赖无关的安装步骤）、**PostInstallation** [高]。高级配方支持 `UseIf` 和 dependency override [高]。
- AppProfile 字段：Flag（`Application` / `Components` / `Virtual`，后者用于纯注册表调整）、最低 CrossOver 版本、**Bottle 模板**（`use` 表示作为主应用时的默认模板，`install` 表示作为依赖被安装时用的模板）、安装检测（Installed Key/Display Pattern 匹配卸载注册表，用 `|||` 分隔；Installed Registry Glob；Installed File Globs，支持 `%ProgramFiles%` 等变量）、Download URL（可按语言区分）、Download Page URL、Download Glob（根据用户手里的 exe 文件名反查配方）、Steam Id、Installer Id（引用另一个 C4 条目作为安装程序）、Application Bottle Group、Extra For [高]。
- InstallProfile 字段：Pre-Dependencies / Post-Dependencies（用 appid 列表**递归**解析依赖，形成依赖 DAG）、CX Diag Check、Installer Globs、Local Installer Globs、Installer Treat As、Installer Environment（`WINE_*` / `CX_*`）、Installer DLL Overrides、Installer Windows Version、Installer Options / Silent Options、PreRmFakeDlls（临时移走 Wine 的 stub DLL）、Pre-Install Registry、Files to Copy、Link Files、自解压参数、Post Install Reboot、Post Register Dlls、Default / Alt File Associations、Post Install URL 等 [高]。
- 规模（2026-09-26 抓取的页面）：库中 **22,667** 个应用，其中 **4,617** 个“金牌”，**4,067** 个可通过 CrossTie 一键安装，CrossTie 累计下载 **600,568** 次，Steam 最多 [27]。
- 评级 [28]：?（无评级）；1★ Will Not Install；2★ Installs, Will Not Run；3★ Limited Functionality；4★ Runs Well；5★ Runs Great。另有用户投票（“想要支持的游戏”）和 BetterTester 积分/经验值激励 [27]。“金牌”的评定规则没有查到公开说明。
- 同步：偏好中的“Automatically Download New Installation Recipes”会定期与兼容性中心同步配方 [23]；Linux 上 `cxtie --register` 用于关联 .tie 文件 [20]。

### 3. 启动器与商店支持、macOS 集成

- **官方明确支持或修复过的启动器**（依据 changelog）[1]：Steam（最常用的 CrossTie）、EA app（23.0 起）、Battle.net、Ubisoft Connect、Rockstar Games Launcher、HoYoPlay，**GOG Galaxy 与 Epic Games Store 在 25.0 正式支持**。这些启动器一更新就经常失效，CrossOver 只能靠点版本修复，例如 24.0.6/24.0.7 修 Battle.net 更新、26.3.0 修 Epic 下载和 GOG Galaxy。
- **macOS 集成** [22][23]：安装完成后自动生成启动器图标（放在 `~/Applications/CrossOver` 一类目录，偏好中可改）[中]，出现在 Launchpad，可拖进 Dock；启动 Windows 应用时 CrossOver 主程序也会随之打开。安装配方可以写入 Default/Alt File Associations，把 Windows 应用注册为宿主系统上某类文件的默认或“打开方式”程序 [25]。**URL scheme 处理没有找到官方文档**（见未解问题）。
- **Retina**：High Resolution Mode 关闭像素倍增，向应用报告 192 DPI，并非所有应用都能正确显示 [16]。

### 4. 命令行工具与自动化接口

- 位置：`/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/` [22]。
- 已核实的用法：`wine --bottle <bottle> --cx-app <exe> [args]` [21][22]；每个已安装应用会生成对应的快捷命令（例如 `bin/winword letter.doc`）[21]；`cxregister`（命令行注册）[21]；`cxbottle`（例如 Linux 上的 `--bottle X --deb --rpm` 打包，以及 `--ro-desktopdata --install` 生成菜单和文件关联）、`cxmenu`、`cxassoc`、`cxtie` [20]；社区脚本中有 `cxbottle --create --template win11_64 --param EnvironmentVariables:…` [38][中]。
- `cxstart`、`cxinstaller` 通常也被列为 bin 下的工具 [中]，但本次**没有找到官方用法文档**。
- 调试：Run with Options 生成 `.cxlog` 文件，Wine 日志通道自动填入，可自定义保存路径 [54][中]。

### 5. 硬件/OS 支持、更新机制、授权与定价（2026）

| 项 | 现状 | 来源 |
|---|---|---|
| CrossOver 26 平台 | Intel 与 Apple Silicon（截至 2026-09-26，产品页仍写“Intel or Apple Silicon based Mac”）；Apple Silicon 上 Wine 是 x86_64，经 Rosetta 2 运行 | [14][43] |
| macOS 对照 | 官网表格覆盖 Catalina(10.15)→Tahoe(26)；Tahoe 需 ≥25.1.1（Sequoia 对应 25.1.1，Sonoma 对应 24）；Apple Silicon 需 macOS 11.1+ 和 CrossOver 21+；**截至 2026-09-26 表中没有 macOS 27 这一行** | [14] |
| CrossOver 27 | 仅 Apple Silicon、macOS 14+；32-bit bottle 完全不能运行；原生 ARM64（用 FEX 是较可信的推断，未核实）；“2027 年初”这个时间来自 AppleInsider 2026-07-31 的报道，不是 6-11 公告原文；据称 97% 用户已在 Sonoma 或更新版本 | [4][5][6][8][15] |
| macOS 27 “Golden Gate” | 已正式发布：apple.com/macos 当前页面展示的就是 macOS 27 Golden Gate，但页面没有写发布日期 [57]。“2026-09-14”这个日期只来自 machow2，没有 Apple 一手来源 [51][中]。截至 2026-09-26，CodeWeavers 没有发布正式兼容声明，产品页兼容表只到 Tahoe [14] | [57][51][14] |
| 自动更新 | Sparkle（出现在 FOSS 清单中；changelog 自 12.1 起就提到 Sparkle 自动更新）；CrossTie 配方单独同步 | [1][29] |
| 价格 | CrossOver+ **$74**/12 个月（含 Preview）；CrossOver Life **$494**（无退款）；常有 15%–75% 的促销 | [12][2][3] |
| 续费 | 支持期到期后 30 天内续费享折扣；OMG! Ubuntu 报道为 50% off | [13][2] |
| 授权 | 按人授权，可装在任意多台机器上；支持期内获得的版本永久可用、可重新下载；试用 14 天，到期后已安装的应用被禁用 | [13][22] |
| 退款 | 购买 30 天内且**未激活**可退；教育优惠 CrossOver+ 30%、Life 20%（需在购买前申请） | [13] |

### 6. 让 CrossOver 显得精致的 UX 细节、错误处理与支持

- **“有清单的安装”**：Install details 把安装程序、bottle、依赖逐项打勾，全部就绪才允许 Install。取消安装会提示“可能导致 bottle 不可恢复”，并提醒用户先找找是否有被遮住的 Windows 对话框 [23]。
- **默认安全**：每个应用一个 bottle；未收录的应用建议放进单独的新 bottle；修改前建议先导出归档，作为快照 [23]。
- **可发现的逃生通道**：Quit All / Force Quit、Simulate Reboot、Task Manager、Repair Bottles、Clear and Rebuild Programs、Open Shell [15][22]。
- **支持闭环**：Run with Options 生成 `.cxlog` 并引导压缩后交给技术支持；付费用户有邮件和电话支持；Preview 用户通过 Preview Center 提交反馈并获得 XP [50][54]；应用内提醒用户提交评分，数据回流兼容性库 [23]。
- **Auto 后端 + 逐游戏数据库**：多数用户不用理解 D3DMetal 和 DXMT 的区别 [16]。
- 不足：设置是**按 bottle 全局生效**（原文：“is used for all applications installed in the bottle”），不能按应用单独设置 [16]；Metal HUD、AVX 开关等要改 conf 文件，社区论坛已有 Metal Performance HUD 的功能请求 [55]。

### 7. 2025–2026 用户抱怨与弱点（Cider 的机会）

| 痛点 | 证据 | 置信度 |
|---|---|---|
| 启动器/游戏更新后失效，要等点版本 | 24.0.1–26.3.0 的 changelog 以“after update”类修复为主 [1] | 高 |
| 配方过时、下载链接失效、兼容性测试日期很老（“三五年甚至更早”） | Dedoimedo 评测（2026-04 更新）[44] | 中 |
| 反作弊（EAC/BattlEye 等内核级）不可用；官方以 DMCA 为由不做绕过，并说明很多开发商未启用 Proton 支持 | 官方支持页 [40]、2026-08-31 博客 [41] | 高 / 中 |
| 需要反复试后端，部分游戏完全不能运行 | machow2 26 评测 [42] | 中 |
| 外接显示器上光标和窗口尺寸异常；D2R 按 Cmd-Tab 会崩溃 | Macworld（2026-08-03 更新）[43] | 中 |
| 依赖 Rosetta；ARM64 版不能迁移旧 bottle；32-bit bottle 被取消 | Rosetta 退场 [10]；旧 bottle 不能转换 [8]；32-bit 取消 [5][6][15] | 高 / 中 / 高 |
| 客服响应慢、退款严格、区域定价引起不满 | Trustpilot 2025–2026 评价 [45] | 中（样本有偏） |
| 缺少内置性能 HUD 和诊断面板 | 论坛功能请求 [55] | 中 |
| Reddit r/macgaming | 本次搜索引擎未收录 Reddit，无法取得一手数据 | — |

背景：Whisky 于 2025-04 停止开发，作者建议用户改用 CrossOver，理由之一是免费前端几乎不向 Wine 回馈，还可能损害 CrossOver 的收入 [46]。Kegworks 已更名为 Sikarugir，仍在维护，但它是“Wine 包装器”式的工具，D3DMetal 需用户自行启用，并注明其许可证有限制 [47]。

### 8. 开源与专有的精确边界

| 组件 | 性质 | 说明 |
|---|---|---|
| CrossOver 修改版 Wine（含 Mac 驱动、MSync 等补丁，推断） | LGPL-2.1，**公开** | 每个版本发布 `crossover-sources-<ver>.tar.gz`，下载路径为 `media.codeweavers.com/pub/crossover/source/`；GitHub 上有第三方镜像（marzent/winecx、Gcenx/winecx）[29][49] |
| vkd3d、DXVK、MoltenVK、FAudio、SDL、GnuTLS、Samba、wine-mono、FreeType、LLVM、Python、PyObjC、Sparkle、cabextract、UnRAR 等 | 各自的开源许可证，**公开**（源码页共列出 29 个 FOSS 项目；页面列的是项目清单而不是 tarball 目录，tarball 未下载，内容没有逐项核实） | [29] |
| DXMT | 上游开源；**v0.80（2026-04-23）是最后一个 MIT 版本，此后改为 LGPL**（已核实）。CrossOver 26 用的是 v0.72（2025-12-11），上游之后有 v0.73（2026-01-21）、v0.74（2026-03-10）、v0.80 | 源码页 FOSS 清单中没有它，但上游可直接获取；Cider 可以跟踪比 CrossOver 26.x 更新的版本 [31] |
| FEX（ARM64 版可能使用，属推断） | 上游 MIT。上游已有面向 Wine 的 PE 模块：ARM64EC 构建（arm64ec 目标）和 WOW64 构建，位于 `Source/Windows`，用 MinGW/Clang 编译，通过 `wine_builtin.bin` 标记为 Wine builtin，在 Windows 侧运行，不依赖 Linux 宿主 [58][59]。**CodeWeavers 在 Mac 上是否用 FEX 没有核实，其 macOS 定制版是否公开源码也不知道** | [52][58][59][7][9] |
| **D3DMetal.framework**（含 nvngx-on-metalfx、nvapi64 等 DLSS→MetalFX 组件） | **Apple 专有**，受 GPTK 许可证约束；位于 `/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64/apple_gptk/external`（AppleGamingWiki 核实 [34]；mybyways 针对 25.1.1 也描述了 `Contents/SharedSupport/CrossOver` 下的 apple_gptk 目录 [35]）；**源码页 FOSS 清单没有列出它**（tarball 未下载核实） | [34][35][29] |
| CrossOver GUI 应用、`cx*` 工具链（推断）、注册和授权机制 | **CodeWeavers 专有**（EULA：含“trade secrets”，禁止逆向、反编译、分发和制作衍生作品；法律条款，本轮事实核查按项目约定跳过，没有复核） | [30] |
| CrossTie/C4 兼容性数据库、配方库、逐游戏 Auto 配置数据库 | **专有服务和数据**（配方格式有公开文档，数据本身不开源） | [16][24][27] |

---

## 对 Cider 的启示与建议（按优先级）

**P0：先定路线，避免一开始就做成“CrossOver 26 的复制品”**
1. **平台基线直接对齐 CrossOver 27**：只支持 Apple Silicon，macOS 最低 14（开发和测试以 26.x 为主），**只做 64-bit bottle**（新 WoW64，32 位程序在 64 位 prefix 中运行）。不要投入任何 32-bit bottle 或 Intel Mac 的工作。Engine B 的最低 macOS 版本**不要照搬“ARM64 部分需 macOS 26.5+”这个说法**。它目前只来自搜索摘要，没有核实 [7]，应以 CodeWeavers 的一手发布说明或 Cider 自己的原型结果为准。
2. **双引擎架构，接口先抽象好**：
   - Engine A（先行上线）：x86_64 Wine（基于 `crossover-sources-26.3.0` 或上游 Wine 11.x 加选定补丁），通过 Rosetta 2 运行。技术成熟，兼容性最高。注意 macOS 26.4 起，系统可能在启动依赖 Rosetta 的应用时弹出通知 [10]，Engine A 会触发这个提示，需要在 UI 和文档里提前解释，并给出迁移到 Engine B 的路径。
   - Engine B（必须在 2027 秋 macOS 28 之前可用）：ARM64 Wine 加 x86 模拟器。**直接以上游 FEX（MIT）已有的 ARM64EC / WoW64 PE 模块为起点**（`Source/Windows`，作为 Wine builtin 在 Windows 侧运行）[58][59]，不必从零移植整个 FEX。主要工作量在 macOS 特有问题的适配上，如 16K 页、JIT/W^X、TSO（推断，需要原型验证）。Rosetta 在 macOS 28 之后只保留有限功能 [10]，Wine 是否属于被保留的“老游戏”范围不确定，**不能指望**。
   - Bottle 元数据要记录 engine 架构，从第一天起就设计 **x86_64 → ARM64 的 bottle 迁移工具**（CrossOver 目前没有，这是差异化点）。
3. **图形后端策略**：DX10/11 默认用 DXMT（开源，可随包分发），并**跟踪上游最新版**（≥ v0.80；CrossOver 26.x 还停在 v0.72）[31]。DX12 目前只有 D3DMetal 可行。**不要把 D3DMetal 提交进 Cider 仓库或主 app bundle**，而是把它做成单独安装的可选组件（完整的 D3DMetal.framework），由 Cider 检测并加载：默认让用户从 Apple GPTK 导入；读过许可证原文后，再决定要不要像 Sikarugir 那样作为可选下载提供。Sikarugir 把 D3DMetal 做成可选开关分发，并附带“D3DMetal-v3.0 License”文件 [47]；第三方转述称 GPTK 许可允许非商业、完整分发 [33]；Apple 的 GPTK 页面没有写再分发条款 [37]。DXVK + MoltenVK 作兜底；DX9 及更早用 wined3d。后端开关要能**按应用**覆盖，而不只是按 bottle。
4. **配置格式**：参照 `cxbottle.conf` 的语义，设计一份有版本号的 TOML/JSON（包括 graphics backend、msync、metalfx、avx_advertise、metal_hud、retina、dll overrides、env），由 GUI 和 CLI 共用。

**P1：补齐 CrossOver 的核心体验**
5. **声明式安装配方（Cider Recipes）**：参考 CrossTie 的三段式（Pre / Install / Post）和 appid 依赖 DAG，改进几点：每个下载都用 **SHA-256 固定**，并配备镜像或存档地址；CI **每天校验链接**，解决 CrossOver 配方链接失效的问题；配方仓库开源在 GitHub，接受 PR，客户端**热更新配方和逐游戏配置，不依赖发版**，用来对冲“启动器一更新就坏”的问题。
6. **启动器优先级**：Steam → Battle.net → EA app → Epic → GOG Galaxy → Ubisoft Connect → Rockstar。为每个启动器建立**夜间自动化冒烟测试**（启动、登录页渲染、下载小文件），启动器更新后尽快发现问题并推送配置热修复。
7. **macOS 集成**：生成 `.app` 启动器（放入 `~/Applications/Cider/`），带真实图标并支持 Spotlight 和 Dock；通过生成的 bundle 的 `Info.plist` 声明文件类型和 URL scheme，实现文件关联和 URL 处理；提供 Run Command 与“另存为启动器”、Open Shell、导出/导入/复制 bottle、Repair、重建启动器。
8. **CLI（`cider`）从一开始就是一等公民**：`cider bottle create --template win11_64`、`cider run --bottle X --app foo.exe`、`cider install <recipe>`、`cider export`、`cider logs`。GUI 只是 CLI 和守护进程的前端，便于自动化测试和 CI。
9. **开放的逐游戏自动配置库**（替代 CrossOver 专有的 Auto 数据库）：按 exe 哈希、Steam AppID 或路径匹配 → 后端、MSync、AVX、MetalFX、环境变量、DLL override。这部分数据开源，可审计。

**P2：做出超过 CrossOver 的地方**
10. **诊断**：一键打开 Metal HUD（`MTL_HUD_ENABLED`），内置 FPS 和帧时间叠加层；一键生成支持包（日志、配置、系统信息，默认脱敏）；崩溃时给出可操作的建议（“换成 DXMT 试试”）。
11. **社区兼容性报告**（类似 ProtonDB）：按 Cider 版本、engine、后端、芯片型号记录报告，自动标记过时数据。这直接解决 CrossOver 兼容性数据老旧的问题。
12. **明确说明反作弊的边界**：像 CodeWeavers 一样不做任何绕过（DMCA 风险），在 UI 中提前标注“不可用：内核级反作弊”，减少差评。
13. **更新**：用 Sparkle 2（EdDSA 签名、delta 更新）；配方和配置走独立的签名通道。

**生态与合规**
14. 严格遵守 LGPL：公开 Cider 所用 Wine 的完整源码和构建脚本；**不要逆向 CrossOver 的专有部分**（EULA 条款本轮核查未复核），只使用公开文档和 FOSS 源码，以净室方式实现 GUI 和配方系统。
15. 汲取 Whisky 的教训 [46]：把 Wine、DXMT、MoltenVK 的修复**提交回上游**，降低“免费产品挤压 Wine 资金来源”的社区观感风险。

---

## 风险

1. **D3DMetal 依赖**：DX12 在 macOS 上几乎只能靠 D3DMetal。它是闭源的，许可证限制未经原文核实（可以先读 Sikarugir 附带的“D3DMetal-v3.0 License”文件 [47]），Apple 随时可能改条款或停止更新；CrossOver 的 ARM64 Preview 目前也还不支持它 [8]。一旦失去这条路径，Cider 的 AAA 兼容性会大幅下降。
2. **Rosetta 退场的时间压力**：macOS 27 是最后一个完整支持 Rosetta 的版本 [10]。它已正式发布 [57]，但“2026-09-14”这个日期只来自 machow2，属中置信度 [51]。macOS 28（约 2027 秋）之后 x86_64 Wine 可能无法运行，Cider 的 ARM64 + FEX 路线必须在大约 12 个月内成熟。CodeWeavers 的 macOS 版 FEX 是否公开确实未知，但上游 FEX（MIT）已有面向 Wine 的 ARM64EC / WoW64 PE 模块 [58][59]，Cider 可以从上游起步。主要风险在 macOS 特有问题（16K 页、JIT/W^X、TSO）的适配，而不是从零移植整个 FEX（推断，需要原型验证）。
3. **维护负担**：CodeWeavers 有专职 QA 团队，大约每 2 个月发一个点版本来追赶启动器变化 [1]。个人或小团队如果没有自动化测试和热更新通道，会被启动器更新持续拖垮。
4. **法律与商标**：“Cider”这个名字至少与 TransGaming 早年的 Wine 类 Mac 移植引擎 Cider、以及同名 Apple Music 第三方客户端有冲突风险（依据既有知识，**本次未联网核实**），需要做商标检索；另外不能使用 CrossOver 或 CodeWeavers 的商标和素材，并遵守 GPTK 许可证与 DMCA。
5. **开发硬件**：开发机只有 8 GB 统一内存。第三方打包的 GPTK 建议 16 GB 以上 [中]，AAA 游戏测试和 Wine 全量构建都会受限，需要至少一台 16–24 GB 的测试机。
6. **社区观感**：免费替代品可能被视为分流 Wine 资金（Whisky 事件）[46]，需要用上游贡献来对冲。
7. **信息风险**：codeweavers.com/blog 与论坛在本次调研中无法直接抓取（事实核查时仍返回 403，archive.org 副本也打不开）。CrossOver 27 和 ARM64 Preview 的细节依赖搜索摘要和媒体转述。核查后仍未核实的有：ARM64 Preview 使用 FEX、universal build、ARM64 部分需 macOS 26.5+、内含 ARM64 DXMT；CrossOver 27 的“2027 年初”只有 AppleInsider 2026-07-31 一个来源。CodeWeavers 的 Preview 页面也已过时，仍写着 Preview 只能在 Intel 处理器的电脑上运行 [50]。

## 未解问题

1. CrossOver 26.x 是否已正式声明支持 macOS 27 Golden Gate？截至 2026-09-26，产品页兼容表没有 macOS 27 这一行 [14]。CodeWeavers 在 Facebook 上提到的“small hiccup”具体是什么？
2. “ARM64 部分要求 **macOS 26.5+**”、“universal build”、“内含 ARM64 DXMT”这几种说法是否属实？目前只来自搜索摘要 [7]，AppleInsider 的全文没有提到 [8]。26.5 这个门槛非常具体，又直接影响 Cider Engine B 的最低系统版本，必须先从官方博客或 release notes 确认。如果属实，原因是什么：依赖了新的系统 API（例如 JIT、内存模型或 TSO 相关），还是别的原因？
3. CodeWeavers 在 Mac ARM64 Preview 中是否确实使用 **FEX**（目前是较可信的推断 [9]）？其 macOS 版 FEX 源码是否公开？ARM64 版采用的是 ARM64EC 还是纯 ARM64 + WoW64 的组合？上游 FEX 两种 PE 模块都有 [58][59]。
4. GPTK 3 / GPTK 4 的 **License 原文**对 D3DMetal 的再分发、非商业用途、面向终端用户游玩的具体限制（它决定 D3DMetal 是作为可选下载提供，还是只能由用户导入；可以先读 Sikarugir 附带的 License 文件 [47]）；CodeWeavers 与 Apple 之间协议的具体条款（未公开）。
5. D3DMetal 4（GPTK 4，WWDC26 发布，支持 Metal 4）何时进入 CrossOver，是否要求 macOS 27？
6. CrossTie Install Profile 的**完整 XML 元素名**（官方页面只描述了编辑器字段）；“金牌”评级规则；兼容性数据的过期机制。
7. CrossOver Mac 的 URL scheme 处理和默认文件关联的具体实现（生成的启动器 bundle 是否声明 `CFBundleDocumentTypes` / `CFBundleURLTypes`）；启动器目录的确切默认路径。
8. `cxstart`、`cxinstaller`、`cxdiag` 在 Mac 版 bin 下的确切参数（官方文档未覆盖）。
9. 26 的设置界面是否确实移除了 ESync（26 的文档不再列出，25 的文档仍有）。
10. Reddit r/macgaming 的一手用户反馈（本次搜索无法获取）。
11. `crossover-sources-26.3.0.tar.gz` 的实际内容：是否含 DXMT、ARM64/FEX 相关源码？源码页只列 FOSS 项目清单，本次没有下载 tarball [29]。用这些源码能否独立构建出与 CrossOver 等价的 Wine？Gcenx 的 port 只重新打包了官方二进制，不能作为证据 [60]。

## 参考来源

1. https://www.codeweavers.com/crossover/changelog — 官方 ChangeLog（23.0.0–26.3.0 各版本日期与条目）
2. https://www.omgubuntu.co.uk/2026/02/crossover-26-released — CrossOver 26 报道（组件版本、价格、续费 50% off）
3. https://appleinsider.com/articles/26/02/10/crossover-26-update-adds-compatibility-for-blockbuster-expedition-33-and-helldivers-2 — CrossOver 26 新支持游戏与首发促销价
4. https://www.codeweavers.com/blog/mjohnson/2026/6/11/whats-in-and-whats-out-for-crossover-27 — 官方博客：CrossOver 27 的取舍（抓取 403，内容来自搜索摘要）
5. https://appleinsider.com/articles/26/06/11/crossover-a-windows-to-mac-gaming-tool-goes-apple-silicon-only — CrossOver 27 仅支持 Apple Silicon 的报道
6. https://gigazine.net/gsc_news/en/20260612-crossover-27-removes-legacy-support-mac-intel/ — CrossOver 27 报道（97% 用户在 Sonoma 及以上）
7. https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac — 官方博客：Mac ARM64 Preview（抓取 403；FEX、universal、需 26.5+、ARM64 DXMT 只来自搜索摘要，未核实）
8. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — ARM64 Preview 的三条限制与“2027 年初”发布计划（全文没有提到 FEX、universal build、macOS 26.5 或 DXMT）
9. https://www.gamingonlinux.com/2025/11/codeweavers-launch-a-new-crossover-preview-adding-linux-arm64-support/ — Linux ARM64 Preview（2025-11，FEX 集成，ARM64EC 时间线）
10. https://developer.apple.com/news/?id=w5ngl9k2 — Apple：Rosetta 支持变更（macOS 27 为最后完整支持版本，老游戏例外）
11. https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/ — WWDC25 宣布 Rosetta 逐步退场
12. https://www.codeweavers.com/store — 官方商店（CrossOver+ $74、Life $494、试用）
13. https://www.codeweavers.com/store/licensing — 授权与支持政策（按人授权、永久使用、续费、退款、教育优惠）
14. https://www.codeweavers.com/crossover — 产品页（系统要求、macOS 与版本对照表）
15. https://support.codeweavers.com/user-guides/crossover-mac-user-guide — Mac 用户指南（功能全貌、偏好、32-bit 选项）
16. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26 — 26 版 Advanced Settings 原文
17. https://support.codeweavers.com/miscellanous/advanced-settings-in-crossover-mac — 25 版 Advanced Settings（ESync/MSync/Default）
18. https://support.codeweavers.com/miscellanous/advanced-settings-in-crossover-237 — 23.7–24 版 Advanced Settings（CSMT 默认开启）
19. https://support.codeweavers.com/advanced-crossover-mac-configuration — Mac 高级配置（路径、plist 键、模板、hooks）
20. https://support.codeweavers.com/advanced-crossover-linux-configuration — Linux 高级配置（cxbottle/cxmenu/cxassoc/cxtie、hook 触发点、环境变量）
21. https://support.codeweavers.com/user-guides/crossover-linux-user-guide — Linux 指南（cxregister、wine --bottle --cx-app）
22. https://www.codeweavers.com/support/docs/crossover-mac/index — Mac 用户指南（Run with Options、Open Shell、Publish Bottle、bin 路径）
23. https://media.codeweavers.com/pub/crossover/docs/en/userguide-crossover-mac-22.pdf — CrossOver 22 Mac 用户指南 PDF（安装流程与偏好细节）
24. https://support.codeweavers.com/crosstie-data-startpage/c4-data-file-framework — C4P/tie XML 框架（c4p、autorun、appid、cxversion）
25. https://support.codeweavers.com/crosstie-data-startpage/an-intermediate-guide-on-what-the-crosstie-editor-options-mean — CrossTie 编辑器字段详解
26. https://support.codeweavers.com/crosstie-data-startpage/c4-data-install-profile — Pre/Install/Post Profile 结构、UseIf
27. https://www.codeweavers.com/compatibility — 兼容性中心统计（应用数、金牌数、CrossTie 数与下载量）
28. https://www.codeweavers.com/compatibility/rating-system — 1–5★ 评级定义
29. https://www.codeweavers.com/crossover/source — 源码发布页（crossover-sources-26.3.0 与 FOSS 清单）
30. https://www.codeweavers.com/crossover/eula — EULA（专有部分、禁止逆向、FOSS 附录）
31. https://github.com/3Shain/dxmt/releases — DXMT 版本与 MIT→LGPL 许可证变更
32. https://github.com/3Shain/dxmt/releases/tag/v0.72 — DXMT v0.72（D3DKMT 需 Wine 10.18+，实验性 Intel 支持）
33. https://github.com/dbc-hbin/d3dmetal-redistributable — 第三方对 GPTK 许可证 2A/2C 的转述（非商业、完整分发）
34. https://www.applegamingwiki.com/wiki/Game_Porting_Toolkit — D3DMetal 在 CrossOver.app 中的路径
35. https://mybyways.com/blog/updating-crossover-to-gameporting-toolkit-3-0 — 手动替换 D3DMetal、nvngx/nvapi、cxbottle.conf 环境变量
36. https://appleinsider.com/articles/26/06/08/game-porting-toolkit-4-ushers-in-support-for-agentic-coding — GPTK 4（WWDC26）
37. https://developer.apple.com/games/game-porting-toolkit/ — Apple GPTK 官方页（GPTK 4，Metal 4）
38. https://github.com/stoicswe/Endfield_FineWine/pull/15 — 社区整理的 cxbottle.conf 键（CX_GRAPHICS_BACKEND 等）
39. https://github.com/matthiasSchedel/rdr2-crossover-apple-silicon/blob/main/docs/esync-msync-ab.md — 社区 A/B 方案（WINEMSYNC/WINEESYNC/MTL_HUD_ENABLED）
40. https://support.codeweavers.com/anti-cheat — 官方反作弊立场（DMCA）
41. https://www.codeweavers.com/blog/mjohnson/2026/8/31/why-do-most-games-with-anti-cheat-not-work-with-crossover-mac — 官方博客：反作弊为何不可用（内容来自搜索摘要）
42. https://machow2.com/crossover-mac-review/ — CrossOver 26 评测（后端说明、弱点、价格）
43. https://www.macworld.com/article/2276922/crossover-for-mac-review.html — Macworld 评测（2026-08-03 更新，外接显示器与 Cmd-Tab 问题）
44. https://www.dedoimedo.com/computers/crossover-mac.html — 评测（配方过时、链接失效、兼容数据陈旧）
45. https://www.trustpilot.com/review/www.codeweavers.com — 用户评价（退款、客服响应）
46. https://www.macrumors.com/2025/04/23/whisky-ends-mac-gaming-tool-crossover/ — Whisky 停止开发并推荐 CrossOver
47. https://github.com/Sikarugir-App/Sikarugir/blob/main/README.md — Sikarugir（Kegworks 后继）README
48. https://github.com/Gcenx/macports-wine — MacPorts overlay（CrossOver 26.3.0 port 是对官方二进制的重新打包，不是源码构建；wine-devel 11.18）
49. https://github.com/marzent/winecx — CrossOver Wine 源码镜像
50. https://www.codeweavers.com/preview — CrossOver Preview 计划说明
51. https://machow2.com/macos-27-golden-gate-compatibility/ — macOS 27 发布日期与 CrossOver 兼容状态
52. https://github.com/FEX-Emu/FEX — FEX（MIT 许可证）
53. https://www.codeweavers.com/about/news/press/20230927 — CrossOver 23.5 新闻稿（D3DMetal 集成）
54. https://support.codeweavers.com/troubleshooting/creating-a-debug-log — 调试日志（.cxlog）
55. https://www.codeweavers.com/support/forums/general?t=27&forumcurPos=5&msg=347467 — 论坛功能请求：Metal Performance HUD
56. https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/4193 — ROSETTA_ADVERTISE_AVX 的作用（Heroic issue）
57. https://www.apple.com/macos/ — Apple macOS 页面（当前版本显示为 macOS 27 Golden Gate，没有发布日期）
58. https://raw.githubusercontent.com/FEX-Emu/FEX/main/Source/Windows/CMakeLists.txt — FEX 的 Windows 侧 PE 模块（ARM64EC / WOW64 子目录，经 wine_builtin.bin 标记为 Wine builtin）
59. https://raw.githubusercontent.com/FEX-Emu/FEX/main/CMakeLists.txt — FEX 顶层构建（MINGW、ARCHITECTURE_arm64ec 分支）
60. https://raw.githubusercontent.com/Gcenx/macports-wine/master/emulators/crossover/Portfile — Gcenx CrossOver port 的 Portfile（下载官方二进制试用包后重新打包并 ad-hoc 签名）

---

## 事实核查记录

> 独立事实核查于 2026-09-26 完成。结论取值：确认 / 部分属实 / 驳回。本轮没有出现核查者之间结论相互矛盾的情况，因此没有标“存疑”的条目。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 截至 2026-09-26 最新稳定版是 26.3.0（2026-07-21）；26.0.0 于 2026-02-10 发布，基于 Wine 11.0，Mac 端含 D3DMetal 3.0、DXMT v0.72，另含 vkd3d 1.18、Wine Mono 10.4.1 | 确认 | 官方 changelog 逐条吻合 [1]。补充：DXMT v0.72 是上游 2025-12-11 的标签，上游之后已到 v0.80（2026-04-23），CrossOver 26.x 自带的 DXMT 落后好几个版本，Cider 可以用更新的版本做基线 [31]。已补入摘要、版本表和第 3 条建议 |
| 2026-06-11 宣布：CrossOver 27（计划 2027 年初）只支持 Apple Silicon 和 macOS 14+，不再运行 32-bit bottle | 部分属实 | Apple Silicon only、macOS 14+、32-bit bottle 在 27 中完全不能运行，这三点有两家独立媒体和官方 26 用户指南支持 [5][6][15]。“2027 年初”并非 6-11 公告中核实到的内容，只出现在 AppleInsider 2026-07-31 的 ARM64 Preview 报道中 [8]。官方博客 403，没有读到原文 [4]。摘要、版本表和第 5 节表格已改 |
| 2026-07-31 Mac 版 Preview 首次提供原生 ARM64 Wine：x86/x64 由定制 FEX 模拟；universal build；ARM64 部分需 macOS 26.5+；内含 ARM64 DXMT；暂不支持 D3DMetal/DX12；旧 bottle 不能转换 | 部分属实 | 已核实：7-31 发布原生 ARM64 Preview；这个构建不含 D3DMetal，DX12 支持“即将推出”；许多启动器不能用；旧 bottle 不能转换；三条限制都预计在 27 之前解决 [8]。FEX、universal build、macOS 26.5+、ARM64 DXMT 这四点只来自搜索摘要，未核实，已降为[低]。FEX 是较可信的推断，因为 Linux ARM64 Preview 已集成 FEX [9]。第 1 条建议已改：Engine B 的最低系统版本不要依赖 26.5 这个说法 |
| macOS 27 是最后一个完整支持 Rosetta 的版本，此后仅保留对老游戏的有限支持 | 确认 | Apple Developer News 原文吻合 [10][11]。补充：Apple 没有说明 Wine 是否属于老游戏例外；macOS 26.4+ 启动依赖 Rosetta 的应用时可能弹出系统通知，Cider 的 x86_64 Engine A 会触发这个提示。已补入摘要和第 2 条建议 |
| `crossover-sources-26.3.0.tar.gz` 包含 Wine、vkd3d、DXVK、MoltenVK、wine-mono 等 FOSS 组件，不含 D3DMetal（路径 `.../lib64/apple_gptk/external`）；EULA 禁止逆向 | 部分属实 | 源码页提供该 tarball，并列出 29 个 FOSS 项目。页面没有提到 D3DMetal，也没有提到 DXMT 和 ARM64/FEX 源码；tarball 没有下载，内容未逐项核实 [29]。D3DMetal 路径由 AppleGamingWiki 核实 [34][35]。EULA 属法律范畴，跳过。D3DMetal 的打包方式：Sikarugir 以可选开关分发 D3DMetal 并附带 License 文件 [47]，第三方转述称 GPTK 许可允许非商业、完整分发 [33]，Apple GPTK 页面没有写再分发条款 [37]。第 3 条建议已从“只允许用户导入”改为“不进仓库和主 bundle，做成可选组件，读完许可原文后再决定是否提供可选下载” |
| CrossOver+ $74（12 个月更新与支持）；Life $494；14 天试用；按人授权、机器数量不限；支持期内版本永久可用 | 确认 | 官方商店与授权页吻合 [12][13]（续费折扣期 30 天、未激活 30 天内可退、教育优惠 30%/20% 也一致）。未改动 |
| Gcenx macports-wine 已提供从源码构建的 CrossOver 26.3.0 port，可见 26.3.0 源码可用于第三方构建 [高] | 驳回 | Portfile（`license Commercial`、`supported_archs x86_64`、`use_configure no`）从 `media.codeweavers.com/pub/crossover/cxmac/demo/` 下载官方二进制 `crossover-26.3.0.zip`，然后删去部分 DXVK DLL、可选改用 GStreamer.framework、去掉 Sparkle URL、ad-hoc 签名，全过程不编译 [60]。它不能证明源码可由第三方完整构建；能证明源码可获取的是 tarball 本身 [29] 和 winecx 镜像 [49]。1.1 节“补充说明”已改，并新增未解问题 11 |
| 风险 2：CodeWeavers 的 macOS FEX 移植是否开源未知，Cider 需自行把 FEX 移植到 macOS，工作量很大 | 部分属实 | CodeWeavers 的 macOS 版 FEX 是否公开确实未知。但上游 FEX（MIT）已有面向 Wine 的 ARM64EC / WOW64 PE 模块，用 MinGW/Clang 编译，经 `wine_builtin.bin` 标记为 Wine builtin，在 Windows 侧运行，不依赖 Linux 宿主 [58][59][52]。Cider 可以从上游起步，主要风险在 macOS 特有问题（16K 页、JIT/W^X、TSO）的适配（推断，需原型验证）。风险 2、第 8 节 FEX 行和第 2 条建议（Engine B）已改 |
| DXMT 上游开源；v0.80 是最后一个 MIT 版本，此后改为 LGPL | 确认 | releases 页原文吻合 [31]。补充了 v0.72 之后的标签：v0.73（2026-01-21）、v0.74（2026-03-10）、v0.80（2026-04-23） |
| macOS 27 “Golden Gate” 已于 2026-09-14 发布；CodeWeavers 尚未发布正式兼容声明；官网系统要求表未列 macOS 27 | 部分属实 | apple.com/macos 当前页面展示 macOS 27 Golden Gate，可以确认已正式发布，但页面没有写日期 [57]；“2026-09-14”只来自 machow2 [51]。截至 2026-09-26，产品页兼容表只到 Tahoe（26.0，对应 25.1.1），没有 macOS 27 行，页面仍写支持 Intel 或 Apple Silicon Mac [14]。第 5 节表格、风险 2 和未解问题 1 已改 |
| 26 版 Advanced Settings：Auto（专有逐游戏数据库）、D3DMetal（DX11/12）、DXMT、DXVK、wined3d；DLSS 仅对 D3DMetal 和 DXMT 生效；MSync；High Resolution Mode 192 DPI；ESync 不再列出；设置作用于 bottle 内所有应用 | 确认 | 官方 26 版页面吻合 [16]。小修正：官方文档只把 DXVK 描述为“for Direct3D 10 and 11”，摘要和 1.2 节表格已注明 |
