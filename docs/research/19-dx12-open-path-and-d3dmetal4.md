# DX12 路线定案：D3DMetal 4 架构与系统要求、DXMT d3d12 成熟度、vkd3d-proton + KosmicKrisp 阻塞项

> 调研日期 2026-09-26（资料截至 2026-09-27 凌晨）· 置信度说明：**[高]** = 本次直接读取一手来源（源码文件、GitHub API、官方页面、本机 SDK 头文件、本机 Metal 探针）；**[中]** = 可信第三方（维护者 README/issue、媒体转述）或只做了部分验证；**[低]** = 推断或估算。工作量估算一律为 [低]。按用户偏好，本文不做法律分析，许可证只作为“能不能直接拿来用”的工程约束列出。与 04/05/06/14 号报告重叠的内容只做交叉引用。2026-09-27 已按独立事实核查修订（记录见文末“事实核查记录”）。
>
> 本机实测环境：Apple M3（8 核 CPU / 10 核 GPU）、macOS 26.5（25F71）、MacOSX26.5.sdk（Command Line Tools）。实测只用 Metal API 探针和 SDK 头文件，**没有**下载或运行 D3DMetal。

## 摘要

- **D3DMetal 4 仍是 x86_64 独占，公开渠道没有 arm64 或 arm64e 切片。** 没有找到任何第三方贴出的 4.0b2 `lipo -archs` 输出。但是能找到的证据全部指向 x86_64：dbc-hbin（2026-09-23）只再分发了一个 23,617,245 字节的 zip；philippremy 的替换脚本把 GPTK 4 载荷放进 `wine/x86_64-unix` 和 `wine/x86_64-windows`；Highball 把它打成 `x64-` 引擎；gamekit 在 2026-09-21 写明 “Wine and D3DMetal remain x86_64”；UTM 在 2026-07-24 写明 “ships as x86_64 only” [2][3][4][5][6][7]。[中高]
- **D3DMetal 4.0b2 能在 macOS 26 上运行，但只走 Metal 3 后端。** philippremy 在 macOS 26.6.2（25G83）+ CrossOver 26.3.0 上验证可用（2026-08-23），修好了 UE 5.6 在 D3DMetal 3.0 上的 `MaximumSamplerHeapSize` 断言。他的 README 写明 `D3DM_MTL4`（Metal 4 后端）“在 macOS 26 上无关”，只在 macOS 27+ 生效 [1]。另一个独立来源 thetheoryofR/toolkit4 也把 “macOS 27.0 beta” 列为 Metal 4 翻译层的前提，并用 `D3DM_MTL4=1` 开启 [62]。但 Apple 的 GPTK 页面没有写 D3DMetal 4 的最低系统版本 [11]，所以“MTL4 只在 27+ 生效”依据的是第三方说法，不是 Apple 文档；“4.0b2 能在 26 上跑”也只有 philippremy 一人实测。Highball 把 minMacOS 定为 27，这是它自己的策略，不是 D3DMetal 的硬性要求。它还在 macOS 27.0 上发现 Metal 4 后端有时序 bug，所以默认 `D3DM_MTL4=0` [3]。[中高]
- **截至 2026-09-26，没有发现 GPTK 4 的 RC 或正式版。** 最新可见版本仍是 4.0 beta 2。Gcenx 的最新构建仍是 3.0-3（2026-03-03），Apple 的 Releases 页面也没有 GPTK 条目 [4][9][10]。[中高]
- **DXMT 的 d3d12 还是“DO NOT USE”阶段。** 9 个 “D3D12 implementation, part N” PR 从 2026-07-01 开始提交，在 2026-07-06 到 2026-09-13 之间合入 main。当前实现只接受 DXBC（SM 5.1），遇到 DXIL 直接 `E_NOTIMPL`（拒绝点在共享的着色器初始化路径里）；GS、HS/DS、StreamOutput 也都返回 `E_NOTIMPL`；最高 FL 11_1，Tiled 资源、Mesh 着色器和 DXR 都没有。已经实现的有：root signature 1.0/1.1、Binding Tier 2、Heap Tier 2、placed resource（2026-09-09）和 ExecuteIndirect 签名。能跑的公开例子只有 *Animal Well*。作者没有给出 D3D12 或 1.0 的时间表，最新 release 仍是 v0.80（2026-04-23），其 release 说明提到从这一版起许可证由 MIT 改为 LGPL [14]–[24]。[高]
- **KosmicKrisp 的 XDC 2026 演讲在 2026-09-29 16:25（多伦多时间），今天还没发生，演讲内容无从得知。** 公开摘要只提 “features, performance and developer tips”。LunarG 2026-09-25 宣布通过 Vulkan 1.4 一致性认证，带这一版的 Vulkan SDK 在 2026-09-29 发布。两篇公告都没提 GS、XFB、sparse、RT 或 mesh [25][26][27]。上游 Mesa main 最新的 kk 提交是 2026-09-21 的 ae4821f（已通过 freedesktop GitLab REST API 核对，`kk_physical_device.c` 与镜像逐字节一致），没有 GS/XFB 相关内容 [30][63][64]。freedesktop GitLab 的网页被 Anubis 拦截，但 REST API 可以直接访问。据 API 查询，**截至 2026-09-27，没有任何已合并或 open 的 MR 给 kk 加 geometry shader 或 `VK_EXT_transform_feedback`**。RT 方面有一个 Draft MR !43303（实验性 AS / ray query / RT pipeline，2026-07-29 开启，08-11 最后更新）[65][66][67]。树外还有 squidbus 为 shadPS4 写的 “kk: Add sparse buffer binding”（2026-09-20）[35]。[高]
- **新发现：vkd3d-proton 在 KosmicKrisp 上创建设备有两个独立的硬阻塞，不止 XFB 一个。** vkd3d-proton master 的 `device.c` 先检查 `transformFeedbackQueries`，接着检查 texel buffer 的 single-texel 对齐（`storage/uniformTexelBufferOffsetSingleTexelAlignment`，或者 `AlignmentBytes == 1`），任何一项不满足都返回 `E_INVALIDARG` [37]。KosmicKrisp 两项都不满足：它没有暴露 `VK_EXT_transform_feedback`，single-texel 对齐报的是 `false` / 16 字节 [28][29]。本机探针显示，M3 上 Metal 对所有测试格式的 `minimumTextureBufferAlignment` 都是 16 字节 [49]，所以第二项只能靠驱动在着色器端做偏移模拟。05 号报告说“只缺 XFB”，这里更正。[高]
- **XFB 模拟难度中等，路线已经铺好。** Mesa 的 `src/poly`（MIT）是 2025-10-06 从 AGX 迁出的通用 GS/曲面细分模拟库。它的 GS lowering 自带 XFB 路径：count shader、prefix sum、pre-GS kernel、rast shader，没有 GS 时用 passthrough GS 承载 XFB [40][41][42]。KosmicKrisp 已经在用 libpoly 做曲面细分（Mesa 26.2，2026-08-05）[31][32]，所以 GS+XFB 基本就是照 Honeykrisp 接线。Honeykrisp 里 XFB 的 API 层只有约 100 行，而且 `vkCmdDrawIndirectByteCountEXT` 至今还是 `UNREACHABLE("TODO")` [43]。[高]（工作量估算 [低]）
- **MSC EULA §2.B 的原文本次没有拿到。** 原文只在安装包里，按规则不能下载。二手转述是“只允许为 shader conversion 目的分发动态库、须随附许可证与致谢、只能在 Apple 硬件上用”[53]。工程上的结论：Cider 的默认 DX12 路径**不依赖** `libmetalirconverter`。
- **CrossOver 27 的 arm64 DX12 方案还没定型，但留了开关。** CodeWeavers 2026-07-31 的原话是 “No D3DMetal in this build. Direct3D 12 support coming soon.” [59]（只经 AppleInsider 转引，描述的是 7 月的预览版）。NotProton PR #4（2026-09-25，仍 open）披露：FEX 构建里的 `cxcompatdb.so` 只有在 `CX_ENABLE_ARM_D3DM=1` 时才把 D3DMetal 当作可用；否则记录 “d3dmetal was set as the graphics backend but it is unusable”，然后**静默回退**到别的后端。这是 PR 作者对二进制行为的解读，不是 CodeWeavers 的文档；PR 本身没写 CrossOver 版本号，“Preview 2026082” 的对应关系来自 NotProton README [57]。FEX 构建同时内置 arm64 Wine 和 Intel Wine，老 bottle 仍在 Intel 那一侧使用 D3DMetal [55][56][71]。推断：CodeWeavers 已经在为 arm64 版 D3DMetal 预留代码路径，DXMT d3d12 可能是备胎（[低]）。
- **定案：** Engine R 的 DX12 以 D3DMetal 为主（macOS 26 默认 3.0，4.0b2 按游戏开启；macOS 27 上 4.x 默认关闭 MTL4）。D3DMetal 默认由用户导入。许可证并不强制这样做：GPTK 4.0b2 附带的许可证文本允许非商业目的单独分发完整、未修改的 framework。所以 Cider 下载原样 framework 可以作为可选来源，但不得修改（核查更正，见 P0-1）[69]。Engine A 在没有 arm64 D3DMetal 之前，DX12 游戏按 bottle 路由到 Engine R。同时，Cider 立刻向 KosmicKrisp 上游贡献“single-texel 对齐模拟”这个小而硬的阻塞项，并与 LunarG 对齐 GS/XFB 的分工。截至 2026-09-27，上游没有任何 GS/XFB MR，这部分目前没人认领 [67]。开源路径的目标是在 **Mesa 27.0（推断 2027-02 前后）** 前后让未打补丁的 vkd3d-proton 能在 KosmicKrisp 上创建设备。完整决策树和触发日期见 §7，阻塞项清单见 §8。

## 详细调研

### 1. D3DMetal 4（GPTK 4.0 beta 2）：架构切片、系统要求、版本状态

**1.1 架构（Q1）**

| 证据 | 日期 | 结论 | 置信度 |
|---|---|---|---|
| dbc-hbin/d3dmetal-redistributable `gptk-4.0b2`：`D3DMetal.framework-4.0b2.zip` 23,617,245 字节，sha256 `61ff2bb9…0f68`。README 和 release 说明都没有写架构 [4] | 2026-09-23 16:36 UTC | 无 lipo 信息 | 高（事实）/ 无结论 |
| philippremy/crossover-dx12-fix：`reapply-gptk4.sh` 用 `ditto` 把 `gptk4-b2/lib` 覆盖到 `CrossOver.app/.../lib64/apple_gptk/`，目录是 `wine/x86_64-unix/*.so`（符号链接到 `libd3dshared.dylib`）和 `wine/x86_64-windows/*.dll`，装完后用 `codesign -v` 校验；脚本没有任何 lipo 或架构判断 [1][2] | README 标注 2026-08-23 验证 | 载荷按 x86_64 布局组织 | 中高 |
| Highball issue #85：引擎 `x64-sikarugir10.0_6-r6`，“Apple Software Signing chain verified” [3] | 2026-09-13（issue 开启日期；原表误作 09-19） | x64 引擎 | 中 |
| EndofLineTech/gamekit PR #61：“Wine and D3DMetal remain x86_64” [7] | 2026-09-21 | x86_64 | 中 |
| UTM Triton 博客、d3dmetal-native README：“D3DMetal.framework ships as x86_64 only… every process that loads it must be x86_64” [5][6] | 2026-07-24 | x86_64（GPTK 版本未写） | 中 |
| NotProton PR #4：CrossOver Preview 自带的 D3DMetal 文件（framework、`libd3dshared.dylib`、x86_64-unix `.so`、PE DLL）“is x86_64-only” [55] | 2026-09-25 | CX 自带版本为 x86_64 | 中高 |

**arm64e 问题：** 公开渠道根本没有 arm64 切片，所以 arm64e 问题暂时不存在。补充一个推断 [低]：假如将来 Apple 发布 arm64 版 D3DMetal，第三方的 arm64 Wine 进程要加载的是 **arm64** 切片。只有 arm64e 切片的 dylib，dyld 在非 arm64e 进程里会以 “incompatible architecture” 拒绝加载。所以导入器应当把 `arm64` 和 `arm64e` 分开检查。

**1.2 系统要求**
- **macOS 26 可以运行。** philippremy 的实测环境是 M5 Max、macOS 26.6.2（25G83）、CrossOver 26.3.0.39832，README 声称要求 “macOS 15 Sequoia or newer”。原话：Metal 4 后端 “Irrelevant on macOS 26 — you get the Metal 3 backend no matter what the GPU advertises”[1]。[中高]（单一维护者实测）
- **Metal 4 后端（`D3DM_MTL4`）只在 macOS 27 上生效，而且 beta 2 有 bug。** “只在 27 上生效”有三个第三方来源：philippremy [1]、thetheoryofR/toolkit4（把 “macOS 27.0 beta” 列为 Metal 4 翻译层的前提，用 `D3DM_MTL4=1` 开启）[62]、Highball（只在 macOS 27 上测 MTL4）[3]。Apple 没有文档说明这个门槛 [11]。[中高] Highball 在 M1 Pro + macOS 27.0 上跑 HumanitZ，“ends quietly within a minute, twice”，所以在基础环境里设 `D3DM_MTL4=0`。同一测试中，内存从 1.26 GB 降到 260–700 MB [3]。[中]
- Apple 官方只把 macOS 27 + Xcode 27 列为新 Metal 调试工具（`gpucapture`/`gpudebug`）和 agent 工作流的要求，没有写 D3DMetal 4 的最低系统版本 [11][12]。媒体把 GPTK 4 描述为 “DX12→Metal 4，DX11 仍走 Metal 3，只支持 Apple silicon” [13]。
- **对 Wine 的要求：** gamekit 用 Wine 11.0/11.17 跑 4.0b2 时出现 “worker exceptions despite successful DLL loading”，最后换回 Sikarugir Wine 10.0 rev6 [8]。philippremy 在 CX 26.3 的 Wine 里替换则正常 [1]。这再次说明 D3DMetal 依赖 GPTK/CrossOver 系 Wine 的补丁（05 号报告 §1 的 unwinder 补丁）。[中]
- **开发机：** macOS 26.5 上没有人公开测过；已知可用的最近版本是 26.6.2（26.x 最新，2026-08-17）[1][10]。

**1.3 GPTK 4 版本状态（2026-09-23 之后）**
- Apple Developer Releases 页面在 2026-09 只列出 macOS 27.0（26A428，09-14）、27.2 beta 2（09-21）、Xcode 27（09-14）等，没有 GPTK 或 MSC 条目。GPTK 过去也从不出现在这个页面上，所以这只能算弱证据 [10]。
- 从 09-13（Highball #85 开启）到 09-23（dbc-hbin 发布），Highball、gamekit 和 dbc-hbin 用的都还是 4.0b2；Gcenx 没有发布任何 GPTK 4 构建 [3][4][7][9]。
- **结论：到 2026-09-26 为止，没有发现 GPTK 4 的 RC 或正式版。** [中高] 参照 GPTK 3 的节奏（2025-06 WWDC beta，Gcenx 的正式版构建在 2025-12-05 [9]），推断 GPTK 4 正式版大约在 2026 年 Q4 [低]。

### 2. DXMT `src/d3d12` 的成熟度（Q2）

**2.1 时间线**：d3d12 目录的提交全部来自 Feifan He（3Shain）。最早的 d3d12 提交出现在 2026-07-01，由 9 个 PR 分批合入 [14][19]：

| PR | 创建 → 合入 | 规模 | 说明 |
|---|---|---|---|
| #180 part 1 | 07-01 → 07-06 | +5801/−4 | “barely enough for some simple demos. DO NOT USE.” |
| #182 part 2 | 07-07 → 07-08 | +1168/−28 | “Enough for *Animal Well* to work, but still, DO NOT USE.” |
| #198/#199/#203 part 3–5 | 07-28 → 08 月 | — | FEATURE_LEVELS、FORMAT_INFO 等 |
| #205 part 6 | 08-18 → 08-24 | +896/−95 | |
| #207 part 7 | 08-25 → 08-29 | +590/−243 | |
| #210 part 8 | 09-01 → 09-03 | +476/−46 | 独占全屏模拟、Present sync interval |
| #212 part 9 | 09-09 → 09-13 | +876/−268 | “I know something actually works, but it's not ready, yet.” |

到 2026-09-17，`src/d3d12` 共 28 个文件，约 372 KB 源码，其中 `d3d12_command_list.cpp` 63 KB、`d3d12_device.cpp` 49 KB [15]。另一个外部贡献者提交的 PR #158（“experimental D3D12→Metal”，约 1 万行，声称支持 SM6.5 DXIL）在 48 分钟内被关闭，没有合入 [20]。[高]

**2.2 功能矩阵**（读自 main 源码 [16][17][18]）[高]

| 项目 | 现状 |
|---|---|
| 着色器 | 只接受 DXBC（经 airconv）；PSO 里如果出现 DXIL 块，共享的着色器初始化路径 `MTLD3D12PipelineState::InitializeShader` 直接 `return E_NOTIMPL`（约第 244 行；不是字面上在 `CreateGraphicsPipelineState` 里，但效果相同）；`HighestShaderModel = D3D_SHADER_MODEL_5_1` |
| 管线阶段 | 只支持 VS+PS。GS、HS/DS、`StreamOutput.NumEntries` 都返回 `E_NOTIMPL`（日志分别是 “GS not supported”、“Tess not supported”、“SO not supported”） |
| Feature level | `MaxSupportedFeatureLevel = min(max, 11_1)` |
| 资源绑定 | `ResourceBindingTier_2`、`ResourceHeapTier_2`；placed resource（f1512a5，09-09）；`CreateReservedResource` 返回 `E_NOTIMPL`，`TiledResourcesTier` 为 NOT_SUPPORTED |
| Root signature | 1.0 和 1.1 都支持：root constants、root CBV/SRV/UAV、descriptor table、static sampler；映射到 Metal argument buffer（按 qword 偏移布局） |
| 其他 | `ROVsSupported = TRUE`；`ConservativeRasterization`、`DepthBoundsTest`、`Int64ShaderOps`、`WaveOps`、`EnhancedBarriers` 都是 FALSE/NOT_SUPPORTED；Mesh 和 SamplerFeedback 不支持；`D3D12_FEATURE_D3D12_OPTIONS5`（DXR）走 “unhandled feature” 分支；ExecuteIndirect 的 command signature 已有实现 |

**2.3 可用性和作者的态度**：用户在 issue #194（2026-07-15）问怎么测试，3Shain 的回复只有 “**Don't** (at the moment)” [21]。作者唯一一次公开的优先级表态是 2024-03-11 的 “DX11 > DX12 (SM5.1) = DX10 > DX9 > DX12 (SM6.0+) > x86”[22]。**没有找到 D3D12 或 1.0 的时间表** [高]。

**2.4 对 ARM64 的意义**：`Ci/arm64x`（PR #209）在 2026-09-17 合入，同一天还加了 `-marm64x` 和 ARM64 的 stb_image SIMD 规避 [24]。NotProton issue #3 表明 DXMT 已经能在 CrossOver FEX 构建（arm64 Wine）上工作 [56]。不过 UTM 称 x86_64 的 D3DMetal 在 Rosetta 下的表现 “still outperforms DXMT running on native ARM64”（DX11 FireStrike，没有给数字）[6]。

**2.5 集成方面的警告**：Gcenx 在 #194 里说，想给上游 Wine 加 DXMT 接口的尝试 “failed to go anywhere”，这套接口在 wine-11.12 坏掉，到 11.13 “became even more broken”[21]。Cider 的 Wine 分支必须自己维护 04 号报告 §3 列出的 `macdrv_functions` 符号表。

**评估**：DXMT d3d12 目前只能覆盖“FL11 + SM5.1 + 不用 GS/Tess/SO”的早期或独立 DX12 游戏，DXIL（SM6）完全不支持，而现代 UE4/UE5 的 DX12 路径都是 DXIL。所以 2027 年上半年之前，它**不能**作为通用 DX12 后端，只适合作为 Engine A 的补位选项，按游戏白名单启用。[中]

### 3. XDC 2026 的 KosmicKrisp 演讲与 Mesa MR（Q3）

- **演讲**：“KosmicKrisp production ready!”，讲者 Aitor Camacho Larrondo（LunarG），**2026-09-29 16:25（America/Toronto）**，时长 20 分钟（indico JSON 导出里是 2026-09-29 20:25 UTC，即 16:25 EDT，内部贡献 id 为 18；URL 里的 538 未经二次核对）[70]。摘要只有 “status update on features, performance and developer tips and tricks” [25][26]。**演讲还没举行，对 GS、XFB、fillModeNonSolid、sparse、RT、mesh 会讲什么，目前没有任何可引用的信息。** 需要在 2026-09-30 之后看录像或幻灯片。
- **Vulkan 1.4 一致性认证**（2026-09-25 公告）：新增 “Tesselation, Robustness2, and Multi-draw”，性能相比 1.3 认证时 “tripled”，这一版 “will be available… in the Vulkan SDK releasing September 29”。公告没提 GS、XFB、sparse 或 RT [27]。[高]
- **Mesa main 的实际状态**（镜像 `chaotic-cx/mesa-mirror`，并已通过 freedesktop GitLab REST API 与上游 main 核对：上游 `kk_physical_device.c` 与镜像逐字节一致；最近一次 kk 提交是 ae4821fd “kk: Raise max samplers to table size for M3+”，squidbus 2026-09-21 编写，2026-09-22 由 Marge 合入，MR !44585）[28][30][63][64]：
  - `kk_physical_device.c`：Vulkan 1.0 特性里**没有** `geometryShader` 和 `fillModeNonSolid`（有 `tessellationShader` 和 `shaderTessellationAndGeometryPointSize`）；扩展表里**没有** `.EXT_transform_feedback`。但 XFB properties 已经写好（第 884–894 行）：`maxTransformFeedbackStreams = 4`、`maxTransformFeedbackBuffers = 4`、`transformFeedbackQueries = true`、`transformFeedbackDraw = true`、`transformFeedbackRasterizationStreamSelect = true`，另有 `maxGeometryOutputVertices = 1024` 等，这些值与 Honeykrisp 一致，看起来是在铺路 [28][44]。[高]
  - sparse：只有 `sparseAddressSpaceSize = KK_SPARSE_ADDR_SPACE_SIZE`（1<<39），没有 `sparseBinding` 或 `sparseResidency*` 特性 [28][29]。
  - main 里 mesh、ray query、acceleration structure、descriptor buffer/heap 都**没有** [28]。RT 另有一个未合入的 Draft MR（见下条）。
- **MR**（本节原先写“网页和 API 都被 Anubis 拦截，无法确认有没有进行中的 MR”，经事实核查更正）：gitlab.freedesktop.org 的**网页**确实被 Anubis 拦截（错误码 9e4edb5b6b850c41），但 **REST API**（`gitlab.freedesktop.org/api/v4/projects/176/...`）不经过 Anubis，可以直接查询 [64][65][66][67]。截至 2026-09-27：
  - **没有任何已合并或 open 的 Mesa MR 给 kk 加 geometry shader 或 `VK_EXT_transform_feedback`。** 用 `search=transform feedback` 查到的 open MR 属于其他驱动（pvr !44385、panvk !43359）[67]。[高]
  - 带 KosmicKrisp 标签的 open MR：!44565、!44222、!44221、!44095、!39602、!39186，以及 Draft !43303 [65]。
  - **Draft MR !43303** “kk: Experimental support VK_KHR_acceleration_structure VK_KHR_ray_query VK_KHR_ray_tracing”：jarrettsjohnson 于 2026-07-29 开启，最后更新 2026-08-11，用 Khronos / Sascha Willems 示例和 PyMOL 测试过 [66]。所以 RT 不是“完全没有”，而是有一条未合入的实验路径。[高]（MR 状态）/ [低]（合入时间）
  - 树外补丁：squidbus 在 `shadexternals/mesa` 的 “kk: Add sparse buffer binding”（2026-09-20，供 shadPS4 使用）[35][36]。
- **节奏参考**：LunarG 在 2026-02 的 Vulkanised 路线图里把 “Tessellation/Geometry” 列为 3–6 个月内的事项（见 05 号报告 [3]）。结果：Mesa 26.1（2026-05-06）完成了 clc 接入和 “Rework draw recording for easier addition of stages like tessellation”；Mesa 26.2（2026-08-05）完成了 “Implement tessellation” 和 “Rework shader compilation to handle more than 2 stages”，同一版本还有 “kk: Move to Metal4 command encoding” 和 “kk: Disable workarounds 1-6 in macOS 27” [32][33]。GS 已经超出那张路线图的 6 个月窗口。Mesa 26.3 的 rc1 是 **2026-10-14**，rc2 10-21、rc3 10-28，rc4 或正式版在 2026-11-04 [34]。由于截至 09-27 连 GS/XFB 的 MR 都没有，它们基本不可能进入 26.3 [推断]。

### 4. XFB 模拟有多难：Honeykrisp / libpoly 的做法与 KosmicKrisp 的现状（Q4）

**4.1 vkd3d-proton 的硬检查**（master，`libs/vkd3d/device.c`，该文件最近提交 2026-09-22，含 7878670 “Enable EXT_shader_atomic_float” 和 cbb73c1；两项检查都在 `vkd3d_init_device_caps()`（第 2427 行起）里，XFB 在第 2479 行，single-texel 在第 2485–2497 行）[37][72] [高]：

```c
if (!physical_device_info->xfb_properties.transformFeedbackQueries)
{   ERR("Lacking support for transform feedback.\n"); return E_INVALIDARG; }
...
if (!single_storage_texel || !single_uniform_texel)
{   ERR("Lacking support for single texel alignment.\n"); return E_INVALIDARG; }
```

`xfb_properties` 只在驱动暴露了 `EXT_transform_feedback` 时才会挂进 `properties2` 链。所以 KosmicKrisp 虽然在 properties 里写了 `transformFeedbackQueries = true`，vkd3d-proton 读到的仍是 0。其他硬性要求 KosmicKrisp 都满足：vertex attribute divisor、`samplerMirrorClampToEdge`、robustness2（含 `robustImageAccess2` 和 `nullDescriptor`）、`shaderDrawParameters`、push descriptor、maintenance5/6、descriptorIndexing；`KK_MAX_DESCRIPTORS = 1<<20`，达到 README 要求的 100 万 [28][29][38]。

**4.2 第二个硬阻塞：single-texel 对齐。** KosmicKrisp 在 Vulkan 1.3 properties 里写的是 `storage/uniformTexelBufferOffsetAlignmentBytes = KK_MIN_TEXEL_BUFFER_ALIGNMENT (16)` 和 `…SingleTexelAlignment = false` [28][29]。Honeykrisp 两项都是 `true` [44]。本机 M3 / macOS 26.5 的探针结果：`minimumTextureBufferAlignment(for:)` 在 r8、rg8、r16F、rgba8、r32F、r32U、rg32F、rgba16F、rgba32F 上**全部返回 16 字节**，所以只有 rgba32F 满足 single-texel [49]。MoltenVK 用同样的逻辑算出 `false` [47]。结论：这个限制来自 Metal，只能在驱动里补，做法是把描述符里 16 字节对齐之后剩下的元素偏移带进着色器、在寻址时加上去（`nir` lowering）。D3D12 的 typed buffer view 允许任意 `FirstElement`，所以这不是理论问题。[高]（修法为 [低]）

**4.3 libpoly：现成的 GS+XFB 模拟**（MIT）[40][41][42] [高]
- 2025-10-06 的 “poly: Migrate AGX's GS/TESS emulation to common code” 把 Asahi 的实现迁出为通用库；2026-01 起又为 panfrost 抽出 passthrough GS。`poly_nir_lower_gs.c` 共 1,504 行，另有 `cl/geometry.cl`（11.5 KB）。
- 机制：GS 改写成 compute。先由“geometry count shader”统计每个图元发出的顶点、图元和 XFB 图元数，prefix sum 之后，真正的 GS 写出索引缓冲和 XFB 数据；1×1×1 的 “pre-GS” kernel 生成 indirect draw，同时更新 XFB offset 和计数器；最后用“GS rasterization shader”作为硬件 VS 光栅化。能静态推出拓扑时会降级为静态索引，省掉动态分配。`poly_gs_info` 里有 `xfb`、`prefix_sum`、`multistream` 字段；`poly_passthrough_gs_key` 带着 `nir_xfb_info`，**在没有应用 GS 时，XFB 靠插入一个 passthrough GS 实现**。
- Honeykrisp 的接法 [43]：`hk_handle_passthrough_gs()` 在 VS 有 XFB 输出时自动绑定 passthrough GS；XFB 的 API（Bind/Begin/End 加计数器拷贝 kernel）约 100 行；`VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT` 和 `PRIMITIVES_GENERATED` 由 GS 参数里的计数器回写；`hk_CmdDrawIndirectByteCountEXT` 仍是 `UNREACHABLE("TODO")`（poly 在 2026-02-04 已补上 DrawIndirectByteCount 支持）。
- **KosmicKrisp 已经具备的部分** [31]：`kk_shader.c` 引用了 `poly/nir/poly_nir.h`，对 VS 调 `poly_nir_lower_vs_before_gs`、对 TCS 调 `poly_nir_lower_tcs`（注释写着 “When using poly to emulate tessellation…”）；`kk_cmd_draw.c` 里有 `poly_heap`、`poly_vertex_params` 和 `kk_unroll_geometry`。也就是说，**“VS 作为 compute 运行、poly 堆、pre-gfx compute encoder 依赖”这些基础设施已经在曲面细分里用上了**，GS 这一段没接线。
- **Metal 侧的额外限制** [48]：`MTLTriangleFillMode` 只有 `Fill` 和 `Lines`（MacOSX26.5.sdk 头文件；`MTL4RenderCommandEncoder` 也有 `setTriangleFillMode:`）。Vulkan 的 `fillModeNonSolid` 要求同时支持 POINT 和 LINE，POINT 得靠 poly 或 unroll 模拟，这大概就是 KosmicKrisp 没暴露它的原因 [推断]。D3D12 只需要 WIREFRAME，vkd3d-proton 把它映射为 `VK_POLYGON_MODE_LINE` [37]。

**4.4 难度估计**（[低]，按熟悉 Mesa 的工程师计）：GS 接线加 XFB（含 passthrough、查询、Begin/End、ByteCount）约 **8–16 人周**，其中大头是 CTS 调试和 Metal 4 的 encoder 依赖。single-texel 对齐模拟约 **2–4 人周**。POINT 填充模式在 GS 完成后约 **1–2 人周**。参照点：LunarG 从铺好基础设施（26.1）到交付曲面细分（26.2）用了一个 Mesa 周期，约 3 个月。

### 5. Metal Shader Converter EULA §2.B（Q5）

- **本次没有拿到原文。** MSC 4.0 beta 的官方页面只写了要求：工具需要 macOS 13 + Xcode 15，生成的库在运行时需要 Argument Buffers Tier 2 和 macOS 14；支持 SM6.0–6.6。页面上**没有**许可证文本 [52]。EULA 只在安装包里，按本次规则不能下载。[高]
- 二手转述（wmarti/metal-shader-converter，Xenia 使用）：“Apple's EULA for Metal Shader Converter allows distributing the dynamic libraries **solely for shader conversion** (see Section 2.B)… must be used on Apple-branded hardware. Keep the bundled license/acknowledgements alongside the copied files.” [53] [中低]。另有第三方称 `metal_irconverter_runtime.h` 这类运行时头文件是 Apache-2.0 [54] [中低]。
- **工程决策**（不做法律分析）：D3D12 翻译层在运行时把 DXIL 转成 Metal IR，从字面看属于 “shader conversion”。但原文没有核对，也不知道有没有别的限制条款。所以 Cider 的**默认路径不依赖 `libmetalirconverter`**。将来如果自研 D3D12→Metal 层，MSC 只作为“用户自装”的可选插件，Cider 从 `/usr/local/lib` 探测；开源着色器链（dxil-spirv → SPIR-V → KosmicKrisp NIR→MSL）作为基线。拿到安装包后，要把 §2.B 原文逐字存档到 `docs/licenses/` 再评估。

### 6. CrossOver 27 的 arm64 D3DMetal 与 NotProton PR #4（Q6）

- **CodeWeavers 的公开表态**：2026-06-11 宣布 CrossOver 27 只支持 Apple silicon 和 Sonoma+，不再支持 32 位 bottle [60]。2026-07-31 的 ARM64 预览版写明 “No D3DMetal in this build. Direct3D 12 support coming soon.”，正式版 “penciled in for… early 2027” [59]。codeweavers.com 对抓取返回 403，内容来自 AppleInsider 转引。[中] 注意这句话描述的是 7 月的预览版，而下面 PR #4 描述的是之后的 Preview 2026082 构建。
- **截至 2026-09-26，CrossOver 正式版仍是 26.3.0（2026-07-21）**，macOS 27 发布后没有出新版 [61]。[高]
- **NotProton PR #4**（LaganYT，2026-09-25，截至 09-27 仍 open；维护者评论 “I will review this and merge it in today”）[55][71]：
  - FEX 构建的 `cxcompatdb.so` “only treats D3DMetal as usable when `CX_ENABLE_ARM_D3DM=1`, and logs `d3dmetal was set as the graphics backend but it is unusable` otherwise”；diff 注释说它会 “quietly falls back to another backend”。这是 PR 作者对二进制行为的解读，不是 CodeWeavers 的文档 [中]。PR 本身没写 CrossOver 版本号，“Preview 2026082” 这个对应关系来自 NotProton README [57]。
  - 即使设了这个标志也没用，因为 runner 里所有 D3DMetal 文件都是 x86_64，而 FEX 构建的 Wine 是原生 arm64 进程。
  - 检测方法：`${wine_unix##*/} = aarch64-unix` 并且 `CX_GRAPHICS_BACKEND=d3dmetal`；每个 prefix 只弹一次提示（marker 文件 `notproton-d3dmetal-fex-warned`），让用户改用 Rosetta 构建或 DXMT/DXVK。
- issue #3 的讨论补充了两点 [56]：FEX 构建 “has both the new Apple Silicon Wine and the older Intel Wine inside it. Older bottles keep running on the Intel side, and they can still use D3DMetal”；NotProton 维护者计划在 Steam 里分别列出 “Rosetta” 和 “FEX” 两个兼容工具，选 FEX 时隐藏 D3DM 选项。NotProton 当前的目标版本是 CrossOver Preview 2026082 [57]。
- **推断** [低]：`CX_ENABLE_ARM_D3DM` 说明 CodeWeavers 的兼容库里已经有“arm64 D3DMetal”这条分支，最可能的解释是 Apple 在按双方协议准备 arm64 版 D3DMetal（先私下提供给 CodeWeavers，再进入公开 GPTK）。另一种可能是 DX12 改由 DXMT d3d12 承担：DXMT 版权署名是 “Feifan He for CodeWeavers”，d3d12 开工时间（2026-07-01）也正好在 ARM64 预览版前一个月。两种可能不互斥。
- **对 Cider 的启发**：(1) 一个 app 同时内置两套引擎，bottle 绑定架构，这已被证明可行，Cider 的 Engine R/A 应当照此设计；(2) 兼容库必须知道每个后端在每个架构上是否可用；(3) **不能静默回退**，NotProton 的问题就出在静默回退上。

### 7. DX12 决策树（Engine R / Engine A）与触发日期

```
DX12 游戏启动
├─ Engine R（x86_64 Wine + Rosetta；寿命：macOS ≤27 确定，macOS 28 待定）
│  ├─ 有可用的 GPTK D3DMetal？
│  │  ├─ 是（默认来自用户导入；可选来自 Cider 下载的原样 framework，见 P0-1）→ 按 macOS 版本选：
│  │  │   ├─ 26.x：默认 D3DMetal 3.0；兼容库里标记的游戏（例如 UE ≥5.6 的 sampler heap 断言）改用 4.0b2（只走 Metal 3 后端）
│  │  │   └─ 27.x：默认 4.0b2 并设 D3DM_MTL4=0；逐游戏白名单开启 MTL4；GPTK 4 正式版出来后重新评估默认值
│  │  └─ 否 → ① 游戏支持 -dx11 就用 DXMT；② 实验项：Wine 内置 d3d12（vkd3d 2.1）+ KosmicKrisp，最高只到 FL11_0，不支持 GS/SO，只适合最低 FL 要求为 11_0 的游戏（§8 说明）；③ 明确提示“需要 D3DMetal”
│  └─ [T5] macOS 28 上 Rosetta 子集不覆盖 Wine 或 D3DMetal → Engine R 冻结在 ≤27
└─ Engine A（arm64 Wine + ARM64EC + FEX）
   ├─ 有 arm64 切片的 D3DMetal？（导入时用 lipo 区分 arm64 和 arm64e）
   │  ├─ 是 → 实现 arm64 版 unixlib 胶水（对应 libd3dshared）→ 作为默认 DX12
   │  └─ 否 ↓
   ├─ Rosetta 可用（≤ macOS 27）→ 这个游戏的 bottle 路由到 Engine R（和 CrossOver FEX 构建一样）
   ├─ KosmicKrisp 已解决 B1 和 B2（XFB + single-texel）→ vkd3d-proton（arm64x 构建）+ KosmicKrisp
   │     未解决时：内部可以用打了补丁的 vkd3d-proton 做测试，但不能发布
   ├─ FL11 + SM5.1 且在白名单内 → DXMT d3d12（等作者去掉 DO NOT USE 之后）
   └─ 都不行 → DXMT 跑游戏的 DX11 模式，或标记为不支持
```

| 触发点 | 日期 | 动作 | 依据 |
|---|---|---|---|
| T0 | 2026-09-29 / 09-30 | 看 XDC 演讲录像和新 Vulkan SDK；按 §8 更新 KosmicKrisp 状态；用 GitLab REST API 定期查 kk 的 MR（GS/XFB 新 MR、Draft !43303 的进展） | [25][27][65][66] |
| T1 | 2026-10-14 | Mesa 26.3-rc1：截至 09-27 连 GS/XFB 的 MR 都没有，基本确定进不了 26.3，最早只能等 27.0（推断 2027-02 前后） | [34][67] |
| T2 | 预计 2026 Q4（推断） | GPTK 4 正式版：用 lipo 查架构、核对许可证；在 26.5/26.6 和 27.x 上各测一遍，再定默认值 | [9] |
| T3 | 2027 年初 | CrossOver 27 正式版：看 arm64 构建带不带 D3DMetal、默认有没有开 `CX_ENABLE_ARM_D3DM`；读它按 LGPL 公开的源码，找 arm64 D3DM 胶水 | [55][59] |
| T4 | 2027-06（WWDC27） | GPTK 5 有没有 arm64 切片；macOS 28 beta 的 Rosetta 子集能不能跑 Wine + D3DMetal | 06 号报告 |
| T5 | 2027-09（macOS 28） | 决定 Engine R 的 DX12 是否退役 | 06 号报告 |

### 8. 完全开源路径（vkd3d-proton + KosmicKrisp）阻塞项量化清单

| # | 阻塞项 | 类型 | 现状证据 | 影响 | 修复路径 / 工作量 [低] |
|---|---|---|---|---|---|
| B1 | `VK_EXT_transform_feedback`（`transformFeedbackQueries`） | **硬，设备创建失败** | KosmicKrisp 只填了 props，没有 ext [28]；vkd3d-proton 在约第 2479 行检查 [37]；截至 2026-09-27 上游没有相关 MR [67] | 100% 的 D3D12 游戏 | 用 libpoly 的 passthrough GS + 查询 + Begin/End；与 B3 一起做，8–16 人周 |
| B2 | Texel buffer single-texel 对齐 | **硬，设备创建失败** | KosmicKrisp 报 `false`/16 B [28][29]；M3 实测对齐都是 16 B [49]；vkd3d-proton 在约第 2494 行检查 [37] | 100% | KosmicKrisp 在着色器侧补偏移，2–4 人周。**Cider 最适合先接这一项** |
| B3 | `geometryShader` | 功能 | 未暴露 [28]；截至 2026-09-27 上游没有相关 MR [65][67] | 含 GS 的 PSO 创建失败 | 同 B1 |
| B4 | Sparse / Tiled（TR Tier ≥2 才会报 FL 12_0） | 功能 | KosmicKrisp 没有 sparse [28]；vkd3d-proton 的 FL 12_0 要求先满足 FL11_1（OutputMergerLogicOp、`vertexPipelineStoresAndAtomics`、UAV 槽数，KosmicKrisp 都满足），再同时满足 `TiledResourcesTier ≥ 2`、`ResourceBindingTier ≥ 2` 和 `TypedUAVLoadAdditionalFormats` [37] | 要求 FL12_0 的游戏拒绝启动；可以用 `VKD3D_FEATURE_LEVEL=12_0` 骗过去（它强制 TR Tier ≥2、Binding Tier ≥2、TypedUAVLoadAdditionalFormats 和 SM ≥6_0，然后打印 “Overriding feature level”），但这只改上报的能力，不增加 sparse 功能，用到 tiled 的游戏会坏 | 树外已有 buffer sparse 补丁 [35]；image sparse 走 MTLHeap（M3 tile 16 KB [49]），6–12 人周 |
| B5 | DXR（ray query / AS） | 功能 | KosmicKrisp main 没有 [28]；但有 Draft MR !43303（标题为 “kk: Experimental support VK_KHR_acceleration_structure VK_KHR_ray_query VK_KHR_ray_tracing”，2026-07-29 开启，08-11 最后更新）[66]；M3 `supportsRaytracing=true` [49] | 强制光追的游戏 | 优先跟进或协助 !43303 合入；如果它停滞，从零做仍是 20 人周以上；也可参考 MoltenVK PR #2771（05 号报告） |
| B6 | Mesh / Amplification | 功能 | 没有 `EXT_mesh_shader` [28] | 要求 DX12 Ultimate 的游戏 | Metal 原生支持 mesh，8–16 人周 |
| B7 | FL 12_1（ROV + 保守光栅化） | 功能 | KosmicKrisp 没有 interlock 和保守光栅化 [28]；M3 支持 ROG [49] | 少数游戏 | ROV 2–4 人周；保守光栅化 Metal 没有对应功能 |
| B8 | `fillModeNonSolid` | 功能 | 未暴露；Metal 没有 POINT 模式 [48] | 线框渲染（少见） | GS 完成后 1–2 人周 |
| B9 | `EXT_image_view_min_lod` | 推荐项 | 要设 `MESA_KK_EXPERIMENTAL` [28] | 采样精度 | 默认开启前需要过 CTS，约 1 人周 |
| B10 | descriptor buffer/heap | 性能 | KosmicKrisp 没有 [28]；可退回 mutable descriptor 路径 | CPU 开销 | 8 人周以上 |
| B11 | `shaderFloat64` | 功能 | KosmicKrisp 为 false | 极少数游戏 | 暂不处理 |
| B12 | 两层着色器翻译（DXIL→SPIR-V→NIR→MSL） | 性能/兼容 | 结构性问题 | 编译卡顿 | 管线缓存和预编译 |

说明：B1 和 B2 修完之后，上游 vkd3d-proton **不打补丁**就能在 KosmicKrisp 上创建设备。B3 和 B4 决定能覆盖多少主流 AAA 游戏。B5 和 B6 决定新作能不能跑。vkd3d-proton 已经支持 `--build-arm64x`，Engine A 可以直接用 [39]。

**上游 vkd3d（Wine 内置 d3d12）在 KosmicKrisp 上只是 FL11_0 路径（经事实核查更正）。** 原先的判断是它“很可能是今天就能在 KosmicKrisp 上跑起来的开源 DX12 路径”，这个判断过于乐观。上游 vkd3d master 的 `required_device_extensions[]` 确实只有 `KHR_maintenance1`、`KHR_maintenance2` 和 `KHR_shader_draw_parameters`（第 73 行），缺 `EXT_transform_feedback` 只打印 “Stream output is not supported” 警告 [50]。但它的 `CHECK_FEATURE` 列表里有 `geometryShader` 和 `pipelineStatisticsQuery`，KosmicKrisp 两项都没暴露 [28]。缺任何一项都会把 `have_11_0` 置为 false（第 1431–1468 行），而 FL11_1 需要 `have_11_0`（第 1491 行），FL12_0 还需要 `TiledResourcesTier ≥ 2`（第 1499 行）。所以在 KosmicKrisp 上，上游 vkd3d **最多上报 D3D_FEATURE_LEVEL_11_0**。应用要求的最低 FL 为 11_1 或 12_0 时，设备创建会在 `max_feature_level < minimum_feature_level` 检查处返回 `E_INVALIDARG`（第 1935 行）；含 GS 或 stream output 的 PSO 也会失败 [50]。结论：它是“FL11_0、无 GS/SO”的有限路径，作为 DX12 兜底的价值远低于原先的判断。[中]（源码推断，未实测）对最低只要求 FL11_0、又不用 GS/SO 的游戏仍可能可用 [低]。vkd3d 2.1 版本存在（ANNOUNCE），tag 日期 2026-08-24 未经事实核查者二次核实 [51]。

## 对 Cider 的启示与建议

**P0（本季度）**
1. **把 Engine R 的 DX12 定为 D3DMetal。默认由用户从 GPTK 导入；Cider 下载原样 framework 作为可选来源。**（核查更正：原先的前提是“许可证要求用户自行导入”，这个前提不成立 [68][69]。）只记一条会影响设计的许可证事实：GPTK 4.0b2 附带的许可证（EA18380，2023-08-17，目前只看过 dbc-hbin 转存的第三方副本）允许在非商业、只用于 Apple 硬件、不修改（§2A(iii)/§2C/§2D）的前提下单独分发完整的 `D3DMetal.framework`。dbc-hbin 和 philippremy 已经这样分发，philippremy 在仓库里直接带了 `gptk4-b2/lib` [1][4][68][69]。因此，用户导入不是许可证逼出来的设计，而是 00 号综述认定的默认决策。保留它的理由是与 Apple 的 beta 节奏解耦，也方便用户换版本。工程约束：
   - 默认来源是用户导入，与 00 号综述一致。可选来源是 Cider 下载（或随附）的**原样** `D3DMetal.framework`。两种来源共用同一套校验和版本存储。
   - 无论哪种来源，Cider **都不得修改或给 framework 打补丁**，只能校验后原样加载。
   - 需要从 Apple 的 GPTK DMG 核对许可证原件（见未解问题 8）；GPTK 正式版如果改了许可证，要重新核对。

   导入器（以及下载器）需要：
   - 用 `lipo -archs` 分别记录 `x86_64`、`arm64`、`arm64e`；
   - 用 `codesign -v` 校验签名；
   - 读取 `D3DMetal.framework/Resources/Info.plist` 的版本；
   - 用 `ditto` 保留 `x86_64-unix` 下的符号链接 [1][2]；
   - 同时保存 3.0 和 4.0b2 两个版本，逐游戏选择。

   在 macOS 26 上默认用 3.0。在 macOS 27 上默认 `D3DM_MTL4=0` [3]。
2. **在开发机上实测 4.0b2。** 分两步：先在 26.5 上测，再升级到 26.6.2 测（目前只有 26.6.2 有人测过）[1][10]。另外准备一个 macOS 27 的外置测试卷，用来验证 MTL4。测试时注意 gamekit 在 Wine 11 上遇到的 worker exceptions [8]。
3. **后端可用性矩阵和可见的回退。** 以（引擎架构、macOS 版本、GPU family、后端版本）为键判断后端能不能用，回退时必须在 UI 和日志里说明原因。不要重复 CrossOver FEX 构建那种静默回退 [55]。
4. **向 KosmicKrisp 上游贡献 B2（single-texel 对齐模拟）。** 这一项小、边界清晰、不容易与 LunarG 撞车，还能让 Cider 在 Mesa 里建立信用。T0 之后找 LunarG，确认 GS/XFB 由谁来做。截至 2026-09-27，上游没有任何 kk GS/XFB MR（REST API 查询）[65][67]，B1/B3 目前无人认领。如果 LunarG 没有排期，Cider 应在 B2 之后接手 B1（用 libpoly passthrough GS 实现 XFB）。跟踪用 GitLab REST API（`/api/v4/projects/176/merge_requests?labels=KosmicKrisp`），不受 Anubis 影响，可以放进 CI 定时任务。

**P1（2026 Q4 – 2027 Q1）**
5. **内部 bring-up 分支。** 给 vkd3d-proton 打补丁，跳过 XFB 和 single-texel 两项检查，在 M3 上跑 vkd3d-proton 自带测试加 20–30 款 DX12 游戏，统计失败原因在 B3–B8 之间的分布，按结果排上游贡献顺序。这个分支**不能发布**。
6. **实测 Wine 内置 d3d12（vkd3d 2.1）+ KosmicKrisp，但降低预期和优先级。** 源码显示它在 KosmicKrisp 上最多上报 FL11_0，不支持 GS 和 stream output（§8 说明）[50][28]。实测只需确认三件事：设备能否创建、上报的 FL/SM 是多少、哪些游戏最低只要求 FL11_0。它只作为没有 D3DMetal 时的最低兜底，只对白名单里的 FL11_0 游戏开放，不当作通用 DX12 后端。
7. **Engine A 的 bottle 路由。** 仿照 CrossOver FEX 构建同时带两套 Wine 的做法 [56]，把 DX12 游戏自动路由到 Engine R 的 bottle。UI 上标明它依赖 Rosetta。
8. **跟踪 DXMT d3d12。** 作者去掉 “DO NOT USE” 之后，只对 FL11 + SM5.1 的白名单游戏开放 [16][17]。

**P2（2027）**
9. 在 T3 和 T4 检查 arm64 D3DMetal。一旦出现，就在 Engine A 里实现对应 `libd3dshared` 的 arm64 unixlib 胶水，可以参考 CrossOver 27 按 LGPL 公开的 Wine 源码。
10. **不推荐**“跨进程 D3DMetal”，即 arm64 Wine 把命令发给一个在 Rosetta 下运行的 x86_64 宿主（`d3dmetal-native` 已提供 D3D12 入口和跨进程共享资源 [5]，UTM 用同样的思路做了 DX11 [6]）。原因有三：要远程化整个 D3D12 接口，工作量巨大；仍然依赖 Rosetta；解决不了 macOS 28 的问题。只在其他路径全部失败时作为研究项。
11. MSC 只作为未来自研 D3D12→Metal 层的“用户自装”插件（§5）。

## 风险

1. **Apple 可能一直不公开 arm64 D3DMetal**，或者只通过协议提供给 CodeWeavers。那样 Engine A 的 DX12 就只能靠开源路径，而开源路径最早 2027 年上半年才能起步（推断）。
2. **Rosetta 在 macOS 28 上的子集不覆盖 Wine 或 D3DMetal**（06 号报告）。那样 Engine R 的 DX12 在 2027-09 之后就会断档。
3. **KosmicKrisp 的 GS/XFB 没有排期迹象**：据 REST API 查询，截至 2026-09-27 上游没有任何相关 MR（已合并或 open）[65][67]，XDC 演讲还没举行。如果 GS 被 LunarG 排到后面，B1 就要靠社区（包括 Cider）完成。RT 有 Draft MR !43303，但它从 2026-08-11 起没有更新，合入时间未知 [66]。
4. **D3DMetal 4 仍是 beta**：MTL4 有时序 bug [3]；Wine 11 系有兼容问题 [8]；4.0b2 附带的许可证仍是 2023 版（EA18380，2023-08-17）[69]。GPTK 正式版可能改动许可证文本，届时 Cider 的分发方式（随附、下载或用户导入，见 P0-1）要重新核对。
5. **DXMT 与上游 Wine 的接口在 11.12 和 11.13 两次坏掉** [21]，Cider 的 Wine 分支维护成本会上升。
6. **8 GB 开发机的内存压力**：D3DMetal 3.0 在部分游戏上占用超过 1.2 GB，4.0b2 有所改善 [3]；vkd3d-proton 加 KosmicKrisp 两层翻译的内存和编译开销未知。
7. **单一来源风险**：“D3DMetal 4 可在 macOS 26 上运行”目前只有 philippremy 一个维护者的实测 [1]。“MTL4 只在 macOS 27+ 生效”来自三个第三方来源，Apple 没有文档说明 [1][3][62][11]。

## 未解问题

1. 4.0b2 的 `lipo -archs` 实际输出，以及 `D3DMetal.framework` 里有没有 arm64 或 arm64e 切片（需要合法拿到 GPTK 4 DMG 后在本机核对）。
2. XDC 2026（09-29）演讲里有没有提到 GS、XFB、fillModeNonSolid、sparse、RT、mesh 的时间表。（“freedesktop 上有没有未合并的 kk GS/XFB MR”已通过 REST API 回答：截至 09-27 没有 [65][67]。之后需要持续跟踪，同时关注 Draft MR !43303（RT）能否合入 [66]。）
3. `CX_ENABLE_ARM_D3DM` 在 CrossOver 内部对应的是 Apple 私下提供的 arm64 D3DMetal，还是一个占位开关。
4. D3DMetal 4.0b2 在 macOS 26.5（开发机）上能不能用，以及 Metal 3 后端下的性能和内存与 3.0 相比如何。
5. Wine 内置 d3d12（vkd3d 2.1）在 KosmicKrisp 上的实测结果。源码推断最多 FL11_0，不支持 GS/SO（§8 说明）；还需实测设备能否创建、SM 是多少，以及有多少 DX12 游戏最低只要求 FL11_0。
6. MSC EULA §2.B 的原文。
7. GPTK 4 正式版的发布日期，以及它会不会改动许可证文本。
8. GPTK 4.0b2 许可证（EA18380，2023-08-17）目前只看过 dbc-hbin 转存的第三方副本 [69]，需要从 Apple 官方 GPTK DMG 核对原件。

## 参考来源

1. https://raw.githubusercontent.com/philippremy/crossover-dx12-fix/main/README.md — 用 D3DMetal 4.0b2 替换 CrossOver 26.3 自带版本；macOS 26.6.2（25G83）实测；`D3DM_MTL4` 在 26 上无效；2026-08-23 验证
2. https://raw.githubusercontent.com/philippremy/crossover-dx12-fix/main/reapply-gptk4.sh — `x86_64-unix`/`x86_64-windows` 布局、`ditto`、`codesign -v`
3. https://github.com/gauthierpiarrette/highball/issues/85 — 引擎 `x64-sikarugir10.0_6-r6`（issue 2026-09-13 开启；原先误记为 09-19），minMacOS 27 是项目策略，只在 macOS 27 上测 MTL4，MTL4 时序 bug，内存测量
4. https://github.com/dbc-hbin/d3dmetal-redistributable/releases/tag/gptk-4.0b2 （API：https://api.github.com/repos/dbc-hbin/d3dmetal-redistributable/releases）— 4.0b2 zip 23,617,245 B，2026-09-23
5. https://github.com/utmapp/d3dmetal-native — “x86_64 only”，GFXT 宿主接口，D3D11/12 入口，MIT
6. https://blog.getutm.app/2026/introducing-triton-directx-11-driver-for-qemu/ — 2026-07-24；D3DMetal 在 Rosetta 下仍优于 arm64 上的 DXMT
7. https://github.com/EndofLineTech/gamekit/pull/61 — 2026-09-21：“Wine and D3DMetal remain x86_64”
8. https://github.com/EndofLineTech/gamekit/pull/4 — 2026-09-15：Wine 11.0/11.17 出现 worker exceptions，改用 Sikarugir Wine 10.0 r6
9. https://github.com/Gcenx/game-porting-toolkit/releases — 最新 3.0-3（2026-03-03）；3.0 正式版 2025-12-05
10. https://developer.apple.com/news/releases/ — macOS 27.0（26A428，09-14）、26.6.2（25G83）；没有 GPTK 条目
11. https://developer.apple.com/games/game-porting-toolkit/ — GPTK 4 为当前版本，评估环境支持 Metal 4
12. https://github.com/apple/game-porting-toolkit — macOS 27 / Xcode 27 只是调试工具和 agent 工作流的要求
13. https://appleinsider.com/articles/26/06/17/apples-game-porting-toolkit-4-is-a-big-improvement-for-modern-game-coders — DX12→Metal 4，DX11→Metal 3
14. https://api.github.com/repos/3Shain/dxmt/commits?path=src/d3d12 — d3d12 提交史（2026-07 至 09-17）
15. https://api.github.com/repos/3Shain/dxmt/contents/src/d3d12 — 28 个文件及大小
16. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d12/d3d12_device.cpp — CheckFeatureSupport（SM5.1、FL11_1、Tier）
17. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d12/d3d12_pipeline_graphics.cpp — DXIL、GS、Tess、SO 均为 `E_NOTIMPL`
18. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d12/d3d12_root_signature.cpp — root signature 实现
19. https://github.com/3Shain/dxmt/pull/180 、/182 、/205 、/207 、/210 、/212 — D3D12 implementation part 1–9
20. https://github.com/3Shain/dxmt/pull/158 — 外部实验性 D3D12 PR（未合入）
21. https://github.com/3Shain/dxmt/issues/194 — 作者回复 “Don't (at the moment)”；Gcenx 谈 Wine 接口在 11.12/11.13 损坏
22. https://github.com/3Shain/dxmt/discussions/4 — 2024-03-11 的优先级表态
23. https://github.com/3Shain/dxmt/releases — 最新 v0.80（2026-04-23）；release 说明提到从该版起许可证由 MIT 改为 LGPL
24. https://github.com/3Shain/dxmt/pull/209 — Ci/arm64x（2026-09-17 合入）
25. https://www.lunarg.com/lunarg-at-xdc-2026-kosmickrisp-update/ — XDC 演讲 2026-09-29 16:25
26. https://indico.freedesktop.org/event/12/contributions/538/ — XDC 2026 贡献 538（URL 中的 id 未经二次核对；JSON 导出里的内部 id 为 18，见 [70]）
27. https://www.lunarg.com/kosmickrisp-achieves-vulkan-1-4-conformance-on-apple-silicon/ — Vulkan 1.4 一致性认证；SDK 09-29；性能 “tripled”
28. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_physical_device.c — KosmicKrisp 特性、扩展、properties
29. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_private.h — `KK_MAX_DESCRIPTORS`、texel 对齐 16、sparse 地址空间
30. https://api.github.com/repos/chaotic-cx/mesa-mirror/commits?path=src/kosmickrisp — kk 提交，最近一次 2026-09-21
31. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_shader.c （及同目录的 kk_cmd_draw.c）— 用 poly 模拟曲面细分
32. https://docs.mesa3d.org/relnotes/26.2.0.html — 2026-08-05：“kk: Implement tessellation”、“kk: Rework shader compilation to handle more than 2 stages”、“kk: Move to Metal4 command encoding”、“kk: Disable workarounds 1-6 in macOS 27”、Vulkan 1.4
33. https://docs.mesa3d.org/relnotes/26.1.0.html — 2026-05-06：clc、重做 draw recording、texel_buffer_alignment
34. https://docs.mesa3d.org/release-calendar.html — 26.3.0-rc1 在 2026-10-14，rc2 10-21，rc3 10-28，rc4 或 26.3.0 正式版 11-04
35. https://github.com/shadexternals/mesa/commits — “kk: Add sparse buffer binding”（squidbus，2026-09-20，树外）
36. https://github.com/shadexternals/mesa-kosmickrisp/pull/11 — 为 sparse buffer 补丁切换到 fork
37. https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/master/libs/vkd3d/device.c — `vkd3d_init_device_caps()` 中的 XFB 和 single-texel 硬检查；`d3d12_device_caps_init_feature_level()` 的 FL 判定；`d3d12_device_caps_override()` 读取 `VKD3D_FEATURE_LEVEL`
38. https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/master/README.md — 驱动硬性要求
39. https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/master/package-release.sh — `--build-arm64x`
40. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/poly/nir/poly_nir_lower_gs.c — libpoly 的 GS/XFB lowering（1,504 行，MIT）
41. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/poly/nir/poly_nir.h — `poly_gs_info.xfb`、passthrough GS key
42. https://api.github.com/repos/chaotic-cx/mesa-mirror/commits?path=src/poly — 2025-10-06 从 AGX 迁出；panfrost 复用
43. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/asahi/vulkan/hk_cmd_draw.c — Honeykrisp 的 XFB API、passthrough GS、ByteCount 仍为 TODO
44. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/asahi/vulkan/hk_physical_device.c — Honeykrisp 暴露 GS、XFB、fillModeNonSolid、sparse，single-texel 为 true
45. https://asahilinux.org/2024/10/aaa-gaming-on-asahi-linux/ — 用 compute 模拟 GS 和曲面细分；vkd3d-proton 跑通 Cyberpunk 2077
46. https://alyssarosenzweig.ca/blog/asahi-gpu-part-n.html — GS/曲面细分模拟花了一年，设计目标是供其他 Mesa 驱动复用
47. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm — Metal 上 texel 对齐的计算方法
48. 本机 `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/.../Metal.framework/Headers/MTLRenderCommandEncoder.h` 和 `MTL4RenderCommandEncoder.h` — `MTLTriangleFillMode` 只有 Fill 和 Lines
49. 本机 Metal 探针（M3，macOS 26.5 25F71）：所有测试格式的 `minimumTextureBufferAlignment` 都是 16 B；`maxArgumentBufferSamplerCount` 500000；Argument Buffers Tier 2；`supportsRaytracing` true；`sparseTileSizeInBytes` 16384；`areRasterOrderGroupsSupported` true；`supportsFamily(.metal4)` true
50. https://gitlab.winehq.org/wine/vkd3d/-/raw/master/libs/vkd3d/device.c — 上游 vkd3d 强制要求的扩展只有 maintenance1/2 和 shader_draw_parameters；但 `CHECK_FEATURE(geometryShader)` 和 `CHECK_FEATURE(pipelineStatisticsQuery)` 缺失时会把 `have_11_0` 置为 false，从而挡住 FL11_1 及以上；最低 FL 检查返回 `E_INVALIDARG`
51. https://gitlab.winehq.org/wine/vkd3d/-/raw/master/ANNOUNCE — vkd3d 2.1 存在（tag 日期 2026-08-24 未经二次核实；2.0 为 2026-05-21）
52. https://developer.apple.com/metal/shader-converter/ — MSC 4.0 beta；系统要求；页面上没有许可证文本
53. https://github.com/wmarti/metal-shader-converter — 对 EULA §2.B 的二手转述（“solely for shader conversion”）
54. https://github.com/renderbag/plume/pull/112 — 第三方称 MSC 运行时头文件为 Apache-2.0
55. https://github.com/NotProtonNot/NotProton/pull/4 （API：https://api.github.com/repos/NotProtonNot/NotProton/pulls/4）— `CX_ENABLE_ARM_D3DM`、cxcompatdb 日志、静默回退
56. https://github.com/NotProtonNot/NotProton/issues/3 （评论 API：https://api.github.com/repos/NotProtonNot/NotProton/issues/3/comments）— FEX 构建内置两套 Wine；DXMT 在 FEX 构建上可用
57. https://raw.githubusercontent.com/NotProtonNot/NotProton/main/README.md — 目标版本 CrossOver Preview 2026082
58. https://www.codeweavers.com/blog/mjohnson/2026/7/31/crossover-preview-the-right-to-bear-arm64-on-mac — 原文（对抓取返回 403）
59. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — “No D3DMetal in this build. Direct3D 12 support coming soon.”；目标 2027 年初
60. https://appleinsider.com/articles/26/06/11/crossover-a-windows-to-mac-gaming-tool-goes-apple-silicon-only — CrossOver 27 只支持 Apple silicon 和 Sonoma+
61. https://www.codeweavers.com/crossover/changelog — 最新 26.3.0（2026-07-21）
62. https://github.com/thetheoryofR/toolkit4 — 把 “macOS 27.0 beta” 列为 Metal 4 翻译层的前提，用 `D3DM_MTL4=1` 开启（独立于 philippremy 的第二个来源）
63. https://gitlab.freedesktop.org/api/v4/projects/176/repository/files/src%2Fkosmickrisp%2Fvulkan%2Fkk_physical_device.c/raw?ref=main — 上游 Mesa main 的 `kk_physical_device.c`（与 chaotic-cx 镜像逐字节一致）
64. https://gitlab.freedesktop.org/api/v4/projects/176/repository/commits?path=src/kosmickrisp — 上游 kk 提交；最新 ae4821fd（2026-09-21 编写，09-22 合入，MR !44585）
65. https://gitlab.freedesktop.org/api/v4/projects/176/merge_requests?state=opened&labels=KosmicKrisp&per_page=50 — 带 KosmicKrisp 标签的 open MR（!44565、!44222、!44221、!44095、!39602、!39186、Draft !43303），没有 GS/XFB
66. https://gitlab.freedesktop.org/api/v4/projects/176/merge_requests/43303 — Draft “kk: Experimental support VK_KHR_acceleration_structure VK_KHR_ray_query VK_KHR_ray_tracing”（jarrettsjohnson，2026-07-29 开启，08-11 最后更新）
67. https://gitlab.freedesktop.org/api/v4/projects/176/merge_requests?state=opened&search=transform%20feedback — open 的 XFB MR 只属于 pvr（!44385）和 panvk（!43359）
68. https://raw.githubusercontent.com/dbc-hbin/d3dmetal-redistributable/main/README.md — 对 GPTK 许可证 §§2A/2C 的概述；再分发 D3DMetal.framework 的做法
69. https://raw.githubusercontent.com/dbc-hbin/d3dmetal-redistributable/main/License.rtf — GPTK 4.0b2 附带的许可证文本（第三方转存）：EA18380，2023-08-17；§2A(iii)、§2C、§2D
70. https://indico.freedesktop.org/export/event/12.json?detail=contributions — XDC 2026 日程 JSON：“KosmicKrisp production ready!” 2026-09-29 20:25 UTC，20 分钟，内部 id 18
71. https://patch-diff.githubusercontent.com/raw/NotProtonNot/NotProton/pull/4.diff — PR #4 的 diff，注释写明 FEX 构建 “quietly falls back to another backend”
72. https://github.com/HansKristian-Work/vkd3d-proton/commits/master/libs/vkd3d/device.c — `device.c` 最新提交 2026-09-22（7878670 “Enable EXT_shader_atomic_float”、cbb73c1）

## 事实核查记录

核查日期 2026-09-27。共 10 条：7 条确认（其中 1 条附带日期更正），3 条部分正确，没有被推翻或无法核实的条目。下表只记录结论和改动，证据见各条对应的参考来源。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| vkd3d-proton master `device.c`（最近提交 2026-09-22）的 `vkd3d_init_device_caps` 在 `transformFeedbackQueries == 0` 时返回 `E_INVALIDARG`（“Lacking support for transform feedback.”）；storage 和 uniform texel buffer 只要有一项既不满足 `SingleTexelAlignment` 也不满足 `AlignmentBytes == 1`，就返回 `E_INVALIDARG`（“Lacking support for single texel alignment.”） | 确认 | 函数从第 2427 行开始，XFB 检查在第 2479 行，single-texel 在 2485–2497 行；`xfb_properties` 只在 `EXT_transform_feedback` 为真时挂链。其他硬检查（divisor、mirror clamp、robustness2、nullDescriptor、shaderDrawParameters、push descriptor、maintenance5/6）KosmicKrisp 都满足。§4.1 补了函数名、行号和提交号 [37][72] |
| KosmicKrisp（Mesa main，最新 kk 提交 ae4821f，2026-09-21）没有 `EXT_transform_feedback`、`geometryShader`、`fillModeNonSolid`；single-texel 为 false/16 B；XFB properties 已填好 | 确认 | 已用 freedesktop GitLab REST API 与上游核对，与镜像逐字节一致；ae4821fd 于 09-22 由 Marge 合入（MR !44585）。§3 补了上游核对说明和完整的 XFB properties 字段 [28][29][63][64] |
| D3DMetal 4.0b2 替换进 CrossOver 26.3.0.39832 后能在 macOS 26.6.2 上运行（philippremy，2026-08-23），但在 macOS 26 上只走 Metal 3 后端，`D3DM_MTL4` 只在 macOS 27+ 生效 | 确认（附日期更正） | 新增独立来源 toolkit4 [62]。说明 macOS 27 门槛来自第三方说法，Apple 没有文档说明 [11]；“能在 26 上跑”只有一人实测。**更正**：Highball #85 在 2026-09-13 开启，不是 09-19（§1.1 表格、§1.3 和参考来源 [3] 已改） |
| CrossOver Preview 2026082 的 FEX 构建只在 `CX_ENABLE_ARM_D3DM=1` 时把 D3DMetal 当作可用，否则记录日志并静默回退（NotProton PR #4，2026-09-25，open）；CodeWeavers 2026-07-31 称 “No D3DMetal in this build” | 确认（附限定） | 补了三条限定：PR 本身没写 CrossOver 版本，“2026082” 的对应来自 NotProton README [57]；“只在 =1 时可用”是 PR 作者对二进制行为的解读；CodeWeavers 原话只经 AppleInsider 转引，而且描述的是 7 月的预览版。摘要和 §6 已改 [55][57][59][71] |
| DXMT main `src/d3d12`（PR #180–#212，2026-07-01 至 09-13，均标 “DO NOT USE”）：SM 5.1、FL 上限 11_1、TR NOT_SUPPORTED；DXIL、GS、HS/DS、SO 都返回 `E_NOTIMPL`；最新 release v0.80（2026-04-23） | 确认（措辞微调） | DXIL 的拒绝点在共享的 `MTLD3D12PipelineState::InitializeShader` 里（约第 244 行），不是字面上在 `CreateGraphicsPipelineState` 里，效果相同。#180 在 07-06 合入。补充：v0.80 起许可证由 MIT 改为 LGPL。摘要、§2.2 和参考来源 [23] 已改 |
| vkd3d-proton 只有在 TR Tier ≥2、Binding Tier ≥2 和 TypedUAVLoadAdditionalFormats 都满足时才上报 FL12_0；`VKD3D_FEATURE_LEVEL=12_0` 强制这些能力 | 确认（补充细节） | FL12_0 还有 FL11_1 前提（KosmicKrisp 满足）；override 另外强制 SM ≥6_0，只改上报的能力，不增加 sparse 功能。§8 的 B4 已补充 [37] |
| （§3 / 未解问题 2 / 风险 3）freedesktop GitLab 被 Anubis 拦截，无法确认 KosmicKrisp 有没有 GS/XFB MR；KosmicKrisp 没有 RT（B5） | **部分正确** | 网页确实被 Anubis 拦截，但 REST API 可以访问。截至 2026-09-27，**没有**任何 kk GS/XFB MR（已合并或 open）；open 的 XFB MR 属于 pvr !44385 和 panvk !43359。RT 有 Draft MR !43303（jarrettsjohnson，2026-07-29 开启，08-11 更新）。已改：摘要、§3、§7 的 T0/T1、§8 的 B1/B3/B5、P0-4、风险 3、未解问题 2 [65][66][67] |
| （§8 说明 / P1-6）上游 vkd3d（Wine 内置 d3d12，2.1）只强制要求 maintenance1/2 和 shader_draw_parameters，XFB 可选，所以很可能是今天就能在 KosmicKrisp 上跑的开源 DX12 路径 | **部分正确** | 扩展要求和 XFB 可选都属实。但 `CHECK_FEATURE` 包含 `geometryShader` 和 `pipelineStatisticsQuery`，KosmicKrisp 都没有，所以 `have_11_0 = false`，FL11_1 及以上被挡住，**最多上报 FL11_0**；要求最低 FL11_1/12_0 的应用在设备创建时得到 `E_INVALIDARG`，GS/SO PSO 也会失败。已改：§8 说明、§7 决策树 ②、P1-6（降级为白名单 FL11_0 兜底）、未解问题 5。vkd3d 2.1 的 tag 日期 2026-08-24 未经二次核实 [50][51] |
| （P0-1 / 决策树 / 风险 4）Engine R 的 DX12 依赖用户导入的 D3DMetal；4.0b2 的许可证文本仍是 2023 版 | **部分正确** | “2023 版”属实（EA18380，2023-08-17；目前只看过 dbc-hbin 转存的副本）。但许可证并不强制用户导入：§2A(iii) 和 §2C 允许在非商业、Apple 硬件、不修改（§2D）的前提下单独分发完整的 framework，dbc-hbin 和 philippremy 已经这样做。已改：摘要“定案”、§7 决策树、P0-1（默认仍是用户导入，与 00 号综述的决策一致；Cider 下载原样 framework 作为可选来源；任何来源都禁止修改）、风险 4、新增未解问题 8 [1][68][69] |
| XDC 2026 “KosmicKrisp production ready!” 在 2026-09-29 16:25（多伦多时间），20 分钟；Mesa 26.3-rc1 在 2026-10-14，rc4/正式版约 11-04；Mesa 26.2 包含 “kk: Implement tessellation” | 确认 | indico JSON 导出显示 20:25 UTC，内部 id 18（URL 中的 538 未经核对）。26.2 还包含 “kk: Move to Metal4 command encoding” 和 “kk: Disable workarounds 1-6 in macOS 27”，已补进 §3 和参考来源 [32]。Honeykrisp `hk_CmdDrawIndirectByteCountEXT` 仍是 `UNREACHABLE("TODO")`，已核实 [34][43][70] |
