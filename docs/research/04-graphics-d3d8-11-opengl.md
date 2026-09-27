# Cider 图形转译研究 I：D3D8/9/10/11、DirectDraw、OpenGL 在 macOS 上的实现路线

> 调研日期 2026-09-26 · 置信度说明：**[已验证]** 表示直接来自一手来源（官方发布说明、源码、许可证文本，附编号 URL）；**[推断]** 表示基于源码或文档推理得出，尚未实机验证；**[未证实]** 表示只有二手或营销来源，或者本次调研没能核实。所有版本号和日期均以文中所引来源为准。本次调研没有做任何实机测试。
>
> 修订说明（2026-09-26）：本文已按独立事实核查的结论修订，涉及 wined3d 能否承载 D3D10/11、D3DMetal 的架构与许可来源、Sikarugir-App/d9mt 的实际状态等。逐条结论见文末"事实核查记录"。

---

## 摘要

- **D3D10/11 的最佳开源路线是 DXMT（D3D11/10 直接转 Metal）**。最新 tag 是 **v0.80（2026-04-23）**，这也是最后一个 MIT 版本，main 分支已改为 **LGPL-2.1-or-later** [2][3][4]。已支持的能力：32 位（WoW64，v0.50，2025-04-26）、D3D10（v0.60，2025-06-15）、Geometry Shader（v0.40，2025-03-13）、基于 Metal mesh shader 的新 tessellation 管线（v0.70，2025-10-17），以及 DLSS→MetalFX [2]。2026-09 的 main 分支仍很活跃，正在做 **D3D12** 和 **ARM64EC/ARM64X** 构建 [5]。
- **CrossOver 从 25.0.0（2025-03-11）起内置 DXMT**（DXMT v0.30 的发布说明写明它随 CrossOver 25.0 发布）。**CrossOver 26.0.0（2026-02-10）** 升级到 DXMT v0.72、D3DMetal 3.0 和 Wine 11.0；26.1–26.3（截至 2026-07-21）的 changelog 没有再提图形组件更新 [2][17]。
- **上游 DXVK 在上游 MoltenVK 上不能用**，原因是硬性特性缺失，不是性能问题。DXVK 3.0（2026-06-25）要求 Vulkan 1.4 级特性，外加 `VK_EXT_depth_clip_enable`、`VK_EXT_robustness2`、`VK_EXT_transform_feedback`、`VK_KHR_maintenance5/6` 等扩展 [20][21]，源码中 `geometryShader`、`shaderCullDistance` 也是 required [25]。MoltenVK（1.4.2，2026-07-20）没有 geometry shader、transform feedback 和 `VK_EXT_depth_clip_enable` 扩展（它只通过 `extendedDynamicState3DepthClipEnable = true` 提供动态 depth clip 状态），而且 `robustBufferAccess2=false`、`nullDescriptor=false` [27][28][29]。这里说的"不能用"只针对上游组件。CodeWeavers 历史上是在自己打过补丁的 MoltenVK 上跑 DXVK-CX 的，所以准确的说法是"不 fork MoltenVK、不伪造特性就不可能" [33]。
- **DXVK-macOS（Gcenx，基于 1.10.3）实际上已停止维护**：最后一个版本是 v1.10.3-20230507，2024-07-23 的 repack 删掉了 d3d9/dxgi，代码最后提交在 2024-06 [34][35]。它只适合作为少数 D3D10/11 游戏的遗留回退，不应该再投入。
- **上游 wined3d 在未打补丁的 Apple GL 或上游 MoltenVK 上预计撑不起 D3D10/11** [推断，依据上游源码，未实机验证]。GL 后端的 FL10+ 要求 `ARB_polygon_offset_clamp`（`GL_EXT_polygon_offset_clamp` 也映射到同一标志），Apple GL 4.1 两者都没有，所以上限是 FL 9_3 [38][40]。Vulkan 后端的 FL10 要求 `geometryShader` 和 `pipelineStatisticsQuery`，上游 MoltenVK 两个都没有，上限同样是 FL 9_3 [39][28]。**但这不等于 wined3d 在 Mac 上不能承载 D3D10/11。** CrossOver 21（2021-08）起，在 macOS 上把 wined3d Vulkan 后端设为 64 位 D3D10/11 游戏（未启用 DXVK 时）的默认路径，Skyrim SE 因此能在 Apple Silicon 上运行；CrossOver 26 的 Auto 在数据库没有指定后端时仍回落到 wined3d [17][18][66]。所以准确的结论是：上游有上限，打补丁的构建可以突破。wined3d 在 macOS 上的主要价值仍在 **D3D9 及更早版本和 DirectDraw**，打补丁的 wined3d-vk 则可以作为 D3D10/11 的兜底候选。
- **D3DMetal（Apple GPTK）面向 D3D11/12，不支持 D3D9** [18][46][67]。它**只有 64 位，而且 D3DMetal.framework 只提供 x86_64 版本**，所以在 Apple Silicon 上整个 Wine 进程都必须跑在 Rosetta 2 下 [48][67]。这一点来自第三方来源，没有找到 Apple 关于 32 位支持的一手说明。再分发条款（只允许非商业再分发、只能整体单独分发、保留 Apple 声明）来自第三方仓库对 GPTK 许可第 2A/2C 节的转述，本次没能在 Apple 页面上读到许可原文 [47]。GPTK 4 已在 WWDC26（2026-06）发布，4.0 beta 2 出现在 2026-09 [45][47]。
- **D3D9 是 macOS 上最大的空白**：D3DMetal 不做，DXMT 官方把它排在 DX11/DX12(SM5.1)/DX10 之后 [15]。目前主力仍是 wined3d-GL。能核实的 D3D9→Metal 项目只有 neo773/d9mt，属于研究级，只测过 GTA IV 一款游戏 [61]。Sikarugir-App/d9mt 的默认分支虽然叫 `dx9`，但它是否真的包含 D3D9 实现，本次无法核实 [62]。
- **OpenGL 游戏**：默认只能走 Apple 的 OpenGL 4.1（跑在 Metal 之上，没有 compute/SSBO）。替代方案是 Zink：Wine 已有把 Zink 作为 PE 侧 opengl32 的 MR（2026-04）[57]；**KosmicKrisp 于 2026-09-25 通过 Vulkan 1.4 一致性认证**，但要求 macOS 26 + Metal 4 [52][53]。Zink 的 GL 3.0/3.2 分别硬性需要 transform feedback 和 geometry shader [56]，目前 macOS 上的 Vulkan 实现都不具备，只能靠强制覆盖版本号来"凑"，属于实验性质。
- **给 Cider 的核心结论**：D3D10/11 默认用 DXMT；64 位 D3D11/12 游戏可以让用户自行安装 D3DMetal 作为按游戏的替代（它依赖 Rosetta 2，受 macOS 27 之后 Rosetta 收缩的影响）；D3D10/11 的兜底除了遗留的 DXVK-macOS，还应评估打补丁的 wined3d-vk（有 CrossOver 21 的先例）；D3D8/9/DDraw 默认用 wined3d-GL；OpenGL 默认用 Apple GL，Zink+KosmicKrisp 作为实验选项。**中长期最值得投入的是 D3D9→Metal**，而且应该放在 DXMT 框架内做。

---

## 详细调研

### 1. wined3d 在 macOS 上：OpenGL 后端与 Vulkan（MoltenVK）后端

#### 1.1 Apple OpenGL 4.1 的硬性限制

- Apple Silicon 的 core profile 报告 `GL_VERSION: 4.1 Metal - 71.0.7`、`GLSL 4.10`，底层跑在 Metal 上 [40]。在这份 43 个 GL 4.1 扩展的清单里，**没有** `polygon_offset_clamp`、`ARB_compute_shader`、`ARB_shader_image_load_store`、`ARB_texture_compression_bptc`；**有** `ARB_draw_indirect`、`ARB_tessellation_shader`、`ARB_sampler_objects` [40]。
- OpenGL 从 macOS 10.14 起被标记为 deprecated。Apple 开发者论坛上有人问 macOS 26 之后会不会移除，得到的说法是 Apple 没有宣布移除，但也没有承诺 [网页检索摘要，未证实]。

#### 1.2 wined3d GL 后端的 feature level 判定（上游 master 源码）[已验证：代码；推断：在 Mac 上的结果]

`dlls/wined3d/adapter_gl.c` 中的 `feature_level_from_caps()` [38]：
- 进入 FL10 及以上的前提是：`WINED3D_GL_VERSION_3_2 && ARB_POLYGON_OFFSET_CLAMP && ARB_SAMPLER_OBJECTS`（已由事实核查逐字核对）。扩展映射表把 `GL_ARB_polygon_offset_clamp` 和 `GL_EXT_polygon_offset_clamp` 都映射到 `ARB_POLYGON_OFFSET_CLAMP` [38]。
- FL11_1 还需要 SM5、`ARB_DRAW_INDIRECT` 和 `ARB_TEXTURE_COMPRESSION_BPTC`；FL10_1 需要 SM4、`ARB_TEXTURE_CUBE_MAP_ARRAY` 和 `ARB_DRAW_BUFFERS_BLEND`。GL 路径没有单独的 FL11_0 档位，满足 10_1 之后直接跳到 11_1 [38]。
- 前提不满足时会降到 `shader_model >= 3 && texture_size >= 4096 && buffers >= 4`，得到 **FL 9_3**。

Apple GL 没有 polygon_offset_clamp（ARB 和 EXT 两个名字都没有），也没有 BPTC [40]。**所以在 macOS 上，上游未打补丁的 wined3d-GL 给 D3D10/11 应用报告的最高 feature level 很可能只有 9_3** [推断，置信度中高，未实机验证]。凡是要求 FL10_0/11_0 的 D3D10/11 游戏，在这种构建下都会在 `D3D11CreateDevice` 阶段失败。原稿还写了"wined3d GLSL 后端的 SM5 需要 GLSL 4.30，而 Apple 只有 4.10"；事实核查时 `glsl_shader.c` 的抓取被截断，这一点**仍未核实** [未证实]。如果它成立，那么即使放宽 polygon_offset_clamp 的门槛，GL 路径最多也只能到 FL10_1（SM4）。

**对 Cider 的机会**：可以给 wined3d 打一个很小的补丁，对 Apple GL 放宽 polygon_offset_clamp 的要求（depth bias clamp 会不精确），让 wined3d-GL 在 Mac 上能报出 FL10/10_1，作为 D3D10 游戏的兜底 [推断，需实测]。不过 1.3 节说明，打补丁的 wined3d-vk 有 CrossOver 的商业先例，是更有依据的方向，应该优先评估。

#### 1.3 wined3d Vulkan 后端（跑在 MoltenVK 上）

`adapter_vk.c` 中的 `feature_level_10_supported()` 要求 `multiViewport、geometryShader、depthClamp、depthBiasClamp、pipelineStatisticsQuery、shaderClipDistance、shaderCullDistance、shaderDrawParameters` 以及 vertex divisor 相关特性；FL11 还要求 `tessellationShader` 等 [39]。MoltenVK 没有 geometry shader [30]，并且明确写着不支持 `VK_QUERY_TYPE_PIPELINE_STATISTICS` [28]。**所以上游 wined3d-vk 在上游 MoltenVK 上同样被卡在 FL 9_3** [推断，置信度高，未实机验证]。

**先例（事实核查补充）**：上面的结论只适用于上游组件，CodeWeavers 已经在商业产品中突破了这个上限。
- CrossOver 21.0.0（2021-08-03）的 changelog 写有 "Vulkan WineD3D backend now on by default for D3D10/11 games" [17]。Wine-Reviews 转述的发布说明称，该功能在 macOS 上"对 64 位 Direct3D 10/11 游戏、且未启用 DXVK 时默认开启"，Skyrim Special Edition 因此能在 Apple Silicon 上运行 [66]。
- CrossOver Mac 26 的 Auto 模式在数据库没有为游戏指定后端时，仍然使用 wined3d [18]。
- 可见 CodeWeavers 靠下游补丁（很可能再加上自有的 MoltenVK 分支）让 wined3d-vk 在 Mac 上达到了 FL10+/11。具体做法没有公开资料：可能是放宽或伪造 `geometryShader`/`pipelineStatisticsQuery` 等门槛，也可能是 2021 年时上游的判定条件本来就更宽松 [推断，未核实]。
- **对 Cider 的含义**：打补丁的 wined3d-vk 应当被视为 D3D10/11 的真实兜底候选，而不只是打补丁的 wined3d-GL。代价是依赖 GS 或 stream output 的游戏会渲染出错，所以需要按游戏白名单启用，并建立自有回归测试。

wined3d-vk 对 **D3D≤9** 的支持正在快速补齐：
- Wine 10.0 加入了基于 HLSL 的 D3D9 及更早版本的 fixed-function 管线，为 Vulkan renderer 提供 FFP 模拟；还使用 dynamic state 扩展来减少卡顿，并新增 `shader_backend=glsl-vkd3d` [41]。
- **Wine 11.0（2026-01-13）** 的 Vulkan renderer 补上了 point size/point sprite、vertex blending、fixed-function bump mapping、color key、flat shading、alpha test、user clip planes；vkd3d-shader 也加入了 SM1 像素着色器 [42][43][70]。

结论：wined3d-vk 可以作为 D3D8/9 在 MoltenVK 或 KosmicKrisp 上的**备选**（通过 `HKCU\Software\Wine\Direct3D` 中的 `renderer=vulkan` 切换）。它在 MoltenVK 上的实际兼容性没有公开数据，需要 Cider 自己建测试集 [未证实]。

#### 1.4 WoW64 下 32 位 GL 的风险

D3D8/9 游戏大多是 32 位。新 WoW64 模式要求 GL buffer 映射到 32 位地址空间。Wine 11 的 ANNOUNCE 原话是 "using Vulkan extensions if available"，**没有点名具体扩展** [42]；在 Linux 上依赖 `VK_EXT_map_memory_placed` 这一点来自检索摘要。**MoltenVK 的扩展列表里没有 `VK_EXT_map_memory_placed`** [28]，因此 macOS 上的 32 位 wined3d-GL 可能要走较慢的回退路径 [推断，未实测；由于 ANNOUNCE 没点名扩展，这条推断只得到部分支持]。这是 Cider 以新 WoW64 为基线时必须验证的性能风险点。

| wined3d 路径（macOS） | D3D10/11 最高 FL | D3D8/9/DDraw | 成熟度 |
|---|---|---|---|
| GL 后端（Apple GL 4.1，上游） | 约 9_3 [推断] | 可用，CrossOver 等产品的主力路径 | 高（D3D9 及以下） |
| Vulkan 后端（上游 MoltenVK，上游 wined3d） | 约 9_3 [推断] | Wine 10/11 起逐渐可用 | 低到中，缺少 Mac 实测 |
| Vulkan 后端（打补丁，CrossOver 21 先例） | FL10+/11：CrossOver 21 起在 Mac 上默认用于 64 位 D3D10/11 [17][66] | 同上 | 需自行打补丁并实测；依赖 GS/SO 的游戏会出错 |

---

### 2. 上游 DXVK 2.x/3.x 的要求，以及为何无法在 MoltenVK 上运行

#### 2.1 DXVK 版本与要求时间线 [已验证]

| 版本 | 日期 | 关键要求/变化 |
|---|---|---|
| 2.0 | 2022 | 要求 **Vulkan 1.3**，依赖 dynamic rendering、extended dynamic state、null descriptors [23] |
| 2.4 | 2024-07-10 | **D8VK 并入 DXVK**，D3D8 基于 D3D9 实现 [24] |
| 2.7 | 2025-07-05 | **`VK_KHR_maintenance5` 变为必需**；**移除 state cache**（此前自 2.0 起已被 GPL 取代）；在新 AMD/NV GPU 上默认使用 descriptor buffer [22][26] |
| 3.0 | 2026-06-25 | 改用 **dxbc-spirv** 编译器；**需要 Vulkan 1.4 的大量特性**；D3D9 SM1–3 重写；FFP 改为 ubershader；磁盘着色器缓存（`DXVK_SHADER_CACHE_PATH`）；默认使用 `VK_EXT_descriptor_heap` [21] |
| 3.1.1 | 2026-09-15 | 当前最新 [26] |

DXVK 3.x 官方驱动要求：Vulkan 1.4、Wine ≥10.1；必需扩展为 `VK_EXT_depth_clip_enable`、`VK_KHR_maintenance5`、`VK_KHR_maintenance6`、`VK_KHR_load_store_op_none`、`VK_EXT_robustness2`、`VK_EXT_transform_feedback`；另外要求 256 字节 push constants、descriptor indexing、8/16/64 位整数和 scalarBlockLayout [20]。
源码 `dxvk_device_info.cpp` 中标记为 required 的核心特性还包括 `geometryShader`、`shaderCullDistance`、`sampleRateShading`、`textureCompressionBC`、`robustBufferAccess`、`dualSrcBlend`、`multiViewport`，扩展特性包括 `robustBufferAccess2`、`nullDescriptor`、`depthClipEnable` [25]。

#### 2.2 MoltenVK 的缺口 [已验证/部分推断]

- MoltenVK 1.3.0（2025-04-27）支持 Vulkan 1.3，1.4.0（2025-08-20）支持 Vulkan 1.4，1.4.1 在 2025-11-24 发布，**1.4.2 在 2026-07-20 发布**（最低 macOS 12），1.4.3 尚未发布 [27]。
- 扩展列表中**没有** `VK_EXT_transform_feedback`、`VK_EXT_depth_clip_enable`、`VK_EXT_graphics_pipeline_library`、`VK_EXT_descriptor_buffer`、`VK_EXT_descriptor_heap`、`VK_EXT_map_memory_placed`；**有** maintenance5/6、load_store_op_none、robustness2（仅扩展名）、depth_clip_control、vertex_attribute_divisor、multi_draw [28]。细节：MoltenVK 宣告了 `extendedDynamicState3DepthClipEnable = true`，但没有提供 `VK_EXT_depth_clip_enable` 扩展本身，所以仍不满足 DXVK 的必需扩展要求 [29]。
- `MVKDevice.mm` 的 ROBUSTNESS_2 特性里显式设置了 `robustBufferAccess2 = false`、`robustImageAccess2 = isAppleGPU`、`nullDescriptor = false` [29]。第三方项目 Highball 也指出 MoltenVK 在这两项上的"广告"不准确 [49]。
- Geometry shader：维护者曾计划借助 Apple Metal Shader Converter 技术实现，但"资源有限、进展缓慢"，一直没有 ETA [30]。isoline tessellation 也不受支持，因为 Metal 本身没有 [32]。

**因此 DXVK ≥2.0 在上游 MoltenVK 上创建设备会直接失败**，因为缺 required 特性 [推断，置信度高]。这种缺失是结构性的，无法靠调参解决。需要说明：CodeWeavers 历史上在自有的、打过补丁的 MoltenVK 上运行 DXVK-CX [33]，所以"不可能"的准确含义是"不 fork MoltenVK、不伪造特性就不可能"。即使伪造特性让设备创建成功，依赖 GS/TF 的游戏仍会渲染出错。

#### 2.3 macOS 分支现状

- **Gcenx/DXVK-macOS**：基于 upstream 1.10.x 并 cherry-pick 部分提交，带 async 补丁（`DXVK_ASYNC`/`dxvk.enableAsync`）和 state cache（`DXVK_STATE_CACHE`）。最后一个版本是 **v1.10.3-20230507**（2023-05-07，要求 Vulkan 1.2 即 MoltenVK 1.2.0+、wine-7.1+），**2024-07-23 的 repack（v1.10.3-20230507-repack）移除了 d3d9.dll 和 dxgi.dll**，理由是它们"不应在 macOS 上使用"。最后一次代码提交在 2024-06-13 [34][35]。
- **marzent/dxvk**：README 自称 "not under active development anymore"，并指向 Gcenx 的分支 [36]。
- **CrossOver 内置的 "DXVK-CX"**：CodeWeavers 自 2019 年起用 MoltenVK + DXVK-CX 为 FFXIV 做商业移植。维护者承认，因为缺少 TF/GS，Witcher 3 等游戏会出现敌人不可见、画面错误 [33]。
- **metalsharp/DXVK-MacOS**：2026 年出现的新分支，自称基于 DXVK v3.1 并带 `dxbc-spirv-moltenvk.patch`，zlib 许可，只有 1 个 star [37]。**完全未经验证**，只建议观察。
- 性能：没有找到独立、系统性的 DXVK-macOS 与 DXMT/D3DMetal 基准测试。DXMT 作者 2024-07 称在若干场景中 DXMT 稳定 60fps，而 DXVK+MoltenVK 或 D3DMetal 做不到 [13]（来自开发者本人，属于轶事证据）。

---

### 3. DXMT（github.com/3Shain/dxmt）

#### 3.1 版本时间线 [已验证，来自 GitHub Releases API][2]

| 版本 | 日期 | 要点 |
|---|---|---|
| v0.10 | 2025-01-13 | 第一个 tag |
| v0.30 | 2025-03-13 | Deferred Context；"Shipped with CrossOver 25.0" |
| v0.40 | 2025-03-13 | **Geometry Shader**；NVEXT，DLSS→MetalFX |
| v0.41 | 2025-03-21 | DXBC→AIR 输出**确定性**，可利用系统 Metal 着色器缓存 |
| v0.50 | 2025-04-26 | 规范的 Unix Call；**32 位（WoW64 构建）** |
| v0.60 | 2025-06-15 | **D3D10**；HDR/色彩空间；D3D11 fence 与多线程层 |
| v0.61 | 2025-08-12 | 修复大量一致性问题，降低 WoW64 内存占用 |
| v0.70 | 2025-10-17 | **基于 Metal mesh shader 的新 tessellation 管线** |
| v0.71 | 2025-11-13 | 修复 PSO 卡顿回归；AIR 缓存随 Metal 缓存一起保存；修复 Sonoma 上 Metal 3.2 intrinsic 导致的崩溃 |
| v0.72 | 2025-12-11 | **经 D3DKMT 共享资源（需要 Wine 10.18+）**；实验性 Intel Mac 支持 |
| v0.73 | 2026-01-21 | fast-math 行为调整；修复 CS2、SFV 等 |
| v0.74 | 2026-03-10 | MSAA 性能、独占全屏、swapchain gamma |
| **v0.80** | **2026-04-23** | Timestamp Query；基于 `MTLFence` 和 intrapass barrier 的资源同步；**最后一个 MIT 版本**，此后改为 LGPL；计划未来几个月发布 1.0 [3] |

截至 2026-09-26 没有更新的 tag（tags API 中最新的依次是 v0.80、v0.80-rc.0、v0.74）[71]。main 分支在 2026-09 有大量提交，涉及 d3d12（placed resource、ClearRTV/DSV、alpha-to-coverage、`D3D12_FEATURE_D3D12_OPTIONS7`）、`d3d11.maxTessFactor` 选项、"fix custom HUD metrics on macOS 27"、"add `-marm64x` flag to arm64ec build" 等 [5]。

#### 3.2 架构 [已验证：目录与源码；推断：细节]

- **源码目录**：`src/{airconv, d3d10, d3d11, d3d12, dxgi, dxmt, nativemetal, nvapi, nvngx, util, winemetal}` [5]。
- **PE 侧**：`d3d11.dll`、`dxgi.dll`、`d3d10core.dll`（可选）、`winemetal.dll`。**Unix 侧**：`winemetal.so`，是 Mach-O 格式，只是按惯例用 `.so` 扩展名 [6][7]。
- **airconv**：把 DXBC（SM5.0，以及用于 D3D12 root signature 的绑定）直接翻译成 **AIR（Apple 的 LLVM IR 方言）**，再由 `metallib_writer.cpp` 写成 metallib，经 `MTLDevice newLibrary` 加载 [5][12]。这条路径**不经过 MSL 文本和 Metal 前端编译器**，所以构建时需要**完整的 LLVM 15 静态库（主版本必须是 15）** [6]。GS 和 tessellation 的转换代码分别在 `dxbc_converter_gs.cpp` 和 `dxbc_converter_ts.cpp` [10][11]。
- **winemetal.so**：通过 Wine unixlib 调用表封装 Metal API，包括 `MTLDevice_newBuffer/newTexture/newLibrary`、render/compute encoder、MetalFX 的 `newTemporalScaler/newSpatialScaler`。它还提供 32 位 thunk（`thunk32_SM50Compile` 等），shader 转换本身也在 Unix 侧执行 [12]。
- **与 winemac.drv 的耦合（关键集成点）**：`_CreateMetalViewFromHWND` 先 `dlsym(RTLD_DEFAULT, "macdrv_functions")`，失败时回退为逐个 `dlsym` 以下四个符号：`get_win_data`、`release_win_data`、`macdrv_view_create_metal_view`、`macdrv_view_get_metal_layer`，再从 HWND 取得 `CAMetalLayer` [12]。上游 Wine 的 `macdrv_cocoa.h` 声明了 `macdrv_view_create_metal_view`/`macdrv_view_get_metal_layer` [44]，但 `macdrv.h`/`macdrv_main.c` 里**找不到 `macdrv_functions`** [已验证：本次检索]。**这意味着 Cider 的 Wine 构建必须保证这些符号能被 `dlsym` 找到**（导出 `macdrv_functions` 表，或者保证符号可见），否则 DXMT 无法创建 swapchain [推断，置信度中高]。
- **构建要求**：Meson ≥1.3、LLVM 15、llvm-mingw 或 mingw-w64、Wine ≥8（需要头文件和工具）、macOS Sonoma+、**Xcode 16+（含 Metal toolchain）**；`-Dwine_builtin_dll=true` 时可作为 Wine builtin 安装 [6][7]。
- **安装与覆盖**：builtin 模式下，`winemetal.so` 放进 `<wine>/lib/wine/x86_64-unix/`，DLL 放进 `x86_64-windows` 和前缀的 system32；native 模式下使用 `WINEDLLOVERRIDES="dxgi,d3d11,d3d10core=n,b"` [7]。
- **运行要求**：所有 Apple Silicon Mac；最低 **macOS 14**，推荐 15；Wine 8+；支持 32 位和 64 位。Intel Mac 为实验支持，计划在 macOS 28 发布后移除 [8]。
- **配置**（`dxmt.conf` / `DXMT_CONFIG`）：`d3d11.preferredMaxFrameRate`、`d3d11.metalSpatialUpscaleFactor`（需要配合 `DXMT_METALFX_SPATIAL_SWAPCHAIN=1`）、`d3d11.maxFeatureLevel`、`d3d11.maxTessFactor`（4–64）、`d3d11.sampleNaNToZero`、`d3d11.defuseFma`、`dxgi.customVendorId/DeviceId`、`dxgi.forceSDR`、`dxmt.shaderMetalVersion`（310/320）[9]。

#### 3.3 兼容性、性能、许可、与 CrossOver 的关系

- **兼容性**：2024-09 宣称达到 FL11.1（有已知限制）[16]；此后按版本逐个修复游戏（上表）。官方兼容报告站点是 dxmt.report [2]。没有找到覆盖游戏数量的统计 [未证实]。
- **性能**：作者和社区认为 DXMT 利用 Metal 自身的资源跟踪做同步，而 D3DMetal 存在"过度同步"（DXVK 开发者 K0bin 在 discussion #15 中的观点）[14]；另有开发者自述的若干 60fps 场景 [13]。**没有独立的 A/B 基准测试**。CodeWeavers 的描述是 DXMT "对低配 Mac 尤其有利" [17 所附博客与检索摘要]。
- **许可**：`LICENSE` 为 **LGPL-2.1-or-later** [4]；v0.80 是最后一个 MIT 版本 [3]。Cider 可以免费分发，但需要公开对 DXMT 所做的修改。
- **CrossOver 捆绑**：CrossOver **25.0.0（2025-03-11）"Inclusion of DXMT"**；**26.0.0（2026-02-10）"Update to DXMT v0.72"** [17]。CodeWeavers 的 CrossOver 25 博客也把 DXMT 描述为与 wined3d、DXVK、D3DMetal 并列的按游戏选项 [65]。CrossOver Mac 26 的高级设置中，DXMT 被描述为 "Metal-based implementation of Direct3D 11"，与 D3DMetal、DXVK、wined3d 并列，由 Auto 数据库按游戏选择 [18]。
- **D3D9 计划**：作者在 2024-03-11 给出的优先级是 "DX11 > DX12 (SM5.1) = DX10 > DX9 > DX12 (SM6.0+) > x86" [15]。目前 main 分支在做的正是 D3D12（SM5.1 路线）[5]。D3D9 由社区分支推进（见第 5 节）。

---

### 4. D3DMetal（Apple GPTK）用于 D3D11

- **定位**：GPTK 中的 D3D11/D3D12/DXGI→Metal 实现，**不支持 DirectX 9**。多个来源互相印证：CrossOver 的描述是 "supporting DirectX 11 and DirectX 12 games" [18]，AppleGamingWiki 称它不支持 DX9、只处理 DX11/DX12 [46]，UTM 的 d3dmetal-native 也只列出 D3D11 和 D3D12 [67]。Apple 公开的 GPTK 页面没有写支持哪些 DX 版本 [45]。
- **位宽与架构**：**只有 64 位，更准确地说是 D3DMetal.framework 只提供 x86_64 版本**。utmapp/d3dmetal-native 的 README 说明，因为框架只有 x86_64，整个进程都必须是 x86_64，在 Apple Silicon 上全部跑在 Rosetta 2 下 [67]。Highball 项目（2026-09-17 合并的 PR #142）因此把 32 位 D3D10/11 游戏路由到 DXMT [48]。这些都是第三方来源，**没有找到 Apple 关于 32 位支持的一手说明**（置信度中高）。
- **与 Rosetta 的绑定（关键）**：Apple 开发者新闻称 macOS 27 是最后一个完整支持 Rosetta 的版本，之后只保留一个子集，服务于"依赖 Intel 框架的较老、无人维护的游戏" [63]。按目前的发布形态，D3DMetal 的可用性因此取决于这个游戏豁免会保留到什么程度。D3DMetal 将来是否会推出 arm64 或 ARM64EC 版本，没有公开信息 [未证实]。这进一步说明应以开源、并且正在做 ARM64EC 构建的 DXMT 作为主路径 [5]。
- **版本**：CrossOver 25 捆绑 D3DMetal 2.1，CrossOver 26 捆绑 3.0 [17]。GPTK 3 在 WWDC25（2025-06）发布，稳定版约在 2025-12，带 DLSS→MetalFX 模拟 [检索摘要]。**GPTK 4 在 WWDC26（2026-06）发布**，评估环境支持 Metal 4 [45]。第三方仓库在 2026-09-23 转发了 "D3DMetal.framework 4.0 beta 2" [47]。
- **着色器路径** [推断]：Apple Metal Shader Converter 以 DXIL（SM6.0–6.6）为输入 [50]，并提供把 GS 和 tessellation 映射到 mesh shader 的方案与示例 [51]。D3D11 用的是 DXBC，D3DMetal 内部如何处理 DXBC 没有公开资料 [未证实]。
- **质量**：按游戏差异很大。CrossOver 用数据库在 D3DMetal、DXMT、DXVK、wined3d 之间自动选择 [18]，本身就说明没有哪个后端能全面胜出。一个竞品的博客（Bourbon 26，2026-07）称 D3DMetal 在部分 DX12 游戏上是"最快的"，DXMT 胜在层数少 [64]（营销来源，可靠性低）。
- **许可（关键）**：以下条款来自第三方仓库对 Apple GPTK 许可第 2A/2C 节的转述：再分发"只限非商业用途"，D3DMetal.framework "只能整体单独分发"，并且必须保留所有 Apple 版权和专有声明 [47]。**本次没能在 Apple 页面上读到许可原文**，Apple 公开的 GPTK 页面也没有写再分发条款 [45]，所以置信度为中。Cider 免费不等于法律上一定安全，因为 Apple 许可还有"开发评估用途"等语境。**建议采用"用户自行从 Apple 下载 GPTK，由 Cider 导入"的方式**，不要随包分发，也不要放进仓库。这一保守设计与其他开源项目的做法一致，例如 JellyBean47/wyn 把 D3DMetal 当作"可选、由用户提供"的组件 [68]。

---

### 5. D3D9 方案

| 方案 | 现状 | 优点 | 缺点 | Cider 定位 |
|---|---|---|---|---|
| **wined3d-GL**（Apple GL 4.1） | 成熟；CrossOver 和 Bourbon 都用它处理 DX9 [18][64] | SM3 足够；开源；32/64 位 | GL 已被弃用；CPU 开销高；WoW64 下 32 位缓冲映射的性能风险 | **默认** |
| **wined3d-vk**（MoltenVK/KosmicKrisp） | Wine 10/11 补齐了 FFP 和 SM1 [41][42] | 摆脱 GL | 在 Mac 上缺乏实测 | 实验性回退 |
| **DXVK-macOS d3d9** | Gcenx 在 2024 年 repack 时已移除，认为"不应在 macOS 上使用" [34] | — | 1.10 时代的代码，没人维护 | 不采用 |
| **D3DMetal** | 不支持 DX9 [46] | — | — | 不适用 |
| **DXMT 官方** | 没有 D3D9；优先级靠后 [15] | 可复用 airconv/winemetal | 无时间表 | 跟踪或贡献 |
| **Sikarugir-App/d9mt**（3Shain/dxmt 的 MIT 分叉，默认分支名为 `dx9`） | README 和仓库描述仍是 DXMT 原文 "A Metal-based translation layer for Direct3D 11 and 10"，只有 2 个 star。**是否真的包含 D3D9 实现、进度如何，都无法从仓库首页核实** [62] [未证实] | 如果确有 D3D9 代码，会与 DXMT 同构 | 状态未知 | 视为未知，需要查看 `dx9` 分支的提交记录后再决定 |
| **neo773/d9mt** | 复用 vendored 的 DXVK D3D9 前端，DXSO→SPIR-V→SPIRV-Cross→MSL，自建 Metal 后端和 PSO 缓存；"research-grade"，README 称只用 GTA IV 一款游戏测试过；要求 macOS 14+，以及 CrossOver 26+ 或启用了 DXMT 的前缀 [61] | 路线清晰，是目前唯一能核实的 D3D9→Metal 项目 | 单人项目 | 参考实现 |

---

### 6. D3D8 与 DirectDraw

- **d8vk 已并入 DXVK 2.4（2024-07-10）**，建立在 DXVK 的 d3d9 之上 [24]。所以它继承了 DXVK ≥2.0 的全部 Vulkan 要求，**无法在 MoltenVK 上使用** [推断，置信度高]。Sikarugir-App/d8vk 的 `moltenvk-version` 分支是基于 1.10 的移植，几乎没有活跃度 [检索摘要]。
- **wined3d 的 d3d8/ddraw/d3d(1–7)** 经 wined3d-GL 渲染，这是 macOS 上唯一成熟的开源路径。纯 2D 的 DirectDraw 也可以用 wined3d 的 `renderer=no3d`（GDI）。
- **cnc-ddraw**（MIT 许可）只替换 `ddraw.dll`，**只支持 2D DirectDraw 游戏，不支持 Direct3D 3D 游戏**。可选渲染器有 GDI、OpenGL、Direct3D9；Wine 下需要设置 `ddraw` 的 native 覆盖；列出了 500+ 款经典 2D 游戏 [60]。在 Cider 里适合作为经典 2D 游戏的按游戏预设，用来修复窗口化、缩放和黑屏问题。
- **dgVoodoo2**（把 Glide/DDraw/D3D1–9 转成 D3D11，再交给 DXMT）理论上可行，但它是闭源软件，本次没有核实它的许可和在 Mac 上的表现 [未证实]。

---

### 7. Windows OpenGL 游戏在 macOS 上

- **默认路径**：Wine `opengl32` → `winemac.drv` → Apple CGL（最高 4.1 core）。这条路径**没有 compute shader、SSBO、image load/store、BPTC** [40]，所以需要 GL 4.3+ 的游戏（部分 id Tech、Minecraft 光影、模拟器、CAD 软件）必然失败或降级。
- **Zink**：Zink 的 GL 3.0 要求 `VK_EXT_transform_feedback` 和 `VK_EXT_conditional_rendering`，GL 3.2 要求 `geometryShader` 和 `VK_EXT_depth_clip_enable`，GL 2.1 就已经要求完整的 line rasterization（包括 stippled）等 [56]。
  - **Zink over MoltenVK**：缺 TF、GS 和 depth_clip_enable [28]，基本不可行。
  - **Zink over KosmicKrisp**：有人在 macOS 上用 `MESA_GL_VERSION_OVERRIDE=4.6` 强行提升版本号跑起了 Minecraft。早期约 70fps，之后测到 25fps（原生 100fps），而且 CPU-bound；transform feedback 相关的 mod 无法工作 [58]。这证明路线可行，但离产品化还远。
  - **Wine 上游**：Rémi Bernon（CodeWeavers）在 2026-04 提交了 MR !10531，"opengl32: Just use Zink (as PE-side OpenGL implementation)"，内嵌 Mesa 26.0.3 的子集，Steam 和 KOTOR 可以运行，能否合并不确定 [57]。**如果合并，Zink 会成为 PE 侧组件，通过 winevulkan 调用任意 ICD**，对 Cider 意义很大，因为它能绕开 Apple GL 和 32 位映射问题。
- **KosmicKrisp**：LunarG 基于 Mesa 的 Vulkan→Metal 驱动。2025-08 宣布 [55]，并入 Mesa 26.0 [检索摘要]。Vulkan SDK 1.4.357.0（2026-07-28）起"暴露完整 Vulkan 1.4"，**必须 Metal 4 + macOS 26**，性能最多提升约 2.35 倍 [53]。**2026-09-25 通过 Vulkan 1.4 CTS**，额外支持 tessellation、robustness2、multi-draw，只支持 M1 及以上（不支持 Intel Mac），通过认证的构建随 9 月 29 日发布的 Vulkan SDK 提供 [52]。注意这篇 LunarG 文章的页面元数据日期是 2026-09-25，正文却误写成 "September 25, 2025"。LunarG 2026-01 的 "State of Vulkan on Apple" 仍把 KosmicKrisp 描述为 Vulkan 1.3 一致性、"1.4 coming soon"，可以确认 2026 是正确年份 [69]。官方材料**没有声称支持 geometry shader 或 transform feedback** [52][54]，所以 Zink 的 GL 3.0+/3.2+ 需要的 TF、GS 和 `VK_EXT_depth_clip_enable` 仍然没有原生满足 [56]。
- **MGL**（OpenGL 4.6 on Metal，Apache-2.0）：面向嵌入应用，不支持 GS，大量函数未实现 [59]，不适合作为 Wine 的通用后端。

---

### 8. Geometry Shader / Tessellation / Stream Output 在 Metal 上的模拟

| 实现 | Geometry Shader | Tessellation（HS/DS） | Stream Output / Transform Feedback |
|---|---|---|---|
| **DXMT** | 自 v0.40 起：VS 转成 **object function**，GS 转成 **mesh function**，由 mesh 发射图元；**只支持单个 stream**，受 `gs_max_vertex_output` 限制 [2][10] | 自 v0.70 起：**VS+HS 合并为 object shader**，生成 `TessMeshWorkload`；**tessellator+DS 在 mesh shader 中实现**；tess factor 被钳制到 PSO 推导出的上限，可用 `d3d11.maxTessFactor` 限制 [2][9][11] | 2024-07 时只对"关闭光栅化且非 strip 拓扑"的特例做了 hack（面向 Unity GPU 蒙皮），计划改用 mesh 管线完整模拟 [13]；当前完整度未证实 |
| **D3DMetal / Metal Shader Converter** | 映射到 mesh shader（Apple 示例）[51] | 映射到 mesh shader [51] | 未公开 [未证实] |
| **MoltenVK** | **不支持**，多年规划但无 ETA [30] | compute kernel 计算 tess factor，再走 Metal 固定功能 tessellator 和 post-tessellation vertex function；**不支持 isoline** [32] | **不支持** [28][33] |
| **KosmicKrisp** | 未声明 | 2026-09 声明支持 [52] | 未声明 |
| **Apple GL 4.1（wined3d-GL）** | 原生支持（GL 3.2） | 原生支持（GL 4.0） | 原生支持（GL 3.0/4.0） |

注意最后一行的反差：Apple GL 其实具备 GS/TF/tessellation，但上游 wined3d 的 FL 判定被 polygon_offset_clamp 挡住了，SM5 可能还受 GLSL 版本限制（后者未核实，见第 1 节）。

---

### 9. 按 API 的推荐矩阵（Cider v1）

| API | 默认 | 回退 1 | 回退 2 / 实验 | 备注 |
|---|---|---|---|---|
| DirectDraw（2D） | wined3d ddraw（GL） | cnc-ddraw（按游戏预设） | wined3d `renderer=no3d` | cnc-ddraw 为 MIT 许可，可以随包分发 |
| D3D 1–7 | wined3d（GL） | wined3d（Vulkan） | dgVoodoo2→DXMT [未证实] | 基本只有 wined3d |
| D3D8 | wined3d（GL） | wined3d（Vulkan） | 长期方案：DXVK d3d8 前端 + Metal D3D9 后端 | 上游 d8vk 在 Mac 上不可用 |
| D3D9 | wined3d（GL） | wined3d（Vulkan，MoltenVK） | d9mt 类 D3D9→Metal（实验） | **最大空白**，D3DMetal 不支持 |
| D3D10/10.1 | **DXMT**（d3d10core） | 打补丁的 wined3d-vk（CrossOver 21 先例，按游戏白名单） | DXVK-macOS 1.10.3（遗留）；wined3d-GL 打补丁放宽 FL（实验） | 上游 wined3d 在未打补丁的 Apple GL/MoltenVK 上预计只到 FL 9_3 |
| D3D11（64 位） | **DXMT** | D3DMetal（用户自装 GPTK，按游戏；x86_64，需要 Rosetta 2） | 打补丁的 wined3d-vk；DXVK-macOS 1.10.3 | 由数据库按游戏选择；没有指定时仿照 CrossOver 回落到 wined3d |
| D3D11（32 位） | **DXMT**（WoW64） | 打补丁的 wined3d-vk（需验证 32 位） | DXVK-macOS 1.10.3（32 位） | D3DMetal 只有 64 位 |
| OpenGL ≤4.1 | Apple GL（winemac.drv） | — | Zink+KosmicKrisp（macOS 26+） | — |
| OpenGL ≥4.3 | Zink+KosmicKrisp（实验，需覆盖版本号） | — | — | 目前没有可靠方案 |

---

## 对 Cider 的启示与建议（按优先级）

**P0（MVP 必做）**
1. **以 DXMT 为 D3D10/11 主后端**，用 `-Dwine_builtin_dll=true` 集成进 Cider 的 Wine 构建，并跟踪 main 分支（LGPL，改动需开源）。在 Cider 的 Wine 中**保证 `macdrv_functions` 表，或 `get_win_data`/`release_win_data`/`macdrv_view_create_metal_view`/`macdrv_view_get_metal_layer` 这几个符号能被 `dlsym` 找到**，并写入 CI 自检 [12][44]。Wine 基线定为 **≥11.0**（DXMT v0.72 的共享资源需要 ≥10.18）[2][43]。
2. **后端选择框架**：仿照 CrossOver 的 Auto，建立按游戏的数据库（exe 哈希、Steam AppID → 后端 + 环境变量 + dxmt.conf）。底层通过 `WINEDLLOVERRIDES` 和 builtin/native DLL 放置来切换。同时提供全局覆盖和按程序覆盖 [18]。
3. **D3D8/9/DDraw 默认走 wined3d-GL**，并针对 32 位 WoW64 下的 GL buffer 映射做性能基准（MoltenVK 没有 `VK_EXT_map_memory_placed`）[28][42]。
4. **开发环境**：安装完整 Xcode（DXMT 需要 Xcode 16+ 和 Metal toolchain）、LLVM 15（静态库）、llvm-mingw、Meson ≥1.3 [6]。当前机器只有 CLT，这是第一个阻塞项。
5. **持久化着色器缓存**：DXMT 的 AIR 转换是确定性的，并和 Metal 系统缓存一起保存 [2]。Cider 应为每个 bottle 固定缓存目录，禁止清理工具误删，后续可考虑预热。

**P1（首个公开版本）**
6. **D3DMetal 走"用户自带"模式**：引导用户登录 Apple Developer 下载 GPTK，由 Cider 校验 SHA256 后导入，**不随包分发，也不放进仓库** [47][68]。只对 64 位 D3D11/12 按游戏启用 [48]。由于 D3DMetal.framework 只有 x86_64 版本 [67]，启用它的 bottle 必须以 x86_64 Wine 跑在 Rosetta 2 下。Cider 的 bottle 架构（x86_64 与将来的 arm64/ARM64EC）需要把"是否可用 D3DMetal"作为显式属性，安装流程也要检测 Rosetta 是否已安装。
7. **UI 暴露 DXMT 能力**：MetalFX 空间放大（`DXMT_METALFX_SPATIAL_SWAPCHAIN=1` + `d3d11.metalSpatialUpscaleFactor`）、帧率限制、`maxTessFactor`（低配 8GB 机器很有用）、DLSS→MetalFX、HDR 开关 [9]。
8. **cnc-ddraw 预设**：随包提供，用于经典 2D 游戏 [60]。
9. **D3D10/11 回退层**：（a）**评估打补丁的 wined3d-vk 作为 D3D10/11 的兜底**。CrossOver 21 起在 macOS 上默认用它处理 64 位 D3D10/11 游戏，Skyrim SE 因此可玩，CrossOver 26 的 Auto 在没有指定后端时仍回落到 wined3d [17][18][66]。具体做法是在 Cider 的 Wine 分支里放宽或伪造 `feature_level_10_supported()` 中的 `geometryShader`/`pipelineStatisticsQuery` 等门槛，只对通过回归测试的游戏按白名单启用 [39][推断，需实测]。它的优先级高于"放宽 polygon_offset_clamp 的 wined3d-GL"，因为前者有商业先例。（b）**DXVK-macOS 1.10.3 只作为遗留回退**，不主动维护；不采用 metalsharp 分支，直到它有可信的验证 [34][37]。

**P2（中长期差异化）**
10. **投入 D3D9→Metal**：推荐在 DXMT 框架内实现，复用 winemetal 和 airconv 的基础设施，并参考 neo773/d9mt 复用 DXVK D3D9 前端的做法。在 D3D9 之上再用 DXVK 的 d3d8 前端覆盖 D3D8 [24][61]。这是 Cider 相对 CrossOver 最可能做出差异的方向，建议与 3Shain 或上游协作，避免分叉。neo773/d9mt 是目前唯一能核实的参考实现，但只测过一款游戏 [61]；Sikarugir-App/d9mt 在核实其 `dx9` 分支确有 D3D9 代码之前，不应当作可复用的基础 [62]。
11. **跟踪 Wine 的 Zink PE MR !10531 和 KosmicKrisp**：在 macOS 26+ 上提供 "OpenGL（实验：Zink）" 开关；同时评估 KosmicKrisp 作为 MoltenVK 的可选 ICD（Highball 已把它列为待评估项）[49][52][57]。
12. **ARM64EC 与后 Rosetta 时代**：Apple 表示 macOS 27 是最后一个完整支持 Rosetta 的版本，之后只保留一个子集，服务于"依赖 Intel 框架的较老、无人维护的游戏" [63]。DXMT 已经在做 ARM64EC/ARM64X 构建 [5]。Cider 的图形层应优先选择能编译为 ARM64EC 的开源组件。**D3DMetal 的问题比"形态不明"更具体**：D3DMetal.framework 目前只有 x86_64 版本，必须在 Rosetta 2 下运行整个 Wine 进程 [67]，所以它的前途取决于 Rosetta 的游戏豁免会保留多久、覆盖多广。这进一步支持把 DXMT 作为主路径，D3DMetal 只作为可选增强，不能成为任何功能的唯一路径。

---

## 风险

1. **DXMT 单点依赖**：核心开发者集中在个人（3Shain），1.0 尚未发布 [3]。缓解办法是跟随上游，同时建立自有回归测试集，保持向上游贡献。
2. **LGPL 合规**：v0.80 之后的 DXMT 是 LGPL [4]，Cider 需要公开修改并允许替换库。Wine 本身也是 LGPL，所以不构成额外负担。
3. **D3DMetal 的法律与技术风险**：许可限制再分发（条款来自第三方转述，Apple 原文未核实）[47]；只有 64 位，框架只有 x86_64 版本，必须依赖 Rosetta 2 [48][67]，而 macOS 27 之后 Rosetta 只保留面向老游戏的子集 [63]；闭源、无法调试；Apple 的版本节奏（GPTK 4 beta）可能打破兼容。
4. **Apple OpenGL 可能被移除**：wined3d 的 D3D≤9、DDraw 和 GL 游戏都依赖它。目前没有移除公告，但在 macOS 27/28 周期内存在不确定性。
5. **MoltenVK 的特性缺口长期存在**：GS/TF 规划多年没有落地 [30]，让 DXVK 和 Zink 路线长期不可用。KosmicKrisp 要求 macOS 26 + Metal 4 [53]，会把老系统用户排除在外。
6. **32 位 GL 性能**：新 WoW64 下 32 位 GL 映射在 macOS 上可能走慢速路径 [42][28]，影响大量 D3D8/9 老游戏。
7. **8GB 统一内存**：DXMT 已降低 WoW64 内存占用 [2]，但 D3DMetal 和 GPTK 的评估环境官方建议 16GB [检索摘要]，大型 D3D11 游戏在开发机上的测试结论可能偏悲观或出现 OOM。
8. **wined3d FL 判定的推断需要实测**：第 1 节"FL 9_3 上限"的结论来自上游源码推理，只适用于未打补丁的上游 wined3d 加上 Apple GL 或上游 MoltenVK；Apple 驱动实际暴露的扩展集合也可能因 macOS 版本而不同。反过来，打补丁的 wined3d-vk 虽有 CrossOver 先例 [17][66]，但 CodeWeavers 的具体补丁不公开，Cider 需要自己重做，而且依赖 GS/SO 的游戏会渲染出错。

## 未解问题

1. 在 macOS 26.5 上，wined3d-GL 和 wined3d-vk（MoltenVK 1.4.2）实际报告的 D3D11 feature level 是多少？（用 `WINEDEBUG=+d3d` 实测）放宽 `geometryShader`/`pipelineStatisticsQuery` 门槛后，wined3d-vk 能跑通哪些 D3D10/11 游戏？
2. DXMT 当前对 stream output（`CreateGeometryShaderWithStreamOutput`）支持到什么程度，是否仍是特例 hack？
3. D3DMetal 4.0 对 D3D11 DXBC 的处理方式（是否先转 DXIL）。另外，Apple 是否会推出 arm64 或 ARM64EC 版的 D3DMetal？目前第三方资料显示框架只有 x86_64 版本 [67]，但没有 Apple 一手说明。
4. `macdrv_functions` 符号表是 CrossOver 的私有补丁还是上游已有？（在上游 `macdrv.h` 和 `macdrv_main.c` 中没有找到）
5. Wine MR !10531（Zink PE）是否已经合并或有替代方案？（GitLab 被反爬拦截，本次无法核实）
6. KosmicKrisp 是否计划支持 geometry shader 和 transform feedback？只有这两项到位，DXVK 3.x 和 Zink GL 4.6 才能在 Mac 上真正可用。
7. Sikarugir-App/d9mt 的 `dx9` 分支是否真的包含 D3D9 实现？仓库首页和 README 仍是 DXMT 的 D3D11/10 原文，无法判断 [62]。需要查看该分支相对 3Shain/dxmt 的提交差异。
8. CrossOver 26.x 实际捆绑的 MoltenVK 和 DXVK-CX 版本（source tarball 未拆包核对）。
9. CrossOver 21 起让 wined3d-vk 在 MoltenVK 上达到 FL10+ 的具体补丁是什么？是放宽或伪造 wined3d 的特性门槛、修改 MoltenVK，还是 2021 年上游的判定本来就更宽松？可以通过 CodeWeavers 公开的 CrossOver 源码包核对。
10. wined3d GLSL 后端的 SM5 是否要求 GLSL 4.30？事实核查时 `glsl_shader.c` 的抓取被截断，没能核实。

## 参考来源

1. https://github.com/3Shain/dxmt — DXMT 仓库首页（D3D11/10→Metal）
2. https://api.github.com/repos/3Shain/dxmt/releases — DXMT 全部版本号、日期和发布说明
3. https://github.com/3Shain/dxmt/releases/tag/v0.80 — v0.80（2026-04-23），最后一个 MIT 版本，1.0 计划
4. https://raw.githubusercontent.com/3Shain/dxmt/main/LICENSE — LGPL-2.1-or-later
5. https://github.com/3Shain/dxmt/commits/main/ — 2026-09 提交：d3d12、arm64ec/-marm64x、macOS 27 HUD 修复
6. https://raw.githubusercontent.com/3Shain/dxmt/main/docs/DEVELOPMENT.md — 构建依赖（LLVM 15、Meson、Xcode 16+ Metal toolchain）
7. https://github.com/3Shain/dxmt/wiki/DXMT-Installation-Guide-for-Geeks — 文件清单、安装路径、DLL 覆盖
8. https://github.com/3Shain/dxmt/wiki/Device-System-Runtime-Specifications — macOS 14+、Wine 8+、Apple Silicon、Intel 实验支持
9. https://raw.githubusercontent.com/3Shain/dxmt/main/dxmt.conf — 配置项与环境变量
10. https://raw.githubusercontent.com/3Shain/dxmt/main/src/airconv/dxbc_converter_gs.cpp — GS→object+mesh 实现
11. https://raw.githubusercontent.com/3Shain/dxmt/main/src/airconv/dxbc_converter_ts.cpp — tessellation→object+mesh 实现
12. https://raw.githubusercontent.com/3Shain/dxmt/main/src/winemetal/unix/winemetal_unix.c — winemetal unixlib、winemac 符号 dlsym、MetalFX
13. https://github.com/3Shain/dxmt/discussions/9 — 2024-07 状态：SO 特例 hack、性能说法
14. https://github.com/3Shain/dxmt/discussions/15 — DXMT 与 D3DMetal 的差异（同步、自研转换器）
15. https://github.com/3Shain/dxmt/discussions/4 — D3D9/32 位优先级（2024-03-11）
16. https://github.com/3Shain/dxmt/discussions/19 — 2024-09：FL11.1、tessellation
17. https://www.codeweavers.com/crossover/changelog — CrossOver 25.0.0 纳入 DXMT；26.0.0 升级 DXMT v0.72/D3DMetal 3.0/Wine 11.0；21.0.0（2021-08-03）默认启用 wined3d Vulkan 后端处理 D3D10/11；最新 26.3.0（2026-07-21）
18. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26 — CrossOver Mac 26 各图形后端说明
19. https://www.phoronix.com/news/CrossOver-26 — CrossOver 26 组件报道（检索摘要）
20. https://github.com/doitsujin/dxvk/wiki/Driver-support — DXVK 3.x 的 Vulkan 1.4 与必需扩展
21. https://github.com/doitsujin/dxvk/releases/tag/v3.0 — dxbc-spirv、Vulkan 1.4、descriptor heap
22. https://github.com/doitsujin/dxvk/releases/tag/v2.7 — maintenance5 必需、移除 state cache
23. https://github.com/doitsujin/dxvk/releases/tag/v2.0 — Vulkan 1.3 要求
24. https://github.com/doitsujin/dxvk/releases/tag/v2.4 — D8VK 并入
25. https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxvk/dxvk_device_info.cpp — required 特性清单
26. https://api.github.com/repos/doitsujin/dxvk/releases — DXVK 版本日期（3.1.1 于 2026-09-15）
27. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/Whats_New.md — MoltenVK 1.3.0–1.4.2 日期与内容
28. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/MoltenVK_Runtime_UserGuide.md — 支持的扩展、pipeline statistics 限制
29. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm — robustBufferAccess2/nullDescriptor=false
30. https://github.com/KhronosGroup/MoltenVK/discussions/1785 — GS 实现计划与进度
31. https://github.com/KhronosGroup/MoltenVK/issues/203 — DXVK 所需特性（2018 年起开放）
32. https://github.com/KhronosGroup/MoltenVK/issues/1236 — 不支持 isoline tessellation
33. https://github.com/KhronosGroup/MoltenVK/discussions/1402 — DXVK-CX/FFXIV，TF/GS 缺失的影响
34. https://api.github.com/repos/Gcenx/DXVK-macOS/releases — DXVK-macOS 版本（1.10.3-20230507，2024 repack）
35. https://api.github.com/repos/Gcenx/DXVK-macOS/commits — 最后一次提交 2024-06-13
36. https://github.com/marzent/dxvk — 已声明停止维护
37. https://github.com/metalsharp/DXVK-MacOS — 2026 年新分支（基于 v3.1，未验证）
38. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/wined3d/adapter_gl.c — GL 的 feature_level_from_caps
39. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/wined3d/adapter_vk.c — Vulkan 的 FL10/11 判定
40. https://www.geeks3d.com/20210202/apple-silicon-m1-mac-mini-arm-test-opengl/ — M1 的 GL 4.1/GLSL 4.10 扩展清单
41. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.0/ANNOUNCE.md — Wine 10.0 的 Direct3D 变更
42. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — Wine 11.0 的 Vulkan renderer 旧特性、WoW64 GL 映射
43. https://api.github.com/repos/wine-mirror/wine/commits/wine-11.0 — Wine 11.0 发布于 2026-01-13
44. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/macdrv_cocoa.h — macdrv metal view API
45. https://developer.apple.com/games/game-porting-toolkit/ — GPTK 4
46. https://www.applegamingwiki.com/wiki/Game_Porting_Toolkit — D3DMetal 只支持 DX11/12，不支持 DX9
47. https://github.com/dbc-hbin/d3dmetal-redistributable — Apple 许可"仅非商业再分发"的转述；4.0 beta 2
48. https://github.com/gauthierpiarrette/highball/pull/142 — D3DMetal 只有 64 位，32 位路由到 DXMT（2026-09-17）
49. https://github.com/gauthierpiarrette/highball/issues/41 — MoltenVK 特性问题与 KosmicKrisp 评估
50. https://developer.apple.com/metal/shader-converter/ — Metal Shader Converter（DXIL SM6.0–6.6）
51. https://developer.apple.com/metal/sample-code/ — GS/tessellation 经 mesh shader 模拟的示例（检索摘要）
52. https://www.lunarg.com/kosmickrisp-achieves-vulkan-1-4-conformance-on-apple-silicon/ — KosmicKrisp Vulkan 1.4 一致性认证（2026-09-25）
53. https://www.lunarg.com/lunarg-releases-vulkan-sdk-1-4-357-0/ — KosmicKrisp 完整 Vulkan 1.4、需要 macOS 26 + Metal 4（2026-07-28）
54. https://docs.mesa3d.org/drivers/kosmickrisp.html — KosmicKrisp 文档
55. https://www.lunarg.com/a-vulkan-on-metal-mesa-3d-graphics-driver/ — KosmicKrisp 发布公告（2025-08-07）
56. https://docs.mesa3d.org/drivers/zink.html — Zink 各 GL 版本对 Vulkan 的要求
57. https://www.gamingonlinux.com/2026/04/a-future-wine-release-could-use-zink-to-run-opengl-via-vulkan/ — Wine MR !10531（Zink PE）
58. https://gist.github.com/lucamignatti/5312f5e937de2ba44256ecba6de54cc2 — Zink+KosmicKrisp 运行 Minecraft 的实测记录
59. https://github.com/openglonmetal/MGL — OpenGL 4.6 on Metal（不完整，无 GS）
60. https://raw.githubusercontent.com/FunkyFr3sh/cnc-ddraw/master/README.md — cnc-ddraw（MIT，只支持 2D DDraw）
61. https://github.com/neo773/d9mt — D3D9→Metal（DXVK 前端 + SPIRV-Cross）
62. https://github.com/Sikarugir-App/d9mt — 3Shain/dxmt 的 MIT 分叉，默认分支名为 dx9，README 仍是 D3D11/10 描述；是否含 D3D9 实现无法核实
63. https://developer.apple.com/news/?id=w5ngl9k2 — Rosetta：macOS 27 为最后完整支持的版本，之后保留游戏子集
64. https://pyrosoft.pro/pages/bourbon26-blog.php?a=dxvk-vs-d3dmetal — 竞品 Bourbon 26 的后端说法（营销来源，低可靠性）
65. https://www.codeweavers.com/blog/mjohnson/2025/3/11/experience-next-level-gaming-on-mac-with-crossover-25 — CrossOver 25 博客：DXMT 作为按游戏的后端选项
66. https://www.wine-reviews.net/2021/08/codeweavers-crossover-21-for-linux-mac.html — CrossOver 21：macOS 上 64 位 D3D10/11 默认走 wined3d Vulkan 后端，Skyrim SE 在 Apple Silicon 上可玩
67. https://github.com/utmapp/d3dmetal-native — D3DMetal 支持 D3D11/12；D3DMetal.framework 只有 x86_64，在 Apple Silicon 上整个进程跑在 Rosetta 2 下
68. https://github.com/JellyBean47/wyn — 把 D3DMetal 当作"可选、由用户提供"的组件
69. https://www.lunarg.com/the-state-of-vulkan-on-apple-jan-2026/ — LunarG 2026-01：KosmicKrisp 为 Vulkan 1.3 一致性，1.4 即将到来
70. https://www.winehq.org/news/2026011301 — Wine 11.0 发布公告（2026-01-13）
71. https://api.github.com/repos/3Shain/dxmt/tags — DXMT tag 列表（最新为 v0.80）

## 事实核查记录

核查日期 2026-09-26。结论取自独立事实核查。本轮各核查者之间没有相互冲突的结论，所以没有条目需要标记为"存疑"。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| DXMT 最新 tag 为 v0.80（2026-04-23），是最后一个 MIT 版本；main 的 LICENSE 为 LGPL-2.1-or-later；v0.50（2025-04-26）加入 32 位 WoW64，v0.60（2025-06-15）加入 D3D10 | 属实 | Releases/Tags API 显示 v0.80 之后没有新 tag（依次是 v0.80、v0.80-rc.0、v0.74）；v0.80 说明中写有改用 LGPL 与 1.0 计划；wiki 称支持 32 位和 64 位。第 3.1 节补充了 tags 来源 [2][3][4][8][71]。 |
| CrossOver 25.0.0（2025-03-11）首次纳入 DXMT；26.0.0（2026-02-10）升级到 DXMT v0.72、D3DMetal 3.0、Wine 11.0 | 属实 | changelog、DXMT v0.30 说明和 CrossOver 25 博客相互印证；最新版本为 26.3.0（2026-07-21）。有摘要工具把 "Inclusion of DXMT" 错归到 23.0.0（2023-08-16），这是摘要错误（DXMT 在 2023 年还不存在），不构成真实冲突。补充了博客来源 [17][65]。 |
| DXVK 3.x 要求 Vulkan 1.4 及 6 个必需扩展；MoltenVK 1.4.2（2026-07-20）不支持 TF 和 depth_clip_enable，且 `robustBufferAccess2=false`、`nullDescriptor=false` | 属实 | 补充了两处细节：MoltenVK 宣告了 `extendedDynamicState3DepthClipEnable = true`，但没有提供 `VK_EXT_depth_clip_enable` 扩展本身；`robustImageAccess2 = isAppleGPU`。"不可能"只针对上游 MoltenVK，CodeWeavers 曾在打过补丁的 MoltenVK 上跑 DXVK-CX，准确说法是"不 fork、不伪造特性就不可能"。已修改摘要与第 2.2 节 [20][27][28][29][33]。 |
| wined3d GL 后端 FL10+ 要求 GL 3.2 + polygon_offset_clamp + sampler_objects；Vulkan 后端 FL10 要求 geometryShader、pipelineStatisticsQuery；因此两条路径在 macOS 上都只到 FL 9_3 | 部分属实 | 源码事实已逐字核对（`GL_EXT_polygon_offset_clamp` 也映射到同一标志；GL 路径没有单独的 FL11_0 档位）。"只到 9_3"只适用于未打补丁的上游 wined3d 加上 Apple GL 或上游 MoltenVK，而且是未经实机验证的推断。CrossOver 21 起在 macOS 上默认用 wined3d-vk 处理 64 位 D3D10/11 游戏，所以打补丁的构建可以突破上限。"SM5 需要 GLSL 4.30"未能核实（`glsl_shader.c` 抓取被截断），已标为未证实。已修改第 1.2、1.3、1.4 节 [38][39][40][17][66]。 |
| D3DMetal 只支持 DX11/12、不支持 DX9、只有 64 位；Apple 许可只允许非商业再分发、框架必须整体分发 | 部分属实 | 支持 D3D11/12、不支持 D3D9 得到 CrossOver、AppleGamingWiki、UTM d3dmetal-native 三方印证。"只有 64 位"更准确的说法是 D3DMetal.framework 只提供 x86_64 版本，在 Apple Silicon 上依赖 Rosetta 2；没有 Apple 关于 32 位的一手说明。许可条款来自第三方仓库对 GPTK 许可第 2A/2C 节的转述，Apple 原文未能读到，Apple 公开的 GPTK 页面也没有写 DX 版本和再分发条款。"用户自带、Cider 导入"的保守设计不变。已修改摘要、第 4 节和 P1-6 [18][45][46][47][48][67][68]。 |
| KosmicKrisp 于 2026-09-25 通过 Vulkan 1.4 CTS，要求 Apple Silicon、macOS 26+、Metal 4；SDK 1.4.357.0（2026-07-28）起暴露完整 Vulkan 1.4 | 属实 | LunarG 文章正文把日期误写为 "September 25, 2025"，页面元数据和 2026-01 的 "State of Vulkan on Apple"（当时仍写"1.4 coming soon"）可以确认年份是 2026。通过认证的构建随 9 月 29 日发布的 SDK 提供，不支持 Intel Mac。来源中都没有提到 GS/TF，Zink GL 3.0+/3.2+ 的要求仍未原生满足。已补充第 7 节 [52][53][54][56][69]。 |
| （原稿第 1、9 节与风险部分）wined3d 在 macOS 上撑不起 D3D10/11，只应用于 D3D≤9/DDraw；D3D10/11 只把"打补丁的 wined3d-GL"列为最后手段 | 部分属实 | 对上游未打补丁的构建大概率成立，但原稿忽略了 CrossOver 21（2021-08）起把 wined3d-vk（跑在 MoltenVK 上）设为 macOS 64 位 D3D10/11 默认后端的先例（Skyrim SE 在 Apple Silicon 上可玩），而 CrossOver 26 的 Auto 仍会回落到 wined3d。已修改摘要、第 1.3/1.4 节、第 9 节矩阵、P1-9（新增"评估打补丁的 wined3d-vk"）、风险 8 和未解问题 1/9 [17][18][66]。 |
| （原稿第 4 节与 P2-12）D3DMetal 在 arm64 Wine 下的前途不明；D3DMetal "只有 64 位" | 部分属实 | 实际情况更具体，影响也更大：D3DMetal.framework 只有 x86_64 版本，整个 Wine 进程必须跑在 Rosetta 2 下。macOS 27 是最后一个完整支持 Rosetta 的版本，之后只为"依赖 Intel 框架的较老游戏"保留子集，所以 D3DMetal 的现有形态与这个游戏豁免绑定。这进一步支持以 DXMT 为主路径。已修改第 4 节、P1-6、P2-12、风险 3 和未解问题 3 [63][67]。 |
| （原稿第 5 节）Sikarugir-App/d9mt 是"DXMT 的 dx9 分支"，承载 D3D9 工作 | 无法核实 | 它是 3Shain/dxmt 的 MIT 分叉（2 个 star），默认分支名为 `dx9`，但 README 和描述仍是 DXMT 的 D3D11/10 原文，无法确认是否包含 D3D9 实现及进度，应视为未知。唯一能核实的 D3D9→Metal 项目是 neo773/d9mt（vendored DXVK D3D9 前端→SPIR-V→SPIRV-Cross→MSL，研究级，只测过 GTA IV，要求 macOS 14+ 以及 CrossOver 26+ 或启用 DXMT 的前缀）。已修改摘要、第 5 节表格、P2-10、未解问题 7 和参考来源 62 [61][62]。 |
| （原稿第 2.3 节）Gcenx/DXVK-macOS 最后发布 v1.10.3-20230507；2024-07-23 的 repack 删除了 d3d9.dll 和 dxgi.dll；要求 Vulkan 1.2 / Wine 7.1+ | 属实 | Releases API 核实无误；补充了"MoltenVK 1.2.0+"和原版发布日期 2023-05-07 [34]。 |
| （原稿第 1.3/1.4 节）Wine 11.0 于 2026-01-13 发布，为 Vulkan renderer 补齐旧 D3D 特性，vkd3d-shader 加入 SM1 像素着色器，新 WoW64 下"在可用时借助 Vulkan 扩展"把 GL buffer 映射到 32 位内存 | 属实 | WineHQ 公告与 ANNOUNCE 核实无误。但 ANNOUNCE 没有点名具体扩展，所以"macOS 缺 `VK_EXT_map_memory_placed`、会走慢速路径"只得到部分支持，仍是未实测的推断。已修改第 1.4 节措辞并补充来源 [42][70][28]。 |
