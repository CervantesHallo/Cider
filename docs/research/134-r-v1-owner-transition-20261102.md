# R v1：真正由 renderer 持有窗口的本地／远程转换

接续[133](133-r-v1-unified-local-ownership-20261102.md)。renderer 自己创建和拥有 Root1／两个 child，controller 只拥有 Root2。先通过普通 80 字节管道 bootstrap 交换初始未知句柄，controller 实际核对 Win32 窗口 owner／父子树、真实 PI 的 PID／出生时间、nonce 及完整消息后才 echo；renderer 再创建 D3D11 设备。没有将初始 foreign 窗口冒充 local 起点。

| 方向 | 实际检查 | 转换与退出 |
|---|---|---|
| PE64 controller → PE32 renderer | 493／735，均零失败 | local Root1→foreign Root2→local Root1，CLI0、真实 kernel join／exit0 |
| PE32 controller → PE64 renderer | 493／736，均零失败 | 同上；检查数允许合法暂未就绪 Map 的实际重试差异 |

同一 device／immediate context／两份 swapchain 贯穿三个绘图阶段，真实 6 次 160×100 全像素读回／Present。WindowServer 图像红蓝→蓝红→红红；Root1 的 CGWindow ID 和实际 Native owner PID 在返回后与首帧相同，Root2 的二者不同，补上不同进程真实持有目标窗口的证据。每矩形 69,776 像素，白间隔依阶段为 40／44／40 列；客户区覆盖和两块独立位置均通过。图像宽 880 或 884、高 536，如实保留，不称外框几何完全不变。六幅[真实图片与精确源码](../../tests/graphics/renderer-owner-transition/)可核对。

源码独立复审未发现该增量具体 P1/P2，父代理读回四份新旧代码哈希；双 PE 严格构建。首轮已经完成本地首图及实际跨进程迁移，但目标窗口没有定义白背景，实际呈深色，因白间隔条件失败；原 `runtime-v1` 保留。最终仅为两个真正 owner 的窗口类指定借用的 `WHITE_BRUSH`，父代理限定复核，图像判定阈值未改；Native 实现未为这个用例改色或代画。

最后阶段由 renderer 真正销毁自己持有的 child，保留 Root1 和进程，直到外层实际观察六个精确 route 的只读许可退役和两份本地 `context0 routes0`。之后才给 ACK，renderer 再销毁 Root1／注销自己的 class／退出，controller 实际 join 后仅销毁自己的 Root2。所有窗口操作按真实 owner 执行。两方向每端 5 次管道发送／接收包含 bootstrap；bootstrap 不计作绘图阶段。这个指定两 context 的合作回收，不倒推此前 56 份旧生命周期用例均在退出前释放。

259 项实际源码输入、原始 log／session／两 EXE、精确配套 Native／PE／冻结 core、实际 bootstrap 后身份与 ready／gate、三次图像、迁移／退役、清理恢复及四个保护摘要由父代理再绑定，见[证据](evidence/r-v1-owner-transition-20261102.json)。独立源审与父绑定分别记范围，不把自己重新调用 oracle 写作新的独立全审。

这验证 0029 实验策略下的指定 local→foreign→local 与存活期间合作回收；没有证明所有 Cocoa 回调尾部、全局 GPU 空闲、任意故障、resize 图像、CEF、输入、正式性能或游戏会话。下一仍沿原图形阶段做窗口／backbuffer 更新和 resize 的实际画面，再做 CEF／启动器回归与可回退试用包。默认与游戏门禁保持，Windows 参考机未操作。
