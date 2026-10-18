# Engine R v1：WaitAll 原子获取与异常退出协调

2026-10-18，接续75。用户明确 Windows 暂不能重启，先修 WaitAll，保留原实现能力。本文是新候选0102的源码/构建记录，75的旧暂取回滚缺口保留为历史。Windows 保持 busy，未连接、操作或扣新机时。

## 修复范围

原反例为 WaitAll 暂取信号量后，并发 Release 成功；WaitAll 获取另一个对象失败，再回滚信号量，计数超过 max。0102删除暂取/回滚流程：统一状态视图下先检查全部对象，再发布一次提交决定、应用所有状态。不足时零扣减。没有钳位计数、丢弃许可、禁用 WaitAll 或把调用转到不共享状态的 server 后端。

原16字节对象槽、共享页、Mach登记/注销、ulock睡眠、inproc句柄缓存和权限检查继续使用。WaitAny、WaitAll、信号量 Release/Query、事件操作、mutex获取/释放/查询、server信号/abandon，以及槽复用进入同一个状态协调门。准备共享映射、等待对象、执行APC和阻塞消息发送都在门外；正常通知只允许门内零超时的内核唤醒/Mach发送。

### 提交和恢复

协调文件头保存 `CLEAN → PREPARING → COMMITTED → APPLIED → CLEAN`、序号及最多65个 `{index, after, notify}` 条目。PREPARING前后尚未提交的中断不改变槽；COMMITTED中断由下一参与者在排他门内完成同一份after状态；APPLIED只补通知、不重写状态。查询在恢复完成前不能读取部分提交，allocator也先恢复再复用零引用槽。

恢复者缺少共享页时释放门，再走原Mach映射；重新进入后读取当前日志，不能把旧快照覆盖后续事务。待通知记录在队列满时保留，释放门后阻塞发送，重新进入须核对序号。重复通知只提供就绪提示；不会重复消费许可。消息泵保持不进入协调门，避免server向自身发送通知时形成环路。

协调门使用 Darwin 的 OFD 字节锁：每个原生线程独立 `open()`，不以 `dup()`/传输同一描述符充当独立锁。传给client的identity FD不持事务锁。byte0为实例存活租约，byte1为事务；旧实例文件仅在租约可独占且文件类型、owner、mode、magic和size匹配时清理。这里只处理仍存活系统中的执行者退出，不声称磁盘持久性、断电恢复或server失联已经通过。

### 线程退出与 fork

init_first_thread/init_thread回复携带当前线程的64位代际；协议由963重新生成964。获取状态门后必须匹配仍有效的代际。server恢复旧提交后撤销代际，再处理mutex abandonment；旧线程不能在扫描之后重新写入，TID复用也不能恢复旧权限。提前请求终止的改动限于msync，原非msync顺序保留，thread同步对象仍在abandon之后发信号。

持门期间屏蔽Wine的中断/暂停信号及SIGABRT，关闭pthread取消；释放后恢复原状态。原生私有事务出现异常时直接 `_exit(1)`，避免Wine异常/调试回调进入server RPC；致命路径也不先写stderr，因为满管道能阻塞退出。该标志附加在Unix thread_data尾部，不移动已有成员；无TEB的初始化分支同样检查。正常guest异常处理未改。

客户端显式退出和pthread析构都归还自己的工作FD。fork准备先阻止本进程进入事务，再保护FD清单；parent在开放准入前为全部工作FD重新独立open，child关闭继承的旧FD。因此即使child尚未运行清理回调，也不能持有parent以后新取得的锁。门内获取/恢复FD时重新读取记录，避免fork后使用缓存的旧整数。不能仅凭CLOEXEC、child最终会清理或提前发SIGQUIT就宣称所有权已回收。

原首次共享页映射不再持有os_unfair_lock等待RPC；并发映射通过CAS发布，归还落败映射。回复缓冲区改为调用栈局部，避免同线程重入覆盖。递归计数沿用上游ntsync的u32饱和就绪规则；client和消息泵的提示检查一致，释放饱和计数也通知。mutex的PreviousCount按当前上游转换为`1-oldCount`，输出结构先初始化并在门外复制。

## 对抗性审查

一位独立只读审查者负责server写入/生命周期清单和父代理新实现复审；父代理核对并实际修复，未把审查报告当运行验证。审查中发现的中断映射锁、SIGABRT/RPC、fork继承未来OFD锁、饱和计数提示、无TEB分支和阻塞stderr等问题均纳入修正。retired后公共入口先返回真实终止状态，内部不再递归退出并遗留本地锁。最终限定源码读回未发现剩余P1/P2，父代理核对五个关键文件完整摘要一致；这不是无缺陷证明。最终结论与摘要见[结构化记录](evidence/engine-r-v1-waitall-20261018.json)。

## 构建与证据边界

候选顺序为四项已有迁移 → 0100 → 0101 → 0102。0100/0101原字节保留，必须叠加0102才包含本次修复。协议和生成的请求检查/trace来自当前上游工具；15个变更文件及完整候选链20文件均zero-fuzz/offset精确重建，再次生成协议字节不变。摘要及组件构建记录见同一结构化文件。

使用macOS15.4 SDK、部署14.0及既有GCC C编译器，仅构建server/native ntdll/i386与x86_64 PE ntdll。最终构建退出0，没有新增编译警告。首次构建发现epoch误接入init_process_done，已接回init_thread；初次失败日志保留。没有新增或运行测试套件、并发夹具、Windows参考、产品GUI或游戏。本次不把静态交错分析和编译成功标成并发实测通过，也不把源码修复标成M1/M3完整验收。

## 工程取舍、验收和回退

收益是所有状态变更有共同顺序，取消多对象等待的有损回滚，并显式处理线程中断提交。成本是每次正常状态操作的一次排他获取/释放（2次fcntl）、进程内短准入锁、额外共享日志和线程代际表；队列满/恢复有更多进入和映射成本。独立对象的状态更新也会短暂串行，这是需性能验收的成本，不宣称保持旧热路径吞吐量。

文件/映射逻辑大小为128MiB线程表+64KiB头，稀疏创建、按需触页；不是已测RSS，也不保证任意文件系统支持OFD锁。另有75记录的固定指针表2/8MiB每原生进程。冷启动、磁盘/RSS、多进程扩展、平均/尾部等待延迟均未测。

采用状态：**已实施、已编译的非默认候选**，不是产品默认性能方案。后续门槛仍是新WaitAll交错的Windows/Wine对照、混合对象/零超时/APC/abandon与TID复用、提交各阶段线程退出、队列满/失联/清理，以及真实Steam/CEF/游戏负载和原性能门槛。PulseEvent的旧近似、Mach RPC期限、消息协议完整校验和完整引擎整合仍独立待验。

若性能不达标，继续在同一语义约束下优化准入和状态存储；若正确性不通过，0102保留为未采用候选并修复。回退通过不选中候选完成，既有用户引擎/瓶子未切换，游戏Verdict/门禁不提升；不以砍掉Windows等待能力换取通过。原HY1→HY2→HY3→HY4/HY5顺序和柚子社全系/三款本地目标保持。

## 原始依据

- [微软WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)：WaitAll就绪前不修改对象状态，WaitAny按输入顺序选择。
- [微软ReleaseSemaphore](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-releasesemaphore)：不可越过创建时的最大计数。
- [Apple XNU kern_descrip.c](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_descrip.c)：OFD按fileglob归属、最后close时解锁；本机15.4 SDK头实际提供90/91命令。该源码依据不代替本机运行证据。
- [Linux上游ntsync](https://github.com/torvalds/linux/blob/master/drivers/misc/ntsync.c)：多对象整体仲裁及u32 mutex饱和就绪规则；只参考结构/语义，没有复制为macOS内核驱动。
