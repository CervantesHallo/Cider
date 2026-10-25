# Cocoa model 退役完成夹具

验证0022的实际Cocoa移除回调和producer最终完成之间的所有权。使用真实Win32根/带边框子窗口、真实CAContext/Metal layer和真实自身进程等待线程。它是**同进程的PE32/PE64分别验收**，不代表跨进程renderer路由、屏幕像素、compositor/GPU停止或完整context-ID ABA证明。

## 准备

在固定Wine11.18及0022前置补丁的私有源码副本应用`diagnostic.patch`，把本目录加入native和PE include路径。普通六个Unix调用顺序保持；私有48B owner probe是第6号，80B retirement fixture是第7号，两张调用表计数均8。`probe.h`与remote-transport原件相同。

成组构建native winemac.so与两种PE winemac.drv；导出用本目录`winemac-fixture.spec`，winebuild显式`-Fwinemac.drv`。client.c分别用MinGW GCC的`-std=gnu11 -O2 -Wall -Wextra -Werror`编译。通过一次性私有Cider GUI/XPC瓶运行，无参数，不直接派生Wine，不使用用户Steam瓶子。每轮使用新进程，高位身份种子只递增、不复位。启用warn+macdrv、err+all、trace+macdrv，保留实际模块/EXE/session摘要。

## 完整成功条件

每种PE均20检查、零失败、CLI0。实际返回context ID非零，lease/ticket至少`1<<33`；窗口拓扑、ABI80B和操作真实状态全部通过。

1. 实际创建CAContext、self-process租约和宿主。
2. 退役时，真实列表owner先交给诊断slot持有，暂未提交Cocoa移除；真实STOP使PE worker进入native完成。
3. CHECK12要求pending1、watched1、finalized0、实际model layer仍在、全局退役owner1；3秒截止时间前观察到completion_seen1。slot持有的是实际host/producer/root引用，不是计数模拟。
4. 显式提交原owner，实际执行Cocoa generation匹配的移除和回调。观察model/global/PE owner归零。
5. CLEANUP在native watch、全局退役或实际model仍在时保留CAContext；释放后若PE owner尚存，wrapper继续报告IO_PENDING。最后确认20项、完整清理及GUI实际退出、两次空观察、无pending，恢复普通实验配置/DLL。

`before.patch`只去掉finalizer的pending_retirements保护。同一EXE应20/1、仅CHECK12失败：model仍在且pending1时，实际watched0/finalized1。随后仍完成实际移除与清理。任意提前错误不算有效负例。

`failure.patch`单独叠加到修后诊断基底，首个Cocoa完成在实际移除前返回false；它不抛真实NSException。native保留owner/FIFO和pending1/watched1/finalized0，已有PE恢复timer重试实际移除。每种PE仍应20/0，恰好一次绑定同context/ticket的故障及owner快照，最终归零。不把故障与before分支混用后声称独立对照。

普通源码不含高位种子、held slot、诊断导出、错误分支或故障。无producer的局部host失败重试仍可能需要后续USER入口。立即RELEASE操作成功不是终端model移除回执；未来producer资源调用者必须消费终端状态。实际像素/资源服务器语义和多host/多process仍待，详见research102。
