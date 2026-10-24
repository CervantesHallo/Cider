# R v1：共享纹理、GPU 样本与窗口基础迁移

日期：2026-10-25。接续计划 02 的媒体及 CEF/窗口主题。所有运行均使用自有实验瓶，经私有签名 GUI/XPC 启动；不是用户游戏会话或默认引擎采用。

## 共享纹理的实际结果

普通 DXMT 0.80 在两个真实 D3D11 设备间共享纹理、完成 GPU 工作并读回 256 个 RGBA 像素成功；KEYEDMUTEX 纹理的接口查询返回 `E_NOINTERFACE`。保留该次失败，未降低要求。

上游 [PR 195](https://github.com/3Shain/dxmt/pull/195) 后续实现了 keyed mutex。项目原有 Highball7421db2 组件已经包含这一实现，因此下一步复用该固定组件，未重新实现接口，也没有更换 Wine 同步核心。

Highball 归档 SHA256 为 `0b86f0c793e99ae88bb4ff5f8575403ad138598a44a90248c71d6bc3ec18923b`。八个 PE 与一个 native 模块成组放入新私有候选。只对自有 native 副本签名，签名前后所有文件支撑的 Mach-O section 摘要一致。冻结核心、生产 App 与用户瓶子配置保持不变。

| 实际检查 | PE32 | PE64 | 边界 |
|---|---:|---:|---|
| 普通共享与 keyed 交接、256 像素全通道读回 | 39 项通过 | 39 项通过 | 无 HWND、swapchain 或视频 |
| 增加错误 key、持有期间竞争、未持有释放、递归获取和交回 | 46 项通过 | 46 项通过 | 超时必须精确返回 WAIT_TIMEOUT，不能用 SUCCEEDED 当获取成功 |

读取前用 GPU event 确认生产工作完成，因此该轮没有证明省去同步后的性能收益。legacy shared handle 不交给 CloseHandle。两项规则依据 [AcquireSync](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync)、[ReleaseSync](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-releasesync) 和 [GetSharedHandle](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiresource-getsharedhandle)。

第二版控制器曾重复安装已有私有引擎，安装拒绝后按 finally 清理，尚未运行探针；记录保留。第三版明确复用且核对已有安装，才执行新增边界检查。没有为相同输入挑选较好的性能轮次。

## GPU SourceReader：异常使整个批次仍未通过

旧有宽范围 D3D11 fixture 在 PE64 普通断言 146 项通过，但仍有 13 项已有 TODO，以及格式/缓冲区错误日志。这不是完整视频通过，原断言没有删除。

新增 `gpu-forward` 限定范围使用真实 D3D11 texture 和测试自有 MFT，四种组合为：

- keyed 请求与 keyed 样本、普通共享请求与普通共享样本：检查 sample、buffer、resource 的 IUnknown 身份均保留。
- 请求与实际 sharing flags 不匹配：检查原上传后备给出不同 sample/buffer/resource，返回纹理满足请求。

每条路径还检查非零时间、时长、sample flags、自有属性及返回纹理的全部 256 个 BGR 像素。RGB32 的 X 字节不作为 alpha 检查。测试侧 keyed 所有权不跨越 ReadSample 或 MF 的复制/锁定调用。

PE64 四条路径共 575 项普通检查通过，无 TODO、跳过或 native 异常。PE32 首次在第四条路径退出 5，未交付完成记录；精确原因尚未由该次无 trace 日志证明。

随后仅对失败的 PE32 增加 SEH 日志诊断，主线程虽报告 575 项通过、退出 0，却出现 Objective-C `NSInvalidArgumentException`：向 `0xbaadf00dbaadf00d` 设备对象发送管线创建消息。**本轮拒绝验收**，没有使用主线程计数掩盖异常。原控制器的 false-pass 原件保留，另有 acceptance-correction sidecar 明确否决。不得把该诊断轮写成 PE32 或整组通过。

源码与独立审查确认了构造顺序缺陷：`ArgumentEncodingContext::clear_uav_cmd` 的构造函数向外层 context 请求十个管线，而外层 `device_` 在更后面的成员声明位置，尚未构造。日志中每个新设备的十次异常与该路径相符；不能由此追认首次退出 5 的准确原因。

修复候选在 `engine/porting/dxmt-highball/0001-initialize-device-before-command-contexts.patch`：将成员声明及 initializer 都移到依赖者之前，保留十个真实管线创建。仅调整 initializer 文本顺序不足以修复 C++ 的成员构造顺序。内部 C++ 布局改变，必须成组重编译 PE 依赖闭包，不能换一个目标文件或借旧运行结果。构建及修后运行状态以本文件后续接续与证据为准，当前不提升媒体兼容评级。

## 窗口桥接方案与已实现基础

限定独立源码比较了 [DXMT PR 166](https://github.com/3Shain/dxmt/pull/166) 及其 [Wine MR 11058](https://gitlab.winehq.org/wine/wine/-/merge_requests/11058)。ExtEscape 的固定宽度传输可避免直接暴露 private window struct，但该提案不替代现用 Highball 和用户导入 D3DMetal 所需的完整旧接口，而且旧 MR 的 surface/Vulkan 所有权边界已与 Wine11.18 不同。不能照贴旧代码或只发布十槽短表。

采用的实施顺序是保留完整兼容前端，并基于当前 client_surface/CAContext 机制接入；ExtEscape 后续可共享同一生命周期核心。代价是仍需维护完整旧接口和 callback/位宽边界；收益是保留现用图形组件及导入能力。验收须包括真实创建/失败/释放、设备匹配、子窗口位置/尺寸、跨进程呈现和重建；回退仍为现用稳定引擎。尚未以此方案替换用户运行环境。

基础补丁 `0011-winemac-retain-renderer-device-and-check-surface-creation.patch` 已实现：

1. 新显式设备构造函数真正 retain renderer 设备；旧入口保留默认设备行为。
2. 已创建 swapchain 或缓存视图属于不同设备时拒绝新请求，保留原用户的视图和设备。
3. 在解引用前检查 client surface 和 Cocoa view 分配，失败时释放部分对象。

首轮独立审查发现缓存视图可能绕过设备匹配的 P2，已修复并限定复审闭合。两个完整 native 翻译单元及私有 winemac 模块链接成功；11 条 SDK 弃用/nonnull 告警均映射到原始未修改源码行，保留原日志，并非全模块零告警。四文件补丁 zero fuzz/offset 重建一致。

该基础没有发布 legacy table、没有解除 foreign-child 拒绝，也没有实际窗口运行证明；因此不能称 CEF/窗口主题完成。

## 构建取舍与接续

本机 Metal toolchain 不可用；本次只为 PE 依赖重建准备私有 GCC15.2 C++ runtime 和 Meson/Ninja，不修改系统安装或 Wine 核心工具链。重建使用同一 Highball 归档两种 PE 中逐字相同的 161070 字节 Metal library，SHA256 `0cb052fa20bae7ccd659b59a6c60718a4a22816980e89c299d1a5665d75a1813`；不改 shader 源、不重建 native 编译器，不能称整个 DXMT 从源码构建。

此取舍只适用于当前不变的 shader 与 Unix-call ABI。成本是仍依赖固定二进制 shader 输入；验收要核对两种归档的字节、完整 PE 模块组、wire 定义、真实像素及 native 日志。任何 shader/native 修改都须重新准备完整构建条件。回退保留原 Highball 组件，候选不进入默认配方。

首次独立 C++ runtime 配置缺少 GCC private `gthr-default.h`，误禁用了线程声明，DXMT 编译因此失败；没有修改 DXMT 来绕开线程接口。改为提供固定 GCC15.2 的真实 Win32 gthread header，并重新运行配置检测，实际检测结果为 yes；继续构建与验收。

磁盘紧张时只将早期已拒绝的 in-source GCC 构建目录压缩归档，逐一核对 147166 个文件及其他成员后移除原缓存目录；归档可恢复，固定工具链、Wine 输入、用户应用和数据不改。

资料在 `out/engine-dxmt-bridge-20261025`。下一步先完成设备初始化修复的成组构建与真实受影响运行，再继续窗口完整桥接；原 HY 顺序、同步 U8/15 未过、Windows 待重启条件及游戏门禁均保持。Windows、Actions、定时和 Goal 未操作。

## 持续接续：成组构建完成，另一个运行缺口仍待

设备顺序修复的独立源码复审没有新增具体 P1/P2；全部十个管线请求保留，检查的 wire 文件逐字未变。线程启用的 GCC15.2 目标标准库已在私有目录构建；随后两个 PE 架构的完整依赖闭包各四个模块均编译、链接并完成 Wine builtin 处理。每架构保留一条原源码中 unsupported callback 的非 void/no return 告警，未称零告警。早先缺 gthread header 和生成 version.h 路径的构建失败均保留，未通过删线程或图形功能完成构建。

新私有候选保留相同 native、Metal library、MF 模块和冻结 Wine 核心。首次修后 PE64 运行不再出现上述无效设备 Objective-C 异常，但在最后一种分享属性不匹配的路径发生执行访问异常、退出 5；整个新批次仍拒绝，未继续跑 PE32 或称修后通过。

随后加只观察、不吞异常的 vectored handler 和模块日志，同一组普通断言 575 项通过且未触发 handler。这是改变取证条件后没有复现，不是原缺陷已修复的证明，不覆盖先前访问异常。继续检查样本/资源生命周期和诊断；不盲目重复至通过。诊断 helper 首次生成误插入旧注释，被 Werror 拒绝；旧源保留，正确定位实际函数后才编译执行。每次仍确认 GUI/实验进程清理、普通瓶子配置/DLL 恢复和四保护摘要一致。

公开摘要与指纹见[证据](evidence/r-v1-gpu-window-foundation-20261025.json)。没有替换稳定引擎或将这批候选用于用户游戏。

补充取证：启用真实 heap 校验后，四条 GPU 路径先完成 575 项，再在 DLL detach 出现另一访问异常。观察 handler/实际 PE 符号将它定位到 DXGI 内静态 C++ runtime 的 `std::uncaught_exception` → 输出流 flush → `ios_base::Init` 析构；返回的 TLS 指针是已释放填充值。已用仅含 C++ DLL、LoadLibrary/FreeLibrary 和一条普通线程的最小程序在普通实验引擎独立复现，未调用图形或 MF，FreeLibrary 后记录 fault=1、CLI1。该结果把这次退出异常与 GPU 路径区分开，但不追认更早的执行 AV。

另一次限定生命周期源审发现条件性 P2：MF allocator 最终释放中，先释放最后的 device manager，再销毁仍缓存的共享纹理；Highball 纹理析构仍通过 raw parent 调用 `GetLocalD3DKMT`，可访问已销毁父设备。父代理已核对这两段真实源码和引用模型，尚未证明早前 AV 就发生在该调用。下一步分别准备 allocator 析构次序及 private C++ TLS 清理的最小修复与新旧对照；不通过 fixture 额外 pin、任意 sleep 或忽略析构错误掩盖生产路径。
