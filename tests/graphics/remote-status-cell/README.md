# 完成页的跨位宽与生命周期夹具

仅验证未接入的`experiments/render-status-cell`组件。使用已匹配的remote-retirement私有8入口驱动，实际创建Win32根/child、CAContext及自身进程租约；另起相反位宽的读方。**它不是跨进程renderer呈现，完成页尚由夹具发布。**

## 构建与运行

`build.py --wine-source <已应用前置补丁的私有Wine源码> --cc32 <i686 GCC> --cc64 <x86_64 GCC> --out <新目录>`。脚本只编译PE，不下载工具、不启动Wine、不编译/替换驱动。native组件需使用对应Wine构建的已记录编译参数单独检查。可选`--before-wire`使用独立目录中的原始status_wire.h构建精确alias反例。

在独立Cider GUI/XPC实验瓶中，从PE64运行`x86_64-status-cell.exe C:\CiderTests\i386-status-cell.exe`；反向使用PE32父进程和PE64参数。二进制需先复制到对应路径；不要直接派生Wine，不用用户Steam瓶。每轮实际退出并取得GUI清理回执后，再开始下一轮，结束恢复实验瓶原配置/DLL字节和原缺失状态。

当前完整成功路径为owner52检查、reader35检查、零失败、两个进程实际退出0、cleanup1，根HWND在最终读前真实销毁。共享控制块256B，完成页160B；高位epoch/lease/ticket/序号不能截断。动态序号为明确的测试载荷，不冒称驱动实际请求计量。

必须看到RO view的PAGE_READONLY/MEM_COMMIT；用读方handle请求RW映射返回ACCESS_DENIED。真实PROTECT_FROM_CLOSE使close返回HANDLE_NOT_CLOSABLE：一条路径先成功unmap再失败close，保留owner/handle供重试；另一条保留映射，清保护并关闭handle后仍能读取。父进程在实际Cocoa model完成和资源释放之后关闭writer/销毁窗口，子进程通过仍持有的view读到SAFE_ONLY且values_valid0，然后实际unmap。done事件后父方还必须等待子进程退出，才接纳其最终计数。

`alias.c`两项只核对SC3-01：expected与out->identity同址、wire属于另一代身份。原header应分别错误返回SNAPSHOT0/SAFE_ONLY5，精确2失败/退出1；修后均MISMATCH3、零失败/退出0。不得把任意崩溃或提前失败当有效负例。

本夹具不证明native驱动发布接入、取消/迟到安装、连续并发快照、compositor/GPU停止、完整context-ID ABA、安全隔离、原生Windows或游戏。source/build、实际运行与这些缺口分别记录，详见research103。

## 连续发布

`stress.c`在同一NT组件上启动相反位宽读方，每轮调用真实publisher一百万次。当前session/根及读方PID/birth为实际查询；其他高位身份和动态计数是明确的诊断载荷，不借给coordinator所有权。每次更新applied的高低word、quiescent及error/三个计数；读方由高word反推出n，逐字段核对相关性、`sequence==2+2*n`和单调性。RO view关闭section handle后仍继续读取。

成功需父18/0、读10/0、最终n=1000000、两个进程退出0及实际清理；必须观察到真实RETRY和writer_running区间的中间快照。不能伪造奇序号或因调度不足降低门槛。区间包含yield/中点暂停，不宣称每次快照都与某条store指令同时，也不是读遍全部版本。动态state保持ACTIVE，终端和耗尽由103的其他检查覆盖；重试数量不作为性能基准。实际结果和中断回执恢复见research104。
