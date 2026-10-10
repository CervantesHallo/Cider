# HY1 入口修正与 HY2 独立参考工程（2026-10-10）

本轮按 HY1 → HY2 推进。用户要求继续自主完成代码，简单 Windows 操作由用户执行。仅启用一个审查子代理，复用它做路径和原生参考源码的独立审查；父代理核对源码、整合并构建，子代理报告不作为运行验收。

## HY1 新证据与实施

用户对 Mac HYP 的设置打开/关闭、最小化恢复、拖动后顶部内容三项检查统一回复“是的完整”。此前完整截图及 r2 Wine/Cocoa 坐标记录仍保留。不能扩展为登录、验证码、更新、游戏会话、1×/2×或所有 Windows 程序通过；当前 r2 不作一般发布。

对 [research/35](35-hy1-launch-audit-20261010.md) 的源码发现，父代理采用以下修正：

| 项 | 实施与理由 | 验收范围与回退 |
|---|---|---|
| 宿主路径的瓶子身份 | CiderCore.WinePathMapping 统一用选定 prefix 的盘符映射；CompatDB 的 scoped lookup 只接收 Windows 绝对路径；runner、catalog、兼容卡一致使用映射目标 | App/CLI 构建；静态拒绝任意 /tmp/drive_c 冒充 C: 的旧匹配。实际 GUI/CLI 与自定义盘符组合仍待验证。可撤回独立源码提交 |
| 辅助进程范围 | CatalogApp 保存 profile 的安装目录范围，helper 除名字/程序目录之外还需通过与 profile 相同的根/单层数字版本检查；保留明示 extensionless 别名 | 不扩大到 tools、版本目录内 games 等任意深度。未复现用户误杀，也不据静态修正宣称所有停止场景通过 |
| 保存启动器的两个入口 | 瓶子页按保存 id 调用资料库同一 lifecycle，保留环境、cwd、已在运行检查；缺条目时刷新后重查 | 源码路径统一、App 构建；不把普通自由运行命令改成单实例 |
| 模拟重启 | BottleStore 持同一瓶子操作锁，锁内重读配置，停止后运行 wineboot -r，检查非零退出码 | 整个合作入口事务有源码证据；跨进程故障/重启回归未执行，不能替代 L6 历史实机证据 |
| 安装器子进程的图形环境 | 配方新增 typed environment_profile 引用；只允许安装步骤，要求被接受的 profile 属于同一 recipe target；米哈游配方 rev3 显式绑定 HYP 图形环境 | 收益是安装器首次启动 HYP 时继承 DXMT 跨进程设置；代价是该环境继承整个安装器子树。未知来源直接安装文件不靠名称猜 profile。首次安装渲染仍待实测；回退移除配方绑定及对应字段 |

已有用例的数据设置按新的映射边界更新；本轮未新增 Swift 测试或运行测试套件。现有缺口（msync 池耗尽、长期日志、1×/2×及广泛应用）保持独立待办。

本机 debug Cider.app 已重新打包、ad-hoc签名，并通过应用菜单退出旧主界面后重新打开。CUA实际观察到原资料库仍有5项、Steam/HYP仍显示运行中、米哈游瓶子绑定r2且Retina开关off；再次点击HYP启动，界面返回“已在运行”，对应已有实例分支。这里只验收新版GUI重新载入与该分支，不作为保存启动器字段、实际停止/重启、Retina或整个Wine进程树的新运行证据。

## HY2 交付的代码

`Tools/windows-reference/` 包含普通 x64 程序、只读 WDM 参考驱动、定长无指针协议、两个 MSBuild 工程和构建/Win32 采集脚本。源包的每个文件有 SHA256；生成元数据标明实际来源提交。打包脚本拒绝未提交参考源码，不混入 SDK、EWDK、结果或本地构建产物。

普通程序记录主令牌前后、私有模拟副本、禁用与移除特权的副本。修改仅针对自己创建的副本，恢复先前线程身份。PrivilegeCheck 的主令牌对照明确使用私有 SecurityIdentification 副本，不能冒充 kernel 的主令牌查询。[Win32 契约](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-privilegecheck)

参考驱动直接调用 Windows 的 SeSinglePrivilegeCheck、SePrivilegeCheck、令牌引用/释放和上下文捕获/锁定/释放；记录 UserMode/KernelMode、ALL/ANY、输入/输出属性及调用顺序计数。KernelMode 的合法成功来自真实 Windows API，不是 Wine 成功空桩。[单项](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-sesingleprivilegecheck)、[集合](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seprivilegecheck)

读取只允许 System/Admin 的设备句柄，通过一个 read-access METHOD_BUFFERED IOCTL；只接受版本、长度和三项 LUID，核对当前请求线程/进程，没有任意地址、进程操作或句柄输入。查询失败状态与 IOCTL 传输完成分别记录。驱动不改变令牌、挂钩其他程序或访问游戏。[设备 ACL](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdmsec/nf-wdmsec-wdmlibiocreatedevicesecure)、[IRP 请求线程](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_irp)

## 对抗性审查与证据边界

独立审查发现并修正：请求线程核对缺失；主令牌 Win32 输入和失败不能阻断独立内核观察；零项/变长 TOKEN_PRIVILEGES 的长度和计数边界；输出流的 sticky error。父代理重查调用和清理路径，增加空捕获令牌的明确失败，不将部分输出冒充对照完成。

最终整合审查还发现 Windows CMD 的 `if errorlevel 1` 只判断“大于等于1”，可能漏掉负数崩溃退出状态。构建/采集的每个外部命令均补充负状态检查，避免把部分 JSON 或旧二进制当成成功产物。[CMD 条件语义](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/if) 这是源码层修正，尚未执行 Windows 崩溃场景。

本机 App/CLI 的 Swift build 成功；普通程序用现有 x86_64 mingw GCC 以 -Wall -Wextra -Werror 交叉编译成功；驱动用真实 EWDK 头文件、Clang Windows MSVC target 的语法及结构大小检查成功（SDK RTL_CONSTANT_STRING 宏产生四条 Clang 警告）。Mac 的 UDF 大小写差异用临时 DriverSpecs.h 文件名别名处理，未修改 SDK 或项目源；这些不是 Windows MSVC 构建/链接或驱动运行证据。

用户已回传 Windows MSBuild 版本，但本工程的 Windows 构建及原生 API 输出仍待取得。build.cmd 使用明确工具路径，构建未签名驱动，随后只运行普通程序；不创建/加载驱动服务，不改变系统保护。实际 kernel 采集需要另外确认的可加载自有驱动的参考环境。

单次捕获和配对释放的记录不证明跨进程 token 锁、并发变更、异步上下文、无泄漏或 KMDF 生命周期。当前源码是原生观察工程，尚未修复 Wine 的 token/context 空桩、修改内核保真度 disposition 或解除任何游戏门禁。三款本地会话按 HY3/HY4/HY5 继续分别验收。

取舍：以独立、可撤回的参考代码及用户一条脚本换取实际 Windows 构建/输出；成本是另需合法可加载自有驱动的原生内核运行环境，不能只凭 Win32 结果代替。回退为移除参考目录/退出终端/卸载镜像；没有系统服务需要清理。本轮没有触发 GitHub Actions。
