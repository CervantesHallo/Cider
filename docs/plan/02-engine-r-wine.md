# 02 Engine R：自建 Wine 引擎、补丁队列与性能

> v1 · 2026-09-27 · 依赖 00（ADR-002/004/005/006/007）、研究 02/12/17/18/20。只写现在要做和马上要做的；后续按窗口卡补充。

## 目标

1. **自己构建 Engine R**：不再依赖别人的二进制引擎。构建要可复现：固定输入 + sha256，本机与 CI 使用同一套脚本。
2. **把现在能用的组合（Highball 引擎 + DXMT）变成自己的引擎**：Steam、千恋＊万花都已在上面跑通。
3. **在自己的引擎上解决性能与身份问题**：这类问题只能改引擎才能解决（见“性能工作项”）。

## 当前状态（2026-09-27 实测）

- 在用：`highball-11.0-0011b-dxmthb-x86_64`，即 Highball 0011b（CX 26.3 源码 + Highball 0001–0011）+ Highball 版 DXMT。这是别人的二进制，被剥离过，依赖靠 APFS 克隆的 Gcenx dylib 补齐（`libraryPaths: [frameworks]`）。
- Steam 在 msync 下工作正常，已改为瓶子级默认（`SyncMode`）。
- 限制：
  - 只有会话里第一个进程带 CiderWineHost 身份，子进程直接 exec 真实 loader；
  - 引擎没有调试符号，Rosetta 下无法 profile。

## 基线与版本线

| 线 | 源码 | 用途 | 时间 |
|---|---|---|---|
| **v0 `cider-cx26`** | `crossover-sources-26.3.0.tar.gz`（Wine 11.0 底座，sha256 固定）+ 挑选的 Highball 补丁 + Cider 补丁 | 第一个自建引擎，替换当前的 Highball 二进制；同时充当 00 里的 CX oracle | 现在 |
| v1 `cider/devel` | wine-11.18 + CX 26.3 补丁按主题拆分（ADR-004） | 1.0 主线，逐 tag rebase，12.0 定为 1.0 基线 | v0 跑通 Steam 后开始 |

v0 先行的理由：它与现在跑通 Steam 的组合在源码上等价，差异可控。先把构建管线、布局、签名、冒烟跑通，v1 的 rebase 只需替换源码输入。

### v0 补丁选择（来自 highball-engine/patches）

| 补丁 | 取舍 | 理由 |
|---|---|---|
| 0001 use-the-real-user-name | 取 | 瓶子用户目录 = macOS 用户名，与现有瓶子一致 |
| 0002 wined3d auto renderer opengl-first | 取 | macOS 上 wined3d-vk 在 32 位程序下崩溃（千恋＊万花实测：MoltenVK 外部内存 buffer 报错） |
| 0003 ntdll WINEDLLPATH prepend | 取 | 让组件覆盖目录优先于引擎 builtin，DXMT/D3DMetal 不再需要克隆引擎变体 |
| 0005 yield after auto event set | 取 | Highball 针对启动器卡顿的修复 |
| 0006 kernelbase per-exe CommandLineAppend | 取 | profile 的 argv 追加动作（ADR-009 `cider-rules` 的一部分）先用它实现 |
| 0007 winemac 跨进程子窗口 swapchain（C1） | 取 | Steam CEF 能渲染的根本原因 |
| 0008/0009 TEB FiberData/QoS 槽位 | 取 | 0011b 带着它们时 Steam 已验证可用；逐个复核后再决定是否上游化 |
| 0010 mfreadwrite video processor | 取 | 视频播放（开场动画）相关 |
| 0011 LastError 放进 gs 槽 | 取 | 0011b 已含 |
| 0012 x87sidecar | 不取 | 试验性质 |
| 0013 NX compat under Rosetta | 待定 | 0011b 未含；单独验证后再决定 |

Cider 自己的补丁（主题化，尾注 `Cider-Topic:`）：

- `r3/preflight`（`cider/0001`，已写）：`NtCreateUserProcess` 拒绝 `CIDER_PREFLIGHT_DENY` 列出的映像，返回 `STATUS_ACCESS_DISABLED_BY_POLICY_DEFAULT`（与 Windows AppLocker/SRP 拦截一致，Win32 错误 1260）。瓶子里的启动器也无法拉起被门控的游戏。

- `macos-ux/host-identity`：所有 Windows 进程都从 CiderWineHost.app 的 loader 启动（修改 loader 的自我 exec 路径与 `WINELOADER` 处理）。目标是 Dock、通知和 TCC 只显示一个身份。
- `cjk/systemlink`：Tahoma / MS Shell Dlg 的 SystemLink 默认回退到 macOS 的 CJK 字体。修掉 Steam 引导程序对话框里中文不显示的问题。
- `perf/*`：见下文。

## 工具链（本机与 CI 相同）

**原则**（研究 12）：
- 宿主工具用 arm64 原生；
- x86_64 依赖用 Xcode clang `-arch x86_64` 交叉编译，不用 x86 Homebrew；
- Wine 本体用 `arch -x86_64 configure/make`，这是上游验证过的做法。这棵源码树的 unix 侧只能构建 x86_64（`d3dmetal_objc.h` 位于 `__x86_64__` 宏之下）。

| 组件 | 来源 | 备注 |
|---|---|---|
| bison ≥3、pkgconf、ccache | 源码构建（arm64） | 装到 `~/Library/Caches/Cider/toolchain/host` |
| mingw-w64 gcc（i686 + x86_64） | binutils + gcc + mingw-w64 源码自建 | llvm-mingw 编出的 kernelbase 会让 Steam 登录卡住（研究 17）；llvm-mingw 只留给 Engine A 的 arm64ec |
| freetype、gnutls（gmp/nettle/libtasn1/libunistring）、SDL2、libpng | 源码，`-arch x86_64` | 装进 `toolchain/x86_64`；进引擎的 dylib 一律改成 `@rpath` |
| MoltenVK | 固定到带 shadow-import 的 1.4.2 之前（ADR-006），官方 release | universal |
| GStreamer | 1.28 官方 framework（universal） | 引擎内置所需插件（ADR-012 之后），暂时沿用系统 framework |
| Wine Mono / Gecko | dl.winehq.org，按 `addons.c` 里的版本号 | 免去首次启动时的下载弹窗 |

**SDK 必须与部署目标匹配**（2026-09-27 实测）：用 Xcode 27 的 macOS 27 SDK 构建时，configure 检测到了 `pipe2`（macOS 27 才有）。它被弱链接，在 macOS 26 上解析为空指针，`wineboot` 一启动就在 pc=0 崩溃。因此依赖和 Wine 一律用 Command Line Tools 里的 MacOSX15.x SDK（`CIDER_SDK` 可覆盖），部署目标 14.0。CI 的 macos-26 runner 同样要显式指定。

构建入口：`engine/build.sh <recipe>`。recipe 是 `engine/recipes/<id>.json`，包含源码 URL + sha256、补丁列表、configure 参数、组件。产物是 `out/engines/<id>/`，外加 `manifest.json`（Ed25519 签名在 CI 做）和 tarball。本机用它做冷构建（8 GB 可行，但慢）和单 DLL 增量构建；等有了 GitHub 仓库，由 CI 接手冷构建。

## 引擎布局

```
Engines/<id>/
  manifest.json            # EngineManifest；libraryPaths 只指向引擎内部
  engine/{bin,lib,share}   # wine、wineserver、lib/wine/{i386,x86_64}-{windows,unix}
  frameworks/              # 引擎自带的 dylib（@rpath），不再从别的引擎克隆
  CiderWineHost.app        # 由 EngineHost 生成；host-identity 补丁让所有进程都走这里
```

DXMT 作为组件（ADR-006），不再克隆整个引擎：0003 补丁生效后，组件目录放在 `WINEDLLPATH` 前部即可。`winemetal.dll` 的占位按 highball 的做法放进引擎 builtin。

## 性能工作项（来自 2026-09-27 的卡顿问题）

实测背景：M3 / 8 GB，Steam 下载 千恋＊万花 时出现按键延迟和加载卡顿。

| # | 问题 | 现状与证据 | 做法 | 验收 |
|---|---|---|---|---|
| P-1 | Steam UI 卡顿 | 旧配置对 Steam 关闭 msync，所有同步都往返 wineserver；开启 msync 后用户确认不卡 | 已改为瓶子级 msync 默认 ✔。补 ADR-005 的门禁：msync 下 Steam UI 冷启动 20 次零看门狗 | 脚本化 20 次冷启动，截图非纯色，webhelper 无挂起 |
| P-2 | 下载时 steam.exe 占 119% CPU | 解压与校验在 Rosetta 下进行；3.9 GB swap | 自建引擎后先有符号再 profile；检查 bcrypt/crypt32 路径（gnutls 版本、哈希实现） | 相同下载速率下 CPU 降低 ≥30%，或给出不可降的证据 |
| P-3 | Kirikiri（千恋＊万花）标题画面主线程约 90% | 2026-09-27 用带符号的 v0 引擎 + `scripts/wine-profile.py`（lldb）定位：① 全屏黑边导致每帧“部分呈现”，走 wined3d 的 GDI 回退（`glGetTexImage` + `StretchBlt`）→ `cider/0002` 优先选 COPY 交换格式，已消除；② 游戏每帧 `GetRenderTargetData` + `LockRect` 读回渲染目标（游戏本身的设计），Apple 的 GL 在 CPU 上还原分块布局并同步等 GPU。PBO 读回可以去掉 CPU 还原，但总 CPU 没有下降，已撤回；③ 0013（NX）让辅助线程从约 13% 降到约 6%。总体从 120–150% 降到 100–115% | 下一步：GetRenderTargetData 的读回改为异步或走共享内存纹理（Apple 的 client storage / `GL_STORAGE_SHARED_APPLE`）；把 Kirikiri 自身合成的开销单独量出来（Rosetta 下的 SSE 路径） | 标题画面主线程 ≤40%，画面无回归（截图 SSIM） |
| P-4 | 8 GB 内存压力 | Steam CEF ≈1.5 GB，游戏 ≈1 GB | Cider 启动游戏时 Steam 带 `-silent` ✔；在 UI 提示 Steam 设置（库为首页、低性能模式、关特惠弹窗）；研究 CEF 在隐藏窗口时的节流（winemac 遮挡通知 → `WM_SHOWWINDOW`/occlusion） | Steam 窗口隐藏时 webhelper CPU ≈0，内存不再增长 |
| P-5 | 代理 TUN 的额外开销 | mihomo gVisor 栈在下载时占 34% | 不属于引擎问题：在环境检查里提示 TUN 协议栈可选 mixed/system；Steam CDN 直连规则写进 Cider 的“网络建议”文档 | — |
| P-6 | Rosetta 首次运行卡顿 | AOT 翻译缓存（`/var/db/oah`）在首次运行时生成 | 安装程序或首次运行时预热常用 PE/unix 模块（只读访问触发翻译），与引擎安装一起做 | 第二次启动与第一次的差距 ≤20% |

性能工作的前提是**带符号的自建引擎**（P-2、P-3 都卡在“Rosetta 下没有符号”）。发布版本剥离符号，另存一份 dSYM 与 PE `.debug` 包供诊断下载。

## 验收（v0）

1. 用 `engine/build.sh cider-cx26` 从零构建成功，产物里没有 `/usr/local`、`/opt/homebrew` 引用（`otool -L` 与 `DYLD_PRINT_LIBRARIES` 检查）。
2. 冒烟：`wineboot --init`、`cmd /c ver`、`syswow64\cmd /c ver`。
3. Steam 瓶子切换到 v0：登录记忆仍在；商店页渲染；千恋＊万花能启动并显示标题画面。
4. 所有 Windows 进程都在 CiderWineHost 身份下（`ps` + `lsappinfo` 检查）。
5. 旧的 Highball 引擎保留为回退，`switchEngine` 前自动快照。
