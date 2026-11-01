# 真实 SetParent 自动撤销与移回不复活

Native／PE组与 [cross-resource](../cross-resource/) 逐字相同，private160-v2／Unix8／watch2，不改普通驱动。应用相同五文件 diagnostic.patch／完整spec；本目录仅客户端及pipe操作扩展。host拥有两个真实Root和childslot，producer另进程实际导出context／读RO。

四握手：实际INSTALL/host模型1；producer仍ACTIVE／context持有时，host普通SetParent把child从Root1移到Root2；真实驱动父变更回调自动退休旧Root1消费关系、模型0及HostSAFE；再普通SetParent移回Root1，旧ticket／模型仍0；producerRELEASE／ProducerSAFE／清理后最终缺失确认。全程没有LIVE_CANCEL、手动refresh、RootChanged注入或重发INSTALL。

每次SetParent先记录attempt，再保存实际返回旧parent、GetLastError、实际GetParent/GA_ROOT和两个Root／child的PID，最后才验收耗时／输出。成功时GetLastError实际为5，原样保留；非零预期旧parent和真实树变化才是成功依据。移树前失效旧last_absent，不能用之前缺失释放新状态的观察者。

实际64→32及32→64各host64/0、producer30/0、CLI0，真实kerneljoin/exit0无强制。Root1/CID/lease/ticket/出生时间全程不变，producer原context保持直到实际SAFE。独立源审无P1/P2，完整原始记录／5清理／恢复／保护绑定见research/126。

这是宿主自有childslot，非producer HWND foreign-child前端；不证明物理鼠标／键盘、Root2全部模型、任意竞态、像素／GPU／性能／游戏或默认采用。
