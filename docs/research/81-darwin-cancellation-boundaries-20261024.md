# WaitAll：Darwin 取消边界的限定核对

状态：源码调查及本机两架构原生观测完成；尚未据此修改Wine保护策略，不是Windows、信号安全或所有macOS版本的证明。接续原02与80，不改变HY顺序、性能门槛或游戏门禁。

## 原因与可采用的结论

F25固定六轮速度仍未达5倍。其插桩诊断中，owner重试耗尽11231次、未就绪9800次；盲目扩大spin不能消除等待前后的完整保护成本。接续先核对很小的事件唤醒调用集合是否真的会交付POSIX deferred cancellation，再审查其周围调用链。

Apple公开源码的系统调用生成器按取消点表选择 `cerror` 或 `cerror_nocancel`；所核对的 `__ulock_wake` 生成路径使用后者。两者的差别是前者调用 `_pthread_exit_if_canceled`，后者只处理errno。[生成器](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.101.15/libsyscall/xcodescripts/create-syscalls.pl#L339)、[错误返回实现](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.101.15/libsyscall/custom/errno.c#L58)。

所核对的 `sched_yield` 调用 `swtch_pri` 或 `thread_switch` 并返回，没有POSIX取消检查。它仍然是Mach调度陷阱；“不是deferred取消点”不等于“没有kernel调用”。[libpthread实现](https://github.com/apple-oss-distributions/libpthread/blob/libpthread-539.100.4/src/pthread.c#L2094)。

这支持研究**已解析函数的限定调用集合**，不能推广到整个frame。server read/write、慢映射、close、恢复、日志或尚未证明的Mach消息路径仍需原完整mask；异步取消、Wine控制信号及未解析绑定须分别处理。

## 本机原生观测

公开源为 `tests/sync/darwin-cancel-boundary.c`，与已运行私有源逐字相同；SDK15.4/O2、deployment14分别构建arm64及x86_64，零警告。每个进程使用新的独占session、25秒自截止和35秒外层截止，只操作自己的线程、共享页、pipe及符号；实际均正常结束。

每个目标线程先设ENABLE/DEFERRED、发布modeled held状态，父线程排队真实pthread_cancel后才允许目标调用。真实waiter用本进程thread_info观察阻塞，不靠固定sleep推断。调用返回后先清除held，再以显式pthread_testcancel交付请求。已解析/预热的调用不覆盖首次动态绑定。

| 调用 | 两架构结果 | 限定含义 |
|---|---|---|
| raw ulock wake，无waiter | 返回−1/ENOENT，held清零后取消 | 错误路径未提前交付 |
| raw ulock wake，真实自有waiter | 返回0，held清零后取消 | 实际wake路径未提前交付 |
| sched_yield | 返回0，held清零后取消 | 本机调度调用未提前交付 |
| 已知取消点read | 未返回，cleanup见held=1 | 正对照确认请求确实排队并可交付 |

每架构5个自有线程均join，cleanup各一次；r2 arm64约1.194秒、Rosetta x86_64约1.310秒，无外层超时。结果保存本机macOS26.7/25G229和实际libsystem符号位置/入口字节；入口字节可能含邻接函数，不当完整反汇编或完整调用图证明。探针只观察合成义务，不证明Wine pin/owner/信号协议。

## 实施前门槛

只考虑cached普通SetEvent的短路径；必须保留原事件提交、引用、准入、代际、死亡恢复和Highball调度让步。所有不满足条件的分支在**同一个仍活跃的frame内**先完整mask，再交回原路径。提交后不得返回PENDING造成调用重放；需要恢复或Mach消息时，在执行前升级保护并完成原义务。不能只在入口检查multiple_waiters：登记可并发变化，真实发送分支也必须守住。

必须排除TLV解析/懒初始化等隐含调用，保留短Wine控制guard，末引用释放前完整mask。任何扩大范围须先做实际产物审查和针对性验证；收益未证实前保持非默认。原5倍、CS±3%和精确产物自己的小时条件不变；不切用户瓶子，不提升完整M1或游戏兼容评级。

独立审查指出初版只验证先前park观察，未要求waiter结果，后续清理wake可能掩盖超时。r2先join再判定，强制target返回0且waiter返回0、明确无rescue；双架构重新执行通过。原v1源码/构建/结果保留，不能把它追认为已具备r2验收条件。
