# HY1 启动入口：工作目录与宿主启动准备

2026-10-12，用户在关闭定时续做后要求按正确方向继续。原 HY1→HY2→HY3→HY4/HY5 顺序保持。本轮从资料库、保存启动器、快捷方式和 CLI 的工作目录传递继续收尾，没有扩大安装辅助工具或解除游戏门禁。

## 发现与修正

AppModel 的 start/restart 将显式工作目录经 `try? WindowsPath.hostURL` 转換，失败变成 nil。结果是用户保存的宿主绝对路径、无效路径或已不存在的目录可以被静默丢弃；底层随后使用继承目录。CLI 同时没有显式 cwd 选项，只为宿主程序路径自动选择父目录。依赖相对资源路径的程序可能因此得到不同的请求；未复现具体 HYP 故障，不归因此前白屏或“找不到游戏文件”。

已实施：

- 共用 WindowsPath.workingDirectory 接收宿主绝对路径或 Windows 绝对盘符路径，确认确实是目录；不存在/非目录/无效请求报错，空值保持原继承行为。GUI 的保存启动器和快捷方式继续走同一 AppLifecycle；stop 不验证 cwd，restart 在停止之前验证，避免无法重新启动时先结束现有程序。
- CLI run 增加 `--cwd`，与 GUI 共用解析；显式目录优先。没有该选项时，宿主程序原有的父目录默认行为保留，普通 Windows 路径/builtin 的既有默认不变。
- WindowsPath.hostURL 原来把 `C:foo` 当根路径，且把 `..` 直接拼到宿主路径。现在要求 ASCII 盘符、冒号及根分隔符，拒绝 NUL；点分量先在该盘根内做词法归一化，根处的 `..` 不越出 drive_c/映射根。仍接受 `/` 分隔和合法盘符映射；不实现相对盘符、UNC、设备命名空间、环境变量展开或完整 Shell 路径行为。50 的配方检测规范化要求未放宽。

路径类别及 cwd 的独立语义依据：[微软路径格式](https://learn.microsoft.com/en-us/dotnet/standard/io/file-path-formats)、[IShellLinkW 工作目录](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ishelllinkw-setworkingdirectory)。宿主目录检查不是 guest CurrentDirectory 的运行证明，也不是恶意同用户进程隔离或无竞态授权。

追查到 Spawn 后确认此前 actions/attributes 初始化及 addopen/adddup2/addchdir/setflags/setpgroup 全未检查返回；strdup 失败还可能提前截断 argv/envp。现逐步检查准备返回码，只有成功初始化的对象才进入 defer 销毁；所有字符串分配检查失败并释放已分配项，再调用 posix_spawn。拒绝参数/路径/环境 NUL、空执行/日志路径及无效环境名，保留有效空参数和空环境值。正常进程组、描述符隔离、日志/标准流设置不变。

收益是显式 cwd、参数和宿主启动设置不再因准备失败悄悄降级；成本是先前被忽略的坏目录/字符串现在会明确失败，以及路径映射在有效盘符内执行点归一化。未修改底层 Windows API 返回、成功空桩或反作弊条件。

## 对抗性静态审查

按源码确认：nil/空 cwd 保持兼容；已有目录与文件区分；宿主/盘符路径分开；非法盘符/NUL/相对盘符拒绝；点分量只修改本地组件栈；缺失目录不阻止 stop、不先停止再失败。CLI 显式 cwd 不被宿主目标的父目录覆盖；参数空串保留，不展开 shell 内容。

Spawn 审查覆盖 actions/attributes 的各失败边、CString 部分分配和 init 前格式拒绝；调用 posix_spawn 前没有创建子进程。destroy/free 只对应本次成功初始化/分配的资源；错误使用 API 的返回码而非猜 errno。没有运行故障注入或 OOM/路径测试，不能宣称这些反例已实测通过。

回退可独立撤回本工作目录/Spawn 修改；旧 App 包保留，未改用户前缀配置或游戏文件。完整快捷方式语义、实际 guest cwd、多个入口和正常/失败启动运行回归仍待验收。

## 本轮实际结果

- CLI 最终 swift build：退出 0，3.28 秒；App 最终 swift build：退出 0，4.34 秒。
- `ciderctl run --help` 实际显示 `--cwd`；这是 CLI 帮助读取，没有运行 Wine 或安装器。
- `scripts/build-app.sh debug`：退出 0，复用构建 0.54 秒，out/Cider.app 已 ad-hoc 签名；上一包在 out/backups/hy1-cwd-20261012-ZDw4MR/Cider.app。未重启现有 Cider/启动器以加载新包。
- CUA 实际可读 Cider 主界面，米哈游/Steam 显示运行中；Wine 宿主 bundle ID 歧义后，按当前 r2 完整路径选择仍 timeoutReached。没有点击 Wine 窗口、进行设置/最小化/拖动/重启，不能标为视觉验收。
- 本机配置只读显示米哈游为 r2、zh_CN.UTF-8、highResolution=0；CLI ps 只读显示 HYP 及五个 HYPHelper。运行中不是交互/新代码验证，也不证明当前没有下载任务。
- 已通过异步问题询问当前下载/更新状态，收到答复前保留现有会话；此前“无下载”不能无限沿用。没有启动 Windows/RDP/SSH、修改机时账本、Actions、子代理或测试套件；定时任务仍 PAUSED。

下一步是 HY1 的真实入口/显示与首装验收；HY2 原生 kernel/context、HY3 启动许可和三款会话仍按各自证据推进。Windows 超额预约未结算的边界保持。
