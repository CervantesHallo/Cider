# R v1：两个真实失败 owner 的 FIFO 进展

日期：2026-10-31，UTC+8。接续[107](107-r-v1-startup-failure-recovery-20261030.md)。普通队列实现不变，验证限定合法时序下的多owner公平性。

## 资源、调度与结果

[夹具](../../tests/graphics/recovery-fifo/)两次普通acquisition，各留下实际受保护thread handle和setup FIFO owner，真实ID8589934603/8589934604。Root HWND及原epoch登记消失后，只实际清新项保护；纯snapshot不做清理。

私有诊断组串行化实际cleanup消费者，旧项普通pop后暂留busy，等待独立客户端清新项。旧项随后实际NtClose c0000235；普通requeue完成时，在remote_lock内观察真实位置，要求新项仍queued、非busy/active。这是允许的测试时序，不是产品的新锁或全部并发公平性证明。

| 组 | PE64 | PE32 | 实际旧/新位置 |
|---|---|---|---|
| 普通tail重排 | 20/0、CLI0 | 20/0、CLI0 | 1/0 |
| 仅失败retry改head | 20/1、CLI1，仅CHECK14 | 同左 | 0/1 |

修后新项实际retry成功、last_close0且移除，旧项仍保护/c0000235；随后清旧项，两组都实际归零/cleanup1。CHECK13先接纳真实锁内order，CHECK14直接拒绝错误位置，不把任意超时当饥饿。初次入队、NT故障、PE/client及其他普通逻辑保持相同。

## 审查及观察器纠正

RF-P2-01：首稿仅看进展/超时，重叠callbacks可能让错误head策略也通过，或误判调度延迟；修为明确时序和锁内真实order。RF-P2-02：RUN部分失败先捕获owner，cleanup按实际owner掩码。RF-P2-03：补RUN和销毁后单次观察时限。最终限定独立复审无新增P1/P2。

最终首个PE64程序20/0，但控制器仍要求基线older_retries大于0。新时序在关闭前暂停旧项，该计数此时为0。原failed记录保留，单独纠正；父代理独立复核有效PE64 raw/CLI0/完整清理，复用子集，仅补未执行PE32。没有改写旧record或无变化重跑通过组。

有效Native include/helper、PE/header/frame120/version2、两表8槽、watch2匹配构建通过，诊断及唯一负例zero-fuzz/zero-offset逐字重建。独立绑定4次raw/session/EXE、16PE+2native/F27、12份完整清理回执、普通配置/DLL原字节或缺失恢复及四保护摘要。见[证据](evidence/r-v1-recovery-fifo-20261031.json)。时限不作为性能数据。

## 接续与边界

继续指定旧zero和新ARM的恢复代次竞态，再推进per-host、未知安装/取消、真实资源消费者、事件reparent和两三进程像素。代次夹具已单独准备源码，尚未构建/运行/接纳。

本次不是任意负载/并发消费者公平性、完整内核/heap泄漏或callback退出证明；没有原生Windows、性能、CEF/游戏或默认引擎采用结论。原HY/U8-15/Windows忙待重启及门禁保持，Windows、Actions、定时、Goal零操作。
