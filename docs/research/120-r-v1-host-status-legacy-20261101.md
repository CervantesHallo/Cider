# R v1：旧接口资源与 managed 交接

日期：2026-11-01，UTC+8。接续[119](119-r-v1-host-status-attached-cancel-20261101.md)，补 110 中尚缺的普通 legacy 交接运行，0027 普通实现保持。

## 实际交接与独立责任

普通旧接口以真实导出的 CAContext 创建 generation=0 图层，实际 attached／ledger／selection 各 1。随后为同一 context 通过普通 USER PREPARE／INSTALL 创建 managed ticket；返回后独立 Cocoa 观察证实旧图层／登记／选择各 0、新图层／登记／选择各 1，managed 标志为真。旧责任的 checked retirement 真实回调及另一次实际缺失观察确认旧模型消失，原 context 和新图层仍然存在。

另一用例不发送 managed PREPARE／INSTALL／CANCEL。实际释放 producer、读取 ProducerSAFE 后，旧接口图层／登记／选择仍各 1，context 仍持有。这说明 producer 完成页没有覆盖不受其 Native 资源计数管理的旧接口模型，不能单凭该 SAFE 释放 context。

夹具为旧接口保留独立责任：导入效果发生前先锁存 attempted；只有普通 checked deferred retirement 实际回调确认、且独立实际模型观察均为零后，才允许进入既有 managed／producer 清理。窗口和 context 的原始所有者一直保留，没有增加额外 context 引用掩盖错误释放。实际缺失许可在关闭观察窗口前锁存，不把关闭后的无观察结果当作完成。

| 实际用例 | PE64 | PE32 |
|---|---|---|
| 旧 generation=0 → 同 context 的 managed ticket | 30 检查、0 失败、CLI0 | 30 检查、0 失败、CLI0 |
| ProducerSAFE 时旧模型仍存在，独立移除后才释放 | 22 检查、0 失败、CLI0 | 22 检查、0 失败、CLI0 |

## 证据

限定独立源审无 P1／P2，尤其检查异步回调与窗口寿命、未知导入效果、关闭前许可锁存、串行控制及 CREATE 前失败清理。Native coordinator NDEBUG、192B 新帧／128B live 帧、两张 9 槽 Unix 表和 watch v2 匹配；两 PE／两模式严格编译，11 组有效依赖闭包、124 项输入和最终组件绑定。10 条 Cocoa 旧警告的消息及源语句一致。

独立读取 4 份原始日志／session／EXE，确认旧模型存在、实际交接、producer 许可边界及最后释放；另核对 16 PE／2 Native／F27、9 份完整进程清理、两批原瓶字节或缺失恢复及四保护摘要。五文件诊断精确重建，[公开夹具](../../tests/graphics/host-status-legacy/)复用 112 的相同 include，详见[证据](evidence/r-v1-host-status-legacy-20261101.json)。4 棵临时整树、4 份前缀 EXE 已按差异归档清理，源码、原始结果、必要组件及普通工作基底保留。

## 后续边界

这是一个 Root／context 的普通交接及独立旧责任测试，没有旧错误版运行负对照，不证明 busy／重入交接、全部对象更新 pin、新旧代次竞争或 Retina。继续补这些当前阶段剩余运行证据，然后按原顺序推进多进程资源消费者、事件 reparent、两三进程像素。没有跨进程、GPU／compositor、性能、Windows 原生或游戏结论；默认引擎、评级和门禁保持，Windows 无操作。
