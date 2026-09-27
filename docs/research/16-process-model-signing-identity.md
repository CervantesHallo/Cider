# Cider 进程模型与代码签名身份：引擎放置、per-app .app shim、TCC / 本地网络 / Game Mode / Metal 缓存 / Rosetta 提示的归属

> 调研日期 2026-09-26 · 本机实验于 2026-09-27 07:27–07:31（本机时间）在 M3 / macOS 26.5（25F71）上完成，只用 arm64 探针（本机未装 Rosetta，没有做 x86_64 实测）。
> 置信度标记：**[证实]** 表示有一手文档、源码或 Apple 二进制/资源文件的直接证据；**[实测]** 表示本机可复现实验；**[第三方]** 表示来自 GitHub issue 或博客、未独立复核；**[推断]** 表示基于前述证据推理，需要实测确认；**存疑** 表示证据相互矛盾。
> 2026-09-27 按独立事实核查结果修订（涉及 agent 归属、Game Mode 证据强度、ad-hoc DR、Engine A entitlement、macOS 28 legacy games、Highball 版本等），详见文末“事实核查记录”。

## 摘要

- **冲突的解法**：12 号报告（引擎放在 bundle 外、不开 hardened runtime、x86_64 做 ad-hoc 签名或不签）和 07/09 号报告（所有 Wine 进程放进签名 .app、LC_UUID 唯一、引擎签名并公证）**各自只在一种进程模型下成立**。默认模型（模型 A）是由 Cider 进程 `posix_spawn` 出 wine。本机实测 E1 验证的是**经 LaunchServices 启动的 GUI app** 直接派生子进程的情形，这时 TCC 和本地网络都归到 **Cider.app 这个 responsible code** 名下 [1][2][3][实测]。**由 SMAppService 登记的 `cider-agent` 派生时是否同样归到 Cider.app，本文没有验证，现有证据相互矛盾，列为存疑** [1][51][52]（见 §1 最后一行）。在 GUI 派生的前提下，用途说明字符串、entitlements 和唯一的 LC_UUID 只需要放在 Cider.app 上，**Engine R 可以继续用 ad-hoc 签名，放在 bundle 外**。只有让 Wine 进程自己成为 responsible code 的模型（shim exec 或“loader 即主程序”）才需要 07/09 的要求。**Engine A 另有约束** [推断，中高]：Apple 文档 [13] 说受限 entitlement 需要 provisioning profile，而独立可执行文件无法嵌入 profile，只能包成 app 形态的 bundle。该文档讲的是使用 `com.apple.developer.networking.custom-protocol` 的 launchd daemon，套用到 Engine A 是本文的推断。`com.apple.developer.cross-architecture-support` 不在 Apple 公开的 entitlement 索引里（对应文档页 404 [61]），Apple 是否会批准也未核实。按此推断，Engine A 的 loader 应是一个 Developer ID 签名、开 hardened runtime、内嵌 profile 的 `.app` 形态 bundle，放在 Cider.app 外面也可以。
- **TCC 身份** [证实]：TCC 记录的是 responsible code 的 designated requirement（DR），之后每次访问都检查当前版本是否满足最初记录的 DR [4]。Xcode 生成的 Developer ID DR 为 `anchor apple generic` 且 identifier 匹配，且满足以下之一：叶证书是 Mac App Store 证书；或签发者是 Developer ID、叶证书是 Developer ID Application、叶证书 `subject.OU` 等于 Team ID。这个 DR 跨版本更新后依然有效。TN3127 对 ad-hoc 只说其 DR “tied to that specific version of the code”，**并没有提到 cdhash**。cdhash 形式是 codesign 给 ad-hoc 代码**默认**生成的隐式 DR（本机实测输出 `# designated => cdhash H"…"`），所以默认情况下每次重新构建都会失效 [4][2][48]。ad-hoc 签名也可以用 `codesign -s - -r='designated => identifier "x"'` 显式指定只含 identifier 的 DR（本机实测可行）。有项目报告这样能跨重建保留 TCC 授权 [53][54][第三方]，但这个 DR 没有 anchor，任何同 identifier 的代码都能满足，安全性弱。LC_UUID 不在 TCC 的 DR 里。本地网络隐私另外会用主可执行文件的 UUID，并建议使用 Apple 签发的证书签名 [1]。
- **Game Mode 与 exec 的关系** [实测，26.5，证据有限]：本机只做了少量短时实验：每种变体跑 1–2 次，每次约 5 秒，用的是 arm64 AppKit 探针，不是真实的 Wine/winemac.drv。在这些实验里，只有当全屏进程**就是 LaunchServices 启动的那个进程**、而且它的 bundle 声明了 `LSApplicationCategoryType=public.app-category.games` 时，Game Mode 才进入 on（约在启动后 3.5 秒）。进入 on 的情况：shim 用 exec 替换成 bundle 外的二进制；loader 本身就是 bundle 的主程序。这几次运行中没进入 on 的情况：shim 用 `posix_spawn` 起子进程（停在 paused 约 5–6 秒后转为 off）；由 GUI 派生的 loader；只在 loader 内嵌 `__info_plist` 里声明游戏类别。这些结果只说明 spawn 模型**在这几次运行中**没拿到 Game Mode，不能证明它永远拿不到，也不能证明 exec 是唯一途径。Apple 公开的规则只有“游戏类别、全屏、位于最前” [9]。DTS（2025-06）表示 Game Mode 不会迁移到另一个进程创建的窗口 [10]。exec 方案有代价：GamePolicyAgent 报 “This process is not the executable of a bundle”，TCC 的 responsible 变成被 exec 的那个二进制本身。
- **Metal 着色器缓存** [实测]：26.5 上缓存路径已经是 `$(getconf DARWIN_USER_CACHE_DIR)/<bundleID>/com.apple.metal`。本机该目录下有 23 个这样的子目录，另有一个顶层 `com.apple.metalfe`。其中 bundleID 取自**进程内的 `NSBundle.mainBundle`**，而不是 LaunchServices 的登记。上游 Wine loader 内嵌的 `CFBundleIdentifier` 是 `org.winehq.wine` [20][21][59]，所以 exec 或 spawn 方案下所有游戏共用一个缓存目录。只有“loader 即 bundle 主程序”的方案能做到每个 bundle 一个缓存目录。
- **Rosetta 弃用 UI** [证实：本机 Apple 二进制与资源文件；后果部分为推断]：负责这件事的是 `ecosystemd`。它按 **responsible bundle 和开发者**记录 Rosetta 进程，有一条“嵌入组件”通知，文案为 “This version of “%@” includes a component that will not work with a future release of macOS”。它还有游戏过滤、已知启动器过滤和忽略清单三道过滤。**26.5 自带的忽略清单里有 `responsible_developer_name = "CodeWeavers Inc."`**，也就是 CrossOver 被豁免，Cider 不在清单里。26.5 自带的频率配置（文件日期为 5 月 1 日，是随系统附带的默认值，可能经 MobileAsset 替换）里有 `appRateLimitInterval=2592000`、`overallRateLimitInterval=86400` 和 `pluginNotificationEnabled=false`。按键名推断，含义是同一 app 30 天最多一次、所有 app 合计每天最多一次、插件通知关闭。Cider 会不会收到“嵌入组件”这一种通知，也是从键名和字符串推断的，都没有实际观察到 [推断]。
- **竞品做法**：frankea/Whisky（app-v3.7.0，2026-08-30）和 Highball（最新 v0.9.38，2026-09-26；本文最初核对的是 v0.9.37，2026-09-25）都用模型 A，引擎放在 `~/Library/Application Support` 下 [57][58]。它们的 shortcut 或 stub 要么是未签名的 bash 脚本，要么是 ad-hoc 签名、通过 `open` 转发 URL 的 stub [24]–[32]。CrossOver 26.2 则把 wineserver、wineloader 放在自己的 bundle 里，由 CodeWeavers 签名，并据报带有 `disable-library-validation` [33][34][第三方]。
- **安全**：Wine 不是沙箱，Z: 默认映射 `/`。shell32 默认把 Documents、Desktop、Downloads 链接到 `$HOME` 下对应目录 [23]。在模型 A 下，所有瓶子共用 Cider.app 这一个 TCC 授权。App Sandbox 实际上不可行；`sandbox-exec` 在 26.5 上仍然存在，但 man 页已标为 DEPRECATED。

## 详细调研

### 1. notarized Cider.app 派生 ad-hoc 或未签名 x86_64 loader 时，responsible code 与身份怎么计算

**机制** [证实]：

- TN3179（修订于 2026-02-17）写道：“if your app spawns a helper tool and the helper tool performs a local network operation, macOS considers the app to be the responsible code.”系统据此在弹窗里显示 app 名称和用途说明，把用户的选择记在整个 app 名下，并显示在“系统设置”里 [1]。
- Quinn 在 “On File System Permissions”（2026-09-18 修订）中说，MAC/TCC 同样依赖 responsible code，也列出了同样三点；子进程做“奇怪的事”（比如 daemonise）时会断链 [2]。
- Qt 的博客给出了细节 [3]：
  - 默认按进程树继承 responsible；
  - 经 Finder 或 LaunchServices 启动的 app 自己就是 responsible；
  - 私有 API `responsibility_spawnattrs_setdisclaim` 可以断开这条链；
  - 断链后子进程“no longer inherit hardened runtime entitlements”。这说明 hardened runtime 下的资源 entitlement 检查看的是 **responsible** 进程。
- 实证：Highball 的构建脚本注释写道，Wine 是 app 的子进程，TCC 把设备访问归到 Highball；app 没声明 `audio-input` 时，“macOS silently denies the microphone to every game and never shows a prompt”（2026-08-25 的用户报告）[30]。

**本机实测**（26.5，E1–E4；用 dlsym 调 `responsibility_get_pid_responsible_for_pid`，用 `AVCaptureDevice authorizationStatus` 查询授权状态，查询不会触发弹窗）：

| 启动方式 | responsible pid 与路径 |
|---|---|
| 从 agent shell 直接运行 | 是 Claude 宿主进程，说明继承自调用者 |
| TccParent.app（经 LS 启动）→ `posix_spawn` bundle 外的探针 | 子进程的 responsible 是 **父 app 的主程序** |
| shim.app（经 LS 启动）→ `execv` bundle 外的探针 | responsible 是它自己，路径是 **bundle 外的探针**。exec 之后，TCC 看到的身份就是被 exec 的二进制，与 shim 无关 |

**对各个子问题的回答**：

| 问题 | 模型 A：经 LS 启动的 Cider GUI 派生 wine（agent 派生见最后一行） | 依据 |
|---|---|---|
| 麦克风、摄像头、本地网络的提示和授权归到谁 | Cider.app。弹窗用 Cider 的名字和 Info.plist 用途说明；hardened runtime 的 `device.audio-input`/`device.camera` 必须签在 Cider 上。E1 只验证了 GUI 派生这一种情形 | [1][3][30][实测] |
| 引擎更新后授权还在吗 | 在。引擎不参与身份判定 | [推断，高] |
| Cider 自身更新（LC_UUID 改变）后还在吗 | TCC 存的是 DR（identifier 加 Team ID），不受影响 [4]。本地网络会用主程序 UUID，但 TN3179 没有写更新会重置；同时装有多个版本时可能表现异常（FB15568200），且无法重置（FB14944392）[1] | [证实]加[推断] |
| engine 用 ad-hoc 签名会有影响吗 | 在模型 A 下没有影响 | [推断，高] |
| 如果 wine 自己成为 responsible（exec shim 或 disclaim） | TN3127 说 ad-hoc 的 DR “tied to that specific version”；codesign 默认给 ad-hoc 代码生成 cdhash 形式的隐式 DR，所以默认情况下每次重建或更新后都要重新授权 [4][2][48]。显式指定只含 identifier 的 DR 据报可以避免重新授权，但没有 anchor，安全性弱 [53][54][第三方]。本地网络要求“sign it with an Apple-issued code-signing identity”，并且主程序 UUID 不能和别的程序重复 [1] | [证实]加[第三方] |
| 用户在 Terminal 里运行 `ciderctl` | 如果 ciderctl 自己派生 wine，responsible 变成 Terminal；本地网络对“Command-line tools run from Terminal or over SSH”及其子进程自动放行 [1]。如果 ciderctl 只通过 XPC 转交，归属取决于实际派生 wine 的进程 | [证实] |
| SMAppService 登记的 LaunchAgent（cider-agent）派生 wine | **存疑，未验证，证据相互矛盾。** 支持“归到 Cider.app”的证据：TN3179 要求非 SMAppService 安装的 agent 设置 `AssociatedBundleIdentifiers` 以告诉系统 responsible code [1]；Quinn（论坛 751802，2024-06）说 TCC 会找 “the nearest parent of the process that the user knows about”，并推荐用 SMAppService 或 `AssociatedBundleIdentifiers` [51]。反对的证据：OpenLogi PR #1031（2026-08-27 合并）观察到 SMAppService 登记、经 `launchctl kickstart` 启动的 agent “as its own TCC responsible process” [52]。另外，TN3179 明确写 “The exception for launchd daemons doesn't apply to launchd agents”，agent 不享受本地网络自动放行 [1]；hardened runtime 的设备 entitlement 可能要签在 agent 可执行文件本身上。E1 只测过经 LS 启动的 GUI app 派生子进程，从未测过 SMAppService agent | [推断，低；存疑，待 T13/T2/T3] |

结论：09 号报告里“每个可执行文件都要有唯一 LC_UUID、都用 Developer ID 签名”这条，在“由经 LS 启动的 Cider GUI 派生 wine”的模型 A 下只需要对 **Cider.app 的主程序** 成立。如果改由 `cider-agent` 派生，那么在 T13 证明授权确实归到 Cider.app 之前，agent 本身也应满足同样要求：Developer ID 签名、唯一 UUID、设备 entitlement，以及它自己的 Info.plist 用途说明。

### 2. per-app .app shim：Game Mode、Games app、Dock、URL/文档类型、Metal 缓存

**背景证据** [证实]：

- Game Mode 靠 `LSApplicationCategoryType` 判定一个 app 是不是游戏，并且要求“full screen, and is the front most app” [9]。
- `LSSupportsGameMode` 的适用系统是 macOS 26.0+ 和 iOS/iPadOS 18.6+ [5]。26.0 发布说明另有一条 153125166：“Fixed: The LSSupportsGameMode Info.plist key is currently ignored” [6]。
- 26.0 发布说明有一条已知问题 153127050：“Game Mode will not activate for application binaries spawned directly from Terminal”，给出的规避办法是改用 `open` 启动 [6]。
- DTS 在论坛上（2025-06）表示，Game Mode 不会迁移到启动器之后由另一个进程新建的窗口；另有用户发现 Minecraft Launcher 的 bundle ID 被系统特殊处理 [10]。
- 手动覆盖：`gamepolicyctl game-mode set on` 可以强制开启 Game Mode，但需要装 Xcode [37]，只能作为诊断手段。torqer.app 的文章（2026-07-08 更新）说 CrossOver、Whisky 和 Wine 游戏经常不会自动进入 Game Mode [37]。
- 本机 `GamePolicyAgent` 里能看到 `bundleRecordForAuditToken:`、`markProcessAsIdentifiedGame`、`gameModeOverrides`/`identifiedGameOverrides`、“File system scan found %ld potential games in non traditional paths”等字符串。可以确认它按审计令牌查 LSBundleRecord，并维护一个已安装游戏库。

**本机实测 E3**（26.5，2026-09-27 07:27–07:31）：探针用 AppKit 建窗口并切成全屏，每种方式跑约 5 秒，观察 `log show` 中 GamePolicyAgent 的 `Game mode is on/paused/off`。**证据有限**：每种变体只跑了 1–2 次，每次约 5 秒，探针是 arm64 AppKit 程序，不是真实的 Wine/winemac.drv。下表只说明这几次运行的结果，不能推出某种模型“永远”拿不到 Game Mode。

| 变体 | 结果 |
|---|---|
| T1：shim（类别为游戏，C stub）`execv` bundle 外的 loader | **on**，约在启动后 3.5 秒（日志 07:29:06）。同时报 `isGameModeEnabledByUser/setGameModeAllowed: unable to lookup lsBundleRecord … "This process is not the executable of a bundle."` |
| T2：shim `posix_spawn` 同一个 loader 后 `waitpid`（重复两次） | 停在 **paused** 约 5–6 秒，然后转为 off；这两次都没进入 on |
| T3：普通 GUI app 派生 loader（相当于模型 A） | 没有识别为游戏 |
| T4：同 T3，但 loader 的内嵌 `__info_plist` 带有 games 类别和 `LSSupportsGameMode` | 没有识别为游戏，**内嵌 plist 不起作用** |
| T5：从 shell 直接运行带游戏类别的 loader | 没有识别为游戏 |
| T6：loader 本身就是 bundle 的主程序（bundle 声明游戏类别） | **on**（日志 07:30:12），没有报错 |

**实测 E1/E2：LaunchServices 与 Metal 缓存**（用 `lsappinfo` 和 `NSRunningApplication` 查看）：

| 变体 | Dock / LS 身份 | `NSBundle.mainBundle` | Metal 缓存目录 |
|---|---|---|---|
| 直接运行 loader | “loader”，无 bundle ID | loader 内嵌的 ID | `C/org.ciderprobe.loader/` |
| shim exec | **shim 的名称和 bundle ID 保留**；`CFBundleExecutablePath` 变成 bundle 外的路径 | loader 内嵌的 ID | loader 的目录 |
| shim spawn（stub 自己不 check-in） | 子进程**接管了 shim 的 LS 身份** | loader 内嵌的 ID | loader 的目录 |
| GUI 先 check-in，再派生 loader | 子进程另外登记为 “loader”，没有 bundle ID | loader 内嵌的 ID | loader 的目录 |
| loader 即主程序 | 该 bundle | 该 bundle | `C/<该 bundle ID>/com.apple.metal` |

注：探针当时生成的缓存目录（`C/org.ciderprobe.*`）在事实核查时已不在本机，上表各行的缓存目录无法再次复核。

另一个第三方开发者也得出一致结论：“macOS takes a running app's identity from the bundle its executable lives in” [40]。claude-code #80472（2026-07-23，macOS 27 beta 4 26A5388g）报告：Seatbelt profile 挡住 `C/<bundleID>/` 目录时，`-[_MTLDevice recordBinaryArchiveUsage:]` 拿到的 `MTLGetShaderCachePath()` 为 nil，随后向 NSArray 插入 nil 抛出 NSException，进程 SIGABRT [38]。该 issue 把按 bundle 分目录描述为 macOS 27 的变化，但本机在 26.5 上已经在用这种布局 [48]。26.5 上是否存在同样的崩溃路径，本文没有验证。因此以后给引擎加的任何 Seatbelt profile 都必须允许写入 `DARWIN_USER_CACHE_DIR/<id>/`。

**逐项结论**：

- **Game Mode**（以下均受 E3 样本少、时间短、非真实 Wine 的限制）：
  - 在 E3 中，模型 A（T3）没有被识别为游戏，spawn 型 shim（T2）停在 paused。这与社区对 CrossOver、Whisky 的观察一致 [37]，也符合 DTS“不会迁移到另一个进程创建的窗口”的说法 [10]。但现有证据不足以断言模型 A 永远不会自动进入 Game Mode [实测，证据有限]。
  - 已测变体中只有 shim exec 和“loader 即主程序”两种进入了 on。有没有别的途径没有验证。
  - 就算用这两种做法，也只覆盖**被 LS 启动的那一个 Wine 进程**。Steam 或 launcher.exe 再派生出来的 game.exe 是另一个没有 bundle 的 LS app，推断同样不会进入 Game Mode（由 T2/T3 推断）。
  - Apple 的已知启动器白名单（`com.valvesoftware.steam`、`com.epicgames.EpicGamesLauncher`、`net.battle.app` 等，见 ecosystemd 字符串）只认原生 bundle ID。
- **Games app 收录**：GamePolicyAgent 会在 LS 的 FSNode 上设置 `_NSURLApplicationIsIdentifiedGameKey`，并做 CoreSpotlight 索引。据此推断，带游戏类别、在 LS 中登记过的 shim 会被收录，派生出来的 wine 进程不会 [推断，中；待 T14 验证]。
- **Dock 名称和图标**：
  - exec 和 spawn 两种做法都能拿到 shim 的名称 [实测]。
  - 运行中的 Dock 图标由 winemac.drv 在 `transformProcessToForeground:` 里调用 `setApplicationIconImage:` 设为 exe 的图标 [22][60]。
  - Wine 菜单名取 `[NSBundle mainBundle]` 的 `CFBundleName` [22][60]，所以在 exec 做法下菜单名会显示 loader 内嵌的名称（上游为 “Wine”）。
- **CFBundleURLTypes / CFBundleDocumentTypes**：`cocoa_app.m` 没有 `application:openFiles:`/`openFile:`/`openURLs:` 方法，也没有 `kAEOpenDocuments`/GetURL 的 Apple Event 处理器 [22][60]。LS 把文档或 URL 投递给 shim 时，这个 Apple Event 会丢失。所以 shim 只负责启动；文档和 URL 类型统一登记在 Cider.app 上，由它分发。frankea/Whisky 和 Highball 也是这样做的 [26][30]。
- **exec 到签名不同的二进制之后**：LS 身份保留；进程内的 bundle、TCC 的 responsible 身份、Metal 缓存键、GamePolicy 的 LSBundleRecord 查询都跟着新的可执行文件走 [实测]。

### 3. Rosetta 弃用 UI：arm64 的 Cider.app 派生 x86_64 引擎时会触发什么、多久一次

**官方** [证实]：

- 26.4（169228455）：“users will be notified when they launch apps that use Rosetta”。beta 期间会加快通知频率。MDM 可以用 `allowRosettaUsageAwareness`（域为 `com.apple.applicationaccess`）关闭通知 [7][12]。
- 26.4 还写明：“There will continue to be support for older, unmaintained gaming titles leveraging Rosetta along with software running Intel binaries in Linux VMs” [7]。
- 27 [8]：
  - “Settings > General now lists Intel-based apps that will be incompatible with macOS 28.0”（175697313），并且该列表 “also identifies unused Intel-based software discovered on the system”。
  - “Get Info”中会显示标记（169548657）。
  - “Intel-based plugins and loaders may not appear in Settings or trigger notifications … All Intel-based software will no longer be compatible with macOS 28.0, excluding legacy games”（176042635）。
  - 166398727：legacy Intel 游戏支持被描述为 “the new underlying system behavior”，beta 中用 `sudo game-test-tool enable` 开启，**只在 beta 版可用**。说明里写道，开启 legacy game support 会禁用 Rosetta，“non-game processes might crash or behave unexpectedly”。
  - 168097174：勾选了“使用 Rosetta 打开”的 app 现在以原生方式启动。
  - 163213094：升级到 27.0 后 Rosetta **不会自动恢复**，需要重新安装。

**机制**（本机 `/System/Library/PrivateFrameworks/Ecosystem.framework`，26.5，Platform identifier 26）[证实：字符串与资源文件]：

- **数据来源**：`ecosystemd` 记录 Rosetta 进程时带有 `responsible_bundle_id`、`responsible_path`、`responsible_team_id`、`responsible_developer_name`、`process_path`、`process_bundle_id` 等字段。另有 `topProcessPathsForResponsibleBundle`、`getEmbeddedBundle`、`evaluateThirdPartyApp … isComponentNotification`。
- **通知文案**（en）：
  - 顶层 app：“This version of “%@” will not open in a future release of macOS.”
  - 嵌入组件：“This version of “%@” includes a component that will not work with a future release of macOS.”
  - 插件，以及周期汇总：“You recently used %d apps that still rely on Rosetta … macOS 28 or later.”
- **过滤**：
  - Game Filter：`[GAME_CHECK] type=process/responsible`，结果为 “Bundle is a game, Rosetta usage is supported”。
  - Known Launcher Filter：同样只认原生启动器 bundle ID。
  - Ignore List：`CORAL.plist`，共 214 条规则，其中 212 条按 `process_bundle_id`、1 条按 `responsible_developer_name`、1 条按 `process_path`。**第 0 条是 `responsible_developer_name = "CodeWeavers Inc."`**，第 1 条是 `process_path = "*/steamapps/*"`，第 2 条起有 `unity.*`、`com.feralinteractive.*`、`com.aspyr.*`、`com.blizzard.*` 等游戏和模拟器的 bundle ID。`ecosystemd` 本身也硬编码了 “CodeWeavers Inc.” 字符串，另有 “Matched exclusion rule” 字符串。
  - 论坛 818906 贴出了 26.4 beta 4（2026-03）的真实日志（`[GAME_CHECK] type=process bundle=org.videolan.vlc result=notGame`），与以上逻辑一致；Quinn 在该帖中没有解释这一机制 [11]。
- **频率**（`PUMPKIN.plist`）：`appRateLimitInterval=2592000`、`overallRateLimitInterval=86400`、`catchupInterval=86400`、`periodicInterval=0`、`pluginNotificationEnabled=false`、`enabled=true`、`customURLsEnabled=false`、`suRecentLaunchWindow=31536000`。
- **这些文件的性质**：CORAL 和 PUMPKIN 都是随系统附带的默认资源（文件日期为 5 月 1 日）。`ecosystemd` 日志有 `assetsLoaded=yes`，说明资源可能经 MobileAsset 替换。没有任何公开资料说明 CORAL 或 PUMPKIN。文件内容本身 [证实]；“同一 app 30 天最多一次”“所有 app 每天最多一次”是按键名作的解读 [推断]，没有观察到实际的通知节奏。

**对 Cider 的推断**：

| 模型 | 预计会出现的 UI | 置信度 |
|---|---|---|
| A：arm64 Cider.app 派生 x86_64 wine | 26.4+：“Cider includes a component …”这条嵌入组件通知（会收到这一种而不是别的，是从字符串和 `isComponentNotification` 推断的，未实际观察），按 PUMPKIN 键名推断同一 app 约 30 天一次，所有 app 合计每天最多一次。前提是 Cider 没被游戏过滤命中；如果 Cider.app 声明为游戏类别，可能会被过滤掉。27：Settings 列表会跳过“已是 universal”的 bundle（有 `Rosetta app updated to universal, excluding` 字符串），所以 Cider.app 大概率不上榜；Get Info 按 bundle 架构判断，不会给 Cider.app 加标记 | 中 |
| B：arm64 shim exec x86_64 loader，shim 声明游戏类别 | 游戏过滤很可能命中，不弹通知 | 中低 |
| D：x86_64 loader 作为 shim 主程序 | 如果游戏过滤没命中，每个 shim 都会被当成顶层 Intel app：弹通知、进 27 的 Settings 列表、Get Info 加标记 | 中 |

**macOS 28 之后 Engine R 还能不能用** [未知]：本文初稿推断 macOS 28 的 legacy games 机制“很可能复用同一套游戏判定（ecosystemd 的游戏过滤）”。这个推断**没有依据**，一手证据指向另一套机制。27 发布说明（166398727）把 legacy Intel 游戏支持描述为 “the new underlying system behavior”，beta 中通过 `sudo game-test-tool enable` 开启，并写明开启后**会禁用 Rosetta**，“non-game processes might crash or behave unexpectedly”，而且该工具不在正式版中提供 [8]。Wine 栈本身就包含多个非游戏的辅助进程（wineserver、services.exe、explorer.exe、各种 launcher），在这套机制下能否正常运行完全未知。目前没有任何证据把它和 ecosystemd 那套只管通知的游戏过滤联系起来。Engine R 在 macOS 28 之后的可用性应视为**未知**，需要在 27 beta 上用 `game-test-tool` 实测（T15）。另外，升级到 27.0 后 Rosetta 不会自动恢复（163213094）[8]，会直接影响 Cider 的首次运行引导。相关背景：据媒体报道，CodeWeavers 正在把 CrossOver 27 迁到原生 ARM64（ARM64EC + FEX）引擎，2026 年 7–8 月出预览版，目标 2027 年初 [55][56][媒体报道，未复核]。如果属实，CrossOver 在忽略清单里的豁免长期看可能不那么重要。

### 4. 如果引擎用 Developer ID 签名并开 hardened runtime，最小 entitlement 集是什么

- 公证强制要求 hardened runtime [14]。
- `allow-unsigned-executable-memory` 的含义是：可以不受 `MAP_JIT` 限制地创建可写又可执行的内存 [15]。
- `allow-jit` 只放行 `MAP_JIT`。在 hardened runtime 加 `allow-jit` 的组合下，“it can only create one memory region with the MAP_JIT flag set”；另外，Apple Silicon 无论是否开 hardened runtime 都强制 W^X [16]。
- Rosetta 下，x86_64 代码可以不签名；原生 arm64 代码至少要有 ad-hoc 签名 [17]。

| 组件 | 最小集合（待 T8/T9 二分验证） | 理由 |
|---|---|---|
| Engine R 的 loader（只在模型 B/D 需要签名；模型 A 保持 ad-hoc，不开 runtime） | `cs.allow-unsigned-executable-memory`、`cs.disable-library-validation`、`device.audio-input`、`device.camera`；**不要**加 `allow-dyld-environment-variables`，改用 rpath | PE 映像会以 PROT_EXEC 映射，Windows 程序也会自己做 JIT，所以需要第一项 [推断]。ad-hoc 的 ntdll.so、社区组件，以及 D3DMetal/GStreamer 与引擎不是同一个 Team，所以需要 DLV [推断]。CrossOver 的 wineloader/wineserver 据报就带 DLV [33][34][第三方] |
| Engine A 的 loader | 在上面基础上加 `com.apple.developer.cross-architecture-support`（如果 Apple 批准）和 `cs.allow-jit`，并在 `Contents/embedded.provisionprofile` 放 profile | Apple 文档原文是 “a daemon is a standalone executable, so you can't embed a provisioning profile in it”，所以要包成 app 形态；“your provisioning profile is tied to your App ID, and the bundle identifier is a key part of that App ID”；“Add the Hardened Runtime capability, which you'll need to notarize your daemon” [13][49]。该文档讲的是使用 `networking.custom-protocol` 的 launchd daemon，示例 bundle 装在 `/Library/Application Support/…` 下，说明主 app 以外的 app 形态 bundle 是允许的；套用到 Engine A 是推断 [推断，中高]。`cross-architecture-support` 不在公开 entitlement 索引中（文档页 404 [61]），它是否存在只来自 15 号报告对 xnu 头文件的阅读，事实核查时未能重新取得该头文件；Apple 是否批准也未核实。06 号报告已实测：ad-hoc 签名携带受限 entitlement 会被 AMFI SIGKILL。FEX 若需要多个 JIT 区，要看 `allow-unsigned-executable-memory` 能否解除单区限制 [16][推断] |

**Engine A 的受限 entitlement 很可能要求“Developer ID 签名 + hardened runtime + app 形态 bundle + 内嵌 profile”**（Mac App Store 以外分发时，公证本身就要求 hardened runtime [14]），所以上表 R 行的实验（T8）也是 Engine A 的前置工作。

### 5. CrossOver、frankea/Whisky、Highball 的 bundle、helper 与签名结构

| 产品 | 引擎位置 | 启动模型 | 签名与 entitlements | shortcut / stub |
|---|---|---|---|---|
| CrossOver 26.2.0（build 39821） | 在 bundle 内：`Contents/SharedSupport/CrossOver/{bin/wineserver, lib/wine/x86_64-unix/ntdll.so（LC_RPATH @loader_path/../../../lib64）, lib64/…, CrossOver-Hosted Application/{wine,wineloader}}` [33][34][35] | 未核实 | 整个 bundle 被 CodeWeavers 封存；内嵌的 wineloader/wineserver 带 DLV；主程序开了 hardened runtime [33][34][第三方]。Apple 的 Rosetta 忽略清单包含 CodeWeavers [证实] | `~/Applications/CrossOver/*.app` 是“tiny Mac application”，内部结构未核实 [36] |
| frankea/Whisky（仓库创建于 2026-01-05，app-v3.7.0 发布于 2026-08-30T01:37Z [57]） | 从 Application Support 下载 Libraries | Foundation `Process`，属于模型 A [28] | 已公证；entitlements 恰好是 apple-events、allow-unsigned-executable-memory、audio-input、camera 四项 [25]；声明了 exe/msi/bat 文档类型和 `whisky://` URL scheme [26] | 未签名的 bash 脚本 bundle（`MacOS/launch` 以 `#!/bin/bash` 开头），内容是 `exec …/Whisky.app/Contents/Resources/WhiskyCmd run …`；带游戏类别，没有 `CFBundleIdentifier`，不做签名 [27]。按 T2 推断，Game Mode 可能停在 paused |
| Highball（仓库创建于 2026-08-23，GPL-3.0；最新 v0.9.38 发布于 2026-09-26T20:17Z，此前 v0.9.37 于 2026-09-25、v0.9.36 于 2026-09-22 [58]；本表按 main 分支核对） | `~/Library/Application Support/Highball/engines/<id>`（基于 CrossOver 26.3 的 x64 构建），会剥掉 quarantine [32] | `Process`，属于模型 A | Developer ID、runtime、timestamp、公证；entitlements **只有 audio-input**；Info.plist 有 NSLocalNetwork 和 NSMicrophone 用途说明、文档类型、`highball://` [30] | `~/Applications/Highball/<Game>.app`：`exec /usr/bin/open -g highball://…`，`LSUIElement`，bundle ID 为 `app.highball.stub.<id>`，用 `/usr/bin/codesign -s -` 做 ad-hoc 签名 [31]，属于模型 E |

### 6. 运行不可信 Windows EXE 时的安全模型

- **Z: 与共享授权**：Wine 不是沙箱。删掉 Z: 也挡不住“知道自己在 Wine 里”的程序 [44]。在模型 A 下，TCC 授权是整个 app 共享的，瓶子 B 里的恶意 exe 也能用瓶子 A 里游戏换来的麦克风授权。
- **shell folder 链接**：`_SHCreateSymbolicLink` 默认把 Personal/Desktop/Downloads 等链接到 `$HOME/Documents`、`$HOME/Desktop`、`$HOME/Downloads` 等，可以用 XDG 配置和 `WINE_HOST_XDG_CONFIG_HOME` 覆盖 [23]。Desktop、Documents、Downloads 受 TCC 保护 [19]。任何 exe 只要枚举“我的文档”，就会触发“Cider 想访问文稿”的弹窗。frankea/Whisky 缺少 `NSDownloadsFolderUsageDescription` [26]。
- **沙箱**：App Sandbox 要求子进程带 `inherit` entitlement，也限制写 `/tmp`，而 wineserver 的目录就在 `/tmp` 下，**不可行** [推断]。`sandbox-exec` 在 26.5 的 man 页标为 “DEPRECATED”，可以作为“受限瓶子”的 P3 实验。
- **quarantine 与 provenance**：
  - Gatekeeper 只检查 app、插件和安装包 [18]，不会评估 PE 文件。
  - 非沙盒 app 创建的文件默认不带 quarantine（12 号报告 [42]）。
  - 下载来的 app 写出的文件会带上 `com.apple.provenance` [41][42]。据报，这类 bundle 如果签名封存不完整，首次 exec 会被判为“已损坏”，里面每个二进制都被 SIGKILL [33][第三方]。所以 Cider 生成的 shim **必须用 `/usr/bin/codesign -s -` 完整签名**，Highball 就是这样做的 [31]。
  - App Management 只保护“非同一 Team 修改已公证的 app”[43][搜索摘要]，对 ad-hoc 的 shim 不适用 [推断]。

### 7. 交付物：进程/IPC 架构图与签名矩阵

```
┌── Cider.app（arm64，Developer ID，公证，hardened runtime）──────────────────────┐
│ MacOS/Cider          SwiftUI GUI（entitlements：audio-input、camera）        │
│ MacOS/ciderctl       CLI（只通过 XPC 转交，不自己派生 wine）                   │
│ MacOS/cider-agent    SMAppService.agent，Mach 服务 org.cider.agent            │
│ Info.plist：NSMicrophone/Camera/LocalNetwork/Documents/Desktop/Downloads 用途；│
│             文档类型与 URL scheme 全部在这里登记                               │
└───────────┬───────────────────────────────────────────────────────────────┘
   GUI ─XPC─►│◄─XPC─ ciderctl（归属跟随实际派生 wine 的进程）
             ▼
      派生者：GUI（v0 默认，E1 已实测）或 cider-agent（待 T13/T2/T3 验证）
             │
             ├──posix_spawn──► wine（Engine R：x86_64，ad-hoc，
             │                      ~/Library/Application Support/Cider/Engines/<id>/）
             │                        ├─► wineserver（每个 prefix 一个，/tmp/.wine-501/）
             │                        └─► 子 Windows 进程（steam.exe → game.exe …）
             │        GUI 派生：responsible = Cider.app → TCC / 本地网络记录归 Cider [实测]
             │        agent 派生：responsible 是 Cider.app 还是 agent 自己，存疑 [51][52]；
             │                    agent 不享受本地网络的 daemon 自动放行 [1]
             │        Rosetta 记录按 responsible bundle 归属 [推断]
             │
   ~/Applications/Cider/<Game>.app（arm64 stub，本机 ad-hoc 签名，游戏类别）
        ├─ 模型 E（默认）：用 XPC 或 `open -g cider://launch/<id>` 交给派生者（GUI 或 agent）
        └─ 模型 B（实验开关）：execv ──► CiderGameHost（x86_64 loader，Developer ID
                  + runtime + DLV + unsigned-exec-mem，内嵌 Info.plist）
                  ──dlopen──► Engine R 的 ntdll.so；E3 中 Game Mode 可进入 on；TCC 归 GameHost
   CiderEngineA.app（arm64，Developer ID + embedded.provisionprofile，放在 bundle 外）
        ◄─ posix_spawn（派生者）或 exec（shim）
```

| 组件 | 位置 | 架构 | 签名 | Runtime | Entitlements | 公证 | TCC 角色 |
|---|---|---|---|---|---|---|---|
| Cider 主程序、cider-agent | Cider.app | arm64 | Developer ID | 开 | audio-input、camera（agent 同样配置，并给 agent 准备自己的用途说明，待 T13 验证） | 是 | GUI 派生时主程序是 responsible [实测]；agent 派生时 responsible 是谁存疑 |
| ciderctl | Cider.app | arm64 | Developer ID | 开 | 无 | 随 app | 只负责转交 |
| Engine R 的 wine、wineserver、*.so、dylib | App Support | x86_64 | ad-hoc 或不签 | 不开 | 无 | 否 | 不承担（GUI 派生时归 Cider；agent 派生时见上行） |
| D3DMetal.framework | App Support 的组件目录 | 用 `lipo` 实查 | 保留 Apple 原始签名 | — | — | — | — |
| CiderGameHost（可选） | Cider.app/Contents/Helpers | x86_64 | Developer ID | 开 | 见 §4 R 行 | 是 | 模型 B 下为 responsible |
| CiderEngineA.app | App Support | arm64 | Developer ID + profile | 开 | 见 §4 A 行 | 是 | 视模型而定 |
| 每个游戏的 shim、URL handler shim | ~/Applications/Cider、App Support/Handlers | arm64 | 本机 ad-hoc | 不开 | 无 | 否（本机生成，不带 quarantine） | 不承担，或只提供 LS 身份 |

**测试矩阵**（26.5 和 27 各跑一轮）：

| # | 测试 | 做法 | 预期 / 判据 |
|---|---|---|---|
| T1 | responsible 链 | 用 x86_64 wine 跑一遍 E1 探针，覆盖 A/B/C/D/E 五种模型；模型 A 分成“GUI 派生”和“SMAppService cider-agent 派生”两种 | 结果与本文 arm64 实测一致；记录 agent 派生时的 responsible pid 与路径 |
| T2 | 麦克风 | 游戏语音；比较 Cider 带和不带 audio-input；分别由 GUI 和 agent 派生；用 `tccutil reset Microphone <id>` 重置；升级 Cider 和引擎后再测 | 弹窗名称是 Cider 还是 agent；升级后不重新弹窗 |
| T3 | 本地网络 | 在 VM 或新建用户中测 Steam 局域网发现；分别由 GUI 和 agent 派生；换一个 Cider 构建（UUID 改变） | 弹窗归属；升级后是否重新弹窗 |
| T4 | Game Mode | 用真实 Wine + winemac.drv 重做 E3：每种变体至少重复 5 次，每次全屏持续 60 秒以上，覆盖 GUI 派生、spawn 型 shim、exec 型 shim、loader 即主程序；`log stream --predicate 'process=="GamePolicyAgent" AND category=="Common"'` | 确认 spawn 模型是否确实拿不到 Game Mode，E3 的结论是否成立 |
| T5 | Metal 缓存 | `ls $(getconf DARWIN_USER_CACHE_DIR)` | 缓存目录的 ID 与模型对应 |
| T6 | Rosetta UI | `log stream --predicate 'process=="ecosystemd"'`；27 上查看 Settings 和 Get Info；再测 Cider.app 声明游戏类别的情况 | 通知类型和频率 |
| T7 | 无 Rosetta | 26.x 上设置 boot-arg `nox86exec=1` [6]；在 27 上从 26.x 升级后检查 Rosetta 是否还在（163213094）[8] | 缺少 Rosetta 时的引导流程 |
| T8 | 签名后的 Engine R | Developer ID + runtime，对 entitlements 做二分；测试集覆盖 Steam、DXMT、D3DMetal、GStreamer | 得到最小集合 |
| T9 | Engine A 的 profile | exec 和 spawn 两条路径 | 不被 AMFI 杀掉 |
| T10 | provenance | 在 Cider 生成的引擎和 shim 上查看 xattr；故意破坏签名 | 签名破坏后被杀；完整签名正常 |
| T11 | quarantine | 用 Safari 下载 exe 后交给 Cider | 显示来源并提醒 |
| T12 | 文件夹 TCC | exe 访问“我的文档” | 弹窗归属；关闭链接后不弹窗 |
| T13 | agent 归属与 GUI 退出 | 用真实的 SMAppService agent 派生探针，查 `responsibility_get_pid_responsible_for_pid`，再访问麦克风和本地网络；另测游戏运行中退出 GUI 后访问麦克风 | agent 派生时 responsible 是 Cider.app 还是 agent；有 agent 与无 agent 的差异 |
| T14 | Games app | 登记一个带游戏类别的 shim | 是否出现在 Library |
| T15 | legacy 游戏模式 | 27 beta 上执行 `sudo game-test-tool enable`，再运行 Engine R（wineserver、services、explorer、Steam、游戏） | 哪些进程被杀或行为异常；Engine R 在 macOS 28 之后是否可能存活 |

## 对 Cider 的启示与建议（按优先级）

1. **P0：定下模型 A 作为默认，并据此统一 12、07、09 号报告的结论。**
   - **派生者暂不定死（因事实核查修改）**：E1 只验证了“经 LS 启动的 GUI 进程 `posix_spawn` wine 时 responsible 是 Cider.app”。`cider-agent`（SMAppService）派生时授权是否归到 Cider.app 有相互矛盾的证据：TN3179 和 Quinn 暗示系统能把 agent 关联到宿主 app，OpenLogi #1031 却观察到 agent 自己成了 responsible。而且 launchd agent 不享受本地网络的 daemon 自动放行 [1][51][52]。做法是把“谁来 posix_spawn wine”抽象成一个接口，v0 默认由 GUI 派生；T13/T2/T3 用真实 agent 验证授权确实归到 Cider.app 之后，再切到“全部由 `cider-agent` 派生”。如果验证发现 agent 自成 responsible，agent 就要有自己的 Developer ID 签名、设备 entitlement 和用途说明，授权弹窗也会以 agent 的名义出现，产品上要按这个结果重新设计。
   - GUI 和 `ciderctl` 通过 XPC 下发请求，`ciderctl` 不自己派生 wine，以免 responsible 变成 Terminal。
   - Engine R 保持 ad-hoc 签名，放在 bundle 外，不开 runtime。
   - Cider.app 只带 `device.audio-input` 和 `device.camera`，不需要 `allow-unsigned-executable-memory`；Highball 只带 audio-input 就能运行。
   - Info.plist 写全用途说明，包括 Downloads。
2. **P0：签名流水线预留 Engine A 的形态。** `CiderEngineA.app` 带 profile、runtime、Developer ID，单独公证，单独下载。“app 形态 bundle + embedded.provisionprofile”这一要求是从 daemon 文档类推出来的 [13][推断，中高]。先通过 DTS 或 Developer Relations 确认 `com.apple.developer.cross-architecture-support` 是否对第三方开放申请：它不在公开 entitlement 索引里，文档页 404 [61]，存在性只来自 15 号报告对 xnu 头文件的阅读。确认可以申请后再正式提交（衔接 06 号报告的建议）。
3. **P1：生成游戏 shim 时默认采用模型 E。**
   - arm64 stub，本机 ad-hoc 完整签名，唯一的 bundle ID，带游戏类别和图标。
   - 用 XPC 或 URL 转交给 Cider 的派生者（v0 为 GUI；T13 验证通过后改为 agent）。
   - 不声明文档类型和 URL 类型。
   - Windows 协议（如 `steam://`）用专门的 handler shim 转发。
4. **P1：先跑 T4（用真实 Wine 重做 Game Mode 实验）和 T8，再决定要不要做可选的 CiderGameHost 与模型 B。** E3 样本太少，不足以断定 spawn 模型拿不到 Game Mode。如果 T4 证实只有 exec 或“loader 即主程序”能进入 on，再在设置里提供“为此游戏启用 Game Mode（实验）”开关，并提示用户：该游戏的麦克风、网络授权会改记到 “Cider Game Host” 名下。
5. **P1：Rosetta 提示的应对。**
   - 通过 Feedback Assistant 或 DTS 申请把 Cider 的开发者名加入 Ecosystem 的忽略清单（该清单很可能经 MobileAsset 在线更新）。
   - 评估是否给 Cider.app 或 shim 声明游戏类别，以命中游戏过滤。
   - 在 UI 里预先解释“嵌入组件”通知的含义。
   - **（因事实核查修改）不要假设 macOS 28 的 legacy games 机制复用 ecosystemd 的游戏过滤。** 27 发布说明（166398727）显示它是另一套底层机制，beta 中开启后会禁用 Rosetta，非游戏进程可能崩溃 [8]。Engine R 在 macOS 28 之后的可用性应视为未知，在 27 beta 上跑 T15 实测。相应地提高 Engine A（arm64 原生）的优先级；据报 CodeWeavers 也在把 CrossOver 27 迁往原生 ARM64 引擎 [55]。
   - **（新增）** 升级到 27.0 后 Rosetta 不会自动恢复（163213094）[8]。Engine R 启动前要检测 Rosetta 是否存在，缺失时引导用户重新安装（对应 T7）。
6. **P2：安全默认值。**
   - 新建瓶子默认不链接 Documents、Desktop、Downloads，改成瓶内的真实目录，用户可选择开启链接。
   - 游戏瓶子默认去掉 Z:，按需映射具体目录。
   - 打开 exe 或 msi 时读取 `com.apple.quarantine`，展示来源 URL 并二次确认。
   - 引擎和 shim 生成后逐一执行 `codesign --verify`。
7. **P3**：研究让 Wine 的 CreateProcess 可以“经 LaunchServices 启动指定子 exe”，使 launcher 派生出的游戏也能进入 Game Mode；研究基于 Seatbelt 的受限瓶子。

## 风险

1. **CodeWeavers 独享豁免（高）**：Apple 的 Rosetta 忽略清单点名 CodeWeavers。Cider 用户很可能会看到“Cider 包含将不再工作的组件”的通知（推断）。忽略清单只管通知。到 macOS 28 时，legacy games 支持是另一套机制：按 27 beta 的 `game-test-tool` 说明，开启后会禁用 Rosetta，非游戏进程可能崩溃 [8]。Wine 这种多进程栈在这套机制下能不能用未知，需要 T15 验证。
2. **exec 方案的身份漂移（中高）**：TCC 和本地网络归到被 exec 的二进制上。如果它用默认方式做 ad-hoc 签名（隐式 cdhash DR），每次更新都要重新授权；显式 identifier DR 可以缓解，但安全性弱 [53][54]。本地网络授权还无法重置（FB14944392）。
3. **agent 身份不确定（中）**：如果改由 SMAppService agent 派生 wine，而 agent 自成 responsible [52]，授权弹窗会以 agent 名义出现，用途说明和 entitlements 也要挪到 agent 上；agent 也不享受本地网络自动放行 [1]。
4. **27 上 Metal 缓存目录不可写时直接崩溃（中）**：任何限制写 `DARWIN_USER_CACHE_DIR` 的沙箱方案都会触发（27 beta 4 上已有报告；26.5 上是否同样崩溃未验证）[38]。
5. **私有 API（中）**：`responsibility_*` 和 `gamepolicyctl`（需要 Xcode）都不能出现在产品路径里，只用于诊断。
6. **provenance 与 App Management 行为变化（中）**：Cider 自己生成的 bundle 如果签名不完整会被杀掉，行为可能随系统更新而变 [33]。
7. **第三方证据（中）**：CrossOver 的内部结构来自 2026-09 的一个 issue，未独立复核。

## 未解问题

1. 在模型 A 下，Cider.app 声明 `public.app-category.games` 能否命中 ecosystemd 的游戏过滤？副作用是 GUI 全屏时也会进入 Game Mode。
2. 如果 responsible 进程已经退出（GUI 关闭且没有 agent），tccd 对仍在运行的 wine 怎么判定？
3. 模型 B 中，exec 后的 loader 如果在 Cider.app 内但不是主程序，TCC 用谁的 Info.plist？
4. hardened runtime 加 `allow-unsigned-executable-memory` 能否解除 `MAP_JIT` 单区限制？Rosetta 下的 x86_64 进程是否受 hardened runtime 的 W^X 约束？
5. CrossOver launcher 的内部结构、wineloader 的 flags 和完整 entitlements 是什么？需要在装有 CrossOver 的机器上执行 `codesign -dvvv --entitlements -` 查看。
6. 27 的 Get Info 标记是纯静态的架构判断，还是会参考执行记录？
7. Games app Library 是否会收录 ad-hoc 签名的 shim？
8. 由 SMAppService 登记的 `cider-agent` 派生 wine 时，TCC 和本地网络的 responsible 是 Cider.app 还是 agent 自己？TN3179 和 Quinn [1][51] 与 OpenLogi #1031 [52] 的说法相互矛盾（存疑，T13）。
9. macOS 28 的 legacy games 机制（27 beta 中用 `game-test-tool` 开启）下，wineserver、services.exe、explorer.exe 这类非游戏进程还能不能运行？这套机制靠什么判定一个进程是游戏（T15）？
10. 用真实 Wine、更长时间、更多次数重跑时，spawn 型 shim 或 GUI 派生的 wine 是否真的拿不到 Game Mode（T4）？

## 参考来源

1. https://developer.apple.com/tutorials/data/documentation/technotes/tn3179-understanding-local-network-privacy.json — TN3179（修订于 2024-10-31、2025-07-18、2026-02-17）：responsible code、UUID、代码签名身份、自动放行（launchd daemon、root、Terminal/SSH 下的命令行工具及其子进程；“The exception for launchd daemons doesn't apply to launchd agents”）、非 SMAppService agent 需设 AssociatedBundleIdentifiers、FB14944392、FB15568200
2. https://developer.apple.com/forums/thread/678819 — Quinn，“On File System Permissions”（2026-09-18 修订）：responsible code；ad-hoc 代码会反复弹窗
3. https://www.qt.io/blog/the-curious-case-of-the-responsible-process — responsible 继承规则、`responsibility_spawnattrs_setdisclaim`、entitlements 继承
4. https://developer.apple.com/tutorials/data/documentation/technotes/tn3127-inside-code-signing-requirements.json — TN3127（最后修订 2024-04-02）：DR；ad-hoc 的 DR “tied to that specific version of the code”（原文没有提 cdhash，cdhash 是 codesign 对 ad-hoc 的默认隐式 DR，见 [48]）；未签名代码没有 DR；TCC 记录 DR 并检查新版本；Developer ID DR 示例
5. https://developer.apple.com/tutorials/data/documentation/bundleresources/information-property-list/lssupportsgamemode.json — LSSupportsGameMode，macOS 26.0+、iOS/iPadOS 18.6+
6. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26-release-notes.json — 153127050、153125166、`nox86exec`（136764433）
7. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26_4-release-notes.json — 169228455：Rosetta 启动通知、allowRosettaUsageAwareness
8. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-27-release-notes.json — 175697313、169548657、176042635、166398727（`game-test-tool`，开启后禁用 Rosetta，非游戏进程可能崩溃）、168097174、163213094（升级后 Rosetta 不会自动恢复）
9. https://developer.apple.com/forums/thread/739387 — Frameworks Engineer（2023-10）：Game Mode 依据 LSApplicationCategoryType，要求全屏且位于最前
10. https://developer.apple.com/forums/thread/787702 — DTS（2025-06）：Game Mode 不迁移到另一个进程创建的窗口；Minecraft bundle ID 被特殊处理
11. https://developer.apple.com/forums/thread/818906 — 26.4 beta 的 ecosystemd GAME_CHECK 日志
12. https://derflounder.wordpress.com/2026/03/25/disabling-rosetta-awareness-messages-on-macos-tahoe/ — `com.apple.applicationaccess` / `allowRosettaUsageAwareness`
13. https://developer.apple.com/tutorials/data/documentation/xcode/signing-a-daemon-with-a-restricted-entitlement.json — 受限 entitlement 需要 app 形态 bundle 加 embedded.provisionprofile
14. https://developer.apple.com/tutorials/data/documentation/security/hardened-runtime.json — runtime 例外与资源 entitlement 列表；公证要求 hardened runtime
15. https://developer.apple.com/tutorials/data/documentation/bundleresources/entitlements/com.apple.security.cs.allow-unsigned-executable-memory.json — 不受 MAP_JIT 限制的 W+X 内存
16. https://developer.apple.com/tutorials/data/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon.json — W^X 对所有 app 生效；allow-jit 下只能有一个 MAP_JIT 区
17. https://support.apple.com/guide/security/rosetta-2-on-a-mac-with-apple-silicon-secebb113be1/web — Rosetta 2 的签名规则
18. https://support.apple.com/guide/security/gatekeeper-and-runtime-protection-sec5599b66df/web — Gatekeeper 的检查对象
19. https://support.apple.com/guide/mac-help/control-access-to-files-and-folders-on-mac-mchld5a35146/mac — 受保护的 Desktop/Documents/Downloads（页面版本为 macOS 27）
20. https://gitlab.winehq.org/wine/wine/-/raw/master/loader/wine_info.plist.in — Wine loader 内嵌 plist（org.winehq.wine、LSUIElement）；事实核查时 gitlab.winehq.org 被 Anubis 拦截，改用 GitHub 镜像复核，见 [59]
21. https://gitlab.winehq.org/wine/wine/-/raw/master/configure.ac — `-sectcreate __TEXT __info_plist`、zerofill 链接参数
22. https://gitlab.winehq.org/wine/wine/-/raw/master/dlls/winemac.drv/cocoa_app.m — mainBundle 的 CFBundleName、setApplicationIconImage；没有 openFiles 处理器
23. https://gitlab.winehq.org/wine/wine/-/raw/master/dlls/shell32/shellpath.c — `_SHCreateSymbolicLink` 链接到 $HOME 下各目录
24. https://github.com/frankea/Whisky — 仓库 README 与状态
25. https://raw.githubusercontent.com/frankea/Whisky/main/Whisky/Whisky.entitlements — Whisky 的 entitlements
26. https://raw.githubusercontent.com/frankea/Whisky/main/Whisky/Info.plist — 文档类型、URL scheme、文件夹用途说明
27. https://raw.githubusercontent.com/frankea/Whisky/main/WhiskyKit/Sources/WhiskyKit/Whisky/ShortcutCreator.swift — bash 脚本形式的 shortcut bundle
28. https://raw.githubusercontent.com/frankea/Whisky/main/WhiskyKit/Sources/WhiskyKit/Wine/Wine.swift — 用 Foundation Process 启动 wine64
29. https://github.com/gauthierpiarrette/highball — Highball 仓库（GPL-3.0；本文初稿核对 v0.9.37，2026-09-26 已发布 v0.9.38，见 [58]）
30. https://raw.githubusercontent.com/gauthierpiarrette/highball/main/Scripts/make-app.sh — 签名、公证、entitlements、Info.plist、TCC 注释
31. https://raw.githubusercontent.com/gauthierpiarrette/highball/main/Sources/HighballKit/MacAppStub.swift — ad-hoc 签名的 open 型 stub
32. https://raw.githubusercontent.com/gauthierpiarrette/highball/main/Sources/HighballKit/EngineStore.swift — 引擎布局、剥除 quarantine
33. https://github.com/stoicswe/Endfield_FineWine/issues/9 — CrossOver 26.2 的签名、provenance、SIGKILL（2026-09-23）
34. https://raw.githubusercontent.com/stoicswe/Endfield_FineWine/main/scripts/swap-into-crossover.sh — CrossOver 内部路径、DLV、ad-hoc 重新封存
35. https://www.applegamingwiki.com/wiki/CrossOver — “CrossOver-Hosted Application/wine” 路径
36. https://support.codeweavers.com/user-guides/crossover-mac-user-guide — launcher 位于 ~/Applications/CrossOver（引自搜索摘要）
37. https://torqer.app/how-to-enable-game-mode-on-mac-for-crossover-sikarugir-and-wine-games/ — （2026-07-08 更新）CrossOver/Whisky/Wine 游戏常常不会自动进入 Game Mode；`gamepolicyctl game-mode set on` 需要 Xcode
38. https://github.com/anthropics/claude-code/issues/80472 — 2026-07-23，macOS 27 beta 4（26A5388g）：Metal 缓存目录不可写时 `MTLGetShaderCachePath()` 为 nil，NSException 后 SIGABRT；issue 称按 bundle 分目录是 27 的变化，但本机 26.5 已在用
39. https://gist.github.com/aras-p/5a9ae8f1f7d9998f732aff26dfa62617 — Metal 缓存位置
40. https://github.com/bartekczyz/ai-profiles/pull/40 — “identity from the bundle its executable lives in”（2026-09-20）
41. https://eclecticlight.co/2023/05/10/how-macos-now-tracks-the-provenance-of-apps/ — provenance xattr
42. https://eclecticlight.co/2025/12/05/quarantine-macl-and-provenance-what-are-they-up-to/ — quarantine、macl、provenance
43. https://lapcatsoftware.com/articles/AppManagement.html — Ventura App Management（引自搜索摘要）
44. https://forum.winehq.org/viewtopic.php?t=7449 — Wine 不是沙箱、Z: 盘（引自搜索摘要）
45. https://github.com/ghostty-org/ghostty/issues/9263 — disclaim 的安全权衡（2025-10-19 关闭）
46. https://www.macrumors.com/2026/02/16/macos-tahoe-26-4-rosetta-2-warnings/ — 26.4 的警告
47. https://github.com/anegostudios/VintageStory-Issues/issues/8013 — 缺少类别导致 Game Mode 不触发
48. 本机实验 E1–E4 与系统文件（2026-09-27，macOS 26.5 25F71）：`/System/Library/PrivateFrameworks/Ecosystem.framework/{Support/ecosystemd, Versions/A/Resources/{assets_PICKLED_RIND_CORAL/CORAL.plist, assets_PICKLED_RIND_PUMPKIN/PUMPKIN.plist, Localizable.loctable}}`、`/usr/libexec/GamePolicyAgent` 的字符串、`log show --predicate 'process=="GamePolicyAgent"'`（07:20–07:40）的输出；事实核查补充的本机检查包括：对 ad-hoc 签名副本执行 `codesign -d -r-`（默认得到 `# designated => cdhash H"…"`，用 `-r='designated => identifier "x"'` 得到显式 identifier DR），以及 `ls $(getconf DARWIN_USER_CACHE_DIR)`（23 个 `<bundleID>/com.apple.metal` 目录）；探针源码在 scratchpad 的 probe/ 目录（如 `shim2.c`，分别用 execv 与 posix_spawn+waitpid）
49. https://developer.apple.com/forums/thread/129596 — Quinn：守护进程套上 app 外壳（2020 年原帖，已被 [13] 取代）
50. https://kb.filewave.com/books/end-of-life-statements/page/apple-eol-advisory-intel-based-apps-and-rosetta-dependencies-on-apple-silicon — 通知文案示例；universal app 仍可能带有 Intel 组件
51. https://developer.apple.com/forums/thread/751802 — Quinn（2024-06）：TCC 查找 “the nearest parent of the process that the user knows about”；推荐 SMAppService 或 `AssociatedBundleIdentifiers`
52. https://github.com/AprilNEA/OpenLogi/pull/1031 — 2026-08-27 合并：SMAppService 登记、经 `launchctl kickstart` 启动的 agent “as its own TCC responsible process”
53. https://github.com/YARC-Official/YARG/issues/1695 — ad-hoc 签名的 cdhash DR 导致每次构建后 TCC 重新弹窗；用 identifier 固定的 ad-hoc 签名作为规避（2026）
54. https://github.com/NousResearch/hermes-agent/issues/121857 — 同类问题与规避办法（2026）
55. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — 2026-07-31：首个 Apple Silicon 原生 CrossOver 构建在测试中（ARM64EC + FEX，目标 2027 年初）
56. https://github.com/gauthierpiarrette/highball/issues/6 — 事实核查者引用的 Highball issue，与 Rosetta 退场后的引擎路线有关（本文未单独复核其内容）
57. https://api.github.com/repos/frankea/Whisky/releases?per_page=5 — frankea/Whisky app-v3.7.0 发布于 2026-08-30T01:37Z
58. https://api.github.com/repos/gauthierpiarrette/highball/releases?per_page=3 — Highball v0.9.38（2026-09-26T20:17Z）、v0.9.37（2026-09-25）、v0.9.36（2026-09-22）
59. https://raw.githubusercontent.com/wine-mirror/wine/master/loader/wine_info.plist.in — Wine 官方 GitHub 镜像：CFBundleIdentifier `org.winehq.wine`、CFBundleName `Wine`、NSPrincipalClass `WineApplication`、LSUIElement 1
60. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_app.m — GitHub 镜像：没有 openFiles/openFile/openURLs 方法与 kAEOpenDocuments/GetURL 处理器；菜单用 mainBundle 的 CFBundleName；`transformProcessToForeground:` 里调用 `setApplicationIconImage:`
61. https://developer.apple.com/tutorials/data/documentation/bundleresources/entitlements/com.apple.developer.cross-architecture-support.json — 2026-09-27 请求返回 404；Apple 公开文档中找不到这个 entitlement

## 事实核查记录

2026-09-27 独立核查的结论。“已修改”指正文已按核查结果修订。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| TN3179（修订于 2026-02-17）：app 派生的 helper 做本地网络操作时，responsible code 是 app；按代码签名追踪身份；使用主程序 UUID；对 launchd daemon、root、Terminal/SSH 下的命令行工具及其子进程自动放行；本地网络授权无法重置（FB14944392） | 证实 | 修订历史为 2026-02-17、2025-07-18、2024-10-31；原文要点均与核查一致。“ad-hoc 追踪不可靠”是转述，不是原文。补充一点：TN3179 写明 “The exception for launchd daemons doesn't apply to launchd agents”，所以 SMAppService 的 `cider-agent` 不享受自动放行，只能依靠 responsible code 归属。已补进 §1 表、§7 图和参考来源 [1] |
| TN3127：ad-hoc 代码的 DR 是只对该版本有效的 cdhash 要求；TCC 记录 DR 并检查后续版本；Developer ID DR 为 anchor apple generic + identifier + Team ID 的 Developer ID 证书 | 证实（需更正措辞） | TN3127（最后修订 2024-04-02）只说 ad-hoc 的 DR “tied to that specific version of the code”，全文没有把 cdhash 和 ad-hoc 联系起来。cdhash 形式是 codesign 对 ad-hoc 的**默认**隐式 DR（本机实测）。显式 `-r='designated => identifier "x"'` 可以覆盖；据 YARG #1695、hermes-agent #121857，这样能跨重建保留 TCC 授权，但没有 anchor，安全性弱。Developer ID DR 的完整形式已写入摘要。已修改摘要、§1 表、风险 2 和参考来源 [4][48][53][54] |
| 26.0 发布说明的 153127050 与 `open` 规避办法；LSSupportsGameMode 为 macOS 26.0+；本机 26.5 上只有经 LS 启动且自身全屏的进程（主程序或 execv 的 stub）进入 on，posix_spawn 的子进程从未超过 paused | 部分正确 | 文档部分正确；同一份发布说明还有 153125166（LSSupportsGameMode 被忽略的问题已修复）；LSSupportsGameMode 也支持 iOS/iPadOS 18.6+。本机日志支持观察结果：execv 型约 3.5 秒进入 on（07:29:06），loader 即主程序也进入 on（07:30:12），posix_spawn 型停在 paused 约 5–6 秒后转 off。但证据很薄：每种变体只跑几次、每次约 5 秒，用的是 arm64 AppKit 探针而非真实 Wine。只能说明 spawn 模型在这几次运行中没拿到 Game Mode，不能说明“永远”拿不到，也不能说明 exec 是唯一途径。Apple 公开的规则只有“游戏类别、全屏、位于最前”；DTS（2025-06）说 Game Mode 不会迁移到另一个进程的窗口；`gamepolicyctl` 手动覆盖需要 Xcode。已修改摘要、§2（背景、E3 表、逐项结论、§7 图）、T4 和建议 4 |
| 26.4 发布说明 169228455（Rosetta 启动通知、`allowRosettaUsageAwareness`）；27 发布说明 175697313、169548657、176042635 | 证实 | 原文均已核实。§3 补充了：175697313 的列表也包括“unused Intel-based software”；166398727（`sudo game-test-tool enable`，仅 beta；开启后禁用 Rosetta，非游戏进程可能崩溃）；168097174（“使用 Rosetta 打开”的 app 改为原生启动）；163213094（升级到 27.0 后 Rosetta 不会自动恢复）。参考来源 [8] 已更新 |
| 26.5 的 CORAL.plist 有 214 条排除规则，第 0 条是 `responsible_developer_name = "CodeWeavers Inc."`，第 1 条是 `process_path = "*/steamapps/*"`；PUMPKIN.plist 的频率键；“嵌入组件”通知文案 | 证实（后果部分为推断） | 文件内容准确：212 条按 process_bundle_id、1 条按 responsible_developer_name、1 条按 process_path；PUMPKIN 另有 `customURLsEnabled=false`、`suRecentLaunchWindow=31536000`。这些是随系统附带的默认资源（文件日期 5 月 1 日），`ecosystemd` 日志有 `assetsLoaded=yes`，可能经 MobileAsset 替换；没有任何公开资料说明 CORAL 或 PUMPKIN。“每个 app 30 天最多一次”和“Cider 会收到嵌入组件通知”都是从键名和字符串推断的，没有实际观察到。已修改摘要和 §3，加上了 [推断] 标注 |
| Apple 的 “Signing a daemon with a restricted entitlement”：独立可执行文件不能嵌 provisioning profile，要包成带 `Contents/embedded.provisionprofile`、bundle ID 与 App ID 对应的 app 形态 bundle，公证需要 Hardened Runtime | 证实（套用到 Engine A 是推断） | 文档原文已核实。它讲的是使用 `networking.custom-protocol` 的 launchd daemon，套用到 Engine A loader 是本文的推断。`com.apple.developer.cross-architecture-support` 的文档页返回 404，公开资料里找不到；它的存在只来自 15 号报告对 xnu 头文件的阅读（这次没能重新取得），Apple 是否批准也未核实。已修改摘要、§4 A 行和建议 2（改为先向 DTS 确认能否申请），新增参考来源 [61] |
| §7 架构图与 P0 建议：wine 由 SMAppService LaunchAgent（cider-agent）派生时，“responsible = Cider.app → TCC / 本地网络 / Rosetta 记录都归 Cider” | 部分正确，**存疑** | 这是推断，没有验证，而且证据相互矛盾。TN3179 和 Quinn（论坛 751802，2024-06）暗示 SMAppService 或 `AssociatedBundleIdentifiers` 能让系统把 agent 关联到宿主 app。OpenLogi PR #1031（2026-08-27 合并）却观察到 SMAppService agent “as its own TCC responsible process”。launchd agent 也被明确排除在本地网络自动放行之外，设备 entitlement 可能要签在 agent 本身上。E1 只验证过 GUI 派生。本文判断：在 T13/T2/T3 用真实 agent 验证之前，不能假设 agent 派生的授权归到 Cider.app。已修改摘要、§1 表与结论、§7 图与签名矩阵、T1/T2/T3/T13、建议 1（v0 默认由 GUI 派生）、建议 3、风险 3 和未解问题 8，新增参考来源 [51][52] |
| §3 / 建议 5：macOS 28 的 legacy games 机制很可能复用 ecosystemd 的游戏过滤，声明游戏类别也许能让 Engine R 在 2027 年后继续可用 | 部分正确（结论没有依据） | 一手证据指向另一套机制：27 发布说明 166398727 把它描述为 “the new underlying system behavior”，beta 中用 `sudo game-test-tool enable` 开启，开启后禁用 Rosetta，“non-game processes might crash or behave unexpectedly”，正式版中不可用。Wine 栈有 wineserver、services、explorer、launcher 等非游戏进程。没有证据把它和 ecosystemd 那套只管通知的游戏过滤联系起来。Engine R 在 macOS 28 之后的可用性改为“未知”，新增 T15；27 升级后 Rosetta 不会自动恢复（163213094），已加进建议 5 和 T7。据报 CrossOver 27 正在迁往原生 ARM64 引擎（ARM64EC + FEX）[55][56]。已修改 §3、建议 5、风险 1 和未解问题 9 |
| §2：26.5 上 Metal 着色器缓存已位于 `$(getconf DARWIN_USER_CACHE_DIR)/<bundleID>/com.apple.metal`；27 beta 上目录建不出来时 Metal 会抛异常（claude-code #80472） | 证实 | 本机有 23 个 `<bundleID>/com.apple.metal` 目录，另有顶层 `com.apple.metalfe`。#80472（2026-07-23，27 beta 4）的崩溃路径是 `recordBinaryArchiveUsage:` 拿到 nil 路径，NSException 后 SIGABRT。该 issue 把按 bundle 分目录说成 27 的变化，但 26.5 已在用。26.5 上是否有同样的崩溃路径没有验证。探针的缓存目录已不在本机，按模型列出的缓存表无法再次复核。已在 §2、风险 4 和参考来源 [38][48] 中注明 |
| §1/§4：winemac.drv 没有 openFiles/openURLs 处理器；上游 loader 内嵌 plist 的 `CFBundleIdentifier` 是 `org.winehq.wine` 并带 LSUIElement；菜单名取自 mainBundle 的 CFBundleName，Dock 图标来自 `setApplicationIconImage` | 证实 | gitlab.winehq.org 被 Anubis 拦截，核查改用官方 GitHub 镜像（wine-mirror/wine master）。`wine_info.plist.in` 另有 CFBundleName `Wine`、NSPrincipalClass `WineApplication`；`cocoa_app.m` 里也没有 kAEOpenDocuments/GetURL 处理器，`setApplicationIconImage:` 在 `transformProcessToForeground:` 里调用。已在 §2 补充细节，新增参考来源 [59][60] |
| §5 / 摘要的竞品事实：frankea/Whisky app-v3.7.0（2026-08-30），entitlements 为 apple-events、allow-unsigned-executable-memory、audio-input、camera，shortcut 是没有 bundle ID 的未签名 bash 脚本；Highball v0.9.37（2026-09-25），已公证，entitlements 只有 audio-input | 部分正确 | Whisky 的事实准确（发布于 2026-08-30T01:37Z；entitlements 恰好这四项；`ShortcutCreator.swift` 写出 `#!/bin/bash`、带游戏类别、没有 CFBundleIdentifier、不签名）。Highball 的 `make-app.sh` 确实用 `--options runtime --timestamp`、只有 audio-input、发布前要求公证。但 v0.9.37 已不是最新版：v0.9.38 于 2026-09-26T20:17Z 发布。已更新摘要、§5 表和参考来源 [29]，新增 [57][58] |
