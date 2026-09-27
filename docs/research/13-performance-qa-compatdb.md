# Cider 性能工程、诊断工具、自动化兼容性测试与兼容性数据库设计

> 调研日期 2026-09-26 · 置信度说明：**[高]** 一手来源（官方文档、release notes、源码、Apple 文档/WWDC 逐字稿）直接确认；**[中]** 可信二手来源，或由一手来源间接推出；**[低]** 社区帖子、单一来源或未能打开原文；**[推断]** 作者自己的工程推理，未经实测。所有版本号和日期都附了来源；查不到的地方写明“未能证实”。
> 2026-09-27 根据独立事实核查修订：更正了 DXVK 3.0 限帧器、Metal 系统缓存目录、Game Mode 相关表述和 macOS 27 Rosetta 的来源强度，见文末“事实核查记录”。

## 摘要

- **DXVK 的 state cache 已经没有了。** DXVK 2.7（2025-07-05）删除了 state cache [1]。DXVK 3.0（2026-06-25）改用 dxbc-spirv，把自己的 IR 缓存到 Wine prefix 的 `AppData/Local`（`DXVK_SHADER_CACHE_PATH` / `DXVK_SHADER_CACHE=0`）。3.0 同时要求 Vulkan 1.4 级别的特性，并删掉了 `DXVK_FRAME_RATE` 环境变量。**内置限帧器本身还在**：`dxvk.conf` 的 `dxgi.maxFrameRate` / `d3d9.maxFrameRate` / `dxvk.maxFrameRate`（也可经 `DXVK_CONFIG` 传入）仍然有效 [2][80]。之后的版本：3.0.1（2026-07-05）、3.0.2（2026-07-17）、3.1（2026-08-28）、3.1.1（2026-09-15）[79]。**[高]**
- **macOS 上 DXVK 的主要卡顿源还在 MoltenVK 这一层。** MoltenVK 不支持 `VK_EXT_graphics_pipeline_library`（#1711，2022-09 开到现在）。它的 VkPipelineCache 只存 MSL 源码，不存 Metal 二进制（#1765，2022-11 开到现在）[5][6][8]。main 分支的 `MVKExtensions.def` 里既没有 GPL，也没有 `pipeline_binary` [81]。MoltenVK 1.4.0（2025-08-20）起支持 Vulkan 1.4 [4]；之后的 1.4.1（2025-11-30）和 1.4.2（2026-07-24，当前最新）都没有加 GPL 或 binary archive [4]。**[高]**
- **三条 Metal 后端各自有磁盘缓存：**
  - D3DMetal：`$(getconf DARWIN_USER_CACHE_DIR)/d3dm/<exe 名>/shaders.cache`。这个路径来自 AppleGamingWiki 的清理说明和 GPTK README 副本，Apple 没有把它当作稳定接口写进文档，GPTK 版本之间可能变化 [12][13]；
  - DXMT：v0.71（2025-11-13）起把 AIR 和 Metal PSO 缓存放在 `$(getconf DARWIN_USER_CACHE_DIR)/dxmt/<exe 名含扩展名>`，文件名是 `shaders_<metal_version>.db` [15][82]；
  - Metal 自己的系统缓存：顶层 `$(getconf DARWIN_USER_CACHE_DIR)/com.apple.metal` 存系统进程和没有 bundle 的进程的 shader。有 bundle 的应用**早在 macOS 14 就按 bundle ID 分目录**（`<bundleID>/com.apple.metal`）[16][84]。macOS 27 beta 上看到的新东西是 `com.apple.gpuarchiver`、`archiveUsage.db`，以及沙盒下缓存目录建不出来时直接崩溃 [17]。
  Cider 必须统一管理这些目录，处理版本失效、备份和清理，并弄清 Wine 进程最终继承哪个 bundle ID（从 `.app` shim 启动还是作为裸二进制启动）。
- **Metal 4 的离线编译链路完整，但产物绑定 GPU 架构和 OS 版本。** 链路是 `MTL4PipelineDataSetSerializer` → `.mtl4-json` → `metal-tt` → `MTL4Archive`，查找会因为“没有匹配的 pipeline、OS 不兼容或 GPU 架构不兼容”而 miss，miss 时要退回设备端编译 [11][10]。归档可以同时为多个目标架构构建，目标匹配时照样命中；但 M1–M5 × OS 小版本的矩阵太大，**把分发 Metal 二进制当主路径不划算**，主路径应该分发“pipeline 配方”，由客户端在本地后台预编译。另外 `metal-tt` 属于完整 Xcode 工具链，开发机目前只有 CLT。**[高（链路和 miss 原因）/推断（分发结论）]**
- **Game Mode 对 Wine 类“子进程开窗”架构可能不友好。**
  - Apple 文档元数据把 `LSSupportsGameMode` 标为 macOS 26.0、iOS/iPadOS 18.6 起可用 [31]。macOS 14/15 上 Game Mode 靠 `LSApplicationCategoryType=public.app-category.games` 和旧键 `GCSupportsGameMode` 触发 **[中，检索摘要]**。
  - macOS 26 release notes 的已知问题 153127050 是“直接从 Terminal 派生的二进制不会激活 Game Mode”，规避方法是用 `open` 启动 [32]；26.1 的 release notes 没有把它列为已修复 [83]。
  - Apple DTS（2025-06）针对“启动器用 Java 子进程开新窗口”的回答带有保留：看起来不太可能生效，也就是 Game Mode 不会迁移到新窗口。这不是确定的平台规则 [33]。
  - CrossOver 具体怎么触发 Game Mode，**未能证实**。
- **诊断工具链都能用环境变量打开，不依赖 Xcode：**
  - Metal HUD（`MTL_HUD_*`，每秒把逐帧 present 间隔和 GPU 时间写进系统日志）[37][38]；
  - API/Shader Validation [39][40]；
  - `MTL_CAPTURE_ENABLED=1`（macOS 14+）[41]；
  - macOS 27 + Xcode 27 新增 `gpucapture` / `gpudebug` 命令行工具 [22][25]。
  Instruments（`xctrace`）需要完整 Xcode [42]。
- **上游 Wine 在 macOS 上没有自动化测试。** GitLab CI 在 Tart 上有两个 Mac 任务：MR 用的 `build-mac`（`winehq-sequoia-pristine` 镜像）和每日的 `build-daily-mac`（`winehq-sonoma-pristine` 镜像），都只构建不跑测试 [48]。test.winehq.org 的结果列只有 Windows 各版本和 Linux，没有 Mac 列（截至 2026-09-25 的构建 4e819f054dd2）[49]。Cider 在 Mac 上跑 winetest 是一块空白，也是能反哺上游的地方。
- **macOS VM 不能做 GPU 兼容性测试。** Virtualization.framework 同时最多运行 2 台 macOS VM [51]；半虚拟化 GPU 只报告大约 Apple5 代的特性，也没有 residency set [52]。GPU 测试必须在实体 Mac 上做。**[中-高]**
- **兼容性数据库可借鉴的做法：**
  - ProtonDB：结构化问卷算档位，每月导出，最新是 `reports_sep1_2026.tar.gz` [62][63]；
  - CodeWeavers：1–5 星，22,667 个应用，BetterTester 分档审核 [59][60][61]；
  - AppleGamingWiki：按运行方式分别评级 [64]；
  - Highball-db：每个游戏一个 JSON，记录 provenance，报告以 GitHub issue 提交 [66]。
- **2025–2026 年没有严格的公开 A/B 对比**（D3DMetal vs DXMT vs DXVK-macOS vs 原生），只有零散数据：
  - 《赛博朋克 2077》原生版比 CrossOver 快约 3%（M 系列，2025-07）[36]，M5 上是 59.8 vs 56.35 fps [70]；
  - D3DMetal 的“过度同步”在 M2 Max 上被测出约 42% 帧时间影响（2026-04，论坛帖）[26]。
  Cider 需要自己的基准框架，并公开数据。

## 详细调研

### 1. 着色器编译卡顿的缓解

#### 1.1 各层缓存一览

| 层 | 缓存内容 | 位置 / 开关 | 状态与日期 | 置信度 |
|---|---|---|---|---|
| DXVK ≤2.6 state cache | 管线状态（`.dxvk-cache`） | 游戏目录 | 2.0 起引入 GPL 后基本不用；**2.7（2025-07-05）删除** [1] | 高 |
| DXVK 3.0 IR cache | DXVK 自己的着色器 IR（dxbc-spirv 之后） | prefix 的 `AppData/Local`（Wine 下默认 `%LOCALAPPDATA%/dxvk`）；`DXVK_SHADER_CACHE_PATH`，`DXVK_SHADER_CACHE=0` 关闭 [2][3] | 3.0（2026-06-25），编译完全放到 worker 线程；部分游戏省约 1 GB 内存 [2] | 高 |
| DXVK GPL | 加载 D3D shader 时就编 Vulkan shader | 驱动需支持 `VK_EXT_graphics_pipeline_library` [3] | **MoltenVK 不支持**（#1711 open；`MVKExtensions.def` 里没有这个扩展）[6][8][81] | 高 |
| MoltenVK VkPipelineCache | 只有 SPIR-V→MSL 的 MSL 源码 | 应用自己调 `vkGetPipelineCacheData` 序列化；`MVK_CONFIG_SHADER_COMPRESSION_ALGORITHM` 控制压缩 [7][8] | 用 MTLBinaryArchive 的提案 #1765 自 2022-11-10 起一直 open [5]；1.4.0 修复了“pipeline cache 内的 shader cache miss”[4] | 高 |
| Metal 系统缓存 | 编译后的 GPU 二进制和 PSO | 顶层 `$(getconf DARWIN_USER_CACHE_DIR)/com.apple.metal` 存系统和无 bundle 进程的 shader；有 bundle 的应用在 `<DARWIN_USER_CACHE_DIR>/<bundleID>/com.apple.metal`（macOS 14 上已存在，如 `org.mozilla.firefox`、`org.mozilla.firefox-gpu-helper`）[16][84]；另见 `<bundleID>/com.apple.gpuarchiver`、`archiveUsage.db` [16][17] | 按 bundle 分目录**不是 macOS 27 新引入的**。[17] 是第三方 bug（沙盒 helper 在 27 beta 上崩溃），能说明的只是 27 在 `recordBinaryArchiveUsage` / gpuarchiver 上有新行为，以及沙盒下缓存目录创建失败变成致命错误 | 中-高（目录布局）/ 低（27 的具体变化） |
| MTLBinaryArchive | 应用自管的 PSO 和二进制归档 | `addRenderPipelineFunctions` / `serialize(to:)`，macOS 11+ [9] | 可用 | 高 |
| MTL4Archive | Metal 4 只读归档，可含 IR 和 GPU 专用二进制 | macOS 26.0+ [10] | 可用 | 高 |
| D3DMetal | DXIL→Metal 转换结果 | `$(getconf DARWIN_USER_CACHE_DIR)/d3dm/<exe 名>/shaders.cache`（目录按可执行文件名，不是自由格式的游戏名），下一级按 `MTLGPUFamily` 分 [12][13][14] | 路径来自 AppleGamingWiki 和 GPTK README 副本，Apple 没有作为稳定接口写进文档，GPTK 版本之间（如 GPTK 4）可能变化；GPTK 3→4 缓存格式改变，旧缓存不能复用 [20] | 高（当前路径）/ 低（格式和长期稳定性） |
| DXMT | airconv 转出的 AIR 加 Metal PSO 缓存 | 默认 `$(getconf DARWIN_USER_CACHE_DIR)/dxmt/<exe 名含扩展名>/shaders_<metal_version>.db` [15][82]；`DXMT_SHADER_CACHE=0` 关闭；`DXMT_SHADER_CACHE_PATH` **只在值以 `/` 开头（Darwin 绝对路径）时生效**，否则退回默认目录 [82] | v0.71（2025-11-13）引入，官方称“显著减少首次运行卡顿”[15]；文件名带 Metal 版本，Metal 升级后自动失效；最新 v0.80（2026-04-23）[15] | 高（路径和变量均读过源码） |

#### 1.2 macOS 上 DXVK 的特殊处境

- 上游 DXVK 的抗卡顿主要靠 GPL。GPL 的效果是：Vulkan shader 在游戏加载 D3D shader 时就编译，而不是在 draw 时 [3]。
- MoltenVK 没有 GPL，所以 DXVK 在 Mac 上退回“draw 时编译完整 pipeline”。MoltenVK 的 pipeline cache 又只省掉 SPIR-V→MSL 这一步，MSL→GPU 二进制的编译还得靠 Metal 系统缓存兜底 [5][8]。
- 可调参数：`MVK_CONFIG_SHOULD_MAXIMIZE_CONCURRENT_COMPILATION`（macOS 13.3+，默认 0），以及 `MVK_CONFIG_SHADER_DUMP_DIR`（导出 SPIR-V、MSL 和 pipeline 列表，调试用）[7]。
- 分支现状：
  - Gcenx 的 DXVK-macOS 停在 v1.10.3（2023）[77]；
  - 社区分支 metalsharp/DXVK-MacOS 声称已基于上游 3.1，需要 Wine 打开 `VK_KHR_portability_enumeration` 补丁 [77] **[低]**。
- **[推断]** DXVK 3.0 要求 Vulkan 1.4。MoltenVK 1.4 在版本号上满足，但不是所有必需特性都齐，需要逐项核对。这对 Cider 意味着：DXVK 路线要么维护一个 2.x 分支，要么跟进 3.x 并给 MoltenVK 补特性。

#### 1.3 Metal 4 的离线编译和异步编译（WWDC25 “Explore Metal 4 games”）[11]

- **采集**：把 `MTL4PipelineDataSetSerializer`（`CaptureDescriptors` 配置）挂到 `MTL4Compiler` 上，它会自动记录创建过的 pipeline 描述，然后 `serializeAsPipelinesScriptWithError:` 写成 `.mtl4-json`。
- **构建**：`metal-tt <script> <Metal IR libs> -o archive.mtl4`。`metal-tt` 需要完整 Xcode 工具链，CLT 里没有。归档可以只含 IR、只含针对特定 GPU/系统的二进制，或两者都有 [10]；也可以为多个目标架构构建。
- **运行时**：`newArchiveWithURL:` 加载归档，再 `newRenderPipelineStateWithDescriptor:` 查找。原话是查找会因为“no matching pipeline, incompatible OS, or incompatible GPU architecture”而 miss，**miss 要自己处理**，通常退回设备端编译。
- **Flexible pipeline**：先建 unspecialized pipeline（`MTLPixelFormatUnspecialized` 等），再按颜色附件配置特化。这样能省掉大部分编译时间，代价是片段着色器多一次跳转；重要的 shader 应该在后台编一份完整状态的版本。
- **并发和 QoS**：线程数用 `maximumConcurrentCompilationTaskCount`（macOS 13.3+）；编译线程的 QoS 设为 `QOS_CLASS_DEFAULT`，并且低于渲染线程。
- **对 Cider 的含义 [推断]**：DXMT 和 D3DMetal 都在 Metal 层之上，Cider 可以在自己的 DXMT 分支里：
  1. 在转换层记录“DXBC hash + 管线状态键”；
  2. 在空闲或安装时用低 QoS 线程预热；
  3. 在 macOS 26+ 上试用 MTL4Archive 或 flexible pipeline。
  D3DMetal 是闭源的，只能在外部管理它的缓存目录。

#### 1.4 D3DMetal 缓存的现实问题（GPTK 4，2026-06）

- GPTK 4 把 DX12 转到 Metal 4；AppleInsider 称 DX11 仍走 Metal 3 [18] **[中]**。
- 《007 First Light》在 GPTK 4 下每次冷启动都要编译约 20 分钟 [18]。社区工具“007 Shader Commander”在编完后备份 `d3dm` 缓存、每次启动前恢复，作者称启动从 20 分钟降到 3 秒 [19] **[低]**。说明 D3DMetal 缓存在某些情况下会失效或被清掉，外部备份/恢复有实际价值。
- 二手来源称 GPTK 4 采用“快速转译 + 后台优化”两阶段编译，且缓存格式与 GPTK 3 不兼容 [20] **[低]**。
- 换 D3DMetal 版本后第一次启动很慢，是在重建缓存 [14] **[中]**。
- `d3dm/<exe 名>/shaders.cache` 这个位置来自 AppleGamingWiki 的清理说明（`cd $(getconf DARWIN_USER_CACHE_DIR)/d3dm; cd GAME_NAME; rm -r shaders.cache`）和 GPTK README 副本 [12][13]。Apple 没有把它当作稳定接口写进文档，GPTK 4 及以后的版本可能改动，Cider 的缓存管理器要按 D3DMetal 版本检测目录，不能写死 **[推断]**。

#### 1.5 预编译着色器分发的可行性

| 方案 | 可移植性 | 可行性 | 说明 |
|---|---|---|---|
| 分发 Metal 二进制（MTLBinaryArchive / MTL4Archive） | 绑定 GPU 架构 × OS 版本；可以多架构构建，目标匹配时能命中 [10][11] | **低（推断）** | 矩阵太大（M1–M5 × 26.x/27.x 小版本），命中率难以保证；构建还要完整 Xcode（`metal-tt`）。适合对少数 Top 游戏做补充，不适合当主路径 |
| 分发 DXMT AIR / DXVK 3.0 IR 缓存 | 与 GPU 无关，但绑定转换器版本 | 中 | 需要带转换器版本号做键；体积和隐私要评估 **[推断]** |
| 分发“pipeline 配方”（shader hash + 状态键列表，类似 Fossilize `.foz`）[57] | 与设备无关 | **高** | 客户端在本地后台重放编译，这是 Valve 在 Linux 上的思路 **[中，Steam 预缓存使用 Fossilize 本次未直接确认]** |
| 本地缓存的备份和恢复（类似 007 Shader Commander） | 只对本机有效 | 高 | 能防止 D3DMetal 缓存意外失效 [19] |

### 2. CPU 开销、命令提交、帧节奏、Game Mode、MetalFX 与内存

#### 2.1 CPU 开销链

- **Rosetta**：Mach-O 由 `oahd` 做 AOT 并缓存在 `/var/db/oah`；运行时代码走 JIT [78]。Wine 映射的 PE 镜像**大概率走 JIT** **[推断，见 06 号报告]**，所以首次运行会有 CPU 端卡顿，Cider 的 shader 预热也要避开这段时间。
- **wineserver 往返和同步原语**：见 03 号、06 号报告。
- **转换层的 draw call 开销**：2026-04 有开发者在 M2 Max / macOS 26.4 上用 Metal System Trace 分析 D3DMetal，认为它的显式同步“做了双倍的活”，在《死亡搁浅 2》中造成约 42% 的帧时间影响。Apple DTS 回复说 DX11（隐式同步）和 DX12（显式屏障）没法直接比，要求提交更多 GPU trace（FB22426600）[26] **[低-中]**。
- Metal HUD 的 `rosetta`、`metalcpu`、`shaders` 指标可以把问题粗分到 CPU 翻译、Metal CPU 开销或编译卡顿上 [37]。

#### 2.2 Metal 命令提交策略

WWDC25 给出的 Metal 4 建议 [11]：

- 多个 command buffer 用一次 `commit:count:` 提交；
- 用 `MTL4RenderEncoderOptionSuspending` / `Resuming` 让 Metal 合并跨 command buffer 的 render pass；
- 很少变动的 residency set 挂到 queue 上；
- CAMetalLayer 自带 residency set；
- 每帧执行 `waitForDrawable` → 编码 → `commit` → `signalDrawable` → `present`。

MoltenVK 的相关参数 [7]：

- `MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS`（默认 1，在调用线程上提交）；
- `MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS`（默认 0）；
- `MVK_CONFIG_MAX_ACTIVE_METAL_COMMAND_BUFFERS_PER_QUEUE`（默认 64）；
- `MVK_CONFIG_USE_MTLHEAP`（默认 1）。

#### 2.3 帧节奏、VSync、ProMotion

- `CAMetalLayer.displaySyncEnabled` 默认是 `true`（vsync），设为 `false` 时出帧更快，但可能撕裂（macOS 10.13+）[27]。有社区 Wine 引擎分支把它改成跟随 D3D11 的 `SyncInterval`，并修了与延迟图层属性更新的竞态 [30] **[低]**。
- `present(afterMinimumDuration:)`（macOS 10.15.4+）可以按固定间隔排程 drawable [29]，适合实现“锁 30/40/60 fps”这种平滑限帧。
- `CAMetalDisplayLink`（macOS 14+）有 `preferredFrameRateRange` 和 `preferredFrameLatency`，面向 ProMotion 这类可变刷新率显示器 [28]。
- DXVK 3.0 **只删除了 `DXVK_FRAME_RATE` 环境变量**，release notes 建议改用外部工具，但明确说配置文件选项保留 [2]。master 上的 `dxvk.conf` 仍记载 `dxgi.maxFrameRate`、`d3d9.maxFrameRate`、`dxvk.maxFrameRate`，取值说明包括 0（默认）、n、-n、-1 [80]。**[高]**
  - DXVK 路径：Cider 生成 per-game `dxvk.conf`，或用 `DXVK_CONFIG` 传入 `dxgi.maxFrameRate=N`，就能直接用 DXVK 自带的限帧器，不必自己实现。
  - 其他路径：macOS 上没有 Gamescope 和 MangoHud，D3DMetal（闭源）等后端仍需要 Cider 在 winemac 或转换层提供统一限帧器 **[推断]**。DXMT 有 `d3d11.preferredMaxFrameRate`（见 04 号报告）。
- Metal HUD 的 `presentdelay`、`frameintervalgraph`、`frameintervalhistogram` 可以直接用来判断帧节奏 [37]。

#### 2.4 macOS Game Mode

**已确认的事实：**

- 基本行为 [34]：Apple silicon、macOS 14+；游戏进入全屏时自动开启；给游戏最高 CPU/GPU 优先级、降低后台任务；把蓝牙采样率翻倍。
- `LSSupportsGameMode`：Apple 文档元数据标注的可用性是 macOS 26.0、iOS/iPadOS 18.6 [31]。文档说“不写这个键，Game Mode 可能不会开启”[31]。
- macOS 14/15：Game Mode 靠 `LSApplicationCategoryType=public.app-category.games` 和旧键 `GCSupportsGameMode` 触发；`GCSupportsGameMode` 覆盖 macOS 14+，可以和 `LSSupportsGameMode` 同时写 **[中，检索摘要，未读到 Apple 原文]**。
- macOS 26 release notes [32]：修复了“`LSSupportsGameMode` 被忽略”（153125166）。已知问题是**直接从 Terminal 派生的二进制不会激活 Game Mode**（153127050），规避方法是 `open MyGame.app --env MTL_HUD_ENABLED=1 --args ...`。macOS 26.1 的 release notes 没有提到 153127050，所以至少在 26.1 它没被列为已修复 [83]。
- DTS（2025-06）的说法 [33]：针对“启动器通过 Java 子进程开新窗口”的情况，DTS 的原话是这“看起来不太可能生效”，即 Game Mode 不会迁移到新窗口；回复本身也说对 CLI/Java 子进程的情况不确定。**这是带保留的判断，不是确定的平台规则。** 测试中只把 bundle ID 改成 `com.mojang.minecraftlauncher` 就能激活，说明系统对某些已知启动器有特殊处理。

**未能证实**：CrossOver 如何让 Wine 进程触发 Game Mode（CodeWeavers 论坛相关帖返回 403）。

**[推断] 对 Cider 的方案**：给每个游戏生成一个 per-game `.app` shim，Info.plist 写 `LSApplicationCategoryType=public.app-category.games`、`LSSupportsGameMode=YES` 和 `GCSupportsGameMode=YES`（后两个分别照顾 macOS 26+ 和 macOS 14/15），通过 LaunchServices（`NSWorkspace` 或 `open`）启动，并让 shim 的主可执行文件本身 `exec` 成 Wine loader，这样“开窗的进程”就是 bundle 的主进程。这需要在 M3 / macOS 26.5 和 27 上实测，并纳入测试矩阵。

#### 2.5 MetalFX

D3DMetal 用 `D3DM_ENABLE_METALFX` 实现 DLSS→MetalFX；DXMT 自带 MetalFX 空间放大和 DLSS 兼容（见 04、05 号报告）。在 8 GB 机器上，降低内部渲染分辨率能同时减轻 GPU 负载和显存占用 **[推断]**。Metal HUD 会自动显示 MetalFX 指标 [37]。

#### 2.6 8 GB 机器的内存压力

- `MTLDevice.recommendedMaxWorkingSetSize` 表示“不影响运行性能的前提下 GPU 可分配的近似字节数”[35]。Cider 应该读取它，并和 D3D 报告给游戏的显存大小对齐。
- 实际数据：
  - M1 MacBook Air 8 GB 跑原生《赛博朋克 2077》，开 MetalFX 质量档也只有约 12 fps [36]；
  - RDR2 在 8 GB 的 M3 iMac 上开场即崩（见 08 号报告）；
  - DXVK 3.0 在部分游戏中省了约 1 GB 内存 [2]。
- **[推断]** 在 8 GB 机器上：
  - 限制 shader 编译并发数（不用 `maximumConcurrentCompilationTaskCount` 的上限）；
  - 默认开 MetalFX；
  - 在启动器里监听 `DISPATCH_SOURCE_TYPE_MEMORYPRESSURE`，把事件写进诊断包。

### 3. 诊断工具

#### 3.1 Metal Performance HUD（不需要 Xcode）[37][38]

- **开关**：`MTL_HUD_ENABLED=1`。
- **逐帧日志**：`MTL_HUD_LOG_ENABLED=1`，每秒输出一行 `metal-HUD: <首帧号>,<显存>,<进程内存>,<present 间隔>,<GPU 时间>,...`。
- **编译日志**：`MTL_HUD_LOG_SHADER_ENABLED=1`，每编一个 shader 发一个 signpost，subsystem 是 `com.apple.metal.hud`，category 是 `Logging`，格式如 `CompileShader: name: ... compilation-time: ... cached: 0|1`。**`cached` 字段可以直接用来统计缓存命中率。**
- **指标**：`MTL_HUD_ELEMENTS` 可选 `device, rosetta, layersize, layerscale, memory, fps, frameinterval, gputime, thermal, frameintervalgraph, presentdelay, frameintervalhistogram, metalcpu, gputimeline, shaders, framenumber, disk, fpsgraph, toplabeledcommandbuffers, toplabeledencoders`。
- **其他**：`MTL_HUD_INSIGHTS_ENABLED`、`MTL_HUD_REPORT_URL`（写性能报告的路径）、`MTL_HUD_CONFIG_FILE`、`MTL_HUD_ENCODER_TIMING_ENABLED`、`MTL_HUD_DISABLE_MENU_BAR`。
- **macOS 27**：HUD 新增上采样器曝光、jitter 散点图等调试项 [25]。

#### 3.2 Metal 验证层

- API 验证：`MTL_DEBUG_LAYER=1`，配合 `MTL_DEBUG_LAYER_ERROR_MODE=assert|ignore|nslog`、`MTL_DEBUG_LAYER_WARNING_MODE=...|oslog` [39]。
- Shader 验证：`MTL_SHADER_VALIDATION=1`，配合 `..._REPORT_TO_STDERR`、`..._ENABLE_PIPELINES` / `..._DISABLE_PIPELINES`、`..._FAIL_MODE=zerofill|allow` 等。官方提示它会明显拖慢 GPU 和编译 [40]。
- 完整参数见 `man MetalValidation`。适合做“诊断模式”，查转换层生成的越界访问和 NaN。

#### 3.3 GPU 帧捕获

- `MTL_CAPTURE_ENABLED=1`（macOS 14+）或 Info.plist 的 `MetalCaptureEnabled`；用 `MTLCaptureManager` 加 `.gpuTraceDocument` 写出 `.gputrace` [41]。
- GPTK 的流程 [13]：`MTL_CAPTURE_ENABLED=1 D3DM_DXIL_PROCESS_DEBUG_INFORMATION=1` 启动；DXC 编译时加 `-Zi -Qembed_debug`；Xcode 里 Debug Executable 选 CrossOver.app，把 GPU Frame Capture 设为 Metal，然后 Attach；lldb 里对 SIGUSR1 执行 `process handle -pass false -stop false -notify false`。
- **macOS 27 + Xcode 27** 新增命令行工具 [22][23][24][25]：
  - `gpucapture`：目标进程必须带 `MTL_CAPTURE_ENABLED=1` 启动；快速退出的进程用 `MTLCAPTURE_WAIT_FOR_SIGNAL=1` 挂起等待；
  - `gpudebug`：可以脚本化浏览 trace、导出纹理和缓冲；跨 GPU 架构时 `fetch` 可能不可用。
- MoltenVK 自带 `MVK_CONFIG_AUTO_GPU_CAPTURE_SCOPE` / `..._OUTPUT_FILE` [7]。

#### 3.4 Instruments

- `xcrun xctrace record --template 'Metal System Trace' --attach <pid> --time-limit 10s --output x.trace`，再用 `xctrace export --xpath` 导出 [42]。
- 需要完整 Xcode，开发机目前只有 CLT，所以装 Xcode 是 P0。
- 上面 2.1 的 D3DMetal 同步分析就是用这个方法做的 [26]。

#### 3.5 Wine、转换层与崩溃

- **WINEDEBUG**：语法是 `[class][+|-]channel,...`，class 为 `err|warn|fixme|trace`，例如 `WINEDEBUG=warn+all`、`fixme-all,warn+cursor,+relay` [43]。
- **CrossOver 的做法**：“Run with Options → Create log file”生成 `.cxlog`，频道预先填好；安装问题用 `+seh,+tid` [45]。Cider 可以照搬这套交互。
- **转换层日志**：
  - DXVK：`DXVK_HUD`（`fps, frametimes, submissions, drawcalls, pipelines, memory, gpuload, compiler, cs...`）、`DXVK_LOG_LEVEL`、`DXVK_LOG_PATH` [3]；
  - D3DMetal：日志以 `D3DM` 为前缀写入系统日志，可在 Console 里看；报 bug 时要附未过滤的完整日志 [13]。
- **崩溃**：
  - Windows 侧：`winedbg --auto`（登记为 AeDebug 调试器时输出崩溃摘要）、`--minidump`（写 `.mdmp`，之后可以重新载入）[44]；
  - macOS 侧：`.ips` 是两段 JSON，首行元数据里 `bug_type=309` 表示崩溃报告；正文有 `exception`、`threads`、`usedImages`，以及 **`translated`（是否在 Rosetta 下）** [46]。存放目录一般是 `~/Library/Logs/DiagnosticReports` **[中，常识，本次未在文档中读到]**。

### 4. 自动化兼容性测试（QA）

#### 4.1 上游 Wine 的 CI 现状

- **测试**（`tools/gitlab/test.yml`）：只在 Debian trixie 容器（Xvfb dummy、FVWM、PulseAudio）和 Win10 21H2 runner 上跑，命令是 `winetest.exe -q -q -o - -J winetest.xml`，结果以 JUnit 报告上报 [47]。
- **macOS 只构建不测试**：
  - `build-mac`：MR 触发，Tart executor（`TART_EXECUTOR_SSH_USERNAME/PASSWORD`，tag `mac`），镜像 `winehq-sequoia-pristine`，`--enable-win64 --with-mingw`；失败时只保留 `config.log` [48]；
  - `build-daily-mac`：每日任务，镜像 `winehq-sonoma-pristine` [48]；
  - 两个任务都不跑测试。`test.yml` 里的测试任务只在 debian-trixie 镜像和 Win10 21H2 runner 上，没有 macOS 任务 [47]。
- **test.winehq.org**：索引页最新构建是 4e819f054dd2（Sep 25）。结果列是 Win8、Win1507+、Win1709+、Win1909+、Win10、Win10L、Win11、Linux、Failures，没有 Mac 列；每个构建约 886 个测试单元 [49]。核查者没能逐个打开单次构建的报告列表，所以不能完全排除有 macOS 主机的报告被归到 Linux/Wine 组里，但目前看不到任何一份。
- **回归二分**：官方 wiki 的流程是 `git bisect` + ccache，不 `make install`，直接从构建目录运行，用干净 prefix [50]。

#### 4.2 虚拟机与设备实验室

- Virtualization.framework 同时最多运行 2 台 macOS VM（由系统强制）；VM 登录不了 App Store；VM 里可以用 Rosetta [51]。
- 半虚拟化 GPU（AppleParavirtGPU）在 Tahoe 客体里只报告大约 Apple5 代的家族、32 KB threadgroup memory，也没有 residency set [52]。**D3DMetal、DXMT、Metal 4 路径的真实行为在 VM 里测不出来** **[推断]**。
- 嵌套虚拟化只在 M3/M4 + macOS 15+ 上支持，而且只对 Linux 客体 [53]。
- **结论**：VM（Tart）只用于构建、winetest 的非图形部分和 UI 冒烟测试；GPU 和游戏测试必须用实体 Mac。

#### 4.3 游戏自动化测试流水线 [推断为主]

1. **启动**：用 LaunchServices 拉起游戏（顺带覆盖 Game Mode 行为）。设置 `MTL_HUD_ENABLED=1 MTL_HUD_LOG_ENABLED=1 MTL_HUD_LOG_SHADER_ENABLED=1`，用 `log stream` 采集 `metal-HUD:` 行和 `com.apple.metal.hud` 的 signpost [38]。
2. **判活**：以转换层的第一次 present 作为“启动成功”信号（在 Cider 维护的 DXMT/DXVK 分支里加 hook，比如 `CIDER_FRAME_DUMP=N`）。
3. **输入**：在 prefix 内运行一个 Windows 端 agent，调用 `SendInput` 回放脚本，这样不用申请 macOS 辅助功能（TCC）权限；只有需要测 macOS 层输入时才用 `CGEventPost`。
4. **截图**：优先在转换层 present 时回读 backbuffer，避开屏幕录制权限，也不受窗口遮挡影响；与基线做 SSIM 或感知哈希比较，给容差。
5. **帧时间**：用 HUD 日志算 1% low、p95 帧间隔、编译事件数和 `cached=0` 比例，写入时序数据库，按 commit 做回归告警。
6. **轨迹回放**（开发转换层时用）：参考 Mesa 的 piglit replayer 加 traces-db，回放 apitrace / RenderDoc / GFXReconstruct 轨迹，比对帧校验和 [54]。
   - apitrace 在 Wine 下可以抓 D3D11，用 `apitrace trace -a dxgi`，再用 `d3dretrace.exe` 回放 [55]；
   - GFXReconstruct 支持 D3D12/DXR 抓取和回放 [56]。
   这样 DXMT 和 vkd3d 的回归不需要真游戏也不需要登录。

#### 4.4 测试矩阵（建议）

| 维度 | 取值 | 说明 |
|---|---|---|
| macOS | 26.x（开发机 26.5）、27.x（2026-09-14 发布，MacRumors、9to5Mac 等多家媒体报道 [76][85]）；下一代 28 | “27 是最后一个完整 Rosetta 版本，28 起只保留给部分老游戏”是 Apple 此前公布的计划，本轮核查没有复核一手原文 **[中]**。“升级到 27 会移除 Rosetta，要用 `softwareupdate --install-rosetta --agree-to-license` 重装”只有博客和 PaperCut KB 等二手来源，其中一篇说 27 首个版本缺了自动“安装 Rosetta”提示、后续版本已恢复；没有找到 Apple release note 确认 [86][87] **[低]**。测试矩阵要把“27 升级后 Rosetta 是否还在”作为一个检查项 |
| 芯片和内存 | M1 8 GB / M3 8 GB（开发机）/ M4 或 M5 16 GB / Pro 或 Max 32 GB+ | 8 GB 单独成一个档位；M3 起有硬件 RT 和 mesh |
| CPU 路径 | Rosetta / ARM64EC 加 FEX（见 06 号报告） | `nox86exec=1` boot-arg（`sudo nvram boot-args`）可以验证不依赖 Rosetta，设了之后本该走 Rosetta 的进程启动即崩溃（136764433）[32]。在 Apple silicon 上设置 boot-args 实际可能要先降低安全策略，**未验证** |
| 图形后端 | D3DMetal / DXMT / DXVK+MoltenVK / vkd3d(-proton)+MoltenVK / wined3d | 每个游戏配置锁定后端，矩阵里只测“推荐后端加一个备选” |
| 引擎和启动器 | UE4/UE5、Unity、Source 2、idTech、自研；Steam、EA、Ubisoft、Battle.net | 配合 08 号报告的启动器清单 |

矩阵用 pairwise 组合降维。“Top-N 游戏 × 推荐配置”每晚跑；全矩阵每周或每个 RC 跑一次。

#### 4.5 CodeWeavers 与 Valve 的测试方式

- **CodeWeavers**：内部 QA 加 BetterTester 计划。BetterTester 通过 Beta Center 提交测试报告，每人每天最多 5 份；按字段完整度分 Tier 1/2/3 发积分，员工批准的报告拿最高分，热门应用有加成 [59]。
- **Valve**：Steam Deck / Steam Machine 兼容性评级分 Verified、Playable、Unsupported、Unknown 四档，考察输入、显示、无缝体验、Proton 系统支持。性能门槛是默认设置下 Deck 800p ≥30 fps、Steam Machine 1080p ≥30 fps。有新构建、新 Proton 或用户反馈时会复测 [58]。
- 两家公开资料里都没有描述自动化测试框架，**未能证实**。

### 5. 兼容性数据库与遥测

#### 5.1 现有设计对比

| 数据库 | 评级模型 | 数据来源和审核 | 开放方式 | 可借鉴点 |
|---|---|---|---|---|
| ProtonDB | 2019-10 起改为结构化问卷推导档位（Platinum/Gold/Silver/Borked，Bronze 基本弃用）[63] | 需要 Steam 登录；每人只计最新一份报告 [检索摘要] | 每月导出 tar.gz，最新为 `reports_sep1_2026.tar.gz`（约 70 MB）[62] | 用问卷事实推导档位，比主观打分更一致 |
| CodeWeavers | 1 星“装不上”到 5 星“和 Windows 一样”[61] | 社区和员工评级，BetterTester 分档审核 [59] | 网站；22,667 个应用、4,617 个金牌、4,067 个 CrossTie [60] | 安装配方（CrossTie）和评级绑定 |
| AppleGamingWiki | 按运行方式（原生、CrossOver、Parallels、GPTK……）分别评 Perfect/Playable/Runs/Menu/Unplayable [64] | MediaWiki 人工编辑 | wiki | 同一游戏按方法分别给结论 |
| MacGamingDB | 6 档（Excellent 到 Unplayable），1,575 份报告 [65] | 需要登录，报告带 FPS | 未见 API | 报告里包含 FPS 和硬件信息 |
| Highball-db | `status`（verified-local / community / blocked-anticheat…），`renderer`，`provenance`，`verified{chip, macos, engine, fps, how}`，`knownIssues[{symptom, cause, fix}]` [66] | `highball report` 自动生成 GitHub issue，打上 accepted 标签后并入；有校验器和 CI | git 仓库；另有由 ProtonDB 推导的约 12,500 条预测 | **结构最接近 Cider 的需要** |
| umu-protonfixes | 每个游戏一个 Python 文件（winetricks、DXVK 配置、参数） [67] | PR 审核 | git 仓库 | 跨商店的游戏 ID 映射（umu-database） |

#### 5.2 Cider 的配置和报告模型 [推断]

- **游戏标识**：`store_ids{steam, egs, gog}`，加上主 exe 的 SHA-256 和 PE VersionInfo。
- **Profile**：`backend`、`env{}`、`dll_overrides{}`、`winetricks[]`、`registry[]`、`cpu_path`、`min_engine`、`max_engine`、`fps_cap`、`metalfx`、`game_mode_shim`。
- **Report**：自动采集 `chip`、`ram`、`macos build`、`engine hash`、`backend version`、`profile hash`，HUD 统计（p50/p95 帧间隔、编译次数），结果按问卷答案计算，不让用户直接打总分（ProtonDB 的做法）。
- **状态计算**：按“profile × 引擎大版本”聚合；老版本的报告随时间降权；员工或 CI 验证（Highball 的 `verified-local`）优先级最高。

#### 5.3 签名分发

- **最简方案**：profile 包用 minisign 签名。它基于 Ed25519，可以带一段受签名保护的“trusted comment”，用来放版本号和过期时间，防降级 [69]。
- **长期方案**：采用 TUF（规范 1.0.36，2026-08-05）的 root / targets / snapshot / timestamp 角色模型，防回滚、冻结和混搭攻击 [68]。
- 客户端离线也能用本地的最后一版有效数据。

#### 5.4 遥测与审核

- **默认关闭，由用户主动开启。**
  - 自动报告只包括：崩溃签名（`.ips` 的 `exception` 加前几帧哈希；winedbg minidump 的模块和偏移）、启动是否成功、HUD 统计；
  - 不包括用户名路径和进程参数（写入前脱敏）。
- 社区报告走 GitHub issue 或 Web 表单，靠 CI 校验和人工分档审核（BetterTester 模式）；可重复的报告（同一 profile hash 在不同机器上结果一致）自动加权。

### 6. 2025–2026 公开基准

| 对比 | 结果 | 来源和日期 | 置信度 |
|---|---|---|---|
| 《赛博朋克 2077》原生版 vs CrossOver | 原生只快约 3%（Andrew Tsai）；M3 Max 1080p High 78 fps，开 MetalFX 质量档 104 fps；M1 Air 8 GB 约 12 fps | TechSpot 2025-07-21 [36] | 中 |
| 同上，M5 MacBook Pro 16 GB | 原生 59.8 vs CrossOver 25 56.35 fps（1080p 中画质，未说明后端） | NoobFeed 2025-11-28 [70] | 中-低 |
| 同上，M1 Max 64 GB | 原生约 74 vs CrossOver 约 64 fps，但两边画质设置不同 | BlendLogic 2026-06 [71] | 低 |
| D3DMetal 同步开销 | 约 42% 帧时间影响（《死亡搁浅 2》，M2 Max，macOS 26.4） | Apple 论坛 2026-04 [26] | 低-中 |
| DXMT vs D3DMetal / DXVK+MoltenVK | 只有定性描述：“很多场景 DXMT 能稳 60 fps” | DXMT 讨论帖 2024-07-30 [72] | 低 |
| 各后端的选择建议 | “按游戏选择”：DXVK 成熟稳妥，D3DMetal 在 DX12 上有时最快，DXMT 常能救回有问题的 DX11 游戏 | Bourbon 26 博客 2026-07-26 [73] | 低（没有数据） |
| CrossOver vs Parallels vs Whisky | Parallels Desktop 26 于 2025-08-26 发布 [74]，论坛称仍不支持 DX12 **[中-低]**；Whisky 于 2025-04 宣布停止开发 [75]，此后没有新的对比基准 | — | — |

**结论**：没有同机、同设置、多后端、带帧时间分布的严格公开对比。Cider 应该把 4.3 的流水线做成公开基准（固定存档或 benchmark 模式，每个后端跑 3 次取中位数，报告 p1/p95），并公开原始数据。

## 对 Cider 的启示与建议

**P0（MVP 之前）**

1. **开发环境**：装完整 Xcode（xctrace、Instruments、`metal-tt`）。另准备一台 macOS 27 机器或卷，配 Xcode 27（`gpucapture` / `gpudebug`）[22]。开发机的 8 GB 只作为“低配”档，另需一台 16 GB 以上的 CI 实体机 [51][52]。
2. **统一的诊断开关，集成在 `cider run` 和 UI 里**：
   - HUD、HUD 日志、编译日志 [37][38]；
   - API/Shader 验证 [39][40]；
   - GPU 捕获（`MTL_CAPTURE_ENABLED`，27 上调 `gpucapture`）[41][23]；
   - WINEDEBUG 预设（`+seh,+tid,+loaddll`、`warn+all`）[43][45]；
   - DXVK、DXMT、D3DMetal 各自的日志。
   注意：Metal HUD 的环境变量要能传进 Wine 子进程；用 `open` 启动时要用 `--env` [32]。
3. **一键生成支持包**：
   - 内容：`.cxlog` 风格的 Wine 日志、系统日志中 `D3DM` 和 `com.apple.metal.hud` 相关条目 [13][38]、`.ips` 文件（解析 `translated`、`exception`）[46]、winedbg minidump [44]、profile 和 prefix 的注册表差异、硬件和 OS 信息、Rosetta 是否安装；
   - 默认脱敏。
4. **着色器缓存管理器**：
   - 按游戏把 DXVK 和 DXMT 的缓存重定向到 `~/Library/Caches/Cider/<game>/<backend>/<backend-version>/`（`DXVK_SHADER_CACHE_PATH`、`DXMT_SHADER_CACHE_PATH`）[2][15][82]。`DXMT_SHADER_CACHE_PATH` 必须是以 `/` 开头的 Unix 绝对路径，传 Windows 路径（`C:\...`）或相对路径会被忽略、退回默认目录 [82]。DXMT 的缓存文件已经按 Metal 版本命名（`shaders_<metal_version>.db`），Cider 只需再按 DXMT 版本分目录；
   - 管理 D3DMetal 的目录 `DARWIN_USER_CACHE_DIR/d3dm/<exe 名>`：升级后端时清理，编译完成后自动快照，下次启动前恢复（参考 007 Shader Commander）[12][19]。这个路径不是 Apple 承诺的稳定接口，要按 D3DMetal 版本探测，找不到时降级为“只清理不快照”；
   - Metal 系统缓存按 bundle ID 分目录 [16][84]：Wine 进程从 `.app` shim 启动还是作为裸二进制启动，会决定缓存落在 `<bundleID>/com.apple.metal` 还是顶层 `com.apple.metal`，需要实测后再决定清理和备份的范围；
   - 用 HUD 的 `cached` 字段统计命中率 [38]。
5. **Game Mode 实验**：实现 per-game `.app` shim（`LSApplicationCategoryType` 加 `LSSupportsGameMode`，再加 `GCSupportsGameMode` 兼容 macOS 14/15；通过 LaunchServices 启动，主进程 `exec` 成 Wine loader），在 26.5 和 27 上验证能否激活 [31][32][33]；另外测一次“子进程开窗”的情况，因为 DTS 对此只给了带保留的判断 [33]。把结果写进兼容性数据库的 `game_mode` 字段。
6. **兼容性数据库 v0**：
   - git 仓库，每个游戏一个 JSON，schema 参考 Highball-db 并加上 `profile hash` 和 `engine range` [66]；
   - minisign 签名分发 [69]；
   - 导入 ProtonDB 月度数据作为先验（只作“预测”层）[62]。

**P1（Beta）**

7. **在 macOS 上每晚跑 winetest**：实体机，JUnit 输出，维护已知失败基线，只报新增失败 [47]。评估以“macOS”平台向 test.winehq.org 提交报告，补上上游的空白 [49]。
8. **游戏冒烟和性能 CI**：启动 → 第一次 present → 脚本输入 → 截图比对 → HUD 帧时间统计，每晚跑 Top-N；按 commit 告警 p95 帧间隔、编译次数和内存峰值的回归。
9. **转换层轨迹回放 CI**：DXMT / DXVK / vkd3d 的每个 MR 都回放 apitrace 和 GFXReconstruct 轨迹，比对校验和 [54][55][56]。
10. **一键二分工具**：同时支持 Wine commit 和组件版本（DXMT、D3DMetal、MoltenVK）的二分；配合模板 prefix 快照，保证每一步复现环境一致 [50]。
11. **统一限帧和帧节奏**：UI 上只有一个“限帧”设置，按后端落地：
    - DXVK：写进 per-game `dxvk.conf`（或 `DXVK_CONFIG`）的 `dxgi.maxFrameRate` / `d3d9.maxFrameRate`，直接用 DXVK 自带的限帧器 [2][80]；
    - DXMT：用 `d3d11.preferredMaxFrameRate`（见 04 号报告）；
    - D3DMetal 和其他后端：在 winemac 层实现基于 `present(afterMinimumDuration:)` 的限帧；
    - 都支持 `CAMetalDisplayLink` 和 VSync 开关 [27][28][29]。
12. **可选的遥测和社区报告**：流程是报告 → CI 校验 → 分档审核 → 并入数据库。

**P2（1.0 之后）**

13. **Pipeline 配方采集和本地预编译**：在 Cider 的 DXMT 分支里采集“shader hash + 状态键”，随 profile 分发，客户端在安装或空闲时用低 QoS 重放编译 [11][57]；对 Top 游戏试用 MTL4Archive 和 flexible pipeline [10][11]。
14. **公开基准平台**：发布多后端、多芯片的帧时间数据，成为社区可信的数据源。

## 风险

1. **D3DMetal 闭源**：缓存格式、日志和同步策略都不可控；GPTK 3→4 已经换过一次缓存格式 [20]，升级时要能批量失效和重建。
2. **macOS 27 行为变化**：
   - Metal 缓存按 bundle ID 分目录早就存在（macOS 14 已有）[16][84]，不是 27 的新变化；27 beta 上观察到的是 gpuarchiver / `archiveUsage.db` 的新行为，以及沙盒下缓存目录创建失败变为致命错误 [17] **[低，第三方 bug]**。真正的风险是 Wine 进程的 bundle 归属不确定。
   - Rosetta 进入倒计时：按 Apple 此前公布的计划，28 起只保留给部分老游戏（本轮未复核一手原文）。“升级 27 后 Rosetta 被移除、要手动重装”只有二手来源，而且据说后续版本已恢复自动安装提示 [86][87] **[低]**。
   - Wine 类进程的缓存归属和 Rosetta 可用性都需要在 27 上实机验证。
3. **Game Mode 可能拿不到**：系统有面向特定 bundle ID 的特殊处理 [33]，shim 方案可能无效或被后续版本收紧。DTS 对子进程开窗的判断本身带保留 [33]，153127050 在 26.1 release notes 里也没有列为已修复 [83]，只能靠实测确认。
4. **测试基础设施成本**：VM 不能代替实体 GPU [52]，也最多只能跑 2 台 [51]。设备实验室（M1–M5 × 内存档位 × 两个 OS 版本）的资金和维护是长期负担。
5. **DXVK 路线分叉**：DXVK 3.x 要求 Vulkan 1.4 [2]，MoltenVK 缺 GPL [6]，维护 macOS 分支的成本可能超过收益。
6. **遥测与隐私**：崩溃栈、路径和 exe 哈希可能暴露用户信息，必须默认关闭、先脱敏，并能审计。
7. **公开基准的可比性**：游戏更新、系统更新和温度墙都会导致结果不可复现；如果数据不可靠，反而会损害项目信誉。

## 未解问题

1. CrossOver 如何让 Wine 游戏触发 Game Mode？per-app shim 在 26.5 和 27 上是否有效？
2. Wine 映射的 PE 代码在 Rosetta 下是否只走 JIT？有没有跨进程的持久翻译缓存（对首次运行卡顿影响很大）？
3. Metal 系统缓存按 bundle ID 分目录 [16][84]。**Wine 进程继承哪个 bundle ID**：从 `.app` shim 启动和作为裸二进制启动时，缓存分别落在 `<bundleID>/com.apple.metal` 还是顶层 `com.apple.metal`？macOS 26 和 27 上的容量上限和淘汰策略是什么？
4. D3DMetal 缓存在什么条件下失效？007 First Light 每次冷启动都重编 [18]，原因可能是缓存键包含易变信息，**未能证实**。
5. `gpucapture` / `gpudebug` 是否随 CLT 提供，还是必须装 Xcode 27？能否附加到 Rosetta 下的 x86_64 Wine 进程？
6. DXVK 3.x 在 MoltenVK 1.4.2 上缺哪些必需特性？metalsharp 分支的实际可用性如何？
7. 帧节奏：DXMT、D3DMetal 用的是 `present` 还是 `presentAfterMinimumDuration`？在 ProMotion 屏上的实际行为需要用 Metal System Trace 测。
8. 实体 Mac 上的 winetest 通过率基线和运行时长（8 GB 机器能否在一夜之内跑完）。
9. 在 Apple silicon 上设置 `nox86exec=1` boot-arg 是否需要先降低安全策略？
10. 升级到 macOS 27 是否真的会移除 Rosetta？需要 Apple 一手来源或实机升级验证 [86][87]。

## 参考来源

1. https://github.com/doitsujin/dxvk/releases/tag/v2.7 — DXVK 2.7（2025-07-05）删除 state cache
2. https://github.com/doitsujin/dxvk/releases/tag/v3.0 ；https://api.github.com/repos/doitsujin/dxvk/releases/tags/v3.0 — DXVK 3.0（2026-06-25）：dxbc-spirv、IR 磁盘缓存、要求 Vulkan 1.4、删除 `DXVK_FRAME_RATE` 环境变量（配置文件里的限帧选项保留）；3.1.1 于 2026-09-15 发布（releases API）
3. https://raw.githubusercontent.com/doitsujin/dxvk/master/README.md — DXVK_HUD、日志、缓存变量、GPL 说明
4. https://github.com/KhronosGroup/MoltenVK/releases ；https://api.github.com/repos/KhronosGroup/MoltenVK/releases?per_page=8 ；https://api.github.com/repos/KhronosGroup/MoltenVK/releases/tags/v1.4.0 — 1.4.0（2025-08-20，Vulkan 1.4，修复 pipeline cache 内的 shader cache miss）、1.4.1（2025-11-30）、1.4.2（2026-07-24）
5. https://github.com/KhronosGroup/MoltenVK/issues/1765 — 用 Metal Binary Archive 做 pipeline cache 的提案（2022-11-10，open）
6. https://github.com/KhronosGroup/MoltenVK/issues/1711 — VK_EXT_graphics_pipeline_library（2022-09-05，open）
7. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/MoltenVK_Configuration_Parameters.md — MVK_CONFIG_* 参数
8. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/MoltenVK_Runtime_UserGuide.md — pipeline cache 只存 MSL、扩展列表
9. https://developer.apple.com/documentation/metal/mtlbinaryarchive — MTLBinaryArchive（macOS 11+）
10. https://developer.apple.com/documentation/metal/mtl4archive ；https://developer.apple.com/tutorials/data/documentation/metal/mtl4archive.json — MTL4Archive（macOS 26.0+），可含 IR、GPU/系统专用二进制或两者
11. https://developer.apple.com/videos/play/wwdc2025/254/ — Explore Metal 4 games：采集、metal-tt、miss 处理、flexible pipeline、QoS、suspend/resume、residency
12. https://www.applegamingwiki.com/wiki/Game_Porting_Toolkit — D3DMetal 缓存清理路径（社区文档，非 Apple 稳定接口）
13. https://gist.github.com/lynkos/3999f629560219a81d4e2c083a4bf5b1 — GPTK 2.1 README 副本：D3DM 日志、捕获变量、缓存
14. https://github.com/philippremy/crossover-dx12-fix — 替换为 D3DMetal 4 后首启重建缓存、按 exe/GPU 家族分目录
15. https://github.com/3Shain/dxmt/releases ；https://api.github.com/repos/3Shain/dxmt/releases?per_page=6 — DXMT v0.71（2025-11-13）AIR/PSO 缓存；v0.73、v0.80（2026-04-23）
16. https://gist.github.com/aras-p/5a9ae8f1f7d9998f732aff26dfa62617 — Metal 系统着色器缓存位置；列出 `org.blenderfoundation` 等按应用分的目录，2026-06 的评论提到 `org.blenderfoundation.blender/com.apple.gpuarchiver`
17. https://github.com/anthropics/claude-code/issues/80472 — 第三方 bug（2026-07-23）：沙盒 helper 在 macOS 27 beta 上崩溃，涉及 `recordBinaryArchiveUsage`、`archiveUsage.db`、`com.apple.gpuarchiver`，缓存目录创建失败变为致命错误。不是 Apple 文档，不能证明按 bundle 分目录是 27 新引入的
18. https://appleinsider.com/articles/26/06/17/apples-game-porting-toolkit-4-is-a-big-improvement-for-modern-game-coders — GPTK 4、007 First Light 约 20 分钟编译
19. https://korben.info/en/game-porting-toolkit-4-windows-games-smooth-mac.html — 007 Shader Commander 备份/恢复缓存（20 分钟→3 秒）
20. https://skyfireworks.io/velocity/gptk4 — 二手分析：两阶段编译、GPTK 3→4 缓存格式变更
21. https://developer.apple.com/games/game-porting-toolkit/ — GPTK 4：Metal 4 评估环境、命令行 Metal 工具
22. https://github.com/apple/game-porting-toolkit — 需要 macOS 27 和 Xcode 27（gpucapture/gpudebug）；agent skills
23. https://raw.githubusercontent.com/apple/game-porting-toolkit/main/game-porting-skills/skills/using-gpucapture/SKILL.md — gpucapture、MTLCAPTURE_WAIT_FOR_SIGNAL
24. https://raw.githubusercontent.com/apple/game-porting-toolkit/main/game-porting-skills/skills/using-gpudebug/SKILL.md — gpudebug
25. https://developer.apple.com/videos/play/wwdc2026/357/ — macOS 27 引入 gpucapture/gpudebug，HUD 扩展
26. https://developer.apple.com/forums/thread/821733 — D3DMetal 过度同步分析及 DTS 回复（2026-04）
27. https://developer.apple.com/documentation/quartzcore/cametallayer/displaysyncenabled — VSync 开关，默认 true
28. https://developer.apple.com/documentation/quartzcore/cametaldisplaylink — macOS 14+，可变刷新率
29. https://developer.apple.com/documentation/metal/mtldrawable/present(afterminimumduration:) — 定间隔 present
30. https://github.com/elseform/gamma-wine-engine/releases/tag/engine-cx26.3-w11-gamma087-15 — 社区：displaySyncEnabled 跟随 SyncInterval
31. https://developer.apple.com/documentation/bundleresources/information-property-list/lssupportsgamemode ；https://developer.apple.com/tutorials/data/documentation/bundleresources/information-property-list/lssupportsgamemode.json — 可用性 macOS 26.0、iOS/iPadOS 18.6
32. https://developer.apple.com/documentation/macos-release-notes/macos-26-release-notes ；https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26-release-notes.json — Game Mode 修复（153125166）和已知问题（153127050）、`open --env`、nox86exec（136764433）
33. https://developer.apple.com/forums/thread/787702 — DTS（2025-06）：Game Mode 看起来不太可能迁移到子进程新建的窗口（带保留的判断）
34. https://support.apple.com/en-us/105118 — Game Mode 行为与要求
35. https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize — GPU 工作集建议上限
36. https://www.techspot.com/news/108739-how-well-does-cyberpunk-2077-run-m-series.html — 《赛博朋克》原生 vs CrossOver 约 3%，M1 Air 8 GB 约 12 fps（2025-07-21）
37. https://developer.apple.com/documentation/xcode/customizing-metal-performance-hud — MTL_HUD_* 全部变量
38. https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance ；https://developer.apple.com/tutorials/data/documentation/xcode/monitoring-your-metal-apps-graphics-performance.json — HUD 日志格式、shader signpost（含 `cached: 0|1`）
39. https://developer.apple.com/documentation/xcode/validating-your-apps-metal-api-usage — MTL_DEBUG_LAYER
40. https://developer.apple.com/documentation/xcode/validating-your-apps-metal-shader-usage — MTL_SHADER_VALIDATION
41. https://developer.apple.com/documentation/xcode/capturing-a-metal-workload-programmatically — MTL_CAPTURE_ENABLED（macOS 14+）
42. https://keith.github.io/xcode-man-pages/xctrace.1.html — xctrace record/export
43. https://man.archlinux.org/man/wine.1.en — WINEDEBUG 语法
44. https://man.archlinux.org/man/winedbg.1.en — winedbg --auto/--minidump
45. https://support.codeweavers.com/2-creating-a-debug-log — CrossOver 调试日志（.cxlog）
46. https://developer.apple.com/documentation/xcode/interpreting-the-json-format-of-a-crash-report — .ips 格式、`translated` 字段
47. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/gitlab/test.yml — Wine CI 测试任务
48. https://raw.githubusercontent.com/wine-mirror/wine/master/tools/gitlab/build.yml — `build-mac`（Tart，`winehq-sequoia-pristine`）和 `build-daily-mac`（`winehq-sonoma-pristine`），都不跑测试
49. https://test.winehq.org/data/ ；https://test.winehq.org/data/index.html — 测试结果，结果列没有 Mac（最新构建 4e819f054dd2，2026-09-25）
50. https://wiki.winehq.org/Regression_Testing — 回归二分流程
51. https://eclecticlight.co/2026/04/29/virtualisation-on-apple-silicon-macs-is-different/ — 最多 2 台 macOS VM、VM 限制
52. https://github.com/trycua/cua/blob/main/blog/gpu-passthrough-macos-vms.md — 半虚拟化 GPU 的特性报告（2026-08-11）
53. https://tart.run/faq/ — 嵌套虚拟化的限制
54. https://docs.mesa3d.org/ci/local-traces.html — Mesa 轨迹回放测试（piglit replayer）
55. https://github.com/apitrace/apitrace/wiki/WINE — 在 Wine 下使用 apitrace
56. https://github.com/LunarG/gfxreconstruct — D3D12/Vulkan 抓取与回放
57. https://github.com/ValveSoftware/Fossilize — Vulkan pipeline 序列化与重放
58. https://partner.steamgames.com/doc/steamhardware/compat — Steam Deck/Machine 兼容性评审
59. https://www.codeweavers.com/compatibility/beta — BetterTester 计划
60. https://www.codeweavers.com/compatibility — 数据库规模
61. https://www.codeweavers.com/compatibility/rating-system — 1–5 星定义
62. https://github.com/bdefore/protondb-data — ProtonDB 月度数据导出
63. https://boilingsteam.com/protondb-ratings-revised/ — 2019-10 评级改版
64. https://www.applegamingwiki.com/wiki/AppleGamingWiki:Editing_guide — 按方法评级
65. https://macgamingdb.app/ — Mac 专用、带 FPS 的报告库
66. https://github.com/gauthierpiarrette/highball-db — 每游戏 JSON、provenance、报告流程
67. https://github.com/Open-Wine-Components/umu-protonfixes — 每游戏修复脚本
68. https://theupdateframework.github.io/specification/latest/ — TUF 规范 1.0.36（2026-08-05）
69. https://jedisct1.github.io/minisign/ — Ed25519 签名、trusted comment
70. https://www.noobfeed.com/hardware/m5-macbook-pro-gaming-review — M5 原生 vs CrossOver（2025-11-28）
71. https://blendlogic.com/posts/cyberpunk-2077-on-mac.html — M1 Max 原生 vs CrossOver（2026-06）
72. https://github.com/3Shain/dxmt/discussions/9 — DXMT 定性对比（2024-07-30）
73. https://pyrosoft.pro/pages/bourbon26-blog.php?a=dxvk-vs-d3dmetal — 各后端的定性建议（2026-07-26）
74. https://www.parallels.com/newsroom/news/press-releases/20250826-parallels-desktop-26/ — Parallels Desktop 26（2025-08-26）
75. https://www.macrumors.com/2025/04/23/whisky-ends-mac-gaming-tool-crossover/ — Whisky 停止开发
76. https://www.macrumors.com/2026/09/10/macos-27-golden-gate-release-date/ ；https://blakecrosley.com/blog/macos-27-golden-gate-release — macOS 27 于 2026-09-14 发布；Rosetta 时间表（二手转述，本轮未复核 Apple 原文）
77. https://github.com/Gcenx/DXVK-macOS/releases ；https://github.com/metalsharp/DXVK-MacOS — DXVK-macOS 旧分支和新分支
78. https://dougallj.wordpress.com/2022/11/09/why-is-rosetta-2-fast/ — Rosetta 的 AOT/JIT 和 TSO
79. https://api.github.com/repos/doitsujin/dxvk/releases?per_page=8 — DXVK 发布时间：v2.7（2025-07-05）、v3.0（2026-06-25）、v3.0.1（2026-07-05）、v3.0.2（2026-07-17）、v3.1（2026-08-28）、v3.1.1（2026-09-15）
80. https://raw.githubusercontent.com/doitsujin/dxvk/master/dxvk.conf — `dxgi.maxFrameRate`、`d3d9.maxFrameRate`、`dxvk.maxFrameRate` 仍在文档中
81. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/MoltenVK/MoltenVK/Layers/MVKExtensions.def — MoltenVK 扩展列表：无 GPL、无 `pipeline_binary`，有 `EXT_pipeline_creation_cache_control`
82. https://raw.githubusercontent.com/3Shain/dxmt/main/src/dxmt/dxmt_shader_cache.cpp — `DXMT_SHADER_CACHE=0`、`DXMT_SHADER_CACHE_PATH`（只接受以 `/` 开头的绝对路径）、`shaders_<metal_version>.db`
83. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26_1-release-notes.json — macOS 26.1 release notes，未提到 153127050
84. https://github.com/mozilla-platform-ops/ronin_puppet/pull/1391 — macOS 14 CI 机器上清理 `org.mozilla.firefox/com.apple.metal`、`org.mozilla.firefox-gpu-helper/com.apple.metal`
85. https://9to5mac.com/2026/09/09/apple-confirms-macos-27-golden-gate-launch-date-september-14/ — macOS 27 于 2026-09-14 发布
86. https://www.paulthetall.com/rosetta-de-installed-on-mac-os-27-golden-gate/ — 社区博客：升级 27 后 Rosetta 被移除、可用 `softwareupdate --install-rosetta --agree-to-license` 重装（二手，未经 Apple 确认）
87. https://www.papercut.com/kb/Main/macos-rosetta-transition-end-of-life/ — PaperCut KB：Rosetta 过渡与终止支持（二手）

## 事实核查记录

2026-09-27 按独立事实核查结果修订。“确认”的条目只补充了细节和来源；“部分正确”和“被推翻”的条目已在正文对应位置改正。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| DXVK 2.7（2025-07-05）删除 state cache；3.0（2026-06-25）在 prefix 的 `AppData/Local` 做 IR 磁盘缓存（`DXVK_SHADER_CACHE_PATH` / `DXVK_SHADER_CACHE=0`），要求 Vulkan 1.4，删除限帧环境变量 | 确认 | releases API 核对了日期，并补上 3.0.1 / 3.0.2 / 3.1 / 3.1.1 的日期。3.0 删除的只是 `DXVK_FRAME_RATE`，见下面“被推翻”一条 [2][79][80] |
| MoltenVK 不支持 GPL（#1711）；pipeline cache 只存 MSL（#1765）；1.4.0（2025-08-20）支持 Vulkan 1.4 | 确认 | `MVKExtensions.def` 里没有 GPL 和 `pipeline_binary`；1.4.1、1.4.2（2026-07-24）都没加 GPL 或 binary archive [4][5][6][81] |
| DXMT v0.71（2025-11-13）把 AIR 和 PSO 缓存放在 `DARWIN_USER_CACHE_DIR/dxmt/<exe>`；D3DMetal 缓存在 `d3dm/<GAME>/shaders.cache` | 确认 | 补充：DXMT 文件名 `shaders_<metal_version>.db`；D3DMetal 目录按可执行文件名命名，路径来自 AGW 和 GPTK README 副本，不是 Apple 的稳定接口，GPTK 版本之间可能变化。表格中的 `<GAME>` 已改为 `<exe 名>` [12][13][15][82] |
| `LSSupportsGameMode` 从 macOS 26.0 起可用；26 release notes 说 Terminal 直接派生的二进制不激活 Game Mode（153127050）；DTS 说 Game Mode 不迁移到子进程窗口 | 部分正确 | 可用性元数据是 macOS 26.0 和 iOS/iPadOS 18.6；macOS 14/15 靠 `LSApplicationCategoryType` 和 `GCSupportsGameMode`。153127050 没有在 26.1 release notes 里列为已修复。DTS 的回答是带保留的“看起来不太可能生效”，不是确定规则。已改摘要、§2.4、P0 第 5 条和风险 3；shim 的 Info.plist 加上 `GCSupportsGameMode` [31][32][33][83] |
| Metal 4 归档（MTL4Archive）查找会因“没有匹配 pipeline、OS 不兼容、GPU 架构不兼容”而 miss，需退回设备端编译（WWDC25 session 254） | 确认 | 两点保留：“分发预编译二进制不现实”是推断，归档可以多架构构建、目标匹配时能命中；`metal-tt` 需要完整 Xcode。已改摘要和 §1.5 表格的措辞 [10][11] |
| 上游 Wine GitLab CI 在 Tart `winehq-sequoia-pristine` 上构建 macOS 但不测试；test.winehq.org 没有 macOS 结果（2026-09-25） | 确认 | 补充了漏掉的 `build-daily-mac`（`winehq-sonoma-pristine`）。test.winehq.org 的结论是“结果列没有 Mac”；核查者没能打开单次构建的报告列表，不能完全排除有归到 Linux/Wine 组的 macOS 报告 [47][48][49] |
| 摘要和 §2.3：DXVK 3.0 删掉了内置帧率限制器，所以 Cider 必须提供统一限帧器 | **被推翻** | 3.0 只删了 `DXVK_FRAME_RATE` 环境变量；`dxgi.maxFrameRate` / `d3d9.maxFrameRate` / `dxvk.maxFrameRate` 仍可通过 `dxvk.conf` 或 `DXVK_CONFIG` 使用。DXVK 路径直接用 DXVK 自带限帧器；Cider 自己的限帧器只用于 D3DMetal 和其他后端。已改摘要、§2.3 和 P1 第 11 条 [2][80] |
| §1.1 和风险 2：macOS 26 的 Metal 系统缓存在顶层 `com.apple.metal`，macOS 27 改为按 bundle ID 分目录（来源 claude-code #80472） | 部分正确 | 按 bundle ID 分目录早就存在（macOS 14 上的 Firefox 就是这样）；顶层目录存系统和无 bundle 进程的 shader。#80472 是第三方 bug，只能说明 27 beta 在 gpuarchiver / `archiveUsage.db` 上有新行为，以及沙盒下目录创建失败变为致命错误。关键问题改为“Wine 进程继承哪个 bundle ID”。已改摘要、§1.1 表格、P0 第 4 条、风险 2、未解问题 3 [16][17][84] |
| §1.1：DXMT 有 `DXMT_SHADER_CACHE=0` 和 `DXMT_SHADER_CACHE_PATH`（原标“中”） | 确认 | 源码已读，置信度升为“高”。`DXMT_SHADER_CACHE_PATH` 只接受以 `/` 开头的 Darwin 绝对路径，Cider 不能传 Windows 路径；缓存按 Metal 版本自动失效。P0 第 4 条已补充 [82] |
| §2.4 和 §4.4：26 release notes 修复了“LSSupportsGameMode 被忽略”（153125166）；`nox86exec=1` 可验证不依赖 Rosetta | 确认 | 补充了 136764433 和 `sudo nvram boot-args` 的设置方式。在 Apple silicon 上设 boot-args 可能要先降低安全策略，**未验证**，已列为未解问题 9 [32] |
| §1.1：MoltenVK 1.4.0 修复了 pipeline cache 内的 shader cache miss；§3.1：HUD 的 shader signpost 带 `cached: 0\|1` | 确认 | 无需更正；补充了 JSON 版文档链接 [4][38] |
| §4.4 和风险 2：macOS 27 于 2026-09-14 发布；它是最后一个完整 Rosetta 版本，28 起只留给部分老游戏；升级 27 后需 `softwareupdate --install-rosetta` 重装 | 部分正确 | 发布日期已确认。“升级会移除 Rosetta”只有博客和 PaperCut KB 等二手来源，其中一篇说后续版本已恢复自动安装提示，没有 Apple release note 佐证，降为“低”。28 的 Rosetta 限制是 Apple 此前公布的计划，本轮未复核。已改 §4.4 表格、风险 2，并新增未解问题 10 [76][85][86][87] |
