# 可行性复核与第一批实现证据（2026-10-06）

本轮接续 [内核契约调研](23-kernel-feasibility-20261006.md) 与 [窗口坐标调研](24-winemac-frame-feasibility-20261006.md)。研究确认了可以独立推进的通用兼容工作，以及当前架构还不能兑现的契约。下列实现属于 HY1；没有把原神、崩铁、绝区零标为可玩，也没有加载或修改其反作弊驱动。

## 1 架构取舍

| 候选 | 已有证据 | 本轮决定与限制 |
|---|---|---|
| Engine R：Wine / Rosetta | 当前引擎可运行启动器；KMDF 与 WDM 的通用对象、队列、取消及权限行为有公开契约。同步进程/对象回调还缺裁决协议，宿主访问边界和游戏接受度仍未知，见 23 | 保持主线；先处理可独立复现的窗口和生命周期，再取得 Windows API 对照。未采用成功空桩或隐藏 Wine |
| Windows ARM 虚拟机 | 只读 PE 头确认当前原神安装的 `HoYoKProtect.sys` 为 `0x8664`（x86_64）。Windows ARM 的应用仿真不能运行 x64 内核驱动，驱动须为 ARM64 | 不是这份安装的直接替代方案；未找到这份国服版本的官方 ARM64 驱动证据，未安装虚拟机 |
| 完整 x86 系统仿真 | QEMU 支持全系统 CPU 仿真；能否满足本机性能、GPU 功能及游戏验证没有实测证据 | 保留为 Windows 契约实验环境的候选；未采用为游戏主线，也未承诺可玩 |
| macOS 宿主组件 | DriverKit 是用户空间驱动框架；EndpointSecurity 提供部分宿主事件监测与阻断，需相应 entitlement | 不等同 Windows 内核 ABI 或整套安全契约；未安装宿主特权组件、未更改安全设置 |

依据：[Windows ARM FAQ](https://learn.microsoft.com/en-us/windows/arm/faq)、[QEMU 系统仿真](https://www.qemu.org/docs/master/about/emulation.html)、[Apple System Extensions](https://developer.apple.com/system-extensions/)。架构差异不意味着所有通用 Windows API 永远不能实现；是否完成以各自契约证据判定。

权限研究纠正了一个可能误导实现的假设：微软明确规定 `SePrivilegeCheck` 的 KernelMode 分支返回 TRUE。应验证 UserMode 的令牌、启用状态与输出，不能为了“诚实”把所有调用统一改为失败。历史审计矩阵的标签须按模式和指定源码版本复核，未将调研结论自动写成引擎已修复。[Microsoft 契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seprivilegecheck)

## 2 L5 独立窗口实验

环境：Apple M3 / 8 GB、macOS 26.7、x86_64 Wine / Rosetta、macOS 15.4 SDK。`tests/windows/custom-frame.c` 是自有 GDI 程序；保留 `WS_OVERLAPPEDWINDOW`，分别使用默认非客户区、完整自绘客户区，以及顶部不内缩而左右/底部内缩 8 px 的部分自绘客户区。创建后用 `SWP_FRAMECHANGED` 重算。

使用全新 `FrameLab` 瓶子和单独克隆的引擎文件。没有复制用户的 Steam 登录瓶子，也没有替换用户安装的 r1 引擎。实验路径为临时 `CIDER_HOME`，日志会话 ID 如下：

- 基线：`framelab-e9fd/20261006T132549-cider-custom-frame`。
- 加入 Cider 0003 / 0004：`framelab-e9fd/20261006T133326-cider-custom-frame`。

稳定后多次采样，坐标为屏幕坐标，单位为本次 Wine 逻辑像素：

| 窗口 | window | client | 基线 visible | 实验补丁 visible |
|---|---|---|---|---|
| 默认非客户区 | (40,100)–(360,380) | (44,130)–(356,376) | (44,130)–(356,376) | 相同 |
| 完整自绘 | (400,100)–(720,380) | 与 window 相同 | 与 window 相同 | 相同 |
| 部分自绘 | (760,100)–(1080,380) | (768,100)–(1072,372) | (764,130)–(1076,376) | (764,100)–(1076,376) |

部分自绘基线的 client 从 y=100 开始，而宿主 visible 从 y=130 开始，形成 30 px 裁切。0004 将客户区在窗口内部的部分并入 visible，修正了这份独立 GDI 复现。由此证实 Highball 远程 swapchain 不是这一类缺陷的必要原因；不能由此断言米哈游启动器必定具有同一组矩形。

补丁：

- `0003-winemac-initialize-style-masks.patch`：纠正 `*style_mask = ex_style = 0`，初始化实际的 `*ex_style_mask`。独立确定缺陷。
- `0004-win32u-preserve-custom-client-area.patch`：保留 NCCALCSIZE 得出的、位于 window 内部的客户区，不按应用名分支、不清除 Windows caption 样式。
- `engine/recipes/cider-cx26-r2.json`：实验配方，尚未发布；r1 配方和下载索引保持 r1。实验编译只构建两个 native 模块，使用 macOS 15.4 SDK。

实验 native 模块 SHA-256：

```text
winemac.so 0be7b47a313a2a82b6851d93f575794c50be98bcba21767db636c0d8278fe6db
win32u.so  e9f1b415732379063c14850427311eae59a8ad7d07c761039299b4474d78eea8
```

实验完成后，原 r1 缓存源码从备份恢复，两个缓存构建模块重新构建为 r1；两份新补丁在恢复后的源码上 dry-run 可应用。临时实验引擎保留用于复查，临时瓶子已停止。

静态对抗性审查提出 P2：原生缩放触发 `WINDOW_FRAME_CHANGED` 时若 NCCALCSIZE 同时改变 visible 原点，宿主位置同步可能被跳过；默认 NC → 部分自绘 NC 的动态切换需验证外框、绘制和点击的一致性。尚未动态复现，也未宣称 0004 已消除此路径。

尚未验收：启动器稳态矩形和视觉效果、顶部点击坐标、拖动/缩放、1×/2×、菜单/tool/layered/shaped 窗、全屏 present、同进程/跨进程 DXMT 及嵌套层裁剪。补丁对非客户区的影响仍须这组回归，实验坐标通过不等于可以发布。

## 3 L6 生命周期实现

实现 `AppLifecycle`，由新鲜扫描结果决定启动、停止和重启。重复启动返回已有实例；停止轮询实际退出和辅助进程替换，超过有界次数则报失败；停止失败不继续重启。

进程归属使用瓶子 ID 和准确 prefix；普通程序仅匹配主程序和 profile 显式列出的辅助程序，辅助程序还须位于同一应用目录中。默认排除 `games` / `steamapps` 子库，避免停止启动器时误停游戏；不把同目录内的另一个独立程序当成辅助进程。Mac 路径启动项映射为 Wine 的 C:/Z:/dosdevices 路径后匹配。

米哈游 profile revision 2 增加 HYP/HYPHelper 启动入口别名，以及 HYP/HYPHelper/HYSafeMode 进程名单。实机 Wine argv 中 `HYPHelper` 没有 `.exe`，因此仅对显式声明的 `.exe` 辅助程序接受对应无后缀名；`.bak` 等不匹配。跨进程 DXMT 配置从 `BottleStore` 的兼容数据传给 runner，实机 HYP 环境确认 `DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1`。

`WineRunner.killAll` 检查 wineserver 退出状态，接受 Wine 在无 server 时返回的 1，随后清理 tagged 孤儿与尚未改写 argv 的 Wine loader，并确认 prefix 退出。终止扫描记录出生时间，不对无法验证的身份发信号；TERM/KILL 后观察退出，不将信号发出直接当作成功。

当前 macOS 使用从内核读出的真实 audit token 与动态解析的 `proc_signal_with_audittoken`，由内核在持有进程引用时验证 PID version。本机自有 `/bin/sleep` 子进程验证：真实身份 SIGCONT 返回 0，过期 pidversion 返回 ESRCH 且子进程仍存活。旧 macOS 缺少该 API 时回退为出生时间复查加 kill，仍有复查到发信号间的竞争窗口，未声称消除。[Apple 实现](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/proc_info.c)

应用中的启动/停止/设置/安装准备共享瓶子 activity 锁。独立安装器成功 spawn 后释放 activity，允许停止挂起的安装器。配方 worker 支持取消；下载以 posix_spawn 创建本任务自己的 host 子进程，由本任务 waitpid 管理且在退出前不释放 PID；取消先 TERM，1 秒后可 KILL，3 秒仍未确认退出则明确报错并保留迟到退出的回收，步骤间检查取消；停止等待 worker 收敛后才提示完成；瓶子页在下载阶段也显示“取消安装”，不要求已有 Windows 进程。锁交接使用 owner 和完成标记，重复点击 Stop 去重，迟到的安装回调不能释放另一个操作的锁或自动继续启动。

边界：外部 CLI 与 GUI 尚无跨进程操作锁；未声明的第三方辅助程序不自动归属；下载取消不等待无限的 TERM，忽略 TERM 的自有子进程已通过强制退出检查；若 OS 延迟确认退出仍报告失败，未宣称任何宿主故障都能成功终止。停止失败与安装 defer 前后交错的状态仍需故障注入验证。Wine 裸进程当前不能被界面自动化按应用识别（L4），需要正式 host bundle 才能完成启动器视觉自动验收。

## 4 对抗性审查与验证

审查发现并修正：

| 问题 | 修正与检查 |
|---|---|
| 同目录独立程序被启动器归属 | 精确主程序 + 显式 helper；同目录反例检查 |
| 无 wineserver 时直接失败，跳过孤儿清理 | 接受对应退出码后继续扫描；临时空闲 FrameLab kill 返回 stopped |
| Mac 路径启动项无法匹配 Wine image | dosdevices 映射；瓶子 C: 与外部 Z: 检查 |
| 设置/运行命令与重启并发 | 应用内统一 activity 锁；外部 CLI 边界保留 |
| 复查 PID 后可能被复用 | 当前宿主使用内核原子信号接口；旧宿主竞争窗口明确保留 |
| 新建配方瓶子无 activity 锁、安装器挂起不可 Stop | 先创建，再取得新 ID 的锁；独立安装器 spawn 后允许停止，配方取消并等待收敛 |
| 无后缀 HYPHelper 和 HYSafeMode 被遗漏 | 依据本机实际进程增加精确匹配及反例 |
| 下载子进程忽略 TERM 导致取消无限等待 | 自有 unreaped 子进程、TERM/KILL、超时明确失败、Stop 保留异常；忽略 TERM 的独立检查通过 |
| 动态 NC 切换可能跳过 Cocoa 原点同步 | 保留 P2 与明确复现步骤；0004 继续实验状态，未安装用户引擎 |

本轮有界本地检查：40 个检查、12 个 suite 全通过（生命周期、进程身份/退出、命令取消与忽略 TERM 后强制退出、归属、路径、profile/recipe/预检/矩阵）。CiderKit、CLI、App 构建通过；debug `out/Cider.app` 打包并 ad-hoc 签名，签名验证通过。未新发 GitHub Actions 引擎构建。

授权自动化权限后，已重新加载修复版 Cider；资料库可识别 HYP 运行并显示停止按钮，右键菜单显示启动/重启/停止。重复启动提示“已在运行”，主 PID 78454 不变。随后通过右键“重启”：旧主 PID 78454 和辅助 PID 78465/78469/78471/78473 均退出，新主 PID 78916；Cider 显示“已发起重启…等待窗口就绪”。随后点击停止按钮，HYP 与辅助进程均不在扫描结果中，Cider 提示“已停止 米哈游启动器”，停止按钮消失；没有停止其他瓶子的服务。界面可操作与进程替换证据不替代启动器内容渲染验收。

额外发现：2026-09-28 的历史启动器日志为 30,763,307,262 字节，末段持续 `msync: warn: node memory pool exhausted`。本轮仅有界读取尾部，没有清除历史日志；须加入长期运行的 msync 对象回收与日志资源预算调查。不能仅据旧日志将本次白屏归因于 msync。

## 5 下一验收

1. L5 完成独立窗口交互/渲染矩阵、启动器真实矩形与视觉复测，再决定是否发布 r2。
2. L6 完成米哈游启动器实际停止/重启记录，补足 GUI/CLI 并发协调；L4 提供可识别的 Wine host 身份后补视觉自动化。
3. HY2 取得 UserMode/KernelMode 权限与 KMDF 请求生命周期的 Windows 基线；逐项实现、对照、记录失败范围，再扩展依赖组合。
4. HY3 评审版本环境证据到启动许可的闭环；依赖组合通过后才进行原神集成实验和完整本地会话，三款分别验收。
