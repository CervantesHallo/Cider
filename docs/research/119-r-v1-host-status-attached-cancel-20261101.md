# R v1：图层挂上后的取消与中止清理

日期：2026-11-01，UTC+8。接续[118](118-r-v1-host-status-timeout-20261101.md)，继续当前 per-host 阶段，普通 0027 实现不改。

## 真实图层与取消交错

已知 ticket／RO 页及真实导出 CAContext 准备好后，普通 INSTALL 真正导入并 addSublayer。Cocoa 回调记录 attached／ledger 各 1、indexed 为 0，以及外层真实 request/control/model_job 各 1；然后只用有界 Unix 条件变量等待。Wine 工作线程进入普通 packet_producer_begin／CANCEL／end，实际返回 IO_PENDING。

回调恢复前，外层三项在途 guard 仍各 1，retirement_pending=1、resource_done=0；原图层／登记仍各 1，真实只读页仍为有效非 SAFE snapshot。普通恢复线程可能正转交 retirement owner，故 queued 仅记录，不要求恒为 1。回调继续后，安装实际返回 INVALID_HANDLE；原 context 保留直到实际模型归零及 Host／producer SAFE，之后才释放。没有伪造 NT 结果、完成 guard 或模型计数。

## 两项审查缺陷及实际回归

独立审查先发现 REENTRY-P2-01：ABORT 先于工作线程进入 Native 时，未发布可处理的终止状态，实际 join 后的序号协调会永久待完成。修为只有工作线程**实际进入**被中止的 Native 入口后，才记录 started/done 和未接纳请求的取消分类；不伪造内核线程退出，调用方仍需真实 join。

新增门控回归又发现 REENTRY-P2-02：门等待超时／失败使线程根本没进入 Native，调用方还要求 Native done，阻止普通清理。修为实际 join 后按真实 entry_wait 区分：没有进入 Native 就跳过序号同步，既不伪造 Native 完成，也不消耗未使用的取消序号。另用真实零超时等待实际覆盖该路径；WAIT_FAILED 只审查同一清理分支，没有注入运行。

| 实际用例 | PE64 | PE32 |
|---|---|---|
| 图层已挂上，Wine 工作线程普通取消 | 32 检查、0 失败 | 32 检查、0 失败 |
| 先 ABORT，再真实进入 Native，实际 join 后清理 | 33 检查、0 失败 | 33 检查、0 失败 |
| 真实门等待超时，从未进入 Native，实际 join 后清理 | 33 检查、0 失败 | 33 检查、0 失败 |

六次 CLI 均 0，内部清理均真。两种门控场景均没有接纳普通 CANCEL 或发送 INSTALL，真实 producer RELEASE 使用原序号的下一项，未跳过预留但未消耗的取消序号。主路径则仅在实际 join 后协调测试 helper 已被普通 CANCEL 消耗的序号；产品计数和返回值不改。

## 组装、证据与线程边界

最初考虑从 Cocoa 挂层回调直接关闭 Root；父代理检查发现它会经 producer_revoke 做 NT 关闭及 PE 回调，在线程上下文上不成立，因此**构建前弃用，未运行**。最终 Cocoa 仅做模型观察、Unix 等待和已有 RO 映射的原子读取，实际取消／NT 工作由 Wine 线程执行。这是保持原线程模型的纠正。

两次类型编译错误及 v1/v2 未接纳审查原文保留，未放宽警告；最终 v7 源审无剩余 P1／P2。Native coordinator NDEBUG，256B 诊断帧／128B live 帧、两张 9 槽 Unix 表和 watch v2 匹配。13 组有效依赖闭包、131 项输入，复用对象／组件均逐字核对；Cocoa 的 10 条旧警告与既有源语句、消息一致。

独立解析 6 份原始日志／session／EXE，核对实际挂层、在途 guard、RO 状态、实际 join、两类中止账本及释放序号；另核对 16 PE／2 Native／F27、13 份完整进程清理、每批原瓶字节或缺失恢复和四保护摘要。五文件诊断精确重建。[公开夹具](../../tests/graphics/host-status-reentry/)复用 112 的相同 include，详见[证据](evidence/r-v1-host-status-attached-cancel-20261101.json)。运行后 6 棵临时整树、6 份前缀 EXE 差异归档清理；源码、原始结果、必要组件和普通工作基底保留。

公开客户端另仅清除行尾空白，实际构建输入不改；两 PE／三模式的六组预处理输出与原输入逐字一致，最终公开摘要及原输入映射保留。补丁的单空格上下文行属于语法，不做文本清理。

## 接续

这里覆盖了实际挂层期间的一个取消交错和两条中止清理路径，不等于全部重入、更新对象 pin、新旧代次替换、legacy 交接或 Retina。CANCEL 使用普通函数私有入口，不能称为 USER CANCEL 传输证明；没有旧错误版运行负对照。继续补当前阶段剩余对象更新／legacy 交接／显示模式，再按原顺序推进多进程资源消费者、事件 reparent 与两三进程像素。默认引擎、游戏评级和门禁不变，Windows 无操作。
