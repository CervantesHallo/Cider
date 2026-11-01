# R v1：使用中的旧对象不能被交接孤立

日期：2026-11-01，UTC+8。接续[122](122-r-v1-cocoa-generation-20261101.md)，继续当前模型责任阶段，普通产品实现不改。

先用真实 context 普通创建、选中 unmanaged A，再通过普通 Cocoa 更新方法进入实际 users pin。setter 内记录 A 的 users=1、图层／登记／选择各 1，接着直接重入同一普通模型方法尝试较新 B。两标签是调用方赋值，不是 Native ticket，也没有伪造 managed commit 或 Native job。

B 的真实前任移除尝试因为 A 在用而不能完成；B 实际返回 false 并回滚。重入返回时 A 仍三项各 1，B 三项各 0，A 的 retire_requested 从 0 变 1。原更新继续安全使用 A，最后普通 use 归还才实际移除它，最终 A/B 均为 0。没有额外对象／context retain 掩盖提前释放。

| 实际用例 | PE64 | PE32 |
|---|---|---|
| A 使用中尝试 B → 拒绝交接 → 最后使用移除 A | 25 检查、0 失败、CLI0 | 25 检查、0 失败、CLI0 |

原 context 在整个过程中持有，实际 ProducerSAFE 之后两个独立责任仍通过真实 checked retirement 回调和实际两模型缺失观察闭合，再解除 Cocoa 借用、释放原 context／读方／窗口。不能仅凭新导入失败或当前图层为零就丢弃原责任。

限定独立源审无 P1／P2，最终 v2 仅更正六个诊断名称；Native／PE 与初稿字节相同，客户端重建。Native coordinator NDEBUG、256B 帧、两张 9 槽 Unix 表、watch v2 配套；9 组有效编译闭包／138 项输入及实际产物绑定，10 条 Cocoa 旧警告语句与消息一致。

独立解析原始 users／retire_requested、重入前后实际模型、真实返回值、原始 session／EXE；另核对 16 PE／2 Native／F27、5 份完整清理、原瓶字节或缺失恢复及四保护摘要。五文件诊断精确重建，见[夹具](../../tests/graphics/host-busy/)与[证据](evidence/r-v1-busy-predecessor-20261101.json)。3 棵临时整树、2 份前缀 EXE 差异归档清理，源码／原始结果／组件和普通基底保留。

只证明这个 serialized unmanaged Cocoa 更新／嵌套导入场景，没有本轮错误分支运行对照，不推广为所有 legacy guards、Native prepared ticket／异步更新任务管线、跨进程、像素／GPU／compositor、性能或游戏。当前下一项是 Native 异步更新任务的实际调度／完成，再按原顺序推进多进程资源消费者、事件 reparent 和两三进程像素；默认／评级／门禁保持，Windows 无操作。
