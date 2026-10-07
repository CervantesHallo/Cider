# HY1 用户证据与 HY2 参考环境接入（2026-10-07）

接续 research/30。证据来源分开记录：用户截图/交互反馈、macOS 自动化工具状态、指定 CX26.3 源码、微软文档。没有运行测试套件、测试驱动或三款游戏，没有触发 GitHub Actions。

## 1 HY1 新增的实际证据

用户提供了米哈游启动器完整桌面截图（原图3024×1964）。启动器内容从顶端到下沿可见；顶部设置、最小化、关闭图形及底部“开始游戏”区域完整显示，未见原来新增标题栏覆盖顶部30px的现象。这与 r2 的 Wine/Cocoa 坐标日志相符。截图含无关桌面信息，不复制进公开仓库。

用户确认：窗口可以拖动，不能缩放；顶部只有关闭/最小化一类窗口控制。截图呈现的设置等图形与用户口述分别保留，不能据此宣称已经点击验收所有控制。

结论：本次静态完整显示及拖动取得实际用户证据；仍缺按钮点击、最小化/恢复和不同显示比例的交互证据。不能缩放记录为待对照行为：需要检查官方 Windows 版是否也是固定尺寸，并核对真实窗口样式；在此之前不把它判定为 Wine 回归，也不擅自强制窗口可缩放。

Dock 截图没有出现此前一排灰色占位图标，米哈游启动器有实际图标。该证据支持当前启动器会话的宿主修复，不能推广为所有 Windows 程序已经完成视觉验收。

## 2 真实 Windows 参考环境

用户确认有 Windows 电脑并已开启 ToDesk，设备名 Cervantes，明确授权通过 ToDesk 访问用于本项目。设备列表显示 CERVANTES 在线。这里只记录设备名和用途，不记录设备代码、IP、账户、密码或私人桌面内容。

初始 macOS ToDesk 设备行未暴露可访问性按钮；坐标点击连续返回 `noWindowsAvailable`。窗口 Raise、索引选择和键盘观察未取得指定设备的连接。用户随后手动接通，实际读取到了标题 CERVANTES 的远程画面；坐标点击仍返回相同工具错误。键盘观察曾显示 Windows 开始菜单，但输入与终端焦点没有闭合，尚未取得命令输出。OS、架构、SDK/WDK/compiler 版本仍未核实。没有选择其他设备进行连接。

连接已由用户完成；现请用户切回桌面并打开普通 Windows PowerShell，以补足终端焦点。下一步只读检查系统与开发工具，并在独立项目目录准备参考环境；不在生产游戏会话或反作弊进程上建立基线。若后续驱动对照需要额外系统安全变更，必须单独明确具体操作，不能从“允许连接”推导出可关闭保护。

已准备 `scripts/windows/collect-reference-environment.ps1`：PowerShell5.1+，记录 OS/PowerShell、SDK/WDK 头文件和 MSVC 目录可用性，错误单独记录；不枚举账户/网络/游戏，不加载驱动或修改系统。默认 JSON 输出，有输出目录时只创建带时间/UUID的新文件。不运行编译器，目录/头文件存在不能当作工具链构建或契约验收通过。仅静态审阅，尚未在 Windows 执行。

## 3 权限组的矩阵校正

`data/kernel/fidelity.json` revision 2 重查了两个权限条目及模型概述，其他条目保留原审计来源，不宣称全表重新执行对照。

- `SeSinglePrivilegeCheck` 的 KernelMode TRUE 符合文档，UserMode 必须检查当前线程令牌。[微软契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-sesingleprivilegecheck)
- `SePrivilegeCheck` 的 KernelMode 还要更新请求特权的输出标记；UserMode 涉及启用状态、ALL/ANY 和逐项输出，不能只改布尔返回。[微软契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seprivilegecheck)
- 当前缓存源码的捕获、锁定、解锁、释放，以及 primary/impersonation token 引用/解引用导出仍为 `@ stub`。已有 user-mode token API 不等于已有内核 context 生命周期；须作为依赖组实现并取得参考证据。[上下文契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-secapturesubjectcontext)
- 模型概述不再将指定构建的协议缺口写成“所有未来架构永远无法实现”；新架构仍须重新证明契约和边界。当前不支持范围不能靠改报告值补足。

对抗性复核：未把合法的 KernelMode 成功列为错误；未把 UserMode 一律失败当作忠实实现；未用源码/文档代替 Windows 运行结果；未宣称修改矩阵等于内核修复或游戏可玩。`reachability=partial` 仅记录此审计实现中模式/依赖尚未闭合的范围，后续依赖实现和真实对照须重新评估。

## 4 取舍、验收、回退与后续

采用：用用户实际截图补上工具读不到的静态视觉证据，先对照固定窗口行为；权限组先完成令牌/context 依赖清单及 Windows 环境核查。

收益：避免为截图已完整的窗口重做身份方案，也避免沿“简单 TRUE/FALSE 修补”方向实现内核 API。代价：完整交互和原生驱动结果仍需实际参考环境。

验收：Windows 连接及工具版本有实际观察；窗口行为对照有版本/样式和交互记录；权限实现按 research/28 逐模式保留输入、输出和生命周期证据。当前只完成本报告列出的截图/反馈和文档/源码校正。

回退：数据与文档独立提交；没有改引擎二进制或启动许可。若参考结果与规格不同，修订矩阵并保留原始证据。下一顺序仍为 HY1 收尾 → HY2 真实契约/依赖实现 → HY3 证据许可闭环 → HY4/HY5 本地会话。
