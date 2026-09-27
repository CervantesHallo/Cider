# Cider 总体战略与关键决策

> 2026-09-27 · 首席架构定稿（同日修订：R3 按用户决定改为国服 + K 线；新增 02 引擎计划） · 依据 docs/research 01–22；冲突以 00-research-review 裁定和各报告“事实核查记录”为准。“窗”= 一个 5 小时 AI 配额窗口。不含法律与商业分析。

## 愿景与成功标准

Cider：开源的 Apple Silicon Windows 兼容层，与 CrossOver 基本对齐；差异化在数据热修、回退可见、中文优先、米哈游“预检 + 官方路线”。

| 维度 | 1.0（Engine R，目标 2027-05-31） | 2027-12-31 |
|---|---|---|
| R1 对齐 | `docs/parity.md`（由 CX 26 功能清单拆出约 50 项，分 P0/P1）完成 ≥80%，P0 全部完成 | ≥90% |
| 游戏 | Top-50 profile 30 天内验证过；DXMT 冒烟通过率 ≥ oracle 的 90% | Top-100 |
| 启动器 | LRS：Steam、Epic、EA、Battle.net、米哈游启动器，连续 14 晚全绿 | 加 GOG、Ubisoft、Rockstar |
| 质量 | msync 下 Steam UI 冷启动 20 次零看门狗；崩溃率 <2%/会话；安装到 Steam 登录 ≤10 分钟 | 启动器热修中位 ≤24 小时 |
| R3 | 非 playable 组合拉起 exe 0 次（进程审计）；路线卡 ≤3 秒；K 线 conformance 测试在真实 Windows 上全绿并已提交上游 | 反作弊弹窗到达用户 0 次；72 小时复核 ≥90% |
| R4 | 国内无代理下载引擎 ≤3 分钟；zh-Hans 覆盖 100% | IME 测试集全绿 |

R3 范围（用户 2026-09-27 定）：只做**米哈游启动器安装的国服**原神、星穹铁道、绝区零；不做 Steam 版绝区零、HoYoPlay/Epic 国际服，不与 HoYoverse 合作或外联。唯一的本地路线是 **K 线“忠实内核兼容”**，五项全部必做、不因容量砍掉：① 基于微软开源 Windows-Driver-Frameworks 的 KMDF 运行时；② 语义忠实的 ntoskrnl 安全 API；③ 诚实的失败码（绝不伪造成功）；④ 在真实 Windows 上验证的 conformance 测试 + 上游提交；⑤ Wine 保真修复（crypt32、ndis 导出、启动器 CEF、验证码/实名 WebView）。本地能否可玩取决于反作弊与服务器是否接受这种忠实实现，结果不确定，这一点如实写进 README 与路线卡；通过前由预检阻断启动（不弹反作弊窗），并提供国服官方网页云。

## 需求与约束

- **R1** 与 CrossOver Mac 近乎对齐，质量极高。**R2** 开源；许可证只在迫使技术设计时写一行。
- **R3** 米哈游启动器国服 PC 客户端在 Cider 中运行，反作弊完整保留（K 线，见上）；兜底只用国服官方云；不合作、不外联。**用户底线：永不使用官方 iOS/iPadOS 客户端**（含 Mac App Store 的 iPhone/iPad App、PlayCover 及任何 iOS 运行器）。不篡改、伪装、隐藏或绕过反作弊。
- **R4** zh-Hans 优先 + 英文；国内镜像。
- **资源**：1 人 + Claude Code；M3/8 GB/macOS 26.5，仅 CLT；GitHub Actions；5 小时与每周配额；推荐（不强制）16 GB+ 测试 Mac。
- **时间锚点**：macOS 27（2026-09-14）是通用 Rosetta 最后一版，升级后不自动恢复 Rosetta；Wine 12.0 约 2027-01 中下旬（推断）；CX 27 于 2027 年初发布（仅 Apple Silicon、无 32 位 bottle），源码推断 2–3 月公开；WWDC27 约 6 月；macOS 28 约 2027-09/10，Rosetta 只剩老游戏子集，而且是另一套机制（`game-test-tool` 开启后禁用 Rosetta，非游戏进程可能崩溃）。

## 指导原则

1. 会变的放进数据：启动器、游戏版本、WebView2 问题用签名 profile 热修。
2. 上游优先：补丁有预算、有上游状态、按月清账。
3. 决定性实验先行：T15、T4、entitlement 申请在前 8 周启动；K 线从 conformance 框架起步。
4. R 线过滤：无法带到 Engine A 的 R 专属工作 ≤ R 线的 20%。
5. 产物从第一天按架构参数化（`arch`、`cpu_backend`、DXMT arm64x 进 CI）。
6. 回退可见：后端降级必须在 UI 和日志写明原因。
7. 红线进 schema 和 CI：iOS 形态和反作弊规避无法合并。
8. 一窗一卡：由 CI 或实验室判定完成，从 handoff 续做。

## 关键架构决策（ADR）

**ADR-001 平台与最低系统**
- 决策：只支持 Apple Silicon；最低 macOS 14 + 功能门控（os_sync 14.4+，更低回退 ulock；AVX/GPTK 3 需 15+；KosmicKrisp 26+；Engine A 暂定 26.5+，以 probe 为准）。推荐 15+，CI 覆盖 26/27。只建 64 位（新 WoW64）bottle。
- 理由：00 号裁定 7。被否决：硬下限 15；Intel；32 位 bottle。
- 复议：14.x 用户占比 <3%，或关键组件抬高下限。

**ADR-002 Engine R 是 2027 年主力**
- 决策：x86_64 Wine + 新 WoW64（`--enable-archs=i386,x86_64`）经 Rosetta 运行；每次启动检测 Rosetta，缺失则引导安装。在 ≤27 上服务到 2028 年，DX12 与 HoYo 期间都依赖它。
- 理由：D3DMetal 只有 x86_64；FEX 下反作弊未知；27 用户长期存在。
- 被否决：草案 B 的“R 只是短桥、1.0 等到 28”。
- 复议：T15 表明 Wine 能在 28 的 legacy 机制下运行；或 Engine A 在 Top-50 上的通过率达到 R 的 90%。

**ADR-003 Engine A、entitlement 策略与 Engine V 兜底**
- 决策：Engine A = arm64 Wine + ARM64EC/WoW64 + 上游 FEX PE 模块（`libarm64ecfex.dll`/`libwow64fex.dll`）；macOS 侧只改 FEXUnixLib 的 7 个 unixcall（逐个实现或做桩，配单测）。
  - 产品化前提是拿到 `com.apple.developer.cross-architecture-support`（否则 0x7ffe0000 映射不了，32 位为 0）。第 1 周提交 Capability Request + DTS，邀 Highball、dappermint 联名。
  - CX 27 源码公开前不自研 Wine 侧 arm64-macOS 部分，只做 probe、FEXUnixLib 和 ≤4 窗 A-lite spike；A-lite 退出标准是“工具链、x18、信号路径可用”，**不是**“原生 ARM64 PE 可运行”。
  - 授权语义在测试 Mac 卷 2（A3：关 SIP + `amfi_get_out_of_my_way=1`）上验证，该卷永不做米哈游测试。
  - Engine V（VZ）2027 年只出文档（保留 macOS 27 卷；应用在 27 客户机中运行），不做产品。
- 理由：15 号报告估 9–15 人月（资深 Wine + JIT），CX 27 源码可省 40–60%；一人在 2027 年只能做到 x64 D3D11 游戏的 opt-in 预览。
- 被否决：第 1 周起以 A 为主；VM 跑反作弊游戏；自修 KosmicKrisp B1。
- 复议：G-ENT、G-CX27、G-T15 任一出结果。

**ADR-004 Wine 基线、补丁队列、rebase 节奏与工具链**
- 基线：`cider/devel` = wine-11.18 + 按主题拆分的 CX 26.3 补丁；每个 devel tag rebase（11.19 约 10-02），每两个 tag 发一次引擎，12.0-rc 期间每周 rebase，12.0 为 1.0 基线。CX 26.3 原样构建只作 oracle 与可选回退；不做 11.0-stable。
- 主题：`d3dmetal-abi`（22434/22435/23015；24067 先 `nm -u` 验证，丢弃 cxcompatdb 块）、`rosetta`（含 M3 需要的 24265）、`msync`、`winemac`（C1–C3、C5）、`gl-remap`、`nx`（athei 539aa62）、`cjk`、`macos-ux`。不移植 %gs 字节补丁（上游已有 GSBASE swap）、alt loader、20810。
- 治理：尾注 `Cider-Topic`/`Upstream`/`Cider-Test`/`Cider-Review-By` 由 CI 强制；rebase 出 `patch-report`；1.0 时活跃补丁 ≤80（cider-only ≤25）；每月 ≥2 个上游 MR。
- 工具链：x86_64/i386 PE 用 mingw-w64 gcc（llvm-mingw 编的 kernelbase 卡 Steam 登录，17 号报告）；llvm-mingw 只用于 arm64ec/aarch64 和一个仅作对照的构建。依赖用 arm64 编译器 `-arch x86_64` 交叉编译，不用 x86 Homebrew。
- 被否决：CX 源码包作主干；x86 PE 用 llvm-mingw。
- 复议：连续两次 rebase 中位耗时 >1.5 窗，则改为隔一个 tag rebase，或冻结 12.0 只做 cherry-pick。

**ADR-005 同步**
- 决策：msync 作为 Wine 11 inproc_sync 的 macOS 后端。M0–M1：CX 26.3 实现 + dappermint 8 个修复 + `WINEMSYNC_STATS`；M2：os_sync SHARED；M3（服务器可等待 msync 对象）作 CEF 门禁的后备。Steam UI 用 `sync=server` 会让同 bottle 的 Steam 游戏失去 msync，故“Steam UI 在 msync 下 20 次冷启动零看门狗”是 1.0 硬门禁；通过前 CEF 会话用 `sync=server`，切换时重启 wineserver。
- 被否决：不扩展服务器就在同一 wineserver 内混用同步模式。
- 复议：2027-03-31 门禁仍未通过，则提前做 M3。

**ADR-006 图形（按 API）**

| API | 默认 | 白名单备选 | 不做 |
|---|---|---|---|
| D3D10/11 | DXMT builtin（arm64x 同步进 CI） | D3DMetal；wined3d-vk | DXVK-macOS |
| D3D12 | Engine R 用 D3DMetal：macOS 26 默认 3.0、4.0b2 按游戏开启；macOS 27 用 `D3DM_MTL4=0` | vkd3d（仅 FL11_0 游戏） | 自研转译；发布 vkd3d-proton |
| D3D8/9/DDraw | wined3d-GL + remap（CI 断言影子缓冲计数 = 0） | mtld3d；cnc-ddraw；x87sidecar（1.0 后实验） | 自研 d9mt |
| OpenGL | Apple GL 4.1 | Zink 只跟踪 | — |
| Vulkan | MoltenVK，锁定在带 shadow-import 的 1.4.2 之前 | KosmicKrisp（26+） | fork MoltenVK |

- 32 位 GPU 身份与 NVAPI stub 按游戏配置；`anticheat.vendor=hoyoverse` 的条目禁用。
- D3DMetal 不进仓库和主 bundle；默认用户从 GPTK 导入，另有原样 framework 的可选下载源（许可允许非商业、未修改的单独分发，19 号核查更正）。只校验、原样加载，不修改；记录 `lipo -archs`、`codesign -v` 与版本；宿主层按 GFXT 语义实现、与架构无关。
- 复议：GPTK 出现 arm64 切片；DXMT d3d12 去掉 “DO NOT USE”。

**ADR-007 进程模型、签名身份与引擎打包**
- 模型 A：GUI 用 `posix_spawn` 派生 wine，responsible 为 Cider.app；派生者抽象成接口，T13 验证后才考虑改由 `cider-agent` 派生。`ciderctl` 经 XPC 下发请求，不自行派生。
- Cider.app：Developer ID + hardened runtime + 公证，只带 audio-input、camera。Engine R：`~/Library/Application Support/Cider/Engines/<id>`，ad-hoc 签名 + Ed25519 manifest；CI 另产 Developer ID 变体，T2/T3（TCC/本地网络）失败时启用。Engine A：`CiderEngineA.app`（Developer ID + `embedded.provisionprofile`），单独公证与下载。
- 游戏 shim：arm64 stub，唯一 bundle ID，游戏类别，**不声明文档或 URL 类型**；`steam://` 等协议由专门的 handler shim 转发。
- 经 Feedback 申请加入 Rosetta 通知忽略清单（CORAL），约 0.2 窗。
- 被否决：Wine 放进 app bundle；默认 exec 型 shim。复议：T4 证明只有 exec 能进 Game Mode，则加可选 CiderGameHost。

**ADR-008 App 架构**
- 决策：`cider` 仓库含 `App/`（SwiftUI + XcodeGen）、`Packages/CiderKit`（SwiftPM，CLT 可测，承载全部业务逻辑）、`Tools/ciderctl`、`Tools/cider-probe`（C，运行时选后端）。按模块吸收 frankea/Whisky 的 WhiskyKit。
- `cider-bottle.json`：`cpu_backend`、`engine{id,pin}`、`components`、`engineHistory[]`、`locale`、`graphics.d3d12`（如 `"route:R"`）。换引擎前做 APFS 快照；Denuvo 游戏先警告并冻结 `engine`/`cpu_topology`/`avx_advertise`。
- 被否决：Web UI；CLI 直接派生 wine。

**ADR-009 数据格式与签名通道**
- `cider-data`（CC0，Highball-db 超集，主键 umu-ID）：Recipe 管安装（YAML→JSON，下载带 sha256/大小/多镜像，禁止 shell）；Profile 管运行（`when` 条件 + 类型化幂等动作）；Verdict 记录可玩性与证据。
- 引擎内 `cider-rules` 由 profile 生成（argv 增删改、AppDefaults、DLL override、writecopy），取代 CX 的 exe 名 hack 和 libcef 字节补丁。
- 发布：Schema 校验 → 红线 lint → minisign（密钥在需人工批准的 environment）→ Pages 与国内镜像；`timestamp.json` 7 天过期，`revision` 单调递增，支持 `yanked`；引擎索引另用 Ed25519 密钥，与 Sparkle 分开。
- 被否决：profile 内写脚本；与 Highball 不兼容的新格式。

**ADR-010 R3 HoYoverse 路线架构**
- 预检：识别安装（米哈游启动器国服）→ 版本（本地或只读查启动器接口）→ 环境检查 → 查 Verdict → `playable` / `unverified` / `blocked-anticheat` / `broken-launcher`（只开启动器做更新修复）。
- Verdict 用区间语义，键为 游戏 × 区服 × 渠道 × 版本 × 引擎主版本 × macOS 主版本 × `cpu_backend`。凡会加载 `HoYoKProtect.sys` 而 Cider 的 KMDF 运行时尚未通过 conformance 的版本一律 `blocked`（覆盖原神 6.5 起所有版本）。**不提供任何绕过预检的开关**，测试员也不例外；本地验证只在维护者实验室做。
- 恢复闭环：只读观察退出码、`driverError.log`/`initDriver Failed`、已知错误窗口；命中即弹路线卡、本机裁定降为 `unverified`、出脱敏报告。不注入、不 hook。
- 路线：`form ∈ {web, windows-cloud-client, native-macos}`，无任何 iOS 形态，写入即 CI 失败。只用国服官方云：云·原神、云·星穹铁道、云·绝区零的网页版（Chrome/Edge `--app` 或系统浏览器）。
- lint 拒绝：SteamOS/Deck 伪装、伪造 `SteamAppId`、`DYLD_INSERT_LIBRARIES`、断网、`HideWineExports`、GPU 身份伪装、写游戏目录、伪造成功的内核桩、按反作弊特判的代码路径、改写进程 argv；`wine_get_version` 可见并带构建号。KMDF 运行时本身不在拒绝之列，但必须语义忠实，并由 conformance 测试（Windows 对照）守门。
- 被否决：任何 iOS/iPadOS 形态；Steam 版绝区零与国际服路线；与 HoYoverse 合作；GeForce NOW/Xbox；测试员绕过开关。
- 复议：K 线某项在 Windows 对照测试中证明无法忠实实现（如实公开，不改走伪装）。

**ADR-011 CI / QA / 设备实验室**
- 公开仓库用 macos-26 arm64 标准 runner（3 核/7 GB/14 GB）：`engine-build`（拆 job + ccache，冷构建 ≤90 分钟）、`oracle-cx263`、`deps-build`、`app-release`、`data-publish`、`watchers`（上游发布、Steam buildid、WebView2、HYP tag）、`conformance`（windows-latest 对照）。
- 冒烟门禁：`wineboot --init`、`cmd /c ver`、`syswow64\cmd /c ver`；`DYLD_PRINT_LIBRARIES` 中无 `/usr/local`、`/opt/homebrew`；`otool -l` 显示 `__PAGEZERO=0x1000`。
- `cider-lab`：私有仓库 + 自托管 runner，只拉已签名产物，不跑外部 PR；夜跑 LRS（5 秒内 rAF ≥50、截图非纯色、GPU 进程 ≤1 次创建）、游戏冒烟（首帧、SSIM、p95 帧间隔）、winetest、HoYo 日志回放。GPU 测试只在实体机。

**ADR-012 中文优先与国内镜像**
- zh-Hans String Catalog + 英文。中文 bottle：`LC_ALL=zh_CN.UTF-8`（ACP 936），SimSun→Songti SC、YaHei→PingFang SC；引擎不链接 fontconfig；IME 测试集（拼音、搜狗、微信）；国服验证码与 B 服登录窗纳入 LRS。
- 镜像：引擎、组件、数据、appcast 同步到国内对象存储 + CDN，按 sha256 寻址、测速选源；镜像只是不可信传输，以签名清单为准。自定义域名需先备案，v1 用服务商默认域名。游戏本体走官方 CDN。
- 应用内反馈：生成脱敏诊断包，可附到中文社区或邮件，不要求 GitHub 账号。

## 阶段与里程碑

容量按每周 6 窗规划，其中 20% 留给运维；第一个月实测后重算。两次大 rebase 单列为容量项。

| 阶段 | 时间 / 窗 | 目标与交付 | 退出标准（可自动判定） |
|---|---|---|---|
| P0 地基与决定性实验 | 2026-10-01→11-15 / ~39 | 第 1 周：ADP、entitlement、测试 Mac 决策、装 Xcode 26.6/Metal toolchain/Rosetta。CI 出 Engine R v0 与 oracle（见 02）；probe；CiderKit/ciderctl 骨架；R3 H0（政策、lint、schema、预检、路线卡、国服云、米哈游启动器 watcher、diag 分类）。实验：T15（27.x beta）、T4、T2/T3、T7 | 冒烟门禁全绿；lint 拒绝 20 个违规样例；日志回放分类 100%；伪安装预检 50 次拉起 exe 0 次；entitlement 工单号存档 |
| P1 Alpha → 0.1 | 11-16→2027-01-15 / ~54 | 补丁队列首批主题；DXMT；D3DMetal 导入器；GStreamer 1.28；sdl2-compat；WebView2 固定通道；Steam in bottle；LRS 核心；GUI MVP；**0.1 公开预览，含 R3 最小垂直切片**；K 线 ① KMDF 运行时骨架与 conformance 框架 | Steam 登录、商店页渲染；GL 冒烟影子缓冲计数 = 0；LRS L01–L04 通过；0.1 DMG 已公证 |
| P2 1.0（Wine 12.0） | 01-16→05-31 / ~110 | 12.0 rebase（8 窗）；CX 27 diff-of-diffs（10 窗）；msync M2/M3；parity；Top-50 profile；Sparkle；镜像；恢复闭环；K 线 ②–⑤ | 同“成功标准”1.0 列 |
| P3 Engine A α + WWDC27 | 06-01→08-31 / ~80 | 按闸门投入 Engine A（≤本阶段 50%）：CX 27 arm64 胶水、DXMT arm64x、CiderEngineA.app 签名、R↔A 迁移；28 beta 首周复跑 probe/T15；1.x 维护 | x64/i386 控制台测试与 DXMT D3D11 样例通过；10 个 bottle R→A→R 往返无损；28 beta 报告公开 |
| P4 macOS 28 首日 + 稳态 | 09-01→12-31 / ~100 | 28 首日兼容版（预检与迁移提示；已授权则附 Engine A opt-in 预览）；第二维护者；上游化 | 28 发布后 72 小时内出兼容版；28 上打开 R bottle 必有说明、无静默失败；活跃补丁比 1.0 时少 ≥20% |

可演示里程碑：**M-A**（10-31）M3 上跑 notepad、DXMT 样例和预检路线卡；**M-B**（01-15）0.1 登录 Steam、跑一款免费 D3D11 游戏、一键进国服云；**M-C**（05-31）1.0 安装到 Steam 游戏 ≤10 分钟；**M-D**（08-31）Engine A 跑一款 x64 D3D11 游戏；**M-E** 28 首日兼容版。

## 决策检查点（go/no-go）

| 闸门 | 日期 | 证据 | 通过 | 不通过（降级） |
|---|---|---|---|---|
| G-T15 | 2026-12-15（最迟 28 beta 1） | legacy 模式下 wineserver、services、explorer 能否存活 | R 在 28 上有望延续 | 27.x 起显示迁移提示；上调 Engine A 投入 |
| G-ENT | 2027-01-31 | Apple 书面答复 | Engine A 按产品规划 | 升级 DTS；A 只做 A-lite 与研究；公开 28 限制，出 Engine V 文档 |
| G-CX27 | 2027-03-31 | CX 27 LGPL 源码公开 | diff-of-diffs 导入 arm64/FEX 胶水 | Engine A 缩到 x64 控制台 + DX11，2027 年不承诺 α |
| G-1.0 | 2027-05-15 | 1.0 门禁数据 | 5-31 发布 | 最多顺延 4 周，砍 P1 parity 项 |
| G-WWDC | 2027-06-20 | GPTK 是否有 arm64 切片；28 beta 的 probe/T15 | 编写 arm64 libd3dshared 胶水 | DX12 路由到 ≤27 的 R，UI 标注 |
| G-28 | 2027-08-15 | Engine A 冒烟；T15；E5（HoYo 在 A 上，仅卷 1、已授权） | 28 上可选 Engine A | 首日版只含预检与说明；HoYo 在 28 默认走官方云 |
| G-K | 每个 K 线项完成时 | Windows 对照 conformance + 国服启动器实测 | 对应版本 Verdict 改为 `unverified` 并开放本地测试 | 维持默认阻断 + 国服官方云，每月复测；**绝不转向伪装** |

## 资源与节奏

- **窗口卡**：`docs/tasks/<id>.md`，含 goal、inputs、accept（可执行命令）、rollback、handoff，一卡不跨窗；每个仓库一份 `CLAUDE.md`（构建命令、红线、验收脚本）。
- **配额排序**：rebase 冲突、补丁移植等代理密集工作放在 5 小时周期与周配额前段，文档与数据放后段。
- **人的工时单独记账**（每周 8–10 小时）：审 range-diff 与代理 PR 约 4 小时；实机验证约 3 小时；HoYo 版本复核（每年约 50 次，每次约 1 小时）；对外沟通。
- **分工**：本机只做 Swift、单 DLL 增量构建、probe、日志分析；冷构建、签名公证、数据发布交给 CI。
- **硬件**：推荐 2026-12 前购入 Mac mini（M4+，16 GB/512 GB）+ 2 TB 外置 SSD，分三卷：卷 1 完整安全（HoYo、发布验证、runner）；卷 2 A3（只跑 Engine A）；卷 3 beta。无测试 Mac 时：K 线实测在开发机 + 外置盘上做；实验室改为开发机夜间串行；A3 推迟或试 VM；HoYo SLA 放宽到 7 天。
- **月报**：活跃补丁数、rebase 中位耗时、运维占比（>40% 触发砍项）、flaky 比例、上游 MR 数、HoYo SLA。

## 十大风险

| # | 风险（触发） | 应对 |
|---|---|---|
| 1 | entitlement 不获批（G-ENT） | A-lite + 研究；公开 28 上的限制；R 在 ≤27 上服务到 2028 |
| 2 | 28 的 legacy 机制不接纳 Wine（G-T15） | 提前迁移提示；DX12 和 HoYo 走 ≤27 或官方云 |
| 3 | 反作弊或服务器不接受忠实实现（K 线实测被拒） | 默认阻断 + 国服官方云；如实公开结果；绝不伪装 |
| 4 | msync 与 CEF 冲突（出现看门狗） | CEF 用 `sync=server`；提前做 M3 |
| 5 | winemac C1 不稳（2027-02 LRS 仍红） | 对照 Highball 0007 与 dappermint 二分；按启动器开 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN` |
| 6 | 配额或人力不足（运维占比连续两周 >40%） | 砍 P1 parity 项；招募第二维护者 |
| 7 | rebase 失控（中位 >1.5 窗） | 隔 tag rebase；冻结 12.0 只 cherry-pick |
| 8 | 无 arm64 D3DMetal（G-WWDC） | DX12 路由到 R；跟踪开源路径 |
| 9 | 启动器/Steam 自更新打坏兼容（LRS 变红） | 72 小时内 profile 热修 |
| 10 | 签名密钥泄露或单点 | 离线根密钥与发布密钥分离；timestamp 过期；第二人持钥 |

## 不做清单

1. 官方 iOS/iPadOS 客户端的任何形式：Mac App Store 的 iPhone/iPad App、PlayCover、任何 iOS 运行器，以及云兜底中的 iPad App 形态。
2. 任何反作弊篡改、伪装、隐藏、绕过；伪造成功的内核桩；断网启动；改游戏文件；绕过预检的开关。
3. GeForce NOW、Xbox Cloud 等第三方云入口（用户确认前）。
4. 自研米哈游下载器；内置 Chromium；与 HoYoverse 合作或外联；Steam 版绝区零与国际服路线。
5. 32 位 bottle、Intel Mac、11.0-stable 线、x86 Homebrew。
6. 2027 年内：Engine V 产品化；自研 D3D12/D3D9 转译；发布 vkd3d-proton；自修 KosmicKrisp B1（B2 只在有余力时贡献）；原生 Steam 桥。
7. DXVK-macOS、fork MoltenVK、%gs 补丁、cxcompatdb、alt loader；默认遥测；profile 脚本。
8. WeGame/ACE、网银 U 盾、税控盘、M365 保证支持。
9. 法律、许可与商业模式分析。

## 详细计划文档索引

- `01-architecture.md`：组件图、仓库布局、进程/IPC、接口
- `02-engine-r-wine.md`：自建引擎（v0 cider-cx26 → v1 11.18）、补丁选择、工具链、性能工作项 P-1…P-6（含 2026-09-27 卡顿问题）
- `03-engine-a-arm64.md`：probe、FEXUnixLib、A-lite/A3/A1、CiderEngineA.app、R↔A 迁移
- `04-graphics.md`：DXMT、D3DMetal 导入器、remap、mtld3d、Vulkan、后端矩阵
- `05-app-cli-ux.md`：SwiftUI、ciderctl、CiderKit、bottle 管理、parity 功能、shim
- `06-profiles-recipes-compatdb.md`：三类数据 schema、cider-rules、签名通道、数据导入
- `07-platform-integration.md`：签名公证、TCC/本地网络、Rosetta、CJK/IME、镜像
- `08-games-launchers-anticheat.md`：LRS、CEF/WebView2、启动器 profile、反作弊 lint
- `09-hoyoverse-games.md`：预检、Verdict、恢复闭环、K 线（KMDF 运行时、安全 API、conformance）、国服官方云
- `10-apps-runtimes.md`：运行库、Installer Assistant、应用兼容边界
- `11-qa-perf-ci-release.md`：CI、cider-lab、基准、发布门禁、月报
- `12-dev-environment.md`：开发机、测试 Mac 三卷、CLAUDE.md、窗口卡
- `13-roadmap.md`：逐周窗口卡排期、容量校准、闸门日历

## 评审记录

| 草案 | 技术/排期/价值/风险 | 采纳 |
|---|---|---|
| C 可持续（第 1） | 7–8/6–7/7–8/8 | 骨架：窗口卡、运维节奏、补丁治理、cider-rules、预检四态、E1-oracle |
| A 对齐（第 2） | 6–7/3/7/6 | 工具链拆分、交叉编译依赖、D3DMetal ABI、Developer ID 变体、架构参数化、parity.md、闸门表 |
| B 未来（第 3） | 6/3–4/4/6–7 | R 线 20% 规则、probe 产品化、三卷隔离、FEXUnixLib 单测、双形态诉求、迁移快照 |

修正评审指出的问题：容量重算；x86 PE 用 gcc；T15/T4/E1 前移；Engine A 重估；删 GFN/Xbox 与绕过开关；补恢复闭环、E4、老游戏 P0 项；测试 Mac 改推荐。

