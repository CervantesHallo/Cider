# 单窗口图像夹具

这是 research/129 的固定实验环境源码和两张实际截图，不是产品截图接口。`capture.swift`逐字保留该次构建输入，只允许本次 `/Users/cervantes/Cider/out/` 路径；移植路径须重编并建立新的源／二进制绑定。

调用形状：`capture WINDOW_ID OWNER_PID READY_FILE OUTPUT.png`。父控制器须绑定运行nonce及完整64B token、执行25秒进程超时、独立核对PNG哈希与像素，再写准确gate。工具自己限定20秒并拒绝晚完成；只使用已有屏幕权限，不请求权限或创建／激活窗口。`NSApplication.shared`用于初始化AppKit／WindowServer连接。

单窗口过滤器独立于显示器／Space，元数据分别记录onScreen前后状态。成功生成PNG本身不接受像素、当前桌面显示、物理扫描输出、输入或游戏；`analyze.py`对本次红蓝矩形做独立CRC／解压／滤波／颜色检查。现有red／blue图来自实际WindowServer捕获。
