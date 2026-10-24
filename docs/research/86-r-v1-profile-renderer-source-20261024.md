# R v1：启动参数、渲染器选择与 ABI 源码接续

接续原02主题队列。WaitAll原冻结范围的5倍/CS控制/自己的小时通过；更广U组固定90次比较有8/15单元越界，归因仍未过（85）。以下是可独立完成的源码准备，不解除该门禁，不计完整M1或游戏可玩。

## 每程序命令行追加

将Highball0006迁到固定Wine11.18。保留`HKCU\Software\Wine\AppDefaults\<exe>\CommandLineAppend`的原始REG_SZ追加、basename匹配、原参数与引号，不重新解释参数。插入点仍在应用名规范化后，新的tidy缓冲区仍由原CreateProcessInternalW的清理路径释放。

修复旧helper的读取边界：RegQueryValueExW即使成功也不保证字符串有NUL；旧代码仅清最后一个数组元素，短的未终止数据可能经过未初始化尾部。现在预留一个WCHAR，按实际返回字节数终止，检查对齐/容量，并防止组合分配长度溢出。[微软契约](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexw)。

收益是保持子进程参数兼容并避免不确定尾部。代价仍是每次创建进程的按程序注册表查询；过大、错误类型或奇数字节的值沿原best-effort模式不追加，不把REG_EXPAND_SZ隐式展开。没有引入全局默认参数。

精确完整process.c已按实际Wine Makefile的两种PE编译命令、另加Werror编译，均零诊断；补丁zero-fuzz/offset重建匹配。独立只读审查未发现具体P1/P2。尚未链接或运行迁移后的模块；入口参数、子进程传播和失败清理的实际验收仍待，不能用组件编译替代。

## 原有自动渲染器选择

Wine11.18默认已经选择OpenGL，但缺少原Highball0002在GL适配器创建失败后尝试Vulkan的分支。新端口保留完整原选择顺序：NO3D、显式Vulkan、显式GL、AUTO的GL→Vulkan；不会让显式GL失败后悄悄转到Vulkan。

收益是保持原启动器/旧D3D路径的策略；代价是在AUTO且GL创建失败时多一次后端尝试。配置默认的getter仍报告AUTO所对应的GL，实际设备分派使用各adapter自己的ops；没有把getter当作实际后端检测器。双PE完整directx.c编译零诊断、zero-fuzz/offset重建匹配，独立审查无具体P1/P2。GL失败、显式配置、D3D9图像及DXMT/D3DMetal组合运行验收仍待，不称图形迁移完成。

## 上游已有的机制与未通过的运行项

- 固定源码GetUserNameA/W使用WINEUSERNAME，shell32的USERPROFILE展开调用GetUserNameW，所需真实用户名路径已经存在。无需为此制造空补丁；这不是所有导入瓶子/存档路径已迁移的证明。
- Wine11.18含整体GSBASE切换，原Highball0008/0009/0011是旧CX树的单槽FiberData/QoS/LastError镜像方案。按新基线逐跨界核对，不能直接将旧槽偏移叠加。上游变更：[3a16aabbf5](https://github.com/wine-mirror/wine/commit/3a16aabbf5)。当前源码观察与待验范围在结构化证据；不把源审当Fiber/回调/异常运行通过，也没有从v0移除旧补丁。

## 采用与回退

两个端口只进入准备清单，没有配方/EnginePolicy采用；canonical源码、现有构建产物、私有F27冻结引擎和用户引擎/瓶子均未改写。后续须在新的独立构建中整合、链接并按原受影响契约验证，保留v0及当前F27产物回退。Windows、原生kernel、游戏与完整M1状态分别保留。

证据：`evidence/engine-r-v1-profile-ports-20261024.json`。初次私有编译驱动的else1语法错误发生于启动编译器之前，失败脚本已留存，不记作Wine源失败或组件运行。

## 实际运行接续

命令行端口的PE64和PE32各完成23检查0失败，覆盖缺值、正常追加、无终止符、隐式应用名、奇数字节、错误类型、空值、超容量和边界长度。子进程实际核对空参数、空格、中文及新增参数并返回42。未迁移此补丁的Wine11.18实验基线在五项需要追加的场景返回41，其余基线和清理检查正常。

父进程异常退出不保证子进程退出。修正后的测试入口在所有退出路径执行自有GUI关闭和进程回收，以真实kernel退出和两次空观察为通过条件。故障验证先通过nonce/PID确认子进程STILL_ACTIVE。首版受CLI按设计等待整个前缀空闲的语义影响，40秒超时清理成功，但不算普通失败路径通过，原回执保留。第二版在真实ACK和记录出现后终止自有CLI，普通失败下约9.586秒完成回收，捕获到仍存活的故障程序并确认清理。故障程序的失败没有被写成正常通过。

新渲染器组合的PE64/PE32，以及未迁移实验基线PE64，各完成16检查0失败。64×64系统内存读回的4096个RGB像素全部匹配。测试请求HAL设备并执行Clear/GetRenderTargetData，不检测物理后端，不证明窗口呈现、着色器、GL失败回退、DXMT/D3DMetal、FPS或游戏画面。

实际回执、构建、源审和清理证明均已加入结构化证据。原F27小时/性能不提升为整个新组合通过；U归因、完整M1、其余图形和专门ABI运行回归继续待验。

## 后续：私有链接与启动前置

两个模块的PE32/PE64均已完整链接到独立输出，原四份DLL摘要未变。新candidate安装在原受控Lab中，沿用已验证的同一native/PE ntdll核心；新旧各一个全新空瓶的wineboot/用户目录/字体初始化成功并实际清理。改变的四份kernelbase/wined3d DLL及两个前缀文件摘要另列，不借F27旧性能/小时宣称整个新组合已验收。

GSBASE独立源审已覆盖syscall/unixcall进入与返回、回调、信号和线程启动。旧host_qos所占0x340在新布局已是syscall_dispatch，不可叠加；原三个旧补丁仍留在v0，新基线记为上游替代机制的源映射，运行回归仍待。对应隐式GS读取、Fiber/异常/回调与宿主接口风险均保持待验。
