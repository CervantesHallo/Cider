# 跨位宽 USER 消息与进程租约夹具

用于固定 Wine 11.18 的 0021 候选。通过实际 Win32 根窗口、相反位宽的独立进程和真实进程句柄，检查固定96字节消息、主动撤销、进程退出以及旧代次拒绝。它不覆盖 CAContext 的跨进程呈现、CEF、屏幕像素或游戏。

## 准备与运行

1. 在已应用 0020 及其前置补丁的私有源码副本应用 0021，再应用本目录 `diagnostic.patch`；将 `probe.h` 放入两种 PE 和 native 的 include 路径。诊断只追加只读48字节 observer、PE owner 计数和初始高位身份种子；原六个 Unix 调用保持顺序，私有 probe 为第6号，两张调用表计数均为7。
2. 成组构建 native `win32u.so`、`winemac.so` 和两种 PE 的 `winemac.drv`。私有驱动导出用 `winemac-fixture.spec`，winebuild 使用 `-Fwinemac.drv`（winegcc：`-Wb,-F,winemac.drv`）。所有组件必须与同一普通/诊断源码组绑定，不能混旧模块。
3. 分别使用两种 MinGW GCC 编译 `client.c`：`gcc -std=gnu11 -O2 -Wall -Wextra -Werror -I<普通源码>/include client.c -luser32 -o transport-probe.exe`。
4. 通过私有 Cider GUI/XPC 实验瓶运行主 EXE，唯一参数是相反位宽 EXE 的绝对 Windows 路径。依次运行64→32和32→64两个方向。不要直接派生 Wine，也不要使用用户的 Steam 瓶子。

诊断引擎不能安装为用户默认引擎。只读计数包含已退役但仍有引用的 PE watch，以及正在删除的 timer owner；它们不是操作系统全部异步回调已经静止或完整堆泄漏证明。

## 完整成功要求

每个方向主进程68检查、3个 case、零失败、CLI0；子进程实际位宽必须相反，检查数依次15/13/26，退出码依次0/77/0。

- case1：子进程活着时主动释放，清理到空后再次确认子进程仍活着。
- case2：持有租约且真实进程仍存活时，observer 确认指定租约 active/watched；随后终止该受控子进程，实际进程对象发出退出信号，完成 outcome 为 EXITED。
- case3：下一 producer generation 增加 `2^32`，低32位相同；旧 acquire/release 被拒，新租约不受影响，释放保持幂等。
- 四个唯一租约均至少为 `1<<33`，完成 outcome 为1次 EXITED、3次 CANCELED，error0。幂等重复交付必须一致。
- 每个子进程必须记录真实 kernel wait 清理回执。截止时间前资源归零；超时后才变空不能追认成功。
- 核对 session/EXE/全部成组模块、致命异常和完整清理回执；GUI实际退出、至少两次空观察、无pending后恢复普通实验配置/DLL。

本夹具没有主动注入 STOP 丢回执、NtClose 失败或 worker 非预期终止；普通运行通过不能代替这些故障恢复验收。PID/TID 是消息来源观察，不是安全认证边界。证据和确切范围见 research99。
