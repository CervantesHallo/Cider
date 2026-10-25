# MF allocator 最后设备所有者

`mf-allocator-lifetime.c` 在真实 D3D11 设备上创建两个缓存共享 RGB32 样本，分别覆盖普通共享与 keyed mutex。它不使用 SourceReader、窗口或测试 MFT。

先确认缓存样本数为 2，再释放调用者的设备与 manager 引用；两者返回计数必须恰好为 1，才满足“allocator 持有最后 manager 所有者”的前提。最终释放 allocator，必须正常完成两条路径，共 46 项检查、0 失败、退出 0。此计数前提绑定当前 Highball 弱父设备模型，不作为任意 D3D 实现的通用 COM 计数规范。

旧释放顺序的实际对照在两种 PE 架构均于最终释放访问异常，探针退出 117；新顺序均通过。异常观察器不恢复执行；递归异常与独立 watchdog 直接终止，避免在故障路径依赖 CRT 日志。编译使用原 MF 测试依赖，启用 Werror。

这是缓存样本销毁顺序的限定证明。异步 result 仍持有样本时，单改 allocator 不够；还需研究 94 中的 reader/rtworkq 生命周期修复。原始失败和结果见[研究 94](../../docs/research/94-r-v1-gpu-teardown-20261025.md)。
