# R v1：修复导出图层缩放后仍保留旧尺寸

接续[134](134-r-v1-owner-transition-20261102.md)，沿同一设备／两交换链的 local→foreign→local 流程加入实际窗口和 backbuffer 放大。首次运行证明 Win32 client 已为 218×118、backbuffer 已为 200×120，全部 24,000 像素读回正确，但 WindowServer 色块仍为旧的 356×196，顶端下移 40 像素。这是 GPU 测试无法单独发现的真实前端缺口；原失败及[截图](../../tests/graphics/renderer-resize/before-fix-phase2.png)保持。

原因是导出 `CAMetalLayer` 仅在构造时设置 bounds，DXMT 的 drawableSize 更新没有同步窗口实际 bounds。0030 在 Wine 的事件／恢复线程读取当前 client rectangle，重新核对 PID／出生时间／incarnation，再由纯 Cocoa 主线程事务更新同一 layer 并实际读回确认。没有替换 device、context、swapchain，没有将 NT／PE 调用放到 Cocoa，也没有在每帧 getter 增加 USER 工作。

审查另发现一项 P2：setter 部分生效后抛异常，缓存仍为旧尺寸；若窗口也回到旧值，相等快路会永远漏恢复。现于操作前将 cache 标为不确定，仅在实际读回成功后确认；失败保留 owner，后续即使尺寸等于历史缓存也重试。此项是源码闭合，尚未实际注入该指定异常／回滚故障。原拒绝报告未改。一次 Cocoa 编译的 NSString 日志格式错误已修，原失败构建也保留；10 条既有 Cocoa 警告的消息与原源码行逐一匹配，无新增警告，不称全引擎零警告。

窗口变化另通过既有 PE 消息通道发送合并唤醒：SetParent／WindowPosChanged 释放私有 window-data 锁后，只在服务锁外异步 post；Cocoa 不 post NT 消息，不创建额外 helper，不把通知当 SAFE。已知 post 失败仍保留 250ms 周期恢复。新事件增量独立复审无具体 P1/P2；Unix 8 槽、消息和完成页格式不变。

| 最终事件版本 | 实际检查 | 实际结果 |
|---|---|---|
| PE64 controller → PE32 renderer | 521／893，均零失败 | 128,000 像素、两次 ResizeBuffers、20 次 Release、三幅新图及完整退出 |
| PE32 controller → PE64 renderer | 同上 | 同上 |

每块图像由 69,776 增长为 102,896 像素，即 436×236，位置为 `[22,78,457,313]`／`[542,78,977,313]`，第三帧红红共 205,792 像素。实际窗口图像也从 880×536 增大为 1080 或 1084×656；外框差异原样保留。三种图案、ready／gate、实际 Cocoa bounds 确认、四次迁移及六份 route 退役／两本地 freed 都是最后 ACK 前的必要条件。[六张最终 PNG 与源码](../../tests/graphics/renderer-resize/)可核对。

最终版本两方向各观察 7 次完成的 geometry 消息回调，但这两次往返中的 bounds 确认没有落在其回调作用域内，不能据此单独宣称事件负责了那些尺寸变化。另复用受影响的原本地生命周期：双 PE 各 1,415 检查／56 交换链／168 Present，零失败；分别 90／89 次完成消息回调，**各 9 次实际 bounds 确认发生在同线程的 geometry 回调内**。这是事件路径的运行见证，不是精确数量、延迟或正式性能承诺，也不要求进程退出时每个回调尾部均完成。

五个修改文件及全部 25 个候选代码文件由固定上游／已有补丁链零 fuzz、零 offset 重建。最终真实输入、配套 Native／PE／冻结 core、原始 log／session、尺寸／CRC／图像／门禁、回收和清理恢复由父代理再绑定，见[证据](evidence/r-v1-exported-resize-20261102.json)。默认引擎、游戏门禁和 Windows 状态不变。

本项限定组合窗口放大与 backbuffer resize，并保留原正常失败；不证明所有独立移动／裁剪／Retina 情形、指定异常注入、所有 Cocoa 尾部、全局 GPU 空闲、物理显示、输入、正式性能或游戏会话。下一回到真实 CEF／启动器回归和可回退试用包，仍按原图形迁移及 HY 顺序推进。
