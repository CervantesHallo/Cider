# R v1：真实安装超时后保留资源并取消

日期：2026-11-01，UTC+8。接续[117](117-r-v1-host-status-overlap-20261101.md)，仍在当前 per-host 阶段。这次实际使用普通 USER 发送器，不再使用直接调用安装函数或不消费回复的适配器。

## 实际结果

普通发送器的 SMTO_ABORTIFHUNG／3000ms 保持不变。拥有 Root 的主线程泵送实际消息，接收方进入普通安装函数、真正取得 request/control/model_job 后等待测试自己的事件。发送线程实际超时返回，捕获真实 LastError 和耗时，再读取仍持有的 context／RO，最后发事件解除接收方等待。

| 实际观测 | PE64 | PE32 |
|---|---|---|
| 发送结果 | delivery=false，ERROR_TIMEOUT 1460，3001ms | delivery=false，ERROR_TIMEOUT 1460，3001ms |
| 超时返回时接收方 | held/control/job/request 各 1，resource_done／terminal 各 0 | 相同 |
| 发事件前资源 | context／读方持有，Host RO 非 SAFE，模型各 0 | 相同 |
| 接收回调结束后的实际效果 | 图层／登记／选择各 1 | 相同 |
| 原 ticket 取消及实际释放 | 36 检查、0 失败、CLI0 | 36 检查、0 失败、CLI0 |

这证明该次真实超时并不表示安装没有发生。调用方保留原 ticket、context 和完成页，没有重发 INSTALL；随后真实 USER CANCEL，等实际模型归零及 Host／producer SAFE 后才销毁资源。工作线程的 guard／context 快照在发事件前捕获，主线程 join 后才输出，不把输出顺序误当捕获时刻，也不声称两个快照原子一致。

## 证据与审查

限定独立源审无 P1／P2。审查沿普通消息封送／回复路径确认接收缓冲区不依赖已经返回的发送方栈，并检查 Root 消息泵、实际超时分类、事件借用、迟到效果与失败清理。诊断 NTSTATUS 成功仅表示诊断入口完成；实际 delivery=false、LastError=1460 和耗时独立记录，`error=IO_PENDING` 明确只是回复未知的分类。

Native coordinator NDEBUG、176B 诊断帧／128B live 帧、两张 9 槽 Unix 表和 watch v2 匹配。仅重建受影响 coordinator、PE watch 和客户端，Main／init／Cocoa 对象逐字复用；9 组有效编译闭包、122 项输入及最终产物另行绑定。独立解析原始帧／session／EXE，验证超时在途、实际晚安装、唯一取消及释放顺序；另核对 16 PE／2 Native／F27、5 份完整清理、原瓶字节或缺失恢复及四保护摘要。详见[证据](evidence/r-v1-host-status-timeout-20261101.json)。

[公开夹具](../../tests/graphics/host-status-timeout/)复用 117 的六文件诊断补丁／spec 及 112 的 live include；PE include 虽相同，因引号 header 搜索规则需与新 176B header 同目录，避免误用旧 160B 帧。六文件精确重建完成。3 棵临时整树、2 份前缀 EXE 差异归档清理，普通工作基底、源代码、原始结果和必要组件保留。

## 接续边界

实际 USER 超时／晚安装／后续取消的这个同进程跨线程场景已验收；117 则单独覆盖取消时安装仍在途，不能把二者冒称为全部超时／取消组合。没有跨进程、任意重入、像素、GPU、性能、Windows 原生或游戏结论。继续当前阶段剩余重入／legacy 交接的实际运行，再按原计划推进多进程资源消费者、事件 reparent 和两三进程像素。普通实现、默认引擎和游戏门禁不改，Windows 无操作。
