# M1 余项：按原 N/U/S 与应用门禁核对

依据：research17「测试与基准计划」原要求。83/84的结果不等于完整M1，也不覆盖全体Windows或游戏会话。当前继续可在Mac进行的部分，Windows主力机仍busy/不能重启、零连接。

| 原项目 | 当前实际证据 | 下一步/边界 |
|---|---|---|
| auto-event双线程信号到peer-return p50 | F27原AC固定三对5.0倍，CS控制+0.6135% | 已过该预声明门槛，不改名为所有对象/所有N组都过 |
| 单对象semaphore/mutex/manual ping延迟 | 有操作均摊辅助数据，尚无完整独立ping尾分布 | 不将均摊数据冒充唤醒p50/p99 |
| WaitAll许可、混合对象、8/64、APC、abandoned、命名/跨进程 | 两位宽当前短功能及自己的小时 | 保留原失败历史；未测项不借其它用例 |
| 随机3600秒/资源 | 自己的64547f41…/7b0cb109…78检查、758497967许可收支、peak36/fallback0 | 已过本轮，资源范围含启动/运行/退出，不泛化全进程泄漏证明 |
| 真实live登记终止回收 | 独立trace image双位宽各64轮0→1→0及missing-unwind负例 | 不是本体性能/hour或kernel-park/Windows对照 |
| U：CS/SRW/CV/WaitOnAddress/InitOnce | 当前正式控制仅private uncontended CS | 补真实正确性；全U基准±3%仍待，不能由正确性替代 |
| S：进程/线程、timer、IOCP、overlapped pipe及N混合 | 生命周期已有进程/线程证据；其它未覆盖 | 增加有界正确性，IOCP不直接塞入WaitForMultipleObjects |
| N：MsgWait、SignalObjectAndWait、Pulse、关闭中的句柄 | 原完整矩阵未闭合 | 按公开契约区分确定语义与未定义/不可靠项，不强造返回值 |
| 应用级/CEF20次、5GB下载、游戏帧时间等 | 尚未在完整新R v1闭合 | 不换用户引擎或借v0图形/媒体游戏结果；依赖其它主题迁移与真实交互 |

## 新的有界 U/S 正确性实现

`sync-matrix.c`分别使用真实CS竞争、SRW读写、带谓词循环的condition-variable生产消费、WaitOnAddress/WakeByAddressAll和InitOnce。每个自有线程在释放栈/锁之前join，超时不能返回越过仍活跃的存储。WaitOnAddress与CV允许spurious/stolen唤醒，必须复查谓词；不因ready计数宣称已进入内核阻塞。[条件变量契约](https://learn.microsoft.com/en-us/windows/win32/sync/condition-variables)、[WaitOnAddress契约](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress)。

InitOnce返回的context值低保留位为0，并核对真实回调只执行一次，不存放伪代码指针。[微软契约](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initonceexecuteonce)。

S组独立源以同一个check回调汇总，pending IO与OVERLAPPED backing storage必须先安全排空，不能在活跃IO仍指向栈时返回。全套初次只支持correctness，不把它记作新的性能比较。编译、Wine实际运行、server对照、归因基准和应用级状态分别记录。

## 本轮实际 U/S 正确性结果

两位宽分别在msync/server各运行一次，四次均114检查0失败，检查清单与结果完全相同。均使用原64547f41…/7b0cb109…及冻结双PE；未借trace产物或重新跑已过小时/基准。

审查发现并修复新夹具两项P2：缺失检查行也可被不完整metadata接受，以及SRW只检查递增计数不足以证明shared/exclusive互斥。现有完整source-defined inventory、总数/失败计数/类型核对、立即/异步write两条合法分支；额外收紧后对原四份raw结果离线验证，原字节/原passed不改，不冒充新运行。源checker漏掉任意清理项亦被拒绝。

SRW加入worker实际持shared/exclusive期间的opposite TryAcquire与最终uncontended检查。独立省略shared acquire/release的负例实际触发U.SRW.shared_blocks_exclusive失败，其余安全清理完成，不当正常案例通过。

S复审纠正即时WriteFile完成使用HasOverlappedIoCompleted的契约越界；仅此前IO_PENDING才用该宏。写入字节统一来自GetOverlappedResult，管道改为message-read保证单payload完整性，不强加byte-stream原子读取。取消不等于完成，不能排空则终止自有进程而不越过活跃栈I/O。[WriteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile)、[消息模式](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes)、[完成宏约束](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-hasoverlappediocompleted)。

这闭合本轮U/S正确性子集；U组±3%性能归因、N其它ping/尾分布、其余API和应用级门禁仍待，不声明完整M1。
