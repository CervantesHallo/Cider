# 开源先例与可复用资产：Whisky、Kegworks/Sikarugir、Heroic、Mythic、Porting Kit、Gcenx 构建、Bottles/Lutris/umu

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 本次直接读取一手来源（GitHub 仓库/源码文件/API、release 页、官方文档、Homebrew cask 文件）；**[中]** = 可信第三方（媒体、社区站点、搜索摘要）或只做了部分验证；**[低]** = 推断或估算。所有 star 数和 pushed_at 都取自 2026-09-26 当天的 GitHub API。codeweavers.com 博客对抓取返回 403，相关内容只能借搜索摘要获得。“节省工作量”是工程估算，统一标为 [低]。本文不单独做法律分析，许可证只作为“能不能直接拿来用”的工程约束列出。与 01/02/04/06 号报告重叠的部分（CX 源码差异、DXMT、rosettax87 等）只做交叉引用，不重复展开。

> **修订说明（2026-09-26）**：本版已按独立事实核查结论修订。共核查 11 条声明：7 条属实，4 条部分属实，没有被驳回的。部分属实的内容已在正文原处更正，并注明“（核查更正）”；涉及的建议（P0-3、P1-5、P1-8）和风险 3 也已同步调整。逐条结论见文末“事实核查记录”。

---

## 摘要

- **Whisky 已死，而且原版已不能自举**：维护停止公告最后修改于 **2025-04-09**（docs commit `f157e66`）[3]；仓库最后一次推送在 **2025-05-11**，之后归档，许可证 GPL-3.0，15,107 star [1][2]。停更原因由作者 Isaac Marovitz 本人说明：对项目失去兴趣，作为学生无偿维护耗时过多；认为 Whisky “寄生”在 CrossOver 之上，几乎没有向 Wine 回馈，反而损害 CodeWeavers 的收入，而这笔收入在资助 Wine 的 Mac 开发 [3][4][5]。2026-09-26 实测 `data.getwhisky.app/Wine/WhiskyWineVersion.plist` 和 `Libraries.tar.gz` 都返回 **HTTP 404** [15]。也就是说，原版 Whisky 2.3.5 在新装机器上已经下载不到自己的 Wine。[高]
- **WhiskyWine 的内容** [12]：Wine 7.7 分支（CrossOver 22.1.1 + GPTK 源码）的 `wine64` 与 `wine32on64`，加上 DXVK-macOS、GPTK `redist`（D3DMetal）、winetricks、Wine Mono 7.4.1，打包成 `Libraries.tar.gz`，托管在作者个人的 Cloudflare R2 上。[高]
- **2026 年最活跃的 Whisky 后继是 `frankea/Whisky`**：2026-01-05 创建，GPL-3.0，788 star，App 版本 3.7.0（2026-08-30）[16][17]。
  - 稳定运行时用的是 Gcenx 打包的**上游 Wine 11.0** [21]。
  - 另有 beta 引擎 “Wine Libraries” v4.x，由 `dappermint/winecx-gptk` 的 CI 构建：把 CX 26.3 的改动前移到 Wine 11.16/11.17 上，D3DMetal 由**用户自行导入** GPTK [18][19][22][73]。
  - 治理文档写明“release 的 bus factor 为 1”[20]。[高]
- **关键技术事实**：`dappermint/winecx-gptk` 的 README 写明，D3DMetal 只能在 **CrossOver 派生的 Wine** 上运行，因为它在加载时会 patch CX Wine 的 unixcall 内部结构 [22][73]。这与 02 号报告对 `macdrv_functions`/CW Hack 的源码分析一致。[高]
- **Homebrew 渠道已经断了**：官方 cask `wine-stable`（11.0_1）和 `wine@devel` 在 **2026-09-01** 被禁用，理由是 `:fails_gatekeeper_check` [47][48]。依据是 Homebrew 5.0.0 的政策：官方仓库中未通过 Gatekeeper 的 cask 一律禁用 [49][50]。第三方 tap 仍可使用，但 6.0.0 起需要先 `brew trust` [50]。[高]
- **Apple 的 GPTK Homebrew 工作流停在 Wine 7.7**：`apple/homebrew-apple` 的 `game-porting-toolkit` formula 版本号仍是 1.1，源码是 `crossover-sources-22.1.1` [51]。Gcenx 的 GPTK 3.0-3 二进制（2026-03-03）同样基于 homebrew-apple commit `2bc4428`，`VERSION` 文件为 “Wine version 7.7”，建议内存 16 GB [45][76][77]。“Wine 7.7”只对 homebrew-apple 和 Gcenx 这条线核实过；GPTK 4 评估环境带不带 Wine、带的是哪个版本，没有找到一手来源（Apple 页面只指向 Homebrew 和 CrossOver）。GPTK 4 在 WWDC26 发布，评估环境 4.0 的 beta 1 约在 2026-06-17 推出，只支持 Apple Silicon，支持 Metal 4 [52][53]。[高/中]
- **Gcenx（Dean M Greer）是 macOS Wine 生态的单点**：他维护 WineHQ 官方 macOS 包（最新 11.18，2026-09-25，仅 x86_64）[43][82]、MacPorts overlay（`wine-devel` 11.18、CX 26.3.0、sikarugir 1.0.1）[44]、GPTK 重打包 [45]、Sikarugir 引擎 [25]，以及 NotProton [58]。[高]
- **Sikarugir（Kegworks 改名，Wineskin 后继）仍活跃**：3,708 star，最后推送 2026-09-25，要求 macOS 14.6+ 和 Rosetta [25]。许可分两部分：Configure.app 是 LGPL-2.1，Creator/Launcher 不是 [25][30]。它自带的 CX 引擎停在 CX 24.0.7，用户请求 CX 26 引擎的 Issue #238 被以 “not planned” 关闭 [27][28]。[高]
- **Heroic/Mythic/Porting Kit 自己都不造 Wine**：
  - Heroic（GPL-3.0，v2.22.3，2026-09-16）的 Mac 端直接下载 Gcenx 的构建，或调用 CrossOver [38][39][40]。
  - Mythic（GPL-3.0）最后一个 release 是 v0.6.0 预发布版（2025-12-27），引擎改用 `MythicApp/wine` 的 `mythic-crossover-24.0.7-stable` 分支 [35][36][37]。
  - Porting Kit 未找到公开源码 [32][34]。
  - [高/中]
- **Linux 侧值得借鉴的是数据格式，代码语言/平台不同，几乎无法直接复用**：
  - umu 的做法：umu-database 用 CSV 把各商店 ID 映射到统一的 umu-ID（GPL-3.0）；umu-protonfixes 按商店和游戏 ID 组织 Python 修复模块（**BSD-2-Clause**）[71][72][78][79]。umu-ID 是不透明字符串，**不都是** Steam AppID（核查更正，见 §13）。
  - Bottles 的 YAML 依赖清单，以及由 CI 自动拉取的组件索引 [64][65]。
  - Lutris 的 YAML 安装脚本 [68]。
  - [高]
- **对 Cider 的核心建议（详见后文）**：
  1. 以 GPL-3.0 许可 fork `frankea/Whisky` 的 WhiskyKit 层作为 App 起点，或至少逐模块借用。
  2. 按 `winecx-gptk` 的思路自建引擎 CI，产物签名、公证后放在组织名下的 GitHub Releases。
  3. D3DMetal 默认采用“用户导入”流程。这是保守的选择，许可证文本并不强制这样做；是否随 App 打包需要单独决策（核查更正，见 P0-3）。
  4. 游戏配置数据库以 umu-ID 字符串为键，采用纯数据格式。
  5. 从第一天起就把 bus factor 做到 ≥2。

---

## 详细调研

### 1. Whisky（原版，Whisky-App/Whisky）

**1.1 架构** [1][10][11]

| 模块 | 作用 | Cider 可复用性 |
|---|---|---|
| `Whisky/`（SwiftUI App） | 主界面、bottle 列表、程序列表、设置页；Sparkle 自动更新；Crowdin 本地化 | 可 fork（GPL-3.0）|
| `WhiskyKit/`（Swift 框架） | 包含子目录 `Whisky/`（Bottle、BottleSettings、Program、ProgramSettings）、`Wine/`（进程启动、注册表）、`WhiskyWine/`（引擎安装/更新）、`PE/`（`PortableExecutable`、`COFFFileHeader`、`OptionalHeader`、`Section`、`RSRC`，用于提取图标）；另有 `ShellLink.swift`（.lnk 解析）和 `Tar.swift` | 可直接复用，价值最高 |
| `WhiskyCmd/` | CLI 子命令：`list`、`create`、`add`、`delete`、`remove`、`run`、`shellenv`（输出 `export` 语句，供 `eval` 使用）| 可复用 |
| `WhiskyThumbnail/` | QuickLook 缩略图扩展，在 Finder 中显示 .exe 图标 | 可复用 |

**1.2 Bottle / Program 数据模型与环境变量映射** [7][8][9][10]

- 每个 bottle 就是一个 `WINEPREFIX` 目录，配置写在 `<bottle>/Metadata.plist`（XML plist，`PropertyListEncoder`），带文件版本号，解码时会检查 Wine 版本兼容性。
- `BottleSettings` 分为四组：`BottleInfo`（名称、pins、blocklist）、`BottleWineConfig`（`wineVersion` 默认 7.7.0、`windowsVersion`、`enhancedSync`（none/esync/msync）、`avxEnabled`）、`BottleMetalConfig`（`metalHud`、`metalTrace`、`dxrEnabled`）、`BottleDXVKConfig`（`dxvk`、`dxvkAsync`、`dxvkHud`）。
- `ProgramSettings` 为每个 exe 单独保存一份 plist，字段有 `locale`（13 种）、`environment`（字典）和 `arguments`。**按程序设置**正是 01 号报告中 CrossOver 用户抱怨缺少的能力。
- 启动时 `constructWineEnvironment` 固定设置 `WINEPREFIX`、`WINEDEBUG=fixme-all`、`GST_DEBUG=1`，再合并 bottle 和程序的变量：

| 设置 | 环境变量 |
|---|---|
| msync / esync | `WINEMSYNC=1` / `WINEESYNC=1` |
| Metal HUD / Trace | `MTL_HUD_ENABLED=1` / `METAL_CAPTURE_ENABLED=1` |
| AVX | `ROSETTA_ADVERTISE_AVX=1` |
| DXR | `D3DM_SUPPORT_DXR=1` |
| DXVK | 把 DLL 复制进 `system32`/`syswow64`，并设 `WINEDLLOVERRIDES="dxgi,d3d9,d3d10core,d3d11=n,b"`；另有 `DXVK_ASYNC=1`、`DXVK_HUD` |

- Windows 版本通过 `winecfg -v` 设置；Retina 模式写在 `HKCU\Software\Wine\Mac Driver` 的 `RetinaMode`；DPI 写 `LogPixels`；终止 bottle 用 `wineserver -k`；日志按 ISO8601 命名，存放在 `~/Library/Logs/<bundle-id>/`。[高]

**1.3 WhiskyWine 的内容与构建管线** [7][12][13]

- 演进：先是 `WhiskyBuilder`（GPTK builder，2024-04-06 归档），之后由 `Whisky-App/wine` 仓库的 `7.7` 分支接替。
- CI 文件 `.github/workflows/build.yml` 的主要做法：
  - 运行在 `macos-13` + Xcode 15.2，`CC=clang`，用 Homebrew 安装依赖，其中编译器是 `gcenx/wine/cx-llvm`。
  - 先构建 `wine64`（`--enable-win64`），再构建 `wine32on64`（`--enable-win32on64 --disable-loader --with-wine64=...`）。
  - 用 `.github/dylib_packer.zsh` 收拢外部 dylib；把 DXVK 和 GPTK `redist` 拷进 `Libraries/Wine/lib/`；附带 winetricks 和 Wine Mono 7.4.1。
  - 生成 `WhiskyWineVersion.plist` 和 shasum，最后用 rclone 上传到 R2 桶 `whisky-bucket/Wine`。
- App 端的 `WhiskyWineInstaller` 读取 `https://data.getwhisky.app/Wine/WhiskyWineVersion.plist`，与本地的 `SemanticVersion` 比较，下载后解压到 `~/Library/Application Support/com.isaacmarovitz.Whisky/Libraries/`（Wine 位于 `Libraries/Wine/bin/wine64`）。
- **由此可见**：D3DMetal 是随 WhiskyWine 一起再分发的；整套引擎只有一个版本，没有并存或回滚机制。[高]

**1.4 停更时间线** [2][3][4][5][6][14][15]

| 时间 | 事件 |
|---|---|
| 2025-04-09 | 维护停止公告上线（docs 最后修改 2025-04-09），同时写明“WhiskyWine 不再更新”；Homebrew cask `whisky` 2.3.5 标记 `deprecate! date: "2025-04-09", because: :unmaintained` |
| 2025-04-16 / 04-23 | AppleInsider、MacRumors 报道，作者推荐改用 CrossOver |
| 2025-04-18 | CodeWeavers 发博文《Whisky's Legacy…》，态度是理解和致谢 [6][中] |
| 2025-05-11 | 仓库最后一次推送，随后归档 |
| 2026-09-26 | 实测 `data.getwhisky.app` 的引擎清单和 tarball 均返回 404 |

**教训** [低/推断]：
- 引擎托管在个人 CDN 上，项目一死，存量 App 就无法初始化。
- 单一引擎、单一维护者。
- 没有向上游回馈，这被作者本人列为停更的**道德理由**之一。

### 2. Whisky 在 2025–2026 年的分支

**2.1 `frankea/Whisky`（最活跃）** [16]–[21]
- 规模：独立仓库（不是 GitHub fork 关系），2026-01-05 创建，1,000+ commits，最后推送 2026-09-16 [75]。要求 macOS 15+ 和 Apple Silicon，DMG 已签名并公证，通过 Sparkle 更新，另有自己的 Homebrew tap。
- App 版本：
  - 3.5.0（2026-06-14）
  - 3.5.1（07-24）：内置 winetricks
  - 3.6.0（08-04）：22 种语言、Steam 游戏库、GPTK 导入，新增 CLI 命令 `whisky games` 和 `whisky launch <appid>`（支持 JSON 输出）；快捷方式改为运行时走实时管线，不再把环境变量固化进去
  - 3.7.0（08-30）：游戏库首页；D3DMetal 路径支持 Metal 4 command encoding 和 MetalFX
- WhiskyKit 新增的子目录：`Audio`、`Diagnostics`、`Discord`、`GameDatabase`、`Library`、`Steam`、`Troubleshooting`，以及 `ProcessRegistry.swift`、`ClipboardManager.swift`。README 称有 “80+ curated per-game configs” 和各启动器的 profile（Steam/Epic/EA/Rockstar/Battle.net）。
- 运行时（DEPENDENCIES.md，写于 v3.1.1 时期）：
  - Wine 11.0 用 Gcenx 打包的版本（pin 在 11.0_1）。
  - DXVK-macOS 1.10.3，文档说明这是“by design”冻结。
  - DXMT 0.80，即最后一个 MIT 许可版本：v0.80 tag 下的 LICENSE 是 MIT，main 分支已改为 LGPL-2.1+。0.80（2026-04-23）同时也是截至 2026-09-26 最新的 DXMT release，所以 pin 在 0.80 并不算落后 [59][83][84]。
  - winetricks 20260125，外加 arm64 版 cabextract。
  - `Libraries.tar.gz` 首次启动时下载，做 SHA-256 校验，不匹配即安全失败。
  - 治理文档明确写着：它“does not build Wine itself”，并且 release 的 bus factor 为 1。
- Beta 引擎（“Wine Libraries”）：
  - v4.0.0-beta.1（2026-08-04）：第一个能跑 D3DMetal 的引擎，Wine 10，来自 CX 25 源码。
  - v4.5.105-beta.1（08-13）：Wine 11.15。
  - v4.6.4-beta.1（08-29）：Wine 11.16，DXVK 1.10.3（去掉 d3d9）、DXMT 0.80、MoltenVK 1.4.2、GStreamer 1.26.3、gecko 2.47.4；构建环境是 GitHub 托管的 `macos-15` runner；**默认不自动安装**；需要用户自备 GPTK 并在 Settings 中导入。[高]

**2.2 `dappermint/winecx-gptk`，以及 frankea 的 fork** [22][23][24]
- 基本信息：2026-07-31 创建，2026-09-23 仍在推送，**仓库未声明许可证**。
- 当前主线：CX 26.3 的 Wine 改动前移到上游 Wine 11.17；历史 tag 有 `lane/wine10-cx25` 和 `lane/wine11.0-cx26`。
  - 具体做法比简单 rebase 更讲究（据 README）[73]：先取 CX 26.3 相对其 Wine 11.0 基线的差异（221 个文件），用合成的三方合并并到 Wine 11.15 上，再逐版前移到 11.16 和 11.17。
  - 源码在 `dappermint/winecx` 的 `wine1117` 分支，本仓库的 `patches/` 目录故意留空。
- 管线：
  1. 从 `dappermint/winecx` 按 pin 的 commit 克隆源码，在 Rosetta 下构建 x86_64 的 Unix 部分。
  2. PE 部分用 **mingw-w64 GCC**（不是 llvm-mingw），`--enable-archs=i386,x86_64`。
  3. FreeType、GnuTLS、GStreamer 从 **pin 了 `NIXPKGS_REV` 的 nixpkgs（x86_64-darwin）** 获取，并改写成 `@loader_path`；MoltenVK、DXVK、DXMT 的版本和 SHA256 都是 pin 住的。
  4. 输出 Whisky 格式的 `Libraries.tar.gz`，在 plist 里加上 `gptkCapable` 标记。
- 质量门：
  - 扫描所有 Mach-O 文件，查找非系统的绝对路径引用（可重定位性）。
  - 屏蔽 store 路径后逐个 dlopen。
  - 窗口创建与 Media Foundation 解码器冒烟测试。
  - 检查 i386 文件是否齐全，防止 `c0000135`。
  - 剥离 PE 的 DWARF 调试信息。
- 随带的 CX 补丁包括：ntdll 对 Mach-O 代码异常的处理修复、基于 CAContext 的跨进程 Metal layer（让 Chromium/Steam 能用）、根据 Vulkan 如实回答 d3dkmt 适配器查询。
- 最低部署目标为 macOS 26.0。[高]
- **这正是 02 号报告推荐的“CX 差异 → 上游前移”混合路线，而且有人每周都在跑**，对 Cider 的参考价值最高。但它没有许可证：main 分支下 `LICENSE`、`LICENSE.md`、`LICENSE.txt`、`COPYING` 都返回 404，README 里也没有许可声明 [73]。因此 CI 脚本不能直接复制（能否复用属于法律问题，本文不评估）；Wine 源码本身仍然是 LGPL。

**2.3 其他分支** [中/低]：`yermakoffivan/Whisky` 是 frankea 的下游，没有独立活跃度 [16]；“Brandy/Brandywine” 详情不明（见 02 号报告）。

### 3. Kegworks → Sikarugir（Wineskin 谱系）[25]–[31]

- **谱系**：Wineskin（doh123）→ Wineskin Winery → 社区版 Kegworks（加入 Homebrew 分发和 Apple Silicon 支持）→ **Sikarugir**。第三方站点称改名发生在 2025-10-11 [29][中]。
  - 原 `The-Wineskin-Project/WineskinServer`（LGPL-2.1）已归档，最后推送 2025-08-14 [31]。
- **模型**：每个 Windows 程序被封装成独立的 `.app` wrapper，内含 prefix、引擎和 Launcher，由 Creator.app 生成，Configure.app 负责配置。
  - 这是“每个 App 一份引擎和 prefix”的可移植模型，和 CrossOver/Whisky 的 bottle 模型不同。
  - Porting Kit 就建立在这套 wrapper 之上。
- **2026 年状态**：
  - README 要求 macOS 14.6+；Apple Silicon 必须安装 Rosetta（`softwareupdate --install-rosetta --agree-to-license`）。
  - 安装方式：`brew trust Sikarugir-App/sikarugir` 后执行 `brew install --cask Sikarugir-App/sikarugir/sikarugir`。
  - 后端：D3DMetal、DXMT（默认）、DXVK、D9VK（仅 Apple Silicon 且 Tahoe）、WineD3D、CNC-DDRAW。
  - 组织内还 fork 了 dxvk、dxmt、MoltenVK、winetricks 和 wine（wine 的 master 就是上游 11.17，没有额外补丁）[26]。
- **引擎清单**（`EngineList.txt`）[27]：`WS12WineSikarugir11.0`、`WS12WineSikarugir10.0_6`、`WS12WineCX24.0.7_7`、`WS12WineCX23.7.1_4`、`WS12WhiskyWine2.5.0_3`、`WS12WineGPTK1.1_3`，以及 WS11 系列 CX 19–21（含 32Bit 变体）。
  - 可见它把“多引擎并存”做成了一等功能，还把 WhiskyWine 和 GPTK 1.1 收编成了引擎。
  - 用户请求 CX 26 引擎的 Issue #238（2026-06-22）被以 “not planned” 关闭，页面上没有看到维护者的解释 [28]。
- **许可**：Configure.app 是修改过的 Wineskin，LGPL-2.1，源码在 `Sikarugir-App/Sikarugir-foss-sources`；**Sikarugir Launcher 和 Creator.app（v1.0.1+）不是 LGPL** [25][30]。
  - README 引用 D3DMetal 3.0 的 `License.pdf`，称其“限制严格，不能用于商业移植”。
  - **（核查更正）** `License.pdf` 原文的相关条款如下 [80]，仅作为工程约束列出，不做法律解读：
    - §2A(i)：使用授权限定为开发、测试或评估面向 Apple 产品的游戏（“for the sole purpose of developing, testing, or evaluating video games”）。
    - §2A(iii)：允许“solely for non-commercial purposes”地分发 Apple Software。
    - §2C：Framework 整体或任何 Redistributables 可以单独分发，同样受非商业限制。
  - 这也解释了为什么 Whisky、Gcenx、Sikarugir 都在再分发 D3DMetal，而另一些项目不打包。GPTK 4 的许可文本本次没有找到 [中]。
  - 另有 `sikarugir.com` 声称项目是 MIT、支持 10.15.4，这与官方 README 矛盾，应视为非官方站点 [中]。

### 4. Porting Kit（Paul the Tall）[32][33][34]

- 形态：免费软件，本质是“安装脚本库 + Wineskin wrapper 生成器”。
- 版本：Porting Kit 6（2023-11-09）加入 D3DMetal/GPTK 和 GOG 账户集成，Wineskin 升到 2.9.2.0，支持 CX23.0.1+ 引擎 [32]。第三方下载站显示 2026-06 为 7.0.2 [34][中]。
- **未找到公开源码仓库**，按闭源处理 [中]。
- 2026-09-20 官方博客报告：**升级到 macOS 27 “Golden Gate” 后 Rosetta 2 似乎被卸载了**，导致 Porting Kit 的新安装失败；团队正在做自动安装 Rosetta 的修复 [33][中]。
  - **（核查更正）** 原文措辞带保留（“It seems that the Mac OS update de-installs Rosetta 2”）。这只是单一厂商的观察，不是 Apple 的说法，本次也没有找到 Apple 的一手来源。
  - Homebrew 6.0.0 的公告独立印证了 “macOS 27 (Golden Gate)” 这个名称，以及 macOS 27 不再支持 Intel [50]。
- **可借鉴**：“一个游戏一份安装配方”，配方持续维护（2026-08 仍在上新移植）。**不可复用**：代码。

### 5. Mythic（MythicApp/Mythic）[35][36][37]

- 基本信息：SwiftUI 游戏启动器，GPL-3.0，1,414 star，要求 macOS 14+。通过 Legendary 接入 Epic，支持手动导入游戏和 Steam（Windows 版需手动下载）。
- 版本：v0.4.3/0.4.4（2025-01）加入 Intel 支持；v0.5.0（2025-10-26）迁移到 Swift 6；v0.6.0（2025-12-27，预发布）重写了游戏管理。
- 活跃度：仓库 2026-09-04 仍在推送，但 2026 年没有发布新版本。作者在 release notes 里写过开发受“学业”限制。
- 引擎：`MythicApp/Engine` 自称是 “a derivative of WhiskyWine”，2025-12-25 归档，改在 `MythicApp/wine` 的 `mythic-crossover-24.0.7-stable` 分支上开发（基于 CX 24，LGPL）。
- **教训**：又一个学生单人项目；引擎基线停在 CX 24。

### 6. Heroic Games Launcher（macOS 部分）[38][39][40]

- 基本信息：Electron/TypeScript，GPL-3.0，12,283 star，v2.22.3（2026-09-16）。2.22.2 为 macOS 上的 CrossOver 增加了默认路径。
- **Mac 端的 Wine 来源**（wiki）：
  - Wine-Staging/Devel：来自 `Gcenx/macOS_Wine_builds`，支持 wined3d、DXVK、DXMT。
  - GPTK：来自 `Gcenx/game-porting-toolkit`，仅 Apple Silicon，支持 DX11/12。
  - Wine-Crossover：Gcenx 的 CX 23 构建，**原仓库已删除**，Heroic 自己留了镜像；标为过时。
  - 商业版 CrossOver。
  - Whisky：已标注不推荐。
  - 必须装 Rosetta，否则报 `spawn Unknown system error -86`。
- **探测逻辑**（`compatibility_layers.ts`）：
  - CrossOver：查 `/Applications/CrossOver.app`、`CrossOver Preview.app`，以及 `mdfind` bundle id `com.codeweavers.CrossOver`；wine 路径为 `Contents/SharedSupport/CrossOver/bin/wine`。
  - Whisky：读 `Libraries/WhiskyWineVersion.plist`。
  - GPTK：`Contents/Resources/wine/bin/wine64`。
  - 其他 Wine：`mdfind kMDItemCFBundleIdentifier = "*.wine"`。
- **可复用**：
  - 它通过 CLI 子进程接入商店：Epic 用 legendary（GPL-3.0）；GOG/Amazon 使用的 gogdl 和 nile，其许可证本次未核实。
  - Cider 如果要做商店集成，可以同样以子进程方式调用 legendary，但 Heroic 自身的 TS 代码不能移植到 Swift。

### 7. CXPatcher → Procyon（italomandara）[41][42]

- CXPatcher：GPL-3.0，1,637 star。v0.7（2026-02-13）面向 CX 26.x，加入 GStreamer 补丁（来自 Gcenx）；v0.7.1（2026-03-21）恢复 MoltenVK 补丁选择器，更新 MVK main 分支和 UE4 hack，并声明后续开发转到 Procyon。
  - 作用：替换已付费 CrossOver 中的 DXVK/MoltenVK/D3DMetal 组件，外加 UE4 相关 hack。
- Procyon：Swift，GPL-3.0，2026-02-06 创建，最后推送 2026-09-04。它是 Steam 启动器，**必须有 CrossOver 许可证**，支持按游戏选择图形后端，并用 rosettaX87 加速 32 位 x87 代码。
- **可借鉴**：MoltenVK/UE4 相关补丁，以及“按游戏选后端”的 UI。补丁来源和许可需要逐项核对。

### 8. Gcenx 生态（macOS Wine 打包的事实标准）[43]–[48][58]

| 资产 | 状态（2026-09） | 要点 |
|---|---|---|
| `Gcenx/macOS_Wine_builds` | 11.18（2026-09-25）；从 11.14 到 11.18 基本每两周一版（11.15 为 08-08，11.16 为 08-24，11.17 为 09-11）[82] | 描述为 “Official Winehq macOS Packages”；只提供 `wine-{devel,staging}-<ver>-osx64.tar.xz`；内置 gecko 2.47.4、mono 11.3.0；要求系统级 GStreamer.framework；**构建脚本不在本仓库**，依赖 MacPorts + `macports-wine` overlay；**仓库没有许可证** |
| `Gcenx/macports-wine` | 2026-09 活跃 | `emulators/wine-devel/Portfile`：只支持 x86_64，`--enable-archs=i386,x86_64 --enable-win64`，用 llvm-mingw，`--with-coreaudio --with-cups --with-freetype --with-gnutls`，禁用 x11/wayland/pulse/alsa/dbus 等；补丁 `0001-win32u-Enable-host-Vulkan-portability-enumeration.diff`；另有 `crossover` 26.3.0、`game-porting-toolkit` 1.1、`sikarugir` 1.0.1、`winetricks` 20260518、`gstreamer.framework` |
| `Gcenx/homebrew-wine` tap | 最后推送 2026-09-24 | 只剩 `game-porting-toolkit` cask 3.0-3（需要 Rosetta，安装时去除 quarantine 并做 ad-hoc 签名）|
| `Gcenx/game-porting-toolkit` | 3.0-3（2026-03-03）、3.0（2025-12-05）、2.1（2025-03-12）| Wine 7.7；在 Apple 许可允许的范围内**连同 D3DMetal 一起再分发**；Heroic 从这里下载 |
| `Gcenx/DXVK-macOS` | v1.10.3 以后停止更新 | 见 04 号报告 |
| `Gcenx/NotProton` | 2026-09 活跃 | 在 macOS 原生 Steam 客户端里启用 Steam Play：lsteamclient + steam-shim（移植自 Proton 9）+ ntdll 补丁；目标环境是 Steam 客户端 1788652215 或 1790121765，加上 CrossOver Preview 2026082。**（核查更正）** README 写明该 Preview 有 **FEX 构建和 Rosetta 构建**两种，两者都支持；推荐 Rosetta 构建，因为 FEX 构建还处于早期状态 [81]。也就是说，CodeWeavers 已经发布了不依赖 Rosetta 的 FEX 版 Preview（参见 06 号报告） |
| Homebrew 官方 cask | **2026-09-01 被禁用** | `wine-stable.rb`（11.0_1）和 `wine@devel.rb`（11.16）都写有 `disable! date: "2026-09-01", because: :fails_gatekeeper_check` [47][74]；Gcenx 仓库的 Issue #168（2026-08-26）仍未解决 |

**结论** [低/推断]：macOS 上“拿来即用”的上游 Wine 实际上只依赖一个人和一条 MacPorts 管线；Homebrew 官方渠道已经关闭。Cider **必须自己构建引擎并签名**，不能指望用户用 brew 安装 Wine。

### 9. Apple GPTK 的 Homebrew 工作流与 GPTK 4 [45][51]–[57]

- **GPTK 1.x（2023）**：
  1. `brew tap apple/apple`，然后 `brew install apple/apple/game-porting-toolkit`，在 x86_64 Homebrew 下从 `crossover-sources-22.1.1` 编译，依赖 `game-porting-toolkit-compiler`，同时构建 wine64 和 wine32on64，M1 上约 75 分钟。
  2. 把 DMG 里 `redist/lib/external/` 下的 `D3DMetal.framework` 和 `libd3dshared.dylib` 复制到 Wine 的 lib 目录。
  3. 用 `gameportingtoolkit*` 脚本启动。
- **2.x/3.x**：社区普遍改用 Gcenx 的预编译包。Wine 仍然是 7.7，只有 D3DMetal 在更新 [76][77]。GPTK 4 评估环境随附的 Wine（如果有的话）是哪个版本，没有找到一手来源，见“未解问题”第 2 条。
- **GPTK 4（WWDC26）**：Apple 页面列出评估环境支持 Metal 4，并新增 `apple/game-porting-toolkit` 仓库（Apache-2.0，内容是 agent skills、metal-cpp 和示例），**里面不含 D3DMetal 或 Wine** [52][54]。
  - AppleInsider（2026-06-17）称评估环境 4.0 beta 1 只支持 Apple Silicon，演示流程基于 macOS 27 和 CrossOver [53][中]。
  - 第三方仓库在 2026-09-23 再分发了 “D3DMetal.framework 4.0 beta 2”，声明“仅限非商业再分发” [56][中]。
- **`utmapp/d3dmetal-native`**（MIT，2026-07-04 创建）[55]：在原生 macOS 进程中实现 D3DMetal 所需的 **`GFXT` host interface**（窗口、事件、注册表、内存和跨进程资源共享），从而不经过 Wine 直接导出 `D3D11CreateDevice`/`D3D12CreateDevice`。
  - 限制：只有 x86_64（因为 `D3DMetal.framework` 只有 x86_64），在 Apple Silicon 上整个进程要跑在 Rosetta 下。
  - 对 Cider 的意义：它是第三方对“D3DMetal 期望宿主提供什么”的公开实现，可以和 02 号报告里的 `macdrv_functions` 分析互相印证。

### 10. PlayOnMac / Phoenicis [60][61][62]

- PlayOnMac 最后一条新闻是 2022-03-14；4.4 版（2020-07-04）开始支持 Catalina。POL-POM-4（GPL-3.0）在 2026-02 还有推送。
- 继任者 Phoenicis（LGPL-3.0）最后推送在 2025-04-29，5.0 一直停在 alpha。
- **教训**：安装脚本库会随下载链接失效而腐烂；大规模重写（第二系统效应）没能交付。

### 11. Bottles（Linux）[63]–[66]

- 基本信息：GPL-3.0，8,888 star。2026-09 发布到 67.4，包含 Soda 11.0-10、UMU runtime 集成、Cpak 分发和 arm64 Cpak；只支持在 Flatpak 环境中构建。
- **值得借鉴的设计**：
  1. **Environment**（Gaming / Application / Custom）作为预设。
  2. **Runner 与 Component 分离**：DXVK、VKD3D、NVAPI、LatencyFleX 各自独立版本化，索引放在 `components/index.yml`，由 `pull-components.yml` CI 自动从上游拉取，带校验和。
  3. **依赖清单用 YAML 代替 winetricks 脚本**：例如 `vcredist2019.yml` 写明版本、Provider、License、安装步骤（exe 的大小和 MD5、静默参数）以及 17 个 DLL override。
  4. 安装器分 Bronze/Silver/Gold/Platinum 四级，存放在 `bottlesdevs/programs`，并附带维护者评审。
  5. 版本快照（Versioning）。
- 许可：`dependencies` 和 `programs` 两个仓库的许可证本次未能确认。

### 12. Lutris（Linux）[67][68][69]

- 基本信息：GPL-3.0。v0.5.20（2026-02-16）起默认通过 umu 运行 Proton-GE；最新版本 v0.5.22（2026-02-25）。
- **安装脚本格式**（`docs/installers.rst`）：
  - 顶层段：`game`、`files`、`installer`、`wine`、`system`。
  - 指令：`move`、`merge`、`extract`、`execute`、`write_config`、`write_json`、`chmodx`，以及 `task`（`wineexec`、`winetricks`、`create_prefix`、`set_regedit`）。
  - 变量：`$GAMEDIR`、`$CACHE`。
- 2020 年约有 3,300 个游戏带安装脚本 [69]。第三方询问能否复用这些脚本时，页面上没有看到维护者的授权答复，所以**脚本数据的授权状态不明**。

### 13. umu-launcher / umu-protonfixes / umu-database [70][71][72]

- **umu-launcher**：GPL-3.0，1.4.4（2026-07-25），**仅支持 Linux**。它在 pressure-vessel 和 Steam Linux Runtime 3.0（sniper）里运行 Proton，接口是 `GAMEID`、`STORE`、`PROTONPATH`、`WINEPREFIX` 几个环境变量。
- **umu-database**：GPL-3.0 [79]。CSV 表头原文是 `TITLE,STORE,CODENAME,UMU_ID,COMMON ACRONYM (Optional),NOTE (Optional),EXE_STRINGS (Optional)` [72]。
  - 例如 GTA V 在 egs 上的 codename 映射到 `umu-271590`，也就是 `umu-<Steam AppID>` 的形式。
  - **（核查更正）** 原稿写“约 2,000 行，以 Steam AppID 作为统一键”，这不准确。2026-09-26 统计：约 1,202 行数据，约 1,101 个不同的 `UMU_ID` [72]。
    - 多数 ID 是 `umu-<SteamAppID>`，但也有不少不是，例如 `umu-genshin`、`umu-endfield`、`umu-identityv`、`umu-cxbxreloaded`，以及 UUID 形式的 `umu-4bff76f4-…`；还有少数行的 ID 格式有误。
    - 按商店分，行数较多的是 amazon 439、zoomplatform 383、gog 204、egs 107；none 38，ubisoft 10，humble 9，ea 5。
    - **结论**：umu-ID 是不透明字符串，不能当作整数 Steam AppID 处理。
- **umu-protonfixes**：**BSD-2-Clause** [78]。
  - 目录结构：`gamefixes-{steam,egs,gog,ubisoft,battlenet,humble,amazon,itchio,zoomplatform,umu}/<id>.py`，另有 `default.py`。
  - `get_game_id()` 依次读取 `UMU_ID`、`SteamAppId`、`SteamGameId`、`STEAM_COMPAT_DATA_PATH`。
  - 工具函数：`protontricks()`、`append_argument()`、`set_environment()`、`winedll_override()`、`disable_esync()`。
- **意义**：Heroic、Lutris、Bottles 在 2025–2026 年都接入了 umu，Linux 生态已经收敛到一个共享的“游戏 ID → 修复”基础设施上。

### 14. 横向对比

| 项目 | 2026 活跃度 | 引擎来源 | 许可 | 最可复用的部分 |
|---|---|---|---|---|
| Whisky（原版）| 已死（2025-05 归档，CDN 404）| CX 22.1.1/Wine 7.7 + GPTK | GPL-3.0 | WhiskyKit、CLI、QuickLook |
| frankea/Whisky | 很活跃（单人）| Gcenx Wine 11.0；beta 为 CX→11.16 | GPL-3.0 | 整个 App 层、GameDB、Steam 库、GPTK 导入 |
| dappermint/winecx(-gptk) | 很活跃 | CX 26.3 → 11.17 | Wine 部分 LGPL；CI 未声明 | 构建管线设计、质量门 |
| Sikarugir | 活跃 | Gcenx 引擎（多版本）| Configure LGPL；Creator/Launcher 非 LGPL | wrapper 导出思路、多引擎清单 |
| Porting Kit | 活跃（内容）| Wineskin wrapper | 闭源 [中] | 配方运营模式 |
| Mythic | 放缓 | CX 24.0.7 派生 | GPL-3.0 | Epic 集成的 Swift 参考 |
| Heroic | 很活跃 | Gcenx/CrossOver | GPL-3.0 | 商店 CLI、探测逻辑 |
| CXPatcher/Procyon | 活跃 | 依附 CrossOver | GPL-3.0 | MVK/UE4 hack |
| Gcenx 系列 | 很活跃（单人）| 上游 Wine/CX 重打包 | 多数仓库未声明 | Portfile 参数、补丁 |
| Bottles/Lutris/umu | 很活跃 | Linux 专用 | GPL-3.0；protonfixes 为 BSD-2 | 数据格式、ID 体系 |

### 15. 复用决策：直接 fork 还是只借鉴，以及能省多少工作量 [低/估算]

| 资产 | 决策 | 节省估算 | 注意 |
|---|---|---|---|
| frankea/Whisky 整体（或只取 WhiskyKit）| **fork**，前提是 Cider 的 App 层采用 GPL-3.0 | 6–9 人月（bottle 管理、设置 UI、Sparkle、本地化、CLI、Steam 库、诊断）| 继承单人项目的技术债；`.xcodeproj` 需要完整 Xcode（开发机目前只有 CLT）|
| WhiskyKit 的 `PE/`、`ShellLink`、`Tar`、注册表 helper | 直接复用 | 2–4 周 | 同样是 GPL-3.0 |
| winecx-gptk 的 CI 设计（nix pin、`@loader_path`、质量门）| 借鉴后自己重写（或请作者加上许可证）| 1–2 人月的试错 | 仓库无许可证 |
| Gcenx Portfile 的 configure 参数和补丁 | 直接参考 | 2–4 周 | 仅 x86_64 |
| Whisky-App/wine 的 `build.yml` 和 `dylib_packer.zsh` | 借鉴 | 1 周 | wine32on64 已过时，Cider 应走新 WoW64 |
| winetricks | 直接内置（LGPL-2.1）| 数月的依赖配方工作 | frankea 和 Sikarugir 都这么做 |
| umu-protonfixes | 移植其语义（BSD-2）| 规则库冷启动 | 依赖 Proton 专有变量的部分需要转换 |
| umu-database | 直接采用其 ID 体系，数据按 GPL 使用 | 跨商店 ID 映射（约 1,200 行、约 1,100 个 ID，2026-09-26）| umu-ID 是不透明字符串，不都是 Steam AppID，键类型必须用字符串 |
| Bottles 依赖和组件清单格式 | 借鉴 | 2–3 周的设计工作 | 数据许可未确认 |
| Lutris 安装脚本 | 只借鉴 schema | — | 数据许可不明 |
| legendary（gogdl/nile 的许可待核实）| 以子进程方式调用 | Epic 集成少做 2–3 人月 | — |
| utmapp/d3dmetal-native | 作为参考（MIT）| D3DMetal 接口逆向 | 只有 x86_64 |
| NotProton | 评估合作 | Steam Play 集成 | 目前绑定 CrossOver Preview 2026082，该 Preview 已有 FEX 构建（早期状态）和 Rosetta 构建 [81] |

---

## 对 Cider 的启示与建议（按优先级）

**P0：先定下来的架构决策**
1. **App 层采用 GPL-3.0，并以 frankea/Whisky 的 WhiskyKit 作为起点**：可以按模块吸收，不必整体 fork。沿用 bottle 模型（`Metadata.plist` + 每个程序单独的 plist），但要一开始就加上 schema 版本和迁移机制。按程序设置（环境变量、参数、locale）是 CrossOver 缺少的功能，必须保留。先装完整 Xcode，或者把 App 迁到 SwiftPM + `xcodebuild` 在 CI 上构建。
2. **引擎要自己构建，并支持多版本并存**：参照 winecx-gptk 的质量门和 frankea 的 stable/beta 双轨，同时保留 Sikarugir 那样的多引擎清单。
   - stable 用上游 Wine（与 Gcenx 相同的 configure 参数，加 Vulkan portability 补丁）。
   - gptk 线用“CX 差异 → 上游”的 rebase 结果（见 02 号报告）。
   - 产物用 Developer ID 签名并公证，放在**组织名下**的 GitHub Releases，附带 plist/JSON 清单和 SHA-256，并支持回滚。**不要把引擎放在个人 CDN 上**（Whisky 的 404 就是前车之鉴）。
   - **（核查后新增）引擎清单从一开始就标注架构**（x86_64 + Rosetta，或 arm64 + FEX），为不依赖 Rosetta 的路线留出位置。CodeWeavers 已经在 CrossOver Preview 2026082 中发布了 FEX 构建，但仍处于早期状态 [81]。arm64/FEX 路线的细节见 06 号报告。
3. **D3DMetal 默认走“用户导入”流程**：参考 frankea 3.6.0。用户指向自己下载的 GPTK DMG，App 校验后单独存放，跨引擎升级保留，只部署到 `gptkCapable` 的引擎。
   - **（核查更正）** 原稿写“App 本身不打包 D3DMetal”，隐含的前提是 §3 转述的“许可证限制严格”。更准确的说法是：“用户导入”是保守的默认选择，但许可证文本并不强制这样做。
   - D3DMetal 3.0 的 `License.pdf` 允许非商业分发（§2A(iii)、§2C），而使用授权限定为开发、测试或评估游戏（§2A(i)）[80]。Whisky（归档前）、Gcenx、Sikarugir 都曾经或正在再分发 D3DMetal [12][45][25]。
   - 免费的 Cider 能否直接打包，以及 GPTK 4 的许可有没有变化，都需要项目方明确决策（本文不做法律分析）。在决策之前，先按“用户导入”实现，并把打包做成可选的构建开关。
4. **构建全部放到 CI 上（GitHub 托管的 `macos-15`/`macos-26` runner）**：M3/8 GB 开发机只用于调试；GPTK 官方也建议 16 GB 内存。

**P1：数据层与生态**
5. **游戏配置数据库放在独立仓库，只存数据**（YAML/JSON，经过 CI 校验）。以 umu-ID 作为主键，同时保存各商店的 codename。**（核查更正）** umu-ID 是不透明字符串，不一定是 Steam AppID：多数形如 `umu-<SteamAppID>`，但也有 `umu-genshin` 这类名字和 UUID 形式的 ID [72]。所以 schema 里主键必须是字符串，不能用整数 AppID；Steam AppID 另设一个可选字段。规则类型参照 protonfixes 的原语：DLL override、环境变量、参数、winetricks verb、注册表。不执行任意 Python，避免供应链风险。
6. **依赖安装**：短期内置 winetricks；中期用 Bottles 式的 YAML 清单（URL + 校验和 + 步骤）覆盖常用运行库（VC++、.NET、DirectX），解决 CrossOver 配方老化的问题。
7. **商店和启动器优先**：先做 Steam 库（参照 frankea 的 `Steam/`），通过子进程接入 legendary（Epic）。评估与 NotProton 合作，在原生 Steam 客户端里提供 Steam Play。
8. **Onboarding 自检 Rosetta**：缺失时提示运行 `softwareupdate --install-rosetta`。Porting Kit 博客（2026-09-20）观察到升级 macOS 27 后 Rosetta 似乎被移除了 [33]。**（核查更正）** 这只是单一厂商带保留措辞的观察，不是 Apple 的说法，但自检仍然是正确的缓解措施。建议不仅首次启动时检查，每次启动也检查，因为系统升级后可能需要重新安装 [推断]。

**P2：社区与治理**
9. **bus factor ≥ 2**：签名证书和 Sparkle EdDSA 私钥由两人以上托管；release 流程写成文档（frankea 的 GOVERNANCE.md 可以作为反面参照）。
10. **向上游回馈**：Wine MR、DXMT、MoltenVK、winetricks，以及 Gcenx 的 macports-wine。这是对 Whisky 停更理由的正面回应，也能降低“免费替代品分流 CodeWeavers 收入”的舆论风险。
11. **分发**：官方 Homebrew cask 只收录通过 Gatekeeper 的应用，所以 App 和所有 Wine Mach-O 都要签名并公证；另设自己的 tap 作为后备（用户需要先 `brew trust`）。
12. **可选差异化功能**：参照 Sikarugir 的“导出为独立 .app wrapper”，方便分享和离线使用。

---

## 风险

1. **上游单点**：Gcenx、frankea、dappermint 都是个人维护者。只要其中一人停下，Cider 依赖的构建输入（Portfile 参数、CX rebase 分支）就会中断。缓解办法是 fork 并自建镜像。[中]
2. **D3DMetal 与 CX ABI 绑定**：D3DMetal 依赖 CX Wine 内部结构，每次 GPTK 大版本更新（4.0 引入 Metal 4）都可能要求同步更新 CX 补丁。另外，GPTK 4 的 D3DMetal 是否仍然只有 x86_64 **尚未核实**。[中]
3. **Rosetta 的生命周期**：
   - Porting Kit 博客观察到，升级 macOS 27 后 Rosetta 似乎被卸载了。原文措辞带保留，属于单一厂商观察，没有 Apple 一手来源 [33]。
   - 02 号报告提到 macOS 27 是最后一个完整支持 Rosetta 的版本；`winecx-gptk` 的 README 也说 Rosetta 在 macOS 28 中“largely discontinued” [73]。
   - 所以 **x86_64 + Rosetta 这条引擎路线**有明确的到期日。
   - **（核查更正）** 但这不等于整个 x86 兼容路线都到期。CodeWeavers 已经在 CrossOver Preview 2026082 中发布了 FEX 构建（早期状态）[81]，说明非 Rosetta 的替代路线已经存在。Cider 应该把 arm64 + FEX 路线列入路线图（见 06 号报告），不要把 x86_64 引擎当作唯一的形态。[中]
4. **GPL-3.0 的锁定效应**：一旦 fork Whisky 代码，App 层就不能再换成宽松许可。[高]
5. **舆论和社区关系**：Whisky 作者公开把“损害 CrossOver 和 Wine 资金”作为停更理由，Cider 会面对同样的质疑。[高]
6. **数据许可不明**：Lutris 脚本、Bottles 清单、Gcenx 仓库都没有明确许可，批量导入存在不确定性。[中]
7. **frankea/Whisky 可能成为直接竞品，也可能停更**：它和 Cider 的定位高度重合。[低]

## 未解问题

1. `dappermint/winecx-gptk` 和 `frankea/winecx-gptk` 能否补上许可证（例如 MIT 或 LGPL），好让 Cider 直接复用 CI？可以直接联系 @dappermint。
2. GPTK 4 评估环境随附的 Wine 是否仍是 7.7？D3DMetal 4 对宿主 ABI（`macdrv_functions`/`GFXT`）有没有改动？最低需要哪个 macOS 版本？GPTK 4 中 D3DMetal 的 `License.pdf` 与 3.0 相比有没有变化（核查时没有找到 4.0 的许可文本）？
3. Sikarugir 为什么拒绝 CX 26 引擎（Issue #238 没有公开理由）？是因为新 WoW64、wrapper 结构，还是维护成本？
4. Gcenx 对 Homebrew 禁用的应对计划（Issue #168 没有答复）：是否会建自己的 tap，或者申请签名？
5. umu-protonfixes 中有多少规则只用到 winetricks、DLL override、参数这类与平台无关的原语，可以直接用于 macOS？需要实际抽样统计。
6. Porting Kit 的配方数量，以及它是否愿意开放配方数据。

## 参考来源

1. https://github.com/Whisky-App/Whisky — README：维护公告、GPL-3.0、基于 CX 22.1.1 + GPTK、依赖列表
2. https://api.github.com/repos/Whisky-App/Whisky — archived=true，pushed_at 2025-05-11，15,107 star
3. https://docs.getwhisky.app/maintenance-notice.html — 维护停止公告全文（最后修改 2025-04-09，commit f157e66）
4. https://appleinsider.com/articles/25/04/16/whisky-development-ends-on-macos-to-help-wine-flourish — 报道停更原因
5. https://www.macrumors.com/2025/04/23/whisky-ends-mac-gaming-tool-crossover/ — 报道作者引语
6. https://www.codeweavers.com/blog/jramey/2025/04/18/whisky-s-legacy-and-the-spirit-it-leaves-behind — CodeWeavers 的回应（403，内容来自搜索摘要）
7. https://raw.githubusercontent.com/Whisky-App/Whisky/main/WhiskyKit/Sources/WhiskyKit/WhiskyWine/WhiskyWineInstaller.swift — 引擎下载 URL 与安装路径
8. https://raw.githubusercontent.com/Whisky-App/Whisky/main/WhiskyKit/Sources/WhiskyKit/Wine/Wine.swift — 进程启动、环境变量、注册表 helper
9. https://raw.githubusercontent.com/Whisky-App/Whisky/main/WhiskyKit/Sources/WhiskyKit/Whisky/BottleSettings.swift — 设置到环境变量的映射
10. https://raw.githubusercontent.com/Whisky-App/Whisky/main/WhiskyKit/Sources/WhiskyKit/Whisky/Bottle.swift（以及同目录的 ProgramSettings.swift）— Metadata.plist 与程序设置
11. https://raw.githubusercontent.com/Whisky-App/Whisky/main/WhiskyCmd/Main.swift — CLI 子命令
12. https://raw.githubusercontent.com/Whisky-App/wine/7.7/.github/workflows/build.yml — WhiskyWine 构建流程
13. https://github.com/Whisky-App — 组织仓库列表（WhiskyBuilder 于 2024-04-06 归档）
14. https://raw.githubusercontent.com/Homebrew/homebrew-cask/main/Casks/w/whisky.rb — cask 2.3.5，deprecate 日期 2025-04-09
15. https://data.getwhisky.app/Wine/WhiskyWineVersion.plist — 2026-09-26 用 `curl -I` 实测返回 404（Libraries.tar.gz 同样 404）
16. https://github.com/frankea/Whisky — 活跃社区分支 README 与 API（2026-01-05 创建，GPL-3.0）
17. https://api.github.com/repos/frankea/Whisky/releases — App 3.5.0–3.7.0 与 Wine Libraries 4.x 发布记录
18. https://github.com/frankea/Whisky/releases/tag/v4.6.4-beta.1 — beta 引擎的组件版本与 CI
19. https://github.com/frankea/Whisky/releases/tag/app-v3.6.0 — GPTK 导入与 Steam 库
20. https://github.com/frankea/Whisky/blob/main/docs/GOVERNANCE.md — bus factor 为 1
21. https://github.com/frankea/Whisky/blob/main/docs/DEPENDENCIES.md — 运行时依赖（Gcenx Wine 11.0、DXMT 0.80 等）
22. https://github.com/dappermint/winecx-gptk — 支持 GPTK 的 Wine 的 CI、质量门，以及“D3DMetal 只能跑在 CX 派生 Wine 上”的说明
23. https://api.github.com/repos/frankea/winecx-gptk — fork 自 dappermint，未声明许可证
24. https://github.com/dappermint/winecx — CX 源码镜像与 wine1115/1116/1117、arm64 分支
25. https://raw.githubusercontent.com/Sikarugir-App/Sikarugir/main/README.md — 要求、后端、许可分拆、D3DMetal 说明
26. https://github.com/Sikarugir-App — 组织仓库列表
27. https://raw.githubusercontent.com/Sikarugir-App/Engines/main/EngineList.txt — 引擎清单
28. https://github.com/Sikarugir-App/Sikarugir/issues/238 — CX 26 引擎请求被以 “not planned” 关闭
29. https://wineformac.org/news/blog-kegworks-sikarugir-2025.html — 第三方站点，称 2025-10-11 改名
30. https://github.com/Sikarugir-App/Sikarugir-foss-sources — Configure 源码（LGPL-2.1）
31. https://api.github.com/repos/The-Wineskin-Project/WineskinServer — 已归档，最后推送 2025-08-14
32. https://www.paulthetall.com/porting-kit-6-released/ — Porting Kit 6（2023-11-09）
33. https://www.paulthetall.com/rosetta-de-installed-on-mac-os-27-golden-gate/ — Porting Kit 博客（2026-09-20）：措辞带保留，称升级 macOS 27 后 Rosetta“似乎”被卸载；单一厂商观察
34. https://porting-kit.macupdate.com/ — 第三方下载站，Porting Kit 7.0.2
35. https://github.com/MythicApp/Mythic — README、API 与 releases（v0.6.0，2025-12-27）
36. https://github.com/MythicApp/Engine — WhiskyWine 派生，2025-12-25 归档
37. https://github.com/MythicApp/wine — `mythic-crossover-24.0.7-stable` 分支
38. https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/wiki/Using-Heroic-on-a-Mac-computer — Mac 端 Wine 选项
39. https://raw.githubusercontent.com/Heroic-Games-Launcher/HeroicGamesLauncher/main/src/backend/utils/compatibility_layers.ts — CrossOver/Whisky/GPTK 探测代码
40. https://api.github.com/repos/Heroic-Games-Launcher/HeroicGamesLauncher/releases — v2.22.1–2.22.3
41. https://github.com/italomandara/CXPatcher — README 与 releases（v0.7.1，2026-03-21）
42. https://github.com/italomandara/Procyon — CXPatcher 的继任者（GPL-3.0，需要 CrossOver 许可证）
43. https://github.com/Gcenx/macOS_Wine_builds — 官方 WineHQ macOS 包（11.18，2026-09-25）
44. https://raw.githubusercontent.com/Gcenx/macports-wine/master/emulators/wine-devel/Portfile — configure 参数与补丁（另见仓库 README）
45. https://github.com/Gcenx/game-porting-toolkit/releases — GPTK 2.1/3.0/3.0-3 重打包，VERSION 为 Wine 7.7
46. https://raw.githubusercontent.com/Gcenx/homebrew-wine/master/Casks/game-porting-toolkit.rb — tap 中的 cask
47. https://raw.githubusercontent.com/Homebrew/homebrew-cask/main/Casks/w/wine-stable.rb — `disable! date: "2026-09-01", because: :fails_gatekeeper_check`（wine@devel.rb 相同）
48. https://github.com/Gcenx/macOS_Wine_builds/issues/168 — Homebrew 禁用问题（2026-08-26，未解决）
49. https://brew.sh/2025/11/12/homebrew-5.0.0/ — 未签名 cask 废弃，2026-09 禁用
50. https://brew.sh/2026/06/11/homebrew-6.0.0/ — 第三方 tap 需 trust；Intel 进入 Tier 3；提到 macOS 27 Golden Gate
51. https://raw.githubusercontent.com/apple/homebrew-apple/main/Formula/game-porting-toolkit.rb — formula 1.1，源码为 crossover-sources-22.1.1
52. https://developer.apple.com/games/game-porting-toolkit/ — GPTK 4 与 Metal 4 评估环境
53. https://appleinsider.com/articles/26/06/17/apples-game-porting-toolkit-4-is-a-big-improvement-for-modern-game-coders — GPTK 4 beta，仅 Apple Silicon
54. https://github.com/apple/game-porting-toolkit — agent skills 与示例（Apache-2.0）
55. https://github.com/utmapp/d3dmetal-native — `GFXT` host interface 的原生实现（MIT，仅 x86_64）
56. https://github.com/dbc-hbin/d3dmetal-redistributable/releases/tag/gptk-4.0b2 — D3DMetal 4.0 beta 2 的第三方再分发
57. https://www.applegamingwiki.com/wiki/Game_Porting_Toolkit — GPTK 安装流程与 D3DMetal 在 DMG 中的路径
58. https://github.com/Gcenx/NotProton — macOS 上的 Steam Play（README 原文见 [81]）
59. https://github.com/3Shain/dxmt/releases — v0.80 是最后一个 MIT 版本，之后改为 LGPL（仓库 LICENSE 为 LGPL-2.1+，“Feifan He for CodeWeavers”）
60. https://www.playonmac.com/en/news.html — PlayOnMac 新闻，最后一条 2022-03-14
61. https://api.github.com/repos/PhoenicisOrg/phoenicis — 最后推送 2025-04-29
62. https://api.github.com/repos/PlayOnLinux/POL-POM-4 — 2026-02 仍有推送
63. https://github.com/bottlesdevs/Bottles — README 与 releases 67.1–67.4
64. https://raw.githubusercontent.com/bottlesdevs/dependencies/main/Essentials/vcredist2019.yml — 依赖清单示例
65. https://github.com/bottlesdevs/components — 组件索引与 pull-components CI
66. https://docs.usebottles.com/bottles/installers.md — 安装器分级
67. https://github.com/lutris/lutris — README 与 releases 0.5.20/0.5.22
68. https://raw.githubusercontent.com/lutris/lutris/master/docs/installers.rst — 安装脚本格式
69. https://github.com/lutris/lutris/issues/3117 — 第三方询问能否复用脚本（约 3,300 个游戏）
70. https://github.com/Open-Wine-Components/umu-launcher — umu 与 1.4.4 release
71. https://github.com/Open-Wine-Components/umu-protonfixes — BSD-2-Clause；`fix.py` 的加载逻辑
72. https://raw.githubusercontent.com/Open-Wine-Components/umu-database/main/umu-database.csv — CSV 结构与表头；2026-09-26 统计约 1,202 行数据、约 1,101 个不同 UMU_ID，其中不少不是 Steam AppID（GPL-3.0）
73. https://raw.githubusercontent.com/dappermint/winecx-gptk/main/README.md — README 原文：D3DMetal 只在 CX 派生 Wine 上执行；CX 26.3 差异（221 个文件）三方合并到 11.15 后前移到 11.17；源码在 `wine1117` 分支；macOS 26.0 部署下限；PE 用 mingw-w64 gcc；Rosetta 在 macOS 28 “largely discontinued”；main 下无 LICENSE/COPYING
74. https://raw.githubusercontent.com/Homebrew/homebrew-cask/main/Casks/w/wine@devel.rb — 版本 11.16，同样写有 `disable! date: "2026-09-01", because: :fails_gatekeeper_check`
75. https://api.github.com/repos/frankea/Whisky — created_at 2026-01-05，pushed_at 2026-09-16，GPL-3.0，fork=false，788 star
76. https://github.com/Gcenx/game-porting-toolkit/releases/tag/Game-Porting-Toolkit-3.0-3 — 2026-03-03；基于 homebrew-apple commit 2bc4428；建议 16 GB 内存；要求 Apple Silicon 和 macOS 14+
77. https://raw.githubusercontent.com/Gcenx/game-porting-toolkit/main/VERSION — “Wine version 7.7”
78. https://raw.githubusercontent.com/Open-Wine-Components/umu-protonfixes/master/LICENSE — BSD 2-Clause
79. https://raw.githubusercontent.com/Open-Wine-Components/umu-database/main/LICENSE — GPL-3.0
80. https://raw.githubusercontent.com/Sikarugir-App/Sikarugir/main/D3DMetal/3.0/License.pdf — D3DMetal 3.0 许可文本（§2A(i) 使用范围、§2A(iii) 非商业分发、§2C 单独分发 Redistributables）
81. https://raw.githubusercontent.com/Gcenx/NotProton/main/README.md — 目标 Steam 客户端 1788652215/1790121765 与 CrossOver Preview 2026082；FEX 构建和 Rosetta 构建都支持，FEX 构建处于早期状态
82. https://github.com/Gcenx/macOS_Wine_builds/releases/tag/11.18 — 2026-09-25，只有 `wine-{devel,staging}-11.18-osx64.tar.xz`，没有 arm64 构建
83. https://raw.githubusercontent.com/3Shain/dxmt/v0.80/LICENSE — v0.80 tag 为 MIT（Copyright 2023 Feifan He）
84. https://raw.githubusercontent.com/3Shain/dxmt/main/LICENSE — main 为 LGPL-2.1+（“Copyright (c) 2023-2026 Feifan He for CodeWeavers”）

## 事实核查记录

> 核查日期 2026-09-26，由独立核查者完成。结论分为“属实”“部分属实”“被驳回”“无法核实”四种；本轮 11 条中没有“被驳回”或“无法核实”的。许可证只核实了文件是否存在以及原文内容，它们的法律效力不在本文范围内（标注“法律问题不评估”）。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| Whisky 维护停止公告最后修改于 2025-04-09（commit `f157e66`）；仓库最后推送 2025-05-11，已归档，GPL-3.0；WhiskyWine 是 Wine 7.7（CX 22.1.1 + GPTK）；2026-09-26 引擎下载地址返回 404 | 属实 | 公告页、GitHub API（archived=true，15,107 star）、README 和 `Whisky-App/wine` 7.7 分支的 `build.yml` 都核对无误；2026-09-26 10:23 GMT 用 `curl -I` 复测两个地址都返回 HTTP/2 404。补充：API 的 `updated_at`（2026-09-24）只反映 star 等元数据变化；Homebrew cask `whisky` 是 deprecated，不是 disabled [1][2][3][12][14][15] |
| 官方 cask `wine-stable.rb`（11.0_1）和 `wine@devel.rb`（11.16）都写有 `disable! date: "2026-09-01", because: :fails_gatekeeper_check`，依据是 Homebrew 5.0.0（2025-11-12）的政策 | 属实 | 两个文件第 33 行都是这一行，下载的都是 Gcenx 的 osx64 tarball，并依赖 `gstreamer-runtime` cask。6.0.0（2026-06-11）重申了 2026-09 的禁用计划。这一政策只针对官方 homebrew-cask；第三方 tap 不会被禁用，只需要先 `brew trust` [47][49][50][74] |
| `dappermint/winecx-gptk` README 称 D3DMetal 只在 CX 派生 Wine 上执行，因为加载时会 patch unixcall 内部结构；当前主线把 CX 26.3 改动 rebase 到 Wine 11.17；仓库没有许可证文件 | 属实 | README 原文确认。补充了更准确的做法：CX 26.3 相对 Wine 11.0 的差异（221 个文件）先三方合并到 11.15，再逐版前移到 11.16、11.17；源码在 `wine1117` 分支，`patches/` 故意留空；部署下限 macOS 26.0；PE 部分用 mingw-w64 gcc，因为 llvm 构建的 kernelbase.dll 会卡住 Steam 的 CM 登录。§2.2 已补充。能否复用 CI 属于法律问题，不评估 [22][73] |
| `frankea/Whisky`（GPL-3.0，2026-01-05 创建）2026-08-29 发布 beta 引擎 Wine Libraries v4.6.4-beta.1：Wine 11.16，DXVK 1.10.3、DXMT 0.80、MoltenVK 1.4.2，在 GitHub 托管的 macos-15 runner 上构建，需要用户导入 GPTK | 属实 | API 与 release 页核对无误（Pre-release，不会自动安装，需要 Whisky 3.6.0+）。补充：稳定运行时在 DEPENDENCIES.md 中 pin 的是 Gcenx 11.0_1；GOVERNANCE.md 写明 release 的 bus factor 为 1，并且“does not build Wine itself” [18][20][21][75] |
| `apple/homebrew-apple` 的 formula 版本 1.1，源码是 `crossover-sources-22.1.1`；Gcenx GPTK 3.0-3（2026-03-03）基于 commit `2bc4428`，VERSION 为 “Wine version 7.7” | 属实 | 已核对。附加说明：“Wine 7.7”只对 homebrew-apple 和 Gcenx 这条线核实过；GPTK 4（WWDC26）评估环境带不带 Wine、带哪个版本，没有找到一手来源。摘要和 §9 已加上这条限定 [51][52][76][77] |
| umu-protonfixes 是 BSD-2-Clause，umu-database 是 GPL-3.0，CSV 表头为 `TITLE,STORE,CODENAME,UMU_ID,COMMON ACRONYM (Optional),NOTE (Optional),EXE_STRINGS (Optional)` | 属实 | LICENSE 文件和 CSV 第一行都核对无误。原稿 §13 列出列名时省略了“(Optional)”后缀，只是格式问题，已按原文补全。许可证只核实了文件存在，许可范围属于法律问题，不评估 [72][78][79] |
| umu-database 约 2,000 行，以 Steam AppID 作为统一键（§13；P1-5 写“umu-ID（即 Steam AppID）作为主键”） | 部分属实 | **更正**：2026-09-26 统计约 1,202 行数据、约 1,101 个不同 `UMU_ID`。多数 ID 是 `umu-<SteamAppID>`，但 `umu-genshin`、`umu-endfield`、`umu-identityv`、`umu-cxbxreloaded` 以及 UUID 形式的 ID 都不是 AppID，另有少数 ID 格式有误。Cider 的主键应该是不透明的 umu-ID 字符串，而不是整数 Steam AppID。已修改摘要、§13、§15 表格和 P1-5 [72] |
| P0-3 与 §3：D3DMetal 必须由用户导入，App 不打包 D3DMetal（许可证“限制严格，不能用于商业移植”） | 部分属实 | **更正**：“用户导入”是合理而保守的选择，但许可证文本并不强制免费 App 这样做。D3DMetal 3.0 `License.pdf` 的 §2A(iii) 允许非商业分发，§2C 允许单独分发 Framework 或 Redistributables（同样受非商业限制），§2A(i) 把使用授权限定为开发、测试或评估游戏。Whisky、Gcenx、Sikarugir 都在再分发。免费的 Cider 能否打包、GPTK 4 许可有没有变化，都需要单独决策（法律问题不评估）。已修改摘要、§3、P0-3 和未解问题 2 [45][80] |
| NotProton 需要 CrossOver Preview 2026082（§8）；风险 3 认为 x86_64/Rosetta 引擎路线有明确到期日，但没有提到 CrossOver 已有非 Rosetta 的路径 | 部分属实 | **更正**：NotProton 的目标环境是 Steam 客户端 1788652215 或 1790121765，加上 CrossOver Preview 2026082。README 写明该 Preview 有 FEX 构建和 Rosetta 构建两种，推荐 Rosetta 构建，因为 FEX 构建还处于早期状态。也就是说，CodeWeavers 已经发布了基于 FEX、不依赖 Rosetta 的 Preview。已修改 §8、§15 表格、风险 3，并在 P0-2 中新增“引擎清单标注架构、为 arm64 + FEX 路线留出位置” [73][81] |
| macOS 27 “Golden Gate” 升级会卸载 Rosetta 2（Porting Kit 博客 2026-09-20），用于 P1-8 和风险 3 | 部分属实 | **更正**：博客原文措辞带保留（“It seems that…”），只是单一厂商的观察，不是 Apple 的说法，也没有找到 Apple 一手来源。Onboarding 自检 Rosetta 仍然是正确的缓解措施。Homebrew 6.0.0 独立印证了 “Golden Gate” 这个名称和 macOS 27 不再支持 Intel。已修改 §4、P1-8、风险 3 和参考来源 33 [33][50] |
| Gcenx `macOS_Wine_builds` 最新 11.18（2026-09-25），只有 x86_64；DXMT 0.80 是最后一个 MIT 版本，仓库现为 LGPL-2.1+（“Feifan He for CodeWeavers”） | 属实 | 11.18 只有 osx64 资产，前几版是 11.17（09-11）、11.16（08-24）、11.15（08-08）。v0.80 tag 的 LICENSE 是 MIT，main 是 LGPL-2.1+。补充：v0.80（2026-04-23）同时也是最新的 DXMT release，所以 frankea pin 在 0.80 并不算落后；Gcenx Issue #168 仍未关闭。许可证只作为构建输入核对，法律效力不评估 [48][59][82][83][84] |
