# Cider 战略草案 C：可持续优先

> 2026-09-27 · 视角 C（Sustainability First）· 依据 docs/research 01–22，以及 00 号的裁定（冲突时以各报告“事实核查记录”为准）。不含法律分析。

## 0. 论点

一个人加 AI 代理，拼人力是拼不过 CodeWeavers 的。能赢的地方只有三处：

1. **把会变的东西放进数据，不放进代码。** 启动器、游戏版本、WebView2、Steam build 每周都在变，CrossOver 的 changelog 大半是“更新后坏了”的修复。Cider 把“怎么跑”全部做成签名、可热更新的 Profile/Verdict 数据。回归由夜间自动化发现，修复靠推送数据，而不是发新版。
2. **自有代码越少越好。** 坚持上游优先：上游 Wine 加一个有预算上限的 CX 补丁队列。DXMT、FEX PE 模块、mtld3d、Highball 的 CC0 数据都直接复用。每个补丁都要标明上游状态，并定期清账。
3. **只在 CrossOver 做不到的地方做深。** 一是米哈游三游戏的“预检、不弹窗”体验（R3），二是中文优先（R4），三是诊断。其余功能按“够用”标准对齐 CrossOver。

原则：机器负责盯回归，人只修根因；用数据热更新，少发代码版本；“不做清单”和“做清单”同样重要。

## 1. 阶段计划（2026-10 → 2027-12）

容量假设：每周 8 个 5 小时配额窗口（下称“窗”），其中 20% 固定留给运维（rebase、游戏版本日、启动器故障）。实际配额不同，就按比例伸缩。每项任务写成一张“窗口卡”，包含目标、涉及文件、验收命令和回滚方法。一窗做一张卡，中断后可以接着做。

**P0 地基与自动化（2026-10-01 → 11-15，约 40 窗）**
- 开发机：装 Rosetta、Xcode 26.6 + Metal toolchain、llvm-mingw、bison≥3、pkgconf、ccache、meson/ninja。加入 Apple Developer Program；通过 DTS/Feedback 申请 `com.apple.developer.cross-architecture-support`。
- 仓库：
  - `cider`：App/、Packages/CiderKit、Tools/{ciderctl,cider-probe}、recipes/engines、workflows；
  - `cider-wine`：补丁队列；
  - `cider-data`：CC0 数据；
  - `cider-lab`：私有仓库，放夜间实验室。
- CI：
  - `engine-build`：wine-11.18 基线，11.19（约 10-02 发布）出来后立即 rebase；
  - `oracle-cx263`：原样构建 CX 26.3，tarball 和 sha256 自行归档；
  - `data-publish`：数据发布；
  - `watchers`：监视上游变化（见 D8）。
- CiderKit/ciderctl：`bottle create/run/logs`、引擎清单验签、切换前做 APFS clone 快照。
- 数据 schema v1（Recipe/Profile/Verdict），配校验器、红线 lint 和 minisign 发布。
- `cider-probe`：输出 JSON，覆盖 4K spawn、soft pagezero、`thread_set_x86_64_compat`、x18、MAP_JIT 五项。
- R3 的 H0 阶段（见 §3）。
- **退出标准**：
  - 引擎冷构建 ≤90 分钟，热构建 ≤25 分钟；
  - 冒烟全绿：`wineboot --init`、`cmd /c ver`、`syswow64\cmd.exe /c ver`；`DYLD_PRINT_LIBRARIES` 下没有加载 `/usr/local` 或 `/opt/homebrew` 的库；`otool -l` 显示 `__PAGEZERO=0x1000`；
  - `ciderctl run` 在 M3 上跑通 notepad 和一个 DXMT D3D11 样例；
  - lint 对 20 个违规样例全部拒绝；
  - 在伪 HoYo 安装目录上预检 50 次，游戏 exe 被拉起 0 次；
  - entitlement 申请已拿到工单号。

**P1 Engine R Alpha（2026-11-16 → 2027-01-31，约 70 窗）**
- 补丁队列首批主题（见 D2）：
  - D3DMetal ABI；
  - Rosetta 相关补丁，含 M3 专用的 24265；
  - msync M0–M1；
  - winemac C1–C3、C5；
  - `unix_wgl.c` 的 `mach_vm_remap`；
  - 中文字体和 locale 默认值；
  - sdl2-compat；
  - GStreamer 1.28 裁剪包。
- 图形：
  - 集成 DXMT；
  - D3DMetal 导入器：支持 3.0 和 4.0b2，导入时用 `lipo -archs` 和 `codesign -v` 校验；
  - 后端回退必须可见。
- 测试 Mac 到位，夜间实验室上线。
- 12.0-rc 期间每周 rebase 一次。
- Steam in bottle；启动器回归套件 LRS 首批覆盖 Steam、Epic、EA、Battle.net、GOG、cefclient、WebView2 样例。
- WebView2 固定版本通道：使用 Fixed Version，并设 `msedgewebview2.exe` 为 win7，禁止 EdgeUpdate 常驻；LRS 通过后才升级通道。
- GUI MVP（zh-Hans/en）：瓶子、配方安装、按程序设置、游戏 shim、诊断包。
- 国内镜像；R3 的 H1 阶段。
- **退出标准**：
  - 夜间实验室连续 7 晚没有新增失败；
  - Steam UI 冷启动 20 次，零次 “killing unresponsive browser”（本阶段允许 Steam 会话用 `sync=server`）；
  - 10 款免费或 DRM-free 游戏冒烟通过；
  - E1–E4 报告公开；
  - rebase 中位耗时 ≤1 窗；
  - alpha 测试者 ≥50 人。

**P2 Cider 1.0（Engine R）（2027-02-01 → 04-30，约 85 窗）**
- rebase 到 Wine 12.0。12.0 预计 1 月中下旬发布（±1–2 周），排期留 2 周余量。
- msync M2：改用 os_sync SHARED。
- Top-100 Profile：导入 Highball CC0 数据，再自行验证。
- 接入 Sparkle 2.10；根据 T4（Game Mode）结果落地。
- CX 27 源码公开后，自动做 diff-of-diffs，优先吸收 arm64 相关的 LGPL 改动。
- Engine A 原型第 1–2 步（A-lite：工具链、x18、原生 ARM64 PE）。
- 与 CrossOver 对齐的功能清单：瓶子导出/导入/复制、Run Command、启动器 .app、按程序覆盖后端、MSync 开关、高分辨率模式（RetinaMode + LogPixels 192）、Metal HUD、DLSS→MetalFX（DXMT）。
- **退出标准**：
  - LRS 覆盖 6 个启动器，连续 14 晚全绿；
  - Steam UI 在 msync 下通过冷启动门禁（20 次零看门狗），Steam 游戏因此也能用 msync；
  - 游戏类瓶子默认开 msync，N 组单对象乒乓 p50 达到 server 模式的 5 倍以上，1 小时压力测试无挂起；
  - 启动器故障从发现到 Profile 热修复，中位 ≤24 小时；
  - 测试者计划中，反作弊弹窗到达用户的次数为 0；
  - 公证 DMG 发布。

**P3 Engine A 冲刺 + WWDC27（2027-05-01 → 07-31，约 80 窗）**
- 按触发器 T-ENT（2027-03-31 前是否拿到 entitlement）分两路：
  - **拿到**：做 Engine A beta。PE 侧为 aarch64/arm64ec/i386；FEX 用上游 PE 构建（FEX-2609 起），为 FEXUnixLib 的 7 个 unixcall 写 Darwin 实现或桩；图形用 DXMT arm64x；支持瓶子从 R 迁到 A（`wineboot -u` 重建系统 DLL）。
  - **没拿到**：只保留 A-lite；公开说明 macOS 28 上的限制；V 后端（虚拟机）只给出 UTM/Fusion 的使用指引。
- WWDC27 当周：在 macOS 28 beta 上跑 T15（legacy games 机制对 wineserver 的影响），并检查 D3DMetal 是否有 arm64 切片，据结果当周重排 P4。
- **退出标准（A1 路线）**：x64 和 i386 控制台测试、DXMT D3D11 样例都通过，20 款游戏可玩；10 个瓶子迁移成功。

**P4 macOS 28 过渡（2027-08-01 → 10-31，约 80 窗）**
- 发布 28 首日兼容版，Rosetta 检测逻辑按 28 的规则更新。
- A1 成立时，28 上的新瓶子默认用 A。
- DX12 游戏在没有 arm64 D3DMetal 时，路由到 ≤27 系统上的 R 瓶子，并在 UI 上标注。
- 向 KosmicKrisp 上游贡献 B2（single-texel 对齐模拟）。
- **退出标准**：28 正式版发布后 72 小时内发出 Cider 兼容版；每次后端回退都在 UI 和日志中写明原因。

**P5 稳态与社区（2027-11-01 → 12-31，约 50 窗）**
- **退出标准**：
  - bus factor ≥2：第二维护者持有一把发布密钥；
  - 社区提交的 Profile PR 占比 ≥30%；
  - 补丁队列比 1.0 时缩小 ≥20%（靠上游合入）；
  - 完成 2028 规划。

## 2. 关键架构决策

**D1 引擎：CPU 后端可插拔**
- 瓶子元数据记录 `cpu_backend ∈ {rosetta-x86_64, arm64-fex}`。只做 64 位瓶子，32 位程序走新 WoW64（`--enable-archs=i386,x86_64`）。
- **Engine R**：macOS ≤27 上的主力。每次启动都检查 Rosetta，缺失时引导用户重装（升级到 27 后 Rosetta 不会自动恢复）。
- **Engine A**：作为研发方向可以开工；能否成为产品，取决于是否拿到上述 entitlement。最低系统版本由 `cider-probe` 实测决定，26.5 只是假设（00 裁定 1）。
- 最低系统 macOS 14，功能按系统版本门控；推荐 15 及以上；CI 覆盖 26 和 27（00 裁定 7）。

**D2 Wine 基线与补丁队列（00 裁定 11，17 号报告）**
- `cider-wine` 的 `cider/devel` 分支 = 上游 wine-11.18 + 按主题拆分的 CX 26.3 补丁。
- 每个 devel tag 都 rebase，每两个 tag 发布一次引擎；12.0 作为 1.0 的基线。
- 原样构建的 CX 26.3 只作对照 oracle 和回退引擎；不维护 11.0-stable 线。
- 补丁就是 git 提交，以下尾注必填，CI 会检查：
```
Cider-Topic: msync|d3dmetal-abi|rosetta|winemac|cef|cjk|quirk
Upstream: merged <sha> | MR !11058 | bug 60263 | cider-only: <理由>
Cider-Test: lrs/L01-steam | winetest/ntdll:sync
Cider-Review-By: 2027-03-31
```
- 每次 rebase，CI 生成 `patch-report`，列出三类补丁：已被上游吸收的（自动丢弃）、有冲突的、过了复查日期仍未上游的。
- 补丁预算：1.0 时活跃补丁 ≤80 个，其中 cider-only ≤25 个。超出预算时，新补丁必须替换掉一个旧补丁。
- 引擎内置一个数据驱动的规则层 `cider-rules`，内容由 Profile 生成，包括子进程 argv 的增、删、改，AppDefaults，DLL override，writecopy。它替代 CX 按 exe 名写死的 hack 和 libcef 字节补丁。
- **不移植**：
  - CX 的 %gs 字节补丁（上游 10.5 起已有 GSBASE swap）；
  - cxcompatdb、alt loader；
  - 32 位瓶子相关的 20810。
- CW Hack 24067 单独成组：先用 `nm -u`/`dyld_info -imports` 确认 libd3dshared 真正需要哪些符号，再决定是否保留（00 裁定 10）。
- 上游化节奏：每月至少提 2 个 MR。候选包括子窗口 Metal swapchain（bug 60263）、FSEvents 目录通知、剪贴板图片、中文字体默认值、Joy-Con L 的 PID 笔误。同时向 test.winehq.org 提交 macOS 上的 winetest 结果。

**D3 同步**
- msync 定位为 Wine 11 inproc_sync 的 macOS 后端。
  - M0–M1：移植 CX 26.3 的实现，加上 dappermint 的 8 个修复；
  - M2：改用 os_sync SHARED（macOS 14.4+，更低版本回退）；
  - M3：让 wineserver 能等待 msync 对象，视需要再做。
- 门禁：Steam UI 冷启动 20 次零看门狗。门禁通过之前，CEF 类会话固定使用 `sync=server`，切换时重启 wineserver（00 裁定 9）。

**D4 图形（按 API）**

| API | 默认 | 备选 / 实验 | 不做 |
|---|---|---|---|
| D3D10/11 | DXMT（跟上游 main，必要时做薄 fork） | D3DMetal；打补丁的 wined3d-vk（白名单） | DXVK-macOS |
| D3D12 | D3DMetal（Engine R，用户导入） | vkd3d-proton + KosmicKrisp 内部 bring-up（不发布） | 自研转译层 |
| D3D9/8/DDraw | wined3d-GL + CX remap 补丁；cnc-ddraw | mtld3d（白名单） | 自研 d9mt |
| OpenGL / Vulkan | Apple GL 4.1 / MoltenVK 1.4.x | Zink 只跟踪；KosmicKrisp（macOS 26+） | fork MoltenVK |

D3DMetal 不能提交进仓库，也不能随包分发，所以只从用户自己的 GPTK 导入，原样加载，不做修改。

**D5 App、CLI 与进程模型（16 号报告，00 裁定 6）**
- 业务逻辑全部放在 CiderKit（SwiftPM，只用 CLT 就能测试）。`ciderctl` 是一等公民，只通过 XPC 把请求交给派生者；GUI 只是前端。按模块借用 frankea/Whisky 的 WhiskyKit。
- 进程模型 A：由 GUI `posix_spawn` 启动 wine，responsible 进程是 Cider.app。
  - Engine R 用 ad-hoc 签名，放在 `~/Library/Application Support/Cider/Engines/<id>`；
  - Cider.app 用 Developer ID 签名并公证，entitlements 只有 audio-input 和 camera；
  - Engine R 是否改成 Developer ID 签名，由 T8 实验决定。
- 游戏 shim（模型 E）：arm64 stub，本机 ad-hoc 签名，唯一 bundle ID，声明游戏类别，通过 XPC 转交给派生者。
- Engine A 做成单独公证的 `CiderEngineA.app`（Developer ID + provisioning profile + hardened runtime）。

**D6 数据格式：三类数据，一条签名通道**
- `cider-data` 采用 CC0，字段是 Highball-db 的超集，可以直接导入它的数据。三类数据分工如下：
  - Recipe 描述怎么装：用 YAML 编写，编译成 JSON；每个下载都带 sha256、大小和多个镜像；宿主侧禁止执行任意 shell。
  - Profile 描述怎么跑：带 `when` 条件，动作按类型定义且幂等。
  - Verdict 记录某个组合现在能不能跑，附 provenance。
- 主键：不透明的 umu-ID 字符串，外加商店 ID、主 exe 的 sha256 和 PE VersionInfo。
```json
{"schema":"cider.profile/v1","id":"umu-990080","revision":3,
 "match":{"steam_appid":990080,"exe":"HogwartsLegacy.exe"},
 "when":{"engine":">=12.0-c1","cpu":"rosetta-x86_64","macos":">=15"},
 "actions":{"renderer":{"d3d12":"d3dmetal"},"sync":"msync","retina":false},
 "drm":{"denuvo":true,"freeze":["engine","cpu_topology","avx_advertise"]},
 "warn":[{"if":"ram_gb<16","key":"mem.low"}],
 "provenance":{"by":"cider-lab","date":"2027-02-10","chip":"M3","ram_gb":8,"result":"unverified"}}
```
- 发布流程：CI 校验 → 红线 lint → minisign 签名（签名密钥放在需人工批准的 GitHub environment 中）→ 发布到 GitHub Pages 和国内镜像。
- 防回滚：`timestamp.json` 7 天过期，`revision` 只增不减。
- 客户端每 6 小时拉取一次；离线时使用最后一份有效数据；用户可以在本地覆盖。
- 引擎清单 `index.json` 用 Ed25519 签名，密钥与 Sparkle 的分开；支持 `yanked` 撤回。每个瓶子固定引擎版本。

**D7 打包与分发**
- Cider.app 做成小体积的公证 DMG，用 Sparkle 更新。
- 引擎和组件（DXMT、GStreamer、SDL、MoltenVK）各自打包成 tar.xz，放在组织名下的 GitHub Releases。
- R4：国内对象存储 + CDN 镜像同步引擎、组件、数据和 appcast。所有产物都有签名清单，镜像只作不可信的传输通道；客户端测速后选源。游戏本体始终走官方 CDN。

**D8 CI / QA**
- 公开仓库使用标准 arm64 runner（3 核、7 GB 内存、14 GB 磁盘，免费）。跑在上面的任务有：
  - Wine 冷构建（i386 和 x86_64 可以拆成两个 job）；
  - 依赖前缀、LLVM 和 DXMT，产物作为 release 资产缓存；
  - 签名和公证；
  - 数据校验；
  - 每日检查链接和哈希漂移。
- watchers 监视以下变化：
  - Wine tag：自动尝试 rebase，冲突时开 issue 并附冲突清单；
  - Steam 客户端 buildid；
  - WebView2；
  - DXMT、MoltenVK、KosmicKrisp 的发布；
  - CX 源码 tarball；
  - GPTK；
  - HoYo HYP API 的 `tag`（只读）。
- `cider-lab` 夜间实验室：私有仓库加自托管 runner，只执行定时任务和受信分支，只拉取已签名的产物，绝不运行外部 PR 的代码。测试 Mac 到位之前，先用开发机夜间串行跑。测试内容：
  - macOS winetest：输出 JUnit，维护已知失败基线；
  - LRS：5 秒内 rAF ≥50、截图不是纯色、GPU 进程只创建 ≤1 次；
  - 游戏冒烟：首帧 present → 截图 SSIM 比对 → 用 HUD 统计 p95 帧间隔；
  - HoYo 日志回放。
- 虚拟机只用于构建和非图形测试，GPU 相关测试必须在实体机上做。
- 诊断：`ciderctl diag` 一键生成脱敏的支持包；提供 Metal HUD 开关；错误先规则化分类，再给出可以直接操作的建议。

**D9 中文优先**
- UI 用 zh-Hans String Catalog。
- 中文瓶子模板设置 `LC_ALL=zh_CN.UTF-8`（ACP 936），并写入字体替换：SimSun→Songti SC，YaHei→PingFang SC。
- 引擎不链接 fontconfig。
- IME 测试集覆盖拼音、搜狗、微信输入法。

**被否决的选项（从可持续性角度）**
- 直接用 CX 源码包作为主干：没有 git 历史，每个 CX 版本都要重做一遍 diff，新鲜度落后上游 0–12 个月。只保留它作 oracle。
- 另开 11.0-stable 线：9.0.1 之后上游再没出过维护版，等于所有回移植都由自己负责。
- 把 Wine 放进 Cider.app 的 bundle：引擎每次更新都得整包公证，也不能按瓶子固定引擎版本。
- 自托管 runner 跑公开 PR：有安全风险，而且会占用开发机内存。
- Profile 里允许写 Python 或 shell 脚本：会引入供应链风险，审核成本高，也无法做静态红线检查。
- 与 Highball 另起一套不兼容的数据格式：社区数据会被割裂，冷启动成本翻倍。

## 3. R3 专项：原神、星铁、绝区零

**主路线与红线**
- 主路线：Windows PC 客户端在 Cider 中原样运行，反作弊完整保留。覆盖的渠道有 HoYoPlay、米哈游启动器、Epic，以及绝区零 Steam 版。
- **官方 iOS/iPadOS 客户端永远不用。** 不走 Mac App Store 的“iPhone 与 iPad App”，不用 PlayCover 或任何 iOS 应用运行器；云兜底中如果是 iPad App 形态，同样排除。
- **不篡改、不伪装、不隐藏、不绕过反作弊。** 这样做会让玩家被封号，本身也属于检测规避。
  - `docs/policy/anticheat.md` 写入 21 号报告 §4.3 的红线表。
  - CI lint 对 `anticheat.vendor=hoyoverse` 的 Profile 做拒绝，命中以下任一项即不能合并：`SteamOS`/`SteamDeck` 伪装变量、`DYLD_INSERT_LIBRARIES`、网络阻断或延迟、`HideWineExports`、GPU 身份覆盖、指向游戏目录的文件替换。
- 不引入 wdfldr/KMDF 宿主。例外只有两种：HoYoverse 书面认可；或上游合入了带 tests、语义诚实的安全 API。
- 允许做的是有 Windows 对照测试的通用 API 修复：先提交上游，不按 exe 名或反作弊模块名分支。

**预检状态机：用户永远不会停在反作弊报错上**
```
识别安装 → 区服(cn/global)/渠道 → 版本(先读本地，再只读查 HYP API)
→ 环境检查(Rosetta/引擎/DXMT/磁盘≥游戏大小×2/内存) → 查 Verdict(游戏×区服×渠道×版本×引擎×macOS×CPU)
  playable          → 启动器 → 游戏（进程组管理，退出后回收 HYP* 与 wineserver）
  unverified        → 不启动游戏 exe；显示“新版本验证中”和云端按钮
  blocked-anticheat → 不启动游戏 exe；显示路线卡
  broken-launcher   → 只允许打开启动器，用于更新或修复
```
- 裁定库里没有的版本，一律按 `unverified` 处理。
- 如果检测到某个版本会加载 `HoYoKProtect.sys`，而 Cider 的 Wine 没有 wdfldr，默认判为 `blocked`。这条规则覆盖原神 6.5 以后的所有区服。
- 路线卡：
  - 国服：官方网页云，优先用 Chrome/Edge 的 `--app` 窗口打开，其次用默认浏览器；
  - 国际服：可选的授权云（GeForce NOW、Xbox Cloud），默认折叠；
  - 绝区零日服云：只有核实为网页版或原生 macOS 客户端后才展示。
  - schema 中 `form ∈ {web, native-macos, unverified}`，`ios-on-mac` 不在枚举里，写进去 CI 直接失败。
- `ciderctl diag hoyo` 只采集日志，不做任何修改。
  - 日志通道：`+module,+ntoskrnl,+seh,+loaddll`。
  - 分类：`DRIVER_IMPORT_MISSING(WDFLDR.SYS)`、`ZWLOADDRIVER_FAIL(c0000142/c0000355)`、`INITDRIVER_FAILED`、`UNITY_CRASH_HANDLER`、`SILENT_EXIT_AFTER_WORLD`、`LAUNCHER_CEF_WHITE`。
  - 同时标记环境是否被第三方改过（第三方 Wine、启动器补丁、GPU 身份伪装）。

**Verdict 示例**
```json
{"id":"mihoyo.zzz","edition":"global","channel":"steam","game_version":"3.2",
 "engine":"cider-wine-12.0-c2@<sha>","macos":"27.0","cpu":"rosetta-x86_64",
 "result":"unverified","ac_popup":null,"evidence":"cider-lab#123",
 "routes":[{"kind":"official-cloud","region":"JP","form":"unverified"},
           {"kind":"authorized-cloud","providers":["geforce-now"],"opt_in":true}]}
```

| 阶段 | 时间 | 内容 | 验收 |
|---|---|---|---|
| H0 | 2026-10 | 红线政策与 lint；三款游戏的 Verdict；预检和路线卡；国服云入口；版本 watcher（国服 `hyp-api.mihoyo.com`；国际服 `VYTpXlbWo8`，游戏 ID `gopR6Cufr3`、`4ziysqXOQ8`、`U5hbdsT9W7`）；diag 分类器 | 回放公开日志样例（#653、#763、spritz #16），分类准确率 100%；路线卡 3 秒内出现；版本变化后 30 分钟内自动提 PR，把对应 Verdict 置为 `unverified` |
| H1 | 2026-11 → 12 | 在测试 Mac 上取证，每款游戏预留 ≥75 GB：E1 绝区零 Steam 版（Windows Steam in bottle + DXMT，零伪装；先在 CX 26.3 oracle 上跑，再在 Cider 引擎上跑），并与 HoYoPlay 版对照；E2 原神（国际服和国服）；E3 星铁；E4 官方云的 Windows 客户端 | 每项都产出可复现报告，回答三个问题：`HoProtect` 服务是否创建、是否交给游戏进程、能否登录。E1 通过的标准是 M3 8 GB 上连续 30 分钟没有反作弊报错 |
| H2 | 2026-12 → 2027-02 | 忠实性修复：crypt32 先用原版 11.18 复测，仍复现则写 conformance test，在 windows-latest 上与 Windows 对照；ndis 只补不涉及安全语义的导出；HoYoPlay CEF 呈现问题；国服登录 WebView 的验证码 | 每项修复都有上游 MR 或 bug 号，不含按名称的分支 |
| H3 | 2027-01 → 03 | 写 Compatibility Brief v1（中英双语），分级诉求：①给出可查询的“不支持”返回，不弹通用反作弊错误；②不因使用兼容层封号；③把对 Proton 的放行扩展到带 Cider 构建标识的 macOS Wine；④提供技术联系人，并在版本更新前开放测试窗口。Cider 的回报：每个版本的回归报告、问题的最小复现、中文 Mac 玩家反馈汇总。渠道：Help Center 工单、HoYoLAB 公开技术帖、国服客服、媒体联系人，并抄送 CodeWeavers | 投递并公开存档；90 天内每月跟进 |
| H4 | 2027-03 起 | 版本日运维：新版本 72 小时内复核；测试员计划（默认关闭，开启时逐条确认风险）；8 GB 预设（1080p 低/中画质、DXMT 限帧）；Engine A 可用后，在 FEX 下复测 | SLA 达成率 ≥90%；用户看到的反作弊弹窗为 0 |

Cider 能保证的是：用户不会停在反作弊弹窗上，而且总有官方路线可走。能不能在 Cider 里玩，取决于 HoYoverse 的策略，所以产品文案不承诺“安全”或“不封号”。

## 4. 资源计划（1 人 + AI 代理，M3 8 GB）

- **本机只做**：Swift 和 CiderKit 开发；单个 DLL 的增量构建（ccache 已预热）；日志分析；数据和政策文档。不在本机冷构建 Wine、LLVM 或 DXMT，也不在本机签名公证。
- **交给 CI**：所有冷构建、签名公证、数据发布、各类 watcher，以及补丁队列的逐提交构建。
- **AI 代理与人的分工**：
  - 代理负责：rebase 冲突的初步解决、补丁移植、起草 Profile、日志分类、测试脚本；
  - 人负责：架构判断、红线审核、发布批准、与 HoYoverse 和上游沟通。
  - 每个仓库放一份 CLAUDE.md，写明构建命令、红线和验收脚本。
- **每周 8 窗的分配**：
  - 开发约 5 窗；
  - 运维约 1.5 窗：两周一次 rebase 约 1 窗；米哈游三款游戏一年约 50 次版本变更，每次约 0.5 窗；
  - 缓冲约 1.5 窗。
- **16 GB 以上测试 Mac 的采购时间**：必须在 2026-11 中旬（P1 开始）前到位。理由有四：
  - HoYo 取证：yaagl 建议 16 GB，三款游戏合计要 250 GB 以上磁盘；
  - CEF 多进程的 LRS 在 8 GB 上内存吃紧；
  - 夜间实验室不能和日常开发抢同一台 8 GB 机器；
  - 需要 macOS 26.5 和 27 两台机器分开覆盖。
- **采购建议**：Mac mini（M4 或更新，16 GB / 512 GB）+ 2 TB 外置 SSD，系统装 macOS 27。开发机保持 26.5，兼作 8 GB 轻量档测试机和 Engine A 的探针机。开发机上只装一款米哈游游戏做 8 GB 档验证，其余放在测试 Mac 的外置盘上，避免挤占 ccache 和构建目录的空间。2027-06 在测试 Mac 的外置卷上装 macOS 28 beta。

**运维日历（固定节奏，靠它避免“救火式”开发）**

| 频率 | 内容 | 执行者 | 占用 |
|---|---|---|---|
| 每日 | watchers、链接与哈希漂移检查、夜间实验室、HYP 版本轮询；失败时自动开 issue 并打标签 | CI / 实验室 | 0 窗（只在告警时处理） |
| 每周 | LRS 全量、Top-N 游戏冒烟、社区报告分诊、Profile 合并与发布 | 代理起草，人审核 | 约 0.5 窗 |
| 每两周 | 跟随 Wine devel tag rebase，看 `patch-report` 清账 | 代理先解冲突，人复核 range-diff | 约 1 窗 |
| 每月 | 引擎发布（每两个 tag 一次）、App 发布、至少 2 个上游 MR、评估 WebView2 通道、HoYo Brief 跟进 | 人主导 | 约 2 窗 |
| 每季度 | 复盘路线；逐条检查风险触发器；检查依赖单点和补丁预算；更新“不做清单” | 人 | 约 1 窗 |
| 年度节点 | Wine x.0（1 月）、CX 新源码、WWDC（6 月）、macOS 大版本（9–10 月）：各预留一周专项 | 人 + 代理 | 按需 |

**窗口卡模板（一窗一卡，放在 `docs/tasks/`）**
```yaml
id: P1-msync-M1-03
goal: 移植 dappermint 307f90f 与 6d31614（有界空转后睡眠）
inputs: [cider-wine: server/msync.c, dlls/ntdll/unix/msync.c]
accept:
  - ci: engine-build 绿；winetest ntdll:sync 与已知失败基线一致
  - lab: L01-steam 冷启动 20 次零看门狗
rollback: git revert；Profile 中 sync=server
handoff: 结果写入 docs/tasks/P1-msync-M1-03.md，列出未完成项
```
约束：一张卡不能跨越两个窗；做不完就拆卡。代理每次会话结束都必须写 handoff，下个窗口从 handoff 继续，不重跑已完成的工作。

**可持续性指标（每月自动出报表）**
- 活跃补丁数和其中 cider-only 的数量（目标：不超出预算，且逐季下降）；
- rebase 中位耗时（目标 ≤1 窗）；
- 启动器故障从发现到修复的中位时间（目标 ≤24 小时，其中数据修复占 ≥70%）；
- 运维占比（目标 ≤25%，超过 40% 触发砍项）；
- 夜间实验室的误报率，即 flaky 测试比例（目标 <2%，超标的用例先隔离再修）；
- 上游 MR 的提交数和合入数；
- 米哈游版本复核 SLA 的达成率。

## 5. 风险登记

| 风险 | 触发器 | 应对 |
|---|---|---|
| entitlement 不获批 | 2027-03-31 仍未获批 | Engine A 只做 A-lite；公开 macOS 28 上的限制；V 后端只给外部方案的指引 |
| macOS 28 的 legacy 机制不接纳 Wine | T15 中 wineserver 或 services 被系统杀掉 | 在 28 上把 A 设为默认；R 只在 ≤27 上提供 |
| rebase 负担失控 | 连续两次 rebase 中位耗时 >1.5 窗，或补丁数超出预算 | 改为隔一个 tag 再 rebase；冻结在 12.0，只 cherry-pick；集中两周做上游化 |
| msync 与 CEF 冲突 | 冷启动门禁不通过 | CEF 会话固定 `sync=server`；推迟 M3 |
| HoYoverse 不回应 | E1 零伪装取证失败，且 Brief 发出 90 天无回应 | 保持默认阻断 + 云兜底；每月复测；**绝不转向伪装** |
| 米哈游版本更新积压 | 连续两次没达到 72 小时 SLA | 新版本自动降为“验证中”；扩大测试员队伍；暂停新功能开发 |
| 单人倦怠、配额耗尽 | 连续两周运维占比 >40% | 按“不做清单”砍项；招募第二维护者 |
| 上游单点停更 | 60 天无提交，或出现破坏性改动 | 固定到已知版本；只做最小 fork |
| CI runner 资源不够 | 构建 OOM 或磁盘满 | 拆分 job；依赖改用 release 资产；受保护分支改到测试 Mac 上构建 |
| D3DMetal 一直没有 arm64 版 | WWDC27 后 GPTK 仍只有 x86_64 | Engine A 上的 DX12 标为不可用，并路由到 R 瓶子；推进 KosmicKrisp B2 |
| 签名密钥泄露 | 出现异常发布 | 离线根密钥与发布密钥分离；利用 timestamp 过期机制；按预案轮换 |
| 国内下载慢 | 镜像探测 p50 <1 MB/s | 增加镜像节点；支持分块续传 |

## 6. 不做清单

1. 官方 iOS/iPadOS 客户端的任何形式，包括 iPhone/iPad App、PlayCover、各种 iOS 运行器，以及云兜底中的 iPad App 形态。
2. 任何反作弊篡改、伪装或绕过：AC patch、HideWineExports、SteamOS/Deck 伪装变量、把非 Steam 安装伪装成 Steam 启动、timeout fix 或断网启动、GPU 身份伪装、让安全 API 空转的 KMDF 宿主、修改游戏文件来解锁帧率。
3. 自研米哈游下载器（Sophon）。
4. 32 位瓶子、Intel Mac、11.0-stable 引擎线。
5. DXVK-macOS 维护、fork MoltenVK、自研 D3D12 或 D3D9 转译层。
6. 自研原生 Steam 桥：只给 NotProton/macos-steam 提供 runner 接口。
7. 内置 Chromium、Microsoft 365、WeGame/ACE、网银 U 盾、税控盘、内核级反作弊游戏的联机。
8. 2027 年内把 VM 后端做成产品功能。
9. 默认开启遥测；在公开仓库的 PR 上跑自托管 runner。
10. CX 的 %gs 字节补丁、cxcompatdb、alt loader；`-static` 链接。
11. 法律和许可分析（不在本草案范围内）。
