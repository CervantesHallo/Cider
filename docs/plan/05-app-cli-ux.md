# Cider 应用（GUI/CLI）与用户体验，含 CrossOver 功能对等清单

> 版本 v1 · 2026-09-27 · 依赖文档：00-strategy-and-decisions（ADR-001/002/005/006/007/008/009/010/012，阶段 P0–P4，里程碑 M-A…M-E）。估时单位“人周”= 1 人 + AI 代理 ≈ 5 窗。

## 目标与范围（含明确不做的事）

**目标**
- G1 `Cider.app`（SwiftUI）+ `ciderctl` 覆盖 CrossOver Mac 26 的用户可见功能；1.0 时本文对等矩阵的 P0 全部完成、总完成率 ≥80%（本矩阵即 `docs/parity.md` 初版）。
- G2 在 CX 弱项上超越：按程序设置、诊断 GUI、一键支持包、崩溃时后端建议、按瓶子钉引擎 + APFS 快照回滚、开放数据热更新。
- G3 R3：三款米哈游游戏的统一入口、启动前检查（预检）、路线卡、恢复闭环；非 `playable` 组合拉起游戏 exe **0 次**（进程审计可证），路线卡 ≤3 秒。
- G4 zh-Hans 优先、英文 100%；国内无代理可装引擎、收数据、收更新。

**不做**
1. 官方 iOS/iPadOS 客户端的任何形态：UI、文案、链接、schema 中都不出现 Mac App Store 的 iPhone/iPad App、PlayCover 或任何 iOS 运行器。
2. 任何绕过预检的开关：没有“仍然启动”按钮、CLI 隐藏参数或环境变量，测试员也没有。
3. GeForce NOW / Xbox Cloud 入口（ADR-010，待用户确认）。
4. Web UI；由 `ciderctl` 自行派生 wine（ADR-008）。
5. 32 位瓶子、Intel Mac、试用/注册；默认遥测；宿主 shell 钩子（CX `scripts.d`）。
6. Wine 内部、数据 schema 本体、预检规则内容：分别归 02/04、06、09，本文只定义消费接口。

## 现状与关键事实

- CX 26.3 功能面：安装清单、Run Command/Run with Options、瓶子导出导入/Publish、Open Shell、Auto 后端、MSync、High Resolution Mode、Repair/Rebuild、cx* CLI [01 §1.2、§4，高]。设置只按瓶子生效，Metal HUD 要改 conf [01 §6，高/中]。
- Whisky 原版已死；frankea/Whisky（GPL-3.0，app-v3.7.0）的 WhiskyKit 有 `PE/`、`ShellLink`、`Tar`、注册表 helper、`Steam/`、按程序 plist。整体 fork 可省 6–9 人月，按模块取可省 2–4 周 [11 §1.1、§2.1、§15，高/估算低]。
- 模型 A：经 LS 启动的 GUI `posix_spawn` wine 时，responsible 是 Cider.app；SMAppService agent 派生时归属存疑（T13）[16 §1，实测/存疑]。
- Game Mode：E3 中只有 exec 型 shim 和“loader 即主程序”进入 on；spawn 型停在 paused，样本少 [16 §2，证据有限]。
- winemac 没有 openFiles/openURLs 处理，文档与 URL 事件会丢失，所以 shim 只负责启动，关联由专门的接收方处理 [16 §2，证实]。Cider 生成的 bundle 必须 `codesign -s -` 完整签名，否则 provenance 会导致进程被杀 [16 §6，第三方]。
- 26.4+ 有 Rosetta“嵌入组件”通知；忽略清单豁免了 CodeWeavers，没有豁免 Cider [16 §3，文件证实/行为推断]。升级到 27 后 Rosetta 不会自动恢复，需每次启动检测 [00 裁定 5，高]。
- 引擎放在 bundle 外；换引擎前用 clonefile 快照，因为 `wineboot` 会隐式升级前缀 [12 §2.2、§4.5，已证实/推断]。Sparkle 2.10.0 最低 macOS 12，支持 EdDSA、delta 和分批推送 [12 §2.6，已证实]。CLT 没有 actool/xcstringstool，App 资源必须在 Xcode 或 CI 上构建 [12 §5.3，已证实]。
- 诊断全部可由环境变量开启：`MTL_HUD_*`、`MTL_DEBUG_LAYER`、`MTL_SHADER_VALIDATION`、`MTL_CAPTURE_ENABLED`、WINEDEBUG；`.ips` 带 `translated` 字段；遥测默认关闭并脱敏 [13 §3、§5.4，高]。
- 必须由 Cider 生成 `LANG/LC_*`，继承来的 `LC_CTYPE=UTF-8` 会让 ACP 落到 1252。`RetinaMode` 只能按前缀设置；“PS 手柄当 Xbox 手柄用”要写 `WineBus\Devices\<VID>/<PID>` 的 `Hidraw=0` [09 §4.3、§5.1、§3.3，已证实/推断高]。
- 云路线的 form 只能取 web 或经过核实的原生 macOS 客户端；绝区零日服云的 Mac 形态存疑，按底线不展示 [22 §2.2、§5.1，低]。GOG 默认走 gogdl 是产品取舍 [08 P1-6，中]；Epic 整条路线不做（用户 2026-09-28 定，00「不做清单」第 5 条），研究 08 的 P1-6/P1-7 中“官方 Epic 启动器保留为兼容选项”随之作废。

## 设计

### 1 仓库与模块

```
cider/App/            SwiftUI 外壳：project.yml（XcodeGen）、Localizable.xcstrings、AppIcon.icon
cider/Packages/CiderKit/  SwiftPM，macOS 14，全部业务逻辑，CLT 下 swift test 可跑
cider/Tools/ciderctl/  swift-argument-parser；Tools/cider-agent/；Tools/cider-shim-stub/（C，arm64）
```

| CiderKit 模块 | 职责 |
|---|---|
| CiderCore | ID、路径、schema 模型、迁移、原子写、`MessageID` |
| CiderPE | PE/RSRC 图标、PE 架构、安装器类型识别、`.lnk`（移植自 WhiskyKit） |
| CiderData | cider-data 拉取、minisign 验签、`timestamp`/`revision`、Recipe/Profile/Verdict |
| CiderEngine | 引擎/组件索引（Ed25519）、镜像测速、下载、引用计数 GC、GPTK 导入 |
| CiderBottle | 瓶子 CRUD、模板、clonefile 快照/回滚、导入导出（含 CX/Whisky） |
| CiderRun | SettingsResolver、EnvBuilder、LaunchPlan、Spawner、RunLedger |
| CiderInstall | Recipe 执行器（06 的动作白名单）、依赖 DAG、CAS 下载 |
| CiderShim / CiderLibrary | shim 生成与重建 / Steam 扫描、gogdl 适配 |
| CiderDiag / CiderHoYo | 诊断开关、分类器、支持包 / 预检宿主、路线模型、恢复观察器 |
| CiderIPC | XPC 协议（Codable 请求与事件） |

约束：Core/Data/Engine/Bottle/Run 不得 import AppKit/SwiftUI（CI lint）。CiderKit 不含本地化字符串，只返回 `MessageID` + 参数。WhiskyKit 是 GPL-3.0，吸收后 App 与 CiderKit 随之为 GPL-3.0；cider-data（CC0）不引入这部分代码。

**Whisky 复用**（固定 frankea/Whisky app-v3.7.0 的 commit，文件头注明来源，`THIRD_PARTY.md` 记录 sha）：
- 直接移植（先补测试）：`PE/`、`ShellLink`、注册表 helper、`Steam/` 的 VDF 与 ACF 解析、WhiskyThumbnail（P2 做 QuickLook 扩展）。
- 借鉴后重写：`Bottle/ProgramSettings` 的字段语义（改为 cider-bottle.json）、`constructWineEnvironment`（改为 EnvBuilder）、WhiskyCmd 的子命令命名、GPTK 导入的 UX、`ProcessRegistry`。
- 不用：`WhiskyWine/`（单引擎、无签名）、`GameDatabase`（由 cider-data 取代）、`Discord`、DXVK 相关代码。

### 2 进程、IPC 与 GUI 状态

```
shim / GUI / ciderctl ─XPC(org.cider.agent，由 SMAppService 登记，只做汇合点)─► GUI
GUI: LaunchRequest → SettingsResolver → [guard=hoyoverse ? CiderHoYo.preflight → Token]
     → EnvBuilder → LaunchPlan → GUISpawner.posix_spawn(wine) → RunLedger(audit.jsonl) → RunStore
```

```swift
public protocol Spawner: Sendable {            // ADR-007：可替换；T13 通过前只有 GUISpawner
  func spawn(_ plan: LaunchPlan) async throws -> RunHandle }
public struct LaunchPlan: Codable, Sendable {
  var bottle: BottleID; var engine: EngineRef; var argv: [String]
  var env: [String: String]         // 全量生成：不继承宿主；清除 DYLD_*；显式 LANG/LC_ALL
  var sync: SyncMode                // .msync | .server；与运行中的 wineserver 不一致时先重启
  var backend: BackendDecision      // chosen + fallbackReason?（原则 6）
  var diag: DiagFlags
  var preflight: PreflightToken?    // 瓶子 guard=hoyoverse 时必需，state 必须为 .playable
}
```

- **唯一卡口**：所有启动（shim、资料库、Run Command、`ciderctl run`）都经过 `Spawner`。它负责校验 PreflightToken（绑定游戏×区服×渠道×版本×引擎×macOS×`cpu_backend`、exe 的 sha256 和 verdict revision，TTL 10 分钟），并向 `Logs/audit.jsonl` 写入 `{ts, bottle, exe_sha256, token_id, decision}`。
- **设置优先级**：程序 > Profile（cider-data）> 瓶子 > 全局。瓶子级专属项（sync、RetinaMode、手柄 Hidraw、Z: 映射）在程序页只读显示并说明原因。回退时顶部横幅写“本次 DXMT→wined3d：原因 …”，日志同步记录。
- **GUI**：Observation（`@Observable`），根对象 `AppModel{bottles, engines, library, runs, data, hoyo, host}`，各 Store 只调用 CiderKit 的 actor。场景包括主窗口（NavigationSplitView，侧栏为 资料库｜米哈游｜瓶子｜安装｜引擎与组件｜诊断）、Settings、MenuBarExtra（运行中的程序，可结束或打开日志）。GUI 是唯一写者；ciderctl 的写操作一律走 XPC，读操作可以直接读文件；GUI 未运行时由 agent 以 `openApplication(hidden)` 拉起。

### 3 持久化、目录与迁移

```
~/Library/Application Support/Cider/
  Engines/<id>/  Components/{dxmt,d3dmetal,gstreamer,sdl,gogdl}/<ver>/
  Bottles/<bid>/{cider-bottle.json, .cider/programs/<pid>.json, drive_c/, *.reg}
  Snapshots/<bid>/<ts>-<reason>/   Templates/<engine>/<tpl>/   Data/   Handlers/   state/
~/Applications/Cider/<名称>.app      ~/Library/Caches/Cider/{cas,shaders}   ~/Library/Logs/Cider/
```

```json
{ "schemaVersion": 1, "id": "b-7f3c2a", "name": "Steam", "template": "win10_64+zh-CN+gaming",
  "cpu_backend": "rosetta",
  "engine": { "id": "cider-r-11.19-c1-x86_64", "pin": true },
  "engineHistory": [ { "id": "cider-r-11.18-c3-x86_64", "until": "2026-10-20T08:00Z",
                       "snapshot": "20261020-0800-engine-switch" } ],
  "components": { "dxmt": "0.80-c1", "d3dmetal": "imported:3.0-3", "gstreamer": "1.28.7-c1" },
  "locale": "zh_CN.UTF-8",
  "graphics": { "d3d11": "dxmt", "d3d12": "route:R", "legacy": "wined3d-gl" },
  "sync": "msync", "display": { "retina": true, "logPixels": 192 },
  "folders": { "linkMacHome": false, "zDrive": false },
  "policy": { "guard": null, "denuvoFrozen": null } }
```

程序文件 `programs/<pid>.json` 保存 exe、umu-ID、args、env 和 `overrides{backend, metalfx, avx, hud, winver, locale, fpsCap, keymap}`。UI 偏好放 UserDefaults；业务数据只落 JSON，不用 SwiftData，以便 CLI 共享、瓶子可移植、diff 可读。

迁移：`Migration(from:to:)` 链式执行；执行前用 clonefile 备份 `*.v{n}.bak`；写入时先写临时文件再 rename；未知字段原样保留。如果文件的 `schemaVersion` 高于 App 支持的版本，只读打开并提示升级，绝不降级写入。每个版本都有 golden fixture。

### 4 瓶子管理

- **模板**：只有 64 位（`win7_64`…`win11_64`），外加内容层 `zh-CN`（LC_ALL、SimSun→Songti SC、YaHei→PingFang SC）、`gaming`（去掉 Z:、不链接 Documents/Desktop/Downloads）、`dotnet48`、`vcrun`。模板按引擎 id 预先 `wineboot` 一次存入 `Templates/`，新建瓶子直接 clonefile，≤3 秒。旧版 Windows 的 winver 按程序经 AppDefaults 设置。
- **快照**：换引擎、执行每个配方步骤、导入、启动器自更新（profile 声明）之前自动打；用户也可手动打。打快照前经 Spawner 执行 `wineserver -k -w`。每个瓶子默认保留 5 个，可钉住。瓶子目录所在卷不是 APFS 时关闭快照，换引擎改为先导出归档并二次确认。
- **换引擎**：检查 `requires`（macOS、rosetta、minApp、arch）→ Denuvo 冻结的瓶子弹警告并二次确认 → 快照 → 写 engine 和 engineHistory → `wineboot -u` → 冒烟（`cmd /c ver`，≤60 秒）→ 失败则把当前目录 rename 到 `.trash`，再从快照 clone 回来，并显示原因。R↔A 迁移（P3）复用这条流程。
- **导出/导入**：`.ciderbottle` 用 Apple Archive（lzfse）打包，内含 `manifest.json`（文件清单 sha256、引擎钉选、App 版本）。排除 shader 缓存和日志；导入时重建 `dosdevices`、分配新 id；缺少钉选的引擎时提示下载，或改用同线最近版本。另可导入 CrossOver 瓶子（`cxbottle.conf` 的 `CX_GRAPHICS_BACKEND`、`WINEMSYNC`、`ROSETTA_ADVERTISE_AVX` 映射到 JSON）和 Whisky 瓶子（`Metadata.plist`）。
- 其余动作：复制、重命名、删除（移到废纸篓并清理启动器）、在 Finder 打开 C:、结束全部 / 强制结束、模拟重启（`wineboot -r`）、修复（校验引擎文件、`wineboot -u`、重建 dosdevices）、卷序列号/卷标、打开终端（`guard=hoyoverse` 的瓶子禁用）。

### 5 引擎与组件

- 列表字段：id、线（stable/devel/oracle）、arch、requires、大小、引用它的瓶子、`yanked`（显示红标，不再用于新装）。操作：安装、设为新瓶子默认、GC（引用计数为 0 且不在任何快照里）。索引用独立的 Ed25519 密钥验签（ADR-009）。
- **GPTK 导入**：用户选择 Apple 的 GPTK DMG → `hdiutil attach -readonly -nobrowse` → 定位 `redist/lib/external/{D3DMetal.framework, libd3dshared.dylib}` → `codesign -v` 通过、记录 `lipo -archs` 和版本 → `ditto` 原样复制到 `Components/d3dmetal/<ver>/`。可选下载源（ADR-006）走同一套校验。系统低于 macOS 15 时置灰并说明原因。
- 组件（DXMT、GStreamer、SDL、gogdl）按瓶子钉版本，UI 同引擎。

### 6 安装向导（Recipe）

```
目录搜索 → 详情（Verdict、last_verified、已知问题）→ 安装清单：
  [✓]安装程序（下载 / 本地文件，sha256）[✓]瓶子（新建自模板 / 已有）[✓]依赖 DAG [✓]磁盘
→ 全部就绪才能“安装” → 每步前打快照 → 步骤进度 → 失败回滚到最近快照，显示分类原因和支持包按钮
```

- 未收录的应用：选择 exe/msi → CiderPE 识别 MSI/NSIS/Inno/InstallShield/Burn → 建议新建瓶子 → 用对应的静默参数或交互安装 → 解析新增的开始菜单 `.lnk`，询问要生成哪些启动器。
- 安装助手：Cider.app 登记 exe/msi/lnk/bat 文档类型（`LSHandlerRank=Alternate`）。打开时先按 sha256 和 Download Glob 匹配配方；再读 `com.apple.quarantine`，显示来源 URL 并请用户确认。
- 下载走 `~/Library/Caches/Cider/cas/sha256/<aa>/<hash>`，多源、测速选源，哈希不符就失败，没有“忽略”开关。

### 7 启动器 shim 与关联（ADR-007）

```
~/Applications/Cider/原神.app/Contents/{Info.plist, MacOS/cider-shim, Resources/AppIcon.icns, Resources/shim.json}
Info.plist：CFBundleIdentifier=org.cider.shim.<bid>.<pid>（唯一）、CFBundleName、LSApplicationCategoryType=
  public.app-category.games（非游戏用 productivity）、LSSupportsGameMode、GCSupportsGameMode、
  NSPrefersDisplaySafeAreaCompatibilityMode（按 profile）、LSMinimumSystemVersion=14.0；不声明文档或 URL 类型
```

- stub（arm64，CI 构建后随 App 分发）读取 `shim.json{bid,pid}` → XPC `launch(pid)`；失败时退回 `open -g cider://launch/<pid>`。默认 `forward-exit` 模式（`LSUIElement`，转交后退出）。T4 证明 spawn 拿不到 Game Mode 时，按游戏提供“Game Mode（实验）”开关，启用 exec 型 CiderGameHost，并提示用户该游戏的麦克风、网络授权会改记到 Cider Game Host 名下。
- 图标：CiderPE 取最大的 `RT_GROUP_ICON`，用 ImageIO 写 icns，按 Tahoe 规范预合成 squircle 底板。生成后执行 `codesign -s - --force` 并 `--verify`。“重建启动器”会重新生成全部 shim。
- **handler shim**（`App Support/Handlers/<id>.app`）声明 `CFBundleURLTypes`（`steam://`、`battlenet://` 等）或 `CFBundleDocumentTypes`（来自配方或 HKCR 的关联），实现 `application:openURLs:` 后 XPC 转交给目标瓶子执行 `start`。只有用户点“设为默认”时才调用 `NSWorkspace.setDefaultApplication`；与原生 Steam 冲突时询问用户。

### 8 资料库

| 来源 | 实现 | 启动 |
|---|---|---|
| Steam（瓶内） | 解析 `libraryfolders.vdf` 和 `appmanifest_*.acf`；封面取 Steam CDN 并缓存，失败回退 exe 图标 | `steam.exe -applaunch <id>`；Steam UI 会话在 msync 门禁通过前用 `sync=server`，切换时提示“将重启该瓶子全部程序” |
| GOG（1.x） | gogdl 组件子进程（许可证未核实：只按需下载，不 vendoring） | 由 gogdl 取得启动命令，再由 Cider 组装 LaunchPlan |

条目统一为 `LibraryItem{umuId, store, title, bid, exe, art, verdict, guard}`，按 umu-ID 合并 Profile。`guard=hoyoverse` 的条目点击后进入游戏中心。

### 9 首次运行引导

1. 语言（默认跟随系统，zh-Hans 优先）。
2. 网络与镜像测速（GitHub 或国内 CDN，可手动选）。
3. Rosetta：用 `arch -x86_64 /usr/bin/true` 检测，每次启动都查。缺失时通过系统管理员授权对话框执行 `softwareupdate --install-rosetta --agree-to-license`，Cider 不接触密码；另提供“复制命令”。
4. 预先说明 Rosetta“嵌入组件”通知的含义。
5. 批准后台项（cider-agent）。
6. 触发本地网络授权：主动连一次网关。
7. 下载默认 Engine R。
8. 可选：导入 GPTK、安装 Steam、进入米哈游游戏中心。

### 10 诊断

- 按程序、按次的开关：Metal HUD（元素预设）、HUD 日志、shader 编译日志、API/Shader 验证、GPU 捕获、WINEDEBUG 预设（`+seh,+tid,+loaddll` / `warn+all` / 自定义）、DXMT/D3DMetal 日志、`WINEMSYNC_STATS`。开关对下一次启动生效；“带选项运行”输出 `.ciderlog`。
- 支持包（`Cider-Diag-<日期>-<id>.zip`）：版本、瓶子和 profile 哈希、`.ciderlog`、`log show` 中 `D3DM` 与 `com.apple.metal.hud` 的条目、wine 相关 `.ips`（取 `translated` 和 `exception`）、minidump、相对模板的注册表差异、芯片/内存/系统 build、Rosetta 状态、probe 输出、预检轨迹。脱敏：用户名、家目录、邮箱、IP、序列号、类 token 字符串，不收 `loginusers.vdf`。`manifest.json` 列出每个文件的脱敏计数；用户预览后可以复制、在 Finder 中显示或发邮件，不需要 GitHub 账号。
- 崩溃分类器：规则放在 cider-data，按“退出码 + 日志签名 + `.ips`”匹配，输出“用此设置重试一次”（一次性覆盖，用户点“保留”才写入程序设置）：

```yaml
- id: dxmt-device-fail
  when: { backend: dxmt, log_any: ["D3D11CreateDevice failed"] }
  suggest: { retry_once: { backend: d3dmetal }, requires: [gptk], msg: diag.try_d3dmetal }
- id: hoyo-driver-init
  when: { guard: hoyoverse, file_contains: { driverError.log: "initDriver Failed" } }
  action: route_card        # 不给任何重试或后端建议
```

### 11 R3 米哈游游戏中心

- **入口**：侧栏“米哈游”；资料库里识别到的安装；打开 `YuanShen.exe`/`GenshinImpact.exe`/启动器安装包；`cider://hoyo/<game>`；游戏 shim；`ciderctl hoyo preflight`。所有入口都进同一个预检。
- **游戏页**：选区服和渠道（国服官服、B 服；国际服 HoYoPlay，Steam 只有绝区零），然后显示预检清单动画（识别 → 区服/渠道 → 版本 → 环境 → 裁定）。Epic 渠道只做识别、不作为可选项：识别到经 Epic 安装的国际服，直接按“不在支持范围”出路线卡（06 §10、05 §15 #55）。
- **路线卡**：显示状态徽标，按 README 口径如实写出预期，并给出按钮：国服官方网页云（优先 Chrome/Edge `--app=`，其次默认浏览器，附网络预检）；`broken-launcher` 时“只打开启动器更新/修复”；E4 通过后，原神国际服显示 Genshin Impact · Cloud 的 Windows 客户端路线；星铁国际服写明“暂无官方云”。另有“查看裁定证据”和“可玩时通知我”。
- 只渲染 `form ∈ {web, windows-cloud-client, native-macos}` 的路线，`unverified` 形态不显示。
- 游戏中心创建的瓶子带 `policy.guard="hoyoverse"`：禁用“运行命令”启动任意 exe、打开终端、shellenv；图形后端和 GPU 身份等设置按 lint 锁定。
- 恢复闭环：只读观察退出码、`driverError.log` 和已知错误窗口。命中后弹路线卡，本机裁定降为 `unverified`，生成脱敏报告。

### 12 i18n

- `developmentRegion=zh-Hans`，en 为翻译。App 用 xcstrings；ciderctl 用 CI 从 xcstrings 导出的 `.strings`，按 `--lang` 或 `AppleLanguages` 选择。
- 术语表 `docs/i18n/glossary.md`：Bottle=瓶子、Engine=引擎、Recipe=安装配方、Profile=运行配置、Verdict=裁定、Pre-flight=启动前检查、Route card=路线卡。
- 瓶子 locale 与 UI 语言相互独立，选项为“跟随系统（算出完整 `xx_YY.UTF-8`）/ 简中 936 / 繁中 950 / 日 932 / 韩 949”。
- CI 门禁：两种语言 100% `translated`；伪本地化（加长 40%）截图检查截断。

### 13 更新与镜像

- **App**：Sparkle 2.10（SPM），`SURequireSignedFeed`，stable/beta 两个频道，开启分批推送和 delta。生成 global 和 cn 两份 appcast（enclosure 不同，EdDSA 密钥相同），由 `feedURLString(for:)` 按镜像测速结果选择。有游戏在运行时推迟重启。
- **数据**：`timestamp.json` 超过 7 天显示“数据过期”横幅；HoYo 裁定过期一律按 `unverified` 处理。
- **引擎/组件**：独立签名通道。镜像只是不可信传输，内容按 sha256 寻址，以签名清单为准；另支持离线导入签名的 `.tar.xz`。

### 14 ciderctl 规格

```
全局：--json  --lang zh-Hans|en  -q；退出码 0 成功 2 用法 3 不存在 4 预检阻断(附路线JSON) 5 校验失败 6 缺引擎 7 缺Rosetta 10 agent不可用
ciderctl status                                   # Rosetta/GPTK/引擎/agent/数据时效
ciderctl bottle list|create --template T [--engine E]|clone|rename|delete|info|repair|reboot|kill [--force]
ciderctl bottle config get|set <b> <key> <val>    # 瓶子级
ciderctl bottle snapshot create|list|restore|prune <b>
ciderctl bottle export <b> -o f.ciderbottle | import <f> | import --from crossover|whisky <dir>
ciderctl bottle shell <b>                         # 打印 env；guard 瓶子返回 4
ciderctl run <b> <exe|C:\path> [--env K=V] [--winedebug +seh] [--hud] [--backend dxmt] -- args
ciderctl program list|set <b> <pid> <key> <val>|launcher create|remove
ciderctl engine list|install|remove|gc|use <b> <engine>;  ciderctl component list|install|import-gptk <dmg>
ciderctl install <recipe> [--bottle b|--new] [--installer path] [--dry-run];  ciderctl tricks <verb>
ciderctl library scan|list [--store steam|gog]|launch <item>
ciderctl data update|status|show <umu-id>;  ciderctl launchers rebuild
ciderctl hoyo preflight <game> [--edition cn|global] [--channel C];  ciderctl hoyo cloud <game>
ciderctl diag bundle [<b>] [-o path];  ciderctl diag hud on|off <b> [<pid>];  ciderctl logs <b> [-f]
```

安装方式：设置里的“安装命令行工具”在 `~/.local/bin` 建符号链接。Spawner 的规则对 CLI 同样生效，ciderctl 没有任何跳过预检的参数（测试会枚举全部 flag 验证）。

### 15 CrossOver → Cider 功能对等矩阵

“实现”列是 2026-09-27 的实现进度（✔ 已完成并实测，◐ 部分完成，○ 未开始）。“状态”列给的是设计状态：定稿=本文已定；验=待实验；替=换一种方式实现；✕=不做。★ 表示超越 CX。

| # | CrossOver 功能 | Cider 对应 | 级 | 里程碑 | 状态 | 实现 |
|---|---|---|---|---|---|---|
| 1 | 兼容性库搜索、磁贴 | 应用目录（Recipe + Verdict，离线缓存） | P0 | M-C | 定稿 | ◐ |
| 2 | Install details 清单 | 安装清单页 | P0 | M-B | 定稿 | ◐ |
| 3 | 自动下载安装程序与依赖 | CAS、sha256、多镜像 ★ | P0 | M-B | 定稿 | ◐ |
| 4 | 安装高级选项（日志、语言、依赖） | 高级面板 | P1 | M-C | 定稿 | ○ |
| 5 | Install an unlisted application | 未收录安装（识别安装器类型） | P0 | M-B | 定稿 | ✔ |
| 6 | 安装程序缓存与清理 | CAS 管理 | P1 | M-C | 定稿 | ○ |
| 7 | Installer Assistant | 安装助手 + quarantine 提示 | P1 | M-C | 定稿 | ○ |
| 8 | 自动下载新配方 | 签名数据热更新 ★ | P0 | M-B | 定稿 | ○ |
| 9 | 隐藏未测试/不可用条目 | 按 Verdict 过滤 | P1 | M-C | 定稿 | ○ |
| 10 | CrossTie `.tie` | Recipe（无 shell）+ `.tie` 导入器 | P1 | 1.x | 替 | ○ |
| 11 | Home 双击运行、Hide | 程序网格、隐藏 | P0 | M-B | 定稿 | ✔ |
| 12 | Launchpad/Dock 图标 | shim（Game Mode 键） | P0 | M-C | 验 T4 | ○ |
| 13 | Run Command / 存为启动器 | 运行命令 / 另存为启动器 | P0 | M-B | 定稿 | ✔ |
| 14 | Run with Options | 带选项运行 + `.ciderlog` | P0 | M-B | 定稿 | ✔ |
| 15 | Open C: / 装入此瓶子 | 同名功能 | P0 | M-B | 定稿 | ✔ |
| 16 | Quit / Force Quit All | 结束全部 | P0 | M-B | 定稿 | ✔ |
| 17 | New/Duplicate/Rename/Delete | 模板 clonefile ★ | P0 | M-B | 定稿 | ✔ |
| 18 | Export/Import Archive | `.ciderbottle` + CX/Whisky 导入 ★ | P0 | M-C | 定稿 | ✔ |
| 19 | 用归档当快照 | 自动 APFS 快照/回滚 ★ | P0 | M-B | 定稿 | ✔ |
| 20 | Publish Bottle（多用户） | 只读共享瓶子 | — | 1.x 后 | 延后 | ○ |
| 21 | Open Shell | 打开终端（guard 瓶子禁用） | P1 | M-C | 定稿 | ○ |
| 22 | 模板 win98…win11(_64) | 64 位模板 + 按程序 winver | P0 | M-B | 替 | ✔ |
| 23 | scripts.d 钩子 | Recipe/Profile 类型化动作 | — | — | 替 | ○ |
| 24 | `.windows-serial`/`label` | 瓶子卷属性 | P1 | M-C | 定稿 | ○ |
| 25 | 图形 Auto/D3DMetal/DXMT/DXVK/wined3d | Auto(Profile)/DXMT/D3DMetal/wined3d/vkd3d；DXVK ✕ | P0 | M-B | 替 | ◐ |
| 26 | DLSS→MetalFX | 按程序开关 | P1 | M-C | 定稿 | ○ |
| 27 | MSync | msync/server（瓶子级） | P0 | M-B | 定稿 | ✔ |
| 28 | High Resolution Mode | Retina + 192 DPI | P0 | M-C | 定稿 | ✔ |
| 29 | 设置只按瓶子 | 按程序覆盖 ★ | P0 | M-C | 定稿 | ◐ |
| 30 | AVX 自动开启 | Profile + 程序开关 | P0 | M-C | 定稿 | ◐ |
| 31 | Metal HUD 需改 conf | 诊断 GUI ★ | P0 | M-B | 定稿 | ◐ |
| 32 | winecfg / regedit | 同 | P0 | M-B | 定稿 | ✔ |
| 33 | Game Controllers、Disable hidraw | 手柄面板、按设备 `Hidraw=0` | P1 | M-C | 定稿 | ○ |
| 34 | Simulate Reboot | 模拟重启 | P0 | M-B | 定稿 | ✔ |
| 35 | Task Manager、Internet Settings | taskmgr、inetcpl | P1 | M-C | 定稿 | ✔ |
| 36 | 已装软件与卸载 | 卸载页 | P1 | M-C | 定稿 | ✔ |
| 37 | Clear and Rebuild Programs | 重建启动器 | P1 | M-C | 定稿 | ○ |
| 38 | Repair Bottles | 修复瓶子 | P1 | M-C | 定稿 | ✔ |
| 39 | Sparkle 检查更新 | Sparkle + 国内 appcast | P0 | M-C | 定稿 | ○ |
| 40 | Preview 通道 | beta 频道 + devel 引擎 | P1 | M-C | 定稿 | ○ |
| 41 | 启动器目录、瓶子目录可改 | 设置项（快照需 APFS） | P1 | M-C | 定稿 | ○ |
| 42 | 提醒提交评分 | 可选脱敏报告 → Verdict | P1 | 1.0.x | 定稿 | ○ |
| 43 | Enable 32-bit bottles；试用/注册 | — | — | — | ✕ | ○ |
| 44 | 文件关联（Default/Alt） | handler shim | P1 | M-C | 定稿 | ○ |
| 45 | URL scheme | handler shim（`steam://` 等） | P1 | M-C | 定稿 | ○ |
| 46 | `wine --bottle --cx-app`、每应用命令 | `ciderctl run` | P0 | M-A | 定稿 | ✔ |
| 47 | cxbottle/cxmenu/cxassoc/cxtie | ciderctl 对应子命令 | P1 | M-C | 定稿 | ○ |
| 48 | `.cxlog` | `.ciderlog` + 支持包 + 崩溃建议 ★ | P0 | M-B | 定稿 | ✔ |
| 49 | 专有的逐游戏 Auto 库 | 开放、签名的 Profile ★ | P0 | M-B | 定稿 | ◐ |
| 50 | 1–5★ 评级、投票 | Verdict + 开放报告 | P1 | M-C | 定稿 | ○ |
| 51 | 启动器：Steam、EA、Battle.net、HoYoPlay（Epic 见 #55） | LRS + 资料库（Steam 清单扫描） | P0 | M-C | 定稿 | ◐ |
| 52 | 启动器：GOG、Ubisoft、Rockstar | gogdl 与配方 | P1 | 2027-12 | 定稿 | ○ |
| 53 | 反作弊：官方说明不可用 | 预检、路线卡、官方云、恢复闭环 ★ | P0 | M-A/M-B | 定稿 | ◐ |
| 54 | CX 27 ARM64（旧瓶子不能转换） | Engine A opt-in + R↔A 迁移 ★ | P1 | M-D/M-E | 验 G-ENT | ○ |
| 55 | 启动器：Epic Games Store（CX 25.0 起官方支持） | — | — | — | ✕ | ○ |

#55 是明确不做，不是待办：用户 2026-09-28 决定不在 Epic 上投入资源（00「不做清单」第 5 条），Cider 在这一项上**不会**追平 CrossOver，这一点如实记录，不删项。它按 #43 的先例保留编号并留在分母里：矩阵共 55 项，其中 #43、#55 两项 ✕，完成率上限 53/55 ≈ 96%，「总完成率 ≥80%」（需 44 项）不受影响。#55 的级别记为 `—` 而不是 P0，所以「P0 全部完成」仍可达成，但含义随之收窄：#51 的 P0 只覆盖 Steam、EA、Battle.net、HoYoPlay，P0 全绿不再意味着 Epic 能用。`parity.md` 生成器（APP-24）统计时 ✕ 项计入分母、不计入完成数。

## 实施计划

| 编号 | 工作包 | 产出 | 验收标准 | 估时 | 依赖 | 阶段 |
|---|---|---|---|---|---|---|
| APP-01 | 仓库与 CiderKit 骨架 | 模块、XcodeGen、CI（swift test + xcodebuild） | 开发机 CLT 下 `swift test` 绿；CI 构建 App；AppKit 导入 lint 生效 | 0.3 | 12 | P0 |
| APP-02 | 模型、持久化、迁移 | Core、fixtures | kill -9 写入后文件仍有效；高版本文件只读打开 | 0.3 | 01 | P0 |
| APP-03 | EnvBuilder、Spawner、RunLedger、XPC、ciderctl 骨架 | run、bottle create | M-A：M3 上 `ciderctl run` 跑通 notepad；env 中无继承的 `LC_*`/`DYLD_*`；每次派生都有审计记录 | 0.6 | 01、02 | P0 |
| APP-04 | 预检宿主与路线卡 UI、国服云入口 | CiderHoYo、SwiftUI 卡片 | 伪安装 50 次拉起 exe 0 次；p95 ≤3 秒；三款国服云用 Chrome 进入登录页 | 0.6 | 09 | P0 |
| APP-05 | 环境检测与 GPTK 导入 | Rosetta/probe/GPTK | 无 Rosetta 时进入安装引导；篡改过的 framework 被拒 | 0.2 | 07 | P0 |
| APP-06 | 引擎管理器 | 验签、镜像、GC、UI | 伪签名被拒；国内无代理下载 ≤3 分钟 | 0.6 | 02 | P1 |
| APP-07 | 瓶子管理 v1 | 模板、快照、回滚 | 克隆 ≤3 秒；注入失败的换引擎自动回滚且注册表哈希一致 | 0.6 | 03 | P1 |
| APP-08 | GUI MVP | 导航、程序网格、运行命令、带选项运行、结束/重启、winecfg | XCUITest 冒烟流程全绿（zh/en） | 1.0 | 06、07 | P1 |
| APP-09 | Recipe 执行器最小集与 Steam 库 | CAS、run_installer、registry、detect | Steam 安装配方在 runner 上成功；库列出已装游戏 | 0.8 | 06 | P1 |
| APP-10 | 诊断 v1 | 开关、`.ciderlog`、支持包 | 脱敏 fixture 零泄漏；包 ≤20 MB | 0.6 | 03 | P1 |
| APP-11 | i18n 基建 | xcstrings、术语表、门禁 | 两种语言覆盖 100%，CI 强制 | 0.3 | 01 | P1 |
| APP-12 | 引导 v1 与 0.1 发布 | 向导、公证 DMG | M-B：0.1 已公证；一键进入国服云 | 0.4 | 11 号文档 | P1 |
| APP-13 | 按程序设置与回退横幅 | Resolver、UI | 优先级表驱动测试；回退必有原因 | 0.6 | 06 | P2 |
| APP-14 | shim 生成与 T4 落地 | stub、图标、签名、重建 | 50 个 shim 全部 `codesign --verify` 通过；出现在 Launchpad | 0.8 | T4 | P2 |
| APP-15 | handler shim | URL 与文档关联 | `steam://` 与 `battlenet://` 投递到正确瓶子；文档关联按配方生效 | 0.5 | 14 | P2 |
| APP-16 | 导出导入与 CX/Whisky 导入 | `.ciderbottle` | 往返后文件清单和注册表哈希一致 | 0.6 | 07 | P2 |
| APP-17 | 安装向导完整版 | 目录、清单、未收录安装、安装助手 | M-C：从安装到登录 Steam ≤10 分钟 | 0.8 | 09 | P2 |
| APP-18 | ~~Epic 资料库（legendary）~~ 不做（§15 #55） | — | — | — | — | — |
| APP-19 | 崩溃分类器 | 规则引擎、重试建议 | 回放 30 份日志分类正确率 ≥90% | 0.6 | 10 | P2 |
| APP-20 | Sparkle、国内 appcast、数据更新 UI | 更新通道 | 两份 appcast 各升级一次；游戏运行时推迟重启 | 0.5 | 12 | P2 |
| APP-21 | 游戏中心完整版 | 区服与渠道、恢复横幅、通知、E4 路线 | 日志回放命中即出卡，本机降为 unverified | 0.6 | 09、E4 | P2 |
| APP-22 | 控制面板与故障排除 | 手柄、taskmgr、卸载、修复、终端 | 矩阵 #21、#32–38 可演示 | 0.6 | 08 | P2 |
| APP-23 | 引导完整版与权限 | 后台项、本地网络、通知 | T3 弹窗归属 Cider | 0.4 | T3、T13 | P2 |
| APP-24 | parity 统计与 UI 回归 | `parity.md` 生成器 | G-1.0：P0 100%，总计 ≥80% | 0.3 | 全部 | P2 |
| APP-25 | R↔A 迁移 UI 与 GOG 资料库 | 迁移流程、gogdl | 10 个瓶子 R→A→R 无损 | 0.8 | 03 号文档 | P3 |
| APP-26 | macOS 28 首日提示 | 说明页、迁移引导 | 28 上打开 R 瓶子必有说明 | 0.4 | G-T15 | P4 |

合计约 13.9 人周（约 70 窗），其中 P0 2.0、P1 4.3、P2 6.2、P3 0.8、P4 0.4（APP-18 取消，省下 0.5 人周）。按配额节奏安排：UI 与脚手架放在周配额后段；XPC、Spawner、迁移这类高风险工作放在前段。每项按“一窗一卡”拆分。

## 测试与验收

- **单元测试**（CLT/CI）：schema 与迁移 golden；EnvBuilder 快照测试；预检表驱动测试；脱敏 fixture（用户名、邮箱、token 零命中）；shim 的 `plutil -lint` 与 `codesign --verify`；在临时 APFS 映像（`hdiutil create -fs APFS`）上测 clonefile 快照和回滚。
- **集成测试**（macos-26 runner，带 Rosetta）：`ciderctl bottle create` → `run cmd /c ver`；导出导入往返；Recipe 执行 Steam 安装。
- **UI 测试**：XCUITest 覆盖引导、建瓶子、运行、路线卡，zh 和 en 各截一套图；伪本地化截断检查。
- **实验室**：安装到登录 Steam 计时；T3/T4/T13/T14；在 8 GB 机器上测 App 冷启动 ≤1.5 秒、空闲内存 ≤150 MB。
- **R3 红线测试**：进程审计中非 playable 组合拉起 exe 为 0；枚举 CLI flag 和 UI 字符串，确认没有 force/ignore/bypass 类开关；schema 写入 iOS 形态时 CI 必须失败。

## 风险与预案

| 触发条件 | 动作 |
|---|---|
| T4 证实 spawn 拿不到 Game Mode | 按游戏提供 exec 型 CiderGameHost 开关，并说明授权会改记到 Game Host 名下（ADR-007 复议） |
| 用户拒绝后台项，或 T13 显示 agent 自成 responsible | agent 只做汇合点，不派生；后台项被拒时 CLI 写操作提示先打开 Cider，读操作照常 |
| 瓶子目录在非 APFS 卷上 | 关闭快照；换引擎前强制导出归档并二次确认 |
| CLT 缺工具，GUI 无法本机构建 | 逻辑全部放在 CiderKit；GUI 交给 CI；P0 第 1 周装 Xcode 26.6 |
| 移植的 WhiskyKit 代码出现缺陷 | 只移植 4 个模块并先补测试，其余重写 |
| gogdl 被上游 API 打坏 | 作为组件钉版本、热更新；必要时改用官方 GOG Galaxy 配方 |
| Rosetta 通知引发用户恐慌 | 引导页预先说明；提交 CORAL 申请；FAQ |
| GitHub 在国内不可达 | 测速切到国内 CDN 和 cn appcast；支持离线导入签名引擎包 |
| 用户要求 HoYo“强制启动” | 不提供；路线卡文案如实；FAQ 解释红线 |
| Steam 的 `sync=server` 切换打断游戏 | 切换前弹窗；msync M3 落地后移除 |

## 未决问题与需实机验证的点

1. **对 ADR-007 的顾虑**：CLI 与 GUI 之间的 XPC 需要一个在 launchd 登记的 Mach 服务，所以 v0 也离不开 SMAppService agent（仅作汇合点），会增加“后台项”批准的摩擦。备选是 GUI 持有的 Unix socket 加对端签名校验。在 ADR 复议前按 ADR 执行。
2. **对 ADR-007 的顾虑**：游戏 shim 不声明文档类型，文件关联只能经 handler shim。打开文档时 Dock 上短暂出现的是 handler 而不是应用本身，是一项 UX 成本。
3. T4（Game Mode）、T13（agent 归属）、T3（升级后本地网络授权是否保留）、T14（ad-hoc shim 能否进入 Games app）。
4. `softwareupdate --install-rosetta` 是否需要管理员权限；27.x 是否恢复了自动安装提示。
5. 生成的 icns 在 Tahoe 上是否被灰框包裹；stub 的 `LSUIElement` 与 Dock 名称的关系（依赖 02 的 `macos-ux` 补丁，即 CX hack 22144/25964 的对应实现）。
6. Sparkle 按镜像动态切换 feed 的行为；XCUITest 能否在 macos-26 runner 上无头运行。
7. Chrome 的 `--app` 窗口里 Pointer Lock 和手柄是否正常；已装原生 Steam 时 `steam://` 的默认处理。
8. 绝区零日服云的 Mac 形态（22 号报告 7a）：确认之前不显示。
9. GFN/Xbox 入口等待用户确认；如果确认，约 0.5 人周。

## 与其他文档的接口

- **01-architecture**：XPC 协议与 agent 的定位、仓库布局。本文定义 CiderKit 的模块边界和 `Spawner`/`LaunchPlan`。
- **02-engine-r-wine**：引擎 manifest 的 `requires/arch/features`、`sync` 模式与 `WINEMSYNC_STATS`、`macos-ux` 主题（Dock 命名、EditMenu）、`cider-rules` 的输入格式。
- **03-engine-a-arm64**：CiderEngineA.app 在索引中的描述、`cpu_backend=fex`、R↔A 迁移的冒烟命令。
- **04-graphics**：后端枚举与可用性判定、回退原因码、HUD 与各后端日志的环境变量、D3DMetal 校验规则。
- **06-profiles-recipes-compatdb**：Recipe/Profile/Verdict/分类规则的 schema 与动作白名单；本文负责执行器和 UI。
- **07-platform-integration**：Cider.app 与 stub 的签名公证、用途说明字符串、Rosetta 检测、CJK 字体映射、镜像基础设施。
- **08-games-launchers-anticheat**：LRS 调用 `ciderctl --json`；启动器 profile；gogdl 的钉版本。08 的 P1-6/P1-7 中 Epic 相关建议按 00「不做清单」第 5 条作废。
- **09-hoyoverse-games**：预检规则、Verdict 键、路线数据、恢复签名；本文负责游戏中心 UI、Spawner 卡口和审计。
- **11-qa-perf-ci-release**：`app-release` 流水线、XCUITest、parity 统计。**12-dev-environment**：Xcode 安装、`cider/CLAUDE.md`。**13-roadmap**：APP-01…26 的排期。
