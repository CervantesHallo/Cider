# C++ TLS 卸载对照

`library.cpp` 只使用 C++ TLS、异常状态查询和输出流；不调用 DXGI 或 Media Foundation。`probe.c` 在主线程和一个子线程调用该 DLL，再执行 `FreeLibrary`。

每个 PE 架构分别链接原 CRT 和只替换 `tlsthrd` 对象的私有 CRT。DLL 都命名为 `owned-cxx-tls.dll`，放在对应探针旁；运行时只切换这个 DLL，使用相同探针。不要放入用户瓶子。

通过要求：唯一完成记录 `unloaded=1 faults=0`、退出码 0，且完整日志无访问异常或 native 析构错误。原 CRT 的故障对照必须记录异常并退出非零。Vectored handler 只观察、不吞异常；独立 12 秒 watchdog 不经过 CRT 日志，避免故障线程持有输出锁时无法终止。两种编译均启用 `-Wall -Wextra -Werror`。

最终公开探针的两架构新旧对照已实际运行，见[研究 94](../../../docs/research/94-r-v1-gpu-teardown-20261025.md)。该最小用例不是完整 CRT 或 Windows 原生契约验收。
