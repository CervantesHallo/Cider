# Root-local 宿主协调器夹具

检查 0020 的根实例/宿主租约、替换和异步撤销。在**一次性私有实验进程**中使用真实 Win32 窗口、CAContext 和 Cocoa model layer；它不是屏幕像素、跨进程传输、真实 Windows 或游戏验收。

## 准备

1. 在固定 Wine 11.18 及 0011、0015–0017、0019–0020 的独立源码副本应用 `diagnostic.patch`。本目录加入 native/PE include 路径，复制 `coordinator-fixture.h`；其余 include 指向本目录。
2. 成组构建 native `winemac.so` 与两种 PE 的 `winemac.drv`。私有导出用 `winemac-fixture.spec`，显式传 winebuild `-Fwinemac.drv`（winegcc 为 `-Wb,-F,winemac.drv`），不能把选项插到 `-o` 和输出文件之间。
3. 两种 MinGW GCC 分别编译：`gcc -std=gnu11 -O2 -Wall -Wextra -Werror client.c -luser32 -o coordinator-probe.exe`。通过私有 Cider GUI/XPC 实验瓶运行，无参数。所有模块和客户端必须来自同一源码组；不直接派生 Wine。

该诊断会把本进程的身份序号推进到高32位非零，再在最后故意耗尽它，且不复位；还有一次明确的宿主分配点失败。**不能安装到用户/普通引擎，不得复用进程执行其他工作。** 原正常 HWND 构造路径保留；测试 producer 不发送旧的未限定 CREATE/RELEASE 消息。

## 完整成功要求

- 32 项检查、9 次计数 model snapshot，零失败，CLI 0，窗口清理成功。
- epoch 与三份 ticket 都至少为 `1<<33` 且互不相同，PE32 必须保留完整高位。
- 两个诊断故障各消费一次；失败替换保留旧宿主，成功替换退役前项；旧 ticket 更新/释放和人为迟到 job 不能修改新宿主。
- 关闭/重新打开根 authority 产生不同 epoch；耗尽时撤销旧 job 和可见宿主，实际排队 job 的 finally 释放后根引用归于仅剩注册所有者。

model snapshot 只读取真实 Cocoa 对象的几何、代次和数量。它不证明显示像素、颜色、任意并发、合成器提交、硬件内存完全释放或完整进程租约。

## 修复前对照

`exhaustion-before.patch` 故意恢复错误的耗尽分支，只用于独立负例，不能叠加到正常引擎。它在同一夹具中应精确失败检查27、28、29、30：共32项、4失败、CLI 1，其余故障与清理记录仍完整。任意提前错误或少做检查都不能算作该负例通过。

诊断补丁的 include 可展开以逐字重建本轮实际翻译单元；公开源码和原始运行的摘要绑定见 research98。每批还需核对 session、模块、异常、真实 GUI 退出及至少两次空观察，恢复普通实验配置，并及时归档变化模块/清理冗余整树。
