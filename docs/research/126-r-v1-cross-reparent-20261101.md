# R v1：真实父窗口变更自动退休旧消费关系

日期：2026-11-01，UTC+8。接续[125](125-r-v1-cross-resource-20261101.md)，按原顺序进入父变更／reparent回调，Native／PE组逐字复用，不手动代替驱动动作。

host创建两个自有Root和childslot，producer另进程准备真实context／RO及ticket，普通INSTALL后Root1实际模型三项1。producer仍ACTIVE且context／读方持有时，请求host普通SetParent到Root2；实际前端pSetParent及old/new RootChanged链自动退休旧消费关系，Root1模型0、HostROSAFE。再普通SetParent移回Root1，旧模型／ticket不复活。没有CANCEL、手动refresh、RootChanged注入或第二次INSTALL。

| 实际方向 | 宿主 | 生产者 | 结果 |
|---|---|---|---|
| PE64 host → PE32 producer | 64检查、0失败 | 30检查、0失败 | CLI0、kerneljoin/exit0，无强制 |
| PE32 host → PE64 producer | 64检查、0失败 | 30检查、0失败 | 同左 |

四握手始终绑定原Root1／child数值、CID、lease／ticket、实际PID／birth及nonce／递增序号。SetParent的原始旧parent返回值、实际GetParent／GA_ROOT／PID和效果账本先保存，再做耗时／I/O验收；移树前失效先前last_absent。成功调用的GetLastError实际为5，保留而不误当失败，非零预期旧parent返回与实际树才是依据。实际HostSAFE后producer仍持有context，ProducerSAFE及最终清理后hostfresh probe再归还观察引用。

限定独立增量源审无P1/P2，严格双PE客户端编译，Native／PE字节沿用125；private160-v2／pipe200-v1／Unix8／watch2/NDEBUG。9组有效依赖闭包／136项输入、双EXE／session／两个实际进程／真实树变化／模型／RO另行绑定；16PE／2Native／F27、5份完整进程清理、原瓶字节或缺失恢复及四保护摘要独立核对。五文件诊断精确重建，见[夹具](../../tests/graphics/cross-reparent/)及[证据](evidence/r-v1-cross-reparent-20261101.json)。3棵临时整树、2份前缀EXE差异清理，源码／完整raw／组件／普通基底保留。

这不是producer自有HWND foreign-child路由、物理鼠标／键盘投递或Root2全部模型普查，也不证明任意reparent竞态／错误退出。下一进入真实两三进程像素／呈现证据；全局事件／更广故障覆盖继续留在计划，不能把父变更回调冒称全部输入完成。默认／评级／门禁保持，Windows参考机无操作。
