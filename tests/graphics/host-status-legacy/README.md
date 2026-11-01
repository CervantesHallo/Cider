# 旧接口资源与 managed 交接

接 0027 普通实现，复用 [host-status-live](../host-status-live/) 的 fixture.h、native.inc、pe.inc、cocoa.inc、probe.h。include 顺序先本目录后 live；应用五文件 diagnostic.patch，完整 spec／`-Fwinemac.drv`，两张 Unix 表各 9 槽、watch v2；live 帧 128B，新帧 192B/v1，Native coordinator NDEBUG。

默认客户端通过普通 API 以真实 context 创建 generation=0 的旧接口图层，实际 attached／ledger／selection 各 1。随后为同一 context 通过普通 USER PREPARE／INSTALL 安装 managed ticket，要求旧图层／登记／选择均为 0，新图层／登记／选择均为 1。真实 checked legacy retirement 的完成回调和独立实际模型观察再次确认旧资源消失，同时新图层及原 context 保持。

`-DCIDER_LEGACY_ONLY=1` 不发送 managed PREPARE／INSTALL／CANCEL：实际 producer SAFE 后旧图层仍各 1、context 仍持有，证明该许可没有覆盖无 Native owner 的旧接口模型。然后必须实际移除旧模型，才允许释放 context。

CREATE 在任何导入效果前锁存独立旧接口责任。DROP 只用普通 checked deferred retirement 的真实回调及实际模型缺失观察确认；回调在 Cocoa 上只更新标量，不做 NT／PE 操作。CLEANUP 只有在该责任闭合后才调用既有 managed／producer 清理。窗口／context 原始所有者始终保留，没有额外 context 引用掩盖错误释放；实际缺失许可先锁存，避免关闭观察窗口后丢失已经确认的事实。

独立源审无 P1／P2，实际双 PE：默认各 30／0，legacy-only 各 22／0，CLI0。实际原始帧／session／组件、9 份完整外层清理、原字节恢复及四保护摘要另行绑定。见 research/120。没有旧错误版运行负对照，不扩大为 busy／重入交接、Retina、全部 renderer、跨进程、像素／GPU／性能、游戏或默认采用。
