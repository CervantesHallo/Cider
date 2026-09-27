# Cider 战略草案 B：未来优先（Future First）

> 2026-09-27 · 依据 docs/research/00–22（遵循 00 号裁定与各报告的“事实核查记录”）。“窗口”指一个 5 小时 AI 配额窗口。

## 1. 论点

- **Rosetta 的期限**：通用 Rosetta 只到 macOS 27。macOS 28 的“老游戏子集”是另一套机制：用 `game-test-tool` 开启后 Rosetta 被禁用，非游戏进程可能崩溃。
- **闭源组件的期限**：D3DMetal 只有 x86_64 版本；CrossOver 27 已经转向 arm64 + FEX。
- **推论**：只在 Rosetta 上成立的投入，保质期不超过 12 个月。

因此，Cider 从第一周起就把 **Engine A** 当主引擎来建：arm64 Wine + ARM64EC/WoW64 + FEX Darwin 宿主。**Engine R**（x86_64 Wine + Rosetta）只是一座短桥，用来尽早把用户接进来。

R 上只投“能带到 A 上”的东西（msync、winemac、CEF、IME/字体、配方与预检、DXMT）；R 专属工作不超过 R 线的 20%。

图形只押能编译成 arm64 的开源栈：
- D3D10/11：DXMT（arm64x）；
- D3D9：wined3d / mtld3d；
- DX12：KosmicKrisp + vkd3d-proton。它有两个硬阻塞，B2（single-texel）和 B1（XFB），由 Cider 亲自补。

**生死点**：`com.apple.developer.cross-architecture-support`。没有它，A 映射不了低 4GB 和 0x7ffe0000，32 位程序完全不能运行（15 号报告）。所以必须**立即申请**；拿不到就启用有硬日期的 **Engine V** 兜底。

**目标**：macOS 28 发布那一周，A 成为新建 bottle 的默认引擎，同时发布 Cider 1.0。

## 2. 硬决策日

| 日期 | 事件 / 门 | 动作 |
|---|---|---|
| 2026-09-30 | XDC 大会 KosmicKrisp 演讲 | 更新 B1–B12；与 LunarG 确认 GS/XFB 归属 |
| 2026-10-05 | ADP 入会 | Capability Request、DTS、Feedback 三路申请；邀 Highball、dappermint 联名 |
| 2026-12-15 | **G1** | Apple 无回复：升级 DTS，V-mac 原型提前 |
| 2027-01 中下旬 | Wine 12.0 | R/A 共用基线 |
| 2027-01-31 | **G2** | 无授权：V 转正式开发；GS/XFB 无人认领：Cider 做 B1 |
| 2027-03-31 | CX 27 源码 | 未公开：A 的 Wine 侧自研，工期 +6 周 |
| 2027-04-30 | **G3** | A1（已授权）/ A2（只跑原生 ARM64 PE）/ V 三选一 |
| 2027-06 | WWDC27 | 查 GPTK 5 有无 arm64；28 beta 首周回归 |
| 2027-07-15 | **G4** | A 是否作为 28 默认；R 冻结方案 |
| 2027-09/10 | macOS 28 | 发布 1.0；R 只在 ≤27 提供 |

## 3. 阶段

### P0 地基与探针（2026-10-01 → 11-15）

**交付**
- `cider-probe`（C，只依赖 CLT，输出 JSON）：4K spawn（公开 API 与私有 0x1000）、soft pagezero（0x10000 与 0x7ffe0000 FIXED 分配）、trap -108（区分 4/5/0）、x18 保留、MAP_JIT 区数、`FEAT_*`。它也是产品组件，运行时据此选后端。
- 授权申请包。
- 仓库：`cider`、`cider-wine`（上游 11.18 + 主题补丁队列）、`cider-engines`、`cider-db`（CC0）。
- CI 构建：R 引擎（x86_64，llvm-mingw）、CX 26.3 oracle（tarball 自行存档）、DXMT arm64x、Mesa kk。
- `docs/policy/anticheat.md` 和配方 lint。

**退出**
- probe 在 26.5 和 27 上的结果已入库；
- 授权申请已有 case 号；
- R 引擎通过 `wineboot --init`、`cmd /c ver`、`syswow64\cmd /c ver`，`DYLD_PRINT_LIBRARIES` 中无 `/usr/local`、`/opt/homebrew`；
- lint 能拒绝全部 10 个违规样例。

### P1 R 桥 + A-lite（11-15 → 2027-01-31）

**交付**
- **R 侧**：msync M0/M1（移植 dappermint 8 个修复）+ M2（os_sync）；DXMT 集成 + dlsym 自检；winemac C1/C2/C3/C5；D3DMetal 导入器（lipo 区分 x86_64/arm64/arm64e，宿主层按 GFXT 语义实现、架构中立，arm64 版出现即复用）；CJK 字体替换、`LANG` 控制、SDL2。
- **依赖配方**：同一份配方同时产出 x86_64 和 arm64 两个版本。
- **A-lite**：aarch64-darwin 构建（`--enable-archs=aarch64,arm64ec,i386`，移植 winecx 40ce8f4/6baebec）；分发器切换 x18（toggle entitlement 可自签）；sysctl 合成 ID 寄存器。
- **FEXUnixLib Darwin 版**：7 个 unixcall 各给实现或桩并配单测；单独编译以绕开 CMake 宿主检查；提交上游 PR。
- KosmicKrisp B2 的 MR。
- App 0.1：SwiftUI 界面 + `cider` CLI。
- R3：预检 v0、诊断、国服云入口、实验 E1。

**退出**
- 0.1 发布，DMG 已公证；
- bottle 内 Steam 在 msync 下冷启动 20 次零看门狗；
- 5 款 DX11 游戏在 DXMT 下可玩；
- 原生 ARM64 PE 在 A-lite 上能运行；x18 单次切换 ≤10 ns；
- FEXUnixLib 单测 7/7 通过；
- B2 的 MR 已提交；
- 预检审计：未验证组合拉起游戏 exe 0 次。

### P2 A 授权路径端到端（2027-02-01 → 04-30）

**交付**
- 基线切到 Wine 12.0；CX 27 源码公开后，做 diff-of-diffs，导入其中的 arm64/FEX 胶水。
- Engine A 本体：尽早释放 pagezero、映射 KUSER_SHARED_DATA、4K spawn、TSO trap 按线程开启（失败回退 LRCPC2）、W^X 走 MAP_JIT 并按线程切换写保护。
- 接入 FEX 最新 tag，以及 DXMT arm64x、wine-mono arm64、mtld3d aarch64。
- 为 `CiderEngineA.app` 建签名流水线。
- R↔A 迁移：`drive_c` 和注册表复用，系统 DLL 用 `wineboot -u` 重建，迁移前做 APFS 快照。
- 按 G2 结果启动 V-mac 原型、B1。
- 0.2 beta：配方、档案、兼容库 v1，启动器回归套件（LRS）。
- R3：投递 brief；获授权后在卷 1 做 E5。

**退出**
- x64 测试程序通过 ≥30 个，i386 通过 ≥10 个；
- 5 款 DX11 游戏在 A 上的帧率 ≥ R 的 70%（同一台机器）；
- 7-zip 和 Cinebench 测出 A/R 比值，TSO 开、关各一组；
- 10 个 bottle 做 R→A→R 往返迁移，数据无损；
- G3 的决策写入 `docs/decisions/`。

### P3 WWDC27 冲刺（05-01 → 07-31）

**交付**
- A beta（用户自选开启）。DX12 游戏自动路由到 R 的 bottle，界面标注“依赖 Rosetta”。
- 实验开关：vkd3d-proton arm64x + Cider 自己构建的 KosmicKrisp（含 B1、B2）。
- macOS 28 beta 首周报告：`game-test-tool` 下 wineserver 能否存活、probe 复跑、私有 API 是否可用。
- 如果 GPTK 5 带 arm64 切片，编写 arm64 版 libd3dshared 胶水。
- 如果 G3 选了 V，发布 V alpha。

**退出**
- top-100 游戏中，A 的通过率 ≥ R 通过率 × 0.8；
- 28 beta 报告公开；
- 完成 G4 决策。

### P4 切换与 1.0（08-01 → 10-31）

**交付**
- A 成为新建 bottle 的默认引擎；
- 迁移助手：Denuvo 游戏先警告，并冻结指纹；
- R 冻结，只修安全问题；
- Sparkle 分阶段推送，同时提供国内镜像。

**退出**
- macOS 28 正式版发布后 7 天内，1.0 可用；
- top-100 中至少 70 款在 A 上可用；
- 米哈游三款游戏零误放。

### P5 开源 DX12（11-01 → 12-31）

**交付**
- vkd3d-proton + KosmicKrisp 对白名单游戏默认启用；
- DXMT d3d12 在作者解除 “DO NOT USE” 后按白名单启用；
- 给出 V 的结论：发布或封存；
- 上游化：FEXUnixLib、msync、C1、KosmicKrisp。

**退出**
- 至少 10 款 DX12 游戏在 A 的开源路径上通过 30 分钟冒烟；
- 至少 5 个补丁被上游合入。

## 4. 架构决策

**D1 引擎**
- `cpu_backend` ∈ {`rosetta-x86_64`, `arm64-fex`, `vm`}；只做 64 位（新 WoW64）bottle，架构无关，可 R↔A 迁移。
- 最低 macOS 14 + 功能门控（00 号裁定）；Engine A 暂定 ≥26.5，以 probe 实测为准。

**D2 签名**
- `CiderEngineA.app`：Developer ID + hardened runtime + `embedded.provisionprofile`，entitlement 为 cross-architecture-support、custom-x18-abi-toggle、allow-jit；单独公证、单独下载。
- 自编译版本拿不到该 entitlement，只能跑 A-lite。A3（关 SIP + `amfi_get_out_of_my_way=1`）仅限开发。

**D3 Wine 基线**
- R 和 A 共用一棵树：上游 11.18，之后升到 12.0，外加 CX 26.3 的主题补丁队列。
- A 额外维护 `darwin-arm64` 主题，涵盖 pagezero、KUSER、x18、W^X、ID 寄存器、信号处理。
- CX 26.3 原样构建，只作 oracle 对照。

**D4 FEX**：直接用上游 PE 模块，不 fork Linux 部分；macOS 相关改动只放在 FEXUnixLib。

**D5 同步**
- msync：M1 修缺陷 → M2 os_sync SHARED → M3 wineserver 可等待 msync 对象、WaitAll 原子化。与架构无关，A 沿用。
- CEF 冷启动测试达标前默认 `sync=server`。

**D6 图形**

| API | R | A |
|---|---|---|
| D3D8/9 | wined3d-GL（加 CX 的 remap 补丁）；mtld3d 白名单 | 同 R，但 mtld3d 优先 |
| D3D10/11 | DXMT；D3DMetal 可选 | DXMT arm64x；wined3d-vk 兜底 |
| D3D12 | D3DMetal（由用户导入） | ≤27 上路由到 R；B1 和 B2 解决后用 vkd3d-proton+KK；出现 arm64 版 D3DMetal 就接入 |
| Vulkan | KosmicKrisp（26+）；MoltenVK（14/15） | 同 R |

- 回退必须可见：在 UI 和日志中写明原因。
- D3DMetal 不进仓库，不做任何修改，由用户从 GPTK 导入。

**D7 进程模型**：采用模型 A，由 GUI 派生 wine，TCC 权限归到 Cider.app。CLI 通过 XPC 发请求。每个游戏配一个 shim（arm64 stub，带唯一 bundle ID 和游戏类别）。

**D8 分发**
- Cider.app 公证 + Sparkle；引擎在 bundle 外，每个 bottle 固定引擎版本。
- 每个文件多个地址：GitHub 主源 + 国内镜像（候选 Gitee/对象存储 CDN）+ 用户自定义前缀，测速择优；游戏数据直连官方 CDN。

**D9 数据**
- 引擎索引采用 TUF 风格：Ed25519 签名，带 `expires` 和 `revision`。
- 配方用 YAML 编写，编译成 JSON。
- 兼容库以 umu-ID 为键，用 minisign 签名。

```json
{"schema":"cider.bottle/v1","engine":{"id":"A-12.0-c3","pin":true},
 "cpu_backend":"arm64-fex","sync":"msync","locale":"zh_CN.UTF-8",
 "graphics":{"d3d11":"dxmt","d3d12":"route:R"},"drives":{"Z":null}}
```

**D10 App**：SwiftUI + CiderKit + `cider` CLI。界面以 zh-Hans 为主，英文同步提供。

**D11 CI/QA**
- GitHub Actions（macos-26）：R/A 引擎、kk、DXMT 构建，签名公证，数据校验，轮询（HYP tag、Steam buildid、kk MR、组件 tag）。
- 自托管测试 Mac：每夜 LRS、冒烟、A/R 性能对比；新 beta 首日复跑 probe。

**D12 Engine V**
- **V-mac**：VZ 跑 macOS 27 客户机 + Engine R，成本低，只面向应用。
- **V-linux**：VZ Linux 客户机（4K 页、prctl 开 TSO）+ Wine ARM64EC + FEX，GPU 走 Venus/API 远程化，仅研究。G3 定取舍。

## 5. R3：米哈游三款游戏

**底线**：永不使用官方 iOS/iPadOS 客户端，也不使用 Mac App Store 上的 iPhone/iPad App、PlayCover 或任何 iOS 运行器。反作弊原样运行。

**主路线**：在 Cider 中运行 Windows PC 客户端（HoYoPlay、米哈游启动器、Epic；绝区零另有 Steam 版），图形用 DXMT。

**预检**
- 裁定的维度是“游戏 × 区服 × 渠道 × 版本 × 引擎 × macOS”。只有裁定为 `playable` 时，才拉起游戏 exe，否则一律展示路线卡。
- CI 轮询 `getGameBranches` 的 tag；版本一变，对应裁定降为 `unverified`。
- 检测到会加载 `HoYoKProtect.sys` 时，直接判为 `blocked`。

**红线（CI lint 强制执行）**：不做 KMDF 宿主；不伪装 SteamOS/Deck 或 Steam 启动；不阻断网络；不隐藏 Wine 导出；不修改游戏文件；不伪造 GPU 身份。

**兼容修复**：只做有 Windows 对照的忠实性修复，先提交上游（例如 crypt32 问题，先在 11.18 上复测）。

**实验（只记录，不干预）**：E1 绝区零 Steam 版无伪装基线；E2/E3 原神、星铁；E4 官方云 Windows 客户端；E5 在已授权的 Engine A 上运行（仅卷 1）。

**兜底**
- 国服：官方网页云，用 Chrome 的 `--app` 模式打开；
- 国际服：如果 E4 可行，用官方云客户端；
- GFN、Xbox 云：默认折叠。
- `form` 字段的枚举不含 `ios-on-mac`，写进去 CI 直接失败。

**合作**：P2 投递中英双语技术简报，诉求依次为：
1. 在不支持的环境里给出可识别的“不支持”提示；
2. 不因使用兼容层而封号；
3. 把 Proton 的放行策略扩展到 Cider，**同时覆盖 Rosetta 和 FEX 两种形态**；
4. 提供一个测试联系人。

跟踪绝区零在 Steam Frame（ARM+FEX）上的表现，作为 A 的先例。

**Rosetta 退场后**：A 上裁定为 `playable` 才迁移，否则走路线卡，用户永不停在反作弊弹窗上。

## 6. 资源计划

- **配额**：按每周 6–8 个窗口计，15 个月约 420 个窗口。分配为：A 35%、R 15%、开源图形 12%、App 与数据 18%、R3 8%、CI/QA 12%。
  - 每个任务不超过 1 个窗口，并附可执行的验收步骤。
  - 每周留 20% 窗口处理回归。
- **8GB M3**：只做 Swift、DLL 增量编译、probe、小程序；冷构建/签名进 CI，3A 与米哈游进测试机。
- **测试 Mac**：16–24GB 内存，外接 1–2TB SSD，**最迟 2026-11-15 到位**（E1 和 LRS 需要），P2 起为刚需。安全策略按系统卷独立设置，分三个卷：
  - 卷 1：正式版、完整安全，用于米哈游测试、发布验证和 runner；
  - 卷 2：A3 降级安全，只跑 Engine A 原型，**不做米哈游测试**；
  - 卷 3：beta 系统（之后装 28 beta）。
- **只能人工做**：ADP 与授权申请、测试机安全设置、装 Rosetta、米哈游实验、合作投递。

## 7. 风险

| 风险 | 触发 | 应对 |
|---|---|---|
| 拿不到授权 | 到 G2/G3 仍未获批 | 转 V；A 退为 A2 |
| 28 的 Rosetta 子集不接纳 Wine | 28 beta 上 wineserver 崩溃 | R 冻结在 ≤27，推 V-mac |
| 私有语义变化 | 新 beta 上 probe 回归 | 当周修复或回退 |
| FEX 过慢 | A/R 比值中位数 <0.5 | 按游戏路由到 R；关闭向量 TSO |
| GS/XFB 无人做 | 2027-01-31 仍无相关 MR | Cider 做 B1 |
| 无 arm64 版 D3DMetal | WWDC27 仍无 | DX12 只走开源路径 |
| CX 27 源码迟迟不公开 | 03-31 仍未公开 | 自研，工期加 6 周 |
| HoYo 不放行 macOS | E1 被服务端阻断，且 60 天无回复 | 裁定改为只走云 |
| FEX 环境被判异常 | E5 出现反作弊报错 | 28 之前 HoYo bottle 留在 R |
| 配额不足 | 任一阶段延误超过 3 周 | 从 P5 开始往前砍 |
| 证书、profile 单点保管 | 1.0 发布时 | 增设第二名保管人 |

## 8. 不做清单

- 官方 iOS/iPadOS 客户端、iPhone/iPad App、PlayCover 或任何 iOS 运行器。
- 任何反作弊规避：KMDF 宿主、伪装、断网启动、改文件、解锁帧率。
- 32 位 bottle、Intel Mac、Rosetta 运行时 hook、跨进程 D3DMetal。
- DXVK-macOS/MoltenVK fork、米哈游下载器、2027 年原生 Steam 桥。
- 自研 D3D12→Metal、依赖 MSC、默认 Zink、内置 Chromium、上架 Mac App Store、法律分析。
