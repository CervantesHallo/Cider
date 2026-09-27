# 32 位 / DX8–DX9 / OpenGL 老游戏在 macOS 新 WoW64 下的实际表现与优化路线

> 调研日期 2026-09-26 · 事实核查更新 2026-09-27（见文末“事实核查记录”，其中的更正优先于正文；正文中改动处标注“核查更正”）· 置信度说明：**[高]** 表示一手源码、官方 release notes 或官方文档已直接核实；**[中]** 表示可信第三方来源（社区数据库里带环境信息的实测、第三方导入的 CrossOver 源码镜像）或只有单一来源；**[低]** 表示搜索摘要或二手转述；**[推断]** 是本报告自己的推理。
> 重要前提：本文所有 fps 都是**他人实测**，开发机（M3 / 8 GB / macOS 26.5）上没有跑过任何游戏。最系统的数据来自 Highball 的 CC0 数据库 highball-db，但它以 M1 Pro 为主，大多只有单次测量。调研后期 Web 工具额度用尽，少数问题没能继续追查，已列入“未解问题”。

## 摘要

- **OpenGL WoW64 拷贝路径：上游有，CrossOver 没有。**
  - 上游 Wine master（VERSION = 11.18，2026-09-27 取样）的 `dlls/opengl32/unix_wgl.c` 中，`wow64_map_buffer()` 在 32 位进程映射 GL 缓冲时按以下顺序判断 [1][2]（**核查更正**：原文把“指针低于 4 GB”列为第一步，与代码顺序不符）：
    1. `buffer->vk_memory`：Vulkan placed memory（`vkMapMemory2KHR` + `VK_MEMORY_MAP_PLACED_BIT_EXT`）；
    2. `buffer->pinned`：`GL_AMD_pinned_memory`；
    3. 驱动返回的指针恰好低于 4 GB（“we're lucky”），直接使用；
    4. 带 `GL_MAP_PERSISTENT_BIT` 的映射直接 FIXME 失败；
    5. 以上都不适用时，用 `buffer_vm_alloc()` 分配低地址影子缓冲。只有在没有 `GL_MAP_INVALIDATE_*` 时才 `memcpy`，并且每个进程只打印**一次** `Doing a copy of a mapped buffer (expect performance issues)`（有 `static int once` 保护）。
  - 第 1、2 条只对 opengl32 自己包装分配的缓冲生效，`initialize_vk_device()` 还要求 `GL_EXT_memory_object_fd`。Apple GL 两种扩展都没有，所以上游 Wine 在 macOS 上，只要驱动指针在 4 GB 以上，就**必然**走第 5 条影子缓冲路径。文件里没有任何 `__APPLE__` 分支，也没有 `mach_vm_remap` [1][2]。[高]
  - CrossOver 的 Wine 源码在同一函数的 “we're lucky” 判断之后有 `#ifdef __APPLE__` 分支：用 `mach_vm_remap(VM_FLAGS_FIXED|VM_FLAGS_OVERWRITE)` 把驱动映射**别名**到低地址，完全不拷贝。第三方导入的 23.5.0、24.0.4、25.1.0、26.3.0 中都有这段代码，22.1.1 中还没有 [3][4][5][72]。（**核查更正**：原文只列了 24.0.4、25.1.0、26.3.0 三个版本，实际至少从 23.5.0 起就有。）[中高]
  - 上游 MR !8907（2025，opengl32 的 WoW64 buffer wrapper）的说明写明，这次重构的目的之一就是方便以后接入 `mach_vm_remap` 或 `glImportMemoryFdEXT` 这类替代方案 [6]。但截至 11.18，上游仍未合入。[中]
- **拷贝路径不一定是主要瓶颈；Wine 源码树的差异才是。** Highball 用 Half-Life 2 Demo（32 位 D3D9，M1 Pro，macOS 27.0，trainstation 片段，1280×720 timedemo）做了对比。这些都是维护者的单次测量，测于 2026-09-20 和 09-24 [18]：[中]
  - Sikarugir Wine 10 引擎 `x64-sikarugir10.0_6-r7`：wined3d-GL **38 fps**，wined3d-Vulkan 同样 **38 fps**，DXVK-d3d9 **17 fps**，GPU 每帧只忙 2–3 ms。r5 引擎上 wined3d 为 33 fps，DXVK 为 14–17 fps；highball-db 的 `verified` 块用的是 r5 的数字；
  - Wine 11 引擎：wined3d **131 fps**，原因作者自己也说“not yet understood”。这个引擎基于 CrossOver 26.3 源码，但这一点出自 Highball issue #5（2026-09-05 更新：“Highball's own Wine 11 engine (CrossOver 26.3 LGPL base, build 15)”），`half-life-2.json` 本身没有写 CX 26.3 [73]；
  - 再挂上 athei 的 x87sidecar 测试构建：**174 fps**（+30%），未发布。
  - Vulkan 渲染器不经过 GL 映射，却和 GL 渲染器一样慢。这说明 Wine 10 引擎的瓶颈与渲染后端无关。3.4 倍的提升更可能来自 CrossOver 树整体的改动，例如 msync、winemac 和同步路径，而不只是 remap。**[推断]**
- **Rosetta 的 x87 在 macOS 26/27 上没有公开改进。**
  - macOS 26 / 26.4 / 27 的 release notes 都没有提到 x87 [50][51][52]。[高]
  - 真实游戏受 x87 影响很大。例如 WoW 3.3.5a（M5，macOS 26）在户外锁在 24–27 fps，x87 循环占了 87–90% 的帧时间 [46]。[中]
  - 社区方案有两个：
    - **x87sidecar**（MIT）：在 M5 Max / macOS 27.0 上 179 项微基准平均快 61 倍；采用 cooperative 模式时不需要 entitlement，可以公证 [43]；
    - **rosettax87 / rosettax87_jit**：前者 2026-01-02 归档，约 4.7 倍；后者在 macOS 27 RC 上报 `PT_THUPDATE failed` [44][45]。
- **macOS 27 给出了“macOS 28 老游戏”机制的预览，并且与 Rosetta 无关。**
  - release notes 的 Gaming 一节写道：`sudo game-test-tool enable` 在 beta 版中开启“legacy Intel-based games”支持；开启后 **Rosetta 被禁用**，游戏改走“new underlying system behavior”；非游戏进程可能崩溃；正式版不可用（编号 166398727）。macOS 27 于 2026-09-14 发布，初始构建号 26A428 [50][54][77]。[高]
  - 这意味着 Engine R 在 macOS 28 上的去留，以及所有 Rosetta 运行时补丁（x87 hook）的前景，都取决于这个新机制是否接纳 Wine。
- **wined3d 的 Vulkan 渲染器已能承载 D3D9，但在 Mac 上几乎没有数据。**
  - Wine 10 加入了基于 HLSL 的 FFP；Wine 11 补齐了点精灵、顶点混合、固定管线凹凸贴图，并支持 SM1 像素着色器。官方同时说明“not yet at parity with the GL renderer”[9][10]。[高]
  - 它对设备特性的要求比 DXVK 宽松。FL9_3 只要求 SM3 和 `independentBlend`，而 `geometryShader`、`fillModeNonSolid` 这些 DXVK 必需的特性它都不需要 [13]。[高]
  - 所以它是**上游唯一能直接跑在上游 KosmicKrisp 上的 Vulkan 系 D3D9 路径**。[推断]
  - Mac 上唯一的实测就是上面 HL2 在 MoltenVK 上的 38 fps，与 GL 渲染器相同 [18]。
- **D3D9→Metal 格局变了：最值得跟进的是 athei/mtld3d。**
  - neo773/d9mt 的主干最后一次提交是 2026-06-24，此后停滞，只测过 GTA IV（M1 Max 上 50–90 fps）[32]。[高]
  - Sikarugir-App/d9mt 的 `dx9` 分支**确有真实代码**：athei 在 2026-03-27 提交了 `c16e245`，改动 71 个文件，+15,685 行，自带 DXSO→AIR 编译器、FFP 和测试集。但 2026-03-28 以后再没有提交 [33]。[高]
  - 3Shain 的 DXMT 1.0 路线图（#151，2026-04-21）写明 “related to D3D10/11 implementation only”，没有 D3D9 计划 [35]。[高]
  - 同一作者（athei）后来另起了 **mtld3d**：Rust 实现，zlib 许可，仓库创建于 2026-07-14（当天发布 v0.1.0），v0.11.0 于 2026-09-25 发布。它支持 SM1–3（DXSO→MSL）和 FFP；从 v0.6.0（2026-08-16）起同时附带 `wine/x86_64-unix/mtld3d.so` 和 `wine/aarch64-unix/mtld3d.so`。staging 缓冲和 VB/IB 使用 PE 自有内存，外面套 Metal 的 bytesNoCopy 包装（即 `newBufferWithBytesNoCopy`），因此零拷贝。README 说 CI 用的 Wine 基于 CrossOver 26，CrossOver 27 的 arm64 Wine 只做过手工测试。测试过 WoW 1.12/3.3.5a、HL2、TF2、GTA IV、3DMark05 和 Unigine Tropics。已有 WoW 启动器集成了它 [37]–[40][47][76]。[高]
- **16 位程序在 macOS + Rosetta 上能否运行，目前没有证据。**
  - Wine 11.0 宣布新 WoW64 支持 16 位 [9]。
  - macOS 的 `ldt_set_entry()` 对所有选择子都调用 `i386_set_ldt`，源码里找不到针对 Rosetta 的 16 位排除 [60]。[高]
  - 但全网没有找到“Wine 11 + Apple Silicon 跑通 Win16 程序”的报告。另外，stock Rosetta 对遗留编码的覆盖本来就不完整，例如 `ARPL` 和 `DC D8` [43]。**[未验证]**
- **Apple GL 4.1 暂时安全。** macOS 26 移除了 AGL，同时写明 “OpenGL still remains in the SDK”；macOS 27 和 Xcode 27 的 release notes 里完全没有出现 OpenGL [50][51][53]。[高]
- **Zink 作为 PE 侧 OpenGL 的 MR !10531（2026-04-02）是半个愚人节玩笑。** 上游 master 的 `libs/` 里没有 Mesa，说明它没有合入 [56][57]。[中高]
  - Zink 的基线要求 `fillModeNonSolid`，GL 3.0 要求 XFB，GL 3.2 要求 GS [58]。上游 KosmicKrisp 这三项都没有，MoltenVK 没有 XFB 和 GS。
  - 所以在 Mac 上，Zink 暂时只会让 GL 能力**降级**（最多到 2.1）。**[推断]**
- **Engine A（arm64 Wine + libwow64fex）会改变 32 位的成本结构。**
  - 游戏代码和**所有 i386 PE DLL**，包括 wined3d、d3d9、DXMT 的 PE 侧，都要由 FEX JIT 执行；x87 hook 不再适用，改用 FEX 自己的 `X87ReducedPrecision` [64]–[66]。
  - 能否使用低 4 GB，取决于 `com.apple.developer.cross-architecture-support` entitlement [63]（与 15 号报告一致）。

## 详细调研

### 1. OpenGL 缓冲映射拷贝路径，以及 wined3d-GL 的实际表现

#### 1.1 上游机制（master = 11.18）[高]

`wow64_map_buffer()`（master 第 1473–1570 行，2026-09-27 取样）的决策顺序如下 [1]。**核查更正**：原文的顺序是 lucky → Vulkan placed → AMD pinned → 拷贝，与代码不符，下面按代码重排。

1. `if (buffer && buffer->vk_memory)`：Vulkan placed memory，用 `vkMapMemory2KHR` 加 `VK_MEMORY_MAP_PLACED_BIT_EXT` 映射到预留的低地址。
   - 前提是 `initialize_vk_device()`（第 298 行）成功。它要求 GL 侧有 `GL_EXT_memory_object_fd`，Vulkan 侧有 `VK_EXT_map_memory_placed` 和 `VK_KHR_map_memory2`。
   - 这条路径随 Wine 10.18 发布，ANNOUNCE 的第一条就是“OpenGL memory mapping using Vulkan in WoW64 mode”；相关提交是 Jacek Caban 2025-10-29 的 `a2906847`（“opengl32: Support wow64 buffer storage and persistent memory mapping using Vulkan.”）等 [7][74][75]。
   - **核查更正**：Wine 10.18 的发布日期是 2025-10-31（tag `wine-10.18` 的日期为 2025-10-31T21:47Z），不是原文写的 2025-11-03；原文的 MR 编号 !9032 因 GitLab Anubis 拦截无法核实。
2. `if (buffer && buffer->pinned)`：`GL_AMD_pinned_memory`。
   - 第 1、2 条只对 opengl32 自己包装分配的缓冲生效。
3. `if (ULongToPtr(PtrToUlong(ptr)) == ptr) /* we're lucky */`（第 1542 行）：驱动指针低于 4 GB，直接返回。macOS 上 Wine 为 WoW64 预留了整个低区，驱动分配基本都落在 4 GB 以上，这一条几乎不会命中。**[推断]**
4. `GL_MAP_PERSISTENT_BIT`：直接 FIXME（“GL_MAP_PERSISTENT_BIT not supported!”）并 `goto unmap` 失败。
5. **影子缓冲（拷贝）路径**：
   - `buffer_vm_alloc()` 用带 `zero_bits` 的 `NtAllocateVirtualMemory` 分配低地址影子缓冲；**unmap 时不释放**，留着复用；
   - map 时只有在**没有** `GL_MAP_INVALIDATE_RANGE_BIT` / `GL_MAP_INVALIDATE_BUFFER_BIT` 时，才把驱动数据 `memcpy` 到影子缓冲，并打印 FIXME “Doing a copy of a mapped buffer (expect performance issues)”（第 1562 行）。这条 FIXME 有 `static int once` 保护，**每个进程只打印一次**；带 INVALIDATE 位的 map 同样使用影子缓冲，但不打印任何东西；
   - unmap 时如果设置了 `GL_MAP_WRITE_BIT`，再按 `copy_length` 拷回去。
- 版本上限（第 483 行）：WoW64 下如果 `initialize_vk_device()` 失败，并且没有 `GL_AMD_pinned_memory`，上下文版本会被压到 4.3，同时隐藏 `GL_ARB_buffer_storage`。

Apple GL（4.1 core 或 2.1 legacy）没有 `GL_EXT_memory_object_fd`，也没有 `GL_AMD_pinned_memory`，所以在 macOS 上，驱动指针在 4 GB 以上时**只剩第 5 条影子缓冲路径**。核查确认这一结论成立。4.3 的版本上限对 Mac 没有影响，因为 Apple GL 最高只到 4.1。

背景：Elizabeth Figura（CodeWeavers）2024-10-14 在 wine-devel 上讨论过几条可选方案。她认为 “Just use Zink” 不可行（慢，而且旧 GPU 不支持 Vulkan），支持用 GL 扩展加回调的做法 [8]。上游最后选的是 Vulkan placed memory，这条路只对 Mesa 和 Linux 驱动有效。

#### 1.2 CrossOver 早就修掉了（至少从 23.5.0 起）[中高]

dappermint/winecx 的 `crossover-26.3.0` 分支来自提交 `40c09507`（2026-08-12T21:13:52Z）。它的说明首行是 “winecx-26.3.0”，正文是 “imported from crossover-sources-26.3.0.tar.gz”，并注明以 wine 11.0 为基础 [4]。其中的 `unix_wgl.c` 在 “we're lucky” 判断（第 2617 行）之后，紧跟一段 `#ifdef __APPLE__` 的 `else` 分支（第 2623 行起；注释在第 2631 行，`mach_vm_remap` 调用在第 2643 行）[3]：

```c
/* Get some low memory, then remap it to the host allocation. */
base = (vm_map_address_t)ptr & ~PAGE_MASK;
mapping_size = (length + ((vm_map_offset_t)ptr - base) + PAGE_MASK) & ~PAGE_MASK;
if (!buffer_vm_alloc( teb, buffer, mapping_size )) ...
lowaddr = (UINT_PTR)buffer->vm_ptr;
kr = mach_vm_remap( mach_task_self(), &lowaddr, mapping_size, 0, VM_FLAGS_FIXED|VM_FLAGS_OVERWRITE,
                    mach_task_self(), base, FALSE, &cur_protection, &max_protection, VM_INHERIT_DEFAULT );
buffer->map_ptr = (void *)(lowaddr + ((vm_map_offset_t)ptr - base));
```

在同一镜像的 CX 25.1.0（`aa5ddd8`，第 2344 行）和 CX 24.0.4（`0ca30cd4`，第 2317 行）中，这段代码同样存在 [5]。更早的 CX 23.5.0 导入（`5fcf4c8`）也已经有 `mach_vm_remap`，只是写法不同：直接用 `NtAllocateVirtualMemory` 加 `zero_bits` 申请低地址，没有经过 `buffer_vm_alloc()`。CX 22.1.1 的导入（`3c875fb`）中还没有 [72]。

**所以“CX 26 有没有打这个补丁”的答案是：有，而且至少从 23.5.0 起一直都有。** 上游始终没有 [1]。（**核查更正**：原文写“至少从 24.0.4 起”，结论不算错，但低估了时间跨度。）

要注意两点：

- 镜像里没有看到 Apple 专用的拆除逻辑。unmap 或删除缓冲时，别名映射如何回收，需要 Cider 自己审计；
- 如果 `mach_vm_remap` 失败，代码设置 `GL_OUT_OF_MEMORY` 并 `goto unmap`，不会退回到拷贝路径（已经核查确认）。

#### 1.3 对 D3D9（wined3d-GL）的实际影响：理论分析 [高：代码；推断：量级]

- wined3d 里有 `d3d_info->persistent_map = !!gl_info->supported[ARB_BUFFER_STORAGE]`，32 位下还有 `MAX_PERSISTENT_MAPPED_BYTES` 为 128 MB 的上限 [12]。Apple GL 4.1 本来就没有 `ARB_buffer_storage`，所以 **wined3d 在 macOS 上从来没有持久映射**，无论是否 WoW64。拷贝路径只影响“每次 lock 时的 map/unmap”。
- 对典型 D3D9 游戏来说，每帧动态 VB/IB 只有几 MB，按 10–30 GB/s 的 memcpy 速度估算只需不到 1 ms。**[推断]** 真正可能造成数量级影响的是两种情况：无 invalidate 的读回式 map，以及影子缓冲一直不释放。后者会让 LAA 32 位游戏（如 FNV 4GB 补丁版、GTA IV）的虚拟地址空间提前耗尽。**[推断]**
- 旧式 GL 1.x 游戏大多用客户端数组。WoW64 下 32 位指针可以零扩展后直接交给 64 位驱动，不经过 map。**[推断]**

#### 1.4 真实用户数据（Wine ≥10 / CX 25–26，Apple Silicon）

| 游戏（API / 位数） | 硬件 / 系统 | 栈 | 结果 | 来源 / 性质 |
|---|---|---|---|---|
| Half-Life 2 Demo，20 周年版（D3D9 / 32 位） | M1 Pro / macOS 27.0 | Sikarugir Wine 10（r5 为 33 fps，r7 即 `x64-sikarugir10.0_6-r7` 为 38 fps），wined3d-GL | 33–38 fps（trainstation 片段，1280×720 timedemo，GPU 2–3 ms） | [18] 维护者单次实测（2026-09-20/24）；`verified` 块用 r5 数字 |
| 同上 | 同上 | 同上，wined3d-**Vulkan**（MoltenVK） | 38 fps，与 GL 相同（日志确认使用 Vulkan 渲染器） | [18] 实测 |
| 同上 | 同上 | 同上，DXVK-d3d9（MoltenVK 1.4.1） | 14–17 fps（r7 上为 17 fps） | [18] 实测 |
| 同上 | 同上 | **CX 26.3 源码的 Wine 11**（Highball 自有引擎 build 15；CX 26.3 基底出自 issue #5，JSON 未写明），wined3d | **131 fps**（原因“not yet understood”） | [18][73] 单次实测 |
| 同上 | 同上 | 同上，加 athei 的 x87sidecar 测试构建 | **174 fps**（+30%） | [18] 实测，未发布 |
| 同上 | 同上 | CX 26.3 Wine 11，DXVK-d3d9（MoltenVK 1.4.2 + shadow-import 补丁） | 窗口不出现，报 `VK_ERROR_OUT_OF_HOST_MEMORY` | [18][23] |
| CS:GO Legacy（D3D9 / 32 位） | M1 Pro / macOS 26.6.2 | CX 26.3 Wine 11（r7），wined3d-GL，NVIDIA 身份 + 32 位 NVAPI stub | 1728×1117 默认设置 17 fps；1280×800 下 30–62 fps | [19] 实测 |
| 同上 | 同上 | DXVK-d3d9 | Source 的 HDR pass 上 MoltenVK 丢设备，无法进图 | [19] |
| Five Nights at Freddy's（Clickteam D3D9 / 32 位） | M1 Pro / 26.6.2 | Wine 10，DXVK-d3d9 | 60 fps（GPU 0.5 ms）；wined3d “is the slow path”（无数字） | [20] 实测 |
| 同上 | M1 Pro / 27.0 | Wine 11（r11/r12）DXVK 对比 Wine 10 DXVK | 40 fps 对 76 fps（菜单） | [20] 实测 |
| Fallout: New Vegas（D3D9 / 32 位） | M3 Pro / 27.0 | CX 26.3 Wine 11，DXVK | 菜单 40–50 fps，悬停时掉到 10 fps 以下 | [21] 社区报告 |
| 同上 | M4 / CX 26.0 | CrossOver 默认（推断为 wined3d） | Ultra 画质下 “barely playable” | [26] 社区报告 |
| GTA IV CE（D3D9 / 32 位） | M2 / CX 25.0；M3 Pro / CX 25.1.1 | CrossOver 默认（推断为 wined3d） | 45 fps（低画质）；40 fps（Ultra，3456×2234） | [25] 社区报告 |
| GTA IV | M1 Max | neo773/d9mt（D3D9→Metal） | 约 50–90 fps | [32] 作者自述 |
| WoW 3.3.5a（D3D9 / 32 位） | M5 / macOS 26 | Wine x86_64 + Rosetta + mtld3d | 户外 24–27 fps（37 ms），x87 占帧时间 87–90%；把两处 CALL 改成 NOP 后 117 fps（被 120 Hz 封顶） | [46] 实测，NOP 是针对该游戏的 hack |

**对问题 1 的结论：**

1. 上游在 macOS 上，只要驱动指针在 4 GB 以上，就一定走影子缓冲（拷贝）路径；CrossOver 至少从 23.5.0 起就用 remap 规避了（**核查更正**：原文为 24.0.4）[1][72]。[中高]
2. 目前**没有**任何测量能把拷贝路径的开销单独剥离出来。HL2 中 GL 与 Vulkan 渲染器同为 38 fps，这是一个**反证**：在 Wine 10 引擎上，限制性能的是与渲染后端无关的因素。[推断]
3. 候选因素有几个：msync（Highball 在 Steam recipe 中写道 “games gain ~40% frame rate from msync”，见 17 号报告）、CX 对 winemac 和同步的改动、Wine 11 在 macOS 上换 `%gs` 的 syscall 路径 [9]，以及下文 §1.5 的 DEP 缺页风暴。**需要 Cider 自己做 A/B 测试。**
4. DXVK-d3d9（macOS 分支）加 MoltenVK 在 Wine 10 树上常常比 wined3d 快（FNaF），但在 Wine 11 树和 MoltenVK 1.4.2 上出现了退化和故障 [20][23]。**默认后端必须按游戏逐个决定。**
5. CrossOver 自己的 “DXVK” 选项写的是 “for Direct3D 10 and 11”[28]，所以 CX 用户的 D3D9 基本都走 wined3d。[中]

#### 1.5 顺带发现：Rosetta 下的 DEP“缺页风暴”[中]

athei 的 wine-build README 指出一个问题 [41][42]：只要进程中**任何**一个模块缺少 `NX_COMPAT`，Wine 就会在整个进程里关闭 NX，于是“every readable mapping executable”。在 Rosetta 下，每个新的可写且可执行页在第一次访问时都要走一次 Mach 往返，结果是 “a 32-bit program with one 2000s-era DLL turns into a fault storm on every allocation”。

它的补丁是 athei/wine `cx-26-patched` 分支的 `539aa62`：“ntdll: Keep no-exec on under Rosetta and decide it from the main executable”，即只根据主程序决定 NX，在 Rosetta 下始终开启 NX。这正是老 32 位游戏的典型场景，Cider 应当直接评估并移植。

### 2. Rosetta 的 x87 性能，以及 26.x/27 有没有变化

- **机制 [中]**：Rosetta 对 x87 做完整的 80 位软件模拟，每条 x87 指令大约展开成 20 条 ARM 指令（见 06 号报告；WoW 社区的说法是 20–21 条）。在 D3D9 时代的 32 位游戏里，这经常成为 CPU 瓶颈。例如 WoW 3.3.5a 天空和光晕的两个循环有 234 条 x87 指令，占了 87–90% 的帧时间 [46]。
- **Apple 方面 [高]**：
  - macOS 26 的 release notes 只加了 `nox86exec=1` boot-arg，用于测试对 Rosetta 的依赖 [51]；
  - 26.4 加了 Rosetta 使用提醒，并重申“older, unmaintained gaming titles”会继续得到支持 [52]；
  - 27 的说明中有几条相关内容 [50]：
    - 如果之前装过 Rosetta，升级到 27.0 后它不会自动恢复（163213094）；
    - 以前设为“Open using Rosetta”的应用，现在会原生启动（168097174）；
    - “All Intel-based software will no longer be compatible with macOS 28.0, excluding legacy games”；
    - 还有上文的 `game-test-tool`（166398727）；
  - **三份说明都没有提到 x87**。
  - 已知的运行时变化只有一处：rosettax87_jit 的 issue #14（2026-09-12）报告，在 macOS 27 RC（26A428）上出现 `PT_THUPDATE failed`，而此前的 27 dev beta 没有这个问题 [45]。**没有证据表明 Apple 改进了 x87**。[中]
- **社区加速方案（截至 2026-09）**：

| 方案 | 原理 | 数据 | 系统 | 约束 |
|---|---|---|---|---|
| rosettax87（Lifeisawful） | 把 Rosetta 的 x87 handler 换成低精度快速版 | 229,731 对 48,517 tick，约 4.7 倍 | “macOS 15.5 or compatible” | 2026-01-02 归档（API 显示 `archived=true`，最后一次 push 是 2026-01-02T22:59Z）[44][80] |
| rosettax87_jit | 在翻译管线中直接生成 AArch64 | 无公开数字 | 需要调试授权；有关闭 SIP 的选项 | 上游 Wine 已删除 `ROSETTA_X87_PATH` 支持；在 27 RC 上报错 [45] |
| **x87sidecar**（athei → rdbell 分叉） | 修补 Rosetta 的 `translate_insn` 序言，把 x87 交给**进程外**的原生 JIT 处理 | M5 Max / macOS 27.0 上 179 项微基准平均快 61 倍（mul 83 倍，sqrt 247 倍）；HL2 Demo +30% [18][43] | 声明支持 26 和 27；启动时探测运行时，不认识的运行时拒绝修补 | cooperative 模式无需 entitlement，可以公证；需要 Wine 补丁（athei/wine `e00a772`）；默认关闭 FMA 收缩以保持 x87 的舍入语义 |

  - 还有一点：x87sidecar 的 README 指出，stock Rosetta 在 32 位下处理 `DC D8`（fcomp 别名）和遗留模式的 `ARPL` 时会 trap。winerosetta 和 WineRosetta2 也在处理这类问题（针对 WoW 1.12/2.4.3/3.3.5a）[43][49]。
  - WoWSilicon v3.2.x（2026-09-23/24）打包了 Wine 11.13、MTLd3D 和 x87sidecar 1.7.0，声称支持 macOS 14/15/26/27 [47]。matasarei/wow-launcher 的 issue 中则有人说 “x87sidecar ineffective on newer macOS/M-series; logs cache invalidations then becomes dormant”[46]。**说明效果依赖具体场景，必须逐个游戏验证。**
- **对 Cider 的判断**：x87 加速是 D3D9 时代 32 位游戏“能否玩”的第二大杠杆，第一大是 Wine 源码树本身。但 x87 hook 本质上是在修补 Apple 私有运行时，而 macOS 28 的老游戏模式会“disables Rosetta”[50]，**所以它是一项短期收益（macOS 26–27）**。[推断]

### 3. wined3d Vulkan 渲染器在 MoltenVK / KosmicKrisp 上承载 D3D9 的成熟度

- **功能 [高]**：
  - Wine 10.0 新增了基于 HLSL 的 FFP（“fixed function emulation for the Vulkan renderer”）和动态状态扩展；
  - Wine 11.0 补齐了点大小、点精灵、顶点混合和固定管线凹凸贴图，vkd3d-shader 大幅改进了 SM1–3 支持，包括 SM1 像素着色器；
  - 但官方仍说明 “not yet at parity with the GL renderer, and is therefore not yet the default”[9][10]。
- **切换方式 [高]**：环境变量 `WINE_D3D_CONFIG=renderer=vulkan`（也支持 `csmt=`、`shader_backend=`、`MaxShaderModel*`），或者注册表 `HKCU\Software\Wine\Direct3D` 的 `renderer`（可按 AppDefaults 逐程序设置）。`renderer=auto` 时仍使用 OpenGL [14]。
- **设备门槛 [高]**：`feature_level_from_caps()` 的要求如下 [13]：
  - FL9_2：`occlusionQueryPrecise`；
  - FL9_3：SM≥3 加 `independentBlend`；
  - FL10：还需要 `geometryShader`、`pipelineStatisticsQuery`、`depthBiasClamp`、`multiViewport`、clip/cull distance、实例除数等；
  - 必需的设备扩展只有 `VK_KHR_swapchain` 和 `VK_KHR_maintenance1`。
  - KosmicKrisp（Mesa main 镜像，2026-09）有 `independentBlend`、`occlusionQueryPrecise`、`depthBiasClamp`、`multiViewport`、`tessellationShader` 和 `imageCubeArray`，但没有 `geometryShader` 和 `pipelineStatisticsQuery` [16]。所以 **D3D9 可以完整运行，而 D3D10/11 会被限制在 FL9_3**，也就是“FL 门槛”问题。
- **WoW64 映射 [高]**：
  - wined3d-vk 在任何平台上都固定 `persistent_map = true`[12]；
  - Wine 11 的 `win32u/vulkan.c` 在 WoW64 下优先用 `VK_EXT_map_memory_placed`，其次用 `VK_EXT_external_memory_host`，以保证映射指针在 32 位范围内 [15]；
  - KosmicKrisp 同时暴露 `EXT_map_memory_placed`、`EXT_external_memory_host` 和 `KHR_map_memory2`[16]；MoltenVK 有 `external_memory_host` 和 `map_memory2`，没有 `map_memory_placed`[17]。
  - 所以**在两个 ICD 上，Vulkan 系 D3D9 在 WoW64 下都能零拷贝**，这是它相对于 GL 路径的结构性优势。
- **Mac 实测**：只有 HL2 Demo 一条数据，MoltenVK 上 wined3d-vk 与 GL 同为 38 fps [18]。wined3d-vk 在 KosmicKrisp 上**没有任何公开测试**，Highball 对 KosmicKrisp 的评估（#41）也“parked”了。[高]
- **旁证**：Gcenx/kosmickrisp-dxvk 的做法是在 KosmicKrisp 上**宣称**支持 `geometryShader` 和 `fillModeNonSolid`，并用 `mach_vm_remap` 实现 placed memory。这样上游 DXVK 的 d3d9 和 d8vk 就能跑在 CX 26.2 上，验证用的是 GTA IV（D3D9）和 GTA III（D3D8，M1 Max，macOS 27）。缺点是冷缓存时卡顿严重 [31]。另外，K0bin 于 2026-08-21 在 DXVK-macOS 分支上把 `fillModeNonSolid` 改成了可选（`[MTLHACKS]`）[30]。

### 4. D3D9→Metal 项目现状

| 项目 | 许可 | 架构 | 最近活动 | 覆盖面与数据 | 评价 |
|---|---|---|---|---|---|
| **neo773/d9mt** | 跟随 vendored 的 DXVK 和 SPIRV-Cross | DXVK D3D9 前端 → SPIR-V → SPIRV-Cross → MSL；借用 DXMT 的 winemetal；需要 CX 26+ | main 最后提交 **2026-06-24**（`237e293`）；`perf/*` 分支最后提交在 06-22/23。仓库 `pushed_at` 虽为 2026-07-15，但所有分支都没有更晚的提交，也没有 release。约 68 star、17 fork（含 Gcenx，2026-07-18）[32] | 只测过 GTA IV，M1 Max 上约 50–90 fps | 停滞，适合作参考 |
| **Sikarugir-App/d9mt `dx9` 分支** | 与 DXMT 同源 | athei 的 `c16e245`（2026-03-27）：`src/d3d9/`、`src/airconv/` 中的 DXSO 编译器，FFP，`tests/dx9/`；`69361df`（03-28）修 FFP 常量 [33] | **2026-03-28 以后无提交**；issue 区已关闭 | 71 个文件，+15,685 行 | **确有真实代码**，但作者已转向 mtld3d |
| **athei/mtld3d** | **zlib** | Rust 实现。链路为 `d3d9.dll`（PE）→ `mtld3d.dll`（unix-call shim）→ `mtld3d.so`（Metal）。命令先在 PE 侧录制，再通过一次 `unix_call(SubmitCommandBuffer)` 在 unix 侧回放。staging 和 VB/IB 使用 PE 自有内存，外面套 bytesNoCopy 包装（“PE-owned memory under a bytesNoCopy wrapper”，即 `newBufferWithBytesNoCopy`）。4 个异步编译线程 [37][39] | 仓库创建于 2026-07-14。版本从 v0.1.0（2026-07-14）到 v0.11.0（**2026-09-25**）共 14 个 release，另有一个 test-issue-853 预发布，非常活跃 [40][76]。（**核查更正**：原文把起点写成 v0.4.1（2026-08-07），那只是中间版本。）CI 用的 Wine 基于 CrossOver 26，CX 27 的 arm64 Wine 只做过手工测试 | SM1–3（DXSO→MSL，按内容哈希做磁盘缓存）、FFP（光照、texgen、vertex blend、雾）、MRT、MSAA、MetalFX、HDR；缺 D3D9Ex、timestamp query、FFP bump-env；**有意偏离规范**，例如每次 Present 丢弃深度 [38]。v0.6.0（2026-08-16）支持 arm64 Wine（“verified against arm64 Wine with a 32-bit game”），从这一版起同时附带 `wine/x86_64-unix/mtld3d.so` 和 `wine/aarch64-unix/mtld3d.so`；D3D8 已在计划中 | **Cider 的首选 D3D9 实验后端** |
| DXMT（3Shain） | LGPL-2.1+（v0.80 以后） | — | 1.0 路线图（#151，2026-04-21）只涉及 D3D10/11；更早的讨论 #4 中给出的优先级是 “DX11 > DX12(5.1) = DX10 > DX9 > …” [34][35] | 32 位（WoW64）自 v0.50（2025-04-26）起已支持，release 说明写的是 “32-bit program support (WoW64 build) (#67)” [79] | **不要指望它做 D3D9** |
| WineMetalGL（metalsharp） | MIT / LGPL | GL→Metal 直译，GL 3.3 约 93% | 创建于 2026-07-30，4 star [70] | 无游戏数据 | 过早 |

mtld3d 的真实部署：WoWSilicon 在 wine-runtime r16（2026-09-23）中加入了 MTLd3D 0.10.0，次日（r17）又回退到 0.7.0 [47]。说明它仍在快速变化，**回归风险很高**。

### 5. 16 位与 LDT：新 WoW64 + Rosetta，macOS 26/27

- **上游代码 [高]**：
  - `signal_x86_64.c` 的 `ldt_set_entry()`：Linux 用 `modify_ldt`，macOS 用 `i386_set_ldt(sel >> 3, …)`（第 2673 行）；
  - 32 位代码选择子通过 `cs32_sel = ldt_alloc_entry( ldt_make_cs32_entry() )` 分配（第 2825 行）；
  - 16 位的判定是 `is_16bit()`，即 `SS != ds64_sel`，不区分平台；
  - 装上自定义 LDT 以后，macOS 的 mcontext 会变成 `_STRUCT_MCONTEXT64_FULL`，只能靠 `uc_mcsize` 识别；
  - 源码中与 Rosetta 相关的注释只有两处：“Only applies on Intel, not under Rosetta”（syscall CS 修正）和 “Setting debug registers is not supported under Rosetta”[60][61]。
- **Wine 版本**：Wine 11.0 宣布 “16-bit applications are supported in the new WoW64 mode”[9]；11.18 修了 QuickTime 2.x 的 win16 安装器问题（#18260）[11]。平台没有说明，推断是在 Linux 上验证的。
- **Rosetta 能否执行 16 位代码段（D=0）以及 16 位寻址：没有找到任何一手或社区证据。** 已知 stock Rosetta 在 32 位的遗留编码上有空洞（`ARPL`、`DC D8`）[43]，**所以不能假设 16 位可用**。[未验证]
- **Engine A**：Windows on ARM 本身就不支持 16 位；FEX 的 WoW64 对 16 位段的支持情况没有查到。**[推断：大概率不可用]**
- **兜底方案**：otvdm/winevdm（GPL-2.0）是一个 32 位程序，内部自己模拟 16 位环境（README 写明 “64-bit Windows cannot modify LDT”）[69]。理论上它不依赖宿主 LDT，在 Engine R 和 A 上都能作为 16 位的后备。DOS 程序交给 DOSBox。

### 6. Apple GL 4.1 的弃用风险、Zink PE MR !10531、KosmicKrisp 上的 GL

- **Apple 方面 [高]**：
  - macOS 26 的 AGL 条目写道 “AGL is no longer available in the macOS SDK … OpenGL still remains in the SDK. (153913819)”[51]；
  - 我们逐字检查了 macOS 27 的 release notes JSON，其中没有 “OpenGL”“OpenCL”“GLKit” 字样；Deprecation 小节只有一条 Intel 应用 Get Info 标识 [50]；
  - Xcode 27 删除了 ld64（`-ld_classic` 不再可用），`MACOSX_DEPLOYMENT_TARGET>=27` 时默认不再构建 x86_64，但 SDK 仍然支持 x86_64 [53]。
  - **结论**：在 2026-09 这个时间点，GL 4.1 没有新的移除信号。风险主要在 macOS 28 的 Rosetta 收缩：Engine R 的 GL 调用走的是 x86_64 的 OpenGL.framework，它能否留下要看老游戏机制。Engine A 使用 arm64 的 OpenGL.framework，不受这一点影响。[推断]
- **MR !10531 [中高]**：
  - Rémi Bernon 于 2026-04-02 提交，标题 “opengl32: Just use Zink (as PE-side OpenGL implementation)”，内嵌 Mesa 26.0.3 的子集，Steam 和 KOTOR 可以运行。作者自称这是 “meant mostly as a half joke for April fool's day, but it works”[56]；
  - GitLab 的 Anubis 挡住了抓取，无法直接看 MR 状态。但 2026-09 的 wine-mirror `libs/` 中没有任何 Mesa 目录，commits 搜索 “zink” 也是 0 条 [57]。**判断为未合入。**
- **Zink 在 Mac 上的能力天花板 [高：文档；推断：结论]**：
  - Zink 的基线要求包括 `fillModeNonSolid`；GL 3.0 需要 `VK_EXT_transform_feedback`；GL 3.2 需要 `geometryShader`[58]；
  - 上游 KosmicKrisp 这三项都没有 [16]（另见 05 号报告），MoltenVK 有 `fillModeNonSolid`，但没有 XFB 和 GS；
  - **所以 PE-Zink 在 Mac 上最多提供 GL 2.1，而 Apple GL 有 4.1 core，会造成倒退。** 它唯一的好处是：winevulkan 的 placed memory 映射能自然解决 WoW64 拷贝问题。
- **KosmicKrisp 上的 GL 实验 [中]**：lucamignatti 的 gist（最近活动 2026-08-22）在 macOS 上原生运行 Minecraft，用 `MESA_GL_VERSION_OVERRIDE=4.6` 强行报告 4.6：
  - 初始约 70 fps；某个带光影包的配置是 25 fps 对原生 100 fps；
  - 瓶颈在 CPU，XFB 缺失导致部分光影包失败；需要 surfaceless 平台补丁和 `DYLD_INSERT_LIBRARIES` interpose；
  - 作者认为补丁 “very unlikely” 能整体合入上游 [59]。**离产品化还很远。**

### 7. Engine A（arm64 Wine + libwow64fex）上的 32 位 x86

- **执行模型 [推断，与 15 号报告一致]**：
  - 32 位游戏代码，以及**全部 i386 PE DLL**（wined3d、d3d9/d3d8、opengl32 的 PE 侧、DXMT 和 mtld3d 的 PE 侧）都由 FEX 的 WoW64 JIT 执行；
  - wow64 thunk 层和 unix 侧是原生 arm64。Engine R 则是全部 x86_64 代码交给 Rosetta 做 AOT 翻译。
  - 所以在 Engine A 上，**把重活放在 unix 侧的设计更划算**。wined3d 的主体在 PE 侧，全部要被模拟。mtld3d 的“PE 录制、unix 回放”结构相对有利，但其 D3D9→Metal 翻译有多少在 PE 侧，尚未核实。
- **x87 [高]**：
  - Rosetta 的各种 hook 都失效，改用 FEX 自身。
  - `X87ReducedPrecision` 默认为 false（80 位精确模拟）；打开后用 64 位精度，可能出现渲染错误。
  - FEX-2604（2026-04-09）把 reduced-precision 路径下的 SIN/COS/TAN 内联，平均快 3.7 倍，点名受益的是 Fallout: New Vegas 和 Bayonetta；FEX-2605（GitHub release 时间为 2026-05-09T03:02Z UTC，美国时区是 05-08）又优化了 ATAN、FYL2X、FSCALE、F2XM1，快 2–4 倍，开发者说 “starting to run out of x87 instructions to optimize”[64]–[66][78]。
  - **Cider 需要按游戏配置 FEX 的 x87 精度。**
- **TSO**：`TSOEnabled` 默认开启；另有 `VectorTSOEnabled`、`MemcpySetTSOEnabled`、`HalfBarrierTSOEnabled` 和 PE 的 `VolatileMetadata` 等细粒度选项 [64]。硬件 TSO 需要 Apple 授权，见 06 和 15 号报告。
- **低 4 GB [高：源码；中：entitlement 结论]**：
  - xnu `mach_loader.c` 对 64 位 ARM64 可执行文件要求覆盖低 4 GB 的 hard pagezero（`vm_map_has_hard_pagezero(map, 0x100000000)`），而且 pagezero 段会让 map 的 `min_offset` 上移 [62]；
  - dappermint/winecx 的 arm64 分支 `9cc1f74`（2026-08-26）在 `virtual_init()` 中用 `mach_vm_deallocate` 释放 pagezero，并注明只有 loader 带 `com.apple.developer.cross-architecture-support` 时才会成功 [63]；
  - 15 号报告进一步核实：26.4 及以后的 xnu 在 `ml_satisfies_x86_64_requirements()` 为真时，会改为 soft pagezero。
  - **没有这个 entitlement，Engine A 上的 32 位程序完全不可用。**
- **构建约束 [高]**：mtld3d v0.6.0 在支持 arm64 Wine 时，把 D3D DLL 重新构建为不含 BMI1/BMI2/LZCNT（32 位以 Nehalem 为基线），并让 unix 崩溃处理器把 “x86 translation faults” 转发给先前的处理器 [40]。**Cider 的 i386 PE 组件同样需要保守的 ISA 基线。**
- **GL**：CX 的 remap 思路在 arm64 上同样适用，只是按 16 KB 页对齐。**[推断]**
- **16 位**：见 §5，大概率只能靠 otvdm。

### 8. 默认后端矩阵（老游戏）

| API / 位数 | Engine R 默认（2026 Q4–2027） | Engine R 备选（按游戏白名单） | Engine A（2027+） | 依据 |
|---|---|---|---|---|
| D3D9 / 32 位 | **wined3d-GL**，运行在带 CX remap 与 msync 的 Wine 11 树上 | ① mtld3d（实验）；② DXVK-d3d9 macOS 分支 + MoltenVK（仅在已测更快的游戏上）；③ wined3d-vk（KosmicKrisp 上实验） | mtld3d（有 arm64 unix 库）或 wined3d-GL；按游戏配置 FEX 的 x87 精度 | [18][19][20][37] |
| D3D9 / 64 位 | 同上 | 同上 | 同上 | — |
| D3D8 | wined3d-GL | DXVK d8vk 跑在打补丁的 KosmicKrisp 上（GTA III 已验证）；将来 mtld3d 的 D3D8 | 同 R | [29][31][37] |
| DDraw / D3D≤7 | wined3d-GL | cnc-ddraw（Sikarugir 的默认项之一，本文未深入） | 同 R | [29] |
| D3D10/11 / 32 位 | **DXMT**（WoW64，v0.50+） | wined3d-vk（需要放宽 FL 门槛，优先级低） | DXMT 的 i386 PE 部分需要被模拟 | [34][36] |
| OpenGL ≤2.1 / 32 位 | Apple GL legacy（winemac）+ remap | — | Apple GL arm64 | [3] |
| OpenGL 3.2–4.1 core | Apple GL 4.1 | — | 同 R | [51] |
| OpenGL >4.1 | 无可靠方案 | Zink + KosmicKrisp（强制覆盖版本号，实验） | 同 R | [58][59] |
| Win16 | Wine 新 WoW64（待验证） | otvdm | otvdm | [9][69] |
| x87 密集 | 默认不启用 hook | x87sidecar（cooperative 模式，按游戏开启） | FEX `X87ReducedPrecision`，按游戏开启 | [43][64] |

## 对 Cider 的启示与建议（按优先级）

**P0（Engine R 上线前必须完成）**

1. **把 CX 的 `unix_wgl.c` remap 路径列入补丁队列。** 与 17 号报告“11.18 + CX 26.3 补丁队列”的基线对齐。
   - 补上 unmap 和删除时的别名回收审计；
   - remap 分支放在上游现有顺序中的位置：`vk_memory`、`pinned`、“we're lucky” 之后，`GL_MAP_PERSISTENT_BIT` 失败和影子缓冲之前，与 CX 的位置一致 [1][3]；
   - CI 检测影子缓冲路径（**核查后修订**）。上游的 “Doing a copy of a mapped buffer” FIXME 每个进程只打印一次，而且带 `GL_MAP_INVALIDATE_*` 的 map 也会用影子缓冲却不打印，所以不能靠数这条 FIXME。做法是：Cider 补丁在影子缓冲分支里加一条每次命中都打印、带累计计数的 `TRACE`。CI 用 `WINEDEBUG=+opengl` 跑 GL 冒烟测试，这个计数必须为 0，那条 FIXME 也不能出现；
   - 基于 !8907 的 wrapper 整理成上游 MR 提交 [3][6]。
2. **建立老游戏 A/B 基准，先弄清 38→131 fps 的原因。**
   - 38 和 131 fps 都是 Highball 维护者的单次测量，原因连作者都说“not yet understood”[18]。所以先在同一台机器上复现：用 Sikarugir r7 引擎和基于 CX 26.3 的 Wine 11 树各跑 3 次 HL2 trainstation timedemo（1280×720），确认差距确实存在；
   - 用 HL2 Demo 的 timedemo 和 CS:GO Legacy 在 M3（8 GB）上对比四种组合：上游 11.18、11.18 + remap、11.18 + msync、CX 26.3 原树；
   - 分别记录 `WINEDEBUG=+fps`、Instruments 中的 Rosetta 线程占比，以及缺页次数；
   - 这是“老游戏为 Cider 核心价值”的首要量化工作 [18][19]。
3. **移植 athei 的 NX/DEP 补丁 `539aa62`。** 在 Rosetta 下始终开启 NX，并只根据主 exe 决定 NX 策略。上线前用含老 DLL 的 32 位游戏复测缺页数 [42]。
4. **提供按游戏选择 D3D9 后端的基础设施。** 包括 `WINE_D3D_CONFIG`、AppDefaults 下的 `renderer`、DLL override 和环境变量作用域，并且**环境变量必须只作用于单个游戏**（吸取 Highball #198 的教训）。首批 recipe 从 highball-db（CC0）导入 [14][23]。
5. **准备 32 位 GPU 身份和 NVAPI stub。** CS:GO Legacy 在 wined3d 下，若用 AMD 身份会触发 CSM 检查，若用非 AMD 身份会触发 NVAPI 检查 [19]。

**P1（2026 Q4–2027 Q1）**

6. **把 mtld3d 接入为“Metal D3D9（实验）”。** 采用白名单制，从 WoW、HL2/TF2 和 GTA IV 开始。
   - 固定到某个 tag，并在 CI 中跑它自带的 154 项测试；
   - 与作者协作，不要自己分叉；
   - **不要新开一个 d9mt 项目**，neo773 和 Sikarugir 的两条线都已经停滞 [32][33][37]。
7. **把 x87sidecar 作为可选组件。** 用 cooperative 模式和 athei 的 `e00a772` 补丁，按游戏开启；启动时用 `--probe` 检查运行时，失败就自动关闭；**不采用 rosettax87_jit**（需要调试授权，且在 27 RC 上出错）[43][45]。
8. **评估 wined3d-vk + KosmicKrisp 跑 D3D9。** macOS 26+ 专用。这条路完全在上游，零拷贝，不需要虚报特性。同时以 Gcenx 的 kosmickrisp-dxvk 作为 DXVK-d3d9/d8vk 的对照 [13][16][31]。
9. **MoltenVK 版本锁定。** 在修复 Highball #198 所示的映射失败之前，不要升级到带 shadow-import 的 1.4.2 构建 [23]。

**P2（2027）**

10. **macOS 27 beta 上的关键实验：`sudo game-test-tool enable` 之后 Engine R 的 Wine 能否运行。** 这决定了 macOS 28 上是否还有 Rosetta 路线；同时要测 x87 hook 在该模式下是否失效 [50]。
11. **16 位**：在 Engine R 上用若干 Win16 程序做冒烟测试；失败时引导用户使用 otvdm [69]。
12. **Zink（!10531）**：只跟踪，不采用，直到 KosmicKrisp 具备 `fillModeNonSolid`、XFB 和 GS。
13. **wined3d-vk 的 FL 门槛**：暂时只作为 32 位 D3D10/11 的最后兜底，前面已有 DXMT 覆盖。可以考虑把 `pipelineStatisticsQuery` 改为软实现（返回 0），GS 缺失时只对不用 GS 的游戏放行。
14. **Engine A**：按 15 号报告申请 entitlement；i386 PE 采用保守 ISA；配置按游戏设置 FEX 的 x87 精度档；优先让 D3D9 的重活落在 unix 侧。

## 风险

1. **因果不清（高）**：38→131 fps 的差距还没有定位到具体原因。如果 Cider 只采用 11.18 加部分补丁，老游戏性能可能退回到 Wine 10 引擎的水平。
2. **x87 hook 是修补私有运行时的行为（高）**：随时可能被系统更新破坏（27 RC 上已经出现过一次），而且 macOS 28 的游戏模式明确会禁用 Rosetta [45][50]。
3. **mtld3d 高度依赖单人，且刻意偏离规范（中）**：仓库 2026-07-14 才创建（**核查更正**：原文写“2026-08 才开始”），有意为速度牺牲规范一致性，下游已经发生过回退 [38][47][76]。
4. **MoltenVK 1.4.2 与 DXVK-d3d9 在 Wine 11 树上退化（中）**：FNaF 从 76 fps 降到 40 fps，HL2 无法启动。“DXVK 更快”的旧经验不能直接照搬 [20][23]。
5. **Engine A 的 32 位完全取决于 Apple 的 entitlement（高）**：没有它，32 位兼容性为 0 [63]。
6. **数据稀缺（中）**：DX8 以及 16 位几乎没有可用数据；macgamingdb 的 “None” 层级只能推断为 wined3d [25][26]。
7. **8 GB 开发机**：可以跑 32 位游戏，因为它们本身受 4 GB 地址空间限制，但无法代表 M1 Pro 等机型的数据。

## 未解问题

1. HL2 在 CX 26.3 树上达到 131 fps 的真实原因：msync、remap、winemac、syscall 路径，还是 CSMT 行为？
2. CX remap 路径在缓冲删除时如何回收别名映射？Apple GL 驱动内部如果重映射了同一块内存，别名会不会失效？
3. Rosetta 能否执行 16 位代码段？CrossOver 26 或 27 对 Win16 的官方立场是什么？（CodeWeavers 博客返回 403，没能读到。）
4. `game-test-tool` 模式下，Wine 进程会被当作“游戏”吗？“new underlying system behavior”是不是一个新的翻译器？它如何处理 x87 和 32 位代码段？
5. mtld3d 的 DXSO→MSL 翻译和状态编码在 PE 侧还是 unix 侧？这决定了它在 Engine A 上的 CPU 成本。
6. MR !10531 的正式状态（GitLab 被 Anubis 拦截）。KosmicKrisp 何时补上 `fillModeNonSolid`、XFB 和 GS？XDC 2026 的演讲可能会给出答案。
7. wined3d-vk 在 KosmicKrisp 上跑 D3D9 的实际兼容性和性能，目前没有任何公开数据。
8. FEX 的 WoW64 对 16 位段和 LDT 的支持程度；CrossOver ARM64 Preview 能否运行 Win16。

## 参考来源

1. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/opengl32/unix_wgl.c — 上游 `wow64_map_buffer()`（第 1473–1570 行）：依次为 Vulkan placed（`vk_memory`）、AMD pinned、lucky 指针、PERSISTENT 失败、影子缓冲（有 once 保护的拷贝 FIXME）；第 298 行 `initialize_vk_device`，第 483 行 4.3 上限；无 `__APPLE__` 和 `mach_vm_remap`（2026-09-27 核查）
2. https://raw.githubusercontent.com/wine-mirror/wine/master/VERSION — master 为 Wine 11.18
3. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/opengl32/unix_wgl.c — CX 26.3 的 `#ifdef __APPLE__` `mach_vm_remap` 零拷贝路径
4. https://api.github.com/repos/dappermint/winecx/commits?sha=crossover-26.3.0 — 提交 `40c09507`：“winecx-26.3.0”，“imported from crossover-sources-26.3.0.tar.gz … wine 11.0 base”（2026-08-12T21:13:52Z）
5. https://raw.githubusercontent.com/dappermint/winecx/aa5ddd8eae4d785a95a6199f17e9deed6625f07f/dlls/opengl32/unix_wgl.c （CX 25.1.0）；https://raw.githubusercontent.com/dappermint/winecx/0ca30cd4978b80200c51387182ac8f4a1a0f44b9/dlls/opengl32/unix_wgl.c （CX 24.0.4）— remap 已存在
6. https://gitlab.winehq.org/wine/wine/-/merge_requests/8907 — WoW64 buffer wrapper，动机中提到 `mach_vm_remap` 和 `glImportMemoryFdEXT`（内容来自搜索摘要，页面被 Anubis 拦截）
7. https://www.gamingonlinux.com/2025/11/wine-10-18-brings-opengl-memory-mapping-using-vulkan-in-wow64-mode/ — Wine 10.18 在 WoW64 下用 Vulkan 做 GL 映射（报道发于 2025-11 初；10.18 本身发布于 2025-10-31，见 [74][75]；MR 编号 !9032 未能核实）
8. https://marc.info/?l=wine-devel&m=172894900019588&w=2 — Elizabeth Figura 2024-10-14 讨论 WoW64 GL 映射的几条路线
9. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — Wine 11.0：新 WoW64 完全支持、16 位、Vulkan 渲染器的遗留特性、macOS `%gs` 交换
10. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.0/ANNOUNCE.md — Wine 10.0：HLSL FFP、GLSL-vkd3d 后端
11. https://raw.githubusercontent.com/wine-mirror/wine/master/ANNOUNCE.md — Wine 11.18：KOTOR #60292、win16 #18260、winemac 改动
12. https://sourcegraph.com/search?q=repo:%5Egithub%5C.com/wine-mirror/wine%24+file:dlls/wined3d/+persistent_map — wined3d `persistent_map`、`ARB_BUFFER_STORAGE`、32 位 128 MB 上限（Sourcegraph 检索）
13. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/wined3d/adapter_vk.c — `feature_level_from_caps()` 与必需扩展
14. https://sourcegraph.com/search?q=repo:%5Egithub%5C.com/wine-mirror/wine%24+file:dlls/wined3d/wined3d_main.c+WINE_D3D_CONFIG — `WINE_D3D_CONFIG` 与 `renderer`、`csmt`、`shader_backend`
15. https://sourcegraph.com/search?q=repo:%5Egithub%5C.com/wine-mirror/wine%24+VK_EXT_external_memory_host — `win32u/vulkan.c` 中 WoW64 用 external_memory_host 或 placed 映射
16. https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/main/src/kosmickrisp/vulkan/kk_physical_device.c — KosmicKrisp 的扩展与特性（placed、external host、没有 GS 和 pipelineStatistics）
17. https://raw.githubusercontent.com/KhronosGroup/MoltenVK/main/Docs/MoltenVK_Runtime_UserGuide.md — MoltenVK 扩展列表（无 map_memory_placed）
18. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/half-life-2.json — HL2 Demo 的 wined3d、DXVK、x87sidecar 实测（M1 Pro，macOS 27.0，测于 2026-09-20/24；JSON 本身没有写 CX 26.3）
19. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/csgo-legacy.json — CS:GO Legacy 用 wined3d 的实测，以及 NVAPI 和 CSM 问题
20. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/five-nights-at-freddys.json — FNaF 的 DXVK 实测，Wine 11 树上的退化
21. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/fallout-new-vegas.json — FNV 社区报告
22. https://raw.githubusercontent.com/gauthierpiarrette/highball-db/main/db/games/grand-theft-auto-iv.json — GTA IV 用 wined3d 的社区报告
23. https://github.com/gauthierpiarrette/highball/issues/198 — `MVK_SHADOW_IMPORT` 导致 DXVK-d3d9 映射失败
24. https://github.com/gauthierpiarrette/highball/issues/165 — athei 提出的 x87 加速与 D3D9-on-Metal 建议
25. https://macgamingdb.app/games/grand-theft-auto-iv-the-complete-edition — GTA IV 在 CX 25/26 下的社区 fps
26. https://macgamingdb.app/games/fallout-new-vegas — FNV 在 CX 25/26 下的社区报告
27. https://www.codeweavers.com/crossover/changelog — CX 26.0.0（2026-02-10）、26.2.0（2026-06-09，32 位 bottle 警告）、26.3.0（2026-07-21）
28. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26 — CX 26 各图形后端的说明（DXVK 标注为 D3D10/11）
29. https://raw.githubusercontent.com/Sikarugir-App/Sikarugir/main/README.md — Sikarugir 默认：DX9 用 D9VK，DX8 及以下用 WineD3D，另有 CNC-DDRAW
30. https://github.com/Sikarugir-App/d9vk/commits/moltenvk-version — K0bin 的 MTLHACKS：fillModeNonSolid 改为可选（2026-08-21）
31. https://raw.githubusercontent.com/Gcenx/kosmickrisp-dxvk/main/README-DXVK.md — KosmicKrisp + DXVK d3d9/d8vk，用 `mach_vm_remap` 实现 placed memory
32. https://github.com/neo773/d9mt （README、/commits/main、/branches/all、/forks）；https://api.github.com/repos/neo773/d9mt/commits?sha=main — 最后提交 2026-06-24（`237e293`），GTA IV 约 50–90 fps
33. https://github.com/Sikarugir-App/d9mt/commits/dx9 ；https://api.github.com/repos/Sikarugir-App/d9mt/commits?sha=dx9 ；https://github.com/Sikarugir-App/d9mt/commit/c16e245 — athei 的 D3D9 实现（71 个文件，+15,685/−186）；HEAD `69361df`（2026-03-28）
34. https://github.com/3Shain/dxmt/discussions/4 — 3Shain 给出的优先级与 32 位说明
35. https://github.com/3Shain/dxmt/issues/151 — DXMT 1.0 路线图，只涉及 D3D10/11（2026-04-21）
36. https://github.com/3Shain/dxmt/releases — v0.50 加入 32 位 WoW64，v0.80（2026-04-23）
37. https://github.com/athei/mtld3d — mtld3d README（zlib，测试过的游戏，arm64 支持）
38. https://raw.githubusercontent.com/athei/mtld3d/main/docs/STATUS.md — 已实现功能、缺口与有意偏离规范之处
39. https://raw.githubusercontent.com/athei/mtld3d/main/docs/ARCHITECTURE.md — PE 与 unix 的分工、`newBufferWithBytesNoCopy`、线程模型
40. https://github.com/athei/mtld3d/tags ；https://github.com/athei/mtld3d/releases/tag/v0.6.0 ；/v0.8.0 ；/v0.11.0 — 版本时间线与 arm64 说明
41. https://github.com/athei/wine-build — 基于 CX 的构建；Rosetta 下 NX 缺页风暴的说明
42. https://github.com/athei/wine/commits （`cx-26-patched`）— `539aa62` NX 补丁、`e00a772` x87sidecar 接入
43. https://github.com/rdbell/x87sidecar （分叉自 athei/x87sidecar）— 原理、61 倍微基准、ARPL 与 DC D8 问题
44. https://github.com/Lifeisawful/rosettax87 — 4.7 倍，2026-01-02 归档
45. https://github.com/Lifeisawful/rosettax87_jit ；https://github.com/Lifeisawful/rosettax87_jit/issues/14 — macOS 27 RC 上的 `PT_THUPDATE` 错误
46. https://github.com/matasarei/wow-launcher/issues/31 — WoW 3.3.5a 的 x87 瓶颈，24–27 fps 到 117 fps
47. https://github.com/WoWSilicon/WoWSilicon/releases — Wine 11.13 + MTLd3D + x87sidecar，版本回退记录
48. https://github.com/victormlourenco/ROSilicon — WineAndAqua 运行时 + x87 hook 选择
49. https://github.com/Gcenx/winerosetta — 32 位 WoW 客户端的 Rosetta 指令缺口
50. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-27-release-notes.json — macOS 27：`game-test-tool`，Rosetta 相关条目，无 OpenGL
51. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26-release-notes.json — macOS 26：AGL 移除、“OpenGL still remains”、`nox86exec`
52. https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26_4-release-notes.json — 26.4：Rosetta 提醒，老游戏会继续支持
53. https://developer.apple.com/tutorials/data/documentation/xcode-release-notes/xcode-27-release-notes.json — Xcode 27：ld64 移除，x86_64 不再默认构建
54. https://blakecrosley.com/blog/macos-27-golden-gate-release — macOS 27 于 2026-09-14 发布（26A428）
55. https://www.macrumors.com/2026/09/24/macos-golden-gate-features-removed/ — 27 升级时移除 Rosetta，28 起只保留给老游戏
56. https://www.gamingonlinux.com/2026/04/a-future-wine-release-could-use-zink-to-run-opengl-via-vulkan/ — MR !10531（2026-04-02，半个愚人节玩笑）
57. https://api.github.com/repos/wine-mirror/wine/contents/libs — master 的 `libs/` 中没有 Mesa
58. https://docs.mesa3d.org/drivers/zink.html — Zink 各 GL 版本对 Vulkan 的要求
59. https://gist.github.com/lucamignatti/5312f5e937de2ba44256ecba6de54cc2 — Minecraft 跑在 Zink + KosmicKrisp 上
60. https://sourcegraph.com/search?q=repo:%5Egithub%5C.com/wine-mirror/wine%24+i386_set_ldt — `ldt_set_entry` 与 `i386_set_ldt`（signal_x86_64.c:2673）、`cs32_sel`（:2825）
61. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/signal_x86_64.c — `is_16bit`、`FPU_sig` 与 FULL mcontext、Rosetta 相关注释
62. https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/mach_loader.c — ARM64 的 4 GB hard pagezero、`vm_map_raise_min_offset`
63. https://github.com/dappermint/winecx/commit/9cc1f7435e88a3a9e683311a1b622938f1e7e170 — 在 arm64 上释放 pagezero，需要 cross-architecture-support entitlement
64. https://raw.githubusercontent.com/FEX-Emu/FEX/main/FEXCore/Source/Interface/Config/Config.json.in — `X87ReducedPrecision` 与各 TSO 选项
65. https://fex-emu.com/FEX-2604/ — x87 SIN/COS/TAN 内联，平均快 3.7 倍（2026-04-09）
66. https://fex-emu.com/FEX-2605/ — ATAN 等快 2–4 倍，“32-bit games can only run so fast”（2026-05-08，美国时间；GitHub release 为 2026-05-09 UTC，见 [78]）
67. https://appleinsider.com/articles/26/07/31/first-apple-silicon-native-crossover-build-in-testing-as-rosettas-end-nears — CX ARM64 Preview 的已知限制
68. https://appleinsider.com/articles/26/06/11/crossover-a-windows-to-mac-gaming-tool-goes-apple-silicon-only — CX 27 取消 32 位 bottle
69. https://github.com/otya128/winevdm — otvdm，16 位 Windows 程序的兜底方案
70. https://api.github.com/repos/metalsharp/WineMetalGL — GL→Metal 实验项目，创建于 2026-07-30
71. 内部报告：04（D3D8–11/GL）、05（KosmicKrisp）、06（Rosetta/ARM64）、07（地址空间）、15（Engine A 可行性）、17（Wine 基线与 msync）— 位于 docs/research/
72. https://raw.githubusercontent.com/dappermint/winecx/5fcf4c850ccfb1e1829b1b90b7935ed2198e58d9/dlls/opengl32/unix_wgl.c （CX 23.5.0，含 `mach_vm_remap`）；https://raw.githubusercontent.com/dappermint/winecx/3c875fb65750ab683d74d606b39a964855a6259f/dlls/opengl32/unix_wgl.c （CX 22.1.1，不含）— 事实核查补充
73. https://github.com/gauthierpiarrette/highball/issues/5 — 2026-09-05 更新：“Highball's own Wine 11 engine (CrossOver 26.3 LGPL base, build 15)”
74. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.18/ANNOUNCE.md — Wine 10.18 的第一条功能：“OpenGL memory mapping using Vulkan in WoW64 mode”
75. https://api.github.com/repos/wine-mirror/wine/git/tags/1f9cacdfd68c81f89b363510d2f82fe3a25501a5 — tag `wine-10.18`，日期 2025-10-31T21:47Z
76. https://api.github.com/repos/athei/mtld3d/releases ；https://raw.githubusercontent.com/athei/mtld3d/main/README.md — mtld3d 完整 release 时间线（v0.1.0 2026-07-14 → v0.11.0 2026-09-25），许可 Zlib；CI 基于 CrossOver 26，CX 27 arm64 Wine 仅手工测试
77. https://en.wikipedia.org/wiki/MacOS_Golden_Gate — macOS 27 于 2026-09-14 发布，初始构建号 26A428
78. https://github.com/FEX-Emu/FEX/releases/tag/FEX-2604 ；https://github.com/FEX-Emu/FEX/releases/tag/FEX-2605 — 发布时间分别为 2026-04-09T21:52Z 和 2026-05-09T03:02Z（UTC）
79. https://github.com/3Shain/dxmt/releases/tag/v0.50 — 2025-04-26 发布，“32-bit program support (WoW64 build) (#67)”
80. https://api.github.com/repos/Lifeisawful/rosettax87 — `archived=true`，`pushed_at=2026-01-02T22:59:04Z`

## 事实核查记录

独立核查于 2026-09-27 完成。凡是“部分属实”的条目，正文都已就地更正，并标注“核查更正”。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 上游 master（11.18）的 `unix_wgl.c` 没有 `__APPLE__` 和 `mach_vm_remap`；WoW64 映射的回退顺序为 lucky → Vulkan placed → AMD pinned → memcpy 影子缓冲加 FIXME（摘要、§1.1） | 部分属实 | 没有 `__APPLE__`、`mach_vm_remap`，确有这条 FIXME，VERSION 为 11.18，这几点都属实。**顺序有误**：代码实际依次判断 `vk_memory`（Vulkan placed）→ `pinned`（AMD）→ lucky（< 4 GB）→ `GL_MAP_PERSISTENT_BIT` 直接失败 → `buffer_vm_alloc` 低地址影子缓冲。只有在没有 `GL_MAP_INVALIDATE_*` 时才 memcpy，FIXME 有 `static int once` 保护，只打印一次。前两条只对 opengl32 自己包装分配的缓冲生效，而且 `initialize_vk_device` 需要 `GL_EXT_memory_object_fd`；vk 和 pinned 都不可用时，版本上限压到 4.3 并隐藏 `ARB_buffer_storage`（第 483 行）。macOS 上的结论不变：驱动指针在 4 GB 以上时走影子缓冲。受此影响，P0-1 的 CI 检测方式已改为自加逐次计数的 TRACE [1][2] |
| CX 26.3.0 源码（`crossover-26.3.0` 分支，2026-08-12 导入）以及 25.1.0、24.0.4 的导入中，`wow64_map_buffer` 在 lucky 之后有 `#ifdef __APPLE__` + `mach_vm_remap(VM_FLAGS_FIXED\|VM_FLAGS_OVERWRITE)` 分支 | 属实 | 26.3.0：第 2617、2623、2631、2643 行；提交 `40c09507`（2026-08-12T21:13:52Z，wine 11.0 base）。25.1.0 在第 2344 行，24.0.4 在第 2317 行。remap 失败时设置 `GL_OUT_OF_MEMORY` 并 goto unmap，不回退到拷贝路径 [3][4][5] |
| CX 用 `mach_vm_remap` 消除 GL 拷贝路径是“至少从 24.0.4 起”（§1.2、§1.4） | 部分属实 | 23.5.0 的导入（`5fcf4c8`）已经有这段代码，22.1.1（`3c875fb`）还没有。已改为“至少从 23.5.0 起” [72] |
| highball-db 中 HL2 Demo（M1 Pro / macOS 27.0，1280×720 timedemo）的数据：Sikarugir Wine 10 r7 上 wined3d 38 fps（GL、Vulkan 相同）、DXVK 17 fps；Wine 11（CX 26.3）树 131 fps；加 x87sidecar 174 fps | 属实 | 数字与 JSON 的 notes 一致，测于 2026-09-20/24。补充三点并已写入正文：(a) “CX 26.3 基底”出自 Highball issue #5，JSON 本身没写；(b) r5 引擎上为 33 fps、DXVK 14–17 fps，`verified` 块用的是 r5 数字；(c) 这些是单次测量，131 fps 的原因作者“not yet understood”。P0-2 已加入先复现这组数据的步骤 [18][73] |
| macOS 27 release notes：`sudo game-test-tool enable` 只在 beta 中开启 legacy Intel-based games 支持，开启后会禁用 Rosetta（166398727）；文档中没有 “OpenGL”；2026-09-14 发布，26A428 | 属实 | 补充同一文档中的相关条目，已写入 §2：“All Intel-based software will no longer be compatible with macOS 28.0, excluding legacy games”；升级后 Rosetta 不会自动恢复（163213094）；原来设为“Open using Rosetta”的应用改为原生启动（168097174）[50][77] |
| athei/mtld3d：zlib，Rust；tag 从 v0.4.1（2026-08-07）到 v0.11.0（2026-09-25）；SM1–3 与 FFP；v0.6.0 起同时附带 x86_64 和 aarch64 两份 `mtld3d.so`；用 `newBufferWithBytesNoCopy` 别名 PE 内存 | 部分属实 | 仓库创建于 2026-07-14，release 从 **v0.1.0（2026-07-14）** 到 v0.11.0 共 14 个，另有 test-issue-853 预发布，v0.4.1 只是中间版本。其余属实：staging 和 VB/IB 是 “PE-owned memory under a bytesNoCopy wrapper”。README：CI 用的 Wine 基于 CX 26，CX 27 的 arm64 Wine 只做过手工测试。§4 表格和风险 3 已更正 [37]–[40][76] |
| DXMT 1.0 计划（#151，2026-04-21）只涉及 D3D10/11；neo773/d9mt main 最后提交 2026-06-24；Sikarugir-App/d9mt `dx9` HEAD `69361df`（2026-03-28），`c16e245` 改动 71 个文件，+15,685 行 | 属实 | #151 仍为 open，里程碑中没有 D3D9。neo773 的 HEAD 为 `237e293`，`pushed_at` 是 2026-07-15，但没有更晚的提交，也没有 release。`c16e245` 为 +15,685/−186 [32][33][35] |
| Vulkan placed memory 路径是 Wine 10.18（2025-11-03）引入的，MR !9032（§1.1） | 部分属实 | 10.18 的 ANNOUNCE 确实列出这项功能，但发布日期是 **2025-10-31**（tag 日期），不是 11-03。相关提交是 Jacek Caban 2025-10-29 的 `a2906847` 等。MR !9032 因 GitLab Anubis 拦截**无法核实** [7][74][75] |
| MoltenVK 有 `external_memory_host` 和 `map_memory2`，没有 `map_memory_placed`（§3） | 属实 | MoltenVK main 的 UserGuide 扩展列表，2026-09-27 核查 [17] |
| rosettax87（Lifeisawful）于 2026-01-02 归档（§2） | 属实 | API 显示 `archived=true`，`pushed_at=2026-01-02T22:59:04Z`。API 不直接给出归档日期，但最后一次 push 在当天 [80] |
| DXMT 自 v0.50（2025-04-26）起支持 32 位 WoW64（§4、§8） | 属实 | release 发布于 2025-04-26T16:13Z，Features 中有 “32-bit program support (WoW64 build) (#67)” [79] |
| FEX-2604（2026-04-09）内联 SIN/COS/TAN，平均快 3.7 倍，点名 FNV 和 Bayonetta；FEX-2605（2026-05-08）优化 ATAN/FYL2X/FSCALE/F2XM1，快 2–4 倍（§7） | 属实 | FEX-2605 的 GitHub release 时间是 2026-05-09T03:02Z UTC，美国时区为 05-08，正文已注明 [65][66][78] |
