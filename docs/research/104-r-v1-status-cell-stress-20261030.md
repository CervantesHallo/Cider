# R v1：完成页连续跨位宽发布

日期：2026-10-30，UTC+8。接续[103](103-r-v1-status-cell-20261030.md)，补其event稳定阶段未覆盖的连续读写。普通组件三文件保持逐字相同，不重跑已通过的生命周期/身份反例。

## 实现与判据

[stress.c](../../tests/graphics/remote-status-cell/stress.c)使用真实NT section和RO映射；读方关闭自己的section handle后保留view。控制块224B、全固定32位，release/acquire发布身份、准备、进度、结束和abort，不跨位宽复制本地指针/结构。根及子进程PID/birth/session来自实际API；epoch/generation/lease高位常量和动态计数为诊断载荷，未使用coordinator lease或CAContext。

每轮真实publisher调用一百万次；applied high每次增加，low/error/pending_requests/pending_models/cleanup_pending及quiescent有各自相关模式。读方反推n，核对**全部动态字段**、values_valid、无终端标志、精确even version2+2n及单调性。必须观察真实RETRY和writer_running区间的中间n；最终writer_done后另读n=1000000。没有注入奇序号、没有放宽重试/中间观察门槛。

source限定独立审查无新增具体P1/P2，确定父18/读10检查；两PE-Werror编译链接过。父代理审查控制器及独立证据绑定。运行由父代理限定接纳时独立源审仍在进行，最终报告绑定同源hash；不回写审查顺序。

## 实际结果

复用同一已校验的fixed-r1私有引擎，通过真实GUI/XPC运行，不改核心或用户配置。

| 方向 | 实际发布 | 接纳快照 | RETRY | 中间快照 | 不一致 |
|---|---:|---:|---:|---:|---:|
| 64→32 | 1000000 | 29519 | 1537474 | 27044 | 0 |
| 32→64 | 1000000 | 35860 | 1896658 | 32400 | 0 |

两轮均父18/0、读10/0、CLI0，子进程真实对象退出0，无强制终止，最终unmap/close及cleanup1。attempts分别1566993/1932518，严格等于snapshot+retry；不是把RETRY当已验证快照。读方抽样，不声称读遍全部版本。

独立核对两轮raw/session、实际EXE、16PE+2native/F27、四份完整GUI退出/两空/no-pending回执、普通配置/DLL原字节或原缺失恢复和四个生产保护摘要。见[结构化证据](evidence/r-v1-status-cell-stress-20261030.json)。

## 中断与恢复

第一轮64→32 raw曾记录父18/0、读10/0和百万次完成，但用户中断工具后控制器未取得CLI及最终回执，保留原running记录，**不接纳该轮**。恢复先取得真实GUI退出/两空/no-pending回执，再逐文件核对并恢复普通配置；完整恢复记录和raw独立归档。随后使用新路径补做完整两方向验收，不伪造旧CLI0或改写旧结果。经验：工具中断后不能假定finally已执行，必须核对进程、配置和落盘清理回执。

## 边界与下一实现

writer_running区间含yield和中点暂停，不证明单条store与每个snapshot同时；但实际RETRY和关联快照均观察到。state保持ACTIVE，终端/耗尽沿用103的限定证据，不计作本次连续压力覆盖。时钟只拒绝超期观察，不能取消卡住的同步NT；外层60秒负责终止及真实清理。无性能基准、全量泄漏或安全隔离结论。

下一实际实现是typed producer请求在途所有权：现有bool busy不能让终端完成判据看见仍持有资源的请求pin。候选新增request_inflight并让最后一个请求归还后再finalize，先普通/NDEBUG编译、审查及真实admitted-pin/撤销对照，再连接发布、未知安装取消与producer资源。不会因为完成页压力通过就开启foreign路线或切默认。原HY/U8/15/Windows忙待重启及门禁不变，Windows/Actions/定时/Goal零操作。
