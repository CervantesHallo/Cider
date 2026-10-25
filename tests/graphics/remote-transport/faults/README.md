# 有界故障对照

只用于一次性私有诊断引擎。先应用父目录的0021诊断补丁，再在独立副本分别应用以下故障。普通引擎不包含这些入口；两个故障不能混用后声称独立验证。

| 主题 | 故障补丁 | 负例再叠加 | 客户端补丁 |
|---|---|---|---|
| 丢失一次 STOP 交付 | stop-delivery-loss.patch | stop-before.patch | stop-client.patch |
| 实际 worker 在 STOP 等待后退出 | worker-exit-after-stop.patch | worker-before.patch | worker-client.patch |

Wine源补丁在源码根目录 `patch -p1`；客户端补丁在父目录 `client.c` 的单独副本应用。重新成组构建实际改变的模块，并记录来源/EXE/模块摘要：STOP只改native协调器，worker只改两种PE驱动，未改的组仍须按实际字节核对。两种位宽的客户端使用父目录构建方式，通过私有GUI/XPC运行。

必须显式启用 `warn+macdrv,trace+macdrv`，核对故障标记与指定高位租约，只接受下列完整对照，不把任意错误算作有效负例：

- **STOP**：只在第一次STOP、确认标志仍false且实际callback尚未交付时返回失败1168；真实PE worker/event/process句柄仍活着。修后两方向各68/0，完成4个租约；负例仅去掉确认来源条件，两方向各68/1，精确失败CHECK17，首租约的native完成缺失，其余流程、子退出0/77/0和清理完整。
- **worker**：真实stop-event等待返回后，首个实际worker在持锁/发布/引用归还前自行退出。修后两方向各28/0；负例仅省略timer对已终止worker的引用归还，各28/2，精确失败CHECK17和23。两组都必须观察指定租约的实际终止恢复结果FAILED/995，子进程正常退出0。它不覆盖持锁终止或任意指令边界。

`stop-idle-observation.patch` / `worker-idle-observation.patch` 是失败路径的只读日志补充，应用于对应已改客户端。原CHECK、截止时间、成功路径和资源操作不变；只在失败返回前记录最后一次真实probe的有效性及计数。强负例验收还要求valid1、size48/version2、native producers/watch/hosts全0，而PE watch和timer owner各1；缺失或失败的probe不能冒充所有权缺陷。已通过的正常组无需为这条失败路径日志重跑。

每轮必须实际确认所有子进程/owner退出、GUI真实退出、两次空观察、无pending，随后恢复普通配置/DLL。尤其worker负例故意保留进程内引用，断开CLI不能代替确认失败owner已经结束。原始异常和控制器失败保留。此处不覆盖失败NtClose/并发完成、完整heap、CAContext像素、CEF、游戏或性能，详见research100。
