# 图形转译 II：D3D12、Vulkan-on-Metal（MoltenVK / KosmicKrisp）、D3DMetal、着色器转换、光追与超分

> 调研日期 2026-09-26（同日按独立事实核查修订，见文末“事实核查记录”）· 置信度说明：**[高]** = 已用一手来源核实（官方文档、源码、发布说明、许可证原文）；**[中]** = 二手来源，或一手来源里日期/年份显示不完整、需要推断；**[低]** = 单一非权威来源或纯推断。凡标“推断”的内容都没有直接来源。许可证条款多数为转述；除一句短引文外，完整原文请看对应链接。本报告不构成法律意见。

---

## 摘要

- **2026 年最大的变化是 KosmicKrisp。** 它是 LunarG 在 Mesa 中开发的 Vulkan-on-Metal 驱动，MIT 许可 [2][64]。LunarG 于 2025-10-30 发文宣布它通过 Vulkan 1.3 一致性认证 [1]（这是发文日期，Khronos 批准的具体日期文中没写）。2026-02-11 它随 Mesa 26.0 发布 [10]。2026-07-28 的 Vulkan SDK 1.4.357.0 把它切到 Metal 4 命令编码，从此强制要求 macOS 26，性能最多提升约 2.35 倍 [6]。2026-09-25 LunarG 宣布它通过 Vulkan 1.4 一致性认证 [7][8][64]。它现在**只支持 Apple Silicon + macOS 26 及以上** [3][9][64]。注意：2025-10 拿到 1.3 认证时，LunarG 的说法还是“在 macOS 15+ 上原生运行”[1]，macOS 26 的门槛是后来才定的。开发机（M3 + macOS 26.5）满足条件。
- **但 KosmicKrisp 现在既跑不了上游 DXVK，也跑不了上游 vkd3d-proton。** 2026-09 的 Mesa main 源码里，KosmicKrisp 没有暴露 `geometryShader` 和 `fillModeNonSolid`，也没有 `VK_EXT_transform_feedback`、sparse、光追、mesh shader 或 descriptor buffer/heap [11]。DXVK master 把 `geometryShader` 和 `fillModeNonSolid` 都列为**必需**特性 [17]。vkd3d-proton master 在创建设备时**硬性要求** transform feedback（`transformFeedbackQueries`），没有就直接返回 `E_INVALIDARG` [79]。LunarG 在 2026-02 的路线图中把 “Tessellation/Geometry” 列为 3–6 个月内的优先项 [3]。到今天 tessellation 已经完成，geometry 和 XFB 还没有。源码里 XFB 的 properties 结构已经填好，看起来是在铺路，但扩展本身没有暴露 [11][65]。
- **MoltenVK** 最新稳定版是 1.4.2：Whats_New 标注日期 2026-07-20，GitHub Release 发布于 2026-07-24 [13][72]。它使用 Apache-2.0 许可，属于 Vulkan 1.4 portability subset，**不是一致性实现** [4][13][16]。源码显示它仍然缺 geometry shader、transform feedback、sparse 和光追 [68][69]。它还把 robustness2 的 `robustBufferAccess2` 和 `nullDescriptor` 硬编码为 false [68]，所以上游 DXVK 和 vkd3d-proton 在 MoltenVK 上同样无法创建设备。有一个尚未合并的 PR（#2771，2026-07-12 提交）加入了需手动开启的实验性光追 [70]。
- **D3DMetal（GPTK）目前仍是 macOS 上 D3D12 兼容性最好的方案。** CrossOver 26.0.0（2026-02-10）内置 Wine 11.0、D3DMetal 3.0、DXMT v0.72 和 vkd3d 1.18。之后的 26.1.0（2026-04-09）、26.2.0（2026-06-09）和当前最新的 26.3.0（2026-07-21）都没有升级组件 [36]。WWDC26（2026-06）发布了 GPTK 4：D3DMetal 4 把 DX12 转成 Metal 4，DX11 仍走 Metal 3，只支持 Apple Silicon [25][27]。截至 2026-09-26，它看起来仍是 beta，能找到的最新可再分发版本是 4.0 beta 2 [76][83]。macOS 27 已于 2026-09-14 正式发布，只支持 Apple Silicon [74][75]。**D3DMetal 4 是否必须运行在 macOS 27 上，Apple 的一手资料没有写明（存疑）**；Apple 只把 macOS 27 和 Xcode 27 列为新 Metal 调试工具的要求 [73]。
- **D3DMetal 是闭源的 x86_64 组件（社区来源 [34]；4.0 beta 2 二进制的架构没有亲自核对），受 Apple GPTK 许可证约束。** 该许可证允许 "distribute the Apple Software solely for non-commercial purposes" [30]，允许把 D3DMetal.framework 作为整体单独分发，禁止修改和逆向；许可用途限于开发、测试或评估视频游戏 [30]。许可证文本经两份独立核查确认，但来源是 2026-09-23 新建的第三方仓库 dbc-hbin，没有和 Apple 自己的 GPTK 4 DMG 比对过 [30][83]。Whisky（已停更）、Gcenx 的 GPTK 构建、Heroic 和 dbc-hbin 都**以免费、非商业的方式再分发** D3DMetal [29][30][39][41]。CrossOver 是商业产品，据报道与 Apple 达成了协议，但协议没有公开 [37]。
- **建议：**
  - **短期：** 让用户自己从 Apple 导入 GPTK 的 D3DMetal，作为 D3D12 主后端，默认不内置。同时马上开始处理开源栈的阻塞项：XFB 是 vkd3d-proton 的硬阻塞，GS 和 fillModeNonSolid 是 DXVK 的硬阻塞。
  - **中期：** 把 KosmicKrisp 作为 Apple Silicon 上的主 Vulkan ICD，并向上游贡献 GS/XFB 模拟和线框填充模式。ARM64 原生化要提前做，因为 CodeWeavers 已经在测试原生 ARM64 的 CrossOver 27 预览版 [77]。
  - **长期：** 把 vkd3d-proton + KosmicKrisp 做成完全开源、可再分发的 D3D12 路径，用来对冲法律风险和平台风险（Rosetta 退场）。
- **超分和插帧：** macOS 26 起有 `MTLFXFrameInterpolator` 和 `MTLFXTemporalDenoisedScaler` [45][46]。D3DMetal 通过 `D3DM_ENABLE_METALFX=1` 和 `nvngx-on-metalfx` 把 DLSS 转成 MetalFX [31][32]；DXMT 自带开源的 DLSS-SR→MetalFX 实现 [49]，适合作为 Cider 开源 nvngx shim 的参考。

---

## 详细调研

### 1. D3DMetal / Game Porting Toolkit：版本、特性、限制与 Wine 集成

#### 1.1 版本时间线

| 版本 | 时间 | 要点 | 置信度 |
|---|---|---|---|
| GPTK 1.x | 2023-06（WWDC23） | 首个 D3DMetal，支持 DX11/12，Wine 部分基于 CrossOver 22.1.1 源码 [39] | 高 |
| GPTK 2.0 | 2024-06（WWDC24） | macOS 15 上 Rosetta 支持 AVX/AVX2（用 `ROSETTA_ADVERTISE_AVX=1` 向程序宣告），M3 上可用 DXR（`D3DM_SUPPORT_DXR=1`）[31] | 高 |
| GPTK 2.1 | Gcenx 构建发布于 3 月 12 日，按内含 GStreamer 1.26.0 推断为 2025 年 [29] | 修复版本 | 中 |
| GPTK 3.0 beta | 2025-06（WWDC25）；Gcenx beta1 构建于 6 月 12 日，要求 macOS 15 [29] | Metal 4、sparse buffer/texture、性能洞察、实验性 MetalFX；DLSS→MetalFX（`D3DM_ENABLE_METALFX`，macOS 26）[25][32] | 中–高 |
| GPTK 3.0 正式 | Gcenx 构建发布于 12 月 5 日，按 GStreamer 1.26.5.1 推断为 2025 年；3.0-3 于 3 月 3 日，推断为 2026 年 [29] | CrossOver 26.0.0（2026-02-10）采用 D3DMetal 3.0 [36] | 中 |
| GPTK 4 beta | 2026-06（WWDC26）。截至 2026-09-26，能找到的最新版本是 4.0 beta 2（第三方于 2026-09-23 再分发）[30][83]；没找到正式版 [76] | D3DMetal 4：DX12→Metal 4，DX11 仍走 Metal 3；只支持 Apple Silicon；新增 agent skills 和 Metal 命令行工具 [26][27]。Apple 只把 macOS 27 + Xcode 27 列为新 Metal 调试工具的要求 [73]；D3DMetal 4 本身是否要求 macOS 27，Apple 没有说明（存疑，见下） | 中–高 |

Apple 的 GPTK 页面现在把 “Game Porting Toolkit 4” 列为当前版本。它的评估环境支持 Metal 4、sparse、HDR、超分、降噪和插帧，并注明兼容 Homebrew 和 CrossOver [25]。第三方基准只作参考：Andrew Tsai 测得 Black Myth: Wukong 在 M3 Max 上从 60 升到 80 fps [28]；AppleInsider 测得 Cyberpunk 2077 提升约 10% [27]。这些都不是 Apple 官方数字。[低–中]

**GPTK 4 的系统要求（存疑）：** 两份事实核查的说法不完全一致。
- **核查 A** 的依据：
  - Apple 的 `apple/game-porting-toolkit` 仓库只说 macOS 27 提供新的 Metal 调试工具，要配合 Xcode 27 [73]；
  - WWDC26 游戏指南没有写 D3DMetal 4 的系统要求 [26]；
  - “完整特性需要 macOS 27 beta”的说法来自第三方，比如 Korben 是在 macOS 27 beta 上跑的基准 [28]。
- **核查 B** 的说法：GPTK 4 面向 macOS 27 “Golden Gate”，而 macOS 27 已于 2026-09-14 正式发布（9to5Mac、MacRumors 报道）[74][75]，所以已经不需要 beta 系统。

两份核查都认为原稿的“需要 macOS 27 beta”已经过时。它们的分歧在于：D3DMetal 4 的 DX12→Metal 4 转译本身是否依赖 macOS 27。我的判断是：Metal 4 从 macOS 26 起就有，D3DMetal 4 **有可能**在 macOS 26 上运行，但没有一手来源支持，只能在开发机（macOS 26.5）上实测确认。[低]

**CrossOver 27 预览版：** 据 AppleInsider 2026-07-31 报道 [77]，CodeWeavers 正在测试原生 ARM64 的 CrossOver 27 预览版，目标是 2027 年初正式发布。这个构建**不带 D3DMetal**，D3D12 支持“稍后提供”。CrossOver 26 是最后一个支持 Intel 的版本。[中]，单一媒体来源。

#### 1.2 功能与限制
- **API 覆盖：** 只有 64 位 D3D11/D3D12；Sikarugir 文档也写明 “64Bit Direct3D 11 & 12” [40]。GPTK 里有 d3d10 转发 DLL [33]，但 D3D9 及更早版本不支持。[高]
- **CPU 指令：** D3DMetal 本身跑在 x86_64 Wine 进程里，经 Rosetta 2 翻译。macOS 15 起 Rosetta 支持 AVX/AVX2，但只有设置 `ROSETTA_ADVERTISE_AVX=1` 后才会通过 cpuid 向程序宣告 [31]。[高]
- **内存：** 官方建议 16 GB 或以上 [29][31]。开发机只有 8 GB，测试 AAA D3D12 游戏会很吃紧。
- **已知缺口（2026-03 开发者论坛）** [35]：
  - `DXGI_FEATURE_PRESENT_ALLOW_TEARING` 返回 0；
  - `D3D12_OPTIONS2.DepthBoundsTestSupported` 在不支持的硬件上返回 false。Apple DTS 确认并非所有 Apple Silicon 都支持 depth bounds；KosmicKrisp 源码里 `depthBounds` 也只在 `gpu_apple_family >= 10` 时开启 [11]；
  - 外接 HDR 显示器上 `IDXGIOutput6::GetDesc1().ColorSpace` 返回 SDR。
- **反作弊 / DRM：** 大多不兼容 [31]。

#### 1.3 与 Wine 的技术集成（逆向观察，[中]）
- **目录结构**（两份事实核查都确认；mybyways 的 GPTK 3.0 指南写于 2025-12-14，2026-02-11 更新 [32]）：
  - `redist/lib/external/` 放 `D3DMetal.framework` 和 `libd3dshared.dylib` [31][32]；
  - `lib/wine/x86_64-windows/` 放 PE 转发 DLL，包括 d3d10、d3d11、d3d12、dxgi、nvapi64、nvngx-on-metalfx [33]；
  - `lib/wine/x86_64-unix/` 下对应的 `.so` 都是符号链接，指向 `../../external/libd3dshared.dylib` [33]。
  - PE 侧通过 Wine 的 unixlib 机制调用 dylib，dylib 再加载 D3DMetal.framework。framework 的查找顺序是：dylib 同目录，然后 `../Frameworks` [33]。
- **Wine 补丁需求（已按事实核查更正）：**
  - **Wine 的行为：** 上游 Wine 的 x86_64 `virtual_unwind` 在 `dlls/ntdll/signal_x86_64.c`（master 约第 110–121 行）。当某个栈帧没有 PE function entry 时，它用宿主（Unix）unwinder 查找 unwind 信息。如果这个帧不在任何已加载的 PE 模块里（`module == NULL`，即宿主或系统库代码），Wine 会打印 FIXME “calling personality routine in system library not supported yet”，并把该帧的 `LanguageHandler` 置空 [78]。
  - **原稿的错误：** 对 `LDR_WINE_INTERNAL` 的 builtin PE 模块，handler 是保留的。所以原稿说“不调用 builtin 模块中的 personality routine”不准确；社区 issue 的措辞本身也比较松 [33]。
  - **对 D3DMetal 的影响：** 真正受影响的是 D3DMetal 在 Unix 侧的 C++ 代码（`libd3dshared.dylib` 和 D3DMetal.framework）。异常一旦穿过这些宿主库帧，C++ 异常处理就会失败。GPTK 或 CrossOver 系的 Wine 带有相应修复 [33]。
  - **证据强度：** “反复打印 FIXME 后进程终止”这一现象只有一份社区来源（Whisky 分支的 issue，2026-07-31）[33]。机制上说得通，但没有独立验证。[中]
  - **对 Cider：** Wine 分支需要一个 GPTK/CrossOver 式的 unwinder 补丁，让宿主库帧的 personality routine 也能执行。这个假设要先在 Cider 自己的 Wine 上用测试验证，再定为硬性要求。
- **启用 DLSS→MetalFX：** 把 `nvngx-on-metalfx.dll/.so` 改名为 `nvngx.dll/.so`，并把 `nvapi64.dll` 放进 bottle 的 system32，再设置 `D3DM_ENABLE_METALFX=1` [32]。
- **UTM 的 d3dmetal-native（MIT）** [34]：它在非 Wine 进程里实现 D3DMetal 期望的宿主接口（窗口、事件、注册表、内存），说明这个宿主接口可以被独立实现。Cider 研究接口时可以参考，但不能分发 D3DMetal 本体。

**常用环境变量** [31][32]：`D3DM_SUPPORT_DXR`（M3 及以上，默认 0）、`D3DM_ENABLE_METALFX`、`D3DM_DXIL_PROCESS_DEBUG_INFORMATION` 配合 `MTL_CAPTURE_ENABLED`、`ROSETTA_ADVERTISE_AVX`、`MTL_HUD_ENABLED`、`WINEMSYNC`。

### 2. D3DMetal / GPTK 许可证与再分发

**许可证版本：** dbc-hbin 仓库里随 GPTK 4.0 beta 2 的 D3DMetal 附带了 `License.rtf`，标题为 Apple Inc. Software License Agreement for Game Porting Toolkit，标识 EA18380，日期 8/17/2023 [30]。可见 Apple 在 GPTK 4 beta 中仍沿用 2023 年的许可文本（[中]：没有拿到 GPTK 4 DMG 原件直接比对）。两份独立事实核查都直接读取了 `License.rtf`，确认标识、日期和下面的条款转述无误。但要注意来源本身：
- dbc-hbin/d3dmetal-redistributable 是 2026-09-23 才建立的第三方仓库，0 star，与 Apple 无关；
- 它的 release `gptk-4.0b2`（2026-09-23）里有约 23.6 MB 的 `D3DMetal.framework-4.0b2.zip`；
- “许可证随 4.0 beta 2 附带”只有这一个来源 [30][83]。

**主要条款（转述，原文见 [30]）：**
- **§1 定义：** “Framework” 指 D3DMetal.framework；“Redistributables” 指 `/redist` 目录下的组件。
- **§2A 授权：**
  - (i) 可以安装、内部使用和测试，但**唯一目的是开发、测试或评估面向 Apple 品牌产品的视频游戏**；
  - (ii) 可以转授给代表你行使 (i) 的第三方服务商；
  - (iii) 可以分发，但**只能用于非商业目的**，并须遵守 §2C。复制件必须保留全部版权声明。
- **§2C：**
  - 禁止在非 Apple 硬件上运行，或帮助他人这样做；
  - 禁止出租、出借、托管或出售；
  - Apple Software 作为一个整体提供，组件原则上不能拆开分发；**例外是 Framework 可以作为整体单独分发，Redistributables 的任何部分也可以单独分发**，但所有分发都受 2A(iii) 的非商业限制；
  - 禁止用于 service bureau、分时等服务。
- **§2D：** 禁止反编译、逆向、修改或制作衍生作品。
- **§5：** 违约即自动终止，终止后必须销毁所有副本。

**各项目的做法：**

| 项目 | 方式 | 来源 |
|---|---|---|
| Whisky | 免费；运行时下载自家的 WhiskyWine 包（其中含 D3DMetal）；README 致谢 D3DMetal。2025-04-09 宣布停止维护，作者推荐 CrossOver | [38][39] |
| Gcenx/game-porting-toolkit | 在 GitHub Release 中二进制再分发 GPTK Wine 和 D3DMetal，并提示用户遵守 Apple 的 License.pdf | [29] |
| Heroic | 通过 GitHub API 直接下载 Gcenx 的 GPTK 构建 | [41] |
| Sikarugir（原 Kegworks） | 提供 D3DMetal 开关，并明确说明 D3DMetal 的许可不能用于商业移植 | [40] |
| dbc-hbin/d3dmetal-redistributable | 附带许可证和 SHA256，再分发未修改的 4.0b2 framework | [30] |
| CrossOver | 商业产品，自 23.5（2023-09）起内置 D3DMetal；据报道与 Apple 达成“共同协议”，协议未公开 | [37]（[低–中]） |

**对 Cider 的含义：**
1. 只要 Cider **永久免费且不做任何商业化**，按 2A(iii) 分发整个 framework 在字面上有依据，Whisky、Gcenx 和 Heroic 都是这样做的。但**付费版、捐赠解锁、广告或企业版**都可能构成“商业目的”。
2. 终端用户拿 D3DMetal 日常玩游戏，是否属于“评估视频游戏”存在灰色地带。
3. Cider 不能修补 D3DMetal。如果为了修正 caps 在 dxgi 前面加代理 DLL，要评估是否构成“衍生作品”。

**最稳妥的做法是：**用户用自己的 Apple 开发者账号，从 developer.apple.com 下载 GPTK，亲自接受许可证；Cider 只负责从 DMG 导入。

### 3. vkd3d（上游）与 vkd3d-proton

- **vkd3d-proton（LGPL-2.1）** [21][79]：
  - **必需（README 列出）：** Vulkan 1.3；`VK_EXT_robustness2`；`VK_KHR_push_descriptor`；描述符索引，除 UniformBuffer 外每类至少 1,000,000 个 UpdateAfterBind 描述符，且 `VkPhysicalDeviceDescriptorIndexingFeatures` 需全部支持；`samplerMirrorClampToEdge`；`shaderDrawParameters`。
  - **必需（README 没写，但源码有硬检查）：** `VK_EXT_transform_feedback` 的 `transformFeedbackQueries`。依据是 master 的 `libs/vkd3d/device.c`（最近提交 2026-09-22）[79]，[高]：
    - 只有设备支持 `EXT_transform_feedback` 时才会查询 XFB properties（约第 1290 行）；
    - 随后在 `vkd3d_init_device_caps()` 中，如果 `transformFeedbackQueries` 为 0，就打印 `Lacking support for transform feedback.` 并返回 `E_INVALIDARG`（约第 2479 行）；
    - 其他硬检查还有 robustness2 的 `robustBufferAccess2` 和 `nullDescriptor`、push descriptor、maintenance5/6、`samplerMirrorClampToEdge`、`shaderDrawParameters` 和 vertex_attribute_divisor；
    - `geometryShader` 不做硬检查，只打日志。
  - **强烈推荐（今后可能改为必需）：** `VK_EXT_image_view_min_lod`、`VK_EXT_mutable_descriptor_type`、`VK_EXT_descriptor_buffer`。
  - **DXR** 可用 `VKD3D_CONFIG=dxr/nodxr/dxr12` 控制。`VK_EXT_descriptor_heap` 已合入，但要用 `VKD3D_CONFIG=descriptor_heap` 显式开启 [21]（[中]）。
- **dxil-spirv（MIT）：** 负责 DXIL（SM6.x）→SPIR-V；旧的 DXBC 通过 dxbc-spirv 处理 [22]。
- **上游 vkd3d（Wine 的 D3D12，LGPL）（已按事实核查更正）：**
  - vkd3d-shader 的**实验性 MSL（Metal Shading Language）目标早在 vkd3d 1.14（2024-11-21）就已加入**，要用构建开关 `-DVKD3D_SHADER_UNSUPPORTED_MSL` 开启 [80][81][82]；1.15 和 1.16 继续扩展了它；
  - 2.0 随 Wine 11.10（2026-05-30）发布，只是让已有的 MSL 目标支持像素着色器指定的 stencil reference 值 [23]。原稿写成 2.0 “新增” MSL 目标，是错的；
  - 2.1（2026-08-24）扩展了早已存在的实验性 GLSL 目标（间接寻址、原子操作、屏障），并加入 mesh 管线子对象支持 [24]。原稿写成 2.1 “加入” GLSL 目标，也是错的。GLSL 目标最早出现在哪个版本，没有核实；
  - MSL 和 GLSL 目标至今都还是实验性的；
  - CrossOver 26 使用的是 vkd3d 1.18 [36]。
- **历史：** CodeWeavers 在 2022–2023 年曾用 vkd3d + MoltenVK 逐个游戏支持 D3D12（首个是 Diablo II: Resurrected），要修大量 MoltenVK 和 SPIRV-Cross 的 bug，无法规模化 [57]。[中]

**在 MoltenVK 和 KosmicKrisp 上的可行性（已按事实核查更正）：**
- **MoltenVK：**
  - MoltenVK 宣告了 robustness2，但 main 分支 `MVKDevice.mm`（约第 636–638 行）把 `robustBufferAccess2` 和 `nullDescriptor` 硬编码为 false，`robustImageAccess2` 只在 Apple GPU 上为 true [68]。这与 highball issue 的说法一致 [54]。它也缺 transform feedback 和 geometry shader [68][69]。[高]。highball 还提到它缺 depth clip，这一项仍只有单一来源 [54]，[低]；
  - highball 还称，缺少 `VK_EXT_transform_feedback` 会让 vkd3d-proton 无法创建设备 [54]。这一点现在已在 vkd3d-proton 源码中证实，见上文 [79]。所以**上游 vkd3d-proton 在 MoltenVK 上无法创建设备**。
- **KosmicKrisp：** vkd3d-proton 的其他硬性要求它大多能满足，但**只缺 XFB 这一项，设备创建就会失败**：
  - 默认暴露：robustness2（KHR 和 EXT，含 robustBufferAccess2/nullDescriptor）、push descriptor、mutable descriptor [11]；
  - `VK_EXT_image_view_min_lod`（vkd3d-proton 的“强烈推荐”项）**默认不暴露**。扩展表里写的是 `.EXT_image_view_min_lod = KK_EXPERIMENTAL(IMAGE_VIEW_MIN_LOD)`，而 `KK_EXPERIMENTAL()` 是对 `MESA_KK_EXPERIMENTAL` 的运行时检查，要设置这个环境变量才会暴露。custom border color 也是这样 [11][66]。原稿说它“已暴露”，不准确；
  - `KK_MAX_DESCRIPTORS = 1<<20`（1,048,576，高于 100 万）；
  - maxPushConstantsSize 为 256 [11][12]；
  - 仍缺：**transform feedback（硬阻塞）**、descriptor buffer/heap、GS、sparse、光追、mesh、VRS [11]。
  - **结论（已核实，更正原稿的推断）：** 原稿认为 vkd3d-proton 在 KosmicKrisp 上“短期只能覆盖一部分游戏”，这是错的。**不打补丁的 vkd3d-proton 在 KosmicKrisp 上根本无法创建 D3D12 设备** [79]。所以 XFB 模拟是开源 D3D12 路径的 **P0 阻塞项**，不是覆盖率问题。补上 XFB 之后仍有缺口（推断）：
    - vkd3d-proton 不硬检查 GS，但用到 GS 的着色器仍会失败；
    - tiled resources 依赖 sparse；
    - DXR 依赖 RT。

### 4. MoltenVK（2026 年状态）

| 项目 | 状态 |
|---|---|
| 最新稳定版 | 1.4.2：Whats_New 标 2026-07-20，GitHub Release 发布于 2026-07-24（rc1 为 07-19）[13][14][58][72] |
| 1.4.2 新增 | `VK_EXT_sampler_filter_minmax`（Apple10 GPU + macOS 26 起）、macOS 上的 gl_DrawID；最低系统提高到 macOS 12 / iOS 15；SPIRV-Cross 大量更新 [13] |
| 1.4.1 | Whats_New 标 2025-11-24，GitHub Release 为 2025-11-30：新的描述符状态跟踪器和描述符 set/pool 实现，引入 CMake 构建 [13][14] |
| 1.4.0 | 2025-08-20：Vulkan 1.4，用 Metal fence 实现 barrier，启用 residency set [13] |
| 开发中 | 1.4.3（Whats_New 标“Released TBD”）加入 `VK_EXT_multi_draw` 和 `VK_EXT_nested_command_buffer` [13] |
| 实验中（未合并） | PR #2771（2026-07-12 提交，截至核查时仍未合并）加入需手动开启的实验性光追：`MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING=1`，包括加速结构、ray query 和 RT 管线 [70] |
| 一致性 | portability subset，需要 `VK_KHR_portability_enumeration`；README 写明它“not fully compliant”。LunarG 称它“接近一致”，但缺少部分基础功能 [4][16] |
| 缺失（源码核实） | `MVKDevice.mm` 从 `mvkClear(&_features)` 开始，此后从未设置 `geometryShader` 或任何 sparse 特性；`MVKExtensions.def` 里也没有 transform_feedback、ray_tracing/ray_query、mesh_shader 或 sparse 相关扩展 [68][69]。GS 的 issue #1524 和 XFB 请求至今都未解决 [71]。注意：Runtime UserGuide 的 “Known Limitations” 只列了 pipeline statistics query、PVRTC 和 allocation callbacks [15]；原稿把这些缺失也归到 [15]，引用不准确 |
| robustness2 / maintenance | `robustBufferAccess2` 和 `nullDescriptor` 硬编码为 false，`robustImageAccess2` 仅 Apple GPU 为 true [68]；支持 `VK_KHR_maintenance5/6` [69]；`fillModeNonSolid` 为 true [68] |
| 许可 / 平台 | Apache-2.0；支持 Intel 和 Apple Silicon，覆盖 macOS/iOS/tvOS/visionOS [16] |

**定位：**
- MoltenVK 的着色器转换依赖 SPIRV-Cross，GS/XFB 在短期内看不到上游计划 [71]。
- 上游 DXVK master 在 MoltenVK 上**无法创建设备**：DXVK 必需的 `geometryShader`、`robustBufferAccess2` 和 `nullDescriptor` 它都不满足 [17][68]。上游 vkd3d-proton 同样不行，因为它缺 XFB，robustness2 的两项也不满足 [68][79]。
- Whisky 一类项目因此只能把 DXVK 固定在 Gcenx 的 DXVK-macOS 1.10.3 分支 [55]。
- metalsharp/DXVK-MacOS 声称基于 DXVK 3.1 做了 MoltenVK 适配补丁，涉及 dxbc-spirv，以及给 Wine 加 portability enumeration 标志 [59]。[低]，未验证能否实际运行。
- **结论（已按 2026-09 的平台变化更新）：** 对 Cider 来说，MoltenVK 是兜底方案，不是前进方向。它兜底的范围是：停留在 macOS 26 及以下的 Intel Mac，以及还在 macOS 14–15 上的 Apple Silicon Mac。原因有三：
  - macOS 27（2026-09-14 发布）只支持 Apple Silicon [74][75]，Intel Mac 最高只能到 macOS 26；
  - CrossOver 26 是最后一个支持 Intel 的 CrossOver [77]；
  - Intel 支持对 Cider 而言是遗留需求，优先级应该低。

### 5. KosmicKrisp（重点）

#### 5.1 时间线

| 日期 | 事件 | 来源 |
|---|---|---|
| 2024-11 | 项目启动，Google 资助，用于 macOS 上的 Android Emulator | [1][3] |
| 2025-10-30 | LunarG 发文宣布通过 Vulkan 1.3 一致性（从启动算起 10 个月）。这是发文日期，Khronos 批准的具体日期文中没写。当时的说法是“在 macOS 15+ 上原生运行”；这篇文章没提 MIT，MIT 许可的依据是 [2][64] | [1] |
| 2025-10（XDC 2025） | 架构：Mesa NIR 加自研 NIR→MSL，不依赖 SPIRV-Cross；MIT 许可 | [2] |
| 2026-02-02 | Vulkan SDK 1.4.341.0：KosmicKrisp 进入 beta | [5] |
| 2026-02-11 | Mesa 26.0.0 正式包含 KosmicKrisp | [10] |
| 2026-02（Vulkanised） | 路线图：只支持 Apple Silicon、macOS 26+、Metal 4；3–6 个月优先做 Tessellation/Geometry、性能、1.4 一致性、shader object 和 descriptor heap；之后是 RenderDoc 和 iOS | [3] |
| 2026-07-28 | SDK 1.4.357.0：完整暴露 Vulkan 1.4；改用 Metal 4 命令缓冲编码（需要 macOS 26+）；性能最多提升约 2.35 倍 | [6] |
| 2026-09-25 | 宣布通过 Vulkan 1.4 一致性；新增 tessellation、Robustness2、multi-draw；面向 M1 及以后的 Apple Silicon、macOS 26+、Metal 4；9 月 29 日随 SDK 发布 | [7][8][64] |
| 2026-09-29 | XDC 2026 更新演讲 | [7] |

#### 5.2 源码级特性核对
依据 2026-09 的 Mesa main 镜像 `src/kosmickrisp/vulkan/kk_physical_device.c` [11]。该镜像最后一次提交是 2026-09-26，涉及此文件的最近 kk 提交是 2026-09-21/22 [65]。

**已支持：**
- API 版本：`VK_MAKE_VERSION(1, 4, ...)`；
- tessellationShader、shaderTessellationAndGeometryPointSize、dualSrcBlend、multiViewport、depthClamp、shaderInt64、BC 纹理压缩、shaderCullDistance、shaderResourceMinLod；
- robustBufferAccess2 和 nullDescriptor、depthClipEnable、mutable descriptor、push descriptor、maintenance5/6、load_store_op_none、multi_draw、extended_dynamic_state3；
- `depthBounds` 只在 Apple GPU family ≥ 10 时开启。

**未支持：**
- `geometryShader`、**`fillModeNonSolid`**（线框填充模式）、`sparseBinding`、`shaderFloat64`、`pipelineStatisticsQuery`、`wideLines`；
- transform feedback 扩展、descriptor buffer/heap、shader object、ray query、mesh shader、fragment shading rate；
- **要开实验开关才暴露**（`KK_EXPERIMENTAL(...)`，即运行时检查 `MESA_KK_EXPERIMENTAL` [66]）：custom border color（`KK_EXPERIMENTAL(CUSTOM_BORDER)`）和 **`VK_EXT_image_view_min_lod`**（`KK_EXPERIMENTAL(IMAGE_VIEW_MIN_LOD)`）。环境变量里具体填什么值，要到 kk 源码的解析表里确认。

**已在铺路、但还没暴露：** `kk_get_device_properties` 已经填好了 `VK_EXT_transform_feedback` 的 properties 结构（`maxTransformFeedbackStreams = 4`、`transformFeedbackQueries = true`），也填了 shader_object 的 limits，但这两个扩展都没进扩展表 [11]。2026 年 8–9 月的 kk 提交主要是 sampler、MSL 4.1 和 deferred_host_operations，没有加入 GS 或 XFB 的；也没找到公开的 GS/XFB merge request [65]。

**调试变量** [9]：`MESA_KK_DEBUG`、`MESA_KK_GPU_CAPTURE`、`MESA_KK_EXPERIMENTAL`、`MESA_KK_DISABLE_WORKAROUNDS`。

**与 DXVK master 的必需特性对照（已按事实核查更正）** [17]：事实核查用脚本把 `dxvk_device_info.cpp` 里所有 `ENABLE_FEATURE(..., true)` 的特性名和 `kk_physical_device.c` 逐一比对。
- **有两个硬阻塞，不是一个：**
  - `geometryShader`：约第 855 行 `ENABLE_FEATURE(core.features, geometryShader, true)`；
  - `fillModeNonSolid`：约第 852 行 `ENABLE_FEATURE(core.features, fillModeNonSolid, true)`。
  - KosmicKrisp 两个都没暴露。原稿只写了 GS。对比一下，MoltenVK 设置了 `fillModeNonSolid = true` [68]。
- `robustBufferAccess2`、`nullDescriptor`、`depthClipEnable`、`maintenance5/6`、`dualSrcBlend`、`multiViewport` 都已满足。
- transformFeedback、descriptorHeap/Buffer、tessellation 在代码里都标为可选。DXVK wiki 把 XFB 列为 3.x 的要求 [19]，与代码不一致，以代码为准。
- 这个结论在 DXVK v3.1（2026-08-28）和 v3.1.1（2026-09-15）之后仍然成立 [67]。
- 此外，DXVK 3.0（2026-06-25）要求 Vulkan 1.4 级驱动，默认使用 `VK_EXT_descriptor_heap`，着色器编译改用 dxbc-spirv [18][20]。
- **含义：** 要让上游 DXVK 在 KosmicKrisp 上跑起来，光有 GS 模拟不够，还要支持线框填充模式。

**可行性证据：** Asahi Linux 的 Honeykrisp（同在 Mesa 中，面向同一 Apple GPU）早在 2024-10 就用 compute 着色器模拟了 GS 和 tessellation，在 Linux 上跑通了 DXVK 和 vkd3d-proton [62]。LunarG 也强调 Mesa 里已有大量现成的模拟代码 [3]。推断：KosmicKrisp 补上 GS/XFB 在技术上可行，但要经 Metal 间接实现，性能开销会比 Honeykrisp 更难控制。

#### 5.3 分发方式
LunarG 推荐在 app bundle 中放：
- `Contents/Frameworks/libvulkan_kosmickrisp.dylib`；
- Vulkan loader `libvulkan.1.dylib`；
- `Resources/vulkan/icd.d/libkosmickrisp_icd.json`。

MoltenVK 的 ICD 可以并列放置：Intel 用 MoltenVK，Apple Silicon 用 KosmicKrisp [3]。社区已有自动化的 universal 构建（squidbus/mesa-kosmickrisp）[60]。

**从源码构建（已按事实核查更正）：** Mesa 的 KosmicKrisp 文档把以下各项列为构建要求 [9]：
- 排在第一位的是 **“Xcode and command line tools”**；
- meson 1.9.1+、cmake、pkg-config、LLVM 20.1.8+、spirv-llvm-translator、spirv-tools；
- Python 包 mako、packaging 和 pyyaml。

需要修正两点：
- 原稿列了 libclc，但核查者转述的文档清单里没有它，构建时以文档为准；
- 开发机现在只有 CLT，没有 Homebrew。**只有 CLT 可能不够**，应该计划安装完整 Xcode（按核查者的说法，它同时提供 Metal 工具链）。

**结论：**
- KosmicKrisp 许可宽松（MIT），实现一致，有 Google 和 LunarG 持续投入，而且它的平台要求（Apple Silicon + macOS 26+）与 Cider 的主目标重合，**应当作为 Cider 的主 Vulkan 驱动**。
- 但到 2026-09，它**还不能承载 DXVK 和 vkd3d-proton 这套 Proton 式技术栈**，两者都无法创建设备：
  - DXVK 卡在 GS 和 fillModeNonSolid [17]；
  - vkd3d-proton 卡在 XFB [79]。
- XDC 2026（2026-09-29）上的 “KosmicKrisp production ready!” 演讲可能会改变这个时间表 [7]。

### 6. 着色器转换链

| 路径 | 用途 | 许可 | 备注 |
|---|---|---|---|
| Apple Metal Shader Converter（MSC） | DXIL→Metal IR/metallib | Apple 专有 EULA | 当前为 4.0 beta。支持 SM6.0–6.6，包括 SM6.3 光追、SM6.5 mesh/amplification、SM6.6 动态资源。提供 CLI 和 `libmetalirconverter` C API；要求 argument buffers Tier 2 [42] |
| dxil-spirv + dxbc-spirv | DXIL/DXBC→SPIR-V | MIT | 被 vkd3d-proton 和 DXVK 3 使用 [18][22] |
| SPIRV-Cross | SPIR-V→MSL | Apache-2.0 | MoltenVK 使用 |
| Mesa NIR→MSL | SPIR-V→NIR→MSL | MIT | KosmicKrisp 使用，不依赖 SPIRV-Cross [2] |
| vkd3d-shader | DXBC/DXIL/HLSL→SPIR-V/MSL（实验）/GLSL（实验） | LGPL | MSL 目标从 1.14（2024-11）起就有，要用 `-DVKD3D_SHADER_UNSUPPORTED_MSL` 开启；GLSL 目标更早就有。2.0/2.1 在这两个目标上继续扩展 [23][24][80][81] |
| Microsoft dxilconv | DXBC→DXIL | 开源（DXC 仓库） | 可给 MSC 补上 SM5.x 输入 [44] |

**MSC 的再分发：** 二手资料称，EULA §2.B 允许把 `libmetalirconverter.dylib` 随应用分发，但**仅限着色器转换用途**，须随附许可证和致谢，并且只能在 Apple 硬件上运行。Xenia 的 macOS 移植据此使用 [43]。[中]：我没拿到 EULA 原文，Cider 采用前必须下载安装包并逐条核对。

**推断：** 对 D3D12 而言，“DXIL→MSC→Metal” 的中间层品质最接近 D3DMetal；而 “DXIL→dxil-spirv→SPIR-V→KosmicKrisp NIR→MSL” 完全开源，但转换环节多了一层。

### 7. 光追、Mesh Shader、VRS、Sampler Feedback、DirectStorage

| D3D12 特性 | Metal 对应 | D3DMetal | 开源栈（vkd3d-proton + KK/MVK） |
|---|---|---|---|
| DXR 1.0/1.1 | Metal RT（加速结构、intersection function）。M3 起有硬件 RT 和硬件 mesh shading [48] | 支持，需 `D3DM_SUPPORT_DXR=1`，官方文档写的是 M3 [31] | 需要 `VK_KHR_ray_tracing_pipeline/ray_query`，KK 和已发布的 MVK 都没有 [11][69]；MVK 未合并的 PR #2771 有可手动开启的实验性 RT [70] |
| Mesh/Amplification | Metal 3 mesh 管线 | MSC 支持 SM6.5 [42]；推断 D3DMetal 可用 | 需要 `VK_EXT_mesh_shader`，KK 和 MVK 都没有 |
| VRS Tier1/2 | 只有 rasterization rate map，语义不同 | 未核实（推断为不暴露或有限） | KK 没有 fragment shading rate [11] |
| Sampler Feedback | 没有直接对应（推断） | 未核实 | 无 |
| Tiled/Reserved resources | Metal sparse texture，Metal 4 起 sparse buffer | GPTK 3 起支持 sparse buffer/texture [25] | KK 和 MVK 都不支持 sparse |
| DirectStorage | Metal IO（`MTLIOCommandQueue`，macOS 13 起，内置 zlib/LZFSE/LZBitmap 等压缩）[52] | 游戏自带的 dstorage.dll 跑在 D3D12 之上，GDeflate 走 compute 回退路径（推断） | vkd3d-proton 2.10 起有 GDeflate 回退着色器 [51] |

**推断：** 需要 DirectStorage 的游戏一般通过游戏自带的运行库加 D3D12 compute 实现，Cider 不用单独实现一个 DirectStorage；关键是 D3D12 compute、wave ops 和 SM6.x 的正确性。

### 8. 超分与插帧

- **Metal 侧：**
  - `MTLFXTemporalScaler` 和 Spatial 从 macOS 13 起可用；
  - `MTLFXTemporalDenoisedScaler`（降噪加超分）在 macOS 上从 **26.0** 起可用 [46]；
  - `MTLFXFrameInterpolator` 从 **macOS 26.0** 起可用 [45]；
  - Metal 4 支持 M1 及以后的芯片 [47]；
  - WWDC26 的 Metal 4.1 重做了时域超分器，在 M5 Pro/Max 上使用 Neural Accelerators，并改进了与主流超分器的 API 兼容 [26]。
- **DLSS→MetalFX：**
  - D3DMetal 通过 `nvngx-on-metalfx` 加 `nvapi64` 实现，由 `D3DM_ENABLE_METALFX=1` 开启 [32]；
  - GPTK 3 已经可以用，2025-07 已有 CrossOver + GPTK 的实测 [61]；
  - 一份逆向报告称 GPTK 4 把 DLSS-FG 接到了 Metal 4 插帧器上，但没有 DLSS 4 Transformer 模型；该来源可信度低，且与 GPTK 3.0 已包含 nvngx-on-metalfx 的事实不一致 [63]。[低]
- **DXMT：** 为 D3D11 提供开源的 DLSS-SR 实现（v0.70–0.80 陆续修复），v0.80 之后从 MIT 改为 LGPL [49]。
- **FSR 1/2/3：** 是游戏内的 HLSL compute 着色器（MIT），翻译层只要正确就能跑。推断：FSR 3 插帧也属于这一类。**FSR 4** 依赖 RDNA4 的 ML 指令，推断不可用。
- **XeSS：** 有基于 DP4a / SM6.4 点积的通用路径，推断可以通过 MSC 或 SPIR-V 跑；插帧部分未核实。
- **OptiScaler（GPL-3.0）：** 能把 DLSS/FSR/XeSS 的输入改接到其他后端 [50]，适合作为用户可选插件，但 GPL-3 与 Cider 的许可证是否兼容需要评估。

### 9. 原生 Vulkan 的 Windows 游戏（winevulkan）

Wine 的 `winemac.drv` 会把 `VK_KHR_win32_surface` 映射到 `VK_EXT_metal_surface`，旧版映射到 `VK_MVK_macos_surface` [53]；host 端既可以直接用 MoltenVK，也可以用 Vulkan loader。
- **用 MoltenVK 时：** 通过 loader 需要带 portability enumeration 标志。社区为此给 Wine 打过补丁 [59]，说明上游默认不设置这个标志（推断）。
- **用 KosmicKrisp 时：** 是标准 ICD，不需要 portability subset [3]。

推断：原生 Vulkan 游戏受益最直接，一致性减少了 subset 相关的崩溃，但用到 GS、XFB、RT、mesh 或 sparse 的游戏仍会失败。Cider 应该给每个 bottle 提供 ICD 选择（`VK_DRIVER_FILES` 或 `VK_ICD_FILENAMES`），默认顺序为 KosmicKrisp，其次 MoltenVK。

---

## 对 Cider 的启示与建议

**P0（0–3 个月）**
1. **图形后端抽象层：** 每个 bottle 或每个 app 可选 `d3dmetal | dxmt | dxvk | vkd3d-proton | vkd3d | wined3d`，由它生成 DLL override、环境变量和 ICD 配置。先支持一套可复现的配置格式，内置 `D3DM_SUPPORT_DXR`、`D3DM_ENABLE_METALFX`、`ROSETTA_ADVERTISE_AVX`、`MTL_HUD_ENABLED` 等预设。
2. **D3DMetal 采用“用户自带”模式：**
   - 用户用自己的 Apple 开发者账号下载 GPTK DMG，Cider 负责挂载和导入；
   - 从 `redist/lib/external` 和 `lib/wine/x86_64-{windows,unix}` 复制 D3DMetal.framework、libd3dshared.dylib、PE 转发 DLL 和 nvngx/nvapi；
   - 导入时检查 Apple 代码签名，并按版本记录 SHA256 清单；
   - 保存到 `~/Library/Application Support/Cider/Runtimes/D3DMetal/<version>/`，通过符号链接接入 Wine 树；
   - 界面中展示 Apple 许可证全文，由用户确认；
   - 绝不修改二进制。
   - **GPTK 版本策略（已按事实核查更新）：**
     - 默认用 GPTK 3.0。
     - macOS 27 已于 2026-09-14 正式发布，不再是 beta [74][75]。在 macOS 27 上，把 GPTK 4 作为可选项，并在界面上标注“beta”，因为截至 2026-09-26 最新可见版本是 4.0 beta 2 [76][83]。
     - D3DMetal 4 能不能在 macOS 26 上运行还不确定（存疑），要先在开发机（macOS 26.5）上实测，再决定是否对 macOS 26 开放 [73]。
3. **Wine 分支：** 以 CrossOver 按 LGPL 公开的源码，或 Gcenx/Apple 的 GPTK Wine 源码为基础，确保包含 D3DMetal 必需的 unwinder 补丁 [33][78]。
   - **补丁内容（已更正）：** 异常穿过非 PE 的宿主库帧（如 `libd3dshared.dylib`、D3DMetal.framework）时，也要执行该帧的 personality routine。上游 `signal_x86_64.c` 在 `module == NULL` 时会把它置空；原稿写的是“builtin 模块”，不准确。
   - **回归用例：** 先写一个“C++ 异常穿过宿主库帧”的最小测试，用来验证这个补丁确实必要；另外覆盖 D3D12 设备创建、DXR 查询和 DLSS 探测。
4. **Vulkan 运行时：** 打包 Khronos loader、MoltenVK 1.4.2（Apache-2.0）和 KosmicKrisp（MIT，从 Mesa 固定 commit 构建）。
   - 在 macOS 26+ 的 Apple Silicon 上，把 KosmicKrisp 作为可选 ICD 提供给原生 Vulkan 游戏。
   - MoltenVK 只留给 macOS 26 及以下的 Intel Mac，以及 macOS 14–15 的 Apple Silicon Mac。
   - 给 vkd3d-proton 配置档预留 `MESA_KK_EXPERIMENTAL` 开关，用来暴露 `VK_EXT_image_view_min_lod` [11][66]。
   - 构建 KosmicKrisp 前要装好完整 Xcode [9]。
5. **（新增）开源栈阻塞项马上开始处理：** 事实核查确认，上游 vkd3d-proton 和上游 DXVK 在 KosmicKrisp 上都无法创建设备。
   - **XFB 模拟是开源 D3D12 路径的 P0 阻塞项** [79]。KosmicKrisp 的 properties 里已经有 XFB 字段，可以作为切入点 [11]。
   - DXVK 需要 GS 和 `fillModeNonSolid` [17]。
   - 0–3 个月内要做的事：
     - 在 XDC 2026（2026-09-29）后与 LunarG 对齐 GS/XFB/线框模式的路线图 [7]；
     - 做 XFB 模拟原型；
     - 为了 bring-up，可以在 Cider 本地给 vkd3d-proton 打一个临时补丁，跳过 `transformFeedbackQueries` 检查。这样用到 stream output 的游戏会出错，只能用于内部测试，不能发布。

**P1（3–12 个月）**
6. **向上游推进 KosmicKrisp 的 GS、XFB 和线框填充模式：** 参考 Honeykrisp 的 compute 模拟，与 LunarG 协作。目标是让上游 DXVK 3.x 在 KosmicKrisp 上跑起来，这需要 GS 和 `fillModeNonSolid` 两项都补上 [17]；同时让上游 vkd3d-proton 不打补丁就能创建设备，这需要 XFB [79]。做成后可以替换 DXVK-macOS 1.10.3，用 DXVK 覆盖 D3D9/10/11；DXMT 并行保留，两边都跑基准。
7. **把 vkd3d-proton 移植到 KosmicKrisp：** 前提是 XFB 已经补上（或者在内部测试中用第 5 项的临时补丁）。用 vkd3d-proton 自带测试加一组游戏清单做 bring-up，统计 GS、feature level、tiled resources、DXR 和 mesh 的缺口，并按缺口排出 KosmicKrisp 上游贡献的顺序。
8. **开源 nvngx shim：** 参考 DXMT 的实现，把 DLSS-SR 映射到 `MTLFXTemporalScaler` 或 `TemporalDenoisedScaler`，把 DLSS-FG 映射到 `MTLFXFrameInterpolator`（macOS 26+），用于非 D3DMetal 的后端。
9. **ARM64 过渡（从 P2 提前）：** 这已经是近期的竞争要求，不再只是 P2 的对冲。
   - CodeWeavers 已在测试原生 ARM64 的 CrossOver 27 预览版，目标 2027 年初正式发布；该构建不带 D3DMetal，D3D12 “稍后提供” [77]。
   - Apple 表示 macOS 28 起只保留面向旧游戏的 Rosetta 子集 [56]。
   - D3DMetal（包括 4.0 beta）据社区来源仍然只有 x86_64 版，依赖 Rosetta [34]（[中]，4.0b2 的二进制没有亲自核对）。
   - 开源栈可以编译成 ARM64EC 或 ARM64。应尽早规划 ARM64 Wine，以及“ARM64 进程 + x86_64 D3DMetal”共存或切换的方案。

**P2（12–24 个月）**
10. **长期 D3D12 方向（二选一，或并行评估）：**
    - (a) **vkd3d-proton + KosmicKrisp。** 首选。前提是 P0/P1 的 XFB 已经落地（否则无法创建设备）。在此之上，还需要 KosmicKrisp 补齐 GS、sparse、ray query/pipeline 到 Metal RT、`VK_EXT_mesh_shader` 到 Metal mesh，以及 descriptor heap。这条路可以复用 Valve/Proton 生态在游戏兼容上的巨大投入。
    - (b) **自研 D3D12→Metal 4 转译器（类似 DXMT）。** 前提是 (a) 的性能不达标，或者 XFB/GS 上游进展太慢。着色器用 MSC（先确认 EULA）或开源链。

## 风险

1. **法律风险：** Apple 可能修改 GPTK 许可或终止授权；“评估视频游戏”的用途限制与日常游玩存在冲突；Cider 如果做任何形式的商业化，都会失去 2A(iii) 的依据。
2. **D3DMetal 闭源：**
   - 缺陷无法修复（比如 ALLOW_TEARING、HDR 色彩空间 [35]），也不能打补丁，版本节奏跟着 WWDC 走。
   - GPTK 4 是否要求 macOS 27，Apple 没有说明（存疑）[73]。如果要求，macOS 26 用户就用不上 D3DMetal 4，其中包括所有 Intel Mac，也包括现在的开发机。
   - D3DMetal 的 Wine 集成依赖 unwinder 补丁，而“缺补丁就崩溃”这个结论只有一份社区来源 [33][78]。
3. **KosmicKrisp 进度不确定：**
   - GS、XFB、fillModeNonSolid、sparse、RT 和 mesh 何时补齐都不确定。其中 XFB 缺失会让 vkd3d-proton 完全无法启动 [79]，GS 和 fillModeNonSolid 缺失会让 DXVK 完全无法启动 [17]。
   - 资助主要面向 Android Emulator，游戏类特性未必是优先项 [3]。
   - 只支持 macOS 26+，老系统用户只能退回 MoltenVK。
4. **性能：** GS 和 tessellation 走 compute 模拟，在 TBDR 架构上开销大；Metal 编码器切换代价高 [3]。
5. **开发环境：**
   - 8 GB 内存低于 GPTK 建议的 16 GB。
   - Mesa 文档把完整 Xcode 列为 KosmicKrisp 的构建要求 [9]，而开发机现在只有 CLT，也没有 Homebrew，需要先装 Xcode，再搭 meson/LLVM 等构建链。
   - 开发机是 macOS 26.5，想用 macOS 27 上的 GPTK 4 工具就得升级系统。
6. **Rosetta 退场与 ARM64 竞争：**
   - macOS 28 起 Rosetta 功能受限 [56]，x86_64 版 D3DMetal 的前景不明。
   - CodeWeavers 已在测试原生 ARM64 的 CrossOver 27 [77]。如果 Cider 的 ARM64 化落后，会在性能和系统寿命上处于劣势。
7. **反作弊 / DRM** 普遍不兼容 [31]。

## 未解问题

1. CrossOver 与 Apple 之间 D3DMetal 授权的实际条款（是否为单独的商业许可）？目前没有公开的一手来源。
2. GPTK 4 正式版什么时候发布（截至 2026-09-26 仍是 4.0 beta 2 [76][83]）？它的许可证是否仍是 EA18380？是否会出 ARM64 或 ARM64EC 版 D3DMetal？CrossOver 27 预览版不带 D3DMetal [77]，CodeWeavers 在 ARM64 上打算怎么处理 D3D12？
3. KosmicKrisp 的 GS、XFB 和 fillModeNonSolid 何时合入 Mesa main？properties 里已有 XFB 字段 [11]，但还没找到公开 MR [65]。XDC 2026（9 月 29 日）的演讲可能会给出时间表。
4. ~~vkd3d-proton 在设备创建时是否硬性要求 transform feedback 或 GS？~~ **已解决：** 硬性要求 XFB（`transformFeedbackQueries`），不硬检查 GS [79]。
5. MSC EULA §2.B 的原文，以及把它用于“通用 D3D12 转译层”是否算“着色器转换用途”。
6. D3DMetal 实际暴露的 VRS、Sampler Feedback、Mesh Shader tier 是多少？需要在 M3 上用 `CheckFeatureSupport` 实测。
7. ~~MoltenVK 是否支持 maintenance5/6？robustness2 是否真的被硬编码为 false？~~ **已解决：** 支持 maintenance5/6 [69]；`robustBufferAccess2` 和 `nullDescriptor` 确实硬编码为 false [68]。
8. D3DMetal 4（GPTK 4）能否在 macOS 26 上运行？Apple 一手资料没有说明（存疑），需要在开发机（macOS 26.5）上实测 [73]。
9. 在 Cider 自己的 Wine 上，D3DMetal 缺少 unwinder 补丁时是否一定会失败？需要用最小测试验证 [33][78]。
10. `MESA_KK_EXPERIMENTAL` 中打开 image view min LOD 的具体取值是什么？需要到 kk 源码的解析表里确认 [66]。

## 参考来源

1. https://www.lunarg.com/lunarg-achieves-vulkan-1-3-conformance-with-kosmickrisp-on-apple-silicon/ — KosmicKrisp 1.3 一致性（2025-10-30）、10 个月开发周期、Google 合作
2. https://www.lunarg.com/lunarg-at-xdc-2025-kosmickrisp-overview/ — XDC 2025 架构概览（NIR→MSL、MIT）
3. https://vulkan.org/user/pages/09.events/vulkanised-2026/1545-Richard-Wright-LunarG.pdf — Vulkanised 2026 演讲：路线图、打包布局、资助方
4. https://www.lunarg.com/the-state-of-vulkan-on-apple-jan-2026/ — MoltenVK 与 KosmicKrisp 的定位（2026-01）
5. https://www.lunarg.com/lunarg-releases-vulkan-sdk-1-4-341-0/ — SDK 1.4.341.0（2026-02-02），KosmicKrisp 进入 beta
6. https://www.lunarg.com/lunarg-releases-vulkan-sdk-1-4-357-0/ — SDK 1.4.357.0（2026-07-28），Metal 4 编码，性能提升约 2.35 倍
7. https://www.lunarg.com/lunarg-at-xdc-2026-kosmickrisp-update/ — 1.4 一致性（2026-09-25）、XDC 2026 演讲
8. https://gamedev.net/news/6019-kosmickrisp-achieves-vulkan-14-conformance-on-apple-silicon/ — 1.4 一致性新闻（经搜索摘要获取，页面 403）
9. https://docs.mesa3d.org/drivers/kosmickrisp.html — Mesa 官方文档：要求、构建、环境变量
10. https://docs.mesa3d.org/relnotes/26.0.0.html — Mesa 26.0.0（2026-02-11）
11. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_physical_device.c — KosmicKrisp 特性和扩展表（freedesktop 仓库的只读镜像）
12. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_private.h — 描述符、push constant 上限
13. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/Whats_New.md — MoltenVK 1.4.0–1.4.3 变更与日期
14. https://github.com/KhronosGroup/MoltenVK/releases — MoltenVK Release 页面
15. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/MoltenVK_Runtime_UserGuide.md — 扩展列表；“Known Limitations”只列了 pipeline statistics query、PVRTC 和 allocation callbacks（GS/XFB/sparse/RT 的缺失依据见 [68][69]）
16. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/README.md — Apache-2.0、Vulkan 1.4 portability
17. https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxvk/dxvk_device_info.cpp — DXVK 必需和可选特性表
18. https://github.com/doitsujin/dxvk/releases/tag/v3.0 — DXVK 3.0 发布说明
19. https://github.com/doitsujin/dxvk/wiki/Driver-support — DXVK 驱动要求
20. https://linuxiac.com/dxvk-3-0-released-with-new-shader-compiler-and-vulkan-1-4-requirement/ — DXVK 3.0 日期（2026-06-25）
21. https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/master/README.md — vkd3d-proton 驱动要求与 VKD3D_CONFIG
22. https://github.com/HansKristian-Work/dxil-spirv — dxil-spirv（MIT）
23. https://www.gamingonlinux.com/2026/05/wine-11-10-is-out-with-vkd3d-2-0-vbscript-compatibility-improvements-and-more/ — Wine 11.10 / vkd3d 2.0（2026-05-30）：已有 MSL 目标支持 stencil reference 输出
24. https://www.linuxcompatible.org/story/vkd3d-21-lands-better-dx12-shaders-and-mesh-support-for-linux-gamers — vkd3d 2.1（2026-08-24）：扩展已有的 GLSL 目标，加入 mesh 管线子对象
25. https://developer.apple.com/games/game-porting-toolkit/ — Apple GPTK 官方页面（GPTK 4）
26. https://developer.apple.com/wwdc26/guides/games/ — WWDC26 游戏指南（GPTK 4、Metal 4.1、MetalFX）
27. https://appleinsider.com/articles/26/06/17/apples-game-porting-toolkit-4-is-a-big-improvement-for-modern-game-coders — GPTK 4 beta 报道与测试
28. https://korben.info/en/game-porting-toolkit-4-windows-games-smooth-mac.html — GPTK 4 第三方基准
29. https://github.com/Gcenx/game-porting-toolkit/releases — Gcenx 的 GPTK 构建（含 3.0-3、3.0 beta1 标签）
30. https://github.com/dbc-hbin/d3dmetal-redistributable （许可证原文：https://raw.githubusercontent.com/dbc-hbin/d3dmetal-redistributable/main/License.rtf）— D3DMetal 4.0b2 与 GPTK 许可证 EA18380
31. https://gist.github.com/lynkos/3999f629560219a81d4e2c083a4bf5b1 — GPTK 2.1 README 副本（环境变量、AVX、目录结构）
32. https://mybyways.com/blog/updating-crossover-to-gameporting-toolkit-3-0 — GPTK 3.0 文件布局，nvngx 与 MetalFX 设置
33. https://github.com/frankea/Whisky/issues/163 — D3DMetal 转发 DLL 结构，Wine unwinder 问题（社区来源，2026-07-31；机制措辞不严谨，见 [78]）
34. https://github.com/utmapp/d3dmetal-native — 原生宿主接口加载 D3DMetal（MIT）
35. https://developer.apple.com/forums/thread/820469 — D3DMetal 缺失 caps 与 DTS 回复（2026-03/04）
36. https://www.codeweavers.com/crossover/changelog — CrossOver 26.x 组件版本（26.0.0 于 2026-02-10；26.1.0 于 04-09、26.2.0 于 06-09、26.3.0 于 07-21，都没有组件升级）
37. https://en.wikipedia.org/wiki/CrossOver_(software) — CodeWeavers 与 Apple 的“共同协议”（二手，经搜索摘要获取）
38. https://docs.getwhisky.app/maintenance-notice — Whisky 停止维护（2025-04-09）
39. https://raw.githubusercontent.com/Whisky-App/Whisky/main/README.md — Whisky 依赖与致谢
40. https://raw.githubusercontent.com/Sikarugir-App/Sikarugir/main/README.md — Sikarugir 的 D3DMetal 选项与商业限制
41. https://raw.githubusercontent.com/Heroic-Games-Launcher/HeroicGamesLauncher/main/src/backend/wine/manager/downloader/constants.ts — Heroic 下载 Gcenx GPTK
42. https://developer.apple.com/metal/shader-converter/ — Metal Shader Converter 4.0 beta
43. https://deepwiki.com/wmarti/metal-shader-converter — MSC EULA §2.B 的二手转述
44. https://github.com/microsoft/DirectXShaderCompiler/tree/main/projects/dxilconv — DXBC→DXIL 转换器
45. https://developer.apple.com/tutorials/data/documentation/metalfx/mtlfxframeinterpolator.json — MTLFXFrameInterpolator 从 macOS 26.0 起可用
46. https://developer.apple.com/tutorials/data/documentation/metalfx/mtlfxtemporaldenoisedscaler.json — 降噪超分，macOS 26.0 起可用
47. https://developer.apple.com/videos/play/wwdc2025/211/ — WWDC25 “Go further with Metal 4 games”
48. https://www.apple.com/newsroom/2023/10/apple-unveils-m3-m3-pro-and-m3-max-the-most-advanced-chips-for-a-personal-computer/ — M3 硬件 RT 与 mesh shading
49. https://github.com/3Shain/dxmt/releases — DXMT 版本、DLSS-SR、许可变更
50. https://github.com/optiscaler/OptiScaler — OptiScaler（GPL-3.0）
51. https://github.com/HansKristian-Work/vkd3d-proton/releases/tag/v2.10 — DirectStorage GDeflate 支持
52. https://developer.apple.com/documentation/metal/mtliocommandqueue — Metal 快速资源加载
53. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/vulkan.c — Wine 在 macOS 上的 Vulkan surface 映射
54. https://github.com/gauthierpiarrette/highball/issues/41 — 社区评估 KosmicKrisp 与 MoltenVK 的缺口（2026-09）
55. https://github.com/frankea/Whisky/issues/264 — 采用上游 DXVK + KosmicKrisp 的提案（2026-09）
56. https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/ — Rosetta 2 在 macOS 28 起受限
57. https://www.codeweavers.com/blog/mjohnson/2023/6/1/unleashing-the-gaming-revolution-crossover-macs-directx-12-support-update — CrossOver 早期 D3D12（vkd3d + MoltenVK）
58. https://www.techtimes.com/articles/321006/20260720/moltenvk-142-rc1-closes-gpu-regression-backlog-drops-two-contested-fixes.htm — MoltenVK 1.4.2-rc1
59. https://github.com/metalsharp/DXVK-MacOS — 基于 DXVK 3.1 的 MoltenVK 适配分支（未验证）
60. https://github.com/squidbus/mesa-kosmickrisp — KosmicKrisp universal 自动构建
61. https://www.techspot.com/news/108556-apple-macos-tahoe-brings-major-gaming-improvements-through.html — GPTK 3 下 DLSS→MetalFX 实测（2025-07）
62. https://asahilinux.org/2024/10/aaa-gaming-on-asahi-linux/ — Honeykrisp 用 compute 模拟 GS/tess，运行 DXVK 和 vkd3d-proton
63. https://skyfireworks.io/velocity/gptk4 — GPTK 4 逆向观察（低可信度）
64. https://www.lunarg.com/kosmickrisp-achieves-vulkan-1-4-conformance-on-apple-silicon/ — LunarG：KosmicKrisp 通过 Vulkan 1.4 一致性（2026-09-25）；开源、MIT；M1+ / macOS 26+ / Metal 4；9 月 29 日随 SDK 发布
65. https://api.github.com/repos/chaotic-cx/mesa-mirror/commits?path=src/kosmickrisp/vulkan/kk_physical_device.c — kk_physical_device.c 的提交历史（最近 kk 提交为 2026-09-21/22）
66. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_debug.h — `KK_EXPERIMENTAL()` 宏（运行时检查 `kk_mesa_experimental_flags`）
67. https://api.github.com/repos/doitsujin/dxvk/releases — DXVK v3.1（2026-08-28）、v3.1.1（2026-09-15）
68. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm — MoltenVK 特性设置（robustness2 硬编码、无 GS/sparse、fillModeNonSolid = true）
69. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/MoltenVK/MoltenVK/Layers/MVKExtensions.def — MoltenVK 扩展表（有 maintenance5/6，无 XFB/RT/mesh/sparse）
70. https://github.com/KhronosGroup/MoltenVK/pull/2771 — 实验性光追 PR（2026-07-12 提交，未合并）
71. https://github.com/KhronosGroup/MoltenVK/issues/1524 — MoltenVK geometry shader 请求（未解决）
72. https://api.github.com/repos/KhronosGroup/MoltenVK/releases — v1.4.2 发布于 2026-07-24T14:00:46Z，rc1 为 2026-07-19
73. https://github.com/apple/game-porting-toolkit — Apple GPTK 仓库：macOS 27 + Xcode 27 用于新的 Metal 调试工具
74. https://9to5mac.com/2026/09/09/apple-confirms-macos-27-golden-gate-launch-date-september-14/ — macOS 27 Golden Gate 于 2026-09-14 发布
75. https://www.macrumors.com/2026/09/10/macos-27-golden-gate-release-date/ — macOS 27 发布日期，只支持 Apple Silicon Mac
76. https://www.ithinkdiff.com/game-porting-toolkit-4-gta-v-66-percent-performance-gain/ — GPTK 4 在 2026-08-30 仍称 beta
77. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — CrossOver 27 原生 ARM64 预览版，不带 D3DMetal；Intel 支持止于 CrossOver 26（2026-07-31）
78. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/signal_x86_64.c — Wine x86_64 unwinder：`module == NULL` 时丢弃宿主库帧的 personality routine
79. https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/master/libs/vkd3d/device.c — vkd3d-proton 设备初始化：XFB（`transformFeedbackQueries`）硬检查
80. https://www.winehq.org/news/2024112101 — vkd3d 1.14（2024-11-21）发布，首次加入实验性 MSL 目标
81. https://www.phoronix.com/news/VKD3D-1.14-Released — “VKD3D 1.14 Released With Initial Metal Shading Language Output”
82. https://www.linuxcompatible.org/story/vkd3d-114-released/ — vkd3d 1.14 发布说明转载（`-DVKD3D_SHADER_UNSUPPORTED_MSL`）
83. https://github.com/dbc-hbin/d3dmetal-redistributable/releases/tag/gptk-4.0b2 （API：https://api.github.com/repos/dbc-hbin/d3dmetal-redistributable/releases）— 4.0b2 再分发 release（2026-09-23，约 23.6 MB）

## 事实核查记录

> 2026-09-26 根据独立事实核查结果修订。输入里有几条重复的核查（许可证、KosmicKrisp 状态、GS/XFB、MoltenVK、CrossOver/GPTK 4、Wine 布局、vkd3d-proton XFB、MSL 目标、image_view_min_lod），下表把它们合并，每项一行；只有 GPTK 4 系统要求一项两份核查说法不一，已标为存疑。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| GPTK 许可证 EA18380（2023-08-17）：只能非商业分发（§2A(iii)、§2C），Framework 可以整体单独分发，用途限于开发、测试或评估视频游戏（§2A(i)），禁止修改、逆向和制作衍生作品（§2D） | 确认（两份核查一致） | 条款转述无误。注意：来源 dbc-hbin 是 2026-09-23 新建、与 Apple 无关的第三方仓库，“随 4.0 beta 2 附带”只有这一个来源，没有和 Apple 的 GPTK 4 DMG 比对过 [30][83]。“用户从 Apple 导入”仍是最稳妥的做法；法律解释不在本次范围内 |
| KosmicKrisp：MIT；Mesa 26.0.0（2026-02-11）；Vulkan 1.3 一致性（2025-10-30）；2026-09-25 宣布 1.4 一致性；要求 Apple Silicon + macOS 26+；SDK 1.4.357.0（2026-07-28）改用 Metal 4 编码 | 确认 | 2025-10-30 是 LunarG 的发文日期，Khronos 批准的具体日期文中没写。拿到 1.3 认证时的说法是 macOS 15+，macOS 26 门槛是后来才定的。MIT 的依据是 [2][64]，不是 [1]。已更新摘要和 §5.1 [1][6][9][10][64] |
| KosmicKrisp（2026-09 Mesa main）没有 geometryShader 和 VK_EXT_transform_feedback；DXVK master 必需 geometryShader，所以上游 DXVK 无法创建设备 | 确认，另有补充 | 还缺 **fillModeNonSolid**（DXVK 约第 852 行，必需）。上游 DXVK 需要 GS 模拟和线框填充模式两项才能解锁。XFB 和 shader_object 的 properties 已经填好，但扩展没有暴露；没找到公开的 GS/XFB MR [11][17][65][67] |
| MoltenVK 1.4.2（Whats_New 2026-07-20，GitHub 约 07-24），Apache-2.0，Vulkan 1.4 portability subset，不完全一致，无 GS/XFB/sparse/RT | 确认 | GitHub 正式发布于 2026-07-24。“无 RT”只对已发布版本成立：PR #2771（未合并）有可手动开启的实验性 RT。缺失项的依据应是源码（MVKDevice.mm、MVKExtensions.def），不是 UserGuide 的 Known Limitations。1.4.1 的 GitHub Release 是 2025-11-30，Whats_New 写的是 11-24。macOS 27 只支持 Apple Silicon，所以 MoltenVK 兜底 Intel 的范围只到 macOS 26 [68]–[72][74][75] |
| CrossOver 26.0.0（2026-02-10）：Wine 11.0、D3DMetal 3.0、DXMT v0.72、vkd3d 1.18；GPTK 4 beta（WWDC26）：D3DMetal 4 DX12→Metal 4，只支持 Apple Silicon，完整特性需要 macOS 27 beta | 部分正确；系统要求一项**存疑** | 组件版本正确（另含 Wine Mono 10.4.1）。26.1.0、26.2.0、26.3.0（2026-07-21，当前最新）都没有组件升级。DX11 仍走 Metal 3。macOS 27 已于 2026-09-14 正式发布，“beta”已过时。GPTK 4 截至 2026-09-26 仍是 4.0 beta 2。Apple 一手资料只把 macOS 27 + Xcode 27 列为新调试工具的要求，没有写 D3DMetal 4 必须用 macOS 27。核查 A 认为该要求未经证实，核查 B 认为 GPTK 4 面向 macOS 27；判断为存疑，需要在 macOS 26.5 上实测。另外，CrossOver 27 原生 ARM64 预览版不带 D3DMetal [36][73]–[77] |
| D3DMetal 的 Wine 布局（lib/external、x86_64-windows 转发 DLL、x86_64-unix 符号链接）；上游 Wine unwinder 不调用 builtin 模块的 personality routine，所以没有补丁会中止 | 部分正确 | 布局正确。机制写错了：`signal_x86_64.c` 只在帧不属于任何 PE 模块时（`module == NULL`，宿主或系统库代码，如 `libd3dshared.dylib`）才丢弃 personality routine；`LDR_WINE_INTERNAL` 的 builtin 模块会保留 handler。“中止”的现象只有一份社区来源。已更正 §1.3、P0 第 3 项和风险 2 [32][33][78] |
| vkd3d-proton 是否必需 XFB 未核实；它的硬性要求 KosmicKrisp 大多能满足，所以 vkd3d-proton + KosmicKrisp“短期能覆盖一部分游戏” | **驳回** | `device.c` 在 `transformFeedbackQueries` 为 0 时返回 `E_INVALIDARG`，README 没有写这一点。KosmicKrisp 和 MoltenVK 都没有 XFB，所以不打补丁的 vkd3d-proton 在两者上都无法创建设备。XFB 是开源 D3D12 路径的 P0 阻塞项；vkd3d-proton 不硬检查 GS。highball [54] 的说法是对的。已更正 §3、P0 新增第 5 项、P1 第 7 项、P2 第 10 项，并关闭未解问题 4 [79] |
| KosmicKrisp 已暴露 image_view_min_lod（以及 robustness2、push descriptor、mutable descriptor） | 部分正确 | robustness2、push descriptor 和 mutable descriptor 默认暴露；`EXT_image_view_min_lod = KK_EXPERIMENTAL(IMAGE_VIEW_MIN_LOD)`，要设置 `MESA_KK_EXPERIMENTAL` 才暴露（custom border color 同理）。已更正 §3 和 §5.2，并在 P0 第 4 项加入实验开关 [11][66] |
| 对照 DXVK master 的必需特性，KosmicKrisp 只缺 geometryShader | 部分正确 | 缺两项：`geometryShader` 和 `fillModeNonSolid`，都是必需，都没暴露。MoltenVK 设置了 `fillModeNonSolid = true`。已更正 §5.2 [17][68] |
| 上游 vkd3d 2.0（Wine 11.10，2026-05-30）新增实验性 MSL 目标；2.1（2026-08）新增实验性 GLSL 目标 | **驳回** | MSL 目标从 vkd3d 1.14（2024-11-21）起就有（`-DVKD3D_SHADER_UNSUPPORTED_MSL`），2.0 只加了像素着色器 stencil reference 支持。2.1（2026-08-24）扩展的是早已存在的 GLSL 目标，并加入 mesh 管线子对象。两个目标都还是实验性的。已更正 §3 和 §6 [23][24][80]–[82] |
| MoltenVK 宣告 robustness2，但把 robustBufferAccess2 和 nullDescriptor 硬编码为 false（原为 [低]）；是否支持 maintenance5/6 未知 | 确认 | `MVKDevice.mm` 约第 636–638 行确实如此；`MVKExtensions.def` 里有 KHR_maintenance5/6。DXVK 必需这两项 robustness2 特性，所以上游 DXVK 在 MoltenVK 上除 GS 外还有这两个阻塞。置信度升为 [高]，关闭未解问题 7 [68][69] |
| 从源码构建 KosmicKrisp 需要 Meson ≥1.9.1、LLVM ≥20.1.8、libclc 和 spirv-llvm-translator；开发机只需补齐这套工具链 | 部分正确 | Mesa 文档把 “Xcode and command line tools” 列在第一位，另有 cmake、pkg-config、spirv-tools 和 Python 包 mako/packaging/pyyaml。只有 CLT 可能不够，应该装完整 Xcode。已更正 §5.3 和风险 5 [9] |
| MoltenVK 是 Intel Mac 的兜底；D3DMetal 是 x86_64 组件，ARM64 是远期对冲（macOS 28 起 Rosetta 受限）；默认“GPTK 3.0，macOS 27 上可选 GPTK 4 beta” | 部分正确 | macOS 27 于 2026-09-14 发布，只支持 Apple Silicon，Intel 最高到 macOS 26。CrossOver 26 是最后一个支持 Intel 的版本；CrossOver 27 原生 ARM64 预览版不带 D3DMetal。D3DMetal（含 4.0 beta）据社区来源仍只有 x86_64 版。ARM64 化已经是近期竞争要求：已从 P2 提前到 P1 第 9 项，并更新 §4 结论、P0 第 2 项和风险 6 [34][56][74][75][77] |
