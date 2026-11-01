# 旧对象使用中拒绝较新交接

接 0027 普通实现，复用 [host-status-live](../host-status-live/) 的相同 include。先 include 本目录再 live，应用五文件 diagnostic.patch；完整 spec／`-Fwinemac.drv`，两张 Unix 表各 9 槽，watch v2，live 帧 128B／新帧 256B/v1，Native coordinator NDEBUG。

同一真实 context 的 caller-assigned unmanaged A/B 标签不是 Native ticket。先普通创建并选中 A，再普通更新 A；真实 setSourceFrame 中的 hook 观察 users=1 后直接嵌套普通 Cocoa 创建 B。B 尝试移除 unmanaged 前任 A，但实际 users pin 阻止移除；B 返回 false 并回滚，A 仍图层／登记／选择各 1，retire_requested 从 0 变 1。外层更新归还最后一次真实 use 后，A 才移除，最终两者均 0。

没有额外 retain、伪造 use 或 managed callback。调用方仍持有原 context／window，实际 ProducerSAFE 及两个 checked retirement／独立模型缺失观察后才释放；Cocoa 仅普通模型调用与观察，无 NT／PE 工作。source v2 仅更正六个客户端诊断名称，其余组装与 v1 字节相同。

独立源审无 P1／P2，实际双 PE 各25／0、CLI0，原始帧／session／EXE、成组组件、5 份完整清理、恢复及四保护摘要另行绑定，见 research/123。没有本轮错误分支运行对照，不推广为所有重入、Native 异步更新／ticket管线、跨进程、像素、性能、游戏或默认采用。
