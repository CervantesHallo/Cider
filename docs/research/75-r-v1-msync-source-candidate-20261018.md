# Engine R v1：macOS同步候选移植与剩余原子性缺口

2026-10-18，接续74。Windows用户暂不能重启，因此保持busy，准备2309秒/测试3600秒/无pending；本轮仅推进原02中可独立进行的Mac引擎工作。

## 实际实现

在独立源码副本中保留CX的Mach/ulock同步后端，接上Wine11.18的inproc cache、对象所有权和请求检查。新基线已改为指定成员object_ops及start_main_thread(data)初始化；没有复制旧位置表或CX协议1810。当前请求定义由新上游工具生成，协议962→963，重复生成保持字节不变。16个变化文件通过零fuzz/offset补丁重建，候选源码字节匹配。

已修源路径包括：关键Mach调用单次求值并在启动失败时退出；分配/NULL析构和真实映射错误回传；固定不搬迁页表及原子发布；共享引用溢出拒绝；线程退出不把借用的alert共享索引当Unix fd关闭。客户端自己创建的reply receive right与send uref分别归还，未使用SDK已弃用的整端口销毁。

独立复审进一步指出的旧WaitAll路径已修：abandoned owner能进入获取；失败只回滚本轮新取得的owner，保留原持有的递归状态；重试检查期限/APC且允许零超时首次获取；登记成功后的提前退出完整注销含alert，登记发送失败归还本地计数并传播失败。server分配起点只读一次原子快照，部分登记注销的i>1改i>0，abandoned状态在count归零后发布。

保留旧事件调度提示，未把它说成每次SetEvent必唤醒一次的Windows保证。Cider0006的same-device/inode硬链接检查迁到新loader，分配失败检查和旧路径释放已补；新基线不存在CX临时per-exe预加载链接，未复制死代码。没有改变Wine版本身份、反作弊或游戏门禁。

## 已有构建证据

server、原生ntdll及i386/x86_64 PE ntdll均链接通过，nm确认真实msync后端符号已进入原生模块。最终修改的增量构建无新增源码警告；首次SDK弃用和声明位置警告修正、原始日志保留。使用macOS15.4 SDK、部署14.0及原C编译器；完整引擎未构建/运行，新模块未安装进用户引擎，没有新增或运行测试套件。

公开候选在engine/porting/wine-11.18/candidates，顺序0100→0101，均依赖此前四项源码移植；[结构化证据](evidence/engine-r-v1-msync-candidate-20261018.json)记录摘要、构建和未完成项。源码候选不计为默认可用配方、原生Windows契约、Dock/TCC或游戏会话通过。

## 仍未修复：WaitAll暂取/回滚与并发释放

这是独立静态审查给出的源码交错，尚未执行运行复现：

1. 信号量S的count=max=1，自动事件E=1。
2. T执行WaitAll(S,E)，暂取S使count变0。
3. U成功ReleaseSemaphore(S,1)使count回1，随后消费E。
4. T取得E失败并回滚S，count变2，超过max1。

微软文档明确要求WaitAll在条件满足前不修改所等对象，ReleaseSemaphore不得令计数越过创建时的最大值。[WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)、[ReleaseSemaphore](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-releasesemaphore)。这支持正确性约束，但不代替本项目实际原生运行证据。

该问题继承自CX的逐对象暂取/回滚算法。简单钳位会丢弃已成功释放的许可；禁用WaitAll、返回假成功或静默切到未同步的server状态均不能算忠实修复。**此P2仍未解决，明确阻止本候选默认采用。**不能由其余源缺口修复或构建成功宣称整个msync/WaitAll完成。

下一实现须统一协调WaitAny、WaitAll、Release/Query、事件操作及abandon/线程死亡。原M3的服务器可等待/协调机制是候选路径，需先建立状态仲裁、异常退出及性能边界；不能只给回滚一行计数加锁。保留原阶段编号与M0候选状态，未宣称M1/M3完成或修改现有产品同步模式。

## 指针表的具体取舍

候选源实现采用固定、进程生命期不移动的指针表，修复旧realloc与无锁读并发造成的悬空访问；命中路径仍为一次acquire读取，无新增全局热路径锁。按28位索引、16字节共享槽计算，16KiB kernel page时每个原生进程分配2MiB表，4KiB页为8MiB；server另有一份。共享数据页仍按需映射，不是预先分配4GiB数据。实际RSS、冷启动及多进程累计成本未测量。

验收包括并发首次映射、跨旧128页容量、索引/引用端点、线程异常退出、实际等待延迟/冷启动/内存，以及上述原生Windows并发计数契约。Mach RPC截止、server丢失及完整清理也未验收。产品默认采用仍关闭；若开销或正确性不通过，候选保持不选中并调整内部映射/协调实现，用户已有引擎不因这批源码移植切换。该回退不等于删除原兼容目标。

## 协调设计检查点（提案，未实现）

服务端事务准入门、每线程可回收操作记录及短WaitAll写事务，是保留16字节槽的一个候选。日常WaitAny、Release、Query以及server Set/Reset/abandon都须登记并确认同一epoch后访问；写事务等待已进入者退出，条件不足时不修改槽且重新开放准入，不能持门等待对象变就绪。条件满足后发布唯一提交决定，查询不能观察部分应用。

旁表须保留线程/索引代际和对象引用、操作提交状态及欠通知；RPC或APC不能发生在仍登记的短临界区内。线程死亡恢复区分未提交、已提交及待通知，不得根据超时、暂停或Wine先置TERMINATED标志就断言旧执行者停止写入。当前kill_thread在发终止信号前先改状态，这项证据不足以清除读侧记录。协调协议需要请求身份、取消/结果仲裁和失联处理，不能在重连时盲重放。

成本包括旁表、每次状态操作的登记/屏障、线程扫描和写事务暂停；尚无性能数据，未采用此新协调方案。后续先核对可复用的原生线程终止证据和现有Mach/Unix通道，再以真正Windows及Wine对照闭合计数、事件、abandon、线程死亡和失联契约，满足原性能门槛后才决定默认采用。该设计不靠不可恢复的全局用户锁，也没有降低现有目标。
