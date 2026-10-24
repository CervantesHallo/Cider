# U 归因前置：验证线程 CPU 来源，而不改旧结论

日期：2026-10-24。承接 89。只操作本机自有进程，没有新 Wine 基准、调度修改或 Windows 操作。

## 身份与单位

当前 Wine 的 Apple `get_unix_tid()` 是 Mach 端口名字，不能直接当全局线程 ID。SDK 还区分 `THREAD_IDENTIFIER_INFO.thread_id` 和 `thread_handle`。Apple 的测试把前者交给 `PROC_PIDTHREADID64INFO`（15）；内核也将 15 与传统选择器 5 分开解释。[Apple 测试](https://github.com/apple-oss-distributions/xnu/blob/main/tests/proc_info.c)、[选择器实现](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/proc_info.c)。

公开内核源码将 basic info 的秒／微秒换算为 libproc 字段的纳秒单位；单位不等于纳秒精度。[单位转换](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c)。这不是对本机整个内核版本的逐字证明，实际接口仍另行验证。

## 实际限定实验

`tests/sync/darwin-thread-cpu.c` 用两个自有 pthread：一个计算、一个休眠。每个线程核对 pthread 与 Mach 的 kernel64 ID，CPU 基线都完成后才释放第二道屏障，使用同一 200ms 工作期限。线程在主线程完成跨线程只读查询前不退出；随后 join，并核对自有 Mach 发送权释放。内部 alarm 5 秒、外层 8 秒限时。

同一组观察记录 libproc 15、Mach extended info、线程时钟与进程 rusage。额外明确以 basic info 的秒／微秒转换作单位控制。它不是旧 U 基准、裸 API 延迟、阻塞时间线或任意进程的计量验收。

第二版观察的普通构建：

| 宿主构建 | 角色 | libproc CPU ms | basic 显式单位 CPU ms | CLOCK_THREAD 输出 ms |
|---|---|---:|---:|---:|
| arm64 | busy | 195.862 | 195.864 | 195.861416 |
| arm64 | sleep | 0.022 | 0.024 | 0.022375 |
| x86_64／Rosetta | busy | 198.467 | 198.474 | 4.763197 |
| x86_64／Rosetta | sleep | 0.030 | 0.038 | 0.000722 |

原组合实验要求三种 CPU 来源都通过 busy/sleep 检查，**x86_64 的组合实验失败**。原第一版失败和第二版带单位观察的失败均保留；没有删除线程时钟检查、改门槛或声称组合通过。libproc／Mach 在这两架构的独立子检查通过，不能将它们提升为尚未执行的 Wine 集成或正式性能验收。

两架构故意以进程 rusage 代替线程 CPU 的独立编译变体均被拒绝。基线屏障确保两个 CPU 起点先于计算，避免先后启动让进程计数假装成线程计数。替身只用于验证 oracle，不改变真实系统计数。

## 解释边界与下一步

Rosetta 观察呈现出明显比例差异。Libc 公开实现会使用 `__thread_selfusage` 及 timebase 转换；本机两个架构读取的 timebase 也已留存。[Libc 实现](https://github.com/apple-oss-distributions/Libc/blob/main/gen/clock_gettime.c)。这支持继续调查转换边界，尚不是已证明的 Rosetta 内部根因；不通过手填换算系数修正结果。

旧 90 轮 U 基准没有调用这个线程 CPU 时钟，因此本轮异常不解释原 8/15 越界。旧门槛和全部回执保持。后续优先以明确 kernel64 身份的 libproc 15 准备只读诊断计量；必须正确进入 Wine 宿主上下文，并绑定同一 worker 的采样边界。原 GetThreadTimes 的当前线程进程回退、跨线程／退出后的完整 Windows 契约仍单列待修，不以自线程专用分支冒充完整实现。
