# Cider 战略草案 A：Parity First（先对齐 CrossOver，再迁移到 ARM64）

> 2026-09-27 · 草案 · 依据 docs/research 01–22，矛盾处按 00 号报告的裁定和各报告的事实核查记录处理。本文中“窗口”指 1 个 5 小时 AI 配额窗口。

## 0. 论点

1. **尽快做出可信的 CrossOver 对齐版本**。先用 Engine R（x86_64 Wine 11.x→12.0，新 WoW64，经 Rosetta 运行）发布，并尽量复用现成工作：
   - CX 26.3 的 LGPL 差异：dappermint `wine1117` 已经把它前移到 11.17，可以直接接手，再与 marzent `ff-wine-11.17` 交叉核对；
   - DXMT；
   - D3DMetal，由用户自行导入；
   - frankea/Whisky 的 WhiskyKit；
   - Gcenx 的 configure 参数和依赖配方；
   - Highball 的 CC0 配方和兼容库；
   - umu-protonfixes 的动作原语。
2. **发布节奏**：
   - 2026-11-30：0.1 技术预览；
   - 2027-01-31：0.5 公测；
   - 2027-04-30：1.0，基于 Wine 12.0，完成 CX 26 功能清单的 ≥90%；
   - 2027-06-30：Engine A beta；
   - macOS 28 正式版发布后 7 天内：2.0，新 bottle 默认用 Engine A。
3. **怎样赶上 macOS 28**：
   - Apple 的 `cross-architecture-support` entitlement 是所有工作里准备时间最长的一项，所以第 1 周就提交申请；
   - Engine R 的所有产物从第一天起按架构参数化，包括 manifest 的 `arch`、bottle 的 `cpu_backend`，以及从 2026-12 起在 CI 构建 DXMT arm64x。这样 App、数据和 CI 不用改，Engine A 只需要换引擎；
   - 2027-02 起，至少 50% 的窗口分给 Engine A，Engine R 转为“只通过数据热修”的维护状态；
   - 另设 4 道硬闸门，每道都有降级方案（§3）。
4. **差异化不放在引擎上**，而在这几处：按应用配置、开放并签名的兼容库、后端回退对用户可见、中文优先、HoYo 游戏先预检、用户不会撞上反作弊弹窗。

## 1. 阶段计划（2026-10 → 2027-12）

| 阶段 | 时间窗 | 目标与交付物 | 可测的退出标准 |
|---|---|---|---|
| **P0 奠基与对照** | 10-01→10-31 | 开发机装 Rosetta、Xcode 26.6 + Metal toolchain、llvm-mingw、bison≥3、ccache；加入 ADP，提交 Capability Request 和 DTS 工单；自行归档 `crossover-sources-26.3.0` 及其 sha256；CI 产出 oracle（CX 26.3 原样构建）和 `cider-wine` v0（11.18 + 补丁队列）；`cider-probe`；反作弊政策和 lint | 两个引擎在 macos-26 runner 上跑 `wineboot --init`、`cmd /c ver`、`syswow64\cmd /c ver` 全部通过；`DYLD_PRINT_LIBRARIES` 显示没有加载 /usr/local 或 /opt/homebrew 下的库；probe 输出 6 项 JSON；entitlement 工单号已存档；lint 拒绝全部 10 个违规样例 |
| **P1 Engine R 核心 → 0.1** | 11-01→11-30 | 补丁队列 v1（D2）；msync；winemac C1–C3、C5；GL remap；DXMT builtin；D3DMetal 导入器；裁剪版 GStreamer 1.28；sdl2-compat + SDL3；CJK 字体和 locale；CiderKit、`ciderctl`；Ed25519 签名的引擎 manifest | 64 位 Steam 能登录、商店页能渲染；msync 下 20 次冷启动出现 0 次 “killing unresponsive browser”；NT 对象同步基准 p50 至少是 server 模式的 5 倍；12 款冒烟游戏中，oracle 能跑的，cider-wine 也能跑 ≥90%；0.1 发布到 GitHub |
| **P2 公测 0.5** | 12-01→01-31 | SwiftUI GUI（zh-Hans/en）；recipe、profile、verdict v1 及签名通道；导入 Highball 数据和 umu 映射；预检和路线卡；R3 全套（§4）；公证 DMG；Sparkle 2.10；国内镜像；一键支持包；LRS 在 16GB 测试机上夜跑；E1–E4 取证 | DMG 通过公证并完成 staple；新机器从安装到 Steam 登录 ≤10 分钟；LRS L01–L07 至少 5 个通过；国内无代理环境下载引擎 ≤3 分钟；非 playable 的组合一次都不拉起游戏 exe |
| **P3 1.0 对齐** | 02-01→04-30 | Wine 12.0 基线（预计 1 月中下旬）；CX 27 源码公开后做差异的差异；启动器覆盖 Steam、EA、Battle.net、Ubisoft、Rockstar、Epic、GOG、HoYoPlay、米哈游启动器；按应用设置；bottle 复制、导出、导入，APFS 快照回滚；Run Command 和“另存为启动器”；每个游戏一个 shim .app，含文件关联；Game Mode 实测（T4）；固定版本的 WebView2 通道；mtld3d 实验白名单；投递 HoYo 合作材料 | `docs/parity.md`（从 CX 26 功能清单拆出 40 项）中 ≥36 项完成；LRS 9/9 连续 7 夜通过；Top-50 条目的 `last_verified` 都在 30 天以内；opt-in 崩溃率 <2%/会话 |
| **P4 Engine A α→β**（与 P3 并行） | 02-01→06-30 | 上游 FEX PE 模块（`libarm64ecfex.dll`、`libwow64fex.dll`），FEXUnixLib 的 7 个 unixcall 移植到 Darwin；Wine 的 arm64-macOS 宿主部分优先吸收 CX 27 源码；`CiderEngineA.app`；DXMT arm64x；wine-mono 11.3 arm64；R→A bottle 迁移工具 | α（04-15）：x64 和 i386 控制台程序、DXMT D3D11 示例、WinForms 示例都能跑。β（06-30）：冒烟集中 ≥70% 的 D3D11 游戏可玩；CPU 基准达到 Engine R 的 75% 以上；迁移 20 个 bottle 不丢数据 |
| **P5 macOS 28 就绪 → 2.0** | 07-01→28 正式版发布后 7 天 | 每个 28 beta seed 都跑回归；用 `game-test-tool` 验证 Engine R；26.5+ 的新 bottle 默认用 Engine A；没有 arm64 版 D3DMetal 时，DX12 游戏路由到 Engine R 的 bottle（仅限 macOS 27 及以下）；在 Engine A 上重跑 E1–E3 | 按时发布 2.0；Engine A 上 LRS 至少 7/9 通过；在 28 上打开 Engine R 的 bottle 时，每次都给出迁移或说明，不会静默失败 |
| **P6 macOS 28 后加固** | 10→12-31 | 修 28.x 的回归；向 KosmicKrisp 上游贡献 B2（single-texel 对齐）；在内部分支上对 vkd3d-proton 做 bring-up；社区测试员计划；把 bus factor 提到 2 | 签名密钥和发布流程有 2 人能操作；每月至少 2 个上游 MR；Engine A 上 Top-100 条目全部复核完 |

## 2. 关键架构决策

**D1 引擎**
- bottle 元数据字段 `cpu_backend ∈ {rosetta-x86_64, arm64-fex}`。
- Engine R 先上线。
- Engine A 取决于 entitlement：拿不到授权时，32 位程序和 `KUSER_SHARED_DATA`（0x7ffe0000）都用不了，作为产品是 NO-GO（15 号报告）。
- Engine A 的最低系统暂定 macOS 26.5，最终以 probe 实测为准。VM 方案只做研究。

**D2 Wine 基线与补丁队列**
- 基线：`cider-wine/cider/devel` 以上游 wine-11.18 起步，每个 devel tag 变基一次，每两个 tag 发一次引擎。Wine 12.0 作为 1.0 的基线。CX 26.3 原样构建，只用作 oracle 和回退引擎。
- 每个补丁的头部标注 `Upstream-Status`。队列按主题分组：
  - `d3dmetal-abi`：`macdrv_functions`/`d3dmetal_objc.m`、CW 22434/22435/23015。24067 只有在 `nm -u libd3dshared.dylib` 证实需要时才保留；其中的 cxcompatdb 代码块丢弃。
  - `rosetta`：24256、23427、20186、22131、24265（M3 上的 MXCSR 问题）。
  - `msync`：CX 26.3 的实现，加上 dappermint 的 8 个修复，再加 `WINEMSYNC_STATS`。
  - `winemac`：子窗口跨进程 swapchain；!11058（ExtEscape）、!11799（GCMouse）、!11880（透明光标）。
  - `gl-remap`：`unix_wgl.c` 里的 `mach_vm_remap`。
  - `macos-ux`：10912、14364、18896、22310、22144。
- 应用特例改写成 profile 规则。20810、alt loader 和 %gs 字节补丁都不移植，改用上游的 GSBASE swap。
- PE 工具链：沿用 dappermint 和 Highball 已验证的 mingw-w64 gcc，因为 llvm 编出来的 kernelbase 会卡住 Steam 登录。llvm-mingw 只用于 Engine A 的 aarch64/arm64ec 构建。

**D3 同步**
- 游戏 bottle 默认开 msync。
- Steam 和 CEF 类 exe 由 profile 设为 `sync: server`，切换时自动重启该 bottle 的 wineserver。
- 2027 Q1 改用 os_sync 公开 API（要求 macOS 14.4+）。
- 等服务器能够等待 msync 对象之后，再放开同一 bottle 内按进程混用两种模式。

**D4 图形（按 API 路由，回退必须在 UI 和日志中可见）**

| API | 默认 | 备选 |
|---|---|---|
| D3D10/11 | DXMT main（builtin） | D3DMetal；wined3d-vk |
| D3D12 | D3DMetal：macOS 26 默认 3.0，4.0b2 按游戏开启，`D3DM_MTL4=0` | vkd3d（只对 FL11_0 白名单游戏） |
| D3D8/9 | wined3d-GL + remap | mtld3d 白名单；x87sidecar 可选 |
| DDraw / Vulkan / GL | cnc-ddraw / MoltenVK 1.4.2 / Apple GL 4.1 | wined3d / KosmicKrisp（macOS 26+）/ — |

D3DMetal 的许可证要求不得修改、仅限非商业用途，所以它不进仓库，也不放进主 bundle，默认由用户从 GPTK 导入。导入时用 `lipo -archs` 记录架构。

**D5 App 与 CLI**
- 按模块吸收 WhiskyKit，放进 `Packages/CiderKit`（SwiftPM）。WhiskyKit 是 GPL-3.0，App 因此采用 GPL-3.0。
- 工程用 XcodeGen 生成。
- `ciderctl` 通过 XPC 让 GUI 去派生进程，避免 responsible 进程变成 Terminal。
- 最低 macOS 14；AVX 相关功能和 GPTK 3 在 15+ 上开放；推荐 15+；CI 覆盖 26 和 27。

```
ciderctl bottle create --name steam --template win10_64 --engine cider-wine-11.20-r1-x86_64
ciderctl install com.valvesoftware.steam --bottle steam
ciderctl run --bottle steam --exe 'C:\Program Files (x86)\Steam\steam.exe'
ciderctl diag hoyo --bottle zzz      # 只读采集并分类
ciderctl probe --json
```

**D6 数据格式**
- recipe、profile、verdict 三类数据放在 `cider-compatdb`。
- 流程：用 YAML 编写 → CI 按 JSON Schema 校验 → 生成规范化 JSON → Ed25519（minisign）签名；另发 `timestamp.json`，7 天过期，用于防冻结。
- 主键是不透明的 umu-ID 字符串。
- 宿主侧不执行任何 shell。

profile 示例：

```yaml
schema: cider.profile/v1
id: umu-4162040
match: {exe: ZenlessZoneZero.exe, steam_appid: 4162040}
when: {engine: ">=cider-wine-11.20", cpu_backend: [rosetta-x86_64]}
actions:
  renderer: {d3d11: dxmt}
  sync: msync
  rosetta: {advertise_avx: false}
  processes: [{exe: HYP.exe, argv_remove: ["--use-gl=swiftshader"]}]
anticheat: {vendor: hoyoverse, policy: run-as-designed}
last_verified: {date: 2026-12-20, engine: cider-wine-11.20-r1, macos: "27.0", chip: M3, ram_gb: 16}
```

recipe 沿用 10 号报告的 `cider.recipe/v1`，每个下载都带 sha256、大小和多个镜像。bottle 元数据文件 `cider-bottle.json` 包含：`schema`、`cpu_backend`、`engine`、`components{dxmt,d3dmetal}`、`engineHistory[]`、`locale`。

**D7 打包、签名与进程模型（16 号报告的模型 A）**
- **Cider.app**：Developer ID 签名、hardened runtime、公证。entitlements 只有 audio-input 和 camera。GUI 用 `posix_spawn` 启动 wine，所以 TCC 和本地网络的授权都记在 Cider.app 名下。
- **Engine R**：放在 `~/Library/Application Support/Cider/Engines/<id>/`，ad-hoc 签名，不开 hardened runtime，完整性靠签名的 manifest 保证。CI 同时产出一个 Developer ID 签名的变体；如果 TCC 或本地网络的实测（T2/T3）失败，就改用它（对应 00 号报告的第 6 条裁定）。
- **Engine A**：`CiderEngineA.app`，Developer ID 签名、hardened runtime，内嵌 `embedded.provisionprofile`；entitlements 为 cross-architecture-support、custom-x18-abi-toggle、allow-jit；单独公证、单独下载。社区自己构建的版本没有 Engine A。
- **游戏 shim**：arm64 stub .app，ad-hoc 签名，bundle ID 唯一，App 类别设为游戏。
- **Rosetta 检查**：每次启动都检查，缺失时引导用户运行 `softwareupdate --install-rosetta`；同时向 Apple 申请把 Cider 加入 Rosetta 弃用通知的忽略清单。
- **更新**：Sparkle 2.10 负责 App 本身，使用独立的 EdDSA 密钥；引擎、组件和数据库各走自己的签名 manifest。

**D8 CI/QA**
- 公开仓库使用 GitHub Actions macos-26 arm64 runner，免费，已预装 Rosetta。工作流：
  - `engine-build`：ccache + attestation；
  - `deps-build`：用 arm64 编译器加 `-arch x86_64` 交叉编译 x86_64 依赖，不依赖 x86 Homebrew；
  - `app-release`；
  - `compatdb`；
  - `linkcheck`：每天检测下载链接和哈希漂移；
  - `hoyo-version`；
  - `conformance`：在 windows-latest 上跑 Wine conformance tests 做对照。
- 自托管的 16GB 测试机每晚串行跑以下任务：
  - LRS 核心 9 项：L01–L07、W01、C01；
  - 扩展项：W02–W04、Q01、E01；
  - 游戏冒烟、Metal HUD 帧时间统计、winetest JUnit。
- 发布门禁：LRS 出现退化即阻断发布；Steam 20 次冷启动不得触发看门狗。

**D9 中文与网络**
- 界面：String Catalog，zh-Hans 和 en 同步维护。
- 中文 bottle 模板：`zh_CN.UTF-8`（ACP 936）；字体映射 SimSun→Songti SC、YaHei→PingFang SC；子进程的 `LANG`/`LC_*` 全部由 Cider 生成。
- 下载层按内容寻址，地址形如 `cider-mirror://sha256/<h>`。源为 GitHub Releases 加国内对象存储 CDN，启动时测速选源。
- 镜像只放 Cider 自有构建和可再分发的开源组件；第三方安装器走原厂 URL，多源回退。
- 游戏数据直连官方 CDN。

## 3. macOS 28 闸门

| 闸门 | 时间 | 判定条件 | 不满足时 |
|---|---|---|---|
| G1 | 2027-01-31 | Apple 对 entitlement 有明确答复 | 向 DTS 升级，并联合 Highball、dappermint 一起申请；每周投 1 个窗口研究 VM 兜底；对外说明 macOS 28 上 Engine A 可能不可用，建议用户暂留 macOS 27 |
| G2 | 2027-03-15 | CX 27 的 LGPL 源码已公开 | 改为基于 dappermint `arm64` 分支自行移植；Engine A 的范围缩小到 x64/i386 控制台程序加 DX11 |
| G3 | 2027-06-30 | Engine A β 达到退出标准；WWDC27 的 GPTK 带 arm64 切片 | 没有 arm64 D3DMetal 时，DX12 只在 Engine R（macOS 27 及以下）上提供，同时加快 KosmicKrisp B1/B2 的上游贡献 |
| G4 | 28 beta 1–3 | `game-test-tool` 模式下 Engine R 能否运行 | 不能运行时，把 Engine R 在 28 上标为不可用，迁移提示提前到 27.x 就开始显示 |

## 4. R3：三款 HoYoverse 游戏

**成功的定义**：用户不遇到反作弊弹窗。做到这一点只有两种方式：一是经过验证后在本地“按设计运行”；二是在启动游戏 exe 之前就把用户路由到官方云。Cider 不修改、不伪装、不隐藏、不绕过反作弊。

1. **政策落地**（P0，1 个窗口）
   - 编写 `docs/policy/anticheat.md`。
   - `anticheat.policy` 只允许取 `run-as-designed`。
   - lint 拒绝以下内容：`SteamOS`/`SteamDeck` 环境变量或伪造的 `SteamAppId`；`LD_PRELOAD`/`DYLD_INSERT_LIBRARIES`；`network.block`；HideWineExports；写入游戏目录的 file.patch；GPU 身份伪装；wdfldr/KMDF 宿主。
   - `wine_get_version` 保持可见，并附上 Cider 构建号。
   - 代码不按反作弊模块名分支。
2. **预检**（P2，3 个窗口）
   - 流程：检测安装 → 判断区服和渠道 → 读取版本（本地文件，或只读查询 HYP API）→ 检查环境（Rosetta、引擎、DXMT、磁盘至少为游戏大小的 2 倍）→ 查裁定库 → 决定。
   - 裁定示例：

     ```json
     {"id":"mihoyo.zzz","edition":"global","channel":"steam","game_version":"3.2",
      "engine":"cider-wine-11.20-r1","cpu_backend":"rosetta-x86_64","macos":"27.0",
      "result":"unverified","evidence":null,"on_block":{"route":"official-cloud","form":"web"}}
     ```

   - 规则：
     - 裁定库里没有的版本一律按 `unverified` 处理，不启动游戏 exe；
     - 原神 6.5 及以后，如果检测到 HoYoKProtect，而当前引擎没有 KMDF，默认判为 `blocked-anticheat`；
     - 云路线的 `form` 只能取 `web|native-macos|unverified` 三个值，iOS 形态不在枚举里，写进去 CI 直接失败。
3. **取证**（P2，4 个窗口加人工；需要 16GB 测试机，每款游戏至少 75GB 磁盘）
   - E1：绝区零 Steam 版，Steam 用 bottle 内的 Windows 版，渲染用 DXMT，不做任何伪装；同时拿 HoYoPlay 版做对照。
   - E2：原神国际服和国服。
   - E3：星铁。
   - E4：官方云的 Windows 客户端在 Cider 中运行。
   - `ciderctl diag hoyo` 只采集 `+module,+ntoskrnl,+seh,+loaddll` 日志并分类；用公开日志样例回放，分类准确率须为 100%。
4. **正当修复**（每项 1 个窗口，先提交上游）
   - crypt32 签名属性：先用 11.18 原版复测，再在 windows-latest 上与 Windows 对照。
   - ndis：只诚实实现不涉及安全语义的导出。
   - HoYoPlay 和米哈游启动器的 CEF 呈现（C1）。
   - 国服 WebView 验证码。
5. **云兜底**（1 个窗口）
   - 国服的云·原神、云·星铁、云·绝区零都用网页端打开：优先用 Chrome/Edge 的 `--app=` 窗口，其次用默认浏览器。
   - 绝区零日本云只有在核实确有网页版或原生 macOS 客户端之后才展示。
   - GeForce NOW 和 Xbox Cloud 默认折叠，由用户主动开启。
   - 不注入脚本，不接触用户凭据。
6. **合作**（P3）
   - E1–E4 的数据齐全后，投递中英双语的 Compatibility Brief。
   - 内容：环境身份和红线承诺；事实矩阵；分级诉求，依次为：明确可识别的“不支持”返回 → 不因使用兼容层封号 → 把对 Proton 的放行策略扩展到可识别的 Cider 环境 → 提供技术联系人和版本前测试窗口 → 原生 Mac 版或 macOS 官方云。
   - 渠道：Help Center 工单、`genshin_cs@hoyoverse.com`、HoYoLAB、米哈游客服、Games Press 上的媒体联系人。
   - 同时通过 Apple 游戏开发者关系和 Valve 侧面沟通，并抄送 CodeWeavers，争取与 CrossOver 共用同一套放行策略。
7. **运维**
   - `hoyo-version` 发现版本 tag 变化后，把相关裁定置为 `unverified` 并自动开 issue，72 小时内在测试机上复核。
   - 2027-07 在 Engine A 上重跑 E1–E3；有结果之前，macOS 28 上默认路由到云。
   - **验收**：进程审计中，非 playable 组合拉起游戏 exe 的次数为 0；路线卡在 3 秒内出现。

## 5. 资源计划

- **分工**：开发者本人只做这几件事：保管签名密钥，与 Apple、HoYo 沟通，做发布决策，实机验证。Claude Code 负责补丁移植（每个主题开一个会话）、schema、测试、日志分类和文档。
- **窗口预算**：P0 8、P1 14、P2 18、P3 20、P4 24、P5 14、P6 10，合计约 108 个窗口，另留 30% 余量。每项任务控制在 1 个窗口以内，并且有 CI 能自动判定的完成标准。
- **8GB 开发机**：做 Swift 和 CiderKit 开发、单个 DLL 的增量构建（`make dlls/x/x86_64-windows/x.dll`）、调试、probe、Engine A-lite 的小原型。ccache 上限 20G，并行度用 `-j6`。
- **交给 CI**：Wine 冷构建、依赖前缀、LLVM、DXMT、MoltenVK、GStreamer、KosmicKrisp、签名和公证、兼容库校验、链接检查。
- **16GB 测试机**：必须在 **2026-11 前**到位，因为 P1 的 Steam 测试和 P2 的 HoYo 取证都要用。建议 Mac mini M4 16/24GB 加 1TB 外置 SSD，注册为 self-hosted runner 跑夜间任务。另分一个 macOS 27 卷，2027-06 起在上面装 28 beta。
- **固定支出**：ADP $99/年，这是 entitlement 申请和公证的前提。

## 6. 风险登记

| 风险 | 触发条件 | 应对 |
|---|---|---|
| entitlement 不批 | G1 未满足 | 见 §3；Engine R 持续维护到 27 生命周期结束 |
| CX 27 源码推迟 | G2 未满足 | 自行移植，缩小 Engine A 范围 |
| 28 上 Rosetta 子集不覆盖 Wine | G4 测试失败 | 迁移提示提前，DX12 游戏走云或留在 macOS 27 |
| 没有 arm64 版 D3DMetal | G3 时仍然没有 | 走开源 DX12 路径，UI 明示 |
| msync 导致 CEF 卡死 | 20 次冷启动中看门狗触发 >0 次 | Steam UI 默认 `sync=server` |
| 启动器或 Steam 自更新打坏兼容 | 夜间 LRS 变红 | 72 小时内通过 profile 热修 |
| HoYo 服务端不放行 macOS | E1 被服务端拒绝 | 默认走云，加快合作沟通 |
| 升级 macOS 27 后 Rosetta 被移除、弃用通知 | 启动时检测到 | 引导重装；申请加入忽略清单 |
| 单人或配额不足 | 任一阶段延误超过 2 周 | 按“不做清单”砍范围，P3 顺延，P4 不顺延 |
| 上游依赖单一维护者（Gcenx、3Shain、dappermint） | 仓库 30 天无响应 | fork 并自建镜像 |
| 被认为“寄生” CrossOver（Whisky 的先例） | 上游 MR 每月少于 2 个 | 把上游贡献列入每个阶段的退出标准 |

## 7. 不做清单

- 32 位 bottle、Intel Mac、macOS 14 以下。
- 任何 iOS/iPadOS 客户端路线，包括 Mac App Store 上的 iPhone/iPad App 和任何 iOS 应用运行器。
- 任何改动、伪装、隐藏或绕过反作弊的做法：KMDF 宿主、HideWineExports、SteamOS/Deck 伪装、启动时断网、改文件、GPU 身份伪装、改游戏配置解锁帧率。
- 为 HoYo 游戏自己写下载器；Cider 内置 Chromium 做云游戏窗口。
- 把 D3DMetal 放进仓库或主 bundle。
- 自研 D3D9 层（用 mtld3d）；2027 年内自研 D3D12→Metal 层；维护 DXVK-macOS。
- 自研原生 macOS Steam 桥（只提供 runner 接口）。
- 依赖 CX 的私有组件（cxcompatdb、alt loader）；移植 %gs 字节补丁。
- M365/Office 2016+ 的保证支持；WeGame/ACE、网银 U 盾、税控盘。
- 商业化，以及任何法律或许可证分析章节。
