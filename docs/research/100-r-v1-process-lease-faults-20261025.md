# R v1：进程租约清理故障的实际对照

日期：2026-10-25。接续[99](99-r-v1-typed-producer-leases-20261025.md)，检验0021已修复的两个特定故障。普通源码未再改动；诊断故障、故意错误分支和失败观察日志均不进入普通/用户引擎。此项仍属于原CEF桥接生命周期主题。

## 一次丢失的 STOP

native在确认标志仍false、真实PE callback尚未交付时，只让首个STOP返回失败1168。此时worker、event和其独立复制的进程句柄均仍真实存在。修后实现保留清理owner，周期timer重试实际STOP，worker取消并归还资源。负例只恢复忽略确认来源的条件，过早把NOT_FOUND当作已清理；没有更换worker或伪造内核成功。

| 方向 | 修后检查/失败 | 错误分支检查/失败 | 负例精确失败 |
|---|---|---|---|
| PE64 → PE32 | 68 / 0 | 68 / 1 | CHECK17，活peer期间wait_idle |
| PE32 → PE64 | 68 / 0 | 68 / 1 | 同上 |

两种方向均实际记录一次故障，绑定首租约0x20000000c。修后确实保留1168错误并完成4个高位租约；错误分支只有后3个native完成，首个worker等到peer真实退出才结束。两组子检查15/13/26、退出0/77/0和清理都完整。负例不是任意提前退出。

首轮修后PE64程序68/0，但控制器未启用warn+macdrv，缺少故障WARN标记而拒绝接纳。原失败完整保留；新版本仅补明确日志级别、重新核对实际已装字节并使用新的原始记录。没有以原程序成功补造缺失的故障标记。

## 实际 worker 终止

真实STOP等待返回WAIT_OBJECT_0后，首个实际PE worker在进入自身SRW/native锁、正常完成发布或引用账尾声前调用ExitThread。恢复timer必须从保留的真实线程句柄观察终止，归还遗留worker引用，并发布FAILED/ERROR_OPERATION_ABORTED(995)。负例只省略“已终止worker引用归还”块，其他内核等待、完成和unlink条件均保留。

| 方向 | 修后检查/失败 | 遗漏归还检查/失败 | 负例精确失败 |
|---|---|---|---|
| PE64 → PE32 | 28 / 0 | 28 / 2 | CHECK17、23，两次wait_idle |
| PE32 → PE64 | 28 / 0 | 28 / 2 | 同上 |

所有运行实际记录指定首租约的exit-after-stop标记及FAILED/995；peer仍按正常流程退出0。负例确认其worker已终止、native完成已交付，却未达到PE owner归零，外层随后确认失败owner真实退出。不是以仅断开连接代替终止证明。

最初未运行的v1把故障放在等待前，父代理发现timer可能抢在acquire完成前撤销的时序风险，遂撤回该准备版本；v2用真实STOP事件保证顺序，不加sleep。原v1源码/构建保留，不追认为已运行。独立审查原件将该撤回写成“User withdrew”，旁证明确更正为父代理撤回，未改审查原文。

## 防止观察器失败冒充负例

限定对抗性独立源审两项均无新增具体P1/P2，且指出仅CHECK17/23不足以排除probe读取失败。父代理另给失败路径增加只读日志，不改原CHECK表达式、截止时间、成功路径或资源操作；记录最后一次probe是否成功以及48B/version2的实际计数。该日志增量由父代理复核，未冒称已包含在先前独立审查中。

新增观察只跑尚需补证的负例，正常通过组不重跑。要求每次失败观察均valid1，native producer/watch/host为0，实际PE live watch及timer owner各1，根仍存在；从而把实际遗留owner与读取失败区分。完整原始运行、session/EXE/成组模块和清理指纹见结构化证据。

## 证据与边界

父代理独立重读首批8次运行，核对原始log/session、实际EXE、16个PE/2个native模块及F27核心。19次清理尝试均落盘原始命令/退出/输出/解析值，确认GUI实际退出、至少两次空观察、无pending；普通实验配置和DLL原字节或原缺失恢复，四个生产保护摘要一致。补充观察的清理和模块绑定另有记录，不混算为首次8次。公开故障/负例/客户端delta已零fuzz/offset重建逐字一致。

这些结果只闭合所列一次丢交付及STOP之后worker终止恢复；不证明重复故障、NtClose失败与并发完成的交错、任意持锁终止、完整heap或性能。下一步仍是失败close/在途完成的实际对照，然后真实CAContext producer资源和事件/reparent路由及两/三进程呈现。

没有启用foreign-child renderer，不切用户默认，不提升CEF/游戏状态。原HY顺序、U90的8/15缺口、Windows忙/待重启及游戏门禁保持。无Windows、Actions、自动化或Goal操作。完成实验的整引擎/stage及重复备份按SHA差异归档后清理，保留工作组和源/raw；恢复必须查清单，不拿manifest当字节证明。

公开文件在[remote-transport/faults](../../tests/graphics/remote-transport/faults/README.md)，指纹见[结构化证据](evidence/r-v1-process-lease-faults-20261025.json)。
