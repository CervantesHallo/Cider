# 图层已挂上时取消与中止清理

接 0027 普通实现，复用 [host-status-live](../host-status-live/) 的 fixture.h、native.inc、pe.inc、cocoa.inc、probe.h。include 顺序先本目录后 live；应用五文件 diagnostic.patch，使用完整 spec 及显式 `-Fwinemac.drv`。两张 Unix 表各 9 槽，watch v2，live 帧 128B、新帧 256B/v1；Native coordinator NDEBUG。

`client.c` 创建真实 CAContext、已知 ticket 与 RO 页。工作线程先等待真实 Cocoa 安装走到 addSublayer 之后。Cocoa 回调观察实际 attached／ledger=1、indexed=0 及普通在途 guard，然后仅用有界 Unix 条件变量等待；Wine 工作线程执行普通 packet_producer_begin／CANCEL／end。它不经 USER CANCEL 发送器。Cocoa 不执行 NT 清理或 PE 回调，RO 读取只是已有映射的原子加载。

实际取消返回 IO_PENDING，外层 request/control/model_job 仍在、retirement_pending=1。图层／登记仍各 1，RO 非 SAFE。回调恢复后普通安装返回 INVALID_HANDLE；实际模型归零及 Host／producer SAFE 后才销毁 context。独立生命周期引用不参与完成 guard。retirement_queued 只记录，不要求恒为 1，因普通恢复线程可能正在转交该 owner。

另有两个 `abort-client.c` 构建模式：

- 默认模式用真实未发信号事件挡住工作线程，先完成 ABORT，再发事件让线程进入 Native；实际 Native 入口记录已中止且没有普通请求被接纳。实际 join 后允许正常清理，不消耗预留的取消序号。
- 加 `-DCIDER_ENTRY_TIMEOUT_CASE=1` 使用真实零超时等待，保持事件未发信号直到线程退出；Native started/done 均为 0，不调用序号同步、不伪造 Native 完成，直接继续普通释放和实际清理。summary 的 gate_timeout_case 区分两种模式；WAIT_FAILED 仅做源码分支审查，未注入运行。

主线程仅在实际 join 后协调测试 helper 的序号；只有普通 CANCEL 确实接纳过，才跳过其已经消耗的序号，不改变产品计数或返回值。提前中止及门等待错误两条 P2 清理缺口均经审查修复；最终审查无剩余 P1／P2。

实际双 PE：挂图层取消各 32／0、进入 Native 后中止各 33／0、未进入 Native 的门超时各 33／0，CLI 均 0，内部清理及外层清理／恢复另行绑定。见 research/119。此前从 Cocoa 直接关闭 Root 的初稿因其内部会做 NT／PE 清理而在构建前弃用，未运行。两个类型编译错误及未接纳的审查记录保留，未放宽检查。

公开的两个客户端仅清理了行尾空白；实际构建输入保持原样，六组 PE／模式的预处理输出逐字一致，最终公开摘要和映射另有回执。diagnostic.patch 中单空格上下文行是补丁语法，保持原样。

这些是限定时序，不代表全部重入、旧／新代次替换、更新／Retina、legacy 交接、USER CANCEL 传输、跨进程、像素、性能、游戏或默认采用。
