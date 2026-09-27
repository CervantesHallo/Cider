# macOS 平台集成：签名/权限/Game Mode/输入/中文/音视频/桌面

> 版本 v1 · 2026-09-27 · 依赖文档：`00-strategy-and-decisions.md`（ADR-001/002/003/005/006/007/010/011/012 为硬约束）、`01-architecture.md`（Spawner、ConfigKey、磁盘布局、签名清单）、`05-app-cli-ux.md`（shim、引导、ciderctl）、`06-profiles-recipes-compatdb.md`（动作白名单、mirrors.json）；调研 07、09、12、13、16 为主，10、17、18、21、22 为辅。1 人周 = 6 窗（00 号容量假设）。

## 目标与范围（含明确不做的事）

**目标**
1. 按 ADR-007/16 号报告的模型 A，给每类二进制定签名、entitlement、公证方式，建成可由 CI 判定的签名公证流水线和密钥托管。
2. TCC（麦克风、摄像头、本地网络、文件与卷）只以 Cider.app 的名义出现，每项都有用途说明和首次引导。
3. Game Mode、Dock 身份、全屏/Spaces/Retina、键鼠手柄、中文（IME、字体、区域）、音频、视频、剪贴板与通知，逐项给出实现落点（Wine 补丁、注册表、环境变量、App 代码）和验收。
4. 8 GB Mac 的内存与显存上报策略；Rosetta 生命周期（检测、安装引导、27 升级、28 legacy）；macOS 14–28 兼容测试矩阵。
5. 国内镜像、ingest 的平台侧部署（01/05/06 要求本文提供）。

**不做**
- 不引导用户关 SIP，不用 App Sandbox，不上 App Store；A3（关 SIP）只在维护者测试 Mac 卷 2 上用（ADR-003）。
- 产品路径不用私有 API `responsibility_*`、`gamepolicyctl`，只在实验里用；不默认走需要“辅助功能”或“输入监控”授权的路径。
- 不把 Wine 放进 Cider.app；不默认用 exec 型 shim（ADR-007）。
- 不打包大型 CJK 字体；Cider 引擎不链接 fontconfig。
- 2027 年不做：Windows 菜单并入 Mac 菜单栏（bug 40642）、Wine→Finder 拖出、动态空间音频、VideoToolbox 零拷贝硬解。
- 不写任何 iOS/iPadOS 相关内容；不为 HoYo 游戏改平台身份（GPU、显存按真实设备，见 §9）。

## 现状与关键事实

| # | 事实 | 来源 | 置信度 |
|---|---|---|---|
| F1 | 经 LS 启动的 GUI `posix_spawn` 出的子进程，TCC 与本地网络都归 Cider.app；SMAppService agent 派生时归属存疑 | [16 §1] | 实测 / 存疑 |
| F2 | TCC 记录 responsible code 的 DR。Developer ID 的 DR 跨版本有效；ad-hoc 默认是 cdhash DR，重建即失效 | [16 摘要、核查] | 证实 |
| F3 | 本地网络：连局域网 TCP、UDP 单播、广播/组播、`.local` 都要授权；终端运行的命令行工具豁免；按签名和主程序 UUID 识别；授权无法重置 | [09 §7]、[16 §1] | 证实 |
| F4 | E3（arm64 探针，样本少）：只有 exec 型 shim 和“loader 即主程序”进入 Game Mode on，spawn 型停在 paused。26.0 已知问题 153127050；`LSSupportsGameMode` 26+，14/15 用 `GCSupportsGameMode` | [16 §2]、[13 §2.4 核查] | 实测，证据有限 / 中 |
| F5 | Metal 系统缓存按进程内 `NSBundle.mainBundle` 分目录；上游 loader 内嵌 `org.winehq.wine`；winemac 没有 openFiles/openURLs 处理 | [16 §2 核查] | 证实 |
| F6 | ecosystemd 有“嵌入组件”通知，CORAL 忽略清单只豁免 CodeWeavers；频率按键名推断为同一 app 30 天一次 | [16 §3] | 文件证实 / 行为推断 |
| F7 | 升级到 27 后 Rosetta 不自动恢复（163213094）；28 的 legacy games 是另一套机制，beta 中 `game-test-tool enable` 会禁用 Rosetta，非游戏进程可能崩溃；26+ 有 `nox86exec=1` | [07 §6]、[16 §3 核查]、[00 裁定 5] | 证实 |
| F8 | 公证要求 Developer ID、runtime、timestamp、无 get-task-allow、ASCII XML entitlements；由内向外签，不用 `--deep`；代码目录应扁平 | [12 §2.3、§2.2 更正] | 证实 |
| F9 | Rosetta 下 x86_64 代码可不签，arm64 至少 ad-hoc；Cider 生成的 bundle 必须完整签名，否则 provenance 会导致进程被杀 | [12 §2.4]、[16 §6] | 证实 / 第三方 |
| F10 | winebus 只 dlopen SDL2；默认设置下 Xbox 和通用手柄只走 SDL，SDL 加载失败就整体消失；DS4、DualSense、Switch Pro、Joy-Con R 默认走 hidraw；按设备 `Devices\<VID>/<PID>` 的 `Hidraw=0` 才能改成按 Xbox 处理，`EnableHidraw` 只能强制启用 | [09 §3.1、§3.3 核查] | 证实 / 推断高 |
| F11 | 鼠标增量带加速；GCMouse（!11799）、透明光标（!11880，规避 26 隐藏光标后锁刷新率）都是开放 MR；ClipCursor 用私有 API，EventTap 退路需要辅助功能授权 | [09 §3.4] | 证实 |
| F12 | Cmd 默认映射为 Alt，Option 不映射；CX Hack 10912 EditMenu 是办公可用性的关键 | [09 §3.5] | 证实 |
| F13 | IME 走 NSTextInputClient→imm32；11.18 修了 59737（中文 HKL 下 WASD 失灵）；!12164 开放；CEF ≥M122 只有 TSF（Steam CEF 126），预编辑和候选窗位置会偏 | [09 §4.1]、[18 §6] | 证实 / 推断 |
| F14 | 不链接 fontconfig 时 Wine 用 CoreText 加载全部系统字体，但没有动态回退；Replacements 别名即可满足内部默认链接；链接 fontconfig 后可能看不到 AssetsV2 里的 PingFang | [09 §4.2 核查] | 证实 / 推断 |
| F15 | ACP 由 `LC_*` 决定；继承来的 `LC_CTYPE=UTF-8` 会让 ACP 落回 1252；bug 47277：zh_CN 且 winver ≥ Win7 时 WPF 死循环 | [09 §4.3]、[10 §2.2] | 证实 |
| F16 | `RetinaMode` 只从前缀全局键读取；Win32 全屏用窗口层级覆盖，不进原生 Space；EDR>1 时总上报 HDR | [09 §5] | 证实 |
| F17 | 音频：11.16 修了 480/512 帧爆音；downmix（!11982）、跟随默认设备（!11370）仍开放；动态空间对象为 E_NOTIMPL | [09 §1 核查] | 证实 |
| F18 | 视频必须带 GStreamer：winedmo 只做解复用，内置 FFmpeg 没有 libavcodec；1.28 的 vtdec 支持 H.264/HEVC/VP9/AV1 硬解 | [09 §2 核查]、[00 裁定 8] | 证实 |
| F19 | Mac 截图的 PNG/TIFF 不会合成 CF_DIB；不能从 Wine 拖出；托盘气泡（34645）和 Toast 都未实现 | [09 §6] | 证实 |
| F20 | M3 8 GB 实测 `recommendedMaxWorkingSetSize` 为 5.33 GiB（2/3 只是实测值）；winemac 不上报显存；8 GB 机器建议报约 4 GiB | [07 §7 核查] | 实测 / 推断 |

## 设计

### 1 签名、公证与二进制类别（ADR-007）

| 类别 | 位置 | 架构 | 签名 / runtime | entitlements | 公证 |
|---|---|---|---|---|---|
| Cider GUI | `Cider.app/Contents/MacOS/Cider` | arm64 | Developer ID / 开 | `device.audio-input`、`device.camera` | 随 DMG |
| ciderctl、cider-agent | `Contents/MacOS/` | arm64 | Developer ID / 开，`-i org.cider.{ciderctl,agent}` | 无（agent 只做汇合点；启用 AgentSpawner 前按 T13 重定） | 随 DMG |
| cider-probe | `Contents/Helpers/` | universal（x86_64 切片用于 Rosetta 检测） | Developer ID / 开 | 由 03 定 | 随 DMG |
| shim / handler 模板 | `Contents/Helpers/cider-{shim,handler}-stub` | arm64 | Developer ID（模板本身） | 无 | 随 DMG |
| 本机生成的 shim、handler | `~/Applications/Cider/*.app`、`App Support/Cider/Handlers/*.app` | arm64 | 本机 `codesign -s - --force` 整包签 | 无 | 否 |
| CiderGameHost（可选，模型 B） | `Contents/Helpers/CiderGameHost.app` | x86_64 | Developer ID / 开 | `cs.allow-unsigned-executable-memory`、`cs.disable-library-validation`、audio-input、camera（T8 二分后定） | 随 DMG |
| Engine R 树 | `App Support/Cider/Engines/<id>` | x86_64 | CI 逐个 Mach-O ad-hoc / 不开 | 无 | 否；完整性靠 manifest 与 `files.sha256` |
| Engine R Developer ID 变体 | 同上，`channel=devid` | x86_64 | Developer ID / 开 | 同 GameHost | zip 公证，不装订；T2/T3 失败时才启用 |
| CiderEngineA.app | App Support，独立下载 | arm64 | Developer ID + `embedded.provisionprofile` | 由 03 定（含 `cross-architecture-support`，前提是获批） | 单独公证、装订 |
| D3DMetal | `Components/d3dmetal/<ver>` | 实查 `lipo` | 保留 Apple 原签，只做 `codesign -v -R="anchor apple"` | — | — |
| PE（exe/dll） | 引擎与 bottle 内 | — | 属于资源，不签 | — | — |

`scripts/sign.sh`（`app-release.yml` 调用；x86_64 部分由 `engine-build` 调用 `scripts/adhoc-sign-tree.sh`）：

```bash
ID="Developer ID Application: <Name> (<TEAMID>)"; A=build/Cider.app/Contents
# Sparkle：非沙盒应用，按官方文档删除 XPCServices，只签 Autoupdate 与 Updater.app
codesign -f -s "$ID" --timestamp -o runtime $A/Frameworks/Sparkle.framework/Versions/B/{Autoupdate,Updater.app}
codesign -f -s "$ID" --timestamp $A/Frameworks/Sparkle.framework
for b in cider-probe cider-shim-stub cider-handler-stub; do codesign -f -s "$ID" --timestamp -o runtime -i org.cider.$b $A/Helpers/$b; done
codesign -f -s "$ID" --timestamp -o runtime -i org.cider.ciderctl $A/MacOS/ciderctl
codesign -f -s "$ID" --timestamp -o runtime -i org.cider.agent   $A/MacOS/cider-agent
codesign -f -s "$ID" --timestamp -o runtime --entitlements App/Cider/Cider.entitlements build/Cider.app
ditto build/Cider.app dist/Cider.app
hdiutil create -fs APFS -format ULFO -volname Cider -srcfolder dist Cider-$V.dmg
codesign -s "$ID" --timestamp Cider-$V.dmg
xcrun notarytool submit Cider-$V.dmg --key $P8 --key-id $KID --issuer $ISS --wait --output-format json > notary.json
xcrun stapler staple Cider-$V.dmg && xcrun stapler validate Cider-$V.dmg
```

**CI 门禁**（`scripts/verify-signing.sh`，任何一项失败都拒绝发布）：
- `codesign -d --entitlements :-` 的输出与 golden 完全一致（GUI 只有两项；全包不得出现 `get-task-allow`、`allow-dyld-environment-variables`）。
- `codesign --verify --deep --strict`。
- `spctl -a -t exec -vv` 的输出含 `source=Notarized Developer ID`。
- 每个 Mach-O 都有 `LC_BUILD_VERSION`、链接 libSystem；`lipo -archs` 符合上表。
- 引擎树：逐个 `codesign -v` 通过；`otool -L` 不含 `/opt/homebrew`、`/usr/local`、`fontconfig`；含 `libSDL2-2.0.0.dylib` 和 `libSDL3.0.dylib`。

**LaunchAgent**：`Contents/Library/LaunchAgents/org.cider.agent.plist` 中 `BundleProgram=Contents/MacOS/cider-agent`、`AssociatedBundleIdentifiers=[org.cider.app]`。27 上 launchd 拒绝带 quarantine 的 plist，首次运行时检测该属性，有则移除（T-27-1 验证）。检测到 App Translocation（`SecTranslocateIsTranslocatedURL`）时，引导用户把 App 移到 `/Applications`。

**密钥托管**

| 密钥 | 存放 | 使用 job |
|---|---|---|
| Developer ID Application `.p12` | GitHub environment `release`（需用户人工批准），离线加密备份一份 | app-release、engine-devid、engine-a-release |
| App Store Connect API key `.p8`（Developer 角色） | 同上 | 同上（notarytool） |
| Sparkle EdDSA | environment `release` | app-release |
| minisign：engines、data、timestamp、root | 按 01 §6：root 离线，两人持有 | engine-build、data-publish |
| Engine A provisioning profile | environment `engine-a` | P3 |
| 国内对象存储 RAM 子账号 AK（只有 Put/Get 权限） | environment `mirror` | mirror-sync |

runner 上每个 job 建临时 keychain（`security create-keychain` → import → `set-key-partition-list`），job 结束时删除。泄露预案见风险表。

### 2 TCC、本地网络与文件访问

Info.plist 用途说明写在 `App/Cider/InfoPlist.xcstrings`（开发语言 zh-Hans，另有 en），只放在 Cider.app 里：

| 键 | 触发 | zh-Hans 文案要点 |
|---|---|---|
| `NSMicrophoneUsageDescription`、`NSCameraUsageDescription` | 游戏语音、会议软件 | “Windows 程序需要使用麦克风/摄像头。授权记在 Cider 名下，对所有瓶子生效。” |
| `NSLocalNetworkUsageDescription` | 局域网联机、SSDP/UPnP、`.local` | “局域网联机和设备发现需要访问本地网络。” |
| `NSDocumentsFolderUsageDescription`、`NSDesktopFolderUsageDescription`、`NSDownloadsFolderUsageDescription` | 用户把这些文件夹连接到瓶子之后 | “你已把“文稿”连接到瓶子 X。” |
| `NSRemovableVolumesUsageDescription`、`NSNetworkVolumesUsageDescription` | 游戏库放在外置盘（01 的 `drives`） | “游戏库位于外置磁盘。” |
| `NSBluetoothAlwaysUsageDescription` | 防御项：如果 SDL3 经 CoreBluetooth 访问 BLE 手柄，缺这个键会导致进程崩溃 [推断] | “蓝牙手柄。” |

- **不声明**：`NSAppleEventsUsageDescription`（没有 apple-events entitlement）；`NSBonjourServices` 等 T3 证明需要时再加；不申请屏幕录制、辅助功能、输入监控。
- **本地网络**：引导第 6 步（05 §9）由 GUI 自己向网关发一个 UDP 单播，触发以 Cider 名义出现的弹窗。拒绝时的错误码以 T3 实测为准，预期是 `EHOSTUNREACH` 或 `EPERM`。检测到拒绝后，“联机”相关诊断指向“系统设置 › 隐私与安全性 › 本地网络”，并说明这项授权无法重置。开发和 CI 中从终端启动的进程会被豁免，所以本地网络的验收**只在经 LS 启动的实验室构建上**做。
- **文件**：新瓶子默认 `shellFolders=isolated`，游戏瓶子不映射 Z:（01 §9）。“连接 Mac 文件夹”走 `NSOpenPanel`，把选中的目录映射为盘符或 shell folder 链接。借助用户意图授予的访问，子进程 wine 能否继承，由 T12 验证。
- **共享授权的补偿**：所有瓶子共用 Cider 的麦克风授权（16 §6）。P2 做每瓶子的“允许录音”开关：`CIDER_AUDIO_CAPTURE=0` 时 winecoreaudio 不枚举采集端点（`cider/` 小补丁）。
- **自检**：`ciderctl status --json` 输出 `tcc.{microphone,camera}`，即 Cider 自身的 `AVCaptureDevice.authorizationStatus`（查询不会弹窗）。本地网络没有查询 API，输出 `unknown`。
- **responsible 存活**：GUI 是派生者。有会话在运行时按 Cmd+Q，弹出“全部结束 / 保持在菜单栏”，默认保持：切到 `.accessory`，只保留 MenuBarExtra。这样 responsible 进程和 events socket 一直存活（对应 16 未解问题 2）。

### 3 Game Mode、per-game shim 与 Dock 身份

```
Launchpad / Dock / Games app
   └─ ~/Applications/Cider/原神.app（arm64 stub，游戏类别，ad-hoc）
        ├─ 模型 E（默认）：XPC launch(pid) ─► GUI ─posix_spawn─► wine  （TCC 归 Cider.app；Game Mode 待 T4）
        └─ 模型 B（按游戏实验开关，仅在 T4 证明 E 无效后提供）：
             execv ─► Cider.app/Contents/Helpers/CiderGameHost.app/Contents/MacOS/CiderGameHost（x86_64）
                     ─dlopen─► Engine R 的 ntdll.so   （TCC 归 “Cider Game Host”，LS 身份仍是 shim）
```

- shim 的 Info.plist 由 05 §7 定义：`LSApplicationCategoryType=games`、`LSSupportsGameMode`、`GCSupportsGameMode`。**这些键只作用于 LS 身份**（Launchpad、Games app、Game Mode 判定）。模型 E 下 wine 进程的 `mainBundle` 是 loader 内嵌的 plist，所以 `NSPrefersDisplaySafeAreaCompatibilityMode` 这类 AppKit 键对 wine 进程无效，刘海兼容改在 winemac 中实现（§4）。
- loader 内嵌 plist 由 02 的 `macos-ux` 主题改为 `CFBundleIdentifier=org.cider.wine`、`CFBundleName=Cider`。这样 Metal 系统缓存落在 `C/org.cider.wine/`，NSUserDefaults 域也固定为 `org.cider.wine`（§5 的长按设置用到这一点）。
- CiderGameHost 做成完整的 bundle，自带用途说明和 `CFBundleName=Cider Game Host`，以规避 16 号报告未解问题 3（exec 后 TCC 读谁的 Info.plist）。它只对“Cider 直接派生游戏 exe”的条目有意义（例如 HoYo 的 playable 路线，或无需客户端的游戏）；Steam 派生的 game.exe 推断在任何模型下都拿不到 Game Mode（16 §2）。`guard=hoyoverse` 的瓶子钳制为模型 E，除非 Verdict 证据覆盖了 exec-host（接口交 09）。
- Game Mode 没有公开的查询 API。UI 只按 T4 结论和 profile 的 `game_mode` 字段显示“预期启用 / 不支持”；实验室用 `log stream --predicate 'process=="GamePolicyAgent"'` 判定。
- Dock：名称用 CX 22144（以应用名命名的 loader 链接），图标用 exe 图标加 Tahoe 遮罩（25964）；辅助进程不建 Dock 图标（24141）；AUMID 分组用 22310。这些都在 02 的 `macos-ux` 主题中实现。
- Cider.app 不声明游戏类别，避免 GUI 全屏时进入 Game Mode。是否能借此命中 ecosystemd 的游戏过滤，等 T6 结果再议。

### 4 显示：全屏、Spaces、Retina、刘海、HDR

| 设置（ConfigKey） | 作用域 | 落点 | 默认 |
|---|---|---|---|
| `display.hiDPI` | bottle（restart=prefix） | `Mac Driver\RetinaMode=y` + `HKCU\Control Panel\Desktop\LogPixels=192`（对应 CX 的 High Resolution Mode） | 关（与 CX 一致）；profile 可开；8 GB 上开启时提示显存代价 |
| `display.dpiCompat` ∈ auto/aware/unaware | app | `AppCompatFlags\Layers` 的 `HIGHDPIAWARE`/`DPIUNAWARE`（win32u 会读取） | auto |
| `display.fullscreen` ∈ overlay/capture | app | `AppDefaults\<exe>\Mac Driver\CaptureDisplaysForFullscreen` | overlay |
| `display.emulateModeset` | app | `AppDefaults\<exe>\X11 Driver\EmulateModeset`（由 win32u 读取，Mac 同样生效） | 游戏瓶子开 |
| `display.hdr` ∈ auto/hidden | app | `macos-ux` 补丁：`Mac Driver\HideHDR=y` 时 `hdr_enabled=false` | auto |
| `display.safeArea` ∈ ignore/avoid | app | `macos-ux` 补丁（参考 CX 20512）：内屏全屏时上报扣除刘海后的显示器矩形 | ignore |

- “按程序 Retina”不直接做，因为源码要求同一前缀内 DPI 一致（F16）。按程序的需求用 `dpiCompat` 或单独的 bottle 满足。
- Spaces 与多屏测试矩阵要覆盖：“显示器具有单独的空间”开/关（26.0 有 WindowServer 崩溃问题 153570422）、台前调度、Cmd-Tab、调度中心、外接屏热插拔、winemac 菜单“进入全屏”（原生 Space）。
- VRR 与限帧归 04（DXMT 呈现路径里的 `CAMetalDisplayLink`）；CX 18576（放开 `kDisplayModeSafeFlag` 模式）进 `macos-ux` 候选。

### 5 输入

**键盘预设**（`input.keymap`，app 级，写到 `AppDefaults\<exe>\Mac Driver`）：

| 预设 | Command | Option | EditMenu（CX 10912） | 用于 |
|---|---|---|---|---|
| `game`（游戏瓶子默认） | Alt（上游默认） | `Left/RightOptionIsAlt=y` | off | 游戏 |
| `office`（应用瓶子默认） | `Left/RightCommandIsCtrl=y` | 左 Option=Alt，右 Option 保留输入重音字符 | `key`（Cmd+C/V/X/A/Z 合成 Ctrl 组合键） | 办公、启动器 |
| `mac` | 上游默认 | 不映射 | off | 用户自选 |

- 退出与隐藏用 Wine 的 Cmd+Opt+Q / Cmd+Opt+H，这些快捷键印在 UI 的“快捷键”卡片上。
- 长按字符弹出重音菜单（ApplePressAndHold）可能影响按键重复（09 未解问题 3）。如果 T-IN-3 证实有影响，写入 `defaults write org.cider.wine ApplePressAndHoldEnabled -bool false`。该域由 §3 固定，对所有瓶子生效，无法按瓶子区分，所以做成全局设置项，默认关闭长按菜单。
- 开放 bug 53243、44382 登记进 known_issues。

**鼠标**：默认用 confinement 限制光标，`UseConfinementCursorClipping` 保持默认，EventTap 退路（需要辅助功能授权）不作为默认。!11799（GCMouse 原始输入）和 !11880（透明光标）进 `mr/` 类补丁。
- `input.rawMouse` 为 app 级开关，关闭时注入 `WINE_DISABLE_GCMOUSE=1`。
- 透明光标先作为选项；在 M3 120 Hz 上用两款免费 FPS 游戏做回归，确认修掉“锁刷新率”后，在 26+ 上默认开启。
- GCMouse 是否会触发输入监控授权待测（T-IN-2）。如果会，`rawMouse` 保持 opt-in，并在 UI 中写明。

**手柄**
- 引擎自带 `lib/libSDL2-2.0.0.dylib`（sdl2-compat 2.32.x）和 `lib/libSDL3.0.dylib`（SDL 3.4.x），winebus 的 rpath 指向引擎 `lib/`。CI 冒烟时用 `DYLD_PRINT_LIBRARIES=1` 确认两者都从引擎目录加载。
- 移植 CW HACK 19629（macOS 下 PS4/PS5 手柄的 BT 震动 hint）。
- **自检**：每次会话后扫描 wine.log 中的 winebus SDL 初始化行。失败时 UI 报“手柄驱动未加载”，不静默（F10）。
- **PS/Switch 按 Xbox 处理**：`input.pad.<VID:PID>` ∈ native/xbox，bottle 级，restart=wineserver，写入：

```reg
[HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\WineBus\Devices\054C/0CE6]
"Hidraw"=dword:00000000        ; DualSense → SDL → XInput 兼容
```

  设备表：DS4 `054C/05C4`、`054C/09CC`，DualSense `054C/0CE6`，DualSense Edge `054C/0DF2`，Switch Pro `057E/2009`，Joy-Con R `057E/2007`。PID 以 SDL 的 controller list 为准，由 CI 比对。不使用全局 `DisableHidraw=1`，它会连带关掉摇杆和方向盘。Joy-Con L 写 `057E/2006` 的 `Hidraw=1`，使左右两侧行为一致（PID 待核实），同时向上游提交 PID 笔误的修复。
- HoYo 三款原生支持 DualSense（22 §1），所以 `guard=hoyoverse` 的瓶子默认 native。
- 手柄面板（05 APP-22）用 `GCController` 在宿主侧列出设备，不用 IOHID，以免触发输入监控；另提供按钮打开 `joy.cpl`。

### 6 中文与 CJK

**字体**：建 bottle 时由 `zh-CN` 模板层经 06 的 `fonts.substitute` 动作写入 `HKCU\Software\Wine\Fonts\Replacements`（每个 Windows 字体名一个 REG_SZ 值，ADR-012）。只写字体族名，不写 AssetsV2 的路径：

```yaml
fonts.substitute:            # 目标族名: [Windows 字体名...]
  Songti SC:   [SimSun, NSimSun, KaiTi, FangSong]           # KaiTi/FangSong 是否改用 Kaiti SC 待 T-CJK-1
  PingFang SC: [Microsoft YaHei, Microsoft YaHei UI, DengXian, SimHei]
  Songti TC:   [MingLiU, PMingLiU]
  PingFang TC: [Microsoft JhengHei]
  Hiragino Sans: [MS Gothic, MS UI Gothic, Meiryo, Yu Gothic]
  Hiragino Mincho ProN: [MS Mincho]
  Apple SD Gothic Neo:  [Malgun Gothic, Gulim]
```

- 所有 bottle 都写日文和韩文映射，简体、繁体映射只在对应模板层写入。
- `FontLink\SystemLink` 只作补充，等 T-CJK-1 验证文件名能匹配后再用。
- 度量兼容问题通过 profile 按需下载思源黑体单字重子集（走镜像，约 8–15 MB/字重 [估计]），默认不下载。
- 引擎 configure 显式加 `--without-fontconfig`，由 §1 的 CI 门禁把关。

**区域（内置 Locale Emulator）**：EnvBuilder 从空环境开始，只放行 `HOME USER TMPDIR PATH=/usr/bin:/bin:/usr/sbin:/sbin`，绝不继承 `LANG`、`LC_*`、`DYLD_*`。

```
zh bottle（ADR-012）:    LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8                                → ACP 936，UI zh-CN
转区 (system≠ui):        LANG=zh_CN.UTF-8 LC_CTYPE=ja_JP.UTF-8 LC_MESSAGES=zh_CN.UTF-8（不设 LC_ALL）→ ACP 932
跟随系统:                Locale.current 的语言和地区 → xx_YY.UTF-8（查 /usr/share/locale；没有就用 en_US.UTF-8）
```

- `locale.system` 和 `locale.ui` 是 app 级、会话级键，对应 05 §12 的五个选项。
- 诊断包记录 `cmd /c chcp` 的结果。

**bug 47277（WPF + zh_CN）**
1. 立即：profile `app.wpf-zh-47277` 用 CiderPE 识别出 CLR 头并引用 PresentationFramework 的程序，把它设为“系统 zh-CN、UI en-US”：`LC_CTYPE=zh_CN.UTF-8 LC_MESSAGES=en_US.UTF-8`。这样保留 GBK，避开 zh-CHS 卫星程序集的查找 [推断，T-CJK-4 验证]。如果无效，退回 bug 中给出的 `LC_ALL=C`，并警告乱码风险。
2. 根治：在 `conformance` job（windows-latest）对照 `GetLocaleInfoEx("zh-CN", LOCALE_SPARENT)` 和 .NET 资源回退行为，确认修复点后进 `cjk` 主题并提交上游 MR。

**IME**
- 基线 ≥11.18（已包含 59737 修复）；!12164 合入后跟进。
- CEF：≤M121 的启动器用 cider-rules 追加 `--disable-features=TSFImeSupport`，改走 IMM32；≥M122（Steam）接受“能上屏、位置偏”，记为已知问题，归 18/08 跟踪。
- 测试集 T-CJK-3：记事本、WordPad、一款 Unity 输入框、一款 UE 输入框、Steam 聊天、HoYo 聊天（只在实验室卷 1）× 系统拼音、双拼、搜狗、微信输入法、鼠须管。每组检查三项：候选窗位置、预编辑显示、按住 WASD 时切换输入法是否丢键。
- 其余乱码场景见 09 §4.3 的表，写进 FAQ 和诊断规则。

### 7 音频与视频

**音频**
- 进 `mr/` 类补丁：!11982（多声道 downmix）、!11370（跟随系统默认设备）。上游合入后自动丢弃。
- 保持 `EnableAppNap=false`。
- 空间音频的动态对象不做，FAQ 说明可用系统的“空间化立体声”代替。

**视频：引擎内置的裁剪版 GStreamer**（按 01 的约定放在引擎内，不作为组件）
- v0（P1，省配额）：取官方 1.28.x universal runtime pkg，固定 sha256，然后 `lipo -thin x86_64`，按白名单保留插件，用 `osxrelocator` 把安装名改成 `@loader_path` 形式，最后逐个 ad-hoc 重签。
- v1（P2，需要打补丁或体积超预算时）：改为 cerbero 裁剪构建。
- 插件白名单：core `coreelements`；base `playback typefindfunctions app audioconvert audioresample videoconvertscale volume`；good `isomp4 matroska audioparsers wavparse id3demux`；bad `applemedia videoparsersbad mpegpsdemux mpegtsdemux`；ugly `asf`；`libav`（LGPL FFmpeg，关闭 nonfree 和 GPL）。
- 不带 GPL 插件、编码器和 gio 模块。
- 体积门禁：解包后 ≤80 MB [估计]。

```
lib/gstreamer-1.0/*.dylib   libexec/gstreamer-1.0/gst-plugin-scanner
env: GST_PLUGIN_SYSTEM_PATH=<eng>/lib/gstreamer-1.0  GST_PLUGIN_SCANNER=<eng>/libexec/gstreamer-1.0/gst-plugin-scanner
     GST_REGISTRY=~/Library/Caches/Cider/gst/<engine-id>/registry.bin  GST_PLUGIN_FEATURE_RANK=vtdec_hw:MAX
```

- **过场动画冒烟集**：用 mingw 构建的 `cider-mediatest.exe`，通过 MF Source Reader、DirectShow 和 XAudio2 各解 120 帧，与参考结果比 SSIM 和 PCM 哈希。样片 8 个：WMV/WMA、MP4 H.264/AAC、HEVC、VP9、AV1、xWMA、MPEG-1、Bink（对照组，不经 GStreamer）。
- 在 M3 上 H.264/HEVC/VP9/AV1 必须选中 vtdec，用 `GST_DEBUG=vtdec*:4` 断言。
- 跟踪上游 #60307、#60326 这类 MF 回归。按游戏 `DisableGstByteStreamHandler=1` 的开关归 04/10。
- GStreamer 出安全更新（如 1.28.7）后 7 天内发引擎补丁版 `-c(n+1)`。

### 8 桌面集成

| 功能 | 方案 | 落点 | 阶段 |
|---|---|---|---|
| 剪贴板图片 | 用 ImageIO 把 `public.png`/`public.tiff` 合成为 `CF_DIB`/`CF_DIBV5`，反方向加 PNG | winemac `clipboard.c`，`cider/` 主题并提交上游 | P1 |
| 剪贴板文件 | 补 `public.file-url` ↔ `CF_HDROP` | 同上 | P2 |
| 托盘 | 上游 `NSStatusItem`；吵闹的启动器按 app 设 `NoTrayItemsDisplay` | 注册表 | P1 |
| 托盘气泡 → 通知 | winemac 收到 `NIF_INFO` 时向 `CIDER_EVENTS` 发 `{"event":"notify","title","body","exe"}`，GUI 用 `UNUserNotificationCenter` 显示，副标题为应用名 | `cider-integration`（01 §2）+ App | P2 |
| WinRT Toast | 不做（2027） | — | — |
| 菜单栏 | winemac 只提供 app 菜单（zh 文案来自 po）；运行中的程序列在 Cider 的 MenuBarExtra | 05 | P1 |
| 拖放 | Mac→Wine 已可用；Wine→Mac 不做 | — | — |
| 打印 | 上游 CUPS；测试 AirPrint/IPP Everywhere 打印机 | 测试 | P2 |
| 链接、URL、文件关联 | `winebrowser` 调用 `/usr/bin/open`；协议与文档由 handler shim 负责（05 §7） | 05 | P2 |

### 9 内存策略（8 GB）

```
rmws = MTLCreateSystemDefaultDevice().recommendedMaxWorkingSetSize     // 由 probe 在运行时读取，不假设 2/3
vram = floor_256MiB(0.75 × rmws)                                        // M3 8GB：5461 MiB → 4096 MiB
if pe_arch == i386 || api ∈ {d3d8, d3d9}: vram = min(vram, 4096 MiB)   // 防 32 位溢出；profile 可继续下调
if guard == hoyoverse: 只允许 auto（按真实设备计算，不接受手工数值）
```

- 落点：wined3d 用 `HKCU\Software\Wine\Direct3D\VideoMemorySize`（MB，可按 app 放到 AppDefaults）。DXMT 先查源码里 `DedicatedVideoMemory` 的来源，没有可配置项就向上游提配置项。D3DMetal 只记录它上报的值，不改。`mem.vram` 键交给 04 实现。
- GUI 用 `DispatchSource.makeMemoryPressureSource([.warning,.critical])` 监听内存压力，写入会话 `events.jsonl`，并显示横幅“内存紧张：建议关闭 …”。
- 8 GB 默认值：`display.hiDPI` 关；同时运行第二个游戏会话时提示内存风险；不给 wineserver 加 `-p` 常驻参数；HoYo 预检加 `mem.low` 警告（06 的 `warn`）。MetalFX 和着色器编译并发归 04。

### 10 Rosetta 生命周期与 macOS 版本适配

`cider-probe --json` 中本文负责的字段（01 的 HostCaps）：

```json
{"macos":{"version":"27.0","build":"26A…"},"chip":"Apple M3","memBytes":8589934592,
 "metal":{"recommendedMaxWorkingSetSize":5726633984,"unified":true},
 "rosetta":{"state":"ok","checkedAt":"2026-10-03T09:00:00Z"},
 "displays":[{"builtin":true,"hz":[24,120],"edrPotential":16.0,"notchPt":32}],
 "tcc":{"microphone":"notDetermined","camera":"authorized"}}
```

```
rosetta.state（App 启动时和每次 Engine R 会话前检查，结果缓存 60 秒）：
  用 posix_spawnattr_setbinpref_np(CPU_TYPE_X86_64) 启动 cider-probe --translated
  EBADARCH                        → absent   → 安装引导
  启动即崩溃或被杀                → blocked  → 说明原因（nox86exec 或系统策略），不引导安装
  输出 translated=1               → ok
  macOS ≥ 28                      → 按 T15 结论细分为 legacy-only 或 unavailable（ARC-17 的 env gate）
```

- **安装引导**（T7 定最终顺序；Cider 始终不接触密码）：①如果 T7 证实不需要管理员权限，直接执行 `softwareupdate --install-rosetta --agree-to-license`；②否则“复制命令 / 打开终端”，让用户执行 `sudo …`。安装成功后自动复检。
- 模板 bottle 预先 `wineboot` 一次（05 §4），顺便让 oahd 完成 Mach-O 的 AOT 预热。
- **通知**：引导中预先解释“Cider 包含将不再工作的组件”这条通知（26.4+）。第 1 周经 Feedback Assistant 和 DTS 申请把 Cider 的开发者名加入 CORAL（0.2 窗）。27 上检查“设置 › 通用”的 Intel 清单和“显示简介”标记是否会点名引擎（T6），并据此准备 FAQ 措辞。
- **宿主已知问题库**：交 06 定 schema，`cider-data/host-issues/`，预检中显示警告。示例：

```json
{"id":"host.rosetta-deadlock-2026","match":{"macos":">=26.2 <26.5"},"affects":{"exe":["D2R.exe","Overwatch.exe"]},
 "severity":"warn","msg":"host.rosetta_deadlock","tracking":"FB21763885"}
```

**兼容测试环境**

| 环境 | 系统 | 用途 |
|---|---|---|
| CI `macos-15`、`macos-26` arm64（有 `macos-27` 镜像后加入） | 15.x、26.6 | 签名门禁、引擎冒烟，无 GPU |
| 开发机 M3/8 GB | 26.5 | 8 GB 档、经 LS 启动的 TCC 实验、日常 |
| 测试 Mac 卷 1 | 27.x 正式版 | 发布验证、HoYo、LRS、T 系列 |
| 卷 2（A3） | 27.x | Engine A、`nox86exec=1`；永不做米哈游测试 |
| 卷 3（beta） | 27.x beta → 28 beta | T15（`game-test-tool`）、beta 首周复跑 |
| VM（在测试 Mac 上） | 14.x、15.x | 最低版本冒烟，不测 GPU 性能 |

测试 Mac 到货前，T15 在开发机外置 SSD 的 27.x beta 卷上做（00 硬件预案）。规则：Apple 每发一个新 beta，7 天内跑完 `platform-suite`；28 beta 1 在首周跑（00 P3）；结果写入 `docs/compat/macos-<ver>.md`。

### 11 本文向 ConfigKey 注册表（01 §7）新增的键

| 键 | 作用域 | restart | 物化方式 | 06 动作名 |
|---|---|---|---|---|
| `locale.system`、`locale.ui` | bottle、app | session | env | `locale` |
| `input.keymap`、`input.editMenu`、`input.rawMouse` | bottle、app | session | AppDefaults、env | `input` |
| `input.pad.<vid:pid>` | bottle | wineserver | WineBus 注册表 | `input` |
| `display.*`（§4） | 见 §4 | 见 §4 | 注册表 | `retina`（已有）、`display` |
| `mem.vram` | app | session | 注册表或后端配置 | `vram` |
| `integration.trayIcons`、`audio.capture` | app、bottle | session | 注册表、env | `integration` |
| `gamemode.host` ∈ spawn/exec-host | app | session | Spawner 选派生模型 | `game_mode`（已有） |

`guard=hoyoverse` 的钳制：`mem.vram=auto`、`gamemode.host=spawn`。

### 12 国内网络：镜像与 ingest 的平台侧

- **镜像 v1**：国内对象存储（阿里云 OSS 或腾讯云 COS，二选一，P2 首周测速后定），使用服务商默认域名。自定义域名要先备案，所以国内 CDN 加速等备案域名就绪后再上（ADR-012）。
- 对象路径 `/<sha256[0:2]>/<sha256>`（01 §6），`mirrors.json` 由 data 角色签名（06）。
- `mirror-sync.yml` 在 app、engine、data 发布后触发：用 `ossutil`/`coscli` 按 sha 上传，再 `HEAD` 校验大小。
- 镜像内容：DMG 与 cn appcast、引擎（含 GStreamer、SDL）、可镜像组件、数据快照、可选字体包。D3DMetal 不进镜像（04 定）。游戏本体走官方 CDN。
- **测速**：3 个源并发 GET `/probe/256k.bin`，取 p50 最低者，结果缓存 24 小时；失败时按顺序换源。
- **ingest**（仅限用户主动提交的脱敏报告，P2）：国内用函数计算 HTTP 触发器（默认域名）写入私有桶 `ingest/`，海外用同一函数的海外区域实例，客户端按测速选择。单条 ≤256 KB，每 IP 限速。每日 Action 拉取后进入 cider-data 审核队列。

### 13 请 02 纳入补丁队列的平台补丁

| 补丁 | 来源 | 主题 | 阶段 |
|---|---|---|---|
| loader 内嵌 plist 改为 `org.cider.wine`/`Cider` | Cider | `macos-ux` | P0 |
| EditMenu 10912、Dock 22144/24141/25964、AUMID 22310、焦点 18896、`force_backing_store` 14364 | CX 26.3 | `macos-ux` | P1 |
| SDL 震动 hint 19629 | CX | `macos-ux` | P1 |
| !11982、!11370、!11799、!11880、!12164（合入前按需） | 上游 MR | `mr/` | P1 |
| Joy-Con L PID 修正 | 自提上游 | `up/` | P1 |
| 剪贴板图片与文件 URL | Cider→上游 | `cider/` | P1–P2 |
| `HideHDR`、safe area（参考 20512）、显示模式 18576 | Cider / CX | `macos-ux` | P2 |
| `NIF_INFO` 通知事件、`CIDER_AUDIO_CAPTURE` | Cider | `cider-integration` | P2 |
| 47277 修复 | Cider→上游 | `cjk` | P2 |

## 实施计划

| 编号 | 工作包 | 产出 | 验收标准 | 估时（人周） | 依赖 | 阶段/里程碑 |
|---|---|---|---|---|---|---|
| PLT-01 | 签名公证流水线 | `sign.sh`、`verify-signing.sh`、`app-release.yml`、临时 keychain、environment 密钥 | 测试 DMG 通过 `spctl` 和 `stapler validate`；entitlements golden 通过；篡改一个文件后门禁变红 | 0.5 | ADP（P0 第 1 周）、APP-01 | P0→M-B |
| PLT-02 | Info.plist、entitlements 与用途说明 | InfoPlist.xcstrings（zh/en）、`status --json` 中的 tcc 字段 | 两种语言齐全；GUI 派生的探针请求麦克风时，弹窗显示 Cider 名称和中文文案 | 0.2 | 01 | P0 |
| PLT-03 | 平台实验 I：T1/T2/T3/T12/T13 | 实验报告与结论表 | 每项有“模型 × 系统版本”结果；ADR-007 的 Developer ID 变体是否启用有书面结论 | 0.5 | 装 Rosetta、Engine R v0 | P0 |
| PLT-04 | 平台实验 II：T4/T5/T14 | 用真实 Wine 重做 E3，每种变体 ≥5 次，每次全屏 ≥60 秒 | 得出“模型 E 能否进入 Game Mode”的结论，写进 ADR-007 复议记录 | 0.4 | PLT-03 | P0 |
| PLT-05 | Rosetta 生命周期 | probe 的 rosetta/metal/display 字段、env gate、安装引导、T6/T7、CORAL 申请 | 在 `nox86exec` 卷和无 Rosetta 的 VM 上分别得到 blocked 和 absent，引导走通；工单号存档 | 0.5 | 01 ARC-05 | P0→M-A |
| PLT-06 | shim/handler stub 与本机签名器 | 两个 arm64 stub、`ShimSigner`、T10/T11 | 50 个 shim `codesign --verify --strict` 全过；破坏签名后被拒并提示“重建” | 0.3 | 05 APP-14 | P1 |
| PLT-07 | CiderGameHost（条件：T4 证明模型 E 无效） | x86_64 host bundle、开关、TCC 说明 | 3 款直接启动的游戏在 60 秒全屏中进入 Game Mode；授权弹窗显示 “Cider Game Host” | 0.6 | PLT-04、T8 | P2 |
| PLT-08 | 键盘与鼠标 | 三套预设、EditMenu、长按设置、rawMouse 与透明光标选项 | 办公预设下记事本 Cmd+C/V 可用；120 Hz 上开透明光标后帧率不再被锁 | 0.6 | 02 `macos-ux` | P1→P2 |
| PLT-09 | 手柄 | 引擎内 SDL、19629、`input.pad` UI、Joy-Con L、自检 | Xbox 手柄在 joy.cpl 可用；DualSense 切到 xbox 后在只支持 XInput 的游戏里可用、摇杆不受影响；删掉 SDL 后 UI 报错 | 0.5 | 02 引擎构建 | P1→M-B |
| PLT-10 | CJK 字体与区域 | zh-CN 模板层、`--without-fontconfig` 门禁、EnvBuilder 的 locale、转区 | 中文瓶子里 `chcp` 输出 936；winecfg 和记事本无豆腐块；外部导出 `LC_CTYPE=UTF-8` 时 ACP 仍为 936 | 0.4 | 05 APP-03 | P0→M-B |
| PLT-11 | 47277 与 IME 测试集 | WPF profile、conformance 对照、`cjk` 补丁、T-CJK-3 | WPF 样例程序在中文瓶子中启动；IME 测试集在 1.0 前 ≥80% 绿，2027-12 全绿 | 0.7 | PLT-10、11 号文档 conformance | P1→P2 |
| PLT-12 | 音频 | 两个 MR 移植、音频矩阵脚本 | 7.1.4→立体声对白可闻；运行中切换 AirPods 不中断；480 帧流无爆音 | 0.3 | 02 | P1 |
| PLT-13 | GStreamer 裁剪包与冒烟集 | 引擎内 GStreamer、`cider-mediatest.exe`、8 个样片 | 8/8 通过；vtdec 断言通过；体积 ≤80 MB | 0.6 | 02 engine-build | P1→M-B |
| PLT-14 | 显示与桌面集成补丁 | §4、§8 的键与补丁 | 高分模式、模式模拟、HideHDR、剪贴板截图粘贴到 mspaint、气泡转系统通知各有自动或半自动用例 | 1.0 | 02 | P1→P2 |
| PLT-15 | 内存策略 | vram 计算与各后端落点、内存压力事件、8 GB 默认值 | M3 8 GB 上 wined3d 与 DXMT 报告 4096 MiB；critical 事件出现在会话日志和横幅中 | 0.4 | 04 | P2 |
| PLT-16 | 镜像与 ingest | 存储桶、`mirror-sync.yml`、测速、ingest 函数 | 国内无代理下载引擎 ≤3 分钟；镜像返回坏字节时自动换源 | 0.5 | 01 ARC-13 | P2 |
| PLT-17 | 兼容矩阵与 beta 流程 | `platform-suite`、T15 报告、host-issues、`docs/compat/` | G-T15 按期出结论；27.x 每个 beta 7 天内有报告 | 0.6 | 测试 Mac 或外置 SSD | P0（T15）→P3 |
| PLT-18 | Engine A 签名形态 | `engine-a-release.yml`、profile 注入、单独公证 | CiderEngineA.app 通过 `spctl`，并在卷 2 上不被 AMFI 杀掉（T9） | 0.3 | G-ENT、03 | P3→M-D |
| PLT-19 | macOS 28 首日（平台部分） | env gate 的 28 分支、FAQ | 28 上打开 R bottle 必出说明，无静默失败 | 0.3 | G-T15、ARC-17 | P4→M-E |

合计约 9.2 人周（约 55 窗），其中 P0 约 2.3 人周。T 系列实验要人点授权弹窗、看日志，按每周约 3 小时的实机验证工时单独记账（00 资源节奏）；AI 窗主要用来写探针、脚本和补丁。重构类补丁移植放在周配额前段，FAQ 和文案放在后段。

## 测试与验收

- **CI**（macos-15/26）：签名门禁（§1）；引擎门禁（ad-hoc 签名、无 fontconfig、SDL 从引擎目录加载、`DYLD_PRINT_LIBRARIES`）；EnvBuilder 快照测试（外部 `LC_CTYPE=UTF-8`、`DYLD_*` 不会漏进子进程）；reg 物化 golden（字体、WineBus、keymap）；`cider-mediatest` 中不依赖 GPU 的子集。
- **cider-lab `platform-suite`**（卷 1，每晚跑 + 每个 beta 跑）：T1–T15（16 §7 的定义）；T-CJK-1（字体族名枚举）至 T-CJK-4（47277）；T-IN-1（手柄路径）至 T-IN-3（长按）；T-AV-1（音频矩阵，半自动）、T-AV-2（过场动画集）；T-DISP（Spaces 矩阵）；T-27-1（agent plist 的 quarantine）；T-CLOUD（Chrome `--app` 窗口中的手柄与 Pointer Lock，服务 09 的网页云路线）。
- 本地网络和 TCC 用例必须经 LS 启动、在全新 macOS 用户或 VM 中执行（本地网络授权无法重置）；麦克风和摄像头用 `tccutil reset Microphone org.cider.app` 重置。
- **里程碑门禁**：
  - M-A：已签名、已公证的开发版 DMG；M3 上 Rosetta 三态检测正确；T1/T2 有结论。
  - M-B（0.1）：已装订的 DMG 在干净的 27 用户上首次打开无警告；中文瓶子 ACP 936、无豆腐块；Xbox 手柄和 DualSense 切换可用；过场动画集 8/8；本地网络弹窗显示 Cider 名义。
  - M-C（1.0）：`platform-suite` 在 26 与 27 上全绿（IME 集 ≥80%）；8 GB 显存规则生效；T4 结论已落地（模型 E 或 GameHost）。
  - M-E：28 首日 `platform-suite` 报告在 72 小时内发布。

## 风险与预案（触发条件 → 动作）

| 风险 | 触发条件 | 动作 |
|---|---|---|
| 模型 E 拿不到 Game Mode | T4 中 spawn 型 5 次都没进入 on | 做 PLT-07（ADR-007 复议）；如果模型 B 用真实 Wine 也失败，UI 删除 Game Mode 承诺，shim 只保留 Launchpad 和 Games app 用途 |
| TCC 或本地网络不归 Cider | T2/T3 失败 | 启用 Engine R Developer ID 变体（带唯一 UUID），复测；在 UI 中说明 |
| agent 自成 responsible | T13 | 维持 GUISpawner；agent 不加设备 entitlement |
| Rosetta 通知引发恐慌 | 反馈或论坛中集中出现 | 引导页和 FAQ；催办 CORAL；不给 Cider.app 声明游戏类别 |
| 28 的 legacy 机制不接纳 Wine | T15 失败（G-T15） | 27.x 起显示迁移提示；HoYo 在 28 上默认走官方云；按 00 上调 Engine A 投入 |
| 私有 API 在新 beta 中失效（光标限制、CALayerHost） | `platform-suite` 在 beta 上红 | 7 天内出 issue 与临时规避；EventTap 退路只在用户同意授权后按游戏启用 |
| SDL 加载失败导致手柄消失 | 自检报错率 >1% | 优先发引擎补丁版；自检规则进 DiagRule |
| PingFang 族名或路径随系统变化 | T-CJK-1 在 beta 上红 | 修改 Replacements 映射并经数据热更新下发（fonts 动作），不必发引擎 |
| GStreamer 安全公告 | 上游发布安全版 | 7 天内发引擎补丁版，同时同步镜像 |
| 公证变慢或服务中断 | `notarytool` 超过 60 分钟 | 重试 2 次；App 发布顺延，引擎和数据通道不受影响 |
| Developer ID 或 ASC key 泄露 | 发现异常签名或公证记录 | 向 Apple 吊销、换证，重签公证当前版本；Sparkle、minisign 独立不受影响；公告 |
| 8 GB 机器 swap 抖动 | critical 事件 ≥3 次/会话 | 预设降档；预检警告；建议 16 GB |
| 47277 的规避无效 | T-CJK-4 失败 | 按程序退回 `LC_ALL=C` 并警告；优先做根治补丁 |

## 未决问题与需实机验证的点

1. **对 ADR-007 的顾虑（按 ADR 执行）**：模型 E 下 shim 的 Info.plist 只影响 LS 身份。05 §7 按 profile 写入 shim 的 `NSPrefersDisplaySafeAreaCompatibilityMode` 对 wine 进程无效，本文改在 winemac 中实现 safe area（§4），请 05 删掉这一项。
2. **对 ADR-012 的补充**：默认中文瓶子按 ADR 用 `LC_ALL`；转区和 47277 规避需要分开设置 `LC_CTYPE`/`LC_MESSAGES`，不能用 `LC_ALL`。建议 ADR 措辞改为“默认 LC_ALL，转区时拆分”。
3. GStreamer 的位置：01 定为引擎内置，05 §3 列在 `Components/gstreamer`。本文按 01 执行，请 05 修正。容量单位也不一致：05 写 1 人周 ≈ 5 窗，00 和 01 写 6 窗，本文用 6 窗。
4. `softwareupdate --install-rosetta` 是否需要管理员权限；27.x 是否恢复了自动安装提示（T7）。
5. GCMouse 是否需要输入监控授权；SDL3 访问 BLE 手柄是否触发蓝牙 TCC（T-IN-2、09 未解问题 9）。
6. 本地网络被拒时的错误码；是否需要 `NSBonjourServices`（T3）。
7. 用户经打开面板授予的访问，wine 子进程能否继承（T12）。
8. DXMT 的显存上报来源和可配置性；D3DMetal 上报的值。
9. Joy-Con L 的 PID 是否为 0x2006；各 PID 以 SDL controller list 为准。
10. 保持 GUI 常驻对 TCC 判定的实际影响（16 未解问题 2），以及 T13 的结论。
11. `Kaiti SC` 等可下载系统字体是否默认可用；`FontLink` 按文件名匹配 AssetsV2 路径是否可行（T-CJK-1）。
12. 国内对象存储默认域名对大文件下载的限制（强制下载头、限速），P2 首周实测后定 OSS 还是 COS。

## 与其他文档的接口

| 文档 | 本文提供 | 本文需要 |
|---|---|---|
| 01 架构 | 签名矩阵、HostCaps 平台字段、Rosetta 状态机、新增 ConfigKey（§11）、镜像部署 | Spawner、events socket、ConfigKey 注册机制、`drives` |
| 02 Engine R | §13 补丁清单；引擎门禁（ad-hoc、无 fontconfig、SDL、GStreamer 布局） | `macos-ux`、`cjk`、`mr/` 主题的移植与 rebase |
| 03 Engine A | CiderEngineA.app 流水线槽位、密钥与 profile 托管 | entitlement 集、T9 结论、probe 的 entitlement |
| 04 图形 | `mem.vram` 规则、HDR 与全屏键、Metal 缓存域 `org.cider.wine` | 各后端显存与 HDR 落点、限帧、MetalFX |
| 05 App/CLI | 用途说明、引导中的 Rosetta/本地网络/Translocation 步骤、shim 签名器、键位与手柄面板的数据 | UI 实现；按未决问题 1、3 修正 |
| 06 数据 | 新动作 `locale/input/display/vram/integration`、host-issues 示例、ingest 部署 | 白名单与 schema 的更新、`mirrors.json` |
| 08 启动器 | CEF IME 参数、托盘设置 | LRS 中的 IME 与托盘用例 |
| 09 HoYo | hoyoverse 钳制（vram auto、模型 E、DualSense native）、T-CLOUD | Verdict 是否覆盖 exec-host；8 GB 预检文案 |
| 10 运行库 | 47277 profile | WPF 识别样例、dotnet48 配方 |
| 11 QA | `platform-suite`、签名门禁脚本 | cider-lab 调度、runner、`conformance` job |
| 12 开发环境 | 测试卷与 VM 规划、Rosetta 安装步骤 | Xcode 26.6、测试 Mac |
| 13 路线图 | PLT-01…19 与估时 | 与其他文档的容量合并排期 |
