# 隔离候选修复队列（2026-10-17）

应用到原候选之前的同一CX26.3/Cider基线，顺序为：

1. `0009-token-kernel-object-mapping.patch`
2. `0012-reference-callers.patch`
3. `0013-object-reference-lifetime.patch`
4. `0015-close-borrowed-object-cleanup.patch`
5. `0016-ioctl-allocation-buffer-ownership.patch`
6. `0017-primary-token-release.patch`
7. `0018-process-primary-reference-ownership.patch`
8. `0019-owned-primary-token-reference.patch`
9. `0020-primary-token-ddk-declarations.patch`

`0010-ntoskrnl-primary-token-reference.patch`已从候选目录撤出，历史字节保留在`../rejected/0010-ntoskrnl-primary-token-reference.patch.txt`以及原隔离分支。不得把它加入本队列。退休0010的访问句柄路线不进入新实现。0018/0019改用现有Wine kernel-object归属与真实引用；不创建令牌访问句柄、不接受PID/访问掩码、不替换当前宿主身份。

0012补真实失败状态、构造器预检与句柄清理、线程池提交失败回滚、回调异常清理。0013补稳定同权限句柄、map→ref锁序、逐层异常清理、FREE引用检查、释放结果不确定时的终止错误状态，以及满足Windows对象对齐的私有对象头。错误状态不会推断持有方或重试释放，可能保留资源到进程清理；不是已验证的恢复协议。

0015修正CLOSE请求分配IRP失败时对借用FILE_OBJECT的未配对释放，保留真实STATUS_NO_MEMORY和0013零引用检查。实际服务端请求持有文件，无需新增本地引用。

0016修正IOCTL构造IRP失败时的缓冲区所有权：context仍持有的输入/别名输出留给主循环清理，仅释放独立输出和被替换旧输入。避免重复释放及旧输入泄漏，失败状态保持。

0017仅实现PsDereferencePrimaryToken及stdcall导出，委托已修的ObDereferenceObject归还引用。0017单独应用时取得接口仍是stub；完整候选队列由0018/0019补取得接口，没有恢复0010。

0018/0019保留目标process与其真实primary token，对manager做地址空间归属检查，并核对进程句柄与既有kernel映射指向同一个server对象。客户端map→ref锁内接纳canonical proxy，失败只回滚本次新持有的backend引用，关闭异常保留实际状态。新增请求将协议1809升至1810，需要新server、ntdll Unix、两架构ntdll PE及ntoskrnl PE一起使用；混用版本不能采用。0020补公共DDK声明。

本队列没有加入任何默认配方或支持矩阵。源码修复/离线验证和原生Windows契约、Wine会话、真实游戏验收分别记载；构建成功不解除门禁。某个早期访问替换提案的工具调用被自动安全审查拒绝是历史事实，不能据此断言所有忠实API实现被禁。当前引用方案的实际工具调用、源码/模型/构建/Wine结果见research71；原生Windows契约仍待。
