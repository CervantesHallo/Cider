# 配方（Recipe）/ 运行档案（Profile）/ 兼容性数据库

> 版本 v1 · 2026-09-27 · 依赖文档：00（ADR-006/008/009/010/011/012 为硬约束）、01、02、04、05、07、08、09、10、11 · 调研依据：01、03、08、10、11、13、18、21、22 号报告与 00-research-review · 1 人周 ≈ 6 窗

## 目标与范围

目标：
- **G1 装得上**：每个下载都能按 sha256 验证，国内无代理可装；上游换链接或改哈希，24 小时内由 CI 发现。
- **G2 热修得动**：“怎么跑”全部写成签名 Profile，经 `cider-rules` 落到引擎；修启动器和游戏不必发版。
- **G3 说得清**：每个条目都有带证据和日期的裁定（Verdict）与评级；预检（09）只凭这份数据决定是否拉起 exe。
- **G4 红线进 schema**：iOS 形态、反作弊规避、宿主 shell 在 schema 与 lint 层面无法合并，客户端加载时再查一次。

范围：三类数据（Recipe、Profile、Game/Verdict）的 schema、校验与编译工具、CiderKit 数据客户端（拉取、校验、求值、安装执行）、下载缓存与镜像、签名通道、兼容库与评级、社区提交、导入器、数据侧 CI、可选遥测。

明确不做：
- 数据内嵌脚本（Python、shell、内联 `.cmd`）；宿主侧执行任意命令。
- 任何 iOS/iPadOS 形态的路线（Mac App Store 的 iPhone/iPad App、PlayCover 及其他 iOS 运行器）；GFN、Xbox 等第三方云（ADR-010）。
- 反作弊规避字段；任何绕过预检的途径（测试员、本地覆盖、第三方源都不行）。
- 复刻 CX 的 `cxcompatdb.so`；批量收录 CrossTie 库（CodeWeavers 专有数据，只做用户本地导入）。
- 自研米哈游下载器；镜像游戏本体；默认开启遥测。

## 现状与关键事实

| # | 事实 | 对设计的约束 | 来源 · 置信度 |
|---|---|---|---|
| F1 | winetricks 只认 sha256，可传空跳过（`webview2`），失败回退 web.archive；`aka.ms` 背后换文件，vcrun2019/2026 哈希 2026-08-27 重改，DirectX Jun2010 同名文件有两个 sha256 | 一个 source 允许多个哈希；浮动链接只给刷新机器人用 | [10 §1.1] 高 |
| F2 | Bottles 依赖清单只校验 MD5 和大小，约 20 种声明式 action；CrossTie 公开文档没有下载校验和签名 | sha256 + size + 签名为硬要求 | [10 §7] 高/中 |
| F3 | CrossTie 的 appid 永不复用；Pre/Post-Dependencies 递归成 DAG；`<useif>` 支持 equal/lt/le/ge/gt/match/not/and/or；靠注册表或文件 glob 判断已装；库 22,667 条，可一键安装 4,067 条 | id 永久；`when` 要能承接 UseIf | [01 §2.2] 高 |
| F4 | Proton 的 `hack_append_command_line` 是写死的 exe 表，只能追加参数；WebView2 要锁 Fixed Version，并把 `msedgewebview2.exe` 设为 win7（58921） | Profile 要支持参数的删、改、追加和子进程匹配 | [10 §3.1][18 §5] 高 |
| F5 | umu-ID 是不透明字符串（约 1,202 行、1,101 个 ID，含 `umu-genshin` 和 UUID 形式）；umu-database 为 GPL-3，umu-protonfixes 为 BSD-2，Proton `default_compat_config` 为 BSD-3 | 主键用字符串；三方数据按原许可单独存放 | [11 §13 核查更正][03 §8.4–8.5] 高 |
| F6 | Highball-db 数据为 CC0：recipe 有 `steps/knownIssues/lastVerified/blocked`，游戏库有 `status`、`provenance`、`anticheat.macVerdict`；报告经 issue 打 accepted 后并入；另有约 12,500 条由 ProtonDB 推导的预测 | 字段取超集，可直接导入；其结论来自 Highball 自有引擎，不能等同 CX 或 Cider | [08 §5 与核查][13 §5.1] 高 |
| F7 | ProtonDB 由问卷推导档位，每人只计最新一份报告；CodeWeavers 用 1–5 星；AGW 按运行方式分别评级 | 档位由事实推导，不让用户直接打分；按“引擎 × 后端”分别评 | [13 §5.1] 高/中 |
| F8 | minisign 基于 Ed25519，trusted comment 受签名保护，可放版本号和过期时间；TUF 1.0.36（2026-08-05）用四个角色防回滚、冻结和混搭 | v1 用 minisign 实现 TUF 形状的元数据，v2 换完整 TUF | [13 §5.3] 高 |
| F9 | 国服和国际服客户端分离（`YuanShen.exe`/`GenshinImpact.exe`）；绝区零 Steam 版为 4162040，标注 HoYoKProtect；HYP API `getGameBranches` 的 `tag` 可只读做版本监控；`HoYoKProtect.sys` 缺 `WDFLDR.SYS` 的失败签名覆盖原神 6.5 起所有区服；绝区零日服云的“Mac”形态疑为 iPad App | Verdict 按区服 × 渠道 × 版本分；需要硬阻断规则；路线 form 枚举不含 iOS | [21 P0-2 核查更正][22 §1、§3、§5] 高/中 |
| F10 | 换引擎、改 CPU 拓扑或 AVX 广告后 Denuvo 是否把机器算成新机器，在 macOS 上未知 | 默认冻结指纹相关字段 | [08 §4 核查] 中 |
| F11 | GitHub 标准 arm64 runner 为 3 核、7 GB、14 GB，已预装 Rosetta | 组件冒烟放托管 runner，应用和游戏冒烟放 cider-lab | [12 §3.1 核查] 高 |
| F12 | Lutris 脚本和 Bottles 清单的数据授权不明 | 只做本地、按需导入 | [11 §11–12] 中 |

## 设计

### 1 总体数据流

```
作者 / AI 代理 / 导入器 ──PR──► cider-data（git，YAML/JSON，CC0）
    validate：JSON Schema → 语义 lint → 红线 lint → 确定性编译成 JSON
    publish（需审批的 environment）── minisign ──► GitHub Pages ────┐
                                            └──► 国内对象存储 + CDN ─┤ 均视为不可信传输
Cider.app / ciderctl ◄── timestamp → snapshot → 通道 index → 目标文件（sha256）
  CiderData：校验、防回滚、分层合并、when 求值、Verdict 查询 ──► 预检（09）、UI（05）
  CiderInstall：DAG 解析 → CAS 下载 → 步骤执行（APFS 快照）→ detect
  CiderRules：Profile → AppDefaults / ArgRules / env / bottle 配置 ──► 引擎 hook（02）
社区报告 / cider-lab 结果 / opt-in 遥测 ──► 聚合 ──► 评级与 Verdict 的 PR（人审）
```

### 2 仓库与本地布局

```
cider-data/                          # 独立仓库，CC0；thirdparty/ 保留原许可
  schema/{recipe,profile,game,feed}.v1.json
  policy/{redlines,domains}.yaml     # 红线规则；下载域名白名单
  recipes/{components,apps,launchers}/*.yaml
  profiles/{games,launchers,processes}/*.yaml
  games/<id>.json                    # 兼容库条目：ids、指纹、gating、routes、verdicts
  maps/{winetricks.yaml,ids.csv}     # verb → 组件；商店 ID 别名
  thirdparty/{umu,protonfixes,proton}/   # 原始快照、许可文本、来源哈希
  tests/{redline,golden,when}/   tools/importers/*.py
~/Library/Application Support/Cider/data/
  state.json                         # 各元数据已见过的最高 version/revision
  feeds/stable/<rev>/                # 保留最近 3 个 revision，供撤回时回退
  local/*.yaml                       # 本地覆盖，只对非门控条目生效
~/Library/Caches/Cider/cas/sha256/<aa>/<hash>
```

校验、lint、编译和 `when` 求值都在 `Packages/CiderKit/Sources/CiderData`，只依赖 Foundation，可在 Linux 编译；CI 与客户端共用一份代码，入口为 `ciderctl data validate|lint|compile|diff`。CI 另用 `check-jsonschema` 跑一遍 JSON Schema，同一份 schema 也供编辑器补全。

### 3 标识与匹配

- 游戏条目 id：有 umu-ID 就用（`umu-990080`、`umu-genshin`），否则用 `cider:<反向域名>`。`aliases` 记录 steam/egs/gog/amazon/hoyoplay 等商店 ID；`fingerprints[]` 记录 `{exe, sha256[], pe:{product, company, file_version}}`。id 永不复用。
- Recipe id：组件用 `runtime.*`、`font.*`、`launcher.*`，应用用反向域名（`cn.com.tdx.tdxw`）；Profile id 为 `profile.<target>[.<变体>]`。
- 识别优先级：商店清单（Steam `appmanifest_<appid>.acf`、HoYoPlay 配置）> 主 exe 的 sha256 > PE VersionInfo + exe 名 > 只有 exe 名（弱匹配，需用户确认）。不读 Legendary 清单（Epic 不做，00「不做清单」第 5 条）；经 Epic 安装的游戏只按 sha256/PE 和安装路径识别，够用来说清“不在支持范围”。
- `when` 只能引用固定的事实集合：`host.{macos,chip,ram_gb,rosetta}`、`engine.{version,cpu_backend,features.*}`、`gfx.{d3dmetal,dxmt}.version`、`bottle.locale`、`game.{edition,channel,version}`。写法是映射（隐式 AND）加 `any`/`not`，值为等值、列表或版本区间。不支持函数和字符串表达式，便于静态 lint，也能无损承接 UseIf。版本比较器同时理解 `11.18-c3`、`12.0-rc2` 和游戏版本 `3.2.0`。

### 4 Recipe schema v1（怎么装）

```yaml
schema: cider.recipe/v1
id: runtime.vcrun.v14
kind: component                   # app | component | virtual | launcher
revision: 4                       # 只增不减
name: { zh-Hans: "VC++ 2015–2026 运行库", en: "VC++ 2015–2026 Runtime" }
provides: [runtime.vcrun.2015, runtime.vcrun.2019, runtime.vcrun.2022, runtime.vcrun.2026, dll.mfc140]
conflicts: []
winetricks: [vcrun2015, vcrun2017, vcrun2019, vcrun2022, vcrun2026, mfc140]
requires: { engine: { version: ">=11.18" }, host: { macos: ">=14.0" } }
sources:
  x64:
    urls:
      - https://download.visualstudio.microsoft.com/download/pr/<guid>/<SHA256>/VC_redist.x64.exe
      - "cas:"                    # 按 sha256 走 mirrors.json，仅 mirrorable 为 true 时有效
    sha256: ["<2026-08-27 版>", "<2026-02-21 版>"]
    size: <bytes>
    authenticode: "Microsoft Corporation"     # CI 用 osslsigncode 核对签名者
    mirrorable: false
    refresh: { floating: "https://aka.ms/vc14/vc_redist.x64.exe" }   # 只给刷新机器人
  x86: { … }
steps:
  - dll_override: { mode: "native,builtin", dlls: [concrt140, msvcp140, msvcp140_2, vcruntime140] }
  - run_installer: { source: x64, kind: burn, args: ["/quiet", "/norestart"], timeout_s: 600 }
  - run_installer: { source: x86, kind: burn, args: ["/quiet", "/norestart"], timeout_s: 600 }
  - extract: { source: x64, format: burn-cab, files: ["msvcp140.dll", "msvcp140_2.dll"], to: "{system32}" }  # bug 57518
detect: { all: [ { file: "{system32}/msvcp140_2.dll", pe_version: ">=14.40" }, { file: "{syswow64}/vcruntime140.dll" } ] }
tests: { smoke: { detect: true, within_s: 900 } }
provenance: { source: "import:winetricks@20260125", reviewed_by: "@maintainer" }
---
id: cn.com.tdx.tdxw               # 应用片段：区域、依赖、由用户提供安装包
kind: app
bottle: { template: win10_64, locale: zh_CN.UTF-8 }       # ACP 936
dependencies: [runtime.vcrun.v14, font.cjk.macos-map]
sources: { installer: { urls: [], sha256: ["<A>", "<B>"], size: <bytes>, on_missing: ask_user, match_filename: "tdx*.exe" } }
steps: [ { run_installer: { source: installer, kind: auto, silent: true, timeout_s: 1800 } } ]
detect: { any: [ { file: "C:/new_tdx/TdxW.exe" } ] }
profiles: [profile.cn.com.tdx.tdxw]
```

规则：所有对象 `additionalProperties: false`；每个 source 必须有 `sha256[]` 和 `size`，`urls` 为空时必须写 `on_missing: ask_user`；新下载域名先登记到 `policy/domains.yaml`；`mirrorable` 默认 false，由维护者逐个组件设定（镜像只放许可允许再分发的文件）。

### 5 Profile schema v1（怎么跑）与动作白名单

```yaml
schema: cider.profile/v1
id: profile.umu-990080
revision: 3
target: umu-990080
match: { exe: HogwartsLegacy.exe, steam_appid: 990080 }
when: { engine.version: ">=12.0-c1", engine.cpu_backend: rosetta-x86_64, host.macos: ">=15" }
actions:
  renderer: { d3d12: d3dmetal }   # 不可用时降级，UI 和日志写明原因
  sync: msync
  avx_advertise: true
  env: { D3DM_ENABLE_METALFX: "1" }
  dependencies: [runtime.vcrun.v14]
drm: { denuvo: true, freeze: [engine, cpu_topology, avx_advertise] }   # 首次成功后冻结，改动前警告
warn: [ { if: { host.ram_gb: "<16" }, key: mem.low } ]
known_issues: [ { symptom: "…", cause: "…", fix: "…", tracking: "cider#123" } ]
---
id: profile.process.webview2      # 进程级规则，对所有 bottle 生效
scope: process
processes:
  - match: { exe: msedgewebview2.exe, file_version: ">=149 <155" }
    winver: win7                  # bug 58921
    args: { remove: ["--use-gl"], append: ["--use-gl=angle", "--use-angle=d3d11"] }
```

动作白名单（安装期写在 Recipe steps，运行期写在 Profile actions；全部幂等；schema 不认识的键直接拒绝）：

| 类别 | 安装期 steps | 运行期 actions | 落点 |
|---|---|---|---|
| 安装 | `run_installer`（auto/msi/inno/nsis/installshield/burn/exe）、`extract`（cab/zip/7z/msi-admin/inno/burn-cab）、`remove_mono`、`webview2_fixed`、`service` | `dependencies`（缺失时先装） | CiderInstall |
| DLL | `dll_override`、`copy_dll`、`register_dll` | `dll_overrides` | 注册表 / AppDefaults |
| 系统 | `registry`（set/delete/import）、`winver`、`fonts`（install/substitute）、`file`（touch/mkdir/symlink/copy/ini/xml，限 bottle 内） | `registry`、`winver`、`file` | bottle |
| 进程 | — | `args` 与 `processes[]`（append/remove/replace、features 合并、`cef` 字段；按父 exe、参数、文件版本匹配）、`env`、`stand_in_process`、`block_process` | cider-rules |
| 图形 | — | `renderer`（按 API，取值受 ADR-006 约束）、`retina`、`frame_limit`、`metalfx`、`hud`、`gpu_identity`、`nvapi_stub` | 引擎 / 04 |
| CPU/同步 | — | `sync`（msync/server）、`cpu_topology`、`avx_advertise`、`writecopy`、`large_address_aware` | 引擎 / 02 |
| 元数据 | — | `drm.freeze`、`warn`、`known_issues`、`game_mode` | App |

`env` 另有全局黑名单：`DYLD_*`、`LD_PRELOAD`、`WINELOADER`、`WINESERVER`、`WINEDLLPATH`。`run_installer` 只能执行按 sha256 固定的来源或已安装应用自身的 exe（Wine 的 Z: 盘能访问宿主文件系统，所以 bottle 本身不是沙箱）。

### 6 cider-rules 编译

启动前由 `CiderRules` 依次合并“进程级 → 启动器 → 游戏 → 本地覆盖”，后者覆盖前者；`features` 做并集，`remove` 先于 `append` 执行。输出三样：
- 注册表：`HKCU\Software\Wine\AppDefaults\<exe>\{Version,DllOverrides}`，以及 `…\<exe>\Cider\ArgRules`（REG_MULTI_SZ，一行一条 JSON，格式与 02 的子进程侧改写 hook 共用 [18 §5]）。
- 环境变量：只由 CiderKit 在 `posix_spawn` 时注入。
- `<bottle>/.cider/rules.lock.json`：记录输入（profile id@revision + sha256）和输出哈希。相同输入必须得到相同输出（CI 做 golden 测试），diag 包附带此文件。

```json
{"id":"profile.process.webview2#0","rev":2,"match":{"fileVersion":">=149 <155"},
 "remove":["--use-gl"],"append":["--use-gl=angle","--use-angle=d3d11"]}
```

### 7 依赖 DAG 与 winetricks 兼容

- 解析：从目标 Recipe 展开 `dependencies`，按 `provides` 选提供者（已安装 > 官方 stable > revision 最高），再用 `conflicts` 对照 bottle 已装集合（`cider-bottle.json` 的 `components`，ADR-008）；用 Kahn 拓扑排序，有环就报错并列出环。
- 执行：每个节点先跑 `detect`，已满足就跳过；否则 `clonefile` 快照 → 下载 → 执行 steps → 再跑 `detect`，失败回滚到快照并给出分类原因。x86 与 x64 两半算同一个节点。
- 模板 bottle：`dotnet48`、`vcrun` 等重组件预装成模板，按应用克隆，避免在 Rosetta 下反复跑微软安装器 [10 P1-9]。
- winetricks：`maps/winetricks.yaml` 把 verb 映射到组件（如 `vcrun2022 → runtime.vcrun.v14`、`fakechinese → font.cjk.macos-map`）。`ciderctl tricks <verb>` 只走映射。未映射的 verb 只能由用户手动执行 `ciderctl tricks --upstream <verb>`（内置固定版 winetricks，标“不受支持”），数据文件不得引用。首批映射 30 个：vcrun 全系、dotnet40/48、dotnet 与 dotnetdesktop 8/10、d3dx9、d3dx11_43、d3dcompiler_47、xact、xinput、corefonts、cjkfonts、msxml6、gdiplus、physx、webview2 和 winver 类。

### 8 内容寻址下载与镜像

- 下载器用 Swift `URLSession`，不依赖宿主的 curl。按 `urls` 顺序尝试：带哈希路径的不可变 URL > 厂商 URL > `cas:` 镜像 > web.archive `id_`。边下边算 sha256，超出 `size` 立即中止，支持 Range 续传，命中任一哈希即通过。不提供“忽略校验”开关，只有开发构建认 `CIDER_DEV_ALLOW_HASH_MISMATCH`。
- 缓存：写入先落 `.partial`，校验后原子改名；按 LRU 回收，默认上限 20 GB，模板 bottle 引用的条目钉住不回收。
- 镜像清单 `mirrors.json` 随 index 一起签名：

```json
{"mirrors":[
 {"id":"gh-pages","base":"https://<org>.github.io/cider-data/","regions":["*"],"weight":1},
 {"id":"cn-cdn","base":"https://<服务商默认域名>/cider-data/","regions":["CN"],"weight":3}]}
```

- 选源：先按系统区域和时区预估地区，再并发取各镜像的 `timestamp.json`（3 秒超时）测延迟，结果以 EWMA 持久化，出错即切换。请求不带查询参数和任何用户信息。元数据与 `mirrorable` 目标按 `v1/`、`cas/sha256/` 布局同步到国内对象存储，基础设施归 07。游戏本体始终走官方 CDN。

### 9 签名、通道与防回滚

v1 用 minisign 实现 TUF 形状的四类元数据：

| 文件 | 签名密钥 | 内容 | 有效期 |
|---|---|---|---|
| `root.json` | root（2 把，离线，两人分持） | 各角色公钥、阈值、version | 1 年 |
| `timestamp.json` | timestamp（`data-timestamp` environment，定时每日重签） | snapshot 的 sha256、size、version | 7 天 |
| `snapshot.json` | release（`data-release` environment，需人工批准） | 各通道 index 的 version 与哈希 | 30 天 |
| `<channel>/index.json` | release | 每个目标的路径、sha256、size、revision；`yanked[]`；`min_client` | 同 snapshot |

- trusted comment 写入 channel、version、expires，与正文交叉校验。App 内置两把 root 公钥；轮换时新 `root.json` 必须由旧 root 签名，客户端逐版验证整条链。
- 客户端按 root → timestamp（未过期、version 不减）→ snapshot（哈希吻合、不减）→ index（哈希吻合、revision 不减）→ 目标（sha256）的顺序校验。任何一步失败都保留上一份有效数据，UI 标“数据未更新”。每 6 小时拉取一次，带 ETag。
- 过期：timestamp 过期后继续用本地数据；门控条目（§11）的 `playable` 在过期 14 天后按 `unverified` 处理。
- 撤回：`yanked` 中的目标立即停用，回退到本地保留的上一 revision；无可回退时按“无 profile”运行并提示。
- 通道：`stable` 为默认；`beta` 需用户显式订阅，只放新 profile 和组件，不放门控条目的放宽。第三方源（P4）用独立密钥，UI 标明信任级别，不得覆盖官方 id，也不得包含门控条目。
- 引擎索引用另一把 Ed25519 密钥（ADR-009），校验代码复用。
- v2（P4）迁到完整 TUF（按目录委托给社区维护者、阈值签名），触发条件是第二维护者到位或出现第三方源。

### 10 兼容库：Game 条目、Verdict 与评级

```json
{"schema":"cider.game/v1","id":"umu-genshin","aliases":{"egs":"<codename>"},
 "names":{"zh-Hans":"原神","en":"Genshin Impact"},"api":{"d3d":11},
 "editions":[
  {"edition":"cn","exe":"YuanShen.exe","channels":["mihoyo-launcher","mihoyo-launcher-bilibili"],
   "version_source":{"local":"config.ini","api":"hyp-api.mihoyo.com","game_id":"<id>"}},
  {"edition":"global","exe":"GenshinImpact.exe","channels":["hoyoplay","epic"],
   "version_source":{"local":"config.ini","api":"sg-hyp-api.hoyoverse.com","launcher_id":"VYTpXlbWo8","game_id":"gopR6Cufr3"}}],
 "gating":{
  "anticheat":{"vendor":"hoyoverse","kernel":true,"components":["mhypbase.dll","HoYoKProtect.sys"],"policy":"run-as-designed"},
  "preflight":"hoyo",
  "hard_blocks":[{"id":"kprotect-needs-wdfldr","if":{"game.version":">=6.5","engine.features.wdfldr":false},
                  "result":"blocked-anticheat","evidence":["yaagl#653","yaagl#763"]}],
  "diag_signatures":["DRIVER_IMPORT_MISSING(WDFLDR.SYS)","ZWLOADDRIVER_FAIL(c0000142)","INITDRIVER_FAILED"]},
 "routes":[
  {"kind":"pc-client","priority":1},
  {"kind":"official-cloud","edition":"cn","form":"web","url":"https://ys.mihoyo.com/cloud/",
   "open_with":["chrome-app","edge-app","default-browser"],"status":"verified"},
  {"kind":"official-cloud","edition":"global","form":"windows-cloud-client","status":"unverified","gate":"E4"}],
 "verdicts":[]}
```

绝区零 Steam 版的一行 Verdict（E1 出结果前）：

```json
{"target":"cider:mihoyo.zzz","edition":"global","channel":"steam","game_version":">=3.2.0 <3.3.0",
 "engine_major":"12","macos_major":"27","cpu_backend":"rosetta-x86_64","engine_range":null,
 "result":"unverified","ac_popup":null,"valid_until":null,"last_verified":"2026-11-10",
 "provenance":{"source":"cider-lab","run":"lab#E1-001","env_modified":false,"hw":{"chip":"M3","ram_gb":8}}}
```

- 键：游戏 × edition × channel × 游戏版本区间 × 引擎主版本 × macOS 主版本 × `cpu_backend`（ADR-010），任一维可写 `*`；可选 `engine_range` 收窄到具体引擎构建。
- `channels` 是识别词表，不是支持承诺：`epic` 保留在国际服的 `channels` 里，只用于把安装识别清楚、让裁定和路线卡如实写出“不在支持范围”，Cider 不集成 Epic（00「不做清单」第 5 条、05 §15 #55）。R3 范围本来就不含国际服，路由由 `routes[]` 和预检决定，不由 `channels` 决定。
- 查询：先判 `hard_blocks`，命中即 blocked；否则取覆盖当前组合、非 `*` 维数最多的一条，平手取 `last_verified` 最新；一条都没有就是 `unverified`。接口为 `CompatDB.verdict(for: VerdictKey) -> VerdictDecision`（结果、原因码、证据链接），供 09 调用。
- `result` 取值：`playable`、`playable-caveats`、`unverified`、`blocked-anticheat`、`blocked-drm`、`broken-launcher`、`broken`。映射到预检四态：前两个 → playable；`broken-launcher` → 只开启动器；`broken` 与 `blocked-*` → 路线卡；其余 → unverified。
- 门控条目的 `playable` 只接受 `source=cider-lab` 且 `env_modified=false`，并且必须带 `valid_until`（默认 last_verified + 30 天）。过期即视为 `unverified`，因为服务端策略可能在版本不变时改变。
- 路线 `form ∈ {web, windows-cloud-client, native-macos}`，枚举外的值 schema 直接失败；`status=unverified` 的路线不展示；改成 `native-macos` 需附官方下载地址和 `CFBundleSupportedPlatforms=MacOSX` 证据 [22 §5.1]。

评级只用于非门控条目，按“条目 × 引擎主版本 × 后端”计算：

| 档位 | 推导规则（问卷与遥测事实） |
|---|---|
| platinum | 装好即玩、无需任何操作，成功会话 ≥95% |
| gold | 自动 profile 生效后正常，≥90% |
| silver | 能玩但有非致命的已知问题，≥70% |
| bronze | 只有部分功能可用（如仅离线、仅故事模式） |
| borked | 装不上或跑不起来 |
| blocked | 反作弊或 DRM 门控，不属于质量档 |

- 证据权重：cider-lab 1.0 > 维护者 0.8 > 可复现的社区报告（同一 profile hash、≥2 台机器）0.6 > 遥测聚合 0.5 > 导入 0.3 > 预测 0.1；半衰期 90 天；引擎主版本变化后旧证据降为“历史”。
- `last_verified` 必填，UI 显示“N 天前在 <引擎>/<macOS>/<芯片> 上验证”；Top-50 要求 ≤30 天。

### 11 反作弊 / DRM 门控与红线 lint

`policy/redlines.yaml` 由 `CiderData.Lint` 执行，CI 和客户端加载时各跑一次；客户端命中时丢弃该条目并记日志。

| 规则 | 范围 | 拒绝的内容 |
|---|---|---|
| RL-IOS | 全部 | 枚举外的 `form`；`apps.apple.com`、`itms-apps:`、PlayCover、`.ipa` 等引用 |
| RL-INJECT | 全部 | `DYLD_*`、`LD_PRELOAD`；任何脚本或 shell 字段 |
| RL-KMDF | 全部 | 伪造成功的内核桩、按反作弊特判的 KMDF/wdfldr 组件（K 线的忠实 KMDF 运行时不在此列，由 Windows 对照 conformance 守门，见 00 R3） |
| RL-SPOOF | 门控 | SteamOS/SteamDeck 类变量、由 Profile 设置的 `SteamAppId`/`SteamGameId`、`HideWineExports`、`gpu_identity`、`nvapi_stub` |
| RL-NET | 门控 | 断网、网络延迟、hosts 改写 |
| RL-GAMEDIR | 门控 | 指向游戏目录的 `file`、`extract`、`copy_dll` |
| RL-ARGV | 门控 | 游戏主进程的 `args.remove/replace`、`block_process`、`stand_in_process` |
| RL-OVERRIDE | 门控 | 本地覆盖、beta 或第三方源中出现 Verdict、`hard_blocks` 或 `preflight` 的改动 |

“门控”指 `gating.anticheat.vendor = hoyoverse` 或 `gating.anticheat.kernel = true`。`tests/redline/` 至少放 24 个违规样例。DRM 方面：`drm.denuvo = true` 时 `freeze` 必填；`drm.kernel`（SafeDisc、SecuROM、StarForce）直接评为 `blocked-drm`。

### 12 社区提交与审核

- 入口：① 数据 PR；② GitHub issue 表单“兼容性报告”；③ 应用内“报告”，生成 `report.json`（问卷答案，加上自动采集的芯片、内存档、macOS、引擎 id、profile hash、实际后端、首帧与崩溃签名）和脱敏 diag 包，可附到 issue、中文社区或邮件，不要求 GitHub 账号（ADR-012）。
- `report-ingest` 机器人：校验格式；识别被改过的环境（第三方 Wine、启动器补丁、GPU 身份伪装 [21 P0-3]），打 `env-modified`；查重；按条目打标签。
- 分级：`community`（未审）→ `triaged`（机器人通过）→ `accepted`（维护者）→ `verified-lab`。门控条目的社区报告只用来触发实验室复测，永远不会直接产生 `playable`（ADR-010）。
- PR 审核：`CODEOWNERS` 把 `schema/`、`policy/`、门控条目和 Top-50 profile 留给维护者，其余可由合并过 ≥5 个 PR 的贡献者审。`diff-risk` 机器人把新下载域名、新 URL、新安装源判为高风险，必须维护者批准；只改 actions 的判为普通。

### 13 导入器

导入器放在 `tools/importers/`（Python），只产出草稿 PR，provenance 标 `import:*`，结论一律为 `unverified`，验证后才改。

| 来源 | 方式 | 映射要点 | 阶段 |
|---|---|---|---|
| Highball-db（CC0） | 批量、定期 | `steps` → Recipe/Profile；`renderer/sync/env` → actions；`knownIssues` → known_issues；`lastVerified` → 外部引擎证据；`blocked`/`macVerdict` → gating 提示 | P2 |
| umu-protonfixes（BSD-2） | 批量；用 Python `ast` 静态解析，不执行 | `protontricks` → dependencies；`set_environment`、`winedll_override`、`append_argument`、`regedit_add`、`set_cpu_topology_limit` → 对应 actions；`disable_*sync`、`install_eac_runtime`、`PROTON_*` 丢弃并记录 | P2 |
| Proton `default_compat_config`（BSD-3） | 批量 | `forcelgadd` → `large_address_aware` 等可移植 flag | P2 |
| umu-database（GPL-3） | 原样放在 `thirdparty/umu/`，编译时生成 ID 别名索引 | 商店 ID ↔ umu-ID | P1 |
| winetricks | 半自动 | `w_download` 的 URL 与 sha256、`w_call`、`w_override_dlls`、`w_set_winver` → 组件草稿；复杂逻辑标 `needs-human`，交代理补全 | P2 |
| CrossTie `.tie` | 仅用户本地（`ciderctl import tie`），产物不签名，标“本地” | 安装检测 → detect；Pre/Post-Dependencies → dependencies；`<useif>` → `when` | P3 |
| Lutris YAML | 仅用户本地 | `task`（wineexec、set_regedit 等）→ steps；`execute` 拒绝 | P3 |

### 14 数据侧 CI（`cider-data/.github/workflows/`）

| 工作流 | 触发 | 内容 | Runner |
|---|---|---|---|
| `validate` | PR | JSON Schema、语义 lint、红线 lint、确定性编译（两次输出哈希一致）、`when` 单测、diff-risk、域名白名单 | ubuntu-latest |
| `links` | 每日 | 所有 URL 做 HEAD 或 Range GET；ETag 或大小变化时下载比对 sha256；死链和漂移开 issue；必需 source 无可用 URL 时标 `needs-source` | ubuntu |
| `refresh-floating` | 每日 | 抓 `refresh.floating`；出现新哈希 → Authenticode 校验 → 组件冒烟 → 自动 PR 追加哈希（保留旧值） | macos-26 |
| `smoke-components` | 每周与相关 PR | 在 Engine R 的新 bottle 中装首批组件，`detect` 通过，记录耗时 | macos-26 |
| `hoyo-watch` | 每 30 分钟 | 只读 `getGameBranches` 的 `tag`；出现新版本 → PR 加一行 `unverified`（带检测时间）并开实验室 issue | ubuntu |
| `publish` | 合入 main | 编译 → snapshot/index → minisign（`data-release` 审批）→ Pages 与国内镜像同步 → 回读校验 | ubuntu |
| `timestamp` | 每日 | 重签 timestamp | ubuntu |
| `lab-ingest` | cider-lab 回传 | `lab-result.json` → Verdict PR | ubuntu |

应用与游戏冒烟、LRS、HoYo 验证都在 cider-lab 进行（11），结果经 `lab-ingest` 回流。

### 15 可选遥测 → 评级

- 默认关闭。同意页（zh-Hans/en）逐项列出字段；`ciderctl telemetry show-last` 可查看发出的原文；随时撤回。
- 事件 `launch_outcome`：条目 id（只报库内已知条目，未知应用只计数）、profile/recipe revision、引擎 id、`cpu_backend`、macOS 主次版本、芯片代际、内存档、实际后端与降级原因、结果（首帧、首帧前崩溃、退出码类别）、会话时长分桶、HUD p50/p95 分桶、崩溃签名（模块加偏移的哈希）。不含路径、用户名、参数和账号。安装 id 随机生成，90 天轮换。
- 聚合：每个桶 ≥5 个安装才公开；每周生成评级更新 PR，人审后合入。遥测只能把门控条目降为 `unverified`（与 09 的恢复闭环一致），不能升级。
- 接收端（P2）：HTTPS 批量上报到最小化的 ingest，海外与国内各一处（部署归 07），服务端设总开关。

## 实施计划（WBS）

只计本文独占的工作；Top-50 profile 的数据运营计入 08，预检与路线卡 UI 计入 09 和 05。

| 编号 | 工作包 | 产出 | 验收标准 | 估时（人周） | 依赖 | 阶段/里程碑 |
|---|---|---|---|---|---|---|
| D01 | Schema v1 | `schema/*.v1.json`、Swift 严格解码、每类 3 个样例 | 样例全过；缺字段、多字段、iOS form 全部失败 | 0.5 | — | P0（10-15） |
| D02 | `when` 与版本求值器 | `CiderData.Eval` | 属性测试 ≥1,000 例；UseIf 的 8 种操作符都有对应写法 | 0.2 | D01 | P0 |
| D03 | 红线 lint | `redlines.yaml`、`CiderData.Lint`、24 个违规样例 | 违规样例 100% 被拒，合法样例 0 误报；设为 CI 必过检查 | 0.3 | D01 | P0 退出标准 |
| D04 | 签名发布 v1 与客户端校验 | `publish`/`timestamp` 工作流、`CiderData.Feed`、`state.json` | “测试与验收”中 6 类攻击全部被拒；发布必须经审批 | 0.5 | D01 | P0（M-A 前） |
| D05 | HoYo 三款条目 | 3 个 `games/*.json` | 3 款 × 2 区服 × 各渠道样例通过校验；伪安装查询结果全部不是 playable | 0.2 | D01、D03、09 | P0 |
| D06 | `hoyo-watch` | 工作流 | 版本变化后 30 分钟内出 PR 和 issue | 0.15 | D05 | P0 |
| D07 | CAS 下载器 | `CiderInstall.Fetch` | 单测全过；1 GB 文件断网后续传成功；错哈希 0 落盘 | 0.5 | D04 | P1 |
| D08 | DAG 与执行器 | `Resolve`/`Execute`、APFS 快照 | 注入失败步骤后 bottle 与快照逐字节一致；已装组件重装时 0 步执行 | 0.8 | D07 | P1 |
| D09 | 首批 12 个组件 | vcrun.v14、vcrun2005–2013、d3dx9、d3dcompiler_47、corefonts、font.cjk.macos-map、webview2.fixed、dotnet48、dotnetdesktop8/10 等 | macos-26 冒烟全过 | 0.5 | D08 | P1（0.1 前） |
| D10 | cider-rules 编译器 | `CiderRules`、ArgRules 契约测试 | golden 一致；与 02 的 hook 联调，WebView2 参数删改在日志中可见 | 0.4 | D02、02 | P1 |
| D11 | 漂移检测与刷新机器人 | `links`、`refresh-floating` | 回放 2026-08 vcrun 哈希变更，24 小时内自动出 PR | 0.3 | D09 | P1 |
| D12 | 数据国内镜像 | `mirrors.json`、同步步骤、测速选源 | 屏蔽 GitHub 后 60 秒内完成数据更新 | 0.2 | D04、07 | P1（0.1 前） |
| D13 | 组件扩到 30 个与 winetricks 映射 | 组件、`maps/winetricks.yaml`、`ciderctl tricks` | 30 个 verb 有映射且冒烟通过；dotnet48 通过 ClickOnce 用例 | 0.8 | D09 | P2 |
| D14 | 批量导入器 | Highball、protonfixes、Proton compat、umu 索引 | 产物 100% 过 lint；抽检 20 条，映射正确率 ≥90% | 0.6 | D03 | P2 |
| D15 | 社区报告与审核 | issue 表单、`report.json`、`report-ingest`、CODEOWNERS、diff-risk | 50 份样例报告自动分级正确；高风险 PR 无维护者批准无法合并 | 0.6 | D01 | P2 |
| D16 | beta 通道与撤回演练 | 通道、yanked 回退、root 轮换 | yank 后 6 小时内客户端回退；轮换后旧客户端能正常更新 | 0.3 | D04 | P2 |
| D17 | 遥测与评级 | 同意页、上报、ingest、`rating` 计算 | 字段审计无个人信息；k ≥ 5；每周自动生成评级 PR | 0.8 | D15、07 | P2（2027-04 前） |
| D18 | 本地导入器 | `ciderctl import tie\|lutris` | 10 个公开样例转换后能安装，或明确列出不支持的项 | 0.5 | D08 | P3 |
| D19 | Engine A 与 macOS 28 维度 | `cpu_backend=fex-arm64ec` 的数据迁移，R 与 A 证据隔离 | A 上没有证据的条目全部为 unverified | 0.3 | 03 | P3 |
| D20 | 完整 TUF 与第三方源 | 委托、阈值签名 | 通过 TUF 一致性测试集 | 0.8 | D16 | P4 |

合计约 9.3 人周：P0 1.85（约 11 窗，属于 R3 H0 与 M-A）、P1 2.7、P2 3.1、P3 0.8、P4 0.8。

## 测试与验收

- 单元：版本比较器与 `when`（属性测试）；DAG（环、冲突、provides 选择）；CAS（截断、错哈希、多哈希、续传）；Verdict 查询（最具体匹配、hard_block、valid_until）。
- golden：`tests/golden/` 固定每个 profile 的编译输出，改动需同步更新快照。
- 签名攻击（6 类）：旧 index/旧 snapshot 回滚、过期 timestamp 冻结、index 与 snapshot 哈希不符（混搭）、错误密钥、镜像返回篡改文件、root 轮换链断裂。客户端必须全部拒绝并保留旧数据。
- 红线：24 个违规样例全部被拒；本地覆盖试图修改门控条目的 Verdict 时被丢弃，预检结果不变。
- 集成：在 macos-26 上新建 bottle 安装首批组件，全部 `detect` 通过；用 hosts 屏蔽 GitHub 后，经国内镜像完成数据更新和组件下载。
- 1.0 发布门禁：Top-50 条目 `last_verified` ≤30 天；stable 通道 0 条 schema/lint 告警；timestamp 连续 30 天未过期。

## 风险与预案（触发条件 → 动作）

| # | 触发条件 | 动作 |
|---|---|---|
| 1 | 一周内 ≥3 个组件因上游换文件失败 | 改用带哈希的不可变 URL；允许的组件设 `mirrorable`；临时切到 `on_missing: ask_user` 并在 UI 说明 |
| 2 | 出现非预期 revision 或未经审批的签名 | yank；靠 timestamp 过期限制冻结窗口；用 root 轮换 release 密钥；公告 |
| 3 | PR 引入可疑下载（新域名、Authenticode 不符） | diff-risk 阻断，维护者复核；已合入的立即 revert 并 yank |
| 4 | 数据运维占比连续两周 >40% | Top-N 从 50 降到 30；非门控条目允许直接合入 triaged 报告；推迟 D17、D18 |
| 5 | HoYo 版本复核超过 72 小时 | 条目已默认为 unverified 并给路线卡；没有测试 Mac 时 SLA 放宽到 7 天（00 资源节） |
| 6 | 国内镜像不可用 | 切第二家服务商；客户端回落 GitHub；备案完成后改自定义域名 |
| 7 | schema 需要不兼容变更 | 升 schema 主版本并设 `min_client`；新旧版本并行发布两个 App 版本周期 |
| 8 | 导入数据误导（Highball 结论来自其自有引擎） | 导入一律 unverified 并标外部证据；抽检不达标就暂停该导入器 |
| 9 | 遥测出现隐私问题 | 服务端总开关关停，删除原始数据，公开说明 |
| 10 | 有人借本地覆盖或第三方源绕过预检 | 客户端 lint 丢弃该条目；按安全缺陷处理，48 小时内修复 |

## 未决问题与需实机验证的点

1. **对 ADR-010 的顾虑（仍按 ADR 执行）**：Verdict 键只到“引擎主版本”，但 Cider 每两个 Wine tag 就发一次引擎，同一主版本内反作弊行为可能变化。本文额外加了可选的 `engine_range` 和门控条目 30 天的 `valid_until`。建议 09 复审时考虑把门控条目的键改为引擎构建区间。
2. 三款游戏的 umu-ID：只确认 `umu-genshin` 存在；星铁、绝区零暂用 `cider:mihoyo.*` 并挂别名。
3. HYP 本地 `config.ini` 的版本字段与 API `tag` 如何对应，以及国服的 `game_id`，需在测试机上核对（09 E2）。
4. 绝区零日服云的真实形态（22 号 P1-7a）。核实前保持 `status=unverified`，iPad 形态永不展示。
5. Swift 在 ubuntu runner 上构建 CiderData 是否可行、耗时多少；不行就把 `validate` 移到 macos-26。
6. 系统 libarchive 能否解开 Burn 内嵌的 cab [10 未解问题 6]；不能就随引擎带 cabextract/libmspack。
7. 哪些组件可以设 `mirrorable: true`，需逐个确认再分发条款（本文不展开）。
8. 门控 `valid_until` 30 天、过期 14 天降级、证据半衰期 90 天等阈值，要用首季数据校准。
9. Denuvo 的 `freeze` 是否必要，AVX 开关是否会被计为新机器 [08 未解问题 5]，需要实测。
10. 遥测 ingest 在国内的部署形态（归 07）。
11. Swift 侧有没有可用的 TUF 客户端，还是要自写最小子集，这会影响 D20 的估时。

## 与其他文档的接口

| 文档 | 本文提供 | 本文需要 |
|---|---|---|
| 01 架构 | `CiderData`、`CiderInstall`、`CiderRules` 的模块边界；`cider-data` 仓库 | 模块依赖规则（CiderData 只依赖 Foundation） |
| 02 引擎 | ArgRules 行格式、AppDefaults 键、`sync`/`writecopy` 取值 | 子进程侧 argv 改写 hook；引擎清单中的 `features`（`wdfldr`、`argv_rewrite`） |
| 04 图形 | `renderer` 取值与降级原因码 | `gfx.*` 可用性事实、D3DMetal 导入状态 |
| 05 App/CLI | 评级、known_issues、路线数据、本地覆盖；`ciderctl data/tricks/import/report/telemetry` | UI 呈现与同意页 |
| 07 平台 | `mirrors.json` 格式、CAS 布局、签名角色 | 国内对象存储与 CDN、密钥托管、ingest 部署 |
| 08 启动器 | Profile 格式与红线 lint | 启动器 profile、LRS 结果、Top-50 数据运营 |
| 09 HoYo | Game 条目的 gating/routes/verdicts、`hoyo-watch`、`CompatDB.verdict(for:)` | 预检状态机、恢复闭环的本地降级、diag 分类 |
| 10 应用 | Recipe 格式、组件目录、安装执行器 | WebView2 固定通道版本、运行库清单 |
| 11 QA | 数据侧 CI、`lab-result.json` 格式 | cider-lab 的执行与结果回传 |
| 12 / 13 | WBS 与估时 | 窗口卡与排期 |
