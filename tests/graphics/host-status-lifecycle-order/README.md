# 当前阶段：取消先行与根窗口关闭

继续per-host安装／取消／清理契约，不前移多进程或呈现阶段。复用`../host-status-live`已复审的Native helper、纯Cocoa观察器和成组引擎字节，普通0027不改；这里只增加两个PE客户端。

- `cancel-first.c`：实际导出context及预留RO页，先取消，再由消费者尝试安装。真实RO SAFE之后，消费者在发送INSTALL之前拒绝，install_sent始终0；实际模型各0、context仍持有，最后正常释放。双PE各25/0。这个拒绝是消费者行为，不冒称服务器处理了安装请求。
- `root-close.c`：实际安装且模型各1后销毁Root，子窗口一同消失。保留RO页及context，观察自动撤销后的HostSAFE／producerSAFE和实际模型各0，再释放。双PE各22/0。客户端不再显式发送CANCEL或RELEASE；清理helper可能尝试已失效端点，完成仍只认实际保留页及模型证据。

MinGW GCC使用同组fixture.h和status_cell.h，-std=gnu11 -O2 -Wall -Wextra -Werror及user32/ntdll。每次新进程、无参数，经Cider GUI/XPC隔离瓶运行；不直接派生Wine或使用登录Steam副本。

见research113与证据。不是NT清理失败／取消并发、未知回复、跨进程、像素、性能或游戏证明；相关关键门槛保持，不能借这两个正常时序提高完整采用状态。
