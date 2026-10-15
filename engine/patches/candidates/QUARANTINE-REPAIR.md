# 隔离候选修复队列（2026-10-15）

应用到原候选之前的同一CX26.3/Cider基线，顺序为：

1. `0009-token-kernel-object-mapping.patch`
2. `0012-reference-callers.patch`
3. `0013-object-reference-lifetime.patch`

`0010-ntoskrnl-primary-token-reference.patch`已从候选目录撤出，历史字节保留在`../rejected/0010-ntoskrnl-primary-token-reference.patch.txt`以及原隔离分支。不得把它加入本队列。重建后的primary接口回到基线未实现stub，没有替代身份/访问通道。

0012补真实失败状态、构造器预检与句柄清理、线程池提交失败回滚、回调异常清理。0013补稳定同权限句柄、map→ref锁序、逐层异常清理、FREE引用检查、释放结果不确定时的终止错误状态，以及满足Windows对象对齐的私有对象头。错误状态不会推断持有方或重试释放，可能保留资源到进程清理；不是已验证的恢复协议。

本队列没有加入任何默认配方或支持矩阵。源码修复/离线验证和原生Windows契约、Wine会话、真实游戏验收分别记载；构建成功不解除门禁。主令牌替换方案的工具调用被自动安全审查拒绝，替换实现未完成，撤出旧稿不等于实现完成。详见research62。
