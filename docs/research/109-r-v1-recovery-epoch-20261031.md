# R v1：旧恢复结果与新任务的代次竞态

日期：2026-10-31，UTC+8。接续[108](108-r-v1-recovery-fifo-20261031.md)。验证0024已有的代次保护，普通实现未改。

## 实际调度

[私有夹具](../../tests/graphics/recovery-epoch/)先登记并结束真实的空setup，启动普通恢复定时器。选中一次实际Native调用成功、pending为0的回调，在它进入普通代次判断前，以独立事件暂停；不持watch_lock等待，也不替换Native返回值。

客户端随后调用普通acquisition，实际打开的临时thread handle受ProtectFromClose保护；原NtClose返回c0000235，真实setup/FIFO保留该资源。新任务的普通ARM把真实恢复代次从8589934593推进到8589934594，且没有PE进程watch。销毁真实Root HWND并确认其原epoch消失后，再释放旧回调。

该回调在原判断之后、同一watch_lock事务内，记录实际的代次及needed值；局部选择标志绑定同一次调用，后来回调不能改写记录。CHECK19先验证完整时序和真实资源，CHECK20才检查旧zero有没有抹去新needed。每步和所选调用都有实际四秒时限；超时不作为竞态命中或性能数据。

## 正反对照

| 组 | PE64 | PE32 | 所选旧回调后的needed |
|---|---|---|---|
| 保留普通代次判断 | 29项、0失败、CLI0 | 同左 | 1 |
| 仅移除该判断 | 23项、2失败、CLI1 | 同左 | 0 |

反例两位宽均先通过CHECK1–19，再于CHECK20拒绝实际错误决定。真实清除handle保护成功后，CHECK23还观察到恢复无法自行排空：setup及cleanup各1，PE timer owner0。其程序明确报告settled=false、cleanup=false；这不是合格的正常清理结果，也不把当时decision_timer=1解释为定时器已经退役。

反例随后由外层监督器确认私有Wine/GUI进程实际退出。正例仅用纯snapshot及Sleep观察到Native资源、PE watch/timer和夹具事件owner归零，不手动执行恢复、不重放ACQUIRE/ARM。进程清理和瓶子恢复另外核对，两组共10份完整回执。四个实际EXE/raw/session、16PE及2native模块、F27核心、原DLL字节或缺失状态、原配置及四个保护摘要均已独立绑定，见[证据](evidence/r-v1-recovery-epoch-20261031.json)。

## 构建、审查与边界

夹具及四文件集成、唯一负例和控制器经限定独立对抗性复审，无新增具体P1/P2。实际编译依赖取得10组闭包，核对126个项目输入；Native、匹配watch2的两PE驱动及两PE客户端构建成功。早期v1遗漏旧probe wrapper导致PE链接失败；v2只补回该wrapper，原失败记录保留，复用未受影响的Native/init对象。

公开diagnostic.patch/before.patch均零fuzz、零offset重建，与实际使用的四个源码文件逐字一致。208B/version1私有帧、48B/version2旧probe、两表8槽与watch2成组；不修改USER96协议。

这只验证明确指定的旧zero/newARM时序及观测资源，不是全部并发交错、完整泄漏、callback栈退出、Windows原生、渲染、游戏或默认引擎采用证明。接着按原顺序完成per-host状态、未知INSTALL结果及取消，再连接真实producer资源、事件reparent与两三进程像素。原HY/U8-15、Windows忙待重启及游戏门禁保持；Windows、Actions、定时、Goal零操作。

完成核对后，6套临时整包/stage/重复备份及4个瓶内EXE副本已清理；全部差异字节、清单、源码、原始结果和工作基底保留。文件系统实测额外释放约55MiB，不能按目录逻辑大小报回收量。
