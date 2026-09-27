# 应用生态调研：运行库、.NET、WebView2、Office、中文常用软件、安装器，以及“CrossTie 式”配方系统

> 调研日期 2026-09-26 · 置信度说明：**[高]** = 一手来源直接核实（WineHQ Bugzilla、wine/winetricks/Proton/Bottles/Lutris 源码、Microsoft Learn、官方 release notes 或 changelog）；**[中]** = 可信二手来源，或官方页面只能通过搜索摘要间接获取（codeweavers.com 的论坛和兼容性页面对抓取返回 403/Cloudflare 验证）；**[低/推断]** = 基于上述事实的工程推断，尚未验证。
> 相关报告：`01-crossover-product.md`（CrossTie 统计与 CX27 路线）、`03-wine-upstream-proton.md`（Wine 11 与 Proton）、`06-cpu-translation-rosetta-arm64.md`（Rosetta 与 ARM64）。

---

## 摘要

- **WebView2 是 2026 年应用兼容性里最大、最不稳定的变量** [高]。
  - 上游进展：Wine bug 56378（Edge/WebView2 必须加 `--no-sandbox` 才能运行）列在 wine-11.1 ANNOUNCE 的“Bugs fixed in 11.1”中，2026-01-23 关闭 [15][74]。但**这个里程碑不对应某一次代码修改**：bug 没有关联提交（“Fixed by SHA1”为空），报告者 2026-01-19 复测后只写了一句“Marking resolved.”。复测时遇到的 `SetAdditionalForegroundBoostProcesses` 崩溃是后来才出现的回归，并非原始根因，已在 **Wine 10.5** 通过 win32u 桩修掉 [75][76]。
  - 另一个对现代 Chromium 更关键的缺口是 `KERNEL32.SetThreadpoolTimerEx`（bug 57980）：wine-11.0 到 11.16 里它只是桩，**wine-11.17 才实现**，bug 列在 11.18 的修复清单中 [54][77][78][86][87]。也就是说，**基于 Wine 11.0 的引擎还跑不了依赖此函数的新版 CEF/Chromium**。另外，沙箱问题的修复只在 Linux 上验证过。
  - 仍未解决：bug 58921（2025-11-04 报告），系统版本设为 Win8.1 及以上时，WebView2 会走 DirectComposition 路径，而 `DCompositionCreateDevice` 返回 `E_NOTIMPL`。通行的绕法是给 `msedgewebview2.exe` 单独设成 win7。Proton 11 的 `wine.inf` 默认就这样写 [25]；winetricks 在 **2026-02-21** 新增的 `webview2` verb 也这样处理 [1][3]。
  - **macOS 上还有专门的问题**：2026-09-18 有报告称，CrossOver 26.3 + Apple Silicon + WebView2 149/153 **只画出第一帧**，之后界面不再更新。报告者把原因指向 runtime 自己追加的 `--use-gl=swiftshader`（这个值已经失效；同时追加的 `--in-process-gpu` 仍是合法参数），以及 winemac.drv 在呈现子窗口上的限制 [19][20]。这只是一位用户的诊断，截至 2026-09-26 微软和 CodeWeavers 都没有回应，根因**未经独立确认** [中]。
  - 结论：Cider 必须**锁定一个验证过的 WebView2 Fixed Version**，不能跟着 Evergreen 自动更新。同时，引擎要么基于 ≥ 11.17，要么把 `SetThreadpoolTimerEx` 回移植过来。
- **CodeWeavers 已放弃 Office 2016 和 Microsoft 365** [中高]。从 CrossOver 26 开始（26.0.0 于 2026-02-10 发布 [31]），这两者不再有 bug 修复。公告大约发在 2026-01 的 26.0 发布之前，由官方论坛的两个帖子（msg=343162、343163）从搜索摘要推断，确切日期未核实。给出的原因包括：微软的持续更新反复破坏兼容、部分账号的登录窗口空白、2FA 无法支持、无法装进 Win10 bottle [32][33][79]。Microsoft 这边，Office 2016/2019 已于 2025-10-14 停止支持，Office 2021 将于 2026-10 停止支持 [35][36]。**Cider 不应承诺支持 M365**，可以把 Office 2010/2013/2016（MSI 版）列为尽力支持的对象。
- **.NET** [高]：
  - wine-mono 的定位是替代 **.NET Framework 4.8.1 及更早版本**，内含的 WinForms、WPF 分支已和上游大幅分叉 [9]。
  - 版本：Wine 11.0 自带 Mono **10.4.1**；wine-11.17、11.18 和 master 自带 **11.3.0**（2026-08-17 发布，首个 ARM64 构建，dl.winehq.org 提供 `wine-mono-11.3.0-arm64.msi`）[8][10][80]。
  - 真 .NET Framework 4.8 仍要按 winetricks 的流程装：`remove_mono` → `dotnet40` → 设 win7 → 加 `fusion=b` override → `mscoree=native` [1]。
  - 现代 .NET 8/9/10 基本只是普通的 PE 程序：.NET 10 LTS 于 2025-11-11 发布；.NET 8 和 .NET 9 都在 **2026-11-10** 停止支持 [14][81]。
  - 与中文用户直接相关的老 bug：**Wine bug 47277**（WPF 在 zh_CN 区域设置下无法启动）从 2019 年至今仍是 NEW [12]。
- **运行库的下载源在持续漂移** [高]。
  - winetricks 给 VC++ 使用 `aka.ms` 永久链接并固定 sha256，结果每次微软更新文件都会出现哈希不匹配 [4][5]。vcrun2026 的哈希在 2026-08-27 刚改过一次 [3]。
  - DirectX Jun2010 的下载地址在 2026-08-05 又换了一次，sha256 也随之变化 [3]。
  - Wine 内置的 `msvcp140` 带了版本资源，导致 vc_redist 安装器直接跳过它，winetricks 只好用 cabextract 手动替换 [6]。
  - Bottles 的依赖清单只校验 **MD5 和文件大小** [47]；CrossTie 的公开文档**没有提到下载校验或签名** [41][43]。这正是 Cider 配方系统可以明显做得更好的地方。
- **中文软件的判断**：微信、QQ（QQNT，基于 Electron）、WPS、钉钉、企业微信、腾讯会议都有 macOS 原生版，Cider 在这部分**几乎没有价值**。真正有价值的是 Windows 独占或 Windows 版功能更全的长尾软件：券商终端（通达信、同花顺的 Windows 版）、个税扣缴端这类政务客户端、行业软件、老的 GBK 编码程序。**不可行**的有三类：WeGame 及依赖 ACE 内核反作弊的游戏、网银 U 盾的内核驱动和 ActiveX 安全控件、税控盘。
- **安装器**：MSI、NSIS、Inno Setup、WiX Burn 大体可用。InstallShield 的 InstallScript 依赖进程外 COM（IDriver/ISBEW64），问题较多。ClickOnce 依赖真 .NET 4.8。**MSIX/Appx 在 Wine 中无法安装**，只能解包后运行 [55][56]。服务可以用。内核驱动只有“不接触真实硬件”的那一小类可行（Wine 11.18 仍在补 NTOSKRNL）[54]。
- **配方系统建议**：采用声明式 YAML（编译成 JSON），宿主侧不允许执行任意 shell。每个下载必须有 **sha256 + 大小 + 多个镜像**，允许同时接受多个哈希。依赖用 DAG 描述，并区分 `provides`/`conflicts`。运行时配置单独放一层（AppDefaults、DLL override、Chromium 参数、区域设置、图形后端）。feed 采用 **TUF 式的 ed25519 签名**，带过期时间和防回滚。CI 每天检查链接、检测哈希漂移、在 macOS 上做冒烟测试。同时提供 winetricks、CrossTie、Lutris 三种格式的导入器。

---

## 详细调研

### 1. 运行库依赖：来源、校验、安装顺序与 macOS 上的坑

#### 1.1 winetricks 现状（事实）[1][2][3]

- **版本**：最新正式版是 `20260125`（2026-01-26）；master 上的版本号是 `WINETRICKS_VERSION=20260125-next`。上一个正式版是 `20250102` [2]。
- **下载与校验**：
  - 完整流程是 `w_download` → `w_download_to`：下载工具依次尝试 aria2c、wget、curl、fetch；**只支持 sha256**（`w_get_shatype` 只识别 64 位十六进制）。
  - 失败后会把 URL 改写成 `https://web.archive.org/web/2000/<url>` 重试。
  - 缓存目录是 `${XDG_CACHE_HOME:-~/.cache}/winetricks/<verb>`。
  - 校验和传空字符串时跳过校验，`webview2` 就是这样处理的 [1]。
- **依赖声明**：用 `w_metadata` 声明 `conflicts=`，在 `load_*` 函数里用 `w_call` 按顺序调用其他 verb（例如 `corefonts` 依次 `w_call` 10 个字体 verb）。另有 `w_package_broken <bug> <from> <to>`，用来标记在哪些 Wine 版本范围内不可用，以及 `w_workaround_wine_bug <bug>`，用来按 bug 编号挂上绕过方法 [1]。
- **与 macOS 直接相关的代码**：
  - 如果报 `Bad CPU type in executable`，就提示用户安装 Rosetta 2（第 4632 行附近）。
  - `steam` verb 在 macOS 上提示必须加 `-allosarches -cef-force-32bit -cef-in-process-gpu -cef-disable-sandbox` 启动参数（对应 bug 49839），并调用 `nocrashdialog`。
  - 对 `WINEARCH=win64` 会发出警告 `w_package_warn_win64`（例如 dotnet40/48）[1]。
  - Wine 11 的新 WoW64 下，macOS 已经不能建纯 32 位 prefix（见报告 03），所以这条警告在 Mac 上**无法靠换 32 位 prefix 来规避**。CrossOver 26.2 也新增了“32-bit bottle 额外警告”[31]；CrossOver 27 计划完全不再运行 32-bit bottle（见报告 01）。
- **2025–2026 的关键提交**（GitHub commits API）[3]：

| 日期 | 提交 | 含义 |
|---|---|---|
| 2026-01-26 | `dotnet10,desktop10: New verb` | 指向 `builds.dotnet.microsoft.com/.../10.0.0/`，x86/x64 都固定了 sha256 |
| 2026-02-21 | `vcrun2026: new verb` | 使用 `aka.ms/vc14/vc_redist.{x86,x64}.exe`（VS 2017–2026 共用的 v14 运行库）。GitHub 提交历史显示的提交日期是 2026-02-21，02-16 可能是作者日期 [3][82] |
| 2026-02-21 | `webview2: New verb` | 作者 qwertychouskie，由 PR #2467 合入，关闭 issue #2436。使用 Evergreen bootstrapper，**跳过校验**（checksum 为空）；把 edgeupdate 服务设为手动启动（bug 53925）；在 `w_workaround_wine_bug 58921` 下给 `msedgewebview2.exe` 设 win7 [1][82][83] |
| 2026-03-31 | `d3dcompiler_47: download through Microsoft's official redistributables` | 换了下载源 |
| 2026-08-05 | `directx: update feb2010/jun2010 download URLs` | Jun2010 从 holarse 镜像切回 `download.microsoft.com`，sha256 从 `8746ee1a…` 变为 `053f76dc…`；Feb2010 改用 `web.archive.org/web/20100205000000id_/…` |
| 2026-08-05 | `vcrun2017/2019/2022: fix msvcp140_2.dll not being replaced on Wine 11+` | 与 bug 57518 相关的后续修补 |
| 2026-08-27 | `vcrun2026: update shasum` / `vcrun2019: update shasum` | `aka.ms` 背后的文件变了，固定的哈希随之失效 |

> 推断 [中]：Jun2010 同名文件出现了两个不同的 sha256，说明“一个文件名只对应一个哈希”的假设不成立（可能是微软重新签名后重发）。Cider 的配方需要**同时接受多个哈希**。

#### 1.2 各类运行库一览

| 组件 | 官方来源 / 现状 | Wine 内置替代 | winetricks 做法 | 已知问题（尤其 macOS） |
|---|---|---|---|---|
| **VC++ v14**（2015–2026）| MS 给出的永久链接是 `aka.ms/vc14/vc_redist.{x86,x64,arm64}.exe`，适用于 VS 2017–2026。**VS 2026 附带的版本只支持 Win10/11**；x64 包内同时带有 ARM64 和 x64 二进制；VS2015 版 redist 已于 **2025-10-15** 停止支持 [7] | ucrtbase、vcruntime140、msvcp140 等是 Wine 内置实现；**没有 MFC**（mfc140 需要原生）| 先 `w_override_dlls native,builtin concrt140 msvcp140 …`，再运行安装器；然后从 exe 内嵌的 cab（`a10`/`a12`/`a2`/`a4`）中用 cabextract 取出 msvcp140、msvcp140_2 手动覆盖 [1] | **bug 57518**：Wine 的 msvcp140 加了版本资源后，安装器认为已有版本更新，于是跳过安装。Wine 开发者认为这应由 winetricks 这类工具处理，把 bug 关为 INVALID [6]。`aka.ms` 背后的哈希频繁变化 [4][5] |
| VC++ 2005–2013 | 均已停止支持，MS 仍提供固定 URL（例如 2013 `12.0.40664.0` 的 `aka.ms/highdpimfc2013x86enu`）[7] | msvcr80–120 为内置实现 | 对应的 `vcrun20xx` verb | 需要 x86 和 x64 两份 |
| **DirectX Jun2010**（d3dx9/d3dx10/d3dx11、xact、xinput、d3dcompiler_43）| 原始下载在 2020 年底因 SHA-1 签名下架（PCGW 的说法 [70]，[中]）；2026-08 winetricks 又切回 MS 链接，但哈希不同 [3] | d3dx9_24–43、d3dcompiler_43/47（vkd3d-shader）、xinput1_x、xactengine3_x、x3daudio（FAudio）均有内置实现 | `helper_directx_Jun2010` 先下载，再用 `cabextract -F '*d3dx9*x86*'` 只抽出需要的 cab，最后 `w_override_dlls native d3dx9_24…43` [1] | 大多数情况**不需要原生 DLL**，只在个别 bug 上才需要。配方应按应用逐个启用，不应全局装 |
| **.NET Framework 2.0–4.8** | `ndp48-x86-x64-allos-enu.exe`（download.visualstudio.microsoft.com，已固定 sha256）[1] | wine-mono（见第 2 节）| 见第 2.2 节 | 在 64 位 prefix 下会有警告；WPF + zh_CN 问题见 bug 47277 |
| **.NET 6–10** | `builds.dotnet.microsoft.com`；winetricks 固定在 10.0.0，而官方已经到 10.0.12（2026-09-08）[1][14] | 无（Mono 不覆盖 .NET Core 系）| 静默安装 x86 和 x64 两份 | 版本老化快，配方应该从官方 release metadata 生成 [推断] |
| XNA 4.0 | `xnafx40_redist.msi`（只能从 archive.org 获取，已固定 sha256）[1] | **wine-mono 内置 FNA**：Wine 11 的 XNA4 基于 SDL3 + SDL_GPU [11] | 需要先装 dotnet40（bug 30718）| 优先使用内置 FNA |
| MSXML3/6 | MS 历史安装包 | msxml3（基于 libxml2）为内置实现 | `msxml6` 装原生版 | 个别应用需要原生版 |
| 核心字体 | winetricks 从 `github.com/pushcx/corefonts` 镜像下载，每个文件固定 sha256 [1] | Wine 自带替代字体 | 用 `w_call` 逐个安装 | macOS 自带字体可以作为 FontSubstitutes 的目标 |
| CJK 字体 | winetricks 的 `fakechinese` 会安装 Source Han Sans，并把 SimSun、Microsoft YaHei、SimHei、KaiTi、FangSong、DengXian 等映射过去（`w_register_font_replacement`）[1] | — | `cjkfonts` = fakechinese + fakejapanese + fakekorean + unifont | macOS 上可以直接映射到 PingFang SC、Songti SC 等系统字体，免去下载 [推断，中] |
| WebView2 | 见第 3 节 | 无 | Evergreen bootstrapper，不校验 | 版本敏感 |
| Java | 各厂商 MSI（Temurin 等）| 无 | 无专用 verb | 本轮未专门验证 [低] |

#### 1.3 macOS 特有的工具链问题

- winetricks 依赖 `cabextract`（找不到时会直接退出，第 835/1119 行）[1]。开发机没有 Homebrew。系统自带的 `bsdtar 3.5.3 / libarchive 3.7.4` 能读多种归档格式，但对 vc_redist 这类“把 cab 嵌在 PE 里的 Burn bundle”是否有效**尚未验证**。建议 Cider 自带 cabextract/libmspack 和 7-Zip 的解包能力 [推断]。
- winetricks 在 macOS 上用 `shasum` 计算哈希，用 perl 实现 `readlink -f` [1]。Cider 如果用 Swift/CryptoKit 原生实现下载和校验，就能完全绕开这些 shell 依赖。

### 2. .NET：wine-mono、Microsoft .NET Framework 与现代 .NET

#### 2.1 wine-mono 的版本线与能力 [8][9][10][11]

| 版本 | 日期 | 要点 |
|---|---|---|
| 10.0.0 | 2025-03-29 | PE DLL 全部标记为 Wine builtin；WPF 更多渲染走 GPU |
| 10.1.0 | 2025-06-13 | FNA 改用 SDL3，`FNA3D_FORCE_DRIVER=OpenGL/D3D11/SDLGPU`，默认 SDL_GPU+Vulkan；加入 msbuild.exe 桩 |
| 10.2.0 | 2025-08-19 | 修复 WinForms 数组封送；mono-basic 合并 |
| 10.3.0 | 2025-10-16 | WPF 加入 Line/Page Services（System.Windows.Documents 开始可用）；修复 WinForms `EnableVisualStyles` |
| 10.4.0 / **10.4.1** | 2025-12-01 / 2025-12-23 | 10.4.1 **撤回了“WPF 用 D3D9 渲染”**，因为几乎所有 WPF 程序都出现了渲染错误。**Wine 11.0 和 CrossOver 26.0 自带的就是这个版本**（`addons.c` 中 `MONO_VERSION "10.4.1"`；CX 26.0.0 changelog 写“Update to Wine Mono 10.4.1”）[10][31] |
| 11.0.0 | 2026-02-13 | 默认对反射隐藏 `Mono.Runtime`，避免应用检测到 Mono 后走错代码路径（Proton 11.0-1 自带此版本，见报告 03）|
| 11.1.0 | 2026-04-22 | runtime 文件中带符号链接（msi 安装时转换为 reparse point）；补充 VB6 兼容 API |
| 11.2.0 | 2026-06-17 | **撤回了**让 `WINE_MONO_HIDETYPES` 默认开启的改动，原因是它引发的问题超出预期 |
| **11.3.0** | 2026-08-17 | **首个 ARM64 构建**（Mono 移植到 ARM64 Windows，部分组件是 arm64ec）；dl.winehq.org 上有 `-arm64.msi` 和 `-arm64.tar.xz`，11.2.1 及更早的版本只有 x86 包。wine-11.17、wine-11.18 和 master 的 `MONO_VERSION` 都是 11.3.0，Gecko 仍是 2.47.4 [8][10][80][93] |
| 11.2.1 | 2026-09-04 | 只供 Proton 使用的修复版（发布时间晚于 11.3.0，但官方要求 Wine 用户使用 11.3.0）[8] |

- README 的定位是“intended as a replacement for the .NET Framework (4.8.1 and earlier)”。WinForms 和 WPF 分支“have diverged significantly”，不再从上游同步 [9]。
- 结论 [推断，中]：只用 WinForms 的中小型程序，先试 wine-mono（安装快，占用小，对 8GB 内存的机器友好）；WPF、WCF、复杂的 System.Web 或 ClickOnce 程序，默认直接装微软的 .NET Framework 4.8。**配方里要明确写出选哪一个**，不要让用户去试错。

#### 2.2 安装真 .NET Framework 4.8 的标准流程（取自 winetricks）[1]

1. `remove_mono internal`：卸载 "Wine Mono Windows Support/Runtime"，删除 `NDP\v3.5`、`NDP\v4` 注册表键，删除带 `WINE_MONO_OVERRIDES` 标记的 mscoree.dll。
2. `dotnet40`：临时把系统版本设为 **winxp**，用 `WINEDLLOVERRIDES=fusion=b` 运行 `dotNetFx40_Full_x86_x64.exe /q /c:"install.exe /q"`，然后设 `mscoree=native`，写入 `NDP\v4\Full` 的 Install/Version 以及 `OnlyUseLatestCLR=1`（Wow6432Node 下也要写）。
3. 设为 **win7**，用 `fusion=b` 运行 `ndp48-x86-x64-allos-enu.exe /sfxlang:1027 /q /norestart`，最后设 `mscoree=native`。
4. 区域设置警告：zh_CN、zh_TW、ru 下 WPF 可能死循环，临时办法是 `LC_ALL=C`（bug 47277）。

Bottles 的 `dotnet48.yml` 基本照搬这套步骤：uninstall Wine Mono → set_windows win7 → install_exe（`WINEDLLOVERRIDES: fusion=b`）→ set_windows win10 → override mscoree native。它用 `file_checksum`（MD5）和 `file_size` 校验 [47]。

**Wine bug 47277**（2019-05-27 报告，针对 Wine 4.9，状态 NEW，最后一条评论在 2022-10-28）[12]：
- 触发条件：区域设置为 zh_CN，且系统版本不低于 Win7。
- 过程：zh-CN 的 `LOCALE_SPARENT` 返回 "zh-Hans"，.NET 据此拼出 zh-CHS 资源程序集路径，然后反复输出 `parse_url failed to parse L"<assemblyname>.resources"`，陷入死循环，WPF 程序起不来。
- 现状：当前 Wine master 的 `tools/make_unicode` 中，zh-CN 仍然定义为 `sparent => "zh-Hans"`（约第 1583 行），触发 bug 的数据没有变 [12][84]。
- bug 中给出的临时办法是 `LC_ALL=C`。

**这对中文用户是 P0 级问题**。下面两种修法都是推断 [推断，中]：
- 在 Cider 自己的 Wine 分支里修正这个返回值。但修复点是否真在 `LOCALE_SPARENT`，还需要对照真实 Windows 的返回值和 .NET 的资源查找逻辑来确认。
- 在配方里给 WPF 应用单独设置 `LC_ALL`。这会影响 GBK 代码页，需要权衡。

**ClickOnce**（Gcenx/macOS_Wine_builds#125，2025-03-13）[13]：在 macOS 上用 Wine 10.0 加 winetricks dotnet48，启动 ClickOnce 程序时报 `class {20fd4e26-8e0f-4f73-a0e0-f27b8c57be6f} not registered`；同样的程序在 CrossOver 25 和 Whisky 下能跑。报告者注意到 **CrossOver 是按 2.0 → 4.0 → 4.8 依次安装**的，但照着复刻也没有成功，根因至今不明。→ Cider 的 dotnet48 配方需要一个 ClickOnce 的回归测试用例。

#### 2.3 现代 .NET（Core 系）[14][1]

- 支持周期：**.NET 10 LTS**，2025-11-11 发布，2028-11-14 停止支持，最新补丁 10.0.12（2026-09-08）；**.NET 8（LTS）和 .NET 9（STS）都在 2026-11-10 停止支持**；.NET 11 RC1 于 2026-09-08 发布（正式版按惯例在 11 月）[14][81][92]。
- 在 Wine 中：framework-dependent 程序需要装 `dotnet{8,9,10}` / `dotnetdesktop{8,9,10}`（desktop 版包含 WinForms/WPF）；self-contained 程序不依赖任何运行库。WPF 走 D3D9（wpfgfx），在 macOS 上会经过 wined3d/GL 或其他 D3D9 实现。出现渲染问题时，可以设 `HKCU\Software\Microsoft\Avalon.Graphics\DisableHWAcceleration=1` 退回软件渲染（WPF 的通用开关，[中]）。
- winetricks 的 `dotnet10` 固定在 10.0.0，已经落后 12 个补丁 [1]。→ Cider 应该在 CI 里从微软的 release metadata 自动生成 runtime 配方，把哈希和最新补丁号一起更新 [推断]。

### 3. WebView2、CEF、Electron 与 Chromium 沙箱

#### 3.1 上游 Wine 的状态（事实）

| Bug | 日期 / 版本 | 内容 | 状态 |
|---|---|---|---|
| **56378** | 2024-02-28 报告（针对 Wine 9.3）| "Microsoft Edge and Edge-based WebView2 do not function without --no-sandbox option"。**这个 bug 没有关联提交**（“Fixed by SHA1”为空）。2025-03-18 复测时，程序因调用未实现的 `user32.SetAdditionalForegroundBoostProcesses` 而崩溃。这是后来出现的回归，**不是**必须加 `--no-sandbox` 的原始根因；补上桩之后，程序不加 `--no-sandbox` 也能运行。该桩已随 **Wine 10.5** 进入上游（`win32u: Add stub for NtUserSetAdditionalForegroundBoostProcesses`；user32.spec 在 10.0–10.3 中是注释掉的 stub，10.4 是 stub，10.5 起改为 stdcall 转发）[75][76][85]。新版 Chromium 还需要 `KERNEL32.SetThreadpoolTimerEx`（bug 57980）| 列在 wine-11.1 ANNOUNCE 的“Bugs fixed in 11.1”中。报告者 2026-01-19 复测后标为 resolved（原话只有“Marking resolved.”），2026-01-23 关闭 [15][74]。**只在 Linux 上验证过**。57980：wine-11.0 到 11.16 中是 stub，**wine-11.17 才实现**（转发到 `ntdll.TpSetTimerEx`），列在 **11.18** 的修复清单中（提交 8fc5b439ff6a，2026-09-18 关闭）[54][77][78][86][87] |
| **58921** | 2025-11-04，Wine 10.18 | 系统版本 ≥ 8.1 时 WebView2 调用 DirectComposition，`DCompositionCreateDevice` 返回 `E_NOTIMPL`。绕法：`HKCU\Software\Wine\AppDefaults\msedgewebview2.exe\Version=win7`。wine-staging **从 11.6 起**就带 dcomp 补丁集（见下文），2026-09-25 的评论称它只解决了一部分（layered 子窗口仍然是黑的）| UNCONFIRMED，2026-09 仍在活跃讨论 [16][88] |
| 58922 / 58923 | 2025-11 | WebView2 中鼠标指针不可见 / AltGr 无效 | 未修复 [17] |
| **60318** | 2026-09-11，Wine 11.16，runtime 153.0.4234.32 | MetaTrader 5 的内嵌 Marketplace 页面一片空白（Linux + NVIDIA）。有人报告**退回 runtime 151.0.4129.78 + 设 win7 + 使用 staging 11.17 的 dcomp 补丁**后可用 | UNCONFIRMED [18] |
| 45642 / 21232 | 历史 | Chromium 沙箱需要与 Windows 一致的 x86-64 syscall thunk，Wine 5.18 已修复 [29]。Steam 的 CEF 从 2015 年起就要靠 `-no-cef-sandbox` 绕过 [28] | 已修复 / 历史 |

- 其他仍然开着、依赖 WebView2 的应用 bug：Adobe Creative Cloud 安装器、FL Studio 安装器卡在 WebView2、Power BI Desktop、ArcGIS Pro、NinjaTrader 8、基于 Tauri 的应用崩溃（60225）[17]。winetricks issue #2226（2024-05-21）指出，Adobe CC 安装器在检测到 WebView2 时会优先用它，而不是 mshtml [71]。
- **DirectComposition** [高]：
  - wine-staging 的补丁集 `patches/dcomp-DCompositionCreateDevice2` 从 tag **v11.6** 开始出现，v11.5 中还没有。作者是 CodeWeavers 的 Zhiyi Zhang（`zzhang@codeweavers.com`），2026-03 提交，共 65 个补丁（补丁头为 `[PATCH 03/65]`），用于修复 bug 54968 和 58315 [27][88][89][90]。
  - 到 v11.18 它仍留在 staging 中，目录条目略增到 68 个 [88]。也就是说，这批补丁**从 11.6 起就一直在 staging 里**，并非 11.15 之后才出现。
  - 补丁集的 `definition` 文件写明，完整实现“would require a dwm.exe and probably some graphics driver integrations”[89]。此前这一点只有 Phoronix 的搜索摘要作依据，现已用 staging 源码直接核实。
- **Proton 的做法** [高]：
  - `proton_11.0` 分支的 `loader/wine.inf.in` 第 1522 行写着 `HKCU,Software\Wine\AppDefaults\msedgewebview2.exe,"Version",,"win7"` [25]；上游 Wine master 没有这一行 [91]。
  - `dlls/kernelbase/process.c` 的 `hack_append_command_line()` 是一张按 exe 名或 SteamGameId 匹配的表，会往 Chromium/CEF/NW.js 程序的命令行追加 `--disable_direct_composition=1`、`--use-angle=d3d9|gl`、`--use-gl=swiftshader|desktop`、`--in-process-gpu` 等参数 [26]。这张表**只能追加，不能删除或替换参数**。**本质上这就是写死在代码里的“运行时配方”**，Cider 应该把它改成数据驱动。

#### 3.2 macOS 上的专有问题（2026-09，事实 + 推断）

- **WebView2Feedback #5720**（2026-09-18）[19]：
  - 环境：CrossOver 26.3（基于 Wine 11.0）、macOS 26.7、M1 Pro，WebView2 153.0.4234.46（Evergreen）和 149.0.4022.98。
  - 现象：只渲染第一帧；日志中有 `Requested GL implementation (gl=none,angle=none) not found`。
  - 报告者的分析：runtime 在 Wine 下会自己追加 `--in-process-gpu --use-gl=swiftshader`，而 "swiftshader is no longer a valid `--use-gl` value"；Wine 又没有 `D3D_DRIVER_TYPE_WARP`。
  - 尝试过的 `--use-angle=vulkan`、`--disable-gpu`、`--enable-unsafe-swiftshader` 都无效，因为 runtime 追加的参数排在后面，会覆盖宿主程序的设置。
  - 核查补充 [中]：被认定无效的只有 `swiftshader` 这个值，`--in-process-gpu` 仍是合法参数。报告者建议改用 `--use-gl=angle --use-angle=vulkan`。这只是单个用户的诊断，截至 2026-09-26 微软和 CodeWeavers 都没有回应，根因未经独立确认 [19][20]。
- **iagd #304**（同一天，同样的环境）[20]：除了“冻结在第一帧”，还有 Retina 模式下内容被裁掉**正好一半**（innerWidth=526 对应 1052pt 的控件）。报告者认为是 winemac.drv 与 WebView2 **把 DSF 除了两次**。
- WebView2 runtime 的节奏：153.0.4234.32 于 2026-09-11 发布 [21]。Edge/WebView2 从 v109 起不再支持 Win7/8.1 [24]，但在 Wine 里把 `msedgewebview2.exe` 报成 win7 仍能运行较新的 runtime（58921 讨论中测试了 150/151）[16]。
- 结论 [推断，中高]：在 macOS 上，WebView2 **既受 Chromium 版本变化的影响，也受 winemac.drv 子窗口呈现和 HiDPI 处理的影响**。Evergreen 自动更新会随时把可用的环境弄坏。

#### 3.3 可以用来控制 WebView2 的正式接口 [22][23]

- 环境变量：`WEBVIEW2_BROWSER_EXECUTABLE_FOLDER`、`WEBVIEW2_USER_DATA_FOLDER`、`WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS`、`WEBVIEW2_CHANNEL_SEARCH_KIND`、`WEBVIEW2_RELEASE_CHANNELS`。
- 策略注册表：`HKLM|HKCU\Software\Policies\Microsoft\Edge\WebView2\{BrowserExecutableFolder,AdditionalBrowserArguments,ReleaseChannels,ChannelSearchKind,UserDataFolder}`，值名为 `{AppId}`（通常是 exe 名）[23]。
- **Fixed Version**：以 cab 形式分发，不走安装器，不自动更新，由应用（或我们）通过 `BrowserExecutableFolder` 指定使用 [22]。**对 Cider 来说，这是让 WebView2 版本可复现的关键**：Cider 维护一份“已验证 runtime 版本”的清单，所有应用通过策略键统一指向它，同时避免 EdgeUpdate 服务常驻（winetricks 就因此把它设为手动启动，bug 53925/56623）[1][30]。

#### 3.4 CEF / Electron

- Steam（CEF）在 macOS 上：winetricks 要求加 `-cef-disable-sandbox -cef-in-process-gpu -cef-force-32bit -allosarches` [1]；社区的 steam-on-m1-wine 方案则用包装脚本强制 `--disable-gpu --single-process`（[中]）[73]。
- Electron/CEF 应用普遍可以用 `--no-sandbox`、`--in-process-gpu`、`--disable-gpu`、`--use-angle=…` 这组参数调整。Cider 应该把“按 exe 追加 Chromium 参数”做成配方的一类一等字段，通过 Wine 侧的数据驱动 hook 注入（机制同 Proton），而不是要求用户去改快捷方式。
- QQNT 是 Electron 应用，三端架构统一 [62]；它有原生 Mac 版，不需要经过 Wine。

### 4. Microsoft Office 与常用生产力软件

#### 4.1 Office（事实 + 推断）

| 版本 | CrossOver 状态 | Microsoft 生命周期 | Cider 建议 |
|---|---|---|---|
| 2010 / 2013 | 历史评分较高（兼容性中心，[中]）[38] | 早已停止支持 | 尽力支持（MSI 安装、32 位组件走 WoW64）|
| **2016** | **CodeWeavers 从 CrossOver 26 起停止支持，不再修 bug**；页面标题为 "Microsoft Office 2016 Is Not Supported in CrossOver" [32][33] | 2025-10-14 停止支持 [36] | 批量授权的 MSI 版尽力支持；C2R 版低优先级 |
| 2019 / 2021 / 2024 | 兼容性评级“outdated / 运行不佳”[中] | 2019 已停止；**2021 于 2026-10 停止**；2024 在 Win10 上“supported with exceptions”[35] | 不承诺 |
| **Microsoft 365（Copilot 365）** | **从 CrossOver 26 起停止支持**。原因是微软更新持续破坏兼容、部分账号登录/密码窗口空白、2FA 无法支持、装不进 Win10 bottle [32] | 在 Win10 上的安全更新到 **2028-10-10** [35] | **不承诺**。可以提示用户改用 Web 版或原生 Mac 版 Office |

- CrossOver 25.1.0（2025-08-12）还专门修过 Office 365 Outlook 登录和 Office 2016 在 Linux 上的崩溃 [34]。半年后官方宣布放弃，说明**维护成本极高**，Cider 不应在这里投入核心资源。
- 登录链路 [中]：Windows 11 上 Entra ID/WAM 的登录已经可以由 WebView2 承载（KB5072033 及之后）[37]。M365 的登录越来越依赖嵌入式浏览器，这把 Office 的可用性和第 3 节的 WebView2 问题绑在了一起 [推断]。
- 社区现状 [中]：基于 Wine 的方案一般“限于 Office 2016 及更早版本，只有 Word/Excel/PowerPoint 可用”[38]。Visio 和 Project 本轮没有拿到一手数据（搜索预算用尽），按 C2R 的同类问题推断为低优先级 [低]。

#### 4.2 其他常用软件

- **Quicken**：CrossOver 26.1.0（2026-04-09）修复了 "distorted Add account window in Quicken"，说明仍在官方维护列表中 [31]。
- **QuickBooks**：兼容性中心提示评级来自数个版本之前，已不准确 [40][中]。
- **MetaTrader 4/5**：MetaQuotes 官方的 macOS 安装器会**自动下载 Wine、配置 prefix 并安装 MT5**，需要 Mono/Gecko，建议 Wine ≥ 8.0.1（2023-11-09 新闻）[39]。也就是说，厂商自己已经在做 Wine 封装。Cider 的价值在于更好的图形表现和 WebView2（Marketplace 依赖 WebView2，bug 60318）[18]。
- **Notepad++**：CrossTie 官方示例中用 `InstalledRegistryGlob` 做安装检测 [44]；winetricks 有 `npp` verb（2026-01-29 更新到 8.9.1）[3]。
- **CAD/工程软件** [推断，低]：瓶颈通常在 OpenGL/D3D 专业特性、.NET/WPF 界面、FlexLM 或 HASP/Sentinel **加密狗驱动**（内核驱动，不可行）和联网授权。建议按“图形 API + 授权方式”先分类，再决定是否写配方。

### 5. 中文市场 Windows 软件的可行性

| 软件 | macOS 原生版 | Wine 路线可行性 | 说明 |
|---|---|---|---|
| 微信 / QQ(QQNT) / WPS / 钉钉 / 企业微信 / 腾讯会议 | **有** | 没有必要 | QQNT 基于 Electron，三端同步 [62]；微信 4.0 甚至已有 Linux 原生版 [63]。deepin-wine 仍打包了 WeChat 4.0、企业微信、Foxmail、阿里旺旺等 [64]，说明这些在 Wine 下**能跑**，但对 Mac 用户意义不大 |
| **通达信（Windows 版）** | 有 Mac 版，但功能较少 [60] | **高** | CrossOver 中文站有实测教程 [59]；需要 GBK 代码页、中文字体映射、行情插件 |
| 同花顺 / 大智慧等券商终端 | 部分有 | 中高 [中] | 常见依赖：vcrun、IE/mshtml 内嵌页、CEF |
| 个税扣缴端（自然人电子税务局）等政务客户端 | **无**（官方只支持 Windows）[66] | 中 [推断] | 常见依赖：.NET/WinForms、证书、打印；有网页版可以替代 |
| **WeGame 及 ACE 反作弊游戏** | 无 | **不可行** | CrossOver 中文站说明不支持 WeGame [61]；ACE 需要加载内核驱动，并检测 DLL 的微软签名 [65] |
| **网银 U 盾 / 企业网银** | 部分银行有 Mac 支持 | **基本不可行** | 依赖 ActiveX 安全控件、驱动和浏览器集成。Wine 的 winscard 在 macOS 上链接 `-framework PCSC`，理论上能访问 CCID 智能卡 [52][53]，但整条链路太长，不值得投入 |
| 税控盘 / 金税盘 | 无 | 不可行 | 硬件驱动 |
| 老的 GBK（ANSI）软件 | — | 高 | 关键是 bottle 的区域设置（CP936）和字体映射 |

**中文体验的共性要求**（事实 + 推断）：
- zh_CN 区域设置 / CP936；
- CJK 字体映射：可以复用 winetricks 的 fakechinese 映射表，但把目标换成 macOS 自带的 PingFang SC/Songti SC [1][67]；
- 输入法（winemac.drv 的 IME 通路，本轮未深入验证）；
- WPF + zh_CN（bug 47277）[12]；
- 国内网络环境下的下载镜像：MS 的 CDN 在国内通常可用，GitHub/archive.org 不稳定 [推断]。

### 6. 安装器类型

| 类型 | 静默参数 / 解包方式 | Wine 下的状态 | 配方需要处理的点 |
|---|---|---|---|
| **MSI** | `msiexec /i x.msi /qn`；`/a` 做管理员解包 | Wine 自带 msi.dll，11.18 周期里修了大量健壮性问题 [54] | 托管代码的 custom action（WiX DTF）需要 .NET；64 位 custom action |
| **WiX Burn**（vc_redist 本身就是）| `/quiet /norestart /log` | 大体可用 | 托管 BA（WPF 界面）依赖 .NET；2016 年 Kinect Studio 有过失败报告 [中] |
| **InstallShield**（Basic MSI / InstallScript）| `/s /v"/qn"`；InstallScript 需要 `/r` 录制 `setup.iss` 再 `/s /f1` [中] | IDriver/ISBEW64 通过进程外 COM 工作；bug 36697 中 ISBEW64 在 OLE 拆除阶段偶发崩溃，但无害 [58] | 超时与挂起检测；CrossTie 为此专门有 `InstallerEnvironment`（例如 `WINE_WAIT_CHILD_PIPE_IGNORE`）[44] |
| **NSIS** | `/S /D=C:\path`（`/D` 必须放在最后）| 普遍可用 | 可以用 7z 直接解包 |
| **Inno Setup** | `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR=` | 普遍可用；Inno 官方为 Wine 的 RichEdit 加过绕行代码 [中] | innoextract 可以离线解包（Lutris 支持 `gog(innoextract)` 格式）[45] |
| **ClickOnce** | `.application` 由 dfsvc 处理 | 需要真 .NET 4.8；macOS 上有 CLSID 未注册的问题 [13] | 回归测试 |
| **MSIX/Appx** | — | **无法安装**（Wine 没有 AppX 部署栈），只能用 7z 解包后运行 exe [55][56][57]；11.18 才实现 `PackageFullNameFromId`（bug 59278）[54] | Cider 可以自己写一个“解包 + 注册快捷方式”的导入器（没有 WinRT 激活）|
| **Squirrel / Velopack**（Electron）| `Update.exe` | 未验证 [低] | 自更新会改写目录，需要纳入快照 |
| **服务型安装器** | — | services.exe 支持服务；`wineboot --shutdown` 不能正确结束服务（bug 56623）[30] | 常驻服务（EdgeUpdate 等）按需改为手动启动 |
| **驱动型安装器** | — | Wine 11.18 “More NTOSKRNL support for kernel drivers”[54]；有 wineusb.sys（基于 libusb）[52] | 只考虑纯软件驱动或虚拟设备；硬件、反作弊、加密狗类判为不可行 |

### 7. 现有配方和依赖系统的对比

| 系统 | 格式 | 校验 | 依赖 / 条件 | 可执行能力 | 可借鉴 / 教训 |
|---|---|---|---|---|---|
| **winetricks** | POSIX shell（约 2 万行）| sha256；可以传空跳过 | `w_call` 顺序调用；`conflicts`；`w_package_broken` 按 Wine 版本；`w_workaround_wine_bug` | 任意 shell | 知识库最丰富；但 `aka.ms` 链接导致哈希反复漂移 [1][4][5] |
| **Lutris** | YAML（`files` + `installer` 任务）| 可选 `checksum: type:hash` [46] | `requires` / `extends`（DLC）| 有 `execute` 任务，可运行任意命令 | wine 任务：`create_prefix`、`wineexec`、`winetricks`、`set_regedit`、`set_regedit_file`、`delete_registry_key`、`winekill`；`N/A:` 表示请用户提供文件 [45] |
| **Bottles dependencies** | YAML（`index.yml` + `Essentials/*.yml`，仓库最近更新 2026-09-20）| **MD5 + file_size** | `Dependencies:` 列表；步骤带 `for: [win64]` | 约 20 种声明式 action：`install_exe/msi`、`uninstall`、`cab_extract`、`get_from_cab`、`archive_extract`、`install_fonts`、`copy_dll`、`register_dll`、`override_dll`、`set_register_key`、`register_font`、`replace_font`、`set_windows`、`use_windows`、`delete_dlls` [47][48] | **是最接近 Cider 目标的格式**；vcredist2022 的 URL 用了带 SHA256 路径的不可变地址 `download.visualstudio.microsoft.com/download/pr/<guid>/<SHA256>/…` [47] |
| **CrossTie（C4P）** | XML：`<c4p><applications><app appid=…>`，分 Pre/Install/Post 三段 | **文档中没有提到下载哈希或签名** [43] | `predependency`/`postdependency`（appid DAG）；`<useif>` 支持 `equal/lt/le/ge/gt/match/not/and/or`，属性包括 appid、locale、bottletemplate、sourcetype、platform、cxversion、opengl.* 等 [42] | 声明式字段：Installer Globs、Installer Environment、Installer DLL Overrides、Installer Windows Version、PreRmFakeDlls、Pre-Install Registry、Files to Copy、Post Register DLLs、文件关联、CD Profile 等 [41] | 应用/组件/虚拟三种类型；以安装检测（注册表或文件 glob）判断幂等；缺点是 XML 冗长、没有完整性保护 |
| **umu-protonfixes** | 按商店分目录（`gamefixes-steam/`、`gamefixes-gog/` 等）的 Python 文件，另有 `umu-database.csv` [49] | — | 按游戏 ID 匹配 | 任意 Python | 证明“运行时修正”和“安装”应该分开 |
| **Proton 代码内表** | `wine.inf` 的 AppDefaults 和 kernelbase 的命令行 hack 表 [25][26] | — | 按 exe 名或 SteamGameId | 编译进二进制 | 应该改成数据驱动 |

---

## 对 Cider 的启示与建议（按优先级）

### P0：配方系统的骨架（在第一个可用版本前完成）

**1. 两层数据模型：安装配方（Recipe）和运行时档案（Profile）分开。**
- Recipe 描述“怎么装”：来源、依赖、步骤、检测。
- Profile 描述“怎么跑”：exe 匹配、环境变量、DLL override、AppDefaults 下的 winver、图形后端、Chromium 参数、区域设置。
- Profile 可以脱离 Recipe 单独下发和热更新，思路与 CrossOver 25 的“逐游戏自动设置数据库”和 Proton hack 表相同，只是做成开放的数据。

**2. Recipe v1 的 schema 草案。** 用 YAML 编写，CI 编译成规范化 JSON 后签名：

```yaml
schema: cider.recipe/v1
id: cn.com.tdx.tdxw              # 反向域名，永久不变、不复用（参照 CrossTie appid 规则）
kind: app                        # app | component | virtual
revision: 7                      # 单调递增，客户端据此防回滚
name: { en: "Tongdaxin (Windows)", zh-Hans: "通达信（Windows 版）" }
requires:
  cider: ">=0.3"
  engine: { wine: ">=11.0", features: [wow64] }
  host:   { macos: ">=14.0" }
when: 'host.cpu == "arm64" && !host.rosetta'   # 条件表达式（CrossTie UseIf 的安全替代）
  # → 不满足时给出指引，而不是静默失败
bottle:
  template: win10_64
  locale: zh_CN.UTF-8            # 决定 ACP=936
  share_group: null              # 或与其他应用共用一个 bottle
dependencies:
  - runtime.vcrun.v14            # 依赖 DAG；组件用 provides/conflicts 声明
  - font.cjk.macos-map           # SimSun/YaHei → PingFang SC/Songti SC
sources:
  installer:
    urls:
      - https://vendor.example/tdx_setup.exe
      - cider-mirror://sha256/…  # 可选镜像
    sha256: ["<hash-A>", "<hash-B>"]   # 允许多个
    size: 123456789
    on_missing: ask_user         # 等价于 Lutris 的 "N/A:"
steps:
  - run_installer: { file: installer, kind: auto, silent: true, timeout: 1800 }
    # kind: auto|msi|inno|nsis|installshield|burn|exe
  - registry: { set: [ { key: 'HKCU\Software\…', name: X, type: dword, value: 1 } ] }
detect:
  any: [ { file: 'C:\new_tdx\TdxW.exe' }, { uninstall_key: '…' } ]
profiles:
  - match: { exe: TdxW.exe }
    env: { }
    dll_overrides: { }
    winver: win10
    graphics: { d3d9: wined3d }
    chromium_args: [ ]
tests:
  smoke: { launch: TdxW.exe, expect_window: "通达信", within_s: 60 }
```

- **声明式 action 白名单**：大体对齐 Bottles，另加 `webview2_fixed`、`clickonce`、`msix_unpack`、`app_winver`、`chromium_args`。**宿主机（macOS）侧不允许执行任意 shell**。确实需要脚本的，只能在 bottle 内以 Windows 进程的形式运行（`.cmd` 或 PowerShell Core），或者写成 Cider 内置的 Swift action。
- 依赖解析：拓扑排序，支持 `provides`（例如 `vcrun.v14` 同时提供 2015–2026 这一整族）、`conflicts`、`replaces`；遇到循环直接报错。组件幂等性靠 `detect` 判断。

**3. 下载层：内容寻址缓存，多源下载，检测哈希漂移。**
- 缓存路径：`~/Library/Caches/Cider/cas/sha256/<aa>/<hash>`。多个应用共用同一份文件，天然去重。
- 下载源优先级：带哈希路径的不可变 URL（例如 `download.visualstudio.microsoft.com/download/pr/<guid>/<SHA256>/…`，Bottles 已在用 [47]）> 厂商 URL > Cider 镜像 > archive.org 的 `id_` 原始地址（winetricks 的做法 [3]）。
- `aka.ms` 这类会变动的永久链接只在“刷新机器人”里使用，不直接写进配方。机器人每天抓取一次，发现新哈希后，先在 CI 里完成冒烟测试，再自动提 PR 追加哈希（解决 [4][5] 这类问题）。
- 校验失败时**不允许**像 winetricks 那样“忽略并继续”，只能通过开发者开关绕过。

**4. 核心组件第一批（每个都要有 macOS 冒烟测试）：**
- `runtime.vcrun.v14`：x86 和 x64 两份。安装前先把 builtin 的 msvcp140、msvcp140_2 替换掉，避免 bug 57518 [6]，同时保留 `native,builtin` override。
- `runtime.dotnetfx48`：采用 CrossOver 式的 2.0 → 4.0 → 4.8 顺序，加上 winetricks 的 winver/fusion/mscoree 细节，并**必须通过 ClickOnce 用例** [1][13]；ngen 队列放到后台低优先级执行（8GB 机器）。
- `runtime.dotnet{8,10}` 和 `runtime.dotnetdesktop{8,10}`：从微软 release metadata 自动生成。
- `runtime.directx.jun2010.*`：拆成 d3dx9、d3dx11、xact、xinput 等粒度，**默认不装**，只在配方里按需启用。
- `font.core`、`font.cjk.macos-map`：优先映射到 macOS 系统字体，不下载。
- `runtime.webview2.fixed`：见第 5 条。

**5. WebView2 的专项策略（这是应用兼容性上最大的单点风险）：**
- 维护一个 **“Cider WebView2 通道”**：固定使用经过验证的 Fixed Version cab（例如先以 151.x 为候选，bug 60318 的讨论中这个版本可用 [18]；是否在 macOS 上可用需要实测）。通过 `HKLM\Software\Policies\Microsoft\Edge\WebView2\BrowserExecutableFolder` 让所有应用指向它 [23]，**禁止 EdgeUpdate 常驻**。
- **引擎基线**（根据核查修订）：56378 没有对应的单一修复提交，“11.1”这个里程碑并不代表代码在 11.1 才改。它依赖的 `SetAdditionalForegroundBoostProcesses` 桩在 Wine 10.5 就已进入上游，基于 11.0 的引擎本身就有 [75][76]。真正卡住新版 Chromium/CEF 的是 `SetThreadpoolTimerEx`：它在 11.0–11.16 中只是 stub，wine-11.17 才实现 [77][78][87]。因此，**如果 Cider 的 Wine 基于 11.0（例如对齐 CrossOver 26），必须回移植 wine-11.17 中的 `SetThreadpoolTimerEx`（转发到 `ntdll.TpSetTimerEx`）**，否则就要直接以 ≥ 11.17 为基线。另外，沙箱修复只在 Linux 上验证过，macOS 上需要单独测试。
- 默认 `AppDefaults\msedgewebview2.exe\Version=win7`（与 Proton 一致）[25]。wine-staging 从 11.6 起就带有 65 个 dcomp 补丁，但 58921 中 2026-09 的反馈显示它们只部分有效 [16][88][89]。所以暂不把这批补丁作为默认方案，也不放开 Win10 版本号，只列为跟踪项，等 layered 子窗口问题解决后再评估。
- 在 Wine 侧实现一个**数据驱动的命令行改写 hook**，参照 Proton 的 `hack_append_command_line`，但比它多一个能力：能**删除或替换** runtime 自己追加的参数，例如把 `--use-gl=swiftshader` 换成 `--use-gl=angle --use-angle=…`。Proton 的表只能追加参数，做不到这一点 [26]。按 #5720 报告者的观察，从宿主侧追加的参数会被 runtime 追加的参数覆盖 [19]。这是单个用户的诊断，尚未独立确认，所以 hook 的效果需要在 macOS 上实测验证。
- 把 winemac.drv 子窗口呈现、Retina DSF 被除两次（#304 [20]）列为 Cider 自研 Wine 补丁的 P0 调查项。
- 建立 WebView2 回归套件，覆盖 MT5 Marketplace、Adobe CC 安装器、一个 Tauri 应用、一个 WinForms+WebView2 样例。每次 Microsoft 发布新 runtime（大约每月一次 [21]）都跑一遍，通过之后才更新通道。

### P1：可信分发与生态导入

**6. feed 签名。** 参照 TUF 的 root/targets/snapshot/timestamp 四个角色 [50]，第一阶段可以简化：
- 签名：CI 生成 `index.json`，每条配方带 sha256；用 **ed25519**（与 Sparkle 的 EdDSA 同类 [51]）离线签名。
- 抗冻结和回滚：`timestamp.json` 设短过期（例如 7 天），客户端拒绝比本地更旧的 `revision`。
- 根密钥：公钥内置在 App 里（Sparkle 用 `SUPublicEDKey` 的思路），支持轮换。
- 第三方源：可以另加签名源（类似 Homebrew tap），但 UI 上要明确标出信任级别。

**7. CI 与数据闭环。**
- 每天做一次链接检查和哈希漂移检测。
- 每周在 macOS arm64 机器上做一次全量冒烟测试：新建 bottle → 装依赖 → 装应用 → 启动 → 等窗口出现 → 截图存档。GitHub 托管的 macOS 运行器是否带 Rosetta 需要确认。
- 用户可以选择上报“成功/失败 + 配方 revision + 引擎版本”，形成兼容性评级（对标 CrossOver 兼容性中心，但数据开放）。

**8. 导入器（降低冷启动成本）：**
- `cider tricks <verb>`：兼容 winetricks 的 verb 名，内部映射到 Cider 组件；没有对应组件时，回退为在隔离环境中调用上游 winetricks。
- CrossTie `.tie` 导入器：把 c4p XML 尽量转换成 Recipe，把 UseIf 转成 `when` 表达式。
- Lutris YAML 导入器：主要面向游戏。

**9. APFS 快照和模板 bottle [推断，高价值]。**
- 每个安装步骤之前用 `clonefile(2)` 给 bottle 打一个写时复制快照，失败就秒级回滚。
- 预先做好“已装 .NET 4.8”“已装 vcrun”这类**模板 bottle**，按应用克隆，避免每次都在 Rosetta 下重跑微软安装器（对 8GB 内存的开发机和用户都明显省时间、省空间）。

### P2：中文和长尾

**10. 中文包：**
- 默认中文 bottle 模板：zh_CN / CP936、字体映射到 PingFang/Songti、IME 自检。
- 修复 bug 47277：在 Cider 的 Wine 分支里修正 `LOCALE_SPARENT`。
- 首批配方：通达信 Windows 版、个税扣缴端、若干券商终端、常见 GBK 老软件。
- 在应用库中**明确标出“不可行”**：WeGame/ACE、网银 U 盾、税控盘。用户可以直接看到结论，同时避免浪费支持资源。

**11. MSIX 解包导入器（实验）**：解析 `AppxManifest.xml`，解包 VFS 目录，创建快捷方式。不做 WinRT 激活。

**12. Office**：只提供 2010/2013/2016（MSI）的尽力而为配方，并在 UI 中声明不支持 M365。不建议投入核心人力。

---

## 风险

1. **WebView2 由微软按月推进，Cider 无法控制**：Chromium 移除 swiftshader 的 `--use-gl` 值这类变化会让已经能用的应用突然坏掉 [19]。即使锁定 Fixed Version，也会面临安全补丁滞后和应用要求更高 WebView2 API 版本的矛盾。
2. **上游下载链接漂移和下架**：`aka.ms` 背后的哈希变化、DirectX 包反复换源 [3][4][5]。如果不做 CAS 缓存和自动刷新机器人，用户就会遇到“装不上”。
3. **Rosetta 2 的生命周期**：winetricks 已经把“没装 Rosetta”列为一种典型失败 [1]。据报道，Apple 计划从 macOS 28 起大幅收缩 Rosetta（[中]，详见报告 06 [73]）。所有经由 x86 微软安装器的步骤（dotnet48 等）都依赖 Rosetta 或 FEX 类转译，性能和可用性都存在风险。
4. **维护成本**：CodeWeavers 放弃 Office 2016 和 M365 的决定表明，大型商业软件的兼容维护代价极高 [32]；中文长尾软件更新频繁，配方会很快过时。
5. **配方供应链安全**：开放社区 PR 以后，恶意配方可以通过“下载地址 + 注册表”植入木马。需要签名、审查、action 白名单，并且宿主侧不执行 shell。
6. **反作弊和驱动类需求的期望管理**：用户会把“CrossOver 替代品”理解为“什么都能跑”，WeGame、网银这类不可行项如果不提前说明，会带来大量负面反馈。
7. **数据源可访问性**：codeweavers.com 的论坛和兼容性页面对自动抓取返回 403/Cloudflare，本报告的 Office 相关结论部分只能依据搜索摘要。
8. **开发机资源**：8GB 内存的 M3 同时跑 Rosetta 下的 .NET 安装、ngen 和大型安装器（Office、Adobe）会很吃紧。CI 最好放到更大的机器上。

## 未解问题

1. Chromium/WebView2 的沙箱在 **macOS + Rosetta（或 FEX）** 下能否正常开启？56378 的修复只在 Linux 上验证过 [15]。这个 bug 也没有对应的单一提交，实际依赖 10.5 的 win32u 桩和 11.17 的 `SetThreadpoolTimerEx` [75][78]。
2. #5720 和 #304 在 macOS 上的根因能否拆开定位：runtime 追加的 swiftshader 参数、缺少 `D3D_DRIVER_TYPE_WARP`、winemac.drv 子窗口呈现、DXMT/D3DMetal 与 ANGLE D3D11 的配合，分别占多大比重？哪个 Fixed Version 在 CrossOver 或上游 Wine 11 的 macOS 上可用，需要实测。
3. CrossOver 内部怎样处理 WebView2 和 .NET 4.8（例如 ClickOnce 能在 CX25 下工作的原因）？CrossTie 的下载是否有未公开的校验机制？
4. CodeWeavers 宣布放弃 Office 的帖子的**确切日期**和完整原文（页面 403，只拿到搜索摘要）。
5. WinUI 3 / Windows App SDK 应用（Build 2026 后微软在大力推广）在 Wine 11.x 下的状态：本轮没有找到一手资料，初步判断是不可用。
6. macOS 自带的 libarchive 能否直接处理 vc_redist 内嵌的 cab？如果能，就可以少带一个 GPL 工具。
7. 中文输入法（候选框定位、全角半角）在 winemac.drv 下对各类控件（Win32 Edit、WPF、CEF）的实际表现。
8. GitHub 托管的 macOS arm64 运行器是否带 Rosetta，能否跑 Wine 的冒烟测试？
9. MetaQuotes 的 macOS 安装器用的是哪一版 Wine、如何构建？能否与它合作，或者被它替代？

---

## 参考来源

1. https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks — winetricks 源码（20260125-next）：`w_download` 的 sha256 与 archive.org 回退、各 verb 的 URL 和哈希、macOS 分支
2. https://github.com/Winetricks/winetricks/releases — 正式版 20260125（2026-01-26）、20250102
3. https://api.github.com/repos/Winetricks/winetricks/commits?path=src/winetricks — 2025–2026 提交记录（dotnet10、vcrun2026、webview2、directx 换源、哈希更新）
4. https://github.com/Winetricks/winetricks/issues/2407 — vcrun2022 SHA256 不匹配（2025-08）
5. https://github.com/Winetricks/winetricks/issues/2235 — vcrun2022 哈希变更（2024-06）
6. https://bugs.winehq.org/show_bug.cgi?id=57518 — msvcp140 版本资源导致 vc_redist 跳过安装
7. https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist — VC++ v14 永久链接、VS2026 版的系统要求、VS2015 版 redist 停止支持（页面更新于 2026-08-04）
8. https://github.com/wine-mono/wine-mono/releases — wine-mono 10.0.0 至 11.3.0 的发布说明与日期（经 GitHub API 读取）
9. https://github.com/madewokherd/wine-mono — README：替代 .NET Framework 4.8.1 及更早版本；WinForms/WPF 分支已分叉
10. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/dlls/appwiz.cpl/addons.c — `MONO_VERSION` 10.4.1（Wine 11.0）；master 为 11.3.0，Gecko 2.47.4
11. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — Wine 11.0 的 Mono/.NET/WinRT 小节（XNA4 使用 SDL3/SDL_GPU、WPF 文本布局、WinForms 主题）
12. https://bugs.winehq.org/show_bug.cgi?id=47277 — WPF 在 zh_CN 下无法启动（2019 年至今 NEW）
13. https://github.com/Gcenx/macOS_Wine_builds/issues/125 — macOS Wine 10.0 下 ClickOnce 的 CLSID 未注册；CrossOver 25 正常
14. https://dotnet.microsoft.com/en-us/platform/support/policy/dotnet-core — .NET 8/9/10/11 的发布与停止支持日期
15. https://bugs.winehq.org/show_bug.cgi?id=56378 — Edge/WebView2 必须 `--no-sandbox` 的问题：列入 11.1 修复清单，2026-01-23 关闭；没有关联提交，是复测后标记为 resolved 的
16. https://bugs.winehq.org/show_bug.cgi?id=58921 — WebView2 在 winver ≥ 8.1 时走 DirectComposition 失败；win7 绕法
17. https://bugs.winehq.org/buglist.cgi?quicksearch=webview2 — WebView2 相关的未关闭 bug 列表（58922/58923/60225/60318 等）
18. https://bugs.winehq.org/show_bug.cgi?id=60318 — MT5 Marketplace 中 WebView2 空白（runtime 153），退回 151 可用
19. https://github.com/MicrosoftEdge/WebView2Feedback/issues/5720 — CrossOver 26.3/macOS 上 runtime 追加 `--use-gl=swiftshader`，只画第一帧
20. https://github.com/marius00/iagd/issues/304 — CrossOver 26 + Apple Silicon 上 WebView2 冻结，Retina 下被裁一半
21. https://learn.microsoft.com/en-us/microsoft-edge/webview2/release-notes/runtime/153 — WebView2 Runtime 153.0.4234.32（2026-09-11，来自搜索结果标题）
22. https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/evergreen-vs-fixed-version — Evergreen 与 Fixed Version 的分发差异
23. https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/webview2-idl — `WEBVIEW2_*` 环境变量与 `Policies\Microsoft\Edge\WebView2` 策略键
24. https://blogs.windows.com/msedgedev/2022/12/09/microsoft-edge-and-webview2-ending-support-for-windows-7-and-windows-8-8-1/ — Edge/WebView2 结束对 Win7/8.1 的支持（仅核实标题，v109 来自相关讨论）
25. https://raw.githubusercontent.com/ValveSoftware/wine/proton_11.0/loader/wine.inf.in — Proton 默认 `AppDefaults\msedgewebview2.exe Version=win7`
26. https://raw.githubusercontent.com/ValveSoftware/wine/proton_11.0/dlls/kernelbase/process.c — `hack_append_command_line` 的 Chromium/CEF 参数表
27. https://www.phoronix.com/news/Wine-Staging-11.6 — Wine-Staging 11.6 合入 65 个 DirectComposition 补丁（Zhiyi Zhang/CodeWeavers；页面 403，依据搜索摘要；内容已由 [88]–[90] 的 staging 源码直接核实）
28. https://bugs.winehq.org/show_bug.cgi?id=39403 — Steam CEF 沙箱问题的历史，`-no-cef-sandbox`
29. https://bugs.winehq.org/show_bug.cgi?id=21232 — Chromium/反作弊的 syscall thunk 问题，Wine 5.18 修复
30. https://bugs.winehq.org/show_bug.cgi?id=53925 — EdgeUpdate 进程常驻；关联 bug 56623（`wineboot --shutdown` 不结束服务）
31. https://www.codeweavers.com/crossover/changelog — CrossOver 25.0–26.3 changelog（Wine Mono 版本、Quicken 修复、32-bit bottle 警告）
32. https://www.codeweavers.com/compatibility/crossover/forum/microsoft-office-365?msg=343162 — CodeWeavers 宣布停止支持 Office 2016/M365（页面 403，依据搜索摘要）
33. https://www.codeweavers.com/compatibility/crossover/microsoft-office-2016 — 页面标题 "Microsoft Office 2016 Is Not Supported in CrossOver"
34. https://www.phoronix.com/news/CrossOver-25.1-Released — CrossOver 25.1 修复 Office 365 Outlook 登录与 Office 2016 崩溃
35. https://support.microsoft.com/en-us/office/lifecycle/officeinstall/what-windows-end-of-support-means-for-office-and-microsoft-365 — Win10 停止支持后 Office/M365 的日期（M365 安全更新到 2028-10-10）
36. https://support.microsoft.com/en-us/office/system-requirements/end-of-support-for-office-2016-and-office-2019 — Office 2016/2019 于 2025-10-14 停止支持
37. https://techcommunity.microsoft.com/blog/windows-itpro-blog/now-generally-available-modernizing-microsoft-entra-id-auth-flows-with-webview2-/4476166 — Entra ID/WAM 登录改用 WebView2（依据搜索摘要）
38. https://gist.github.com/eylenburg/38e5da371b7fedc0662198efc66be57b — 在 Linux 上安装 Office 的社区现状（Wine 方案限于 2016 及更早，依据搜索摘要）
39. https://www.metatrader5.com/en/news/2329 — MT5 官方 macOS 安装器自带 Wine（2023-11-09）
40. https://www.codeweavers.com/compatibility/crossover/quickbooks — QuickBooks 评级已过时（依据搜索摘要）
41. https://support.codeweavers.com/crosstie-data-startpage/an-intermediate-guide-on-what-the-crosstie-editor-options-mean — CrossTie 编辑器全部字段
42. https://support.codeweavers.com/c4-data-advanced-options — `<useif>`、`<predependency>` 与条件属性
43. https://support.codeweavers.com/crosstie-data-startpage/c4-data-file-framework — c4p XML 框架（没有校验和签名的描述）
44. https://support.codeweavers.com/crosstie-data-startpage/c4-data-examples — CrossTie 示例（`InstallerEnvironment` 中的 `WINE_WAIT_CHILD_PIPE_IGNORE`、`InstalledRegistryGlob`）
45. https://raw.githubusercontent.com/lutris/lutris/master/docs/installers.rst — Lutris 安装脚本格式与 wine 任务
46. https://raw.githubusercontent.com/lutris/lutris/master/lutris/installer/installer_file.py — Lutris 的 `checksum` 格式 `type:hash`
47. https://github.com/bottlesdevs/dependencies — Bottles 依赖清单（index.yml；dotnet48/webview2/vcredist2022.yml 使用 MD5 和大小）
48. https://raw.githubusercontent.com/bottlesdevs/Bottles/main/bottles/backend/managers/dependency.py — Bottles 支持的 action 列表
49. https://github.com/Open-Wine-Components/umu-protonfixes — 按商店划分的逐游戏修正（Python）
50. https://theupdateframework.github.io/specification/latest/ — TUF 规范（root/targets/snapshot/timestamp）
51. https://sparkle-project.org/documentation/ — Sparkle 的 EdDSA（ed25519）签名与 `SUPublicEDKey`
52. https://raw.githubusercontent.com/wine-mirror/wine/master/configure.ac — pcsclite 在 darwin 上链接 `-framework PCSC`；libusb 用于 wineusb.sys
53. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winscard/unixlib.c — winscard 的 `__APPLE__` 分支
54. https://raw.githubusercontent.com/wine-mirror/wine/master/ANNOUNCE.md — Wine 11.18（NTOSKRNL 驱动支持，修复 57980/59278 等）
55. https://forum.winehq.org/viewtopic.php?t=37861 — Wine 无法安装 msix（依据搜索摘要）
56. https://github.com/publicsite/appx_msix_wine — 社区在 Wine 上运行 appx/msix 的尝试
57. https://github.com/bottlesdevs/Bottles/issues/3350 — Bottles 的 MSIX/UWP 支持请求
58. https://markmail.org/message/qessnggigo56hgzd — Wine bug 36697：ISBEW64 在 OLE 拆除阶段崩溃
59. https://www.crossoverchina.com/faq/coaz-txd.html — CrossOver 中文站：通达信安装实测
60. https://soft.zol.com.cn/1004/10049552.html — 通达信 Mac 版功能少于 Windows 版
61. https://www.crossoverchina.com/rumen/crossover-enada.html — CrossOver 中文站：WeGame 不受支持（依据搜索摘要）
62. https://36kr.com/p/2329529869966977 — QQ 基于 Electron 重构，三端架构统一
63. https://www.v2ex.com/t/1086474 — 微信 4.0 Linux 原生版上架
64. https://deepin-wine.i-m.dev/ — deepin-wine 软件包列表（WeChat 4.0、企业微信、Foxmail 等）
65. https://hu60.cn/q.php/bbs.topic.103116.html — Wine 游戏助手：WeGame 与 ACE 签名检测风险（依据搜索摘要）
66. https://www.zhihu.com/question/443618942 — 电子税务局个税客户端不支持苹果系统（依据搜索摘要）
67. https://becoder.org/macos-wine-tuning/ — 在 macOS Wine（Whisky）上调校 CJK 字体替换与区域设置
68. https://pkg.go.dev/github.com/sewnie/wine/webview2 — Go 语言的 Wine WebView2 管理库（2026-06-20），指出 Proton 默认带 winver override
69. https://docs.getwhisky.app/maintenance-notice — Whisky 于 2025-04-09 归档（维护公告）
70. https://community.pcgamingwiki.com/files/file/2106-legacy-directx-sdk-redist-directx_jun2010_redistexe/ — DirectX Jun2010 redist 下架与镜像（依据搜索摘要）
71. https://github.com/Winetricks/winetricks/issues/2226 — webview2 verb 请求（2024-05-21；Adobe CC 优先使用 WebView2）
72. https://raw.githubusercontent.com/bottlesdevs/dependencies/main/Essentials/webview2.yml — Bottles 的 WebView2 使用固定的 x64 standalone 安装包（MD5）
73. https://github.com/Arime9/steam-on-m1-wine — 社区方案：steamwebhelper 包装脚本（`--disable-gpu --single-process`）；转述了 Rosetta 退场计划（二手）
74. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.1/ANNOUNCE.md — Wine 11.1 的“Bugs fixed in 11.1”，列有 #56378
75. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.5/ANNOUNCE.md — Wine 10.5 的提交列表，含“win32u: Add stub for NtUserSetAdditionalForegroundBoostProcesses”
76. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.5/dlls/user32/user32.spec — wine-10.5 起 `SetAdditionalForegroundBoostProcesses` 改为 stdcall 转发到 `NtUserSetAdditionalForegroundBoostProcesses`
77. https://bugs.winehq.org/show_bug.cgi?id=57980 — `KERNEL32.SetThreadpoolTimerEx` 未实现（Adobe CC）；评论 7（2026-01-26）指出 2025 年年中 Chromium 引入了对它的硬依赖；由提交 8fc5b439ff6a 修复，2026-09-18 关闭
78. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.17/dlls/kernelbase/kernelbase.spec — wine-11.17 中 `SetThreadpoolTimerEx` 转发到 `ntdll.TpSetTimerEx`
79. https://www.codeweavers.com/compatibility/crossover/forum/microsoft-office-2016/?mhl=343163;msg=343163 — CodeWeavers 在 Office 2016 论坛发布的停止支持公告（页面 403，依据搜索摘要）
80. https://dl.winehq.org/wine/wine-mono/11.3.0/ — wine-mono 11.3.0 下载目录，含 `wine-mono-11.3.0-arm64.msi` 与 `-arm64.tar.xz`（11.2.1 及更早只有 x86 包）
81. https://builds.dotnet.microsoft.com/dotnet/release-metadata/releases-index.json — .NET 官方 release metadata：11.0 RC1、10.0.12（EOL 2028-11-14）、9.0.20 与 8.0.31（EOL 2026-11-10）
82. https://github.com/Winetricks/winetricks/commits/master/src/winetricks?since=2026-01-01&until=2026-04-30 — winetricks 2026-01 至 04 的提交历史（`vcrun2026: new verb` 与 `webview2: New verb` 的提交日期均为 2026-02-21）
83. https://github.com/Winetricks/winetricks/issues/2436 — webview2 verb 的需求 issue，由 PR #2467 关闭
84. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/make_unicode — Wine master 中 zh-CN 仍定义为 `sparent => "zh-Hans"`（约第 1583 行）
85. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.4/dlls/user32/user32.spec — wine-10.4 中 `SetAdditionalForegroundBoostProcesses` 仍是 stub
86. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.18/ANNOUNCE.md — Wine 11.18 的“Bugs fixed in 11.18”，列有 #57980、#59278
87. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/dlls/kernelbase/kernelbase.spec — wine-11.0 中 `SetThreadpoolTimerEx` 仍是 stub
88. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine-staging/repository/tree?path=patches&ref=v11.6 — wine-staging v11.6 的 patches 目录，首次出现 `dcomp-DCompositionCreateDevice2`（v11.5 中没有；v11.18 中该目录有 68 个条目）
89. https://gitlab.winehq.org/wine/wine-staging/-/raw/v11.6/patches/dcomp-DCompositionCreateDevice2/definition — dcomp 补丁集的 definition 文件：修复 bug 54968、58315，并写明完整实现需要 dwm.exe 和图形驱动（win32u 等）集成
90. https://gitlab.winehq.org/wine/wine-staging/-/raw/v11.6/patches/dcomp-DCompositionCreateDevice2/0003-dcomp-Add-IDCompositionDevice-stub.patch — 补丁头：Zhiyi Zhang，2026-03-13，`[PATCH 03/65]`
91. https://raw.githubusercontent.com/wine-mirror/wine/master/loader/wine.inf.in — 上游 Wine master 的 wine.inf.in 中没有 msedgewebview2 条目
92. https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json — .NET 10.0.0 于 2025-11-11 发布
93. https://github.com/wine-mono/wine-mono/releases.atom — wine-mono 发布 feed：11.3.0 发布于 2026-08-17T18:24:41Z，说明中写有新增 ARM64 构建

---

## 事实核查记录

核查日期 2026-09-26。结论来自独立事实核查，正文已据此修订。本报告没有涉及法律范围的核查项，因此没有“因法律问题跳过”的条目。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| Wine bug 56378（Edge/WebView2 必须加 `--no-sandbox`）在 Wine 11.1 修复（2026-01-23 关闭），根因是 user32 缺少 `SetAdditionalForegroundBoostProcesses` | 部分属实 | 前半句属实：bug 列在 wine-11.1 的修复清单中，2026-01-23 关闭。但**没有关联提交**，只是报告者 2026-01-19 复测后标为 resolved。`SetAdditionalForegroundBoostProcesses` 崩溃是 2025-03-18 复测时才发现的回归，不是原始根因，已在 Wine 10.5 修复（win32u 桩）。新版 Chromium 还需要 `SetThreadpoolTimerEx`，它要到 wine-11.17 才实现。已修改摘要、§3.1 表格、建议 5 和未解问题 1 [15][74][75][76][85][77][78] |
| Wine bug 58921（2025-11-04，Wine 10.18，UNCONFIRMED）：winver ≥ 8.1 时 `DCompositionCreateDevice` 返回 `E_NOTIMPL`；Proton 11 的 wine.inf.in 第 1522 行默认设 win7；winetricks 2026-02-21 新增的 `webview2` verb 也这样处理 | 属实 | 日志、Proton 第 1522 行原文和 winetricks `w_workaround_wine_bug 58921` 都已核实；上游 master 没有这一行。“大多数 WebView2 应用不设就无法渲染”属于推断，但与 bug 报告一致 [16][25][1][82][83][91] |
| WebView2Feedback #5720（2026-09-18）：CrossOver 26.3 + macOS 26.7 + M1 Pro 上 runtime 153/149 只画第一帧，原因是 runtime 追加了已失效的 `--use-gl=swiftshader` | 属实 | issue 内容属实。被认定无效的只有 swiftshader 这个值，`--in-process-gpu` 仍合法。这是单一用户的诊断，微软和 CodeWeavers 未回应，根因未经独立确认。已在 §3.2 补充说明，并在建议 5 中改为需要实测验证 [19][20] |
| CodeWeavers 从 CrossOver 26 起（26.0.0 于 2026-02-10 发布）停止支持 Office 2016 和 M365，不再修 bug | 属实（中高）| 两个官方论坛帖（msg=343162/343163）的搜索摘要内容一致。公告早于 26.0 发布，约在 2026-01。帖子原文和确切日期因 403 无法直接读取。changelog 本身没有提到放弃 Office [31][32][33][79] |
| Wine 11.0 自带 Wine Mono 10.4.1，master（11.18）为 11.3.0；wine-mono 11.3.0 于 2026-08-17 发布，是首个 ARM64 构建 | 属实 | `addons.c` 在各 tag 的值已核实，Gecko 均为 2.47.4。11.2.1（2026-09-04）晚于 11.3.0 发布，但它是 Proton 专用的修复版 [8][10][80][93] |
| Wine bug 47277（WPF + zh_CN + winver ≥ Win7 无法启动）2019-05-27 报告，至今仍是 NEW | 属实 | 最后一条评论在 2022-10-28。master 的 `make_unicode` 中 zh-CN 仍是 `sparent => "zh-Hans"`。修复点是否在 `LOCALE_SPARENT` 属于本报告的推断 [12][84] |
| §3.1 表格：bug 57980（`SetThreadpoolTimerEx`）在 Wine 11.18 修复 | 部分属实 | 它确实列在 11.18 的修复清单中（提交 8fc5b439ff6a，2026-09-18 关闭），但**代码在 wine-11.17 tag 中已经存在**，11.0–11.16 是 stub。对新 Chromium 的依赖来自 2025 年年中的 Chromium 改动。已修改 §3.1 表格和建议 5 [54][77][78][86][87] |
| §3.1：Wine-Staging 11.6 合入 Zhiyi Zhang 的 65 个 DirectComposition 补丁，完整实现“需要 dwm.exe”（仅依据 Phoronix 摘要）；另称 dcomp 补丁在“wine-staging 11.15 之后”出现 | 部分属实 | 11.6 和 65 个补丁都属实：`dcomp-DCompositionCreateDevice2` 首次出现在 v11.6（v11.5 中没有），补丁作者是 zzhang@codeweavers.com，2026-03 提交。dwm.exe 的说法已由 staging 的 definition 文件直接证实。“11.15 之后”的说法不对，这批补丁从 11.6 起就一直在 staging 中，到 v11.18 仍在，条目增至 68 个；修复 bug 54968/58315。已修改 §3.1 的表格和 DirectComposition 小节，以及建议 5 [27][88][89][90] |
| Proton 的 `hack_append_command_line()` 是按 exe 名或 SteamGameId 匹配的硬编码表，追加 Chromium 参数；上游 master 没有 msedgewebview2 win7 那一行 | 属实 | 函数在 proton_11.0 `process.c` 第 592 行，在第 715、727 行被调用。这张表只能追加、不能删除或替换参数，这支持了 Cider 需要能删除或替换参数的 hook 的判断。已在 §3.1 补充 [26][91] |
| .NET 10 LTS 于 2025-11-11 发布（EOL 2028-11-14），最新 10.0.12（2026-09-08）；.NET 8/9 于 2026-11-10 停止支持；.NET 11 RC1 于 2026-09-08 发布 | 属实 | 由官方 `releases-index.json` 与 `10.0/releases.json` 核实 [14][81][92] |
| VC++ v14 永久链接覆盖 VS 2017–2026；VS 2026 版只支持 Win10/11；x64 包含 ARM64 和 x64 二进制；VS2015 redist 支持于 2025-10-15 结束；2013 版为 12.0.40664.0 | 属实 | Microsoft Learn 页面原文一致。小出入：提交表中 `vcrun2026: new verb` 原写 2026-02-16，GitHub 显示提交日期为 2026-02-21（02-16 可能是作者日期），已改为 2026-02-21 [7][82] |
