# 实际对象使用、局部 Retina 与移除重入

接 0027 普通实现，复用 [host-status-live](../host-status-live/) 的 fixture.h/native.inc/pe.inc/cocoa.inc/probe.h。先 include 本目录再 live，应用五文件 diagnostic.patch，完整 spec／`-Fwinemac.drv`；两张 Unix 表各 9 槽、watch v2，live 帧 128B、新帧 256B/v1，Native coordinator NDEBUG。

真实 context／known ticket／RO 及实际模型建立后，通过普通 Cocoa updateCALayerHost 更新固定像素 frame=(12,18,160,80)、clip=(20,22,100,50)。setSourceFrame 中的纯观察 hook 读取实际 users=1；返回后 users=0、模型仍各 1。局部 WineContentView Retina 翻转、恢复也走普通方法，先用普通更新设定输入几何，准备期间观察器未激活；实际 Retina snapshot pin 在 setter 内被观察。图层及源图层的真实 contentsScale、container/source frame 用原始 double 位记录。

最后一次更新中，hook 调用普通无 generation 参数的移除方法，真实标记 retire_requested。没有额外 retain 或伪造 users；移除请求后实际图层／ledger／selection 仍各 1，setter 可继续安全使用。最后一次 releaseRemoteHostUse 才实际移除；随后 context／RO 仍持有且非 SAFE，再经真实 USER CANCEL、Host／producer SAFE 才释放。

同步借用的 frame/window 仅在 OnMainThread 调用内使用，active 和 frame 在 finally 清空。缺失对象的 users_after 为 UINT32_MAX，不能解释为真实零计数。新 use_result 是私有诊断结果，与原始 NTSTATUS／USER 结果分开。

独立审查无 P1／P2。实际双 PE 各 37 项／零失败／CLI0；组件、原始日志／session／EXE、5 份完整清理、原字节恢复和四保护摘要另行绑定，见 research/121。旧 Windows 常量在 Cocoa TU 不可见的首次编译失败保留，改私有结果枚举后未放宽编译。

仅证明普通 Cocoa 方法和该局部 view 的模式转换，不是 Native 异步更新任务、任意替换、全局 DPI／屏幕、像素、GPU、性能、跨进程、游戏或默认采用。
