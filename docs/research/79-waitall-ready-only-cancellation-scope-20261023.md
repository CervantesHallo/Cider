# WaitAll：已就绪事件的受限快速路径

状态：非默认 F24 revision2 已独立源码/反汇编复审、组件构建、原生夹具及自己的Wine短功能验证；固定六轮AC性能两项失败，不采用/不跑自己的小时。接续原02，Windows仍busy/不能重启；HY顺序、用户引擎、游戏门禁不变。

## 为什么不直接替换原保护

F22与F23各自完整固定性能队列均失败，不能重试挑样或借用旧小时。当前配对原生组件诊断的 state disable/restore 中位347.726ns、type deferred/restore 5.497ns，只是单线程包含循环的独立成本，不能换算成Wine收益。

独立调用图审查确认，完整帧可到达服务器read/write、close/fcntl、诊断输出和Windows控制信号处理；type DEFERRED不能替代state DISABLE。两个架构各六项真实pthread_cancel/私有pipe-read取消点验证也显示，未保护的type-only负例在模拟资源仍持有时取消，原state保护和取消点前state保护则在资源清理后取消。这是原生合成义务，不是Wine对象引用或信号等价证明。

公开依据：[Apple取消类型手册](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/pthread_setcanceltype.3.html)要求恢复进入时类型，并列出read/write/close/fcntl等取消点。[libpthread公开实现](https://github.com/apple-oss-distributions/libpthread/blob/libpthread-539.100.4/src/pthread_cancelable.c)和[XNU取消入口](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/bsd/kern/kern_sig.c)还表明，修改type不会自动设置内核取消DISABLE；不能仅凭Mach/ulock不在POSIX列表里就认为安全。公开版本不冒充本机加载二进制身份。

## F24 的封闭范围

只尝试已缓存、具备SYNCHRONIZE权限的单个普通事件，且不可alertable，WaitAny/WaitAll类型有效。整个短尝试保留Wine control_depth，防止控制处理器在此期间进入有取消点的服务器调用；不改变永久取消策略，也不在信号处理器调用新的pthread接口。

1. 以type DEFERRED保存/恢复原类型，状态不主动启用；先发布原native帧，再在无分配/日志/释放的缓存查询中取得引用并记入帧。
2. 只使用已发布pthread key的本线程描述符。没有key/描述符/映射/有效epoch就走原路径，不初始化TLS、打开文件或发RPC。
3. 原SC准入发布/重查保留。只做一次owner CAS并要求CLEAN，不自旋、等待、探测租约或恢复日志。epoch还须在owner内再次验证，不能只依赖入门前的检查。
4. 只有普通事件当前确实就绪才由**原事务提交**完成获取：auto清零，manual保持。没有通知义务；不假造成功或吞掉许可。未就绪、竞争、恢复、异常类型均回原完整保护路径。
5. 释放门时若原wait_hint触发kernel wake，先经native回调升级为state DISABLE；fork准入退出需要wake时也先升级。保持原唤醒顺序，不省实际wake。
6. 释放引用只允许在refcount>1时做有界CAS减一；成功后不再碰该对象。若可能最后引用或竞争超限，在移出帧记账**之前**升级state保护，再执行原最终释放/close。
7. 失败尝试升级的是**同一个仍存活的帧**：先DISABLE，再恢复进入时type并清理投机引用，随后进入原慢路径。中间不得出现取消已启用/原ASYNC类型的空隙。
8. 成功后先排空owner、准入、引用及帧链，再恢复state/type，最后恢复Wine controls。退出清理保留原强制DISABLE覆盖。多对象、APC、阻塞和SignalAndWait继续原帧保护。

共享magic3、16字节槽、事务提交、死亡租约和Mach计数不变；新增的before_wake只是本进程native上下文回调，不进入共享文件。旧server产物与新客户端的共享格式仍相同，实际pair须独立核对。

## 收益、代价、验收与回退

候选收益只是假设：已就绪的单事件获取或许能省一次state切换，尚无实际Wine性能证明。代价是更多分支、一次try获取、原生帧字段、缓存引用CAS，以及失败尝试的额外工作；控制只在短CPU路径保持延期，不能把迭代/代码长度当墙钟上界。公平性、取消恢复和真实快慢路径占比都须验证。

进入运行前必须完成源码反例审查，精确新header的原生恢复及try-clean/wake夹具，以及从实际sync.c提取帧函数的取消/最后引用/同帧升级测试。提取测试中的线程数据、引用释放及Wine controls使用明确test double，不能代替Wine真实信号与内核等价验证。

再完成自己的两位宽短功能、生命周期、线程/进程并发和实际pair核对。正式性能仍固定三对、同EXE/pair/电源条件，5倍与CS±3%不变，通过后才跑该精确产物自己的连续小时；不借F7/F8。所有未通过项仍阻止采用。

回退为保留原F21完整保护，不选F24；先停止私有实验实例再切其配对产物，用户瓶子和默认引擎不切换。

## 实际审查与原生结果

独立审查发现一项P2：源码的NULL-record短路仍被编译器提前解析 `coord_retired` TLV，冷线程可能在pin存活时初始化TLS，不能证明封闭范围；未发现实际取消点或泄漏。revision2从ready函数彻底移除TLV表达式，改为既有native描述符中的retired字段；正常发布前清零，fork child在关闭继承描述符之前置一。原TLS及完整慢路径保留。独立复审及父代理反汇编确认NULL检查先于record访问，ready可达路径无TLV解析；原生上下文字段不进入共享文件。

最终ntdll `c97d1a9e8b189c916025ab572a6badfd2231241c2ab157adb91fd8c69d55112e`，server仍为F21的 `1798e22dc9821ac1327f5d9a8c03abb145ba31cfa750defe1b706c391d9740a4`。SDK15.4/O2组件零警告、300个原导出集合相同、四文件补丁精确重建。未运行的revision1产物与原审查记录分开保留。

- 精确新header的原生恢复夹具在arm64/x86_64通过；原三个commit负对照全部拒绝。
- try-clean夹具两架构通过实际owner操作、构造的脏日志拒绝和guarded ulock wake；漏脏日志判断、漏wake前保护两个x86负对照拒绝。
- 从实际msync.c提取ready函数，以明确native test doubles驱动auto/manual获取、无就绪/竞争/脏日志、retired、缺映射、fork准入和epoch在两次检查之间退休，两架构通过；去掉owner内epoch复查和忽略retired的x86负例拒绝。这是构造交错，不冒充真实Windows退休调度。
- 从实际sync.c逐字提取frame函数，两架构各28个真实pthread worker用例通过（各1448项检查）。包含原ENABLE/DISABLE、DEFERRED/ASYNC、非末引用的待取消请求、最后引用释放、同帧升级、kernel回调及嵌套；嵌套场景未发取消请求。最后引用和慢路径使用真实pipe read取消点，引用、thread_data和Wine controls仍明确为test doubles。两个遗漏保护的x86负对照各回收28线程并给失败完成回执。

原夹具交接曾在25秒watchdog终止；其最终交接原只依赖read，修正为义务排空后的显式pthread_testcancel，原失败不改。新增pending非末引用场景的首轮验收器误要求NORMAL_RETURN，四项失败原回执也保留；修正预期后重新运行新夹具，不追认旧记录。另有初版mock宏编译错误及反汇编命令参数错误，均不计引擎运行或通过证据。

用户已再次确认接电开盖，本机读回AC/充电。自己的13阶段及11份native/PE身份逐项核对通过，随后声明独立AC/LPM0固定六轮v4队列。完整结果为server10200/candidate2300ns=4.434782609倍，CS1.061/1.178ns/op=+11.0273327%，两项均失败；不混入旧电池数据，不额外引入quiet/CPU门槛，不借旧小时。

公开native夹具和显式源提取器已在两架构重新构建/执行，保留提取器初版误把后续非Apple空定义作为重复结束边界的错误。修正为唯一start后的首个精确end，仍逐一检查函数定义及字节哈希。后续先收实际ready路径命中/回退原因的诊断证据，不能根据源代码或旧F8采样推定当前负载已得到收益。完整M1、Windows契约及游戏裁定保持未完成。

## 2026-10-24：实际路径覆盖

用户恢复推进后，另建只加全局原子计数的诊断native，原F24源码/二进制/六轮结果未改。计数点无TLS初始化、分配或I/O；报告只在原线程frame/wait/lease清理之后输出。按实际guest Windows PID绑定报告；全局快照非一致，取各计数最大值，不把重复报告相加。新增原子操作会扰动调度/竞争，以下比例不是未插桩负载的精确占比，也不进入性能验收。

固定执行一次ping-only（10000次/peer，另含warmup与运行时）和一次始终就绪manual-event控制。前者22052次尝试、缓存pin22047次；成功148次（约0.67%）。组合fork/busy/dirty门回退11927次、事件未就绪9972次；closed/type/access、冷key、record/epoch、映射等拒绝项为零。组合项不能证明每次都是owner竞争。后者10000次实际就绪获取全部命中，另一次未就绪轮询正确回退；说明路径可工作，当前快路径覆盖没有直接转化为验收所需收益。

诊断native为 `8b5b70adc1dd30b105c67e5dd24b676824e57a5e9ba351ce5febba03dabb7bfb` / `1798e22d…`，只在私有空瓶运行。两程序退出/完成/时钟及身份检查通过，最后确认私有GUI退出、连续两次无自有Wine进程。公开clean夹具也已按公开源路径在两架构构建执行通过。

接续假设仅为：在首次获得owner之前做有界CPU重试，或许能避免部分立即升级完整帧的成本；拿到脏日志立即回原恢复路径，拿到干净owner后仍只检查对象一次。不能依据本诊断就删掉状态保护、调整原门槛或循环等待未就绪对象。
