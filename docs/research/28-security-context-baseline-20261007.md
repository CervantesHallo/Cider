# HY2：权限上下文的 Windows 对照规格（2026-10-07）

状态：文档契约与当前 CX26.3 源码已复核；尚无本轮真实 Windows 输出，不代表内核 API 已修复。接续 [research/23](23-kernel-feasibility-20261006.md) 和 [接管评审](27-current-code-review-20261007.md)。本轮没有创建/运行测试驱动或修改内核代码。

环境更新：用户已提供 ToDesk 设备 Cervantes 作为独立 Windows 参考机并授权访问，手动连接后已读到远程画面；输入/终端焦点仍受工具错误阻碍，版本与开发工具待核实。截图反馈、接入状态、采集脚本和依赖矩阵校正见 [research/31](31-hy1-user-evidence-and-windows-reference-20261007.md)，不作为 Windows 执行输出。

## 为什么先做这一组

当前缓存 CX26.3 树的 ntoskrnl.c 中，SeSinglePrivilegeCheck 与 SePrivilegeCheck 不区分 mode，均直接 TRUE。缺陷不能简单概括成“TRUE 就是伪成功”：微软规定 KernelMode 走成功分支。UserMode 才须核对真实令牌及 enabled privileges；SePrivilegeCheck 还涉及集合 Control 与输出 Attributes。[单项契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-sesingleprivilegecheck)、[集合契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seprivilegecheck)

这不是一个孤立布尔函数：SubjectSecurityContext 必须捕获令牌引用、保持一致性并释放。SeCaptureSubjectContext、SeLockSubjectContext、SeReleaseSubjectContext 等需要作为依赖组处理。[捕获](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-secapturesubjectcontext)、[释放](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-sereleasesubjectcontext)

## 参考规格（待执行）

| 契约 | 最小对照场景 | 记录与否决条件 |
|---|---|---|
| 单项 UserMode | 主令牌；存在且启用、存在但禁用、缺失特权 | 记录实际选择的 subject、输入 LUID 与结果；不能一律 TRUE/FALSE |
| 当前线程模拟 | 使用受控不同特权的模拟令牌，再恢复主令牌 | 比较采用哪份 token；身份恢复后结果应对应参考环境；不能拿宿主登录用户替代 Windows token |
| 集合检查 | ALL_NECESSARY 与 ANY；全部满足、部分满足、全部缺失 | 同时记录返回值和每项 Attributes 前后值；输入输出不能只验一个布尔值 |
| KernelMode | 单项和集合分支 | 对照文档成功路径及实际输出；不能为了“诚实失败”错误拒绝合法请求 |
| 捕获/锁定/释放 | 捕获后重复查询、令牌状态变化、锁定一致性、平衡释放 | 记录引用生命周期及可观测结果；无稳定引用/锁语义则不能宣称完整实现 |
| 参数边界 | 文档允许的边界，真实 Windows 行为待记录的零项/无效参数情况 | 先区分规定与未规定的行为；不凭猜测填成功或伪造输出 |

参考程序应完全自有、与游戏和任何反作弊独立；Windows 原生环境和 Wine 候选分别运行同一输入集合。不要在登录游戏瓶子中建立这个基线。

## 结果记录字段

OS/WDK/compiler 版本、架构、参考程序源提交/二进制哈希、Wine 源提交与补丁哈希、原始输入、token privilege 状态、mode、返回与输出、失败阶段、资源释放、并发事件顺序。敏感账户信息使用本地匿名标识，不写入公开报告。

接受标准：有真实 Windows 对照、逐场景覆盖及明确不支持范围；KernelMode 文档成功不能被笼统误分类，UserMode 不能沿用无条件空桩；引用和上下文一致性未完成则限制相应能力。API 的覆盖不代表宿主边界外的保障，也不代表游戏服务端接受。

## 取舍与回退

- 收益：先校正权限语义与资源所有权，避免在错误矩阵标签上继续开发。
- 代价：需要独立 Windows/WDK 参考环境和生命周期证据，无法仅凭 macOS 上的文档/编译结果完成。
- 证据：上述微软契约与指定本机 CX26.3 源码；未取得新 Windows 执行数据。
- 状态：已采用该参考规格作为下一交付；运行时实现仍待对照，不修改现有门禁。
- 回退：规格可按新的参考结果修订；后续代码必须独立补丁并可恢复原引擎/瓶子快照。
