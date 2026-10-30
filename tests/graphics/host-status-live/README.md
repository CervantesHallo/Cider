# 已知ticket的真实图层生命周期

接0027的普通候选。夹具通过普通v2 USER消息创建lease、预留ticket并映射真实RO页，然后实际INSTALL导出的Metal/CAContext；纯Cocoa观察器分别读取父图层、逐ticket登记及当前选择。

取消后，原始context保持持有，直到RO HostSAFE及实际图层/登记/选择均归零；再读取producer完成，最后销毁真实context、关闭本地NT读方和窗口。未知发送不重放INSTALL。清理把实际已观察的不可逆许可先锁存，避免RO读方进入关闭状态后丢失已取得的完成事实。

应用diagnostic.patch到0027普通组，增加本目录include。使用完整spec并显式-Fwinemac.drv；两张Unix表各8槽，watch2不变，128B/version1帧。客户端用MinGW GCC以-std=gnu11 -O2 -Wall -Wextra -Werror，user32/ntdll编译。通过Cider GUI/XPC隔离瓶无参数运行。

实际双PE各25项、0失败、CLI0。见research112及证据。Native/PE驱动来自build-v1已成功部分，客户端四处格式警告修后单独build-v2；未重写旧失败记录，也不放宽Werror。夹具没有故障注入、受保护NT清理、并发负例、像素/GPU/compositor或跨进程证明，不能替代仍被工具拦截的故障并发验收。
