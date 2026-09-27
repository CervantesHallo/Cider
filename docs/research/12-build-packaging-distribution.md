# Cider 工程底座调研：Wine 构建、打包、签名公证、自动更新与 CI

> 调研日期 2026-09-26 · 置信度说明：**[已证实]** 表示有一手来源可查（官方文档、源码、发布页、仓库文件、API），URL 见“参考来源”；**[本机]** 表示 2026-09-26 在开发机（Apple M3，8 GB，macOS 26.5 25F71，只装了 CLT 26.6）上实测；**[二手]** 表示来自搜索摘要或第三方文章，未直接读到一手原文；**[推断]** 表示根据已证实事实做的工程推断，需要实测确认。版本号和日期只写查到的，查不到的在“未解问题”里列出。本文不涉及许可证和法律问题。

---

## 摘要

- **Wine 在 Mac 上仍然只能构建成 x86_64，经 Rosetta 2 运行。** 上游 CI 的 `tools/gitlab/build-mac` 在 ARM runner 上用 `arch -x86_64 ../configure -C --enable-win64 --with-mingw` 构建：依赖取自 `/usr/local` 下的 x86 Homebrew，bison 用 `/opt/homebrew` 下的 arm64 版，SDK 用 Xcode 自带的，编译经 ccache 加速 [1][2]。WineHQ 官方 macOS 包（Gcenx）改用 MacPorts overlay，搭配 llvm-mingw 和 `--enable-archs=i386,x86_64`。其中 `build_arch x86_64` 是 overlay README 要求写进 `macports.conf` 的全局设置，wine-devel 的 Portfile 本身声明的是 `supported_archs x86_64`。截至 2026-09-25（Atom feed 时间 15:27Z），最新是 11.18 [3][4][5][6]。
- **开发机的 CLT 能完成大部分工作，但缺 4 样东西** [本机]：
  - 能做的：Apple clang 21.0.0 和 ld-1267 可以产出 x86_64 Mach-O，也接受 `-Wl,-no_huge`；clang 还能生成 i386/x86_64 的 COFF 目标文件；`notarytool`、`stapler`、`codesign`、`install_name_tool`、`lldb`、Swift 6.3.3 都在。
  - 缺的：① bison 只有 2.3，而 Wine 要求 ≥3.0；② 没有 pkg-config；③ 没有 lld、PE 头文件和 CRT，所以必须装 llvm-mingw 或 mingw-w64；④ 没有 `xcodebuild`、`actool`、`ibtool`、`metal`。第④项会卡住 DXMT（需要 Metal toolchain）、MoltenVK 源码构建，以及 GUI 的资源编译（asset catalog 和 Icon Composer 图标）[27][62]。
- **x86 Homebrew 这条依赖来源正在失效。** Homebrew 5.0.0（2025-11-12）宣布：2026 年 9 月或之后，Intel x86_64 macOS 降为 Tier 3，不再构建新的 Intel bottle；2027 年 9 月或之后完全不能在 Intel x86_64 上运行 [60][61]。截至 2026-09，Support-Tiers 页面已把 Intel x86_64 列为 Tier 3，写明已停止构建 Intel bottle，并建议 Intel 用户改用 MacPorts [61]。两页都没有专门提到“Apple Silicon 上经 Rosetta 运行的 `/usr/local` x86 Homebrew”，但它本质上就是一个 x86_64 安装，所以同样受影响。**[推断，可信度高]** 这意味着 Cider 要么用 MacPorts 的 `build_arch x86_64`（Gcenx 的做法），要么**自己维护依赖配方**：用原生 arm64 编译器加 `-arch x86_64` 交叉编译 freetype、gnutls、SDL2 等库。**我推荐后者。**
- **GitHub Actions 现状（2026-09）** [48][49][50][53][54][55]：
  - 标准 arm64 runner（`macos-15`、`macos-26`、`macos-latest`）配置为 3 核 M1、7 GB 内存、14 GB SSD，**公开仓库免费**。
  - Intel runner 有 `macos-15-intel` 和 `macos-26-intel`（后者 2026-02-26 GA），但 GitHub 在 2025-09 的公告里写的是“macOS 15 镜像在 2027 年秋退役后不再支持 Intel”。两处说法有矛盾，见“未解问题”。
  - `macos-26` 镜像：Xcode 26.6 为默认，还有另外 6 个版本；Homebrew 6.0.22；**没有 bison、ccache、meson**。镜像软件清单里没有列出 Rosetta，但 runner-images 的 Packer 模板（macOS-26.arm64、macOS-15.arm64）在构建镜像时就执行了 `install-rosetta.sh`（`softwareupdate --install-rosetta --agree-to-license`），所以**标准 arm64 runner 应已预装 Rosetta** [75][76]。
  - 私有仓库按 $0.062/分钟计费；自托管 runner 的平台费（原定 $0.002/分钟）**已推迟**。
- **签名公证的硬规则** [33][34][36][39][41]：
  - 必须用 Developer ID（Apple Developer Program 年费 $99）。
  - 所有可执行文件要开 hardened runtime，签名要带 secure timestamp，不能有 `get-task-allow`。
  - 由内向外逐个签名，不要用 `--deep`；entitlements 只加在主可执行文件上。
  - `notarytool` 只接受 UDIF DMG、flat pkg 和 zip；altool 从 2023-11-01 起已停用。
  - Apple 对“代码”的定义只包括 Mach-O，所以 **Windows PE 文件对 Apple 来说是资源，不需要单独签名** [35]。
- **Rosetta 下的 x86_64 代码可以完全不签名；原生 arm64 代码至少要有 ad-hoc 签名** [38]。另一方面，由 App 自己下载的文件默认**不会**被打上 quarantine（除非设置 `LSFileQuarantineEnabled`）[42]。Whisky 和 Mythic 正是利用这两点，把 Wine 引擎放到 `~/Library/Application Support` 下单独下载，App 本体才得以很小，且容易公证 [8][11]。需要注意，Apple 只承诺 Rosetta 作为通用工具支持到 macOS 27（Homebrew Support-Tiers 页面转引 Apple 的说法）[61]，这是另一个长期风险。**建议 Cider 采用“GUI（公证）+ 独立版本化的引擎包（Ed25519 签名的清单）”架构。**
- **自动更新用 Sparkle 2。** 最新 2.10.0（2026-09-13）把最低系统提到 macOS 12，已不再支持 CocoaPods。签名用 EdDSA（ed25519）：`generate_keys` 生成密钥，公钥写进 `SUPublicEDKey`。`generate_appcast` 会自动生成 delta（delta 格式 4 需要 Sparkle 2.7 以上），支持 .dmg、.zip、.tar.* 和 .aar，也支持 phased rollout [44][45][46][47]。Sparkle 只负责更新 App 本身；引擎和组件要自己实现一套清单与下载器。
- **引擎管理的业界做法**：
  - Whisky：一个 plist 里只写一个 SemVer [11]。
  - Mythic：`Engine.tar.xz` 加 `Properties.plist` [8]。
  - Sikarugir：`EngineList.txt` 加 GitHub Releases 上的 tar.xz，单个约 160 MB [14][15]。
  - Bottles：YAML 清单，含 MD5，每个 bottle 分别记录 Runner、DXVK、VKD3D、NVAPI 的版本 [67][68]。
  - Lutris：JSON API 字段为 `version`、`architecture`、`url`、`default` [69]。
  - wineforge：SHA-256 固定源码、SPDX SBOM、GitHub attestation 和 `*.runtime.json` [16]。

  Cider 应当综合这些做法：清单用 SHA-256 加 Ed25519，每个 bottle 固定引擎版本，切换引擎前先做 APFS 快照以便回滚。
- **在 8 GB 开发机上的节奏**：本机只做增量 DLL 开发和调试；冷构建、LLVM 与 DXMT、签名公证、发布都放到 CI。已知数据点：M2 上只构建 64 位 Wine 不到 15 分钟 [20]；dappermint 在自托管 runner 上、ccache 已预热时约 15 分钟 [19]。

---

## 详细调研

### 1. 在 Apple Silicon 上构建 Wine（上游与 CrossOver 版）

#### 1.1 基本现实：仍然是“x86_64 Wine + Rosetta”

- 上游 CI 的 macOS 构建脚本 `tools/gitlab/build-mac` 检测到 `arch` 为 `arm64` 时，会设置 `ARCH_CMD='arch -x86_64'`，并把 `SDKROOT` 指向 Xcode 的 SDK（脚本注释：“On the ARM runner, use Xcode's SDK… will default to the command line tools”）。之后依次运行 `tools/make_requests`、`make_specfiles`、`make_makefiles` 和 `autoreconf -f`，再执行 `$ARCH_CMD ../configure -C --enable-win64 --with-mingw BISON="$ARCH_BREW_HOME/opt/bison/bin/bison"` 与 `$ARCH_CMD make -s -j$(sysctl -n hw.activecpu)`。依赖来自 `/usr/local`（x86 Homebrew），ccache 的包装器也放在 `/usr/local/opt/ccache/libexec` 下 [1]。**[已证实]**
- 对应的 GitLab job `build-mac` 跑在 Tart 虚拟机镜像 `winehq-sequoia-pristine` 上，按 `$CI_JOB_NAME-ccache` 缓存 ccache，并缓存 `build64/config.cache` [2]。另有 `build-daily-mac` 跑在 `winehq-sonoma-pristine` 上 [2]。**它只构建 64 位（`--enable-win64`），不产出 i386 PE，也不跑测试**：`configure.ac` 在未指定 `--enable-archs` 时取 `cross_archs=$HOST_ARCH`，所以只生成 x86_64 PE [77]；据事实核查，同目录的 `test.yml` 里也没有 mac 测试 job。因此新 WoW64 在 macOS 上没有上游 CI 覆盖（与 03 号报告一致）。**[已证实]**
- Wine 10.0 起，用 Xcode ≥ 15.3 构建时不再需要 preloader [21]。Wine 11.0 移除了 `wine64`，只保留一个 `wine` 加载器，并开始安装 `wine/unixlib.h` [22]。Wine 11.6 起 64 位 macOS 必须有 PE 编译器，11.16 起非 PE 构建直接报错（见 03 号报告）。**[已证实]**
- 值得注意的是，一个 **arm64 进程可以调用只有 arm64 架构的 bison**：上游在 `arch -x86_64` 的构建里用的正是 `/opt/homebrew` 下的 arm64 bison [1]。所以宿主工具（bison、pkgconf、meson、ninja）不必是 x86 版本。**[已证实]**
- `arch -x86_64 make` 起来之后，子进程里的 universal 工具（例如 `/usr/bin/clang`）也会以 x86_64 身份在 Rosetta 下运行，编译速度因此比原生慢。**[推断，需要实测比例]**

#### 1.2 工具链逐项核对（开发机实测 + 需求）

| 项目 | 开发机现状 [本机] | Wine / Cider 需求 | 结论 |
|---|---|---|---|
| Apple clang / ld | clang 21.0.0（clang-2100.1.1.101），ld-1267；`clang -arch x86_64` 生成 x86_64 Mach-O 成功；`-Wl,-no_huge` 能链接 | 编译 unix 侧 `.so` 和 loader | 够用 |
| macOS SDK | CLT 自带 `MacOSX26.5.sdk`、`MacOSX26.sdk`、`MacOSX15.4.sdk`、`MacOSX15.sdk` | 需要 x86_64 的 tbd，CLT 的 SDK 里有 | 够用。上游 ARM runner 用 Xcode SDK 的具体原因未知 [1] |
| bison | `/usr/bin/bison` 为 2.3 | configure 要求 ≥3.0 | **需要另装**（arm64 版即可） |
| flex | 2.6.4 | Wine 需要 flex | 够用 |
| pkg-config | 没有 | 需要 | 需要装 pkgconf |
| PE 编译器 | Apple clang 能生成 COFF（`-target x86_64-w64-mingw32 -c` 成功），但 **没有 ld.lld、lld-link、llvm-dlltool、llvm-ar，也没有 mingw 头文件和 CRT** | 11.6 起强制要求 | **需要 llvm-mingw 或 mingw-w64** |
| llvm-mingw | — | 最新 20260922（LLVM 23.1.2），上一个 20260908（LLVM 23.1.1）[26]；DXMT CI 用的是 `llvm-mingw-20260908` 的 `ucrt-macos-universal` 包 [28] | 推荐，有 universal 包 |
| Xcode（完整版） | 没有 `xcodebuild`、`actool`、`ibtool`、`metal`、`xcstringstool` | 构建 Wine 本身**不需要** [推断]；DXMT 需要 Xcode 16+ 和 Metal toolchain [27]；MoltenVK 源码构建需要 Xcode；GUI 资源（Icon Composer 的 `.icon` 转成 `Assets.car`）需要 Xcode 26 的 actool [64] | **分阶段安装**，见第 5 节 |
| Metal toolchain | — | 从 Xcode 26 开始单独下载：`xcodebuild -downloadComponent MetalToolchain`，约 700 MB；CI 上装它有已知坑 [63] | DXMT 的 CI 要处理 |
| 公证工具 | `notarytool`、`stapler` 在 CLT 里；`altool` 没有 | altool 已停用 [33] | 够用 |

**PE 编译器二选一**：
- **llvm-mingw**：WineHQ 官方包（Gcenx 的 Portfile 写的是 `--with-mingw=${prefix}/libexec/llvm-mingw/bin/clang`）[6]，以及 DXMT CI [28] 都用它。它有 universal 版本，一套工具链同时覆盖 i686、x86_64、aarch64 和 arm64ec，**是 ARM64EC 路线的必需品**。
- **mingw-w64 gcc**：Mythic（固定 mingw-w64 v12.0.0_1，因为需要 binutils ≤ 2.43.1）[8] 和 wineforge [17] 用它。dappermint 通过逐模块二分发现，**llvm 编译的 `kernelbase.dll` 会让 Steam 的 CM 登录卡住**，因此改用了 gcc [19]。**[二手：单一来源]**
- **建议**：以 llvm-mingw 为主，因为它和 ARM64EC 路线一致，也与官方包相同。回归测试里加入 Steam 登录冒烟测试；一旦复现 dappermint 的问题，就给个别模块（kernelbase）准备 gcc 构建的 fallback。

#### 1.3 x86_64 依赖从哪里来：四条路线

| 路线 | 做法 | 使用者 | 优点 | 缺点 |
|---|---|---|---|---|
| A. x86 Homebrew（`/usr/local`）+ `arch -x86_64` | 在 Rosetta 下装一整套 x86 Homebrew | 上游 Wine CI [1]、idrewsomeshapes 博客 [20] | 上游验证过 | **自 2026-09 起 Intel 降为 Tier 3，不再出 bottle，所有依赖都要从源码编；2027-09 起不能再用** [60][61]；两套 Homebrew 容易串路径，博客作者就踩过 `hash -r` 的坑 [20] |
| B. MacPorts + `build_arch x86_64` | macports-wine overlay；`build_arch x86_64` 写在 `macports.conf` 里（overlay README 的要求），Portfile 自身声明 `supported_archs x86_64` | **WineHQ 官方包（Gcenx）** [3][5][6] | 已在生产环境验证，overlay 里已有 wine-stable 11.0、wine-devel 11.18、winetricks、gstreamer.framework 等 port | 要装 MacPorts，全部源码构建，速度慢 |
| C. 自建配方 + 原生交叉编译 | 原生 arm64 的 clang 加 `-arch x86_64`，autotools 用 `--host=x86_64-apple-darwin`，CMake/Meson 用 `CMAKE_OSX_ARCHITECTURES=x86_64` 或 cross file | CodeWeavers 源码包自带 FreeType、GnuTLS、SDL 等源码（见 01、02 号报告）[推断：CX 自建依赖] | **可复现**；编译器原生运行，速度快；**同一套配方加 `-arch arm64` 就能产出 ARM64 引擎** | 初期要写配方（约 10–15 个库） |
| D. Nix（nixpkgs x86_64-darwin，固定版本） | 把依赖扁平地放进 `Wine/lib`，再改写路径 | dappermint [19] | 可复现 | 引入 Nix；x86_64-darwin 在 nixpkgs 中的长期支持状态**未核实** |

**推荐**：P0 阶段用 **B（MacPorts）快速拿到可运行的构建**，并以 Gcenx 的 Portfile 作为参数基线；从 P1 开始换成 **C（自建配方）**，与 ARM64 路线共用同一套配方。Wine 本体继续用上游验证过的 `arch -x86_64 configure/make`。另一种做法是走交叉编译：先原生构建 Wine 的宿主工具，再用 `--host=x86_64-apple-darwin --with-wine-tools=<dir>` 编 Wine。这能让编译器原生运行，但 Wine 在 aarch64-darwin 上只构建工具是否顺畅，**没有验证过**，只能作为优化实验。

#### 1.4 依赖清单（Mach-O 宿主侧）

Wine 已经在 `libs/` 里自带了一批 PE 侧的库：`faudio`、`ffmpeg`、`vkd3d`、`jpeg`、`png`、`tiff`、`zlib`、`lcms2`、`mpg123`、`gsm`、`fluidsynth`、`xml2`、`xslt`、`sqlite3`、`symcrypt`、`icu*`、`capstone`、`musl`、`unwind`、`c++`/`c++abi` 等 [23]。**这些不需要 Cider 另外提供。** 真正需要以 Mach-O dylib 形式提供的宿主侧依赖如下：

| 依赖 | 用途 | 来源建议 | 备注 |
|---|---|---|---|
| FreeType | 字体 | 自建 | 各家都启用 [6][8] |
| GnuTLS（依赖 nettle、gmp、libtasn1、libunistring 等） | secur32 和 bcrypt 的 TLS | 自建，依赖链最长 | 上游已内置 symcrypt，GnuTLS 能否在未来被替代**未核实** |
| SDL2 | 手柄（winebus） | 自建（CMake） | configure 检测的是 SDL2，不是 SDL3（03 号报告） |
| MoltenVK | Vulkan → Metal | 用 Khronos 发布的预编译包 | v1.4.2（2026-07-24），最低 macOS 12 [30]；Mythic 设置 `ac_cv_lib_soname_MoltenVK=libMoltenVK.dylib`、`ac_cv_lib_soname_vulkan=""`，直接加载 MoltenVK 而不走 loader [8] |
| GStreamer | winegstreamer 与媒体播放 | 两种做法：Gcenx 要求用户在全局装 **GStreamer.framework 1.28.5** [4]（overlay README 里的 gstreamer.framework port 仍写着 v1.28.1，与发布说明不一致 [5]）；Mythic 从 Homebrew 挑约 19 个插件打包进来 [9] | **Cider 应当内置**（不应要求用户装 framework）。Wine 11 另有基于内置 FFmpeg 的 winedmo 路线，默认值见 03 号报告 |
| libinotify（kqueue） | 文件变更通知 | 自建 | Gcenx 启用 `--with-inotify` [6] |
| CUPS、pcap、OpenCL、CoreAudio | 打印、网络、计算、音频 | **系统 SDK 自带** | 不需要打包 |
| Kerberos | krb5 | Gcenx 做成可选 variant [6] | P2 再考虑 |

#### 1.5 configure 参数对照（参考基线）

| 选项 | Gcenx wine-devel 11.18 [6] | Mythic（CX 24 系）[8] | winecx-dist（CX 25）[18] |
|---|---|---|---|
| 架构 | `--enable-archs=i386,x86_64 --enable-win64` | `--enable-archs=i386,x86_64` | `--enable-win64 --enable-archs=i386,x86_64` |
| PE 编译器 | `--with-mingw=<llvm-mingw>/bin/clang` | `--with-mingw`（gcc：`i386_CC=i686-w64-mingw32-gcc` 等） | mingw（ccache clang） |
| 图形 | `--with-vulkan --without-opengl --without-x` | `--with-vulkan --with-opengl --without-x` | vulkan；关闭 x 和 opengl |
| 媒体 | `--without-gstreamer --without-ffmpeg`（gstreamer 作为默认 variant 另行启用） | `--with-gstreamer` | 关闭 gstreamer |
| 其他 | `--with-coreaudio --with-cups --with-freetype --with-gnutls --with-sdl --with-pcap --with-opencl --with-inotify`，关闭 alsa/pulse/dbus/udev/usb/v4l2/sane/gphoto/krb5 等 | 基本相同，外加 `--disable-tests` | 关闭 tests 和 winedbg |
| 部署目标 | 宿主为 macOS 15+ 时 `macosx_deployment_target 14.0`；发布说明写的是 Catalina+ [4][6] | `MACOSX_DEPLOYMENT_TARGET=10.15`，外加 Xcode 13 toolchain 和 12.3 SDK（CX 24 需要旧工具链） | 10.15 |

**注意**：Gcenx 的 wine-devel 默认 `--without-opengl`。Cider 如果需要 wined3d 的 GL 路径（04 号报告）或 32 位 GL 程序，应当显式打开 `--with-opengl`。**[已证实 Portfile；影响属推断]**

#### 1.6 CrossOver 源码的构建

- `https://media.codeweavers.com/pub/crossover/source/crossover-sources-<ver>.tar.gz` 解压后得到 `sources/wine/configure`。winecx-dist 在 Intel runner 上用 Homebrew（bison、ccache、gettext、mingw-w64、pkgconfig、freetype、gnutls、libpcap、sdl2）构建 CX 25，设置 `MACOSX_DEPLOYMENT_TARGET=10.15`、`CROSSCFLAGS="-O2 -Wno-incompatible-pointer-types"`、`make -j$(sysctl -n hw.ncpu)`，然后 `install-lib`，签名后打成 `winecx-<ver>-osx64.tar.gz` 放进 Heroic 的 tools 目录 [18]。**[已证实]**
- wineforge 在 `macos-15-intel` 上构建 CX 24.0.7 和 25.1.1。它的做法：用 SHA-256 固定源码，拒绝不安全的归档路径，按版本和目标打补丁，产出 `build-info.json`、SPDX 2.3 SBOM 和 GitHub attestation，job 超时设为 360 分钟 [16][17]。**[已证实]**
- Gcenx overlay 里的 `crossover` port（26.3.0）**只是重新打包 CodeWeavers 的试用版二进制**：它删掉 Sparkle 的更新 URL 和部分 DXVK dll，并不从源码构建 [7]。**[已证实]**
- CX 19 到 22 时代的 wine32on64 需要定制 clang。CX 25 和 26 的 configure 中已经没有它，所以**构建 CX 26 不再需要定制 LLVM**（02 号报告）。

#### 1.7 打包 dylib：rpath、install_name_tool 与签名

- **Mythic 的 `dylib_bundler.zsh`** [9] 做法如下：
  1. 用 `otool -L` 做广度优先遍历，收集非系统的 dylib（跳过 `/usr/lib` 和 `/System`），并把 `@rpath/…` 解析回 Homebrew 路径。
  2. 用 `cp -L` 复制到 `Engine/wine/lib`（GStreamer 插件复制到 `lib/gstreamer-1.0`）。
  3. 执行 `install_name_tool -id @loader_path/<name>`，并用 `-change` 把依赖路径改写成 `@loader_path/`。
  4. **每改完一个文件就立即 `codesign -fs-` 做 ad-hoc 重签**，因为 install_name_tool 会破坏原有签名。
  5. 对 `lib/wine/x86_64-unix/*.so` 中引用了 `/usr/local` 或 `/opt/homebrew` 的项，统一改成 `@rpath/`。

  链接时再用 `LDFLAGS=-Wl,-headerpad_max_install_names -Wl,-rpath,@loader_path/../../ -Wl,-rpath,@loader_path/../../external` 预留 rpath [8]。**[已证实]**
- Gcenx 在 post-destroot 阶段对 `winedmo.so` 和 `winegstreamer.so` 执行 `install_name_tool -delete_rpath <prefix>/lib`，再做 ad-hoc 重签 [6]。**[已证实]**
- **dlopen 的搜索规则**（本机 `man dlopen`）[本机]：
  - 对只给叶子名的调用（例如 Wine 执行 `dlopen("libfreetype.6.dylib")`），依次搜索 `DYLD_LIBRARY_PATH`、**调用者或主程序的 LC_RPATH**、当前目录（仅限不受限进程）。
  - 最后一步只对“old binaries”生效：设置了 `DYLD_FALLBACK_LIBRARY_PATH` 就搜索它；**没有设置时，dyld 会去 `/usr/local/lib`（仅限不受限进程）和 `/usr/lib` 里找**。man page 没有说明怎样才算“old binary”（例如以哪个部署目标或 SDK 为界）。**[未核实]**
  - 原文明确写着：“If the main executable is … codesigned with entitlements, then all environment variables are ignored, and only a full path can be used.”

  **工程结论**：Cider 引擎里所有 dylib 都应通过 **LC_RPATH（`@loader_path/…`）或 `@rpath/` 形式的 soname** 找到，不能依赖 `DYLD_*` 环境变量；否则一旦 wine 加载器带上 entitlements，库就会加载失败。**更正（事实核查）**：此前写的“没有设置 fallback 时，用户 `/usr/local/lib` 里的 Homebrew 库不会被误加载”不成立。对不受限的“old binary”，不设 fallback 时 dyld 恰恰会搜索 `/usr/local/lib`。能把它挡在外面的是：用现代部署目标构建（不再算 old binary），让依赖都能经 `@rpath`/`@loader_path` 在前面的步骤里找到，或者进程本身是受限的。Cider 的引擎不开 hardened runtime，属于不受限进程，所以不能指望“受限”这一条。**建议**：引擎用现代部署目标构建（Gcenx 是 14.0 [6]；Mythic 和 winecx-dist 用的 10.15 是否算 old binary **未核实**）；冒烟测试中用 `DYLD_PRINT_LIBRARIES=1`（`man dyld`）列出实际加载的镜像，一旦出现 `/usr/local/lib` 或 `/opt/homebrew` 路径就判为失败。**[推断]**

#### 1.8 构建时间数据点

| 场景 | 数据 | 来源 |
|---|---|---|
| M2 Mac，只构建 64 位 Wine（i386 尚未构建） | 作者原话 “stepped away for less than 15 minutes and it was done”（2025-12-21） | [20] [二手] |
| dappermint 的自托管 runner，i386+x86_64，ccache 已预热 | 约 15 分钟 | [19] [二手，硬件不详] |
| wineforge，macos-15-intel，完整构建 | job 超时设为 360 分钟（只说明预算，不代表实际耗时） | [17] |
| M1 3 核 7 GB 的 GitHub runner，冷构建、i386+x86_64、Rosetta 下 | **估计 60–120 分钟；ccache 命中后约 15–30 分钟** | [推断，需首次 CI 实测] |

---

### 2. 打包：.app 布局、引擎、DMG、签名、公证、entitlements、Sparkle

#### 2.1 同类产品怎么放 Wine

| 产品 | Wine 放在哪里 | 版本与更新方式 | 签名情况 |
|---|---|---|---|
| CrossOver | `CrossOver.app/Contents/SharedSupport/CrossOver/…`（02 号报告：`lib64/apple_gptk/`、`wine/x86_64-unix` 等） | 整个 App 随 Sparkle 一起更新（01 号报告） | 整包公证。wineloader 的 entitlements 未能核实，见“未解问题” |
| Whisky（2025-05-11 归档）[12] | `~/Library/Application Support/com.isaacmarovitz.Whisky/Libraries/Wine/bin` | 从 `https://data.getwhisky.app/Wine/WhiskyWineVersion.plist` 读取 `version: SemanticVersion`，本地版本较低时下载 tar 并**先删后解压**，**不保留旧版本** [11] | App 的 entitlements 为 apple-events、allow-unsigned-executable-memory、audio-input、camera [13] |
| Mythic | Engine 是一个 tar.xz（`wine/`、`dxvk/`、GPTK redist、`Properties.plist`，内含 SemVer 3.0.0）[8] | 引擎仓库已从 MythicApp/Engine 迁到 MythicApp/wine（2025-12-25 归档）[10] | 打包进来的 dylib 做 ad-hoc 签名 [9]；App 的 entitlements 为空 dict |
| Sikarugir | 每个 wrapper 带一个引擎，例如 `WS12WineSikarugir11.0`、`WS12WineCX24.0.7_7` | `EngineList.txt` 加 GitHub Releases 上的 tar.xz（Engines v1.0 共 45 个资产，单个 55–162 MB）[14][15] | Creator 用 `codesign --deep --force --sign -` 做 ad-hoc 签名 [5] |
| Heroic（macOS） | 用户在 tools 目录里选择 | 直接读取 GitHub Releases API：`Heroic-Games-Launcher/wine-crossover`、`Gcenx/macOS_Wine_builds`、`Gcenx/game-porting-toolkit` [66] | Electron App 的签名与 Wine 无关 |

#### 2.2 推荐的 Cider 布局

```
Cider.app/Contents/
  Info.plist                    # SUFeedURL、SUPublicEDKey、NSMicrophoneUsageDescription 等
  MacOS/Cider                   # SwiftUI 主程序：hardened runtime + 少量 entitlements
  Helpers/ciderctl              # 可选：CLI（-i com.cider.ciderctl，单独签名）
  Frameworks/Sparkle.framework  # 由内向外签名
  Frameworks/CiderKit.framework # 核心逻辑（SwiftPM 产物）
  Resources/                    # 图标（Assets.car + AppIcon.icns）、本地化资源
~/Library/Application Support/Cider/
  Engines/<engine-id>/          # 例如 cider-wine-11.0-c3-x86_64
    manifest.json               # 已验签的引擎元数据
    bin/wine  bin/wineserver
    lib/wine/{x86_64-unix,x86_64-windows,i386-windows}/
    lib/*.dylib                 # 通过 @loader_path / @rpath 定位
    share/wine/{mono/wine-mono-X,gecko/wine-gecko-X-x86{,_64}}/   # 共享解包，不再在每个前缀里安装
  Components/{dxmt,moltenvk,dxvk}/<ver>/
  Bottles/<name>/               # WINEPREFIX 与 cider-bottle.json（固定 engine 和 component 版本）
  Cache/  Logs/
```

理由：
1. **不要把 Wine 放进 App bundle**。放进去就意味着每个 Mach-O 都要用 Developer ID、hardened runtime 和 timestamp 签名，App 每次都要整包公证，而且 hardened runtime 对 Wine 的影响还没有验证（见 2.4）。另外 bundle 内容被 `CodeResources` 封存，运行时往引擎目录添加 DXVK 或 D3DMetal 会破坏签名。
2. Apple 要求每个代码位置“must contain a flat list of code content”，并建议不要再嵌套子目录，否则“might fail later in hard-to-debug ways”[35]。“目录名不能带点号”只是附加条件：如果执意嵌套，codesign 会把名字里带点号的目录当作 bundle，所以不能用点号。（**更正**：原文把点号规则写成了无条件要求。）Wine 的 `lib/wine/x86_64-unix/*.so` 多层结构正是 Apple 不建议的嵌套形式，所以“不把 Wine 放进签名 bundle”的结论不变。
3. 由 App 自己下载的引擎默认没有 quarantine [42]；Rosetta 下的 x86_64 代码不要求签名 [38]。所以引擎的完整性交给 Cider 自己的 Ed25519 清单和 SHA-256 来保证。
4. mono 和 gecko 解包后放在引擎内共享。Wine 的 `appwiz.cpl/addons.c` 会先搜索 `INSTALL_DATADIR/wine/`，然后才去下载 [24]。Gcenx 的包就是这样节省每个前缀的体积的 [4]。

#### 2.3 签名规则（Apple 一手文档）

- **只能用 Developer ID Application 证书**（pkg 用 Developer ID Installer）；其他证书的签名会导致公证失败 [34]。Apple Developer Program 年费 **$99** [41]。
- **必须**：给可执行文件开 hardened runtime（`--options runtime`）；签名带 secure timestamp（`--timestamp`，服务器只能是 `timestamp.apple.com`）；不能有 `com.apple.security.get-task-allow`；entitlements 文件必须是 ASCII 编码的 XML plist（二进制 `bplist` 会被拒）；链接的 SDK 不能早于 10.9 [33][34]。
- **Quinn 的签名流程** [36]：
  - 不要用 `--deep`，由内向外签名。
  - framework 和 dylib 的签法：`codesign -s "Developer ID Application" -f --timestamp`，不加 entitlements，也不加 runtime。
  - 主可执行文件加 `-o runtime --entitlements`。
  - 不在 bundle 结构里的工具用 `-i <identifier>` 指定标识。
  - 复制文件用 `ditto`，以保留 framework 的符号链接。
- **什么算“代码”**：Apple 的定义是 “executable code means a Mach-O image”；脚本没有地方放签名，按资源处理 [35]。据此推断：**PE 格式的 DLL 和 EXE 对 Apple 是资源**，放在 bundle 里只会被 `CodeResources` 哈希封存，不需要也无法单独签名。**[推断]**
- **公证工具** [33][39]：
  - `notarytool submit <file> --key <p8> --key-id <id> --issuer <uuid> --wait`；CI 用 App Store Connect API key 认证，本地可以用 `--keychain-profile`。
  - `notarytool log` 可以取回 JSON 格式的日志。
  - 只接受 UDIF DMG、签过名的 flat pkg 和 zip。
  - 公证完成后执行 `stapler staple` 装订 DMG。
- **公证耗时**：Apple 称“通常少于 1 小时”[33]。另有 2026 年 1–2 月的 DTS 论坛帖，一个压缩后 100 GB 以上的 App 公证用了 3.5–4.5 小时 [40]。Cider 的 GUI 本体应控制在几十 MB，引擎不进公证包，所以耗时不是问题。**[推断]**

#### 2.4 Entitlements：GUI 与 Wine 进程分开考虑

| 进程 | 建议 | 依据 |
|---|---|---|
| `Cider`（GUI，公证） | hardened runtime；`com.apple.security.device.audio-input` 和 `com.apple.security.device.camera`（配合 Info.plist 里的用途说明），视需要加 `automation.apple-events`；**不要**加 `disable-library-validation`，也不要加 `allow-dyld-environment-variables` | Whisky 的 entitlements 就包含 audio-input 和 camera [13]。子进程 wine 访问麦克风或摄像头时，TCC 会把 Cider 当作 responsible process 来归属 **[推断，需实测]** |
| `wine`、`wineserver`（引擎，放在 bundle 外，不公证） | **不开 hardened runtime**；x86_64 版本 ad-hoc 签名或不签名都可以 | 翻译执行的 x86_64 代码可以完全不签名 [38]。hardened runtime 带来的库校验和“忽略 DYLD 环境变量”这两条限制都不会出现 [本机 man dlopen] |
| 如果将来要把引擎签名并公证 | 至少需要 `com.apple.security.cs.allow-unsigned-executable-memory`（Wine 会 mmap 带 PROT_EXEC 的 PE 映像，还有 Windows 程序自己的 JIT）；需要 `disable-library-validation`，因为 D3DMetal.framework、GStreamer 和用户替换的 dylib 签名团队不同；DYLD 相关的 entitlement 按需添加。`disable-executable-page-protection` 是“extreme entitlement”，它的效果涵盖 allow-unsigned-executable-memory，但不包括 disable-library-validation [37] | **[推断]** CrossOver 的实际配置可以直接查（见“未解问题”第 1 条） |
| ARM64 引擎（P2） | 需要 **Developer ID 签名 + provisioning profile**，才能携带 `com.apple.developer.cross-architecture-support` 之类的受限 entitlement；ad-hoc 签名会被 AMFI 直接 SIGKILL | 06 号报告的本机实验；dappermint 的 README 说该 entitlement 由 Apple 酌情授予 [19] |

**关键推论**：x86_64 引擎可以由社区自行构建并 ad-hoc 签名后运行。ARM64 引擎一旦依赖受限 entitlement，**就只有 Cider 官方 CI 用团队证书签出来的构建能用**，签名流水线必须覆盖引擎，而不能只覆盖 GUI。**[推断]**

#### 2.5 DMG

- Sparkle 推荐使用 APFS 格式、lzfse 压缩的 DMG [47]，对应 `hdiutil create -fs APFS -format ULFO`。流程：先签 App，再用 Developer ID 给 DMG 签名，然后 `notarytool submit`，最后 `stapler staple`。**[已证实 Sparkle 建议；命令为常规用法]**
- 本机的 bsdtar 3.5.3 带 liblzma，能直接处理 **.tar.xz**，但**不支持 zstd**（调用 zstd 时报 “Can't launch external program”）。系统自带 `/usr/bin/aa`（Apple Archive，支持 lzfse）[本机]。所以**引擎包用 .tar.xz 最稳**（Gcenx、Mythic、Sikarugir 都这么做）；如果想用 zstd，必须把解压库编进 Cider。

#### 2.6 Sparkle 2

| 项目 | 事实 |
|---|---|
| 最新版本 | **2.10.0 “Golden Gate Bump”**，发布于 2026-09-13：最低部署目标改为 **macOS 12.0**（原因是 Xcode 27 不再支持更旧的部署目标）；移除 CocoaPods；修复 macOS 27 上 delta 更新重新应用文件系统压缩的问题。发布资产为 `Sparkle-2.10.0.tar.xz` 和 `Sparkle-for-Swift-Package-Manager.zip` [44]；2.10.0 tag 的 `Package.swift` 声明 `platforms: [.macOS(.v12)]`，并以 binaryTarget 指向该 zip；Atom feed 显示创建于 2026-09-13（UTC），2026-09-14T00:36Z 有过一次更新 [80] |
| 安全模型 | `bin/generate_keys` 生成 EdDSA 密钥并存入钥匙串，公钥写进 `SUPublicEDKey`；更新源由 `SUFeedURL` 指定，必须是 HTTPS；可以用 `SURequireSignedFeed` 要求 feed 本身签名；`CFBundleVersion` 必须递增 [45] |
| 发布 | `generate_appcast <dir>` 自动完成 EdDSA 签名、delta 生成和 release notes（.html/.md）处理；支持 channel 和 `sparkle:phasedRolloutInterval` 分批推送；归档格式支持 .dmg、.zip（用 `ditto -c -k --sequesterRsrc --keepParent` 保留符号链接）、.tar.*（加 `--no-xattrs`）和 .aar（需 Sparkle 2.7+、macOS 10.15+，并开启 `SUVerifyUpdateBeforeExtraction`）[47] |
| Delta | 格式 1 到 4，格式 4 需要 Sparkle 2.7+；`BinaryDelta create/apply`；bundle 带 ACL 或写在错误位置的签名 xattr 会被拒；Sparkle 可执行文件必须保持 0755 权限；可选属性 `sparkle:deltaFromSparkleExecutableSize` 和 `sparkle:deltaFromSparkleLocales` 用来防止因瘦身导致的 delta 失败 [46] |
| 集成 | 支持 SPM、Carthage 或手动；工具在 `../artifacts/sparkle/Sparkle/bin/` 下；手动集成时 rpath 设为 `@loader_path/../Frameworks` [45] |
| 先例 | CrossOver、Whisky、Mythic 都用 Sparkle（01 号报告，以及 [12]） |

**用法建议**：Sparkle 只负责更新 `Cider.app`。App 本体小，delta 收益有限，但照样开启。appcast 放在 GitHub Pages，DMG 放在 GitHub Releases（单个资产上限 2 GiB，没有带宽限制）[58]。EdDSA 私钥存为 GitHub Actions 的 environment secret，并且只在打 tag 的 release job 里读取。

---

### 3. CI：GitHub Actions 与自托管

#### 3.1 macOS runner 现状（2026-09）

| 标签 | 架构 | 规格 | 价格 | 状态 |
|---|---|---|---|---|
| `macos-latest`、`macos-26`、`macos-15` | arm64 | **3 核（M1）、7 GB 内存、14 GB SSD** | 公开仓库免费；私有仓库 $0.062/分钟 | GA [53][54] |
| `macos-26-intel`、`macos-15-intel` | x64 | 4 核、14 GB 内存、14 GB SSD | 同上 | GA [48][53] |
| `macos-26-xlarge`、`macos-15-xlarge` | arm64 | M2 Pro（价格文档）；5 核为推断 | $0.102/分钟 | **公开仓库也收费**：价格文档原文为 “The larger runners are not free for public repositories.” [54] **[已证实]** |
| `macos-26-large` | x64 | 12 核 Intel | $0.077/分钟 | 同属 larger runner，公开仓库也收费 [54] |
| `xcode-27`、`xcode-27-xlarge` | arm64 | — | — | Preview [50] |
| `macos-14` | 两种都有 | — | — | 2026-07-06 开始弃用，2026-11-02 起完全不支持；runner-images 已标为 Deprecated [51][78] **[已证实]** |

- 镜像内容（`macos-26` arm64，版本 20260907）：macOS 26.6.2（25G83）；Xcode 从 26.0.1 到 26.6 共 7 个版本，默认 **Xcode 26.6（17F113）**；CLT 26.6.0.0.1781586589（与开发机相同）；Homebrew 6.0.22；pkgconf 3.0.7、CMake 4.4.3、Ninja 1.13.2、Homebrew llvm@20、GNU tar 1.35。**没有 bison、ccache、meson、mingw**；软件清单里没有列出 Rosetta，但镜像构建时已安装（见下文）[51][75]。Intel 版 `macos-15`：macOS 15.7.9，默认 Xcode 16.4，同时带 26.0.1 到 26.3 [52]。
- **Intel 的退场时间**：macOS 13 镜像已于 2025-12-04 退役，GitHub 当时新增了 `macos-15-intel`，并称“GitHub will no longer support this architecture on macOS after the macOS 15 runner image is retired in Fall 2027”[49]。但 2026-02-26 的公告又推出了 `macos-26-intel` [48]。**按保守口径，Cider 不应长期依赖 Intel runner。**
- **arm64 runner 上的 Rosetta**：镜像软件清单里没有这一项 [51]，但 runner-images 的 Packer 模板 `images/macos/templates/macOS-26.arm64.anka.pkr.hcl` 在一个 provisioner 里依次执行 `install-xcode-clt.sh`、`install-homebrew.sh` 和 `install-rosetta.sh`，macOS-15 arm64 模板同样引用了 `install-rosetta.sh`。这个脚本调用的是 `/usr/sbin/softwareupdate --install-rosetta --agree-to-license` [75][76]。**[已证实：模板源码]** 所以 GitHub 标准 arm64 macOS runner **应已预装 Rosetta**，“能否装上 Rosetta”不再是风险（**更正**：原文把它列为首个 CI job 要验证的开放风险）。上游 Wine 用的是自己预装好的 Tart 镜像 [2]。**建议**：首个 CI job 仍然先跑一步 `arch -x86_64 /usr/bin/true`（或 `arch -x86_64 uname -m`）作为冒烟检查；如果失败，再兜底执行一次 `sudo softwareupdate --install-rosetta --agree-to-license`。**[推断]**
- **DXMT 的 CI 可以作为参考** [28]：`runs-on: macos-15`，`xcode-select -s /Applications/Xcode_16.1.app`，从源码构建 LLVM 15.0.7 并缓存，llvm-mingw 用 `ucrt-macos-universal` 包，Meson 使用 cross file，打 tag 时生成 build provenance attestation。

#### 3.2 缓存与构建分钟数

- **缓存额度**：每个仓库免费 10 GB，默认条目最后一次访问后保留 7 天，按 LRU 淘汰；Pro、Team、Enterprise 账户可以付费扩容 [56]。
- **ccache**：
  - Wine 的 unix 侧通过 `CC="ccache clang"` 接入；PE 侧设置 `x86_64_CC="ccache x86_64-w64-mingw32-clang"` 和 `i386_CC="ccache i686-w64-mingw32-clang"`。Mythic 用同名变量指定 gcc [8]，winecx-dist 用 `ccache clang` [18]。
  - 缓存 key 建议写成 `ccache-${engine_line}-${toolchain_hash}`，restore-keys 退回到同一条引擎线，与上游的 `$CI_JOB_NAME-ccache` 思路一致 [2]。
  - i386+x86_64 的 ccache 体积估计 2–4 GB。**[推断]**
- **configure 缓存**：上游用 `-C` 加缓存的 `config.cache` [1][2]。
- **分钟数估算**：公开仓库的标准 runner 不计费 [55]。如果仓库转为私有，一次冷构建约 90 分钟，按 $0.062/分钟约 $5.6；缓存命中时约 $1–2。**[推断]**
- **大对象的放置**：LLVM 15（DXMT 用）、预编译的依赖前缀和 Wine 宿主工具都应作为 **release 资产或 cache** 复用，不要每次重新构建。

#### 3.3 在开发机上跑自托管 runner

- GitHub 官方的说法：“Self-hosted runners should almost never be used for public repositories”，因为任何人都能提交 PR，在你的机器上执行代码 [57]。自托管 runner 的平台费已推迟 [55]。
- 开发机只有 8 GB 内存，既要开发又要跑 CI，会互相拖慢。**建议**：
  1. 默认只用 GitHub-hosted runner；
  2. 本机只保留一个 `workflow_dispatch` 触发的私有“签名与公证”兜底 job，或者干脆用本地脚本；
  3. 需要稳定硬件时，考虑租用 Mac mini（不在本次调研范围）。

#### 3.4 构建矩阵建议

```
engine-build.yml   (手动触发 / 推送 recipes/**)
  matrix:
    line:   [wine-11.0-stable, wine-devel(11.x), cx-26.3]   # 引擎线
    arch:   [x86_64]            # P2 加 arm64（FEX/ARM64EC）
    runner: [macos-26]          # arm64 + Rosetta；Intel 只作临时兜底
  steps: deps(prefix cache) -> wine(configure -C, ccache) -> components(dxmt, moltenvk 预编译)
         -> bundle-dylibs -> smoke-test(wineboot --init, cmd /c ver, syswow64\cmd.exe)
         -> package(tar.xz + manifest.json) -> sha256 + attest -> release(draft)
app-build.yml      (每个 PR)
  xcodebuild/swift build + test + swiftlint
app-release.yml    (tag)
  sign(inside-out) -> dmg -> notarytool --wait -> staple -> generate_appcast -> publish
```

wineforge 的冒烟测试用 `syswow64\cmd.exe` 验证 32 位路径 [16]。这一步应当加进 Cider 的冒烟测试，因为上游 CI 不覆盖 macOS 上的新 WoW64。

---

### 4. 引擎与组件更新管线

#### 4.1 各家做法对比

| 项目 | 清单格式 | 完整性校验 | 多版本并存 | 按容器选引擎 |
|---|---|---|---|---|
| Whisky | plist，只有一个 `version` | 无（只靠 HTTPS） | 否（先删后装） | 否 [11] |
| Mythic | Engine 包内的 `Properties.plist`（SemVer） | 无 | 否 | 否 [8] |
| Sikarugir | `EngineList.txt` 列出名字，资产在 GitHub Releases | GitHub 管理 | 是 | 每个 wrapper 一个引擎 [14] |
| Heroic | 直接调用 GitHub Releases API | GitHub 管理 | 是 | 每个游戏单独选 [66] |
| Bottles | YAML：`Name`、`Provider`、`Channel`、`File[file_name, url, file_checksum(MD5), rename]`、`Post`；`index.yml` 自动生成，含 `Category`、`Channel`、`Date`；CI 自动从上游 workflow 拉取组件 | MD5 | 是 | **每个 bottle 分别记录 `Runner`、`DXVK`、`VKD3D`、`NVAPI`、`LatencyFleX`** [67][68] |
| Lutris | JSON：`versions[{version, architecture, url, default}]` | — | 是 | 每个游戏单独选 [69] |
| wineforge | `*.runtime.json`（归档摘要、目标、Wine 路径、能力），外加 SPDX SBOM 和 attestation | SHA-256 + attestation | — | — [16] |

#### 4.2 组件版本快照（截至 2026-09-26）

| 组件 | 最新版本与日期 | 备注 |
|---|---|---|
| Wine stable / devel | 11.0（2026-01-13）/ 11.18（2026-09-18） | 见 03 号报告；Gcenx 11.18 包发布于 2026-09-25 [4] |
| CrossOver 源码 | 26.3.0（2026-07-21） | 见 01、02 号报告 |
| DXMT | v0.80（2026-04-23）、v0.74（2026-03-10）、v0.73（2026-01-21）、v0.72（2025-12-11）；资产为 `dxmt-vX-builtin.tar.gz` | [29][81] |
| MoltenVK | v1.4.2（GitHub Release 2026-07-24；`Whats_New.md` 写的是 “Released 2026-07-20”），最低 macOS 12（工程文件 `MACOSX_DEPLOYMENT_TARGET = 12.0`）；资产包括 `MoltenVK-macos.tar` 和 `MoltenVK-macos-privateapi.tar` | [30][79] |
| DXVK-macOS（Gcenx） | 最后一版是 v1.10.3-20230507-repack（2024-07-23），**已停滞** | [31] |
| wine-mono | 11.3.0（2026-08-17，**首次提供 arm64 包**）；11.2.1（2026-09-04，给 Proton 的 bugfix） | [32][81]；Wine master 的 `MONO_VERSION` 为 11.3.0 [24] |
| wine-gecko | 2.47.4（Wine master 的 `GECKO_VERSION`） | [24] |
| GStreamer | Gcenx 11.18 要求 1.28.5 | [4] |
| llvm-mingw | 20260922（LLVM 23.1.2） | [26][81] |
| Sparkle | 2.10.0（2026-09-13） | [44] |

#### 4.3 固定版本与构建（配方）

- 每个引擎线维护一个配方，例如 `recipes/engines/wine-11.0.yaml`，写明：源码 URL 与 SHA-256（仿照 wineforge 的做法：先校验，再拒绝不安全路径）、补丁集（指向 `cider-wine` 仓库的 git tag 或 SHA）、configure 参数、依赖前缀的版本，以及 mono 和 gecko 的版本。
- **mono 和 gecko 的版本不要手写**：构建脚本从对应 Wine tag 的 `dlls/appwiz.cpl/addons.c` 里解析 `MONO_VERSION`、`GECKO_VERSION` 和 sha256，再去下载，避免版本对不上时 Wine 转而联网下载 [24]。
- 依赖前缀（第 1.3 节路线 C）单独做成一个版本化产物，例如 `cider-deps-x86_64-2026.09.1.tar.xz`，引擎配方引用它的 SHA-256，从而固定下来。

#### 4.4 清单格式建议（示例，非规范）

```json
{
  "schema": 1,
  "generated": "2026-09-26T00:00:00Z",
  "engines": [{
    "id": "cider-wine-11.0-c3-x86_64",
    "line": "wine-11.0", "version": "11.0-c3", "channel": "stable",
    "arch": "x86_64", "requires": {"macos": "14.0", "rosetta": true, "minApp": "0.4.0"},
    "base": {"wine": "wine-11.0", "patches": "cider-wine@<sha>"},
    "bundled": {"mono": "<from addons.c>", "gecko": "2.47.4", "moltenvk": "1.4.2"},
    "artifact": {"url": "https://github.com/<org>/cider-engines/releases/download/…/….tar.xz",
                 "sha256": "…", "size": 0},
    "attestation": "https://github.com/<org>/cider-engines/attestations/…",
    "yanked": false
  }],
  "components": [{"id": "dxmt-0.80-c1", "kind": "dxmt", "arch": "x86_64", "artifact": {"…": "…"}}]
}
```

- **签名**：整份 `index.json` 附带一个独立的 `index.json.sig`，用 Ed25519 签名。App 内置公钥，用 CryptoKit 的 `Curve25519.Signing.PublicKey.isValidSignature` 验证。可以复用 Sparkle 的 EdDSA 密钥，但**建议另外生成一把**，以隔离风险。**[推断]**
- **撤回**：`yanked: true` 的版本不再提供新安装，已安装的 bottle 会收到提示。
- **兼容性标记**：`requires.minApp` 防止旧 GUI 装上需要新 bottle schema 的引擎。

#### 4.5 按 bottle 固定引擎与回滚

- 每个 bottle 的 `cider-bottle.json` 记录 `engine`、`components`，以及 `engineHistory` 数组。
- **Wine 前缀的升级是隐式的**：`wineboot` 会比较 `wine.inf` 的 mtime 和前缀里 `.update-timestamp` 中记录的时间戳，不一致时自动升级前缀；文件内容为 `disable` 则跳过 [25]。因此给一个 bottle 换引擎、尤其是降级时，注册表和系统 DLL 的状态可能已经被新版本改过。**建议**：
  1. 换引擎前，先用 APFS clone（`clonefile()` 或 `cp -c`）给 bottle 做一个几乎零成本的快照，失败就还原；
  2. 保留最近 N 个引擎版本，由 GC 按引用计数回收；
  3. 不在后台自动升级 bottle 的引擎：新 bottle 默认用最新的 stable，已有 bottle 在 UI 里提示“可升级”。**[推断]**
- 这一点优于 CrossOver：CrossOver 的所有 bottle 共用 App 内置的 Wine，只能跟着整个 App 一起升级或回退（01 号报告）。

---

### 5. 仓库布局与 Swift App 构建系统

#### 5.1 参考

- **Proton**：单一主仓库，用 submodule 固定 wine、dxvk、vkd3d-proton 等（03 号报告）。
- **Whisky**：`Whisky.xcodeproj` 加上 `WhiskyKit`、`WhiskyCmd`、`WhiskyThumbnail`；Wine fork 放在另一个仓库 [12]。
- **Mythic**：App 仓库加 `MythicApp/wine`（fork，分支名如 `mythic-crossover-24.0.7-stable`，自带 CI）[8][10]。
- **Sikarugir**：组织下有 Sikarugir、Engines、Creator、Template、wine、dxvk、dxmt、MoltenVK、winetricks、homebrew-sikarugir 等仓库 [14]。
- **Bottles**：主仓库之外，另有 `components`、`dependencies`、`programs` 等数据仓库 [67]。

#### 5.2 建议（“主仓库 + 两个卫星仓库”）

| 仓库 | 内容 | 理由 |
|---|---|---|
| `cider`（monorepo） | `App/`（SwiftUI）、`Packages/CiderKit`（SwiftPM：bottle、引擎管理、清单验签、进程管理）、`Tools/ciderctl`、`recipes/`（依赖、引擎、组件配方）、`scripts/`（bundle-dylibs、sign、notarize、dmg）、`.github/workflows/` | 代码、配方和 CI 一起评审，原子提交 |
| `cider-wine` | Wine fork，每条引擎线一个分支（`cider/11.0`、`cider/devel`），补丁用 git 管理 | Wine 历史很大，放进主仓库会拖慢 clone，submodule 也会给贡献者添麻烦；配方里用 SHA 固定 |
| `cider-compatdb` | 兼容性数据库与安装配方（纯数据，另有签名通道） | 发布节奏与 App 独立（01 号报告指出，CrossOver 的配方过时是一个痛点），也方便社区 PR |
| （可选）`cider-dxmt` 等 fork | 只在必须带补丁时才建 | 其余组件直接用上游 release |

引擎的二进制产物放在 `cider` 仓库的 GitHub Releases，或者单独建一个 `cider-engines` 仓库只放 release。清单由 CI 生成并签名后，发布到 GitHub Pages。

#### 5.3 Swift 构建系统：SwiftPM 为主，App 外壳用 Xcode 工程

- **只用 CLT 能做的** [本机]：Swift 6.3.3 的 `swift build` 和 `swift test`。`CiderKit`、`ciderctl`，以及全部非 UI 逻辑（引擎下载与验签、bottle 管理、进程与日志）都可以放在 SwiftPM 包里，在开发机上直接开发和测试。
- **只用 SwiftPM 组装 App 的已知限制**：可以用 `swift build` 加脚本拼出 `.app`（例如 `tqbf/swiftui-app` 模板，它还包含签名、公证、装订），但它**明确写着 asset catalog 和 previews 不可用** [65]。
  - 本机 CLT 没有 `actool`、`ibtool` 和 `xcstringstool`。
  - macOS 26 的新图标（Icon Composer 生成的 `.icon`）必须用 Xcode 26 的 actool 编进 `Assets.car`；只提供 `.icns` 的 App 在 Tahoe 上会被塞进灰色 squircle [64]。
  - **结论**：可以用它做原型，但不适合面向用户的高质量发布。
- **Xcode 版本约束** [62]：
  - Xcode 26.6 需要 macOS 26.2 或更高，带 macOS 26.5 SDK 和 Swift 6.3。
  - **Xcode 27（及 27.1、27.2 beta）需要 macOS 26.6 或更高**，带 Swift 6.4。
  - 开发机是 26.5，**现在只能装 Xcode 26.x**；想用 Xcode 27，得先把系统升到 26.6.x（runner 镜像已是 26.6.2）。
- **建议**：
  1. 现在就在开发机装 **Xcode 26.6**（磁盘还剩 383 GB），外加 Metal toolchain；
  2. App 工程用 **XcodeGen 或 Tuist 从声明式 spec 生成**，或者用 Xcode 16 起的“buildable folders”，以减少 `.pbxproj` 冲突 **[推断]**；
  3. 业务逻辑一律放进 SwiftPM 包，Sparkle 用 SPM 引入 [45]；
  4. CI 上用 `xcodebuild -scheme Cider -configuration Release archive`，接着 `-exportArchive`，或者直接对产物走自定义的签名脚本。
- **最低系统版本**：Sparkle 2.10 和 MoltenVK 1.4.2 都要求 12 以上 [30][44]；DXMT 要求 Sonoma 以上 [27]；Wine 10 起直接 syscall 仿真要求 Sonoma 以上 [21]；CrossOver 27 也只支持 macOS 14 以上（01 号报告）。**建议 Cider 的最低版本定为 macOS 14.0。**

---

### 6. 8 GB Apple Silicon 上的开发工作流

#### 6.1 硬件与并行度

- 开发机是 M3，4 个性能核加 4 个效率核（`hw.perflevel0/1.physicalcpu` 均为 4），内存 8 GB [本机]。
- Wine 的 C 文件大多很小，单个 clang 进程占用约 100–300 MB，`-j6` 到 `-j8` 应该可行；链接大型 `.so` 和 PE 模块时注意内存压力（可用 `memory_pressure` 或活动监视器观察）。**[推断]**
- **不要在本机从源码编 LLVM 15**（DXMT 需要），8 GB 很可能会大量换页（02 号报告也这样判断）。改用 CI 编好的 release 资产。

#### 6.2 首次环境（建议顺序）

1. `softwareupdate --install-rosetta --agree-to-license`
2. 安装 Xcode 26.6（或者先升级系统到 26.6，再装 Xcode 27），然后执行 `xcodebuild -downloadComponent MetalToolchain`
3. 解压 llvm-mingw 的 `ucrt-macos-universal` 包，装 bison ≥3（arm64 版）、pkgconf，以及 meson、ninja（DXMT 要用）
4. 按路线 B 或 C 准备 x86_64 依赖前缀（第 1.3 节）
5. `ccache`：分别以 `CC`、`x86_64_CC`、`i386_CC` 接入，建议 `max_size=20G`

#### 6.3 增量构建单个 DLL

- 现代 Wine 由 makedep 生成**一个顶层 Makefile**，不能再像早年那样 `cd dlls/xxx && make`。重建单个模块时，可以直接以产物文件作为目标，例如 `make dlls/d3d11/x86_64-windows/d3d11.dll dlls/win32u/win32u.so`。**[推断：依据 `tools/makedep.c` 中按 `arch_dirs[arch]` 拼接模块路径的代码 [74]，需要实测确认目标名]**
- 迭代的办法：
  - 可以直接从构建目录运行 Wine（`./wine`，也可以设置 `WINEPREFIX`）；
  - 或者把新的 `.dll` 或 `.so` 复制到引擎目录下对应的 `lib/wine/<arch>-windows|unix/` 中，替换后重启 wineserver（`wineserver -k`）即可生效。**[推断]**
  - 打包前再完整执行一次 `make install-lib DESTDIR=…`（Gcenx、Mythic、winecx-dist 都这么做 [6][8][18]）。
- 上游 CI 的 build-mac 用 `git rebase --exec` 逐个提交构建 [2]。Cider 也可以对补丁栈做同样的逐提交构建检查，但只在 CI 上跑。

#### 6.4 在 Rosetta 下调试

- **WINEDEBUG** 是主要手段：`+seh,+loaddll,+pid,+timestamp`，必要时加 `+relay`，日志量很大。Cider 应当把每个 bottle、每次启动的 stderr 落到 `Logs/`，并在 UI 里提供开关。
- **lldb**：LLDB 能识别被翻译的进程，改用 Rosetta 自带的 debugserver（`/Library/Apple/usr/libexec/oah/debugserver`）[二手][70][71]。Wine 靠 SIGSEGV、SIGBUS、SIGUSR1 等信号实现异常处理和线程挂起，所以在 lldb 里要先执行 `process handle -p true -s false -n false SIGSEGV SIGBUS SIGUSR1 SIGUSR2`，否则每个 first-chance 异常都会停下来。**[推断]** Rosetta 下**不能设置硬件调试寄存器**（06 号报告，依据 `signal_x86_64.c`）。有报告称，翻译进程生成的 core dump 是 arm64（Rosetta 本身）格式 [二手]。
- **winedbg**：可以用 `winedbg --gdb`，或者用 `--no-start` 输出 gdb remote 地址，再让 gdb 或 lldb 连上作为前端 [二手]。只在 macOS 与 Rosetta 这个组合下是否好用**未验证**；部分发行构建会禁用 winedbg [18]。
- **get-task-allow**：公证构建里不能带这个 entitlement [34]。所以调试一律使用本地构建、不开 hardened runtime 的引擎；GUI 的 Debug 配置由 Xcode 自动注入该 entitlement。

#### 6.5 放本机还是放 CI

| 任务 | 本机 | CI |
|---|---|---|
| Swift GUI 与 CiderKit 开发、单元测试 | 是 | 每个 PR 都跑 |
| 单个 DLL 的修改与调试（ccache 已预热） | 是 | — |
| Wine 冷构建（i386+x86_64） | 首次跑通一次，理解流程 | **日常放 CI** |
| 依赖前缀、LLVM 15、DXMT、MoltenVK | 否 | 是（作为 release 资产缓存） |
| 签名、公证、DMG、appcast | 只在应急时 | 是（证书与 API key 放在 environment secret 里） |
| 引擎冒烟测试（wineboot、cmd、syswow64） | 按需 | 每次引擎构建都跑 |

---

## 对 Cider 的启示与建议（按优先级）

**P0（第 1–4 周：打通从源码到签名 DMG 的最小闭环）**
1. **定下架构**：`Cider.app`（公证，体积小）配合 `~/Library/Application Support/Cider/Engines/<id>`（独立下载，Ed25519 清单加 SHA-256，按 bottle 固定版本）。**不要把 Wine 放进 bundle。** 这个决定会影响签名、更新、回滚和 ARM64 路线。
2. **开发机环境**：装 Rosetta、Xcode 26.6 和 Metal toolchain、llvm-mingw、bison ≥3、pkgconf、ccache、meson、ninja，按第 6.2 节顺序进行。
3. **第一个引擎**：先用 Gcenx 的 macports-wine 配方（路线 B），基于 Wine 11.0 或 CX 26.3 源码，在本机或 CI 上跑通 `arch -x86_64 configure --enable-archs=i386,x86_64 --with-mingw=<llvm-mingw clang> --with-vulkan --with-opengl …`，再用改写过 rpath 的 dylib 打出 `.tar.xz`。冒烟测试包括 `wineboot --init`、`cmd /c ver` 和 `syswow64\cmd.exe /c ver`，并在 `DYLD_PRINT_LIBRARIES=1` 下确认没有从 `/usr/local/lib` 或 `/opt/homebrew` 加载任何库（dyld 对不受限的 old binary 会回退搜索 `/usr/local/lib`，见 1.7 节的更正）。
4. **GUI 工程骨架**：SwiftPM 包 `CiderKit` 加上 Xcode App 外壳（由 XcodeGen 或 Tuist 生成），集成 Sparkle 2.10（SPM）；部署目标定为 macOS 14.0。
5. **申请 Apple Developer Program（$99/年）**，拿到 Developer ID Application 证书，创建 App Store Connect API key 用于 notarytool。写好 `scripts/sign.sh`：由内向外签名，不用 `--deep`，只有主程序加 runtime 和 entitlements。

**P1（第 2–3 个月：让管线可复现、可回滚）**
6. **自建依赖配方（路线 C）**：用原生 arm64 编译器加 `-arch x86_64` 交叉编译 freetype、gnutls 依赖链、SDL2、libinotify 和 GStreamer 子集，产物版本化后由 SHA-256 固定。这样可以彻底摆脱 x86 Homebrew（2026-09 起不再出 Intel bottle，2027-09 起不能运行）。
7. **CI**：写 `engine-build.yml`（macos-26 + Rosetta + ccache + attestation + 清单签名）、`app-build.yml` 和 `app-release.yml`（签名、DMG、公证、staple、appcast），仓库保持公开，以享受免费的标准 runner。标准 arm64 镜像已预装 Rosetta [75]，第一步只需跑 `arch -x86_64` 冒烟检查，不必每次安装。larger runner（large/xlarge）在公开仓库也收费 [54]，默认不用。
8. **引擎管理器**：多版本并存；切换前做 APFS 快照；`yanked` 撤回；`minApp` 兼容性检查；GC 回收旧版本。mono 和 gecko 的版本从 `addons.c` 自动解析，共享安装。
9. **PE 工具链回归**：在 llvm-mingw 构建上跑 Steam 登录冒烟测试，出问题时给 kernelbase 等个别模块准备 mingw-gcc 的 fallback [19]。
10. **验证 hardened runtime**：做一次实验，给引擎签上 Developer ID 和 hardened runtime（加 allow-unsigned-executable-memory、disable-library-validation），看 Wine、DXMT、D3DMetal 和 GStreamer 能否正常工作。这为将来“引擎也公证”或满足 ARM64 受限 entitlement 做准备。

**P2（第 4 个月以后：ARM64 与发布工程）**
11. 用同一套配方加 `-arch arm64` 和 llvm-mingw 的 arm64ec 目标，产出 ARM64 引擎（FEX）。签名改为 Developer ID 加 provisioning profile（如果 Apple 授予受限 entitlement，见 06 号报告），并由 CI 统一签名。
12. 引擎的增量更新：先用整包 tar.xz；如果带宽成为问题，再改成按文件内容寻址的分块存储。
13. 兼容性数据库独立成仓库，发布时用 Ed25519 签名，独立于 App 更新。

---

## 风险

1. **x86 生态提前断档（高）**：Homebrew 自 2026-09 起不再出 Intel bottle，2027-09 起不能在 Intel 上运行 [60][61]；GitHub 的 Intel runner 最早在 2027 年秋就可能没有了 [49]；Rosetta 的完整支持到 macOS 27 为止（06 号报告；Homebrew Support-Tiers 也转引了 Apple 的这一说法 [61]）。依赖 x86 Homebrew 或 Intel runner 的构建方案会在 12 个月左右失效。
2. **hardened runtime 与 Wine 的兼容性未知（中）**：只要走“引擎也公证”或“ARM64 受限 entitlement”这两条路，就必须给 Wine 开 hardened runtime，随之而来的是库校验、DYLD 环境变量被忽略、可执行内存受限等一系列问题，目前都没有公开的一手资料可查。
3. **受限 entitlement 只能由官方签（中高）**：如果 ARM64 引擎必须带 Apple 授予的 entitlement，社区自行构建的版本就跑不了 ARM64，开源项目的“可复现、可自建”会打折扣。
4. **PE 工具链差异（中）**：有单一来源报告，llvm 编译的 kernelbase 会破坏 Steam 登录 [19]。如果不及时发现，就会表现为难以定位的启动器故障。
5. **CI 资源上限（中）**：标准 arm64 runner 只有 3 核和 7 GB 内存，文档写的磁盘是 14 GB [53]；i386+x86_64 的构建目录加 ccache 可能超过可用空间（需要实测 `df -h`）；缓存默认 7 天过期，引擎线不活跃时会冷启动 [56]。
6. **GStreamer 打包的体积与稳定性（中）**：Gcenx 让用户自己装 framework 回避了这个问题 [4]。Cider 如果内置，就要维护插件清单和依赖闭包（Mythic 约 19 个插件）[9]，体积和出问题的面都会增加。
7. **Sparkle 与 Xcode 的版本线（低）**：Sparkle 2.10 已把最低版本提到 macOS 12，理由是 Xcode 27 [44]；Xcode 27 又要求 macOS 26.6 以上 [62]。开发机必须跟着升级系统，否则很快会用不上新工具链。
8. **下载的引擎没有 quarantine（低中）**：这让引擎能绕过 Gatekeeper 顺利运行，但也意味着完整性**完全靠 Cider 自己的签名校验**。清单验签如果写错，就会成为供应链漏洞。

---

## 未解问题

1. **CrossOver 26.x 的 wineloader 和 wineserver 用的是什么签名和 entitlements？开没开 hardened runtime？** 装一个 CrossOver 试用版后执行 `codesign -dvvv --entitlements - "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib/wine/…/wine"`（路径以实际为准）即可。这是决定 P1 第 10 条实验设计的最快途径。
2. 公证服务会不会展开 bundle 里的 `.tar.xz` 资源去检查其中的 Mach-O？如果要把一个离线用的默认引擎以压缩包形式放进 App，这一点决定能否通过公证、会不会被视为规避。
3. GitHub 所说的 Intel 退场时间（2027 年秋，[49]）与 2026-02 推出的 `macos-26-intel`（[48]）如何对应？`macos-26-intel` 的退役时间没有查到（事实核查时也未找到，仍未解决）。
4. ~~GitHub 标准 arm64 runner 能否用 `softwareupdate --install-rosetta` 装上 Rosetta~~ **已部分解决**：runner-images 的 arm64 Packer 模板在镜像构建时就执行了 `install-rosetta.sh`，Rosetta 应已预装 [75][76]。剩下的问题是：在 3 核、7 GB 的 runner 上，`arch -x86_64` 跑 Wine 的 configure、make 和冒烟测试是否稳定、耗时多少，需要首个 CI job 实测。
5. 在 aarch64-darwin 上能否只构建 Wine 的宿主工具（`--with-wine-tools` 交叉路线），能让构建快多少？
6. nixpkgs 对 x86_64-darwin 的长期支持状态（关系到 dappermint 那条路线能不能借鉴）。本次搜索配额用完，未核实。
7. `make <subdir>/<arch>-windows/<module>.dll` 这种单模块目标在当前 Wine 11.x 的 makedep 里的确切写法（第 6.3 节）。
8. Wine 11.x 在 macOS 上 winedmo（FFmpeg）与 winegstreamer 的默认后端。如果能完全不用 GStreamer，打包会大幅简化（03 号报告也列为未核实）。
9. ~~大型 runner（xlarge）在公开仓库上是否一定计费~~ **已解决**：GitHub 价格文档明确写着 “The larger runners are not free for public repositories.” [54]
10. dyld 对“old binaries”的判定界线（部署目标或 SDK 版本）是多少？以 10.15 为部署目标构建的 Wine 加载器会不会回退搜索 `/usr/local/lib`（第 1.7 节）？

---

## 参考来源

1. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/gitlab/build-mac — 上游 macOS CI 构建脚本（`arch -x86_64`、Xcode SDK、x86 Homebrew、arm64 bison、ccache）
2. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/gitlab/build.yml — `build-mac` job：Tart 镜像 `winehq-sequoia-pristine`，ccache 与 config.cache 缓存，`git rebase --exec`
3. https://github.com/Gcenx/macOS_Wine_builds 与 https://raw.githubusercontent.com/Gcenx/macOS_Wine_builds/master/README.md — WineHQ 官方 macOS 包 README（MacPorts + macports-wine overlay、llvm-mingw、Xcode，`--enable-archs=i386,x86_64`、`--with-mingw=/opt/local/libexec/llvm-mingw/bin/clang`）
4. https://github.com/Gcenx/macOS_Wine_builds/releases/tag/11.18 — 11.18（Atom feed 显示发布于 2026-09-25T15:27Z，已由事实核查确认年份）：gecko 2.47.4、mono 11.3.0、GStreamer.framework 1.28.5、Catalina+
5. https://github.com/Gcenx/macports-wine 与 https://raw.githubusercontent.com/Gcenx/macports-wine/master/README.md — overlay 说明（Apple Silicon 上需在 `macports.conf` 中设置 `build_arch x86_64`；gstreamer.framework 仍列为 v1.28.1）与 port 版本（wine-stable 11.0、wine-devel 11.18、crossover 26.3.0、sikarugir 1.0.1）
6. https://raw.githubusercontent.com/Gcenx/macports-wine/master/emulators/wine-devel/Portfile — `supported_archs x86_64`、configure.args（`--enable-archs=i386,x86_64 --enable-win64`）、`--with-mingw=<llvm-mingw clang>`、`os.major>=24` 时部署目标 14.0、delete_rpath 加 ad-hoc 重签
7. https://raw.githubusercontent.com/Gcenx/macports-wine/master/emulators/crossover/Portfile — CrossOver port 只重新打包二进制，去掉 Sparkle URL
8. https://raw.githubusercontent.com/MythicApp/wine/1eef26cddc2cf5677c2bcc6fd96d1041b454e21f/.github/workflows/build.yml — Mythic 引擎 CI（macos-15-intel、mingw-w64 v12、Xcode 13 toolchain、12.3 SDK、rpath 参数、tar.xz 与 Properties.plist）
9. https://raw.githubusercontent.com/MythicApp/wine/1eef26cddc2cf5677c2bcc6fd96d1041b454e21f/.github/dylib_bundler.zsh — dylib 递归收集、install_name_tool 改写、ad-hoc 重签
10. https://github.com/MythicApp/Engine — 2025-12-25 归档，开发迁到 MythicApp/wine
11. https://raw.githubusercontent.com/Whisky-App/Whisky/main/WhiskyKit/Sources/WhiskyKit/WhiskyWine/WhiskyWineInstaller.swift — WhiskyWineVersion.plist 的 URL、安装位置、SemVer 比较
12. https://github.com/Whisky-App/Whisky — 2025-05-11 归档；Xcode 工程结构；使用 Sparkle
13. https://raw.githubusercontent.com/Whisky-App/Whisky/main/Whisky/Whisky.entitlements — Whisky 的 entitlements（apple-events、unsigned exec memory、audio-input、camera）
14. https://github.com/Sikarugir-App 与 https://raw.githubusercontent.com/Sikarugir-App/Engines/main/EngineList.txt — 组织仓库列表与引擎命名
15. https://github.com/Sikarugir-App/Engines/releases — Engines v1.0：45 个 tar.xz，55–162 MB
16. https://github.com/wineforge/wineforge-engines — 可审计的 CX 引擎构建：SHA-256 固定、SPDX SBOM、attestation、runtime.json、syswow64 冒烟测试
17. https://raw.githubusercontent.com/wineforge/wineforge-engines/main/.github/workflows/build.yml — macos-15-intel，mingw-w64，360 分钟超时
18. https://github.com/srimanachanta/winecx-dist 与 https://raw.githubusercontent.com/srimanachanta/winecx-dist/main/scripts/build-wine.sh — CX 源码构建脚本与参数
19. https://github.com/dappermint/winecx-gptk — 用 mingw gcc 的原因（llvm 编译的 kernelbase 导致 Steam 卡住）、nixpkgs 依赖、自托管 runner 预热后约 15 分钟、受限 entitlement 说法
20. https://idrewsomeshapes.ca/posts/2025/12/building-wine-from-source-on-an-m2-mac/ — M2 上构建 Wine 的经验（2025-12-21）
21. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.0/ANNOUNCE.md — Xcode ≥15.3 不再需要 preloader，Sonoma+ syscall 仿真
22. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — 移除 wine64，`%gs` 交换，安装 unixlib.h
23. https://github.com/wine-mirror/wine/tree/master/libs — Wine 内置的第三方库（ffmpeg、vkd3d、faudio、symcrypt 等）
24. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/appwiz.cpl/addons.c — GECKO_VERSION 2.47.4、MONO_VERSION 11.3.0、sha256 与搜索顺序
25. https://raw.githubusercontent.com/wine-mirror/wine/master/programs/wineboot/wineboot.c — `.update-timestamp` 与前缀自动升级逻辑
26. https://github.com/mstorsjo/llvm-mingw/releases — 20260922（LLVM 23.1.2）、20260908、20260826
27. https://raw.githubusercontent.com/3Shain/dxmt/main/docs/DEVELOPMENT.md — DXMT 构建需求（Xcode 16+ 与 Metal toolchain、LLVM 15、Meson 1.3+、Wine 构建目录）
28. https://raw.githubusercontent.com/3Shain/dxmt/main/.github/workflows/ci.yml — DXMT CI（macos-15、Xcode 16.1、llvm-mingw-20260908 ucrt-macos-universal、attestation）
29. https://api.github.com/repos/3Shain/dxmt/releases — v0.80（2026-04-23）等发布日期与资产名
30. https://api.github.com/repos/KhronosGroup/MoltenVK/releases — v1.4.2（2026-07-24），最低 macOS 12
31. https://api.github.com/repos/Gcenx/DXVK-macOS/releases — 最后一版 v1.10.3 repack（2024-07-23）
32. https://api.github.com/repos/wine-mono/wine-mono/releases — 11.3.0（2026-08-17，含 arm64）、11.2.1（2026-09-04）
33. https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution — 公证要求、notarytool/stapler、altool 于 2023-11-01 停用
34. https://developer.apple.com/documentation/security/resolving-common-notarization-issues — Developer ID、hardened runtime、timestamp、get-task-allow、entitlements 格式、10.9 SDK
35. https://developer.apple.com/documentation/bundleresources/placing-content-in-a-bundle — bundle 各位置放什么、“代码 = Mach-O”、每个代码位置应为扁平列表、不建议嵌套（若嵌套则目录名不能带点号）
36. https://developer.apple.com/forums/thread/701514 — Quinn：Signing a Mac Product For Distribution（不用 --deep、由内向外、entitlements 只加在主程序上）
37. https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.disable-executable-page-protection — 该 entitlement 的语义与警告
38. https://support.apple.com/guide/security/rosetta-2-on-a-mac-with-apple-silicon-secebb113be1/web — Apple Platform Security：翻译的 x86_64 代码可以不签名，arm64 必须签名
39. https://keith.github.io/xcode-man-pages/notarytool.1.html — notarytool 认证方式、`--wait`、log、接受的格式
40. https://developer.apple.com/forums/thread/813586 — 超大 App 公证耗时 3.5–4.5 小时（2026-01/02）
41. https://developer.apple.com/programs/ — Apple Developer Program 年费 $99
42. https://eclecticlight.co/2025/12/08/who-decides-to-quarantine-files/ — LSFileQuarantineEnabled 与 quarantine 标记由谁决定
43. https://eclecticlight.co/2026/01/17/whats-happening-with-code-signing-and-future-macos/ — 截至 2026-01，Apple 未宣布 macOS 27 在签名与公证上的新变化
44. https://github.com/sparkle-project/Sparkle/releases/tag/2.10.0 （以及 api.github.com/repos/sparkle-project/Sparkle/releases/latest）— 2.10.0，发布于 2026-09-13T23:54Z
45. https://sparkle-project.org/documentation/ — Sparkle 集成、EdDSA、SUPublicEDKey、SUFeedURL
46. https://sparkle-project.org/documentation/delta-updates/ — delta 格式 1–4、BinaryDelta、限制
47. https://sparkle-project.org/documentation/publishing/ — 支持的归档格式、DMG 建议、phased rollout
48. https://github.blog/changelog/2026-02-26-macos-26-is-now-generally-available-for-github-hosted-runners/ — macos-26 GA 与各标签
49. https://github.blog/changelog/2025-09-19-github-actions-macos-13-runner-image-is-closing-down/ — macos-13 于 2025-12-04 退役，macos-15-intel，Intel 于 2027 年秋终止
50. https://github.com/actions/runner-images — 当前镜像与标签表（xcode-27 preview、macos-26-intel 等）
51. https://raw.githubusercontent.com/actions/runner-images/main/images/macos/macos-26-arm64-Readme.md — macos-26 arm64 镜像软件清单（20260907），页首公告：macOS 14 镜像 7 月 6 日开始弃用、11 月 2 日起完全不支持
52. https://raw.githubusercontent.com/actions/runner-images/main/images/macos/macos-15-Readme.md — macos-15 Intel 镜像软件清单（20260824）
53. https://docs.github.com/en/actions/reference/runners/github-hosted-runners — runner 规格（M1 3 核、7 GB、14 GB），公开仓库免费
54. https://docs.github.com/en/billing/reference/actions-runner-pricing — macOS 每分钟价格（标准 $0.062、12 核 large $0.077、M2 Pro xlarge $0.102），“The larger runners are not free for public repositories.”
55. https://github.com/resources/insights/2026-pricing-changes-for-github-actions — 2026 年价格调整，自托管平台费推迟，公开仓库继续免费
56. https://github.blog/changelog/2025-11-20-github-actions-cache-size-can-now-exceed-10-gb-per-repository/ — 10 GB 免费缓存、7 天保留、可付费扩容
57. https://docs.github.com/en/actions/reference/security/secure-use — 公开仓库不宜使用自托管 runner
58. https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases — 单个资产小于 2 GiB，每个 release 最多 1000 个，不限带宽
59. https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations — attestation 所需权限与 `gh attestation verify`
60. https://brew.sh/2025/11/12/homebrew-5.0.0/ — Homebrew 5.0.0：2026-09 起 Intel 降为 Tier 3、不再出 bottle，2027-09 起不能在 Intel 上运行
61. https://docs.brew.sh/Support-Tiers — Homebrew 支持分级（截至 2026-09：Big Sur 11 到 Tahoe 26 上的 Intel x86_64 均为 Tier 3，已停止构建新的 Intel bottle，2027-09 或之后不再能在 Intel 上运行；建议 Intel 用户改用 MacPorts；转引 Apple 的说法：Rosetta 2 作为通用工具支持到 macOS 27）
62. https://developer.apple.com/xcode/system-requirements/ — Xcode 27 需要 macOS 26.6+，Xcode 26.6 需要 26.2+，对应 SDK 与 Swift 版本
63. https://www.polpiella.dev/metal-toolchain-ci-cd 与 https://github.com/actions/runner-images/issues/13080 — Xcode 26 的 Metal toolchain 单独下载及其在 CI 上的问题（来自搜索摘要）[二手]
64. https://9to5mac.com/2025/08/08/macos-tahoe-fix-gray-box-icons/ 与 https://successfulsoftware.net/2025/09/26/updating-application-icons-for-macos-26-tahoe-and-liquid-glass/ — Tahoe 图标格式、`.icon` 与 `Assets.car`、灰色 squircle（来自搜索摘要）[二手]
65. https://github.com/tqbf/swiftui-app — 只用 SwiftPM 构建 SwiftUI App 的模板，asset catalog 与 previews 不可用
66. https://raw.githubusercontent.com/Heroic-Games-Launcher/HeroicGamesLauncher/main/src/backend/wine/manager/downloader/constants.ts — Heroic 的 macOS Wine 下载源
67. https://raw.githubusercontent.com/bottlesdevs/components/main/README.md 与 https://raw.githubusercontent.com/bottlesdevs/components/main/index.yml — Bottles 组件清单格式
68. https://raw.githubusercontent.com/bottlesdevs/Bottles/main/bottles/backend/models/config.py — BottleConfig 中的 Runner、DXVK、VKD3D、NVAPI、LatencyFleX 字段
69. https://lutris.net/api/runners/wine — Lutris runner API 的 JSON 结构
70. https://developer.apple.com/forums/thread/691574 — JIT、库校验与 Rosetta 2 的 DTS 讨论
71. https://reviews.llvm.org/D82491 与 https://developer.apple.com/forums/thread/719941 — LLDB 对 Rosetta 的支持与翻译进程崩溃调试（来自搜索摘要）[二手]
72. 本机实测（2026-09-26，M3、8 GB、macOS 26.5 25F71、CLT 26.6）：
    - 工具是否存在：xcrun --find 的结果
    - clang 输出 x86_64 Mach-O，`-no_huge` 能通过，COFF 目标文件能生成
    - 缺少 lld 等工具，bison 为 2.3
    - bsdtar 支持 xz，不支持 zstd
    - `man dlopen` 中的搜索规则（fallback 与 `/usr/local/lib`、`/usr/lib` 只对 “old binaries” 生效）；`man dyld` 中的 `DYLD_PRINT_LIBRARIES`
    - 硬件与磁盘情况
73. 交叉引用：本目录下的 01（CrossOver 产品）、02（CX 源码差异）、03（上游 Wine 与 Proton）、04 与 05（图形）、06（CPU 翻译与 ARM64 entitlement）号报告
74. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/makedep.c — makedep 生成顶层 Makefile，模块产物路径按 `arch_dirs[arch]` 拼接（如 `x86_64-windows`）
75. https://raw.githubusercontent.com/actions/runner-images/main/images/macos/templates/macOS-26.arm64.anka.pkr.hcl — macOS 26 arm64 镜像的 Packer 模板，provisioner 依次执行 install-xcode-clt.sh、install-homebrew.sh、install-rosetta.sh（事实核查补充）
76. https://raw.githubusercontent.com/actions/runner-images/main/images/macos/scripts/build/install-rosetta.sh — 调用 `/usr/sbin/softwareupdate --install-rosetta --agree-to-license`（事实核查补充）
77. https://raw.githubusercontent.com/wine-mirror/wine/master/configure.ac — 未指定 `--enable-archs` 时 `cross_archs=$HOST_ARCH`（事实核查补充）
78. https://github.com/actions/runner-images/issues/13518 — macOS 14 镜像弃用公告（事实核查补充）
79. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/v1.4.2/Docs/Whats_New.md 与 https://github.com/KhronosGroup/MoltenVK/releases.atom — v1.4.2 “Released 2026-07-20”、“Raise minimum target to macOS 12.0”；GitHub Release 为 2026-07-24（事实核查补充）
80. https://github.com/sparkle-project/Sparkle/releases.atom 与 https://raw.githubusercontent.com/sparkle-project/Sparkle/2.10.0/Package.swift — 2.10.0 创建时间与 `.macOS(.v12)` 平台声明（事实核查补充）
81. https://github.com/mstorsjo/llvm-mingw/releases.atom、https://github.com/wine-mono/wine-mono/releases.atom、https://github.com/3Shain/dxmt/releases.atom — llvm-mingw 20260922（LLVM 23.1.2）、wine-mono 11.3.0（2026-08-17）与 11.2.1（2026-09-04）、DXMT v0.80（2026-04-23）的发布时间（事实核查补充）

---

## 事实核查记录

独立事实核查于 2026-09-26 完成，共 12 条。“已证实”条目只补充了细节和来源；“部分正确”条目已在正文原处更正。本报告不涉及法律问题，因此没有“跳过：法律不在范围内”的条目。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| Homebrew 5.0.0（2025-11-12）：2026-09 或之后 Intel x86_64 降为 Tier 3、不再构建 Intel bottle；2027-09 或之后不能在 Intel 上运行（摘要、1.3、风险 1） | 已证实 | 原文措辞为 “September 2026 or later” 和 “September 2027 or later”，摘要已改为“或之后”。Support-Tiers（2026-09）已把 Intel 列为 Tier 3，建议 Intel 用户改用 MacPorts，并转引 Apple 的说法：Rosetta 2 作为通用工具支持到 macOS 27。“Apple Silicon 上经 Rosetta 运行的 x86 Homebrew 同样受影响”属于合理推断，已标为 [推断] [60][61] |
| GitHub 标准 arm64 runner 为 3 核 M1、7 GB、14 GB，公开仓库免费；Intel 有 `macos-15-intel`、`macos-26-intel`（macos-26 于 2026-02-26 GA）；2025-09-19 公告称 macOS 15 镜像于 2027 年秋退役后不再支持 Intel（摘要、3.1） | 已证实 | Intel runner 为 4 核、14 GB。`macos-latest` 对应 macOS 26 arm64。`macos-26-intel` 的退役日期仍未找到，与 2025 年公告的矛盾仍列在未解问题第 3 条 [48][49][50][53] |
| Apple Platform Security：翻译执行的 x86_64 代码可以完全不签名，原生 arm64 代码至少要有 ad-hoc 签名（摘要、2.2、2.4） | 已证实 | 原文两句均逐字核对。补充：AOT 翻译产物带有 Secure Enclave 支持的补充签名；非沙盒 App 新建的文件默认没有 quarantine，除非设置 `LSFileQuarantineEnabled` [38][42]。摘要里补充了“Rosetta 只承诺支持到 macOS 27”这一长期风险 [61] |
| Sparkle 2.10.0 “Golden Gate Bump” 于 2026-09-13 发布，最低 macOS 12，移除 CocoaPods；delta 格式 4 需 Sparkle 2.7+（摘要、2.6） | 已证实 | Atom feed 显示创建于 2026-09-13（UTC），2026-09-14T00:36Z 有过一次更新。`Package.swift` 声明 `.macOS(.v12)`。2.6 节已补充以上信息 [44][46][80] |
| Xcode 27 需要 macOS 26.6+ 并附带 Swift 6.4；Xcode 26.6 需要 26.2+，附带 macOS 26.5 SDK 和 Swift 6.3；开发机为 26.5（25F71）+ CLT 26.6，没有 xcodebuild、actool、metal（摘要、1.2、5.3） | 已证实 | 核查时本机复测结果一致（`notarytool` 存在，Swift 6.3.3）。`macos-26` 镜像（20260907）带 Xcode 26.0.1 到 26.6，默认 26.6，系统为 macOS 26.6.2 [51][62] |
| 上游 `tools/gitlab/build-mac` 在 ARM runner 上用 `arch -x86_64` 执行 configure 和 make，SDK 来自 Xcode，依赖来自 `/usr/local`；Gcenx 11.18（2026-09-25）用 MacPorts + llvm-mingw + `--enable-archs=i386,x86_64`，要求 GStreamer.framework 1.28.5（摘要、1.1、1.3、1.4） | 已证实（小幅更正） | ① make 一行实为 `$ARCH_CMD make -s -j$(sysctl -n hw.activecpu)`（1.1 节原本就这样写）。② `build_arch x86_64` 是 overlay README 要求写进 `macports.conf` 的设置，Portfile 本身声明的是 `supported_archs x86_64`，摘要和 1.3 节已据此更正。补充：`configure.ac` 未指定 `--enable-archs` 时 `cross_archs=$HOST_ARCH`，所以上游 CI 只出 x86_64 PE；另有 `build-daily-mac` job；11.18 发布时间 2026-09-25T15:27Z 已证实，参考来源 [4] 去掉了“年份推定”；overlay README 的 gstreamer.framework 仍写 v1.28.1 [1][2][3][4][5][6][77] |
| `macos-26` arm64 镜像没有提到 Rosetta，首个 CI job 必须验证能否装上 Rosetta（摘要、3.1、未解问题 4） | 部分正确 | 镜像软件清单确实没有列出 Rosetta，但 runner-images 的 macOS-26.arm64 和 macOS-15.arm64 Packer 模板在构建镜像时就执行了 `install-rosetta.sh`（`softwareupdate --install-rosetta --agree-to-license`），标准 arm64 runner 应已预装。摘要、3.1 节和未解问题第 4 条已更正；P1 第 7 条改为“只做 `arch -x86_64` 冒烟检查，失败时再兜底安装” [75][76] |
| larger runner（xlarge）在公开仓库也收费（3.1，原标为推断；未解问题 9） | 已证实 | 价格文档原文：“The larger runners are not free for public repositories.” 价格为标准 $0.062、large $0.077、xlarge $0.102 每分钟；2026 年价格调整后，公开仓库的标准 runner 仍然免费，自托管平台费推迟。3.1 节已从 [推断] 改为 [已证实]，未解问题第 9 条标为已解决，P1 第 7 条补充“默认不用 larger runner” [54][55] |
| `macos-14` 于 2026-07-06 开始弃用，2026-11-02 起完全不支持（3.1，原标为二手） | 已证实 | macos-26 arm64 镜像 README 页首公告（关联 runner-images issue #13518），runner-images 已把 macOS 14 标为 Deprecated。3.1 节已从 [二手] 改为 [已证实] [51][78] |
| Apple 要求 bundle 内的代码目录保持扁平，目录名不能带点号（2.2 理由 2） | 部分正确 | Apple 的原意是：每个代码位置应为扁平列表，不建议嵌套，否则 “might fail later in hard-to-debug ways”；“不能带点号”只在执意嵌套时适用，因为 codesign 会把带点号的目录当作 bundle。2.2 节已更正，“不把 Wine 多层目录放进签名 bundle”的结论不变 [35] |
| dlopen 叶子名搜索顺序为 DYLD_LIBRARY_PATH → LC_RPATH → 当前目录 → DYLD_FALLBACK_LIBRARY_PATH → `/usr/local/lib`、`/usr/lib`；不设 fallback 时用户 `/usr/local/lib` 里的 Homebrew 库不会被误加载（1.7） | 部分正确 | 按 `man dlopen`，fallback 这一步只对 “old binaries” 生效；不设 `DYLD_FALLBACK_LIBRARY_PATH` 时，dyld 对不受限的 old binary **会**搜索 `/usr/local/lib`。所以“不会被误加载”的说法错误。真正能挡住它的是：现代部署目标、依赖都能经 `@rpath`/`@loader_path` 在前面的步骤找到，或者进程受限。1.7 节已更正，并在 P0 第 3 条的冒烟测试里加入 `DYLD_PRINT_LIBRARIES=1` 检查；“old binary”的判定界线列为未解问题第 10 条 [72] |
| MoltenVK v1.4.2（2026-07-24）最低 macOS 12；llvm-mingw 20260922 使用 LLVM 23.1.2；wine-mono 11.3.0 为 2026-08-17、11.2.1 为 2026-09-04；DXMT v0.80 为 2026-04-23（1.4、4.2） | 已证实 | 补充：MoltenVK 的 `Whats_New.md` 写的是 “Released 2026-07-20”（GitHub Release 为 07-24），工程文件的 `MACOSX_DEPLOYMENT_TARGET = 12.0`。4.2 节已补充，并加上 Atom feed 来源 [30][79][81] |
