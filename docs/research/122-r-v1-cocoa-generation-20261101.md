# R v1：旧安装恢复时不能覆盖新代次

日期：2026-11-01，UTC+8。接续[121](121-r-v1-model-use-20261101.md)，补 110 中 Cocoa 发布代次保护的实际正反对照。

真实导出 context／窗口创建后，调用方为两个 unmanaged Cocoa 模型分配高位标签 A/B，B=A+1。这些标签不是 Native ticket，没有伪造 managed commit、Native job 或完成计数。外层普通创建 A 走到真实 addSublayer，回调直接进入同一普通 Cocoa 模型方法创建 B；没有再次调用 OnMainThread 或在 Cocoa 上执行 NT／PE。

原始观察先记录 A 的真实图层／登记各 1、尚未选中；嵌套 B 返回时 A 仍部分存在，B 图层／登记／选择各 1。正常实现让 A 恢复后返回 false 并实际回滚，最终 A 各 0、B 各 1。两个实际 bool 与诊断入口完成状态分别记录，不能把 outer=false 解释为没有导入效果。

| 组 | PE64 | PE32 | 最终实际旧／新选择 |
|---|---|---|---|
| 普通代次／serial 发布保护 | 25 检查、0 失败、CLI0 | 同左 | A0／B1 |
| 仅省略两处恢复后的发布保护 | 12 检查、1 失败、CLI1，仅 CHECK12 | 同左 | A1／B0 |

错误对照先通过前 11 项，再实际产生旧模型覆盖新选择，失败不是任意超时。它只修改两处 generation／serial 发布谓词，真实导入、回调、原始 context 和退役逻辑不改；明确仅为诊断，不能采用为产品。

调用方在任何效果前保守持有 A/B 两个独立退役责任。正例中实际 ProducerSAFE 后 B 仍存在、context 仍持有。两组均通过实际 checked retirement 完成两项、独立观察两者消失并解除 Cocoa 窗口借用，之后才释放原 context／读方／窗口；内部 cleanup 均真，不依赖额外 context retain 掩盖错误。

限定独立正例／错误增量源审均接纳，错误增量接纳范围仅诊断；Native coordinator NDEBUG、256B 帧、两张 9 槽 Unix 表、watch v2 配套。固定组 9 组有效编译闭包／124 项输入，对照另绑定唯一改变的 Cocoa TU；同一 PE／客户端及其余对象字节保持。10 条 Cocoa 旧警告的消息和源语句一致。

独立重读 4 份原始日志／session／EXE、两次真实导入、重入前后模型及实际拒绝；另核对 16 PE／2 Native／F27、10 份完整清理、两批普通配置／DLL 字节或缺失恢复、四保护摘要。五文件诊断与错误谓词补丁精确重建，见[夹具](../../tests/graphics/host-generation/)和[证据](evidence/r-v1-cocoa-generation-20261101.json)。6 棵临时整树、4 份前缀 EXE 差异归档清理，原始记录／源码／必要组件／普通基底保留。

这里只证明该 unmanaged Cocoa 排序场景，不是 Native prepared ticket 并发、busy predecessor、任意替换、像素／GPU／compositor、性能、跨进程或游戏。下一继续使用中的旧对象交接及 Native 更新管线，再按原顺序多进程资源、事件 reparent、两三进程像素。默认、评级和门禁保持，Windows 无操作。
