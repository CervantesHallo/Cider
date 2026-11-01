# R v1：Native 异步更新不能把排队当完成

日期：2026-11-01，UTC+8。接续[123](123-r-v1-busy-predecessor-20261101.md)，将验证接回普通 Native 异步更新管线，普通产品逻辑不改。

真实 managed host 建立后，在 Cocoa 队列安排有界 Unix 条件屏障。绑定的 Wine 线程两次调用普通 macdrv_remote_host_refresh；真实代码接纳 job/ref、计算几何并通过 Wine 线程预检，随后异步入队。屏障仍未结束时，原始冻结记录实际 jobs=2、两次排队成功及第二个真实 serial；再解除屏障。

| 实际观测 | 两种 PE 均相同 |
|---|---|
| 两次普通接纳时的 model_jobs | 1、2 |
| Wine 线程预检 | true、true |
| Cocoa 队列上的更新校验 | 旧 false、新 true |
| 实际进入普通 Cocoa 更新方法 | 仅新任务，users=1，Cocoa main=true |
| 普通销毁归还后 model_jobs | 旧任务后1、新任务后0 |
| 验收 | 各32检查、0失败、CLI0 |

旧任务不因已经排队就执行过期更新，仍通过普通 finally 归还自己的任务责任；新任务实际进入普通更新方法。原 context／RO 保留至逻辑任务归还完成，独立观察引用不加入 guard，只有实际计数为零、相应销毁观察和屏障完成后才归还。之后真实 USER CANCEL、Host／producer SAFE 和原始 context／NT 清理通过。

限定独立源审无 P1／P2，Native coordinator NDEBUG、288B 帧、两张 9 槽 Unix 表／watch v2 成组；9 组有效编译闭包／124 项输入及产物绑定，10 条旧 Cocoa 警告语句／消息一致。独立重读原始 job serial、校验结果、方法入口、逻辑销毁、session／EXE；另核对16 PE／2 Native／F27、5份完整清理、原瓶字节或缺失恢复及四保护摘要。五文件诊断精确重建，见[夹具](../../tests/graphics/host-async/)与[证据](evidence/r-v1-native-async-20261101.json)。3棵临时整树、2份前缀EXE差异归档清理，必要源码／原始记录／组件／普通基底保留。

方法入口和参数记录不证明几何无异常应用；逻辑 disposer 观察不证明回调尾部退出、线程 join、全队列空闲或全部内存释放，外层真实进程清理单独留证。这是一个 host／绑定调用线程／两个更新的指定时序，不代表任意取消、终止、USER REFRESH 传输或全部更新组合。没有跨进程、像素／GPU／compositor、性能、Windows原生或游戏结论，默认／门禁不改。

当前 per-host 指定验收集合已补至 Native 异步路径；下一按原顺序接多进程真实资源消费者，再事件 reparent 和两三进程像素。更广泛故障覆盖继续如实保留，不能把有限集合说成任意并发完全证明。
