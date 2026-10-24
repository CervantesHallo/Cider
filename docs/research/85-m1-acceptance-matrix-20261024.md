# M1 余项：按原 N/U/S 与应用门禁核对

依据：research17「测试与基准计划」原要求。83/84的结果不等于完整M1，也不覆盖全体Windows或游戏会话。当前继续可在Mac进行的部分，Windows主力机仍busy/不能重启、零连接。

| 原项目 | 当前实际证据 | 下一步/边界 |
|---|---|---|
| auto-event双线程信号到peer-return p50 | F27原AC固定三对5.0倍，CS控制+0.6135% | 已过该预声明门槛，不改名为所有对象/所有N组都过 |
| 单对象semaphore/mutex/manual ping延迟 | 有操作均摊辅助数据，尚无完整独立ping尾分布 | 不将均摊数据冒充唤醒p50/p99 |
| WaitAll许可、混合对象、8/64、APC、abandoned、命名/跨进程 | 两位宽当前短功能及自己的小时 | 保留原失败历史；未测项不借其它用例 |
| 随机3600秒/资源 | 自己的64547f41…/7b0cb109…78检查、758497967许可收支、peak36/fallback0 | 已过本轮，资源范围含启动/运行/退出，不泛化全进程泄漏证明 |
| 真实live登记终止回收 | 独立trace image双位宽各64轮0→1→0及missing-unwind负例 | 不是本体性能/hour或kernel-park/Windows对照 |
| U：CS/SRW/CV/WaitOnAddress/InitOnce | 双位宽、双模式正确性已过 | 全U基准±3%仍待，不能由正确性替代 |
| S：进程/线程、timer、IOCP、overlapped pipe及N混合 | 生命周期及新增双位宽、双模式正确性已过 | 不是吞吐/尾延迟证明；IOCP不直接塞入WaitForMultipleObjects |
| N：MsgWait、SignalObjectAndWait、Pulse、关闭中的句柄 | 新增双位宽、双模式18项契约已过；关闭等待中句柄不强造保证 | 超时仅观察；Pulse不宣称可靠交付；完整对象ping/尾分布仍待 |
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

## 新增 N 契约实际结果

`sync-nt-extra.c`在PE32/PE64各以msync/server运行一次，每次18检查零失败，匹配相同F27 native/PE产物。消息等待只接受队列返回索引后取到指定载荷；系统事件的无消息唤醒继续等待原绝对期限。SignalObjectAndWait核对真实往返信号、peer退出和自动事件消耗，不声称该API提供原子信号加等待。[消息等待](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)、[SignalObjectAndWait](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-signalobjectandwait)。

Pulse以已置位的manual event开场并验证前提，核对无waiter时最终复位。没有把不可靠的Pulse交付或等待中关闭句柄的未定义行为变成强制保证。[Pulse契约](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-pulseevent)。

审查修了两个可能误报通过的夹具P2（等待超时后仍读取排队消息、Pulse从未置位事件开场），以及一个会误报失败的解析缺口（遗漏timeout_observation类型）。两个限定API-result double实际分别被message_payload和final_reset_only拒绝；它们验证夹具，不冒充引擎破坏变体。

0／1／15.6／100ms的实际QPC观察全部保留。四次运行分别约0.0095–0.0113／1.2569–1.2704／17.6115–17.6708／100.3336–101.1458ms；仅核对STATUS_TIMEOUT及单调时钟，无新增精度门槛、统计等效或原生Windows结论。`out/sync-nt-extra-20261024`保留构建、源审、冻结runner、四原回执、负例与真实GUI退出/两次空观察。原5倍、CS±3%和自己的小时不重跑或改写。

## U组固定性能比较：运行正常，归因门槛未过

独立源码/回执复审后，以同一PE和F27 native pair做2/4/8线程、五种用户态原语、每种模式三次，共90次；每次四个固定warmup/measurement batch。主要指标是全部worker QPC区间包络/完成迭代数，不是单API裸延迟或唤醒百分位。CV含配套CS/谓词循环，WaitOnAddress按实际wait/wake调用计数。没有扣减开销、删样或重试；AC/LPM0、真实session环境与run ID、PE/native摘要均核对，运行和真实清理通过。

下表为msync相对server的成本差异，**粗体超出原±3%门槛**：

| 原语 | 2线程 | 4线程 | 8线程 |
|---|---:|---:|---:|
| CS争用 | **+4.16%** | +1.86% | **-11.58%** |
| SRW读写 | +0.94% | +2.26% | **+5.72%** |
| 条件变量 | +1.23% | -0.30% | **+25.04%** |
| WaitOnAddress | -1.51% | +1.28% | **-8.43%** |
| 已初始化InitOnce | **+16.73%** | **+45.51%** | **-4.86%** |

7/15单元在门槛内，8/15越界，故更广U归因不通过。源/运行检查通过不替代这一结论，也不能把双向差异自动说成msync回归或宿主噪声。原F27的5倍/私有CS控制/小时证据仍只覆盖原冻结范围，完整M1和默认采用继续未通过。

新夹具复审的两项P2（缺失final状态也可接受、超时/等待计数不一致）已修，最终复审无剩余具体P1/P2；原10个预检回执离线按更严校验仍过，19个回执破坏负例被拒绝。预检/正式各有冻结runner/validator，旧字节不改。原始区间、每worker计数、全部batch和实际清理在out/sync-user-bench-20261024。

有限源码检查显示两个tid-alert函数体无直接msync分支，不是间接调用、调度或整体归因证明。另核对Wine ThreadTimes发现macOS可能回退进程CPU时间，因此不能直接把GetThreadTimes数据当真实每线程CPU；后续计量必须先验证这一来源，不以假精度通过门槛。

## WaitAny与WaitAll的对象数量覆盖

sync-wait-widths.c覆盖WaitAny2/8/64和WaitAll2/8，manual/auto/semaphore混合，核对最低就绪索引、只消耗选中对象、manual保持、未就绪WaitAll不消耗、就绪WaitAll和不可重复消耗；另有真实peer信号和有界join，不宣称已kernel-park。WaitAll成功按文档范围判断，不强制索引0。[公开契约](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)。

四次实际执行（双位宽×双模式）各214检查0失败。首个PE32/msync的运行器按Python3.10的Counter零项相等规则编写，在本机Python3.9.6误报失败；修为显式移除零项，保留首个raw passed:false，以独立sidecar验证正确性和所有运行条件，只运行其余三项，未重跑首项或改写旧回执。初审没有识别该版本差异，后续实际诊断闭合，不能宣称初审全覆盖。三个删组/删检查/重复检查负例均被拒绝；真实GUI退出/两次空观察确认。证据在out/sync-wait-widths-20261024。
