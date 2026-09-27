# CrossOver 开源 Wine 源码与上游差异（CW/CX hacks）深度分析 —— 以及 Cider 的 Wine 基线选型

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 本次直接读取一手来源（官方页面、源码文件、LICENSE、release 页）验证；**[中]** = 来自可信第三方（媒体报道、社区 README、搜索摘要转述官方博客）或仅部分验证；**[低]** = 推断或无法核实。codeweavers.com 博客、phoronix、winehq gitlab 对自动抓取返回 403/Anubis 拦截，相关内容只能通过媒体转述或搜索摘要获取，均按 [中] 处理。文中“源码核验”均指通过 GitHub 镜像（PhoenicisOrg/winecx = CX 25.1.0，dappermint/winecx `crossover-26.3.0` 分支 = CX 26.3.0）读取的文件，未克隆、未下载 tarball。**镜像保真度说明**：dappermint 的 `crossover-26.3.0` 分支是 dappermint 本人在 PhoenicisOrg 历史之上做的一次性导入，只有一个 “winecx-26.3.0” 提交（2026-08-12）[81]。两个镜像都属第三方导入，均未与官方 tarball 逐字节比对，其与 tarball 一致是一个假设。
>
> **修订说明（2026-09-26）**：本版已按独立事实核查的结论修订正文，被更正的地方就地注明“（核查更正）”，完整的核查记录见文末“事实核查记录”。

---

## 摘要

- **最新源码**：截至 2026-09-26，CodeWeavers 源码页面只列出 **CrossOver 26.3.0**，下载地址为 `https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz` [1]。CX 26.3.0 于 2026-07-21 发布，CX 26.x 基于 **Wine 11.0**（镜像中 VERSION 文件为 “Wine version 11.0”）[3][17][61]。changelog 中没有 26.4 或 26.3.x，搜索也没有找到。公告论坛返回 403，因此无法排除刚发布、尚未列入 changelog 的版本 [3]。[高]
- **源码包的范围**：页面列出 29 个 FOSS 组件，包括 Wine、vkd3d、DXVK、MoltenVK、FAudio、SDL、LLVM、wine-mono、Python/PyObjC/Sparkle 等 [1]。**不包含**以下几类：D3DMetal（Apple 闭源）、CrossOver 的 UI、兼容性数据库（`cxcompatdb.so`）、bottle 模板和 alt-loader 服务端。FOSS 列表里也没有 DXMT、GStreamer 和 FEX。[高/中]
- **差异规模**：第三方测得 CX 26.3 相对 Wine 11.0 改动了约 **221 个文件** [27]。[中] 改动在源码里用多种标记注明：`CW HACK nnnnn`、`CW Hack`、`CX HACK`、`CrossOver Hack #nnnnn`，旧版还有 `CROSSOVER HACK`。标记后面带 CodeWeavers 内部 bug 号，可以 grep 批量提取。[高]
- **按原样发布的 D3DMetal 需要一个 CrossOver 式的宿主，这是选型时最重要的约束**：
  - `winemac.drv/d3dmetal.c`（© 2023 Brendan Shanks for CodeWeavers，只在 x86_64 下编译，编进 winemac.so）导出 `DECLSPEC_EXPORT struct macdrv_functions_t macdrv_functions`。结构体有 24 个函数指针：前 10 个是 macdrv/Metal/`on_main_thread`，后 14 个是 Win32 的 Reg*、窗口和显示器 API。大小由 `C_ASSERT(... == 192)` 锁定。另有 `C_ASSERT(sizeof(struct d3dmetal_macdrv_win_data) == 120)` [18]。[高]
  - “CW HACK 22434” 由 `#if defined(__APPLE__) && defined(__x86_64__)` 保护。它在第一次 `pe_module_loaded` unixcall 时经 `pthread_once` **延迟**执行，不在进程启动时执行。只有 uname 报告 Darwin 主版本 ≥ 23（macOS 14）时，才以 `RTLD_LOCAL` 方式 `dlopen(getenv("CX_APPLEGPTK_LIBD3DSHARED_PATH"))`，再 dlsym `register_non_native_code_region` 和 `supports_non_native_code_regions` [20]。[高]
  - （核查更正）26.3 中 “CW Hack 24067” 只给 `KeServiceDescriptorTable` 和 `prepend_dll_path()` 加了默认可见性导出。`simulate_writecopy` 在 26.3 里是一个没有可见性属性的普通全局变量（`BOOL simulate_writecopy; /* CW Hack 22996 */`），只有 CX 25.1.0 才导出它 [14][20]。同一标签还标记了启动路径中 `dlopen` 私有 `cxcompatdb.so` 的代码块。因此这两个导出**至少服务于 CrossOver 私有的兼容性插件**，至于它们是不是 D3DMetal 所需，只是推断 [20]。[高（事实）/低（用途推断）]
  - （核查更正）可确认专属 D3DMetal 的只有这几项：22434、winemac.drv 的 `macdrv_functions` 和 `d3dmetal_objc.m`、CW HACK 22435。原文“上游原版 Wine 不能直接跑 D3DMetal / D3DMetal 只能跑在 CX 派生 Wine 上”说得过强：MIT 许可的 `utmapp/d3dmetal-native` 自己实现了 D3DMetal 期望的 “GFXT host interface”（窗口、事件、注册表、内存、swapchain、跨进程共享），能在**非 Wine** 的原生 macOS 进程中通过 D3DMetal.framework 运行 D3D11/D3D12 [63]。所以宿主契约可以复刻，不必整棵继承 CX 的 Wine 树。该项目也说明 D3DMetal.framework 只有 x86_64 版本，只能在 Rosetta 2 下运行 [63]。[中]
- **msync**（Mach semaphore 同步，`WINEMSYNC=1`）在 CX 25/26 源码中都存在（`server/msync.c`、`dlls/ntdll/unix/msync.c`，版权 Zebediah Figura 2018 / Marc-Aurel Zent 2023），上游 Wine 没有：wine-11.0 标签和 master（11.18）下 `server/msync.c` 都返回 404 [69]。CX 25.1.0 同时带 esync 和 msync；CX 26.3.0 的 ntdll/unix 和 server 已没有 esync.c，只剩 msync [13][26][30][31][67][68]。[高]
- **32 位支持**：wine32on64（CX 19 引入，靠定制 clang 实现）在 CX 25.1.0 和 26.3.0 的 configure.ac 里已找不到（完整文件 grep 均无 `win32on64`/`32on64`/`i386_on_x86_64`）[66]。32 位 bottle 改为在新 WoW64 上用 “CX HACK 20810”（`WINEWOW6432BPREFIXMODE`）模拟。CodeWeavers 于 2026-06-11 宣布 CX 27 删除 32 位 bottle 和 Intel 支持，只支持 macOS Sonoma+ 的 Apple silicon；CX 27 计划 2027 年初发布，截至 2026-09-26 尚未发布 [8][9][14][23][65][75]。[高] **Cider 只需要支持新 WoW64，不应实现 32 位 bottle。** 但要注意，取消的只是“纯 32 位 bottle”，32 位应用仍在 64 位 bottle 里运行，所以 Cider 仍要构建非空的 i386 PE 部分（`--enable-archs=i386,x86_64`）。
- **ARM64 路线已经公开**：
  - 2026-07-31 CrossOver Preview 首次提供 Mac 原生 ARM64 构建，基于定制的 macOS 版 FEX（MIT 许可）。“Wine ARM64EC”是根据博客自述的时间线推断的（Wine 10.0 完整 ARM64EC，FEX 作为 ARM64EC/WoW64 后端），没有见到“Mac 构建使用 ARM64EC”的原文 [5]。[中/推断]
  - 该构建是 universal 包（同时含 ARM64 和 Intel Wine），ARM64 部分要求 macOS 26.5+，旧系统上仍跑 Intel Wine + Rosetta。包内**已有 ARM64 版 DXMT**，没有 D3DMetal，DX12 支持“即将到来”。许多启动器不能用，旧 bottle 不能转换 [5][6][7]。[中] 也就是说，ARM64 轨道上 DX11 已可通过 DXMT 运行，缺的是 DX12。
  - Apple 开发者新闻（2026-09-01）声明 macOS 27 是最后一个通用支持 Rosetta 的版本，之后只保留例外：“依赖 Intel 框架的老旧、无人维护游戏”。x86_64 Wine 是否属于这个例外，Apple 没有说明 [10]。[高]
- **GPTK 官方 formula 很旧，但免费生态已不再落后**（核查更正）：Apple 官方 Homebrew formula `game-porting-toolkit` 仍是 1.1，基于 `crossover-sources-22.1.1`（Wine 7.7），同时构建 wine64 和 wine32on64，此后没有随 GPTK 2/3/4 更新 [36][77][78]。原版 Whisky-App/Whisky 基于 CX 22.1.1 + GPTK，2025-04-09 发布停更公告，2025-05-11 归档 [43][44]。[高] 但到 2026-09，“免费替代品都落后约四年”的推论已经过时 [76][27][40][47]（[中]）：
  - 活跃社区分支 `frankea/Whisky` 使用 Wine 11.0，开箱带 DXMT；
  - `dappermint/winecx-gptk` 把 CX 26.3 的 hack 合入 Wine 11.15–11.17；
  - Sikarugir 有 WineSikarugir10.0 引擎（其 CX 引擎停在 24.0.7）；
  - Gcenx 发布上游 WineHQ 11.18 的 macOS 包。
- **选型建议**：采用 **(d) 混合路线**。以上游 Wine 为基线，把 CX 源码包的差异拆成分主题的补丁队列，按“D3DMetal/DXMT ABI → 平台（Rosetta/Dock/msync）→ 应用特例”分级移植；再从 wine-staging 挑选少量补丁；FEX/ARM64EC 作为第二条轨道。短期先原样构建 CX 26.3，作为对照基线（兼容性 oracle）。移植 D3DMetal 宿主时，先实测 libd3dshared 实际引用了哪些符号，不要整体照搬 CW Hack 24067；同时参考 `utmapp/d3dmetal-native` 的 GFXT 宿主实现 [63]。（核查更正）

---

## 详细调研

### 1. CodeWeavers 源码的发布位置、内容与许可证

**发布位置** [高]
- 页面：`https://www.codeweavers.com/crossover/source`，标题为 “FOSS Components for CrossOver 26.3.0”，只给出 26.3.0 一个下载链接 [1]。
- 文件名规律：`crossover-sources-<ver>.tar.gz`，目录为 `media.codeweavers.com/pub/crossover/source/`。该目录对自动抓取返回 403，因此没能列出历史版本清单；社区镜像和 Apple formula 能证明旧版文件仍按同一命名存在（例如 22.1.1）[36]。
- 解包结构：历史上解包后是 `sources/<组件>`，例如 MacPorts 旧 Portfile 使用 `worksrcdir sources/wine` [60]。
- 页面自己的说明是：建议用户直接去各项目官网获取源码，因为上游“通常版本更新”[1]。也就是说，CodeWeavers **不提供完整的 CrossOver 构建脚本**，这一点与社区经验一致 [50]。

**26.3.0 页面列出的组件与许可证**

说明：Wine、DXVK、MoltenVK、FEX、DXMT 的许可证本次已读取 LICENSE 核实 [56][34]；其余为业界公知的许可证，本次未逐一核对，标 [中]。

| 组件 | 许可证 | 对 Cider 的意义 |
|---|---|---|
| Wine | LGPL-2.1+ | 核心，可自由分发，修改需开源 |
| vkd3d | LGPL-2.1+ | D3D12→Vulkan（CX 26.0 用 1.18） |
| DXVK | zlib/libpng（已核实） | D3D8–11→Vulkan；Mac 上需要 MoltenVK 或 KosmicKrisp |
| MoltenVK | Apache-2.0（已核实） | Vulkan→Metal |
| FAudio、SDL、MojoSetup | zlib | 音频、手柄、安装器 |
| FreeType | FTL / GPLv2 双许可 | 字体 |
| GnuTLS、GMP、Nettle | LGPL（GMP/Nettle 为 LGPLv3 或 GPLv2） | TLS |
| Samba、cabextract、po4a | GPL | 附带工具（ntlm_auth 等） |
| wine-mono | 以 MIT 为主 | .NET |
| LLVM | Apache-2.0 with LLVM exception | 旧版用于 wine32on64 的定制 clang |
| Python、PyObjC、PyXDG、htmltextview.py、Sparkle | PSF / MIT 等 | 说明 **CrossOver Mac 的 UI 层是 Python + PyObjC + Sparkle**；UI 本身不开源 |
| libxml2 / libxslt、libjpeg、Perl XML::* | MIT / IJG / Perl 许可 | 依赖库 |
| UnRAR | unRAR 专有免费许可（不得用来重建 RAR 压缩器） | Cider 应避免捆绑，或单独评估 |

**不在列表中、但 CrossOver 实际使用的组件**
- **D3DMetal**：Apple 闭源。关于再分发条款，两个第三方说法不同，均未对照 Apple 原文核实，**存疑**：
  - Sikarugir README 称它“许可证限制严格，不能用于商业移植”[46]。
  - `dbc-hbin/d3dmetal-redistributable` 引用 GPTK 4.0 beta 2 的 License.rtf §2A/§2C，称可以出于**非商业目的**再分发 D3DMetal.framework，条件是不修改、完整分发、保留 Apple 版权声明，并有“Apple 品牌硬件”限制 [64]。

  [中/存疑] 法律判断不在本报告范围。工程上的结论见 §风险 1 和 P1。
- **DXMT**：当前 LICENSE 为 LGPL-2.1+，版权行写着 “Copyright (c) 2023-2026 Feifan He **for CodeWeavers**”，可见作者受雇于 CodeWeavers [34]。v0.80（2026-04-23）是最后一个 MIT 许可版本 [35]。[高]
- **GStreamer、wine-gecko**：没有列出。
- **FEX**：仅用于 Preview，MIT 许可 [56]。
- **CrossOver 私有部分**：UI、`cxcompatdb.so`（按游戏自动启用设置的数据库，由开源 ntdll 按 CW Hack 24067 在启动时 dlopen [20]）、`bottlewrapper.pyc`、bottle 模板等 [32]。

**Cider 的结论**：CX 源码包只提供“Wine 及其依赖的开源部分”。Cider 仍需自研：UI、bottle 管理、按游戏配置数据库、D3DMetal 的获取与合规分发方案、打包与签名流程。

### 2. 社区镜像与衍生版本（截至 2026-09）

| 仓库 | 内容 | 新鲜度 | 置信度 |
|---|---|---|---|
| `Gcenx/winecx`（`crossover-wine` 分支） | 过去最常引用的 CX Wine 镜像 | **2026-09-26 访问返回 404**（仓库主页和 `/tree/crossover-wine` 都是 404，经独立核查确认）[79]，可能已删除或改名；搜索引擎仍有缓存 | 高 |
| `PhoenicisOrg/winecx` | CX Wine 镜像，每个版本一次提交 | 最后提交 “winecx-25.1.0”（2025-09-13），VERSION 为 Wine 10.0 [12] | 高 |
| `dappermint/winecx` | 分支 `crossover-26.3.0`（2026-08-12）、`wine1115`/`wine1116`/`wine1117`、`arm64`、`arm64-1117`（2026-09-23）[17][80]。`crossover-26.3.0` 是在 PhoenicisOrg 历史之上的单次导入提交，VERSION 为 “Wine version 11.0” [61][81] | **最新**，而且已在做“CX 26.3 差异 → 上游 11.17”的混合移植；但属第三方导入，未与 tarball 比对 | 高 |
| `dappermint/winecx-gptk` | 在 CI 中构建“支持 GPTK 的 Wine 运行时”，给 Whisky 社区分支使用（核查称即 `frankea/Whisky`）：CX 26.3 的 221 文件差异经三方合并到 Wine 11.15，再前移到 11.16、11.17 [27] | 2026-09 仍活跃 | 中 |
| `adurham/winecx` | dappermint 的分支，2026-09 在做 VRR（`CAMetalDisplayLink`）、离屏 D3DMetal surface、`WHISKY_EXTERNAL_MODE_CONTROL`；2026-08 有 Wine 11.15 适配提交（msctf、server、WOW64 thunk、D3D12 声明）[28] | 活跃 | 中 |
| `tbodt/crossover-wine` | 带 git 历史的 CX Wine | 停在 `crossover-20.0.0`（2020-11）[58] | 高 |
| `Gcenx/game-porting-toolkit` | 从 crossover-wine 17.x 到 22.1.1 逐版导入，之后是 GPTK 1.0 beta1–4 和 1.1（2023-12-14 提交）[37] | 最新 release 为 GPTK 3.0-3 [37] | 高 |
| `apple/homebrew-apple` 的 `game-porting-toolkit.rb` | version 1.1，源码 `crossover-sources-22.1.1.tar.gz`（sha256 `cdfe282c…1831`），分别构建 wine64 和 wine32on64（`win32on64` 配置）[36] | 仓库只有 7 次提交，最后一次是 “Game Porting Toolkit 1.1”（2023-11）[78]，formula 没有随 GPTK 2/3/4 更新 | 高 |
| `Gcenx/macOS_Wine_builds` | 官方 WineHQ macOS 包（stable/devel/staging），**上游 Wine，不是 CX** | 11.18 发布于 2026-09-25 [40] | 高 |
| `Gcenx/macports-wine` | `emulators/crossover` 是 CX 26.3.0 **二进制试用版的重打包**（删除 DXVK 的 d3d9/d3d10 并做 ad-hoc 签名）；`wine-devel` 11.18 从源码构建，只支持 x86_64 [39] | 2026-09-23 | 高 |
| `Gcenx/NotProton` | 在 macOS Steam 客户端里启用 Steam Play：带 lsteamclient、steam-shim、`ntdll-patch`。README 写明需要 “CrossOver Preview 2026082”，FEX 和 Rosetta 构建都支持（建议用 Rosetta）[41] | 2026-09-25 | 高 |
| `Gcenx/kosmickrisp-dxvk` | Mesa KosmicKrisp（Vulkan-on-Metal）分支，用最少的补丁让 DXVK 在 CrossOver 下运行；通过 `CX_LIBVULKAN` 指向驱动 dylib，实现了 `VK_EXT_map_memory_placed`（用 `mach_vm_remap`）[42] | 2026-07-06 | 中 |
| Whisky / WhiskyWine | 基于 CX 22.1.1 + GPTK（Wine 7.7）；2025-04-09 发布维护停止公告，2025-05-11 归档 [43][44] | 已停止 | 高 |
| `frankea/Whisky`（核查补充） | 自称“已归档 whisky-app/whisky 的活跃社区分支”，使用 **Wine 11.0**，开箱带 DXMT，另有 DXVK/MoltenVK 回退，并支持 D3DMetal；README 称有 1,026 个提交，具体组件版本见 `docs/DEPENDENCIES.md` [76] | 活跃（2026-09） | 中 |
| Brandy（`sasobhabha/Brandywine`） | 自称 Whisky 的延续，提到 “Wine 11.5” 和“更新的 CrossOver 层” | 细节不明 | 低 |
| Sikarugir（原 Kegworks/Wineskin） | 引擎包括 `WS12WineCX24.0.7`（Wine 9.0）、`WS12WineCX23.7.1`、`WS12WineGPTK1.1`、`WS12WhiskyWine2.4.4`、`WS12WineSikarugir10.0_x`；要求 macOS 14.6+ 和 Rosetta [46][47] | 用户请求 CX 26 引擎的 Issue #238 于 2026-06 以“not planned”关闭 [48] | 高 |

**要点**（核查更正）：
- 在 CX 26.3 差异上做上游前移的主要是 dappermint 系列仓库（以及基于它的 adurham 分支），它们实际上已经在跑本文推荐的混合路线。
- 原文“老牌社区项目都停在 CX 22–24”不准确。停在旧版本的只有原版 Whisky（CX 22.1.1，已归档）和 Sikarugir 的 **CX 引擎**（24.0.7）。Whisky 一系已经迁到 Wine 11.x：`frankea/Whisky` 用 Wine 11.0 + DXMT，部分项目还携带 CX 26.3 的 hack。Sikarugir 另有 Wine 10.0 引擎。所以 macOS 上的免费 Wine 生态并没有卡在 CX 22–24 [76][27][47]。
- 剩下能访问的 CX Wine 镜像（PhoenicisOrg、dappermint）都是第三方导入，随时可能消失。Gcenx/winecx 已经 404 [79]，**Cider 应自行归档官方 tarball**。
- Whisky 维护者明确说过，继续基于 CX 的 Wine 做免费替代品会侵蚀 CodeWeavers 的收入 [44]。这一点属于 Cider 的社区关系和道德风险，下文单独讨论。

### 3. CodeWeavers 在 macOS 上相对上游携带的补丁（逐项证据）

下表来自对 CX 25.1.0 和 26.3.0 关键文件的直接阅读 [14][15][16][18]–[26]。

#### 3.1 32 位：从 wine32on64 到“在 WoW64 上模拟 32 位 bottle”
- **历史**：2019-12 发布的 CX 19 用 wine32on64 解决 Catalina 删除 32 位支持的问题，做法是用修改过的 LLVM/clang 生成 thunk [57]。当时的构建命令是 `./configure --enable-win32on64 ...`，并需要随源码提供的 clang [50]。CX 22 时代的 GitHub Actions 和 Apple GPTK formula 仍然分别构建 wine64 和 wine32on64 [36][49]。[高]
- **现状**：
  - CX 25.1.0 和 26.3.0 的 `configure.ac` 中找不到 `win32on64`、`wine32on64`、`i386_on_x86_64` [12][17]。独立核查对两份完整文件（26.3 共 3828 行，25.1 共 3761 行）做了 grep，确认没有匹配；26.3 的 `configure` 也没有。两者都只有 `--enable-archs={i386,x86_64,arm,aarch64}` [66]。[高]（原标 [中]，已由核查升级）
  - 取而代之的是 “**CX HACK 20810**”：
    - `ntdll/unix/env.c` 第 1062–1063 行在 `wow64_using_32bit_prefix` 时注入 `WINEWOW6432BPREFIXMODE=1`。这里的标签写作 “CW HACK 20810”，grep 时注意大小写和 CW/CX 两种写法 [65]。
    - `kernelbase/process.c` 第 1052 和 1075 行在这种模式下让进程“假装不在 WoW64 中”；
    - `ntdll/unix/loader.c`（26.3 第 1525 行附近）通过 `wow64_using_32bit_prefix` 让 `.exe` 使用 32 位 builtin [14][15][23]。

    同样的代码在 25.1 中也存在。[高]
- **路线**：
  - （核查更正）“CX 26 默认创建 64 位 bottle”在 26.0.0 changelog 中没有提及，**未核实** [3]。[低]
  - 26.2.0 changelog（2026-06-09）的原话是 “Added additional warnings for 32-bit bottles”，即“增加了**更多** 32 位 bottle 警告”，说明 26.2 之前已有部分警告 [3]。[高]
  - CX 27 删除 32 位 bottle，官方理由是“即使在 x86_64 上也需要非常侵入的 hack，扩展到 ARM64 是难以想象的负担”[8]（媒体转述，[中]）。CodeWeavers 称约 97% 的用户已经在用 Sonoma 或更新的系统 [9]。
- **对 Cider**：wine32on64 已经没有价值。只用上游新 WoW64（`--enable-archs=i386,x86_64`）[52][70][71][72][73][74]。Wine 11.0（2026-01-13）的 ANNOUNCE 写明：
  - 新 WoW64 “considered fully supported”，包括 16 位应用；
  - 删除 wine64 loader，只保留单一的 `wine` loader；
  - 用 `WINEARCH=win32` 创建的纯 32 位前缀已弃用，新 WoW64 模式下也不支持；
  - 另有一项 macOS 专用修复：在 syscall dispatcher 中切换 `%gs`。

  所以 Cider **不提供纯 32 位前缀**。但 32 位应用仍在 64 位 bottle 里运行，i386 PE 部分必须构建且不能为空。

#### 3.2 msync（Mach semaphore 同步）
- **文件**：`server/msync.{c,h}`、`dlls/ntdll/unix/msync.{c,h}`；loader 中调用 `msync_init()` [13][14][26]。
- **机制**：共享内存加 bootstrap 注册的 Mach 接收端口，并有一个独立的消息泵线程，用 `__ulock_wake()` 唤醒等待线程。通过 `WINEMSYNC=1` 启用：26.3 的 `ntdll/unix/msync.c` 第 422 行为 `getenv("WINEMSYNC") && atoi(getenv("WINEMSYNC"))`，`loader.c` 在启动时调用 `msync_init()`。队列长度由 `WINEMSYNC_QLIMIT` 控制，marzent 原版默认 50 [13][29]。核查发现，CX 26.3 的 `server/msync.c` 第 628–629 行同样读取 `WINEMSYNC_QLIMIT`，所以这个变量在 CX 里也有效，不只存在于 marzent 原版 [67]。两个 msync.c 的文件头都是 “mach semaphore-based synchronization objects / Copyright (C) 2018 Zebediah Figura / Copyright (C) 2023 Marc-Aurel Zent”。[高]
- **性能**：marzent 给出的数据是在 M2 Max、CX 23 上测得：竞争等待 msync 3.8 s，esync 7.4 s，服务器同步 170+ s；FFXIV 室内场景 219 FPS 对 145 FPS [29]。[中：作者自测]
- **版本变化**：
  - CX 25：UI 里同时有 ESync 和 MSync [31]。CX 25.1.0 源码的 server/ 和 ntdll/unix 中 `esync.{c,h}` 与 `msync.{c,h}` 并存 [68]。
  - CX 26：源码的 ntdll/unix 和 server 已没有 esync.c，只剩 `msync.{c,h}`，server 另有 `inproc_sync.c`，ntdll/unix 另有 `sync.c`。UI 只剩 MSync，`WINEESYNC` 被当作旧键迁移掉 [26][30][32][68]。
  - 25.1.0（2025-08-12）修复过“开启 msync 后 Steam 下载问题”[3]。
- **上游状态**：上游 Wine 11.0 的 NTSync 需要 Linux 内核模块，macOS 用不上。msync 未进入上游：wine-mirror 的 `wine-11.0` 标签和 master（VERSION 11.18）下，`server/msync.c` 与 `dlls/ntdll/unix/msync.c` 都返回 404 [69]。[高] **Cider 必须自己携带并维护 msync。**

#### 3.3 D3DMetal 集成胶水与相关 ntdll 改动（d3d11/d3d12/dxgi 如何路由到 D3DMetal）

> （核查更正）本节并非所有条目都专属 D3DMetal。已确认专属 D3DMetal 的是 `d3dmetal.c`/`d3dmetal_objc.m`、CW HACK 22434、22435。CW Hack 24067 至少部分服务于私有的 `cxcompatdb.so`，它与 D3DMetal 的关系只是推断 [20]。
- **winemac.drv/d3dmetal.c**（© 2023 Brendan Shanks for CodeWeavers，`#if defined(__x86_64__)`，编进 winemac.so）[18]：
  - 导出 `DECLSPEC_EXPORT struct macdrv_functions_t macdrv_functions`。24 个函数指针的顺序是：先 10 个 macdrv/Metal/`on_main_thread`，再 14 个 Win32 的 Reg*、窗口和显示器 API（核查逐行确认，依据 d3dmetal.c 第 25、39–66、90、397 行）。其中包括：`macdrv_create_metal_device`、`macdrv_view_create_metal_view`、`macdrv_view_get_metal_layer`、`macdrv_get_cocoa_window`、`get_win_data`/`release_win_data`、Reg* 系列、`EnumDisplayMonitors`、`SetWindowPos`、`on_main_thread` 等。
  - 用 `C_ASSERT(sizeof(struct macdrv_functions_t) == 192)` 锁定 ABI。
  - 另有一个 120 字节的 `d3dmetal_macdrv_win_data` 适配结构（`C_ASSERT(sizeof(struct d3dmetal_macdrv_win_data) == 120)`），用来兼容 D3DMetal 预期的旧 `macdrv_win_data` 布局。
- **winemac.drv/d3dmetal_objc.m**（2025 年新增）：`WineMetalLayer` 重写 `-nextDrawable`，沿 `WineMetalView → WineContentView → WineWindow` 查找 client surface，然后投递 `CLIENT_SURFACE_PRESENTED` 事件 [19]。`window.c` 中的 “CW HACK 22435” 负责释放 `d3dmetal_client_surfaces` [24]。
- **ntdll/unix/loader.c** [14][20]：
  - “CW Hack 24067”（核查更正）：26.3 中共有三个代码块 [20]。[高]
    1. 第 158 行：`KeServiceDescriptorTable` 加 `__attribute__((visibility("default")))`。
    2. 第 365–367 行：`prepend_dll_path()` 加同样的属性。
    3. 约第 2246–2258 行（两份核查给出的结束行分别为 2256 和 2258）：在启动路径（`start_main_thread`）中用 `asprintf("%s/cxcompatdb.so", ntdll_dir)` 拼出路径，再以 `RTLD_LOCAL | RTLD_LAZY` 方式 `dlopen` CrossOver 私有的 **cxcompatdb.so**（按游戏兼容性数据库）。

    原文还把 `simulate_writecopy` 列入 24067，这对 26.3 不成立。26.3 第 1388 行是普通全局变量 `BOOL simulate_writecopy; /* CW Hack 22996 */`，没有可见性属性，`unix_private.h` 中也只有 `extern` 声明 [62]。只有 CX 25.1.0 的 loader.c（第 1266 行）才以 24067 导出它 [14]，所以这个导出是在 25.1 到 26.3 之间去掉的。
    **用途判断**：这些默认可见性导出至少服务于私有的 cxcompatdb.so。它们是否被 D3DMetal/libd3dshared 使用只是推断，Cider 做不到也不需要复刻 cxcompatdb。[低]
  - “CW HACK 22434”（约第 1298–1368 行；两份核查给出的结束行分别为 1363 和 1368）：
    - 整段由 `#if defined(__APPLE__) && defined(__x86_64__)` 保护。
    - `init_non_native_support()` 在第一次 `pe_module_loaded` unixcall 时经 `pthread_once` **延迟**执行，不在进程启动时执行。
    - 只有 `sonoma_or_later()`（uname 的 Darwin 主版本 ≥ 23）为真时，才以 `RTLD_LOCAL` 方式 `dlopen(getenv("CX_APPLEGPTK_LIBD3DSHARED_PATH"))`。
    - 然后取出 `register_non_native_code_region` 和 `supports_non_native_code_regions`，并记录 libd3dshared 的 `__TEXT` 段范围。[高]
  - PE 侧 `ntdll/loader.c` 的 CW HACK 22434：每加载一个 native PE 模块就调用 `unix_pe_module_loaded` 上报地址范围 [25]。
  - “CW Hack 22996”：`WINE_SIMULATE_WRITECOPY`（26.3 中 `simulate_writecopy` 已不再导出）。
- **磁盘布局与选择**（第三方对 CX 26.3 的逆向，[中]）[33]：
  - 目录为 `CrossOver.app/Contents/SharedSupport/CrossOver/lib64/apple_gptk/`，下有 `external/`（D3DMetal.framework、libd3dshared.dylib）和 `wine/x86_64-windows`、`wine/x86_64-unix`（d3d11/d3d12/dxgi PE stub、`nvngx-on-metalfx.{dll,so}`、atidxx64 等）。
  - `bin/wine` 只按完整路径引用 libd3dshared.dylib。
  - bottle 通过 `CX_GRAPHICS_BACKEND=d3dmetal` 选择 D3DMetal。
  - **推断 [低]**：libd3dshared 通过导出的 `prepend_dll_path()` 把 apple_gptk 的 wine 目录插到 DLL 搜索路径最前面，让 Apple 的 d3d11/d3d12/dxgi 覆盖 builtin 版本。dappermint 的描述也吻合：“GPTK payload 只在 CrossOver 派生的 Wine 上运行，加载时会 patch 它们的 unixcall 内部结构”[27]。但这句话没有说明具体用了哪些符号。24067 的导出同时服务于 cxcompatdb.so（见上），所以 D3DMetal 是否依赖 `prepend_dll_path`/`KeServiceDescriptorTable` **存疑**，要等实测确认。
- **非 Wine 宿主的先例**（核查补充）：`utmapp/d3dmetal-native`（UTM 组织，MIT）按 README 的说法，为“原生（非 Wine）应用”提供 D3D11 和 D3D12。D3DMetal “期望一个 GFXT host interface”，负责窗口、事件、注册表和内存服务，该项目还实现了 swapchain 和跨进程共享。该项目同样注明 “x86_64 only: D3DMetal.framework ships as x86_64” [63]。[中] 这说明 D3DMetal 的宿主契约可以独立复刻，并不绑定于 CX 的 Wine 树。
- **对 Cider**（核查更正）：
  - 必须复刻、且已确认专属 D3DMetal 的 ABI：`macdrv_functions` 的布局和大小（24 个指针，192 字节），120 字节的 `d3dmetal_macdrv_win_data`，`d3dmetal_objc.m` 的 client surface 机制和 CW HACK 22435，以及 22434 中 libd3dshared 的加载时机和 non-native code region 注册。
  - 对 24067 的导出，先用 `nm -u`/`dyld_info -imports` 检查 libd3dshared.dylib 和 D3DMetal.framework 的未定义符号，确认需要后再移植，不要整体照搬。cxcompatdb.so 的 dlopen 代码块直接舍弃。
  - 移植时参考 d3dmetal-native 的 GFXT 实现，以便理解接口语义。
  - GPTK 每次升级（3.0 → 4.0）都可能改变这些约定。
  - `d3dmetal.c` 和 22434 只在 x86_64 下编译，D3DMetal.framework 本身也只有 x86_64 版本，所以 **ARM64 版 Wine 目前没有 D3DMetal**，这与 CX Preview 的现状一致 [6][63]。

#### 3.4 DXMT 集成
- **组成**：DXMT 提供 d3d11、d3d10、dxgi，外加 `winemetal.dll` 与 `winemetal.so`（unixlib），以及 `nvapi`、`nvngx`；源码树中还有 `airconv`（着色器转换）和 **`src/d3d12`**（main 分支上已有完整的 D3D12 文件集，成熟度未知）[34]。
- **构建要求**：Meson 1.3+、Xcode 16+（含 Metal toolchain）、**LLVM 15（精确版本）**、Wine 8+ 的构建目录。交叉构建用 `-Dwine_build_path=`；需要 `--enable-archs=i386,x86_64` 才能支持 32 位 [34]。
- **版本**：CX 25.0.0 首次包含 DXMT，CX 26.0.0 升到 v0.72 [3]。v0.72（2025-12-11）起用 D3DKMT 共享资源，要求 Wine 10.18+，并加入实验性 Intel Mac 支持 [34]。v0.80 发布于 2026-04-23，之后许可证改为 LGPL [35]。
- **对 Cider**：DXMT 是开源、许可证友好、可随 Cider 分发的 Metal 后端，应作为 D3D10/11 的**首选开源后端**；它对上游 Wine 的依赖（D3DKMT 等）比 D3DMetal 小得多。

#### 3.5 Rosetta 2 相关补丁（signal_x86_64.c）[21]
- **CW Hack 24256**：Rosetta 下信号上下文里的 MXCSR 不正确，改用 `stmxcsr` 修正。
- **CW Hack 23427**：在 Rosetta 下模拟 `XGETBV`，根据 `sequoia_or_later` 决定是否报告 AVX。bottle 配置中 `ROSETTA_ADVERTISE_AVX` 由 `bin/wine` 默认导出为 1 [32]。[中]
- **CW HACK 20186**：把 Intel CET 指令当作 NOP（Big Sur 的 Rosetta 会报异常）。
- **CW HACK 22131**：Rosetta 不支持调试寄存器，直接返回成功。
- **CW Hack 24265**（核查补充，第 2794–2852 行）：源码注释说，在 M3 上 Rosetta 恢复 MXCSR 时会用 sigcontext 里最初那个错误的值，即使信号处理程序已经改过它。所以这个 hack 把 RIP 重定向到 `__restore_mxcsr_thunk`，由它从 `amd64_thread_data()->mxcsr` 重新加载 MXCSR [21]。[高] **Cider 的开发机正是 M3**，这个 hack 会直接影响开发和测试。
- 各 hack 在 26.3 `signal_x86_64.c` 中的位置（核查记录）：24256 在第 84/1004 行，23427 在第 88/2007 行，22131 在第 1151/1380 行，20186 在第 1943/2517 行，24265 在第 2794–2852 行 [21]。
- **对 Cider**：只要还用 Rosetta 跑 x86_64 Wine，这组补丁（**24256、23427、20186、22131、24265**）就是 P0。改用 FEX 后，大部分会失效或需要重写。

#### 3.6 进程、启动器与应用特例 [22][23][24]
- **CW Hack 10523 / 23741**：CrossOver **alt loader**。设置 `CX_ALT_LOADER_SOCKET` 时，把进程创建经 Unix socket（`sendmsg` 传递 fd）交给 CrossOver 的外部加载服务；白名单和黑名单分别是 `HKCU\Software\CrossOver\UseAltLoader` 与 `SuppressAltLoader`；Rockstar Launcher 被强制排除。服务端不开源。
- **CW Hack 24717**：把 PE 环境变量中带 `__CX_UNIX_` 前缀的项提升为 Unix 环境变量。
- **CW Hack 22144**：为 Wine loader 创建以应用名命名的链接，让 Dock 显示正确的应用名。
- **CW Hack 26536**：Helldivers 2 图标。
- **CrossOver Hack #16933**：Quicken（`qw.exe`/`QFRAME`）窗口。
- **CW Hack 24938**：把 EpicGamesLauncher.exe 移到默认 winstation。**CW HACK 24152**：Epic 的窗口菜单。
- **CW HACK 19252**：为 Ubisoft Connect（`upc.exe`、`UplayWebCore.exe`）生成 `vk_swiftshader_icd.json`。
- **CW Hack 24920/24557**：当 `SteamGameId=2767030` 时拦截 powershell.exe。
- **对 Cider**：这类“按 exe 名匹配”的 hack 应改造成 Cider 自己的**按应用规则引擎**（数据驱动），不要硬编码进 Wine。

#### 3.7 CEF/Chromium（Steam 及各类启动器）
- 旧版有 “CROSSOVER HACK: bug 13322 (winehq bug 39403)”：在 `kernel32/process.c` 中给 `steamwebhelper.exe` 追加 `--no-sandbox`（PlayOnLinux 留存的补丁副本）[51]。[高]
- 在检查过的 CX 26.3 `kernelbase/process.c`、`ntdll/unix/process.c`、`ntdll/loader.c` 中**没有找到** steamwebhelper 特判 [22][23][25]。[中] 近期 CEF 修复可能已进入上游或位于其他文件，本次未能全文 grep。
- 2025–2026 的 changelog 仍在不断修 Steam、Epic、GOG Galaxy、EA 和 Battle.net 问题 [3]。
- 社区衍生版（dappermint）为 Chromium 子窗口额外实现了“跨进程 CAContext 的 Metal layer 托管”[27]。这说明**多进程 GPU 合成在 macOS 上仍是薄弱点**。

#### 3.8 窗口、键盘与 Retina（winemac.drv）[16]
- `Mac Driver` 注册表键：`RetinaMode`、`LeftOptionIsAlt`/`RightOptionIsAlt`、`LeftCommandIsCtrl`/`RightCommandIsCtrl`、`CaptureDisplaysForFullscreen`、`UsePreciseScrolling`、`AllowImmovableWindows`、`EnableAppNap`、`WindowsFloatWhenInactive` 等。它们大多来自上游，UI 中的 “High Resolution Mode”（关闭像素倍增、报告 192 DPI）对应 `RetinaMode` [30]。
- CX 专有部分：**CrossOver Hack 10912**（Mac 编辑菜单）、**14364**（`force_backing_store`）、**18896**（不丢弃 WINDOW_GOT_FOCUS 事件）、**CW Hack 22310**（AppUserModelID），以及 Stage Manager 相关处理 [16]。
- 另有 `opengl_bcdec.h`（bcdec v0.96，MIT/Unlicense），用于在 OpenGL 路径上软件解压 BC1–BC7 纹理 [12]。

#### 3.9 nvapi / DLSS → MetalFX
- UI 选项 “DLSS powered by MetalFX” 只对 D3DMetal 和 DXMT 生效 [30]。
- 对应的环境变量：`D3DM_ENABLE_METALFX=1`（D3DMetal，内部是 `nvngx-on-metalfx`）和 `DXMT_ENABLE_NVEXT=1`（DXMT 自带 nvapi/nvngx）[32][33]。
- 第三方报告：GPTK 4 beta 2 的 D3DMetal 把 GPU 报告为 “AMD Compatibility Mode”（DeviceId 0x66af），因此 DLSS 检测失败 [33]。[中]

#### 3.10 CX_* 与相关环境变量汇总
| 变量 | 作用 | 来源 |
|---|---|---|
| `CX_GRAPHICS_BACKEND` = d3dmetal / dxmt / dxvk / wined3d（缺省为 Auto，由私有数据库决定） | 选择图形后端 | [32][33] |
| `CX_APPLEGPTK_LIBD3DSHARED_PATH` | libd3dshared.dylib 路径（仅 x86_64 + macOS 14+，在首个 PE 模块加载时延迟 dlopen） | 源码 [14][20] |
| `CX_ALT_LOADER_SOCKET` | alt loader 的 socket | 源码 [22] |
| `__CX_UNIX_*` | PE 环境变量转为 Unix 环境变量 | 源码 [22] |
| `CX_LIBVULKAN` | 替换 Vulkan 驱动 dylib | [42] |
| `WINEWOW6432BPREFIXMODE` | 32 位 bottle 模拟 | 源码 [15][65] |
| `WINE_SIMULATE_WRITECOPY` | 模拟 write-copy 内存语义 | 源码 [14] |
| `WINEMSYNC` / `WINEMSYNC_QLIMIT` | msync 开关与队列长度（CX 26.3 的 server/msync.c 也读取 QLIMIT） | [13][29][67] |
| `D3DM_ENABLE_METALFX`、`DXMT_ENABLE_NVEXT` | DLSS→MetalFX | [32] |
| `ROSETTA_ADVERTISE_AVX` | 让 Rosetta 报告 AVX | [32] |
| 旧键 `WINED3DMETAL`、`WINEDXVK`、`WINEESYNC` | 由 bottle 模板迁移 | [32] |

#### 3.11 默认注册表调整
在 CX 25 的 `loader/wine.inf.in` 中没有检出 CX 专有项 [12]。[中] 按游戏自动启用设置的逻辑位于私有的 `cxcompatdb.so` 和 `bottlewrapper.pyc` [32]，**不在开源范围内**。开源的 ntdll 只负责加载它：CW Hack 24067 在启动路径中从 `ntdll_dir` dlopen `cxcompatdb.so`，并给 `KeServiceDescriptorTable`、`prepend_dll_path` 加默认可见性导出，至少供它使用 [20]。Cider 需要自建兼容性数据库（例如 YAML/JSON 规则 + 社区贡献）。

### 4. CrossOver 各版本对应的上游 Wine 基线

| CrossOver | 发布日期 | Wine 基线 | macOS 相关组件 | 置信度 |
|---|---|---|---|---|
| 19.0 | 2019-12 | —— | 引入 wine32on64 | 高 [57] |
| 22.1.1 | 2023 前后 | Wine 7.7 | GPTK 1.x 和 Whisky 的基线 | 中 [36][43] |
| 23.x | 2023 | 推断为 Wine 8.x | 23.5 开始集成 D3DMetal，Sikarugir 有 23.7.1 引擎 | 低/中 |
| 24.0.0 | 2024-02-22 | **Wine 9.0** | MoltenVK 1.2.5、vkd3d 1.10、Wine Mono 8.1；最后版本 24.0.7（2025-01-28） | 高 [3] |
| 25.0.0 | 2025-03-11 | **Wine 10.0** | D3DMetal 2.1、MoltenVK 1.2.10、vkd3d 1.14、首次包含 DXMT、按游戏设置数据库；之后有 25.0.1（2025-04-23）、25.1.0（2025-08-12，修复开启 msync 后的 Steam 下载问题）、25.1.1（2025-09-15，修复 Intel Mac 在 Tahoe 上的问题） | 高 [3][12] |
| 26.0.0 | 2026-02-10 | **Wine 11.0** | D3DMetal 3.0、DXMT v0.72、vkd3d 1.18、Wine Mono 10.4.1、适配 Tahoe UI | 高 [3][17][61] |
| 26.1 / 26.2 / 26.3 | 2026-04-09 / 06-09 / 07-21 | Wine 11.0 | 只修 bug；26.2 “增加更多 32 位 bottle 警告”（此前已有部分警告）。截至 2026-09-26，changelog 中没有 26.4 或 26.3.x，搜索也未找到 | 高 [3] |
| Preview（ARM64） | 2026-07-31 | 未公开 | 定制 macOS FEX；ARM64EC 为推断；含 ARM64 DXMT；要求 macOS 26.5+；无 D3DMetal/DX12。Gcenx/NotProton 提到后续构建 “CrossOver Preview 2026082” | 中 [5][6][41] |
| 27 | 计划 2027 年初（截至 2026-09-26 未发布） | 推断为 Wine 12.0 | 仅支持 Apple silicon 和 macOS Sonoma+，删除 32 位 bottle（2026-06-11 宣布） | 中 [8][9][75] |

**节奏**：CrossOver 每年一个大版本（2–3 月），对应上一年 12 月到当年 1 月发布的 Wine x.0；随后大约每 2 个月一个修正版。上游 Wine 11.0 于 2026-01-13 发布 [52]，2026-09 的开发版是 11.18 [40]。

### 5. 当前在 macOS 上从源码构建 CrossOver 版 Wine 的可行性

**结论：可行，而且不再需要定制 clang**（wine32on64 已移除）。主要难点在依赖打包、PE 工具链选择和 D3DMetal 的 ABI 对齐。[中]

以下是在 Cider 开发机（M3、8 GB、macOS 26.5、只有 CLT）上需要准备的内容：
1. **Rosetta 2**：`softwareupdate --install-rosetta --agree-to-license`。Wine 的 Unix 侧目前只能构建为 x86_64，并在 Rosetta 下运行；Gcenx 的 MacPorts overlay 也要求在 Apple Silicon 上设置 `build_arch x86_64` [39]。
2. **x86_64 依赖链**：freetype、gnutls、SDL2、MoltenVK/vulkan-loader、GStreamer（可选）、libinotify 等。可选来源有三种：x86_64 版 Homebrew（`/usr/local`，用 `arch -x86_64` 运行）、MacPorts overlay、nixpkgs 锁版本（dappermint 的做法）[27]。
3. **bison 3+ 和 flex**：系统自带的 bison 是 2.3，版本不够。
4. **PE 交叉编译器**：可选 llvm-mingw（WineHQ 官方包的做法 [39]）或 mingw-w64 gcc。dappermint 实测**用 llvm 构建的 kernelbase.dll 会让 Steam 的 CM 登录卡住**，因此改用 mingw-w64 gcc [27]。[中]
5. **configure**：`--enable-archs=i386,x86_64`，参考上游 wine-devel 的参数：`--with-coreaudio --with-vulkan --with-sdl --without-x --with-gnutls --with-freetype ...`。WineHQ 官方包还额外打了 `0001-win32u-Enable-host-Vulkan-portability-enumeration.diff`，否则 MoltenVK 无法枚举 [39]。[高]
6. **完整 Xcode**：DXMT（需要 Metal toolchain，Xcode 16+）和 MoltenVK 都需要，只有 CLT 不够 [34]。

**已知问题**：
- 旧版 CX tarball 缺 `distversion.h`，需要手工创建；老 clang 需要 `-fcommon` [49][50]。
- 产物中残留的绝对路径要改写为 `@loader_path`。
- **i386 部分不能为空**，否则会出现 `c0000135` 错误 [27]。
- 下载的二进制要处理 quarantine 属性。
- LLVM 15（DXMT 需要）从源码构建时，8 GB 内存会非常吃紧，建议用预编译包。

**已知成功案例**：
- Apple 的 GPTK formula（CX 22.1.1）[36]；
- GabLeRoux 云构建器（到 CX 22.0.1）[49]；
- Sikarugir 的 CX 23.7.1 和 24.0.7 引擎 [47]；
- dappermint 的 CI（CX 26.3 差异合入 Wine 11.17，包含 D3DMetal）[27]。

### 6. ARM64 / FEX 与 Rosetta 退役（决定 Cider 长期架构）
- **CodeWeavers 的时间线**：
  - Wine 10.0（2025-01）提供完整的 ARM64EC；
  - 2025-11-06 CrossOver Preview 的 Linux ARM64 版本集成 FEX 处理 i386/x86-64 [11]；
  - 2026-07-31 Mac ARM64 Preview 上线 [5][6][7]：
    - universal 包，同时含 ARM64 和 Intel Wine；
    - ARM64 部分要求 macOS 26.5+，旧系统上仍跑 Intel Wine + Rosetta；
    - 含 ARM64 DXMT，因此 DX11 已能在 ARM64 上运行；
    - 暂不支持 D3DMetal 和 DX12（“Direct3D 12 support is coming soon”）；
    - 许多启动器不能用，旧 bottle 不能转换。

    博客说团队“完成了让定制版 FEX 兼容 macOS 的工作”。Mac 构建使用 ARM64EC 是根据博客时间线推断的，没有原文明确说明。[中] Gcenx/NotProton 提到 “CrossOver Preview 2026082”，并称其 FEX 构建处于“早期状态”。截至 2026-09-26，未见 ARM64 版 D3DMetal 发布的证据 [41]。
- **Apple 的 Rosetta 时间线**：Apple 开发者新闻 “Upcoming changes to Rosetta support for Intel-based macOS apps” 发布于 2026-09-01 [10]。[高] 要点如下：
  - macOS 26.4+ 可能弹出 Rosetta 提醒；
  - **macOS 27 是最后一个支持 Rosetta 的版本**，之后 Apple silicon Mac 不再运行仅限 Intel 的应用；
  - 例外是“依赖 Intel 框架的老旧、无人维护游戏”，这部分的 Rosetta 功能会继续支持。

  这个例外的适用范围是依赖 Intel 框架的游戏。x86_64 版 Wine 本身是否属于这个例外，**官方没有说明**。
- **地址空间约束**：Wine 需要低 4 GB 地址空间，但 arm64 进程的强制 pagezero 会把它占满。dappermint 的 README 声称唯一的解法是 `com.apple.developer.cross-architecture-support` entitlement，由 Apple 自行决定是否授予，而 CrossOver 的 ARM64 构建带有该 entitlement [27]。[低：Apple 文档中未能检索到该 entitlement，需要进一步核实]

---

## 对 Cider 的启示与建议（按优先级）

### Wine 基线四选一的成本对比
| 维度 | (a) 直接用 CX 源码包 | (b) 上游 + 自选补丁 | (c) wine-staging | (d) 混合（推荐） |
|---|---|---|---|---|
| D3DMetal 兼容 | 开箱即用 | 需要自行移植宿主 ABI（可参考 utmapp/d3dmetal-native 的 GFXT 实现 [63]） | 需要自行移植 | 移植一次后长期维护 |
| 与 CX 的功能一致性 | 最高 | 低 | 低 | 高（按需取舍） |
| 新鲜度 | 落后上游 0–12 个月，Preview 源码不一定公开 | 最新 | 最新，但偏 Linux | 自己控制 |
| git 历史 | 无（tarball） | 有 | 有 | 有 |
| ARM64 路线 | 取决于 CX 27 源码何时公开 | 上游 ARM64EC 已具备 | 同 (b) | 上游加 FEX |
| 维护成本（推断） | 每个 CX 版本 1–3 天做 diff 和构建 | 所有 mac hack 都要自己发现和实现，成本最高 | 补丁冲突多 | 年度 stable 变基约 2–4 人周；跟 devel 时每两周约 1–3 人天（参考 dappermint 在 11.15→11.17 期间的适配提交量） |
| 社区与道德 | “白嫖 CX”的观感最差（参见 Whisky 公告） | 最好 | 好 | 取决于回馈上游的程度 |

### 分阶段建议
- **P0（第 0–1 个月）：先搭对照基线。**
  - 首先把官方 `crossover-sources-26.3.0.tar.gz` 连同 sha256 **自行归档**。第三方镜像随时可能消失（Gcenx/winecx 已经 404），而且与 tarball 是否一致还未核实 [79][81]。
  - 在开发机上原样构建 CX 26.3.0 的 `sources/wine`（x86_64 + WoW64，i386 PE 部分不能为空），接入 GPTK 的 D3DMetal（按 `apple_gptk` 目录布局并设置 `CX_APPLEGPTK_LIBD3DSHARED_PATH`）、DXMT、DXVK+MoltenVK。
  - 建立一套游戏和启动器冒烟测试集，至少包括 Steam、Epic、GOG、EA、Battle.net、Ubisoft，再加 D3D9、D3D11、D3D12 各若干。
- **P0：建立 `cider-wine` 仓库并提取差异。** 以上游 `wine-11.0` 为基，把 CX 26.3 的整棵 wine 树覆盖上去并提交，得到“CX 差异”提交（约 221 个文件）。然后用 `grep -rniE "(CW|CX|CrossOver|CROSSOVER) ?HACK"` 加 bug 号拆成主题补丁。要忽略大小写，因为同一 bug 号会混用 CW/CX 和 Hack/HACK，例如 20810。拆分如下（核查更正）：
  1. D3DMetal ABI：`d3dmetal*.{c,m}`、CW Hack 22434（含 PE 侧上报）、22435。
  2. CW Hack 24067 **单独成组，暂缓合入**：
     - `KeServiceDescriptorTable`、`prepend_dll_path` 两个导出，先用 `nm -u`/`dyld_info -imports` 检查 libd3dshared.dylib 和 D3DMetal.framework 的未定义符号，确认 D3DMetal 确实需要后再保留；
     - dlopen `cxcompatdb.so` 的代码块直接**舍弃**，它依赖私有组件。
  3. Rosetta：24256、23427、20186、22131、**24265**（M3 上的 MXCSR 恢复问题，直接影响开发机）。
  4. msync（连同 `WINEMSYNC_QLIMIT`）。
  5. macOS 体验：Dock 命名 22144、编辑菜单 10912、焦点 18896、AppUserModelID 22310。
  6. 应用特例：改写成 Cider 的规则引擎。
  7. **舍弃**：32 位 bottle（20810）、alt loader（10523/23741/24717，依赖私有服务端）、24067 中的 cxcompatdb 加载。
- **P1：每个 CX 源码包发布时，自动对比新旧差异。** 例如 26.3 → 27.0 做 diff-of-diffs，优先吸收 D3DMetal ABI 变化和新的启动器修复。
- **P1：后端策略。**
  - D3D10/11：默认 DXMT，开源可分发，且已有 ARM64 版本（CX Preview 已带 ARM64 DXMT）。
  - D3D12：走 D3DMetal，或 vkd3d-proton/vkd3d + MoltenVK/KosmicKrisp。
  - D3D8/9：走 DXVK，可以用 `CX_LIBVULKAN` 这类机制切换到 KosmicKrisp。
  - 持续关注 DXMT `src/d3d12` 和 v1.0 路线图。
  - （核查更正）安装架构上，D3DMetal 始终作为**单独获取、不做修改的组件**，不编进 Cider 的发行包。默认由用户自行获取 GPTK，这是风险最低的做法。有第三方解读认为非商业、完整、不修改的再分发是允许的 [64]，如果属实，免费的 Cider 也可以提供应用内下载。但这一点**存疑**，未对照 Apple 原文核实，法律判断不在本报告范围。
- **P1：D3DMetal 宿主研究。** 研读 `utmapp/d3dmetal-native` 的 GFXT 宿主实现（MIT）[63]，确认 D3DMetal 对宿主的完整期望，包括窗口、事件、注册表、内存、swapchain 和跨进程共享。这样 Cider 的 winemac.drv 适配层就能按接口语义实现，而不是逐行照搬 CX；GPTK 升级时，也可以用它独立回归。
- **P1：选同步原语。** 默认 msync，同时保留服务器同步作为兼容回退。
- **P2（第 6–12 个月）：ARM64 轨道。** 以上游 ARM64EC 加 FEX（MIT）为基础，准备 arm64 的 winemac/ntdll 并跟踪 CX 27 源码。优先核实 low-4GB 所需的 entitlement，以及 macOS 26.5 的具体变化。D3D11 先用 ARM64 DXMT 打通，CX Preview 已证明这条路可行；DX12 是 ARM64 轨道上缺的一块。D3DMetal.framework 目前只有 x86_64 版本 [63]，它在 arm64 上的可用性取决于 Apple 和 CodeWeavers。
- **P2：回馈上游。** 把 msync、Rosetta 修复、winemac 改进尽量提交到 WineHQ，降低自身维护负担，也缓解“寄生”观感。

---

## 风险
1. **D3DMetal 许可与 ABI 风险**：
   - **许可**（存疑）：D3DMetal 闭源。两个第三方说法不同：Sikarugir 称它“不能用于商业移植”[46]；dbc-hbin 引用 GPTK 4.0 beta 2 License §2A/§2C，称可以**非商业**再分发，条件是不修改、完整分发、保留 Apple 声明、限 Apple 品牌硬件 [64]。Apple 的官方页面只提到它可与 Homebrew、CrossOver 配合使用 [53]。两种说法都没有对照 Apple 原文核实，法律判断不在本报告范围。工程上的做法是：D3DMetal 始终作为单独获取、不修改的组件，默认由用户自行获取 GPTK。
   - **ABI**：GPTK 升级（例如 4.0）可能改变 `macdrv_functions`、libd3dshared 加载约定或 unixcall 相关约定，导致 Cider 失效。缓解办法是参考 d3dmetal-native 的独立宿主实现，并做 GPTK 版本回归 [63]。
2. **Rosetta 退役**：macOS 28 起通用 Rosetta 消失。Apple 在 2026-09-01 宣布，之后只对“依赖 Intel 框架的老旧、无人维护游戏”保留 Rosetta [10]。x86_64 Wine 能否继续运行不确定，D3DMetal.framework 目前又只有 x86_64 版本，所以 ARM64 + FEX 路线成为硬性需求。
3. **低 4 GB 地址空间 entitlement**：如果确实需要 Apple 审批的 entitlement（未证实），开源和免费分发的 Cider 可能拿不到，ARM64 路线会被卡住。
4. **CX 源码公开节奏**：Preview（ARM64/FEX）源码是否以及何时公开尚不清楚。Gcenx/winecx 这类镜像可能随时消失，本次已观察到 404，独立核查也确认了 [79]。剩下的 PhoenicisOrg（CX 25.1.0）和 dappermint（CX 26.3.0）都是第三方导入，与 tarball 是否一致未经核实，所以 Cider 应自行归档官方 tarball [81]。
5. **上游漂移成本**：dappermint 的提交显示，每个 devel 版本都要适配 msctf、server 协议、WOW64 thunk、win32u client surface 等变化 [17][28]。
6. **社区与声誉**：Whisky 因“建立在 CX 之上且没有原创贡献”而主动停更 [44]；Cider 如果定位为“免费 CrossOver”，可能遭到 Wine 社区和 CodeWeavers 的抵触。另外不能使用 CrossOver 商标。
7. **私有部分不可复制**：兼容性数据库、alt loader、UI 的积累无法从源码获得，“功能几乎相同”只能做到开源层面的一致。

## 未解问题
- 26.3.0 tarball 的实际目录清单：是否包含 DXMT、GStreamer、构建脚本？（目录列表返回 403，未下载核实。）
- CX 24 是否已经移除 wine32on64？（25.1.0 和 26.3.0 已确认移除，24 未确认。）
- `com.apple.developer.cross-architecture-support` entitlement 是否真实存在、如何申请，以及 macOS 26.5 为 ARM64 Wine 提供了什么新能力。
- `register_non_native_code_region` 的确切语义：与 Rosetta 的 JIT/AOT 协作，还是 D3DMetal 自己的代码区管理？
- （核查新增）libd3dshared.dylib 和 D3DMetal.framework 实际引用了 ntdll/winemac 的哪些符号？CW Hack 24067 的 `KeServiceDescriptorTable`、`prepend_dll_path` 导出是 D3DMetal 必需，还是只给 cxcompatdb.so 用？需要对 GPTK 二进制运行 `nm -u`/`dyld_info -imports` 确认。
- （核查新增）CX 26 是否默认创建 64 位 bottle？26.0.0 changelog 没有提及。
- （核查新增）D3DMetal.framework 再分发条款的原文（Apple License.pdf/rtf）。目前只有第三方转述，法律判断不在本报告范围。
- （核查新增）dappermint 的 26.3.0 导入与官方 tarball 是否逐文件一致。
- GPTK 4 正式版对 CX 26.3 ABI 的兼容性；以及 D3DMetal 是否会推出 arm64 原生版本并继续使用同一套 `macdrv_functions`。
- 现代 CX 中 Steam/CEF 修复的具体位置（需要对 26.3 全树 grep）。
- CrossOver Preview 的 FEX 分支是否会进入上游 FEX，何时进入。

## 参考来源
1. https://www.codeweavers.com/crossover/source — CX 26.3.0 FOSS 组件页与 tarball 链接
2. https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz — 源码包地址（未下载）
3. https://www.codeweavers.com/crossover/changelog — 24.0–26.3 changelog（日期、Wine 基线、组件版本）
4. https://www.omgubuntu.co.uk/2026/02/crossover-26-released — CX 26（Wine 11、D3DMetal 3.0、DXMT 0.72）报道
5. https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac — Mac ARM64 Preview 官方博客（403，经搜索摘要转述）
6. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — ARM64 Preview 限制
7. https://www.tuaw.com/2026/08/02/crossover-goes-native-on-apple-silicon/ — ARM64 Preview 与 CX 27 计划
8. https://www.codeweavers.com/blog/mjohnson/2026/6/11/whats-in-and-whats-out-for-crossover-27 — CX 27 取舍（经转述）
9. https://appleinsider.com/articles/26/06/11/crossover-a-windows-to-mac-gaming-tool-goes-apple-silicon-only — CX 27 仅支持 Apple silicon 和 Sonoma+
10. https://developer.apple.com/news/?id=w5ngl9k2 — Apple：macOS 27 是最后一个通用支持 Rosetta 的版本，保留游戏例外
11. https://www.gamingonlinux.com/2025/11/codeweavers-launch-a-new-crossover-preview-adding-linux-arm64-support/ — Linux ARM64 + FEX（2025-11-06）
12. https://github.com/PhoenicisOrg/winecx — CX 25.1.0 镜像（VERSION、server/、winemac.drv/）
13. https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/server/msync.c — msync 实现与版权
14. https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/dlls/ntdll/unix/loader.c — CW Hack 24067/22144/22996/22434、CX HACK 20810（CX 25.1：第 1266 行以 24067 导出 simulate_writecopy）
15. https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/dlls/kernelbase/process.c — Epic/Ubisoft/WoW64 前缀 hack
16. https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/dlls/winemac.drv/macdrv_main.c 与 cocoa_window.m — Mac Driver 选项与窗口 hack
17. https://github.com/dappermint/winecx/branches/all — crossover-26.3.0、wine1117、arm64 分支；VERSION = 11.0
18. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/d3dmetal.c — `macdrv_functions` ABI
19. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/d3dmetal_objc.m — WineMetalLayer 与 client surface
20. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/loader.c — 26.3：CW Hack 24067 的三处（第 158 行 KeServiceDescriptorTable、第 365–367 行 prepend_dll_path、约第 2246–2258 行 dlopen cxcompatdb.so），约第 1298–1368 行的 22434，第 1388 行的 22996
21. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/signal_x86_64.c — Rosetta 相关 hack（24256/23427/22131/20186/24265）
22. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/process.c — alt loader 与 `__CX_UNIX_`
23. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/kernelbase/process.c — 26.3 的进程 hack
24. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/window.c — Quicken、Helldivers 2、d3dmetal surfaces
25. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/loader.c — CW HACK 22434（PE 模块上报）
26. https://github.com/dappermint/winecx/tree/crossover-26.3.0/server — 26.3 的 server 与 ntdll/unix 文件清单（无 esync）
27. https://github.com/dappermint/winecx-gptk — CX 26.3 差异（221 文件）前移到 11.17、构建问题、entitlement 说法
28. https://github.com/adurham/winecx/commits/ — 2026-08/09 的 Wine 11.15 适配与 VRR 提交
29. https://github.com/marzent/wine-msync — msync 原理、环境变量与基准
30. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26 — CX 26 高级设置（DLSS、MSync、High Resolution）
31. https://support.codeweavers.com/miscellanous/advanced-settings-in-crossover-mac — CX 25 高级设置（ESync + MSync）
32. https://github.com/stoicswe/Endfield_FineWine/pull/15 — cxbottle.conf 键（据称已按 26.2.0 核实）
33. https://github.com/philippremy/crossover-dx12-fix — apple_gptk 目录布局、`CX_GRAPHICS_BACKEND`、GPTK 4 beta 2
34. https://github.com/3Shain/dxmt — DXMT 源码树、release、LICENSE、docs/DEVELOPMENT.md
35. https://github.com/3Shain/dxmt/releases/tag/v0.80 — 许可证由 MIT 改为 LGPL
36. https://raw.githubusercontent.com/apple/homebrew-apple/main/Formula/game-porting-toolkit.rb — GPTK 基于 crossover-sources-22.1.1
37. https://github.com/Gcenx/game-porting-toolkit/commits/main — crossover-wine 与 GPTK 导入历史
38. https://github.com/Gcenx?tab=repositories — Gcenx 仓库概览（2026-09）
39. https://github.com/Gcenx/macports-wine — crossover 与 wine-devel 的 Portfile
40. https://github.com/Gcenx/macOS_Wine_builds/releases — WineHQ macOS 包 11.18（2026-09-25）
41. https://github.com/Gcenx/NotProton — Steam Play for macOS，基于 CrossOver Preview 2026082
42. https://raw.githubusercontent.com/Gcenx/kosmickrisp-dxvk/main/README-DXVK.md — KosmicKrisp + DXVK、`CX_LIBVULKAN`
43. https://github.com/Whisky-App/Whisky — 基于 CX 22.1.1 + GPTK，2025-05-11 归档
44. https://docs.getwhisky.app/maintenance-notice — Whisky 停更理由（2025-04-09）
45. https://github.com/sasobhabha/Brandywine — Whisky 延续项目（信息有限）
46. https://raw.githubusercontent.com/Sikarugir-App/Sikarugir/main/README.md — 渲染器列表与 D3DMetal 许可说明
47. https://github.com/orgs/Sikarugir-App/discussions/24 — Sikarugir 引擎说明
48. https://github.com/Sikarugir-App/Sikarugir/issues/238 — CX 26 引擎请求被关闭
49. https://github.com/GabLeRoux/macos-crossover-wine-cloud-builder — CX 19–22 云构建（win32on64 参数）
50. https://gist.github.com/Alex4386/4cce275760367e9f5e90e2553d655309 — CX 20 源码构建指南（定制 clang、distversion.h）
51. https://raw.githubusercontent.com/PlayOnLinux/wine-patches/master/custom/steam_crossoverhack/crossover_hack_52560.patch — 旧版 steamwebhelper `--no-sandbox` hack
52. https://www.helpnetsecurity.com/2026/01/14/wine-11-released/ — Wine 11.0（2026-01-13）：WoW64 完整、NTSync
53. https://developer.apple.com/games/game-porting-toolkit/ — GPTK 评估环境（可与 CrossOver 配合）
54. https://github.com/apple/game-porting-toolkit — GPTK 4 配套仓库（Apache-2.0）
55. https://appleinsider.com/articles/26/06/08/game-porting-toolkit-4-ushers-in-support-for-agentic-coding — GPTK 4（WWDC26）
56. https://raw.githubusercontent.com/doitsujin/dxvk/master/LICENSE、https://raw.githubusercontent.com/FEX-Emu/FEX/main/LICENSE、https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/LICENSE — zlib / MIT / Apache-2.0
57. https://www.codeweavers.com/blog/jwhite/2019/12/10/celebrating-the-difficult-the-release-of-crossover-19 — CX 19 与 wine32on64（经转述）
58. https://github.com/tbodt/crossover-wine — 停在 crossover-20.0.0 的旧镜像
59. https://github.com/Gcenx/wine-on-mac — wine32on64 局限（不支持 16 位）
60. https://raw.githubusercontent.com/macports/macports-ports/master/x11/wine-crossover/Portfile — 旧 tarball 结构 `sources/wine`
61. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/VERSION — 26.3.0 镜像 VERSION 为 “Wine version 11.0”
62. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/unix_private.h — 只有 `extern BOOL simulate_writecopy;`，没有导出属性
63. https://github.com/utmapp/d3dmetal-native 与 https://raw.githubusercontent.com/utmapp/d3dmetal-native/main/README.md — 非 Wine 的 D3DMetal 宿主（GFXT host interface，MIT；D3DMetal.framework 只有 x86_64 版本）
64. https://github.com/dbc-hbin/d3dmetal-redistributable — 第三方引用 GPTK 4.0 beta 2 License §2A/§2C（未对照 Apple 原文核实）
65. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/env.c — CW HACK 20810（第 1062–1063 行，`WINEWOW6432BPREFIXMODE`）
66. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/configure.ac 与 https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/configure.ac — 完整 configure.ac，没有 win32on64
67. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/server/msync.c 与 https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/ntdll/unix/msync.c — 26.3 的 msync（`WINEMSYNC`、`WINEMSYNC_QLIMIT`、版权头）
68. https://api.github.com/repos/dappermint/winecx/contents/dlls/ntdll/unix?ref=crossover-26.3.0、https://api.github.com/repos/PhoenicisOrg/winecx/contents/server、https://raw.githubusercontent.com/PhoenicisOrg/winecx/master/server/esync.c — 25.1 与 26.3 的 esync/msync 文件清单
69. https://api.github.com/repos/wine-mirror/wine/contents/server?ref=wine-11.0 与 https://raw.githubusercontent.com/wine-mirror/wine/master/VERSION — 上游 wine-11.0 和 master（11.18）都没有 msync
70. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — Wine 11.0 发布说明（新 WoW64 完整支持、删除 wine64 loader、弃用 WINEARCH=win32）
71. https://www.heise.de/en/news/Wine-11-0-uncorks-New-WoW64-architecture-is-complete-11140791.html — Wine 11.0 新 WoW64 完整支持
72. https://www.omgubuntu.co.uk/2026/01/wine-11-0-released — Wine 11.0 发布报道
73. https://linuxiac.com/wine-11-0-brings-fully-supported-wow64-mode/ — Wine 11.0 WoW64 报道
74. https://www.theregister.com/software/2026/01/15/wine-11-runs-windows-apps-in-linux-macos-better-than-ever/5087238 — Wine 11 报道（2026-01-15）
75. https://gigazine.net/gsc_news/en/20260612-crossover-27-removes-legacy-support-mac-intel/ — CX 27 删除 Intel 与旧版支持（2026-06-12）
76. https://github.com/frankea/Whisky 与 https://frankea.github.io/Whisky/ — Whisky 活跃社区分支（Wine 11.0 + DXMT）
77. https://raw.githubusercontent.com/Gcenx/game-porting-toolkit/main/VERSION — GPTK 1.x 的 Wine 版本 7.7
78. https://api.github.com/repos/apple/homebrew-apple/commits — 最后一次提交是 “Game Porting Toolkit 1.1”（2023-11）
79. https://github.com/Gcenx/winecx — 2026-09-26 返回 404
80. https://github.com/dappermint/winecx — CX 26.3.0 等分支所在的仓库主页
81. https://github.com/dappermint/winecx/commits/crossover-26.3.0 — 单次导入提交 “winecx-26.3.0”（dappermint，2026-08-12）

---

## 事实核查记录

> 核查日期 2026-09-26。结论分为：确认 / 部分正确 / 无法核实 / 存疑（两份核查意见不一致）。“已改”指出本文据此修改的位置。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 截至 2026-09-26 最新公开源码为 CX 26.3.0（`crossover-sources-26.3.0.tar.gz`），26.3.0 于 2026-07-21 发布，26.0.0 于 2026-02-10 发布，基于 Wine 11.0，带 D3DMetal 3.0、DXMT v0.72、vkd3d 1.18 | 确认（两份核查一致） | 源码页只列 26.3.0，共 29 个组件。changelog 中 25.0.0–26.3.0 的日期无误，没有 26.4 或 26.3.x。镜像 VERSION 为 Wine 11.0 [1][3][61]。保留意见：公告论坛返回 403，无法排除刚发布、尚未列入 changelog 的版本；镜像是第三方导入，与 tarball 一致只是假设。已改：文首的镜像保真度说明、§4 表格。 |
| `d3dmetal.c` 导出 24 指针、192 字节的 `macdrv_functions`；ntdll 按 CW Hack 24067 导出 `KeServiceDescriptorTable`、`prepend_dll_path`、`simulate_writecopy`；22434 在 macOS 14+ 上 dlopen libd3dshared | 部分正确（两份核查一致） | d3dmetal.c 部分正确，另有 120 字节的 `d3dmetal_macdrv_win_data`。**更正**：26.3 中 24067 只导出前两个符号。`simulate_writecopy`（CW Hack 22996）是没有可见性属性的普通全局变量，只有 CX 25.1.0 才导出它。24067 的第三个代码块 dlopen 私有的 `cxcompatdb.so`。22434 只在 `__APPLE__ && __x86_64__` 下编译，首次 `pe_module_loaded` 时经 `pthread_once` 延迟加载 [14][20][62]。已改：摘要、§3.3、§3.10、§3.11，参考 [14][20]。 |
| 摘要/§3.3：24067 的三项导出属于“D3DMetal 集成胶水”，所以上游原版 Wine 不能直接跑 D3DMetal | 部分正确 | 24067 至少服务于 cxcompatdb.so。已确认专属 D3DMetal 的只有 22434、`macdrv_functions`/`d3dmetal_objc.m` 和 22435。D3DMetal 使用 `prepend_dll_path`/`KeServiceDescriptorTable` 只是推断，已标为存疑 [20][27]。已改：摘要、§3.3、P0 补丁拆分（24067 单独成组，先查 libd3dshared 的未定义符号；cxcompatdb 代码块舍弃）、未解问题。 |
| 隐含：D3DMetal 只能在 CX 派生 Wine 上运行，Cider 必须精确复刻 CX ABI | 部分正确 | 按原样发布的 D3DMetal 期望一个 CX 式宿主。但 `utmapp/d3dmetal-native`（MIT）实现了 “GFXT host interface”，在非 Wine 进程中运行 D3D11/D3D12，所以宿主契约可以独立复刻。D3DMetal.framework 只有 x86_64 版本，需要 Rosetta [63]。已改：摘要、§3.3、成本对比表、选型建议，新增 P1“D3DMetal 宿主研究”。 |
| msync（`WINEMSYNC=1`，版权 Figura 2018 / Zent 2023）存在于 CX 25.1.0 和 26.3.0，上游 Wine 11.0 没有；26.3 已删除 esync.c | 确认（两份核查一致） | 补充：25.1 中 esync 与 msync 并存。26.3 的 `server/msync.c` 也读取 `WINEMSYNC_QLIMIT`（第 628–629 行）。上游 wine-11.0 和 master（11.18）仍没有 msync [67][68][69]。已改：摘要、§3.2、§3.10。 |
| CX 25.1.0/26.3.0 的 configure.ac 不含 win32on64；32 位 bottle 由 CX HACK 20810 在新 WoW64 上模拟；CX 27 删除 32 位 bottle，只支持 Sonoma+ 的 Apple silicon | 确认（两份核查一致） | 核查对完整文件做了 grep，§3.1 的置信度从 [中] 升为 [高] [66]。20810 在 env.c 中写作 “CW HACK 20810” [65]。取消的只是纯 32 位 bottle，32 位应用仍在 64 位 bottle 中运行，i386 PE 部分必须构建。CX 27 截至 2026-09-26 尚未发布 [75]。已改：摘要、§3.1、§4、P0。 |
| 2026-07-31 Mac ARM64 Preview（ARM64EC + 定制 FEX，universal 包，ARM64 部分要求 macOS 26.5+，无 D3DMetal/DX12）；macOS 27 是最后一个通用支持 Rosetta 的版本，保留老旧游戏例外 | 确认（两份核查一致），带保留 | “ARM64EC”是根据博客时间线推断的，没有原文明确说 Mac 构建使用 ARM64EC，已标注推断。补充：包内已有 ARM64 DXMT（DX11 可用，缺 DX12）；Gcenx/NotProton 提到 “CrossOver Preview 2026082”。Apple 新闻发布于 2026-09-01，例外只覆盖依赖 Intel 框架的游戏，x86_64 Wine 是否适用没有说明 [5][6][10][41]。已改：摘要、§4、§6、P1/P2、风险 2。 |
| Apple 的 `game-porting-toolkit` formula（1.1）基于 crossover-sources-22.1.1，产出 wine64 和 wine32on64；Whisky 基于 CX 22.1.1 + GPTK，2025-05-11 归档（隐含推论：免费替代品落后约四年） | 两份核查不一致（确认 / 部分正确）。本文判断：事实部分确认，推论部分已过时 | 两份核查对事实没有分歧：formula 第 28 行 `version "1.1"`，最后一次提交是 2023-11 的 “Game Porting Toolkit 1.1”；Whisky 2025-04-09 发布停更公告，2025-05-11 归档 [36][43][77][78]。分歧只在推论。第二份核查指出 `frankea/Whisky`（Wine 11.0 + DXMT）、`dappermint/winecx-gptk`（CX 26.3 hack 合入 Wine 11.15–11.17）、Sikarugir 的 Wine 10.0 引擎、Gcenx 的 WineHQ 11.18 包都说明免费生态已不再落后，第一份核查也把 frankea 作为补充背景提到 [76][27]。所以采纳“推论已过时”。已改：摘要、§2 表格与要点。 |
| 社区版本表：“老牌社区项目都停在 CX 22–24”“唯一跟进 CX 26.x 的是 dappermint 系列”，且表中没有 Whisky 的活跃分支 | 部分正确 | 原版 Whisky 已归档，但活跃分支 `frankea/Whisky` 用 Wine 11.0 + DXMT（并支持 D3DMetal），原表遗漏了它。Whisky 一系已迁到 Wine 11.x，部分还携带 CX 26.3 hack [76]。已改：§2 新增一行、改写要点。 |
| §3.5：Rosetta 相关补丁是 CW Hack 24256、23427、20186、22131（Cider 的 P0 集合） | 部分正确 | 四项都存在，但遗漏了 **CW Hack 24265**（第 2794–2852 行）：在 M3 上 Rosetta 会用 sigcontext 中错误的 MXCSR 恢复，所以把 RIP 重定向到 `__restore_mxcsr_thunk`。这一项直接影响 Cider 的 M3 开发机 [21]。已改：§3.5、P0 补丁拆分第 3 组。 |
| §3.1/§4：26.2.0 起对 32 位 bottle 显示警告；CX 26 默认创建 64 位 bottle | 部分正确 | 26.2.0 changelog 原文是 “Added additional warnings for 32-bit bottles”，说明此前已有部分警告。“默认 64 位 bottle”在 26.0.0 changelog 中没有提及，未核实 [3]。已改：§3.1（降为 [低]）、§4、未解问题。 |
| Wine 11.0（2026-01-13）完整支持新 WoW64 并删除 wine64 loader；Cider 用 `--enable-archs=i386,x86_64` 即可 | 确认（两份核查一致） | ANNOUNCE 还写明：`WINEARCH=win32` 纯 32 位前缀已弃用、新 WoW64 模式下不支持；有一项 macOS 专用的 `%gs` 切换修复；NTSync 依赖 Linux 内核模块，macOS 用不上 [70][71][72][73][74]。已改：§3.1。 |
| Gcenx/winecx（`crossover-wine` 分支）2026-09-26 访问返回 404 | 确认 | 仓库主页和分支页都是 404。剩下的 PhoenicisOrg（CX 25.1.0）和 dappermint（CX 26.3.0）镜像是第三方导入，也可能消失 [79][80]。已改：§2、风险 4，并在 P0 增加“自行归档官方 tarball”。 |
| 风险 1 / P1 / §1：D3DMetal 许可证“不能用于商业移植”，Cider 不应捆绑，应让用户自行下载 GPTK | **存疑**（两份核查分别判为部分正确 / 无法核实） | 第三方 `dbc-hbin/d3dmetal-redistributable` 引用 GPTK 4.0 beta 2 License §2A/§2C，称可以非商业再分发 D3DMetal.framework，条件是不修改、完整分发、保留 Apple 声明、限 Apple 品牌硬件 [64]。这与 Sikarugir 的“不能用于商业移植”说法 [46] 并不矛盾，但两者都没有对照 Apple 原文核实。本文判断：“让用户自行获取 GPTK”仍是风险最低的安装架构。不论采用哪种解读，都把 D3DMetal 设计成单独获取、不修改的组件。是否允许捆绑是法律问题，不在本报告范围。已改：§1、P1 后端策略、风险 1、未解问题。 |
