# R v1：没有 PE watch 时的失败恢复

日期：2026-10-30，UTC+8。接续[106](106-r-v1-producer-terminal-cell-20261030.md)，补实际失败准备和早期临时句柄故障。

## 普通0025修复

独立复审发现一个交接缺口：恢复线程取走producer FIFO，另一个控制帧仍持cleanup_inflight pin，finalizer无法关闭状态页。若取走的FIFO owner直接释放，无PE watch时只剩“有待办”的计数，后续timer却没有可执行队列项。

[0025](../../engine/porting/wine-11.18/0025-winemac-requeue-status-cleanup-after-blocked-finalization.patch)在finalizer尝试之后、旧队列引用释放之前，再调用既有同锁去重入队函数。仍有状态清理任务则取得新FIFO owner；已入队则不重复；实际完成则无动作。关闭不持锁，也不靠观察代码调用finalize/retry。普通/NDEBUG及Native链接通过，限定独立源码复审无新P1/P2。

这项修复保持原有请求、真实NT清理及Wine身份，未改变窗口/渲染策略；代价是待清理时一次去重交接，收益是无PE watch仍有可运行owner。候选未进入默认配方，回退保留0024实验参考，不能以跳过错误或丢弃handle替代。

## 实际故障与结果

[公开夹具](../../tests/graphics/producer-startup-failure/)在真实Win32根窗口上调用普通acquisition。两模式使用共同PE/客户端；Native只增加实际NT权限/handle保护故障及诊断。

| 模式 | 实际路径 | PE64 | PE32 |
|---|---|---|---|
| section准备失败 | 真实RO权限handle、真实保护、RW map c0000022；WatchSTART未执行，close c0000235 | 25/0，CLI0 | 25/0，CLI0 |
| 临时thread关闭失败 | NtOpenThread真实结果已登记，实际保护，原NtClose c0000235；setup FIFO保留 | 25/0，CLI0 | 25/0，CLI0 |

均无返回lease，PE process-watch owner在全部采样中为0；pending定时器owner非零。目标Root HWND真实销毁后仍保留Native owner，清保护仅执行实际NtQueryObject/NtSetInformationObject。随后只用不做清理的snapshot/probe和Sleep，Native producer/watch/setup/cleanup/model及PE timer owner自主归零。

mode1原始日志绑定同一root_pid/lease/handle的实际map c0000022、close c0000235和最终close0；mode2读实际setup.last_close并绑定同setup日志。实际owner ID保留高位，没有虚构ID或错误码。context/renderer未创建，不能借为图形或游戏结果。

## 复审与失败保留

第一轮止于CHECK9：Wine实际存在3个root，夹具错误假定只会有1个。未运行故障，原CLI1及清理恢复保留。改为目标HWND及实际epoch登记位；销毁后必须确认该目标消失，全局内部窗口仍可存在，不把它们算为本夹具泄漏或全局零。

PSF-P2-01促成普通0025交接修复。验证进一步强制CLEAR pin期间有真实timer retry；PSF-P2-02指出仅在finalizer之后取样可能误把较晚开始的CLEAR算入。最终每个retry局部保存决定前的CLEAR尝试号，决定后要求同一尝试仍持pin，再发布见证；超期不接纳，observer不提供清理动作。旧报告和准备版本均保留。

夹具由一个限定写入范围的代理实现，父代理整合、编译、运行；另一个独立代理复审，最终源码无新增P1/P2。作者报告不是验证。Source/build、原始运行、绑定和清理分别记录。Native/coordinator有效include、两表8槽、96B私有帧/version1、恢复version2、同组PE与实际EXE均已核对。普通补丁及两故障源码zero-fuzz/zero-offset逐字重建。

独立绑定4次raw/session、实际EXE、16PE+2native/F27、10份完整GUI退出/两空/no-pending回执、普通实验配置/DLL原字节或缺失恢复及四个生产保护摘要。详见[结构化证据](evidence/r-v1-producer-startup-failure-20261030.json)。Native日志使用真实返回值，不把缺失或截断日志当作故障证明。

## 接续边界

这闭合上述两条正常执行的失败路径和真实CLEAR交错。FIFO多owner公平、旧zero代次的指定调度仍未实际验证；非局部退出跳过Native收尾也未解决。继续这些门槛后，进入per-host、未知安装/取消、资源消费者、事件reparent和两三进程像素。

timer owner0表示删除/延后取消已确认，不等于所有callback已退出，实际进程清理另行核对。没有全量泄漏、性能、原生Windows、compositor/GPU、完整ABA、CEF或游戏通过结论。原HY顺序、U8/15、Windows忙待重启、反作弊门禁及默认引擎保持；无Windows、Actions、定时或Goal操作。
