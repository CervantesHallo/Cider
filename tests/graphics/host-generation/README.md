# Cocoa 旧／新代次重入排序

接 0027 普通源，复用 [host-status-live](../host-status-live/) 的 fixture.h/native.inc/pe.inc/cocoa.inc/probe.h。先 include 本目录再 live，应用五文件 diagnostic.patch，完整 spec／`-Fwinemac.drv`；两张 Unix 表各 9 槽、watch v2，live 帧 128B、新帧 256B/v1，Native coordinator NDEBUG。

真实 context／窗口创建后，调用方分配两个高位 Cocoa generation 标签 A/B，**不是 Native ticket**；两者 commit=NULL、均为 unmanaged。外层普通创建 A 在真实 addSublayer 后，同一 Cocoa 回调直接进入普通 addCALayerHostView 方法创建 B。避免再次调用没有 main-thread fast path 的 OnMainThread，也没有在 Cocoa 上调用 NT／PE。

回调内先看见 A 实际图层／登记各 1、未选中，再看见 A 仍部分存在且 B 三项各 1。A 恢复后必须返回 false 并回滚，最终 A 三项 0、B 三项 1；真实 outer/inner bool 与诊断入口完成分开。实际 ProducerSAFE 后 B 仍存在、context 仍持有。

调用方在任何效果前保守记录两个独立退役责任。CLEANUP 必须实际 checked retirement 完成两项、独立观察两者实际消失并解除 Cocoa 窗口借用，才进入原始 context／读方／窗口释放；没有额外 context 引用挽救错误释放，也不把 outer=false 当作没有效果。

wrong-publication.patch 是隔离诊断反例，只去掉两处回调恢复后的 generation/serial 发布检查，不能用于产品。同一客户端应在 CHECK12 拒绝它，并记录实际旧 A 被选中、新 B 被移除；随后仍清理两个真实责任。补丁均精确重建，见 research/122 的实际结果及范围。

这里只验证一个 unmanaged Cocoa 排序／责任场景，不是 Native prepared ticket 并发、任意替换／busy交接、屏幕像素、GPU、性能、跨进程、游戏或默认采用。
