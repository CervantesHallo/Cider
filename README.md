# Cider

**在 Apple 芯片的 Mac 上，畅玩 Windows 游戏、运行 Windows 软件。**

我们非常自豪地宣布：**Cider 正式开源了！**

Cider 是一个从零打造的 Mac 兼容层。它基于 Wine，配有一套原生 SwiftUI 界面，目标很明确：达到 CrossOver 的水准，并在中文用户真正在意的地方做得更好。

我们的最终游戏目标包括柚子社全系，以及**原神、星穹铁道、绝区零的 Windows 国服本地适配**。三款逐一以登录、进入可操作场景、图形、音视频、输入、更新与重新启动的实测结果验收。目前仍在开发，启动器可用和下载完成分别记录为中间进度；详见 [`米哈游适配计划`](docs/plan/09-hoyoverse-games.md)。

在 Mac 上玩 Windows 游戏，不该是一件需要折腾的事。你只要在 Cider 里装好 Steam，下载的游戏就会自动出现在资料库里：真实封面、实时运行状态，点一下就能开始。galgame 的补丁拖进去就能装好，不想要了一键撤销。日文游戏自动建议合适的区域，中文界面和字体开箱即用。背后的 Wine 引擎也由我们自己构建：每一个补丁都有来历，每一次性能优化都有测量数据。

这是 Cider 的第一个开发预览版。它已经能在 M3 MacBook 上流畅运行 Steam，也能畅玩《千恋＊万花》。我们把代码、计划和研究资料全部公开，欢迎一起把它做好。

> 当前版本 0.0.x（开发预览）。完整计划见 [`docs/plan/`](docs/plan/)（从 `00-strategy-and-decisions.md` 读起），进度见 [`docs/plan/13-roadmap.md`](docs/plan/13-roadmap.md)，研究资料见 [`docs/research/`](docs/research/)。

## 现在能做什么

- **资料库**：Steam 里安装的游戏、开始菜单里的程序、自建启动器会自动出现，显示真实图标和封面；实时运行状态，每个程序单独停止。
- **Steam**：在瓶子里运行 Windows 版 Steam（登录、中文商店、下载、云存档），从 Cider 直接启动 Steam 游戏。已验证：千恋＊万花。
- **galgame**：补丁拖放安装（自动备份、可撤销）；日文安装程序会被识别，提示你新建日文区域的瓶子。
- **瓶子管理**：新建、复制、改名、删除；APFS 快照与一键回滚；导出/导入 `.ciderbottle`；从 CrossOver / Whisky 迁移；Windows 工具（winecfg、注册表、任务管理器等）；运行命令和自建启动器；模拟重启；高分辨率模式；诊断包（会自动去掉用户名）。
- **应用目录**：一键安装 Steam、VC++ 运行库（均已实测），以及 EA app、战网（尚未实测）。米哈游启动器已验证安装和界面渲染，登录与下载更新流程仍需完整验收。下载有 sha256 校验和内容寻址缓存。不做 Epic Games Store（见 [`docs/plan/00-strategy-and-decisions.md`](docs/plan/00-strategy-and-decisions.md) 的“不做清单”）。
- **自建引擎**：`cider-cx26.3`，基于 CrossOver 26.3 源码，加上 Highball 补丁和 Cider 自己的补丁；自带 GStreamer（开场动画）和 DXMT（DirectX 10/11 → Metal）；msync。
- **米哈游**：国服三款游戏在反作弊问题有如实的解决方案之前不会在本机启动，而是提供官方云游戏入口，所以不会弹出反作弊报错。

## 开始使用

构建出 `Cider.app`（见下文）并打开，首次运行引导会检查 Rosetta，然后从本仓库的 [Releases](https://github.com/CervantesHallo/Cider/releases) 下载引擎（约 250 MB，下载后校验 sha256），再一键装好 Steam。命令行用户可以执行 `ciderctl engine download`。

## 构建

需要 Apple 芯片的 Mac、macOS 14+、Xcode（单元测试要用 Xcode 自带的 Testing 模块），以及 Rosetta。

```sh
# App 与命令行
./scripts/build-app.sh                  # → out/Cider.app
cd Tools/ciderctl && swift build        # → .build/debug/ciderctl
cd Packages/CiderKit && swift test      # 单元测试

# 引擎（第一次需要几个小时：先构建工具链，再编译 Wine）
engine/toolchain.sh                     # bison、gettext、mingw-w64 gcc …（缓存在 ~/Library/Caches/Cider/toolchain）
engine/deps.sh                          # x86_64 的 freetype、gnutls、SDL2、MoltenVK、GStreamer 头文件
engine/build.sh engine/recipes/cider-cx26.json   # → out/engines/<id>
scripts/smoke.sh out/engines/<id>       # 冒烟测试
.build/debug/ciderctl engine install out/engines/<id>
```

引擎必须用 macOS 15 SDK 构建（Command Line Tools 里的 `MacOSX15.x.sdk`，可用 `CIDER_SDK` 指定）。原因见 `docs/plan/02-engine-r-wine.md`。

常用命令：

```sh
ciderctl bottle create "Galgame" --locale ja
ciderctl run -b Galgame /path/to/setup.exe
ciderctl install --list                 # 应用目录
ciderctl inspect setup.exe              # 安装程序类型与语言
ciderctl diag Galgame                   # 诊断包
```

## 目录

| 路径 | 内容 |
|---|---|
| `App/` | SwiftUI 应用 |
| `Packages/CiderKit/` | 全部业务逻辑（引擎、瓶子、运行、兼容库、配方、PE 解析） |
| `Tools/ciderctl/` | 命令行工具 |
| `engine/` | 引擎配方、补丁、构建脚本 |
| `data/` | 兼容库（游戏、Profile、Recipe） |
| `scripts/` | 打包、冒烟测试、性能分析 |

## 原则

- 只做 64 位（新 WoW64）瓶子，32 位程序在其中运行。
- Apple 的 D3DMetal 不随 Cider 分发，由用户从 Game Porting Toolkit 导入。
- 不篡改、伪装、隐藏或绕过任何反作弊；Wine 始终可被识别；不支持的游戏给出明确说明而不是启动。

## English

We're proud to open-source **Cider**, a Wine-based compatibility layer for Apple silicon Macs with a native SwiftUI interface, aiming for CrossOver-level quality with a Chinese-first experience. Today it runs the Windows Steam client (login, store, downloads, cloud saves) and launches Steam games straight from its library; *Senren＊Banka* is verified. It ships its own Wine engine (CrossOver 26.3 sources plus the Highball and Cider patches, bundled GStreamer and DXMT). Bottle management covers snapshots with rollback, import from CrossOver/Whisky, an app directory with one-click installs, and diagnostics bundles. This is an early developer preview; see `docs/plan/` for the roadmap.

Our final game goals include the full Yuzusoft catalog and local compatibility for the official China-server Windows clients of *Genshin Impact*, *Honkai: Star Rail*, and *Zenless Zone Zero*. Each title needs its own evidence for login, interactive gameplay, graphics, audio, input, updates, and reliable relaunch. Local gameplay for these three titles remains unverified; see [the adaptation plan](docs/plan/09-hoyoverse-games.md).
