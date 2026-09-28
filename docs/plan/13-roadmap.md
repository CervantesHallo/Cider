# 13 路线图与待办（执行顺序）

> 2026-09-27 起执行。按 00 的阶段 P0→P1 推进，每项完成后在这里改状态。✔ = 已完成并实测；◐ = 部分完成；○ = 未开始。

## 已完成（截至 2026-09-27 深夜）

对 CrossOver 的对等矩阵（05 §15）：P0 共 29 项，完成 16、部分完成 10；全部 55 项，完成 19、部分完成 10，明确不做 2 项（#43、#55 Epic）。


- ✔ CiderKit 骨架：Core / Schema / Store / Runtime / Bottle / PE / Integration；ciderctl；27 个单元测试。
- ✔ 引擎安装、瓶子创建（64 位 WoW64、区域、字体替换、Shell 文件夹隔离）、换引擎前做 APFS 快照。
- ✔ Steam in bottle：登录、中文界面、商店页、下载；msync 下运行（瓶子级 `SyncMode`）。
- ✔ Cider.app GUI MVP：
  - 资料库（Steam 游戏、开始菜单程序、真实图标与封面）；
  - 实时运行状态、逐个应用停止；
  - 游戏详情；补丁拖放（备份 + 撤销、.zip、.exe 安装器）。
- ✔ 千恋＊万花：从 Cider 经 Steam 启动，云存档同步。

## P0 地基（进行中）

| # | 项 | 状态 | 验收 |
|---|---|---|---|
| E0 | 引擎构建工具链：bison/pkgconf/gettext、mingw-w64 gcc 15.2、x86_64 依赖配方（02）；`engine/toolchain.sh`、`engine/deps.sh` | ✔ | `engine/toolchain.sh` 可从零复现；`x86_64-w64-mingw32-gcc`、`i686-w64-mingw32-gcc` 可用 |
| E1 | Engine R v0 `cider-cx26`：CX 26.3 + Highball 补丁 + Cider 0001–0002，带符号构建（02） | ✔（冒烟 5/5；在复制的 Steam 瓶子里 Steam 登录/商店正常，千恋＊万花可玩；翻译与引擎层预检已验证） | 02 的 v0 验收 1–5 |
| E2 | ~~host-identity 补丁~~：CX 树按 exe 名建 preloader 硬链接（Dock 显示程序名），与“统一身份”冲突；改随 ADR-007 的逐游戏 shim（先做 T4） | — | `lsappinfo` 只看到一个 Cider 引擎身份 |
| E3 | P-3：Kirikiri 标题画面主线程热点（带符号 profile → 修复） | ◐（已定位并消除 GDI 回退，总 CPU 120–150% → 100–115%；剩余为游戏每帧读回与自身合成，见 02 P-3） | 主线程 ≤40% |
| E4 | 对话框字体与 CJK 回退：MS Shell Dlg → 区域的无衬线 UI 字体，常见拉丁字体配 SystemLink；前缀修订版 2，旧瓶子自动升级（`bottle upgrade`） | ✔（Steam 引导程序待下次更新时目测） | Steam 引导程序中文可见 |
| Q0 | 冒烟脚本 `scripts/smoke.sh`（wineboot、cmd ver、syswow64、DYLD 检查） | ✔（7 项：另含 Direct3D 11 与 DirectShow WMV） | 本机全绿 |
| Q1 | P-1 门禁：msync 下 Steam UI 冷启动 20 次 | ◐（`scripts/steam-coldstart.sh` 已写，需屏幕解锁时跑） | 20/20 截图非纯色 |
| D0 | profile/recipe/verdict schema（06）+ 第一批数据：Steam、柚子社 Kirikiri 系列 | ◐（CiderData：Game/Verdict/Profile + 红线 lint，`data/` 随 App 打包；Recipe 未做） | schema 校验 + 红线 lint 通过 |
| H0 | R3 H0：政策、lint、预检、路线卡、国服官方云入口 | ◐（Preflight 内置门控名单 + `WineRunner.launch` 拦截 + 进程扫描兜底 + 路线卡与国服云入口；引擎层 `cider/0001` 拒绝 NtCreateUserProcess 待 v0 引擎验证） | 伪安装预检 50 次拉起 exe 0 次 |
| C0 | CI 工作流（engine-build、app、data），等 GitHub 仓库就绪后启用 | ◐（`.github/workflows/ci.yml`、`engine.yml` 已写） | 工作流文件就位，本机 act 或首次推送验证 |

## P1 → 0.1

| # | 项 | 状态 |
|---|---|---|
| A1 | 对等矩阵 P0（05 §15）：运行命令/另存启动器、带选项运行、打开 C:、winecfg/regedit/taskmgr、模拟重启、结束全部 | ✔ |
| A2 | 瓶子管理：新建/复制/重命名/删除、快照列表与回滚、导出/导入 `.ciderbottle`、从 CrossOver/Whisky 导入 | ✔ |
| A3 | 按瓶子与按程序设置：图形后端、msync、Retina/DPI、winver、DLL override、环境变量 | ◐（瓶子级区域/Windows 版本/引擎/同步/高分辨率模式已完成；按程序覆盖未做） |
| A4 | 未收录安装：识别安装器类型，安装后自动出现在资料库 | ✔（版本资源解析、Inno/NSIS/InstallShield/Burn/7z 识别、语言不符时建议新建对应区域瓶子；`ciderctl inspect`） |
| A5 | 诊断：`.ciderlog`、支持包、Metal HUD 开关 | ◐（诊断包：日志/配置/系统信息，自动去除用户名；`ciderctl diag`；HUD 未做） |
| A6 | 应用目录：Recipe + Verdict 浏览、一键安装（CAS 下载、sha256、镜像） | ◐（Recipe v1 子集 + 执行器 + 安装页目录；Steam/VC++ 已实测，EA/战网/米哈游启动器未实测；Epic 配方已下架，见 00「不做清单」第 5 条；镜像未做） |
| A7 | 首次运行引导：Rosetta 检测、引擎下载、GPTK 的 D3DMetal 导入 | ✔（引导页：Rosetta 安装、按引擎索引下载并校验引擎（GitHub Release）、推荐安装 Steam；`ciderctl engine download`） |
| G1 | D3DMetal 导入器（用户自带 GPTK） | ◐（导入、签名与架构记录、逐文件哈希、界面入口；运行时接入待有 GPTK 后实测） |
| G2 | DXMT 作为组件（0003 补丁 + 组件目录） | ◐（DXMT 已内置进 v0 引擎并实测：`tests/graphics/d3d11-triangle.c` FL 11_0、600 帧 124 fps、0 次呈现失败；作为独立组件覆盖未做） |
| M1 | GStreamer 内置 + 视频冒烟（Kirikiri/WMV 开场动画） | ✔（`engine/bundle-gstreamer.sh`：32 个插件 62 MB，仅 x86_64；`tests/media/dshow-play.c` 实测 MPEG-1 与 WMV 经 DirectShow 播完） |
| L1 | 其他启动器：EA、Battle.net、米哈游启动器（带预检）；Epic 不做 | ◐（**米哈游启动器国服已端到端实测**：下载→安装→启动→界面完整渲染，见 L2；EA/战网未实测；Epic 配方已删除，05 §15 #55） |
| L2 | 米哈游启动器（国服）实测 | ✔（2026-09-28，bottle-ca8f + `cider-cx26.3-r1-x86_64`，未修改环境。安装器渲染正常；启动器初次白屏，定位为 CEF GPU 进程向他进程子窗口呈现，需 Highball 0007 + `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN`；由 `profile.launcher.mihoyo-cn` 热修，复测通过。游戏本体仍按 ADR-010 预检阻断，未启动） |
| L3 | Profile 在启动路径生效 | ✔（此前 `actions.env` 只在界面显示、从不进启动；现 `WineRunner` 持有 `CompatDB`，按 exe 名解析 profile 并注入 env，显式 `--env` 优先。这是 00 指导原则 1「会变的放进数据」第一次真正落地） |
| L5 | 自绘标题栏应用被套上原生标题栏（通用保真缺陷） | ○（实测：米哈游启动器自绘顶部 UI 被 Cocoa 标题栏压住切半。判定在 `winemac.drv/window.c:84 get_window_features_for_style`，只看 `WS_CAPTION` 样式位。上游 `win32u/window.c:2069 get_visible_rect` 已有 `EqualRect(window, client)` → 不加宿主边框的通路，但对该启动器没生效，原因待定：需要一次带 `WINEDEBUG=+macdrv` 的稳态复现取矩形数值（创建期 trace 三个矩形都相等，不能用来判断）。CrossOver 对这一类是逐应用特判（`window.c` 里的 Quicken CW HACK 16933），Cider 要的是通用修复，不按应用名分支。影响所有 Electron/CEF/Qt 无边框/WPF 自绘 chrome 程序） |
| L4 | Wine 程序的 macOS 应用身份（`org.cider.winehost`） | ○（实测发现：Wine 程序是裸进程、无 bundle，影响 Game Mode、麦克风/摄像头授权归属、Dock 显示与可自动化性。仓库已有该 bundle 但未启用，见 05 APP-14/T4） |
| K1 | K 线 ①：内核安全 API 保真度矩阵（47 项审计，22 项当前伪造成功）+ 诚实失败码 | ◐（矩阵已落成数据 `data/kernel/fidelity.json` + `KernelFidelity` 加载器，强制「不可达 ⇒ 只能诚实失败」；KMDF 运行时与 Windows 对照 conformance 未做） |

| R1 | 0.1 打包：签名、公证、DMG、Sparkle（需要 Developer ID） | ○ |

## 等用户处理的事

- ~~GitHub 仓库~~：已建立 https://github.com/CervantesHallo/Cider，CI 通过；引擎以 GitHub Release 发布。
- Developer ID：签名公证 DMG（R1）。
- 可选：从 Apple 下载 Game Porting Toolkit，用来验证 D3DMetal（G1）。

## 约定

- 每项做完：实测 → 更新本表 → 必要时更新对应计划文档。
- 需要用户动手的事（账号、密码、购买、系统设置），做到那一步再请用户处理，其余自主推进。
