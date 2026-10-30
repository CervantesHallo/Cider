# 请求仍在途时的原生完成决定

验证0023在普通finalizer中保留实际请求所有权。夹具创建真实Win32 root/child、自身进程租约和exported CAContext，调用普通packet_producer_begin取得一个INSTALL请求pin，暂不执行host_install。PE通过实际96B SendMessageTimeout消息提交更高序号的RELEASE，执行真实STOP及完成回调，再由普通packet_producer_end归还原pin。

这是PE32、PE64分别运行的同进程实验，不是跨进程renderer、实际Cocoa安装重入、像素、GPU停止或状态页接入证据。context确实创建，但尚未导入Cocoa宿主。

## 构建

在固定Wine11.18、0022前置组上应用0023，再应用本目录diagnostic.patch。把本目录加入native和PE include路径，重建remote_coordinator、macdrv_main、remote_watch_pe及相应链接单元；Cocoa源码不变，可以复用已核对对象。私有Unix槽6保留48B probe，槽7为request fixture，两表共8项。fixture.h为88B/version2，无native指针。probe.h保持原件，导出使用winemac-fixture.spec并显式-Fwinemac.drv。

client.c用各位宽MinGW GCC的-std=gnu11 -O2 -Wall -Wextra -Werror编译，链接user32。使用匹配native/两PE包，在一次性Cider GUI/XPC实验瓶无参数运行；不得直接派生Wine或使用用户Steam瓶。开启warn/trace+macdrv，绑定实际EXE、模块、session及真实GUI退出/两空/no-pending清理回执，结束恢复普通实验配置/DLL字节及原缺失状态。

## 精确判据

每种位宽完整20检查。修后20/0、CLI0；before.patch只移除finalizer的request_inflight条件，相同EXE应20/1、CLI1且仅CHECK12失败。

- 实际持有一个请求，其他cleanup/model计数为0、process handle已关闭、完成已请求。
- 私有hook在普通finalizer决定之后、释放remote_lock之前，首次记录该租约的上述时刻。它记录实际watched/finalized，不复制或改变普通请求归零判据；op2返回此记录，cleanup操作继续读取实时计数。
- 修后记录watched1/finalized0；负例记录watched0/finalized1。decision_detached可为0：负例可能先在两个请求时finalize，随后在一个请求时记录已完成状态。它不是PE收到终端ACK的证明。
- 原pin结束应得到真实ERROR_PROCESS_ABORTED，随后native/PE watches及timer实际归零，context释放、窗口销毁。负例也必须完成全部清理，不接受任意崩溃或提前错误。

held槽的发布、读取及向局部owner转交均在remote_lock下；begin使用局部输出，end在解锁后消费同一个引用。旧RIF-01观察器可能漏判并发时序，旧RIF-02失败清理存在槽位指针数据竞争，均保留历史报告；最终源码与实际结果见research105。
