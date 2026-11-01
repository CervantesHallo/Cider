# 真实 USER 安装超时

接 0027 普通实现和 [安装在途夹具](../host-status-overlap/)。使用后者的六文件 diagnostic.patch／spec；include 顺序为本目录、host-status-overlap、host-status-live。原 live 的 fixture.h／native.inc／pe.inc／cocoa.inc／probe.h 保持相同字节。overlap-pe.inc 虽与上例相同，仍在本目录保存：其引号 include 必须先找到本目录的 **176B** header，不能误用上例的 160B 帧。两张 Unix 表各 9 槽、watch v2、live 帧 128B，Native coordinator NDEBUG。

本例 RUN 调用普通 macdrv_remote_prepared_packet_send，保留真实 SMTO_ABORTIFHUNG／3000ms 超时。拥有 Root 的主线程泵送实际消息；普通接收函数真正取得 request/control/model_job 后，在锁外等待自有事件。发送线程实际得到 delivery=false、LastError=ERROR_TIMEOUT 和约 3001ms；先捕获真实仍在途的 guard、仍持有 context／读方、非 SAFE 的 RO 页以及模型各零，再发事件让接收方继续。

主线程从接收回调返回并实际 join 后，检查真实安装的图层／登记／选择均为 1。虽然发送方没有取得安装回复，安装已产生实际效果，因此始终保留原 ticket 和资源，不重发 INSTALL。随后真实 USER CANCEL，一直等实际模型归零及 Host／producer SAFE 后，才释放 context、读方和观察引用。

`error=IO_PENDING` 是回复未知的诊断分类；实际发送器的 false／LastError／耗时单独保存。`returned` 表示发送方返回，不能解释为接收方完成。工作线程的 guard／context 快照在发事件前捕获，在主线程 join 后输出；不把输出时刻冒充捕获时刻。

独立源审无 P1／P2，实际双 PE 各 36 项、零失败、CLI0；发送方均为实际 ERROR_TIMEOUT／3001ms，在途 witness 和晚安装效果、取消清理另行核对。编译复用已匹配的 Main／init／Cocoa 对象，独立绑定有效源码闭包、成组组件、原始 session／EXE、五份完整进程清理、普通瓶恢复及四保护摘要。见 research/118。

仅证明这个同进程跨线程真实超时及后续取消场景，不代表跨进程消费者、任意超时交错、像素／GPU／性能／游戏或默认引擎采用。
