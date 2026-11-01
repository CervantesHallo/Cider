# R v1：两进程宿主窗口的实际红蓝图像

2026-11-01，UTC+8。接续[127](127-r-v1-cross-pixel-20261101.md)、[128](128-r-v1-three-pixel-20261101.md)。用户明确授权ScreenCaptureKit只抓本项目测试窗口，保存在本机。实际双方向各host66项／0失败、producer35项／0失败；GPU红蓝各16000像素、两次实际窗口图像握手、取消／SAFE／GPU drain／资源释放／kerneljoin及exit0完整通过。

实际PNG为704×468，红帧69776个红色像素／0蓝色，蓝帧0红色／69776蓝色；相同矩形边界[22,78,377,273]、连续196行宽356像素。两个位宽方向分别真实捕获，PNG字节恰好相同。已检查保存的图片：[红帧](../../tests/graphics/window-capture/red.png)、[蓝帧](../../tests/graphics/window-capture/blue.png)。这是WindowServer合成的目标窗口图像，分别绑定实际windowID、Mac PID、Wine PID、CID和nonce，不是从GPU读回数据生成的替代图片。

此前工具错误和修正保留：源审发现路径检查／使用及晚完成缺口，改用无跟随的逐组件目录描述符、固定ready文件身份／精确64B及EOF、独占PNG提交／读回与单调截止，源审闭合。最初又错误要求目标必须在当前Space；实际Cocoa visible=1、mini=0、activeSpace=0、NSApp.hidden=0，位置有效，Mac未锁屏。按[Apple单窗口过滤器说明](https://developer.apple.com/videos/play/wwdc2022/10155/)，独立窗口捕获覆盖其他Space和遮挡／离屏窗口；因此纠正验收工具，将onScreen作为原始元数据，保留物理显示状态未验，未为此修改引擎显示策略。

首个真正捕获调用还暴露辅助程序未初始化AppKit／WindowServer连接，触发CGS_REQUIRE_INIT断言；这是截图辅助程序SIGABRT，不是Wine崩溃。MainActor先取得NSApplication.shared后实际捕获成功，没有创建／激活窗口或修改权限。所有旧failed记录保留。另一次在测试启动前读错评审schema键、一次标准Win32前台请求、一次Cider原生窗口Raise／桌面导航也分别记录，没有据此声称物理显示通过。

最终工具Swift6严格编译，独立源审及父代理单行平台初始化增量复核；只抓准确ID＋PID＋预期标题的一个窗口，无全屏／其他应用图片。父25秒硬超时／子20秒拒晚、PNG完整性及矩形重新解码、实际64B ready／gate与host ACK／原raw-session-双EXE／成组组件另行绑定；4份实际GUI/kernel清理、原配置DLL字节或缺失恢复及四保护摘要通过。源码、可见性只读观察补丁、实际示例及哈希见[夹具](../../tests/graphics/window-capture/)和[证据](evidence/r-v1-window-images-20261101.json)。

当前完成两进程WindowServer图像验收；仍不证明当前Space可见／物理扫描输出、输入、性能、foreign HWND前端或游戏。继续原三进程图像，再接真实窗口前端／CEF；默认引擎与游戏门禁不变，Windows参考机未操作。
