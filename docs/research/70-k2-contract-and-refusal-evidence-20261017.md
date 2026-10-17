# K2 契约核对及安全提示的证据边界

2026-10-17，承接69继续K2。当前没有恢复旧0010，没有实现新的私有访问路径。已独立补上释放半边；取得半边仍未完成，没有把只读观察或部分实施记为K2全部修复完成。

## 拒绝记录的纠正

用户本轮转述的提示只有“需要谨慎开放网络安全……”一类通用文字，没有错误码。当前能取得的最近工具调用均成功；近期会话尾部没有提供新的明确拒绝事件。这不能否定用户实际看到的产品提示，也不能推断是哪个动作或规则触发。

历史`out/kernel-review/blocked-action.json`记录的是此前某个primary-token访问替换提案被自动安全审查拒绝，理由仅为possible cybersecurity risk。该记录没有给出整个K2或忠实Windows兼容实现一律禁止的依据。先前把这个局部历史拒绝概括为“K2整体不能做”，判断过宽。

[OpenAI官方说明](https://learn.chatgpt.com/docs/cyber-safety#false-positives)承认合法工作或无关活动仍可能触发防护，建议检查提示/日志并用可用的feedback入口报告疑似误判。当前无法确认这次具体内部判定；没有通过改提示词、换工具、转交代理或改Codex防护来重试历史被拒动作。

## 正确契约

[PsReferencePrimaryToken](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psreferenceprimarytoken)针对指定process的主令牌增加引用，返回真实对象；[PsDereferencePrimaryToken](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psdereferenceprimarytoken)平衡该引用。当前Wine宿主/调用者不是目标进程的替代；token副本、固定成功和恢复旧query-only代码都不能算完成。

## 已排除的标准接口路线

1. **仅把请求改为QUERY/ALL_ACCESS**：现有[固定Wine实现](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntoskrnl.exe/ntoskrnl.c)经process handle和NtOpenProcessToken得到token。当前后者仍检查调用方对令牌的DACL；更小/更大的请求权不能独自兑现指定KernelMode对象引用契约。不能把受限目标失败忽略成完成。
2. **零访问权打开令牌**：本机32/64真实Wine均OpenProcessToken(access=0)失败，Win32 error=5；原TOKEN_QUERY打开成功。Wine固定快照的[对象测试](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntdll/tests/om.c)也明确期望该零访问打开返回STATUS_ACCESS_DENIED。因此没有删除alloc_handle/check_object_access的拒绝分支。DuplicateHandle(desired=0)可以得到零权句柄，而查询它仍返回5；这不能冒充零权打开可行。
3. **当前进程token伪句柄**：[微软说明](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocesstoken)把它限定为当前进程且不允许DuplicateHandle/DuplicateToken复制。本地get_magic_handle也解析当前Wine请求方，不能借此替换任意指定process的主令牌。

只读观察器仅调用自己进程的公开API，不修改token、权限或DACL，不切换身份、不接收PID/地址、不加载驱动。原始观察/清理在`tests/kernel/token-reference-contract/results`，它不是新一次原生Windows参考、平台放行或K2实现证据。既有Windows机器、预算、门禁和源码隔离保持。

## 已实际实施的释放半边

0017仅把PsDereferencePrimaryToken从stub改为stdcall导出和真实函数体，直接调用既有ObDereferenceObject；与固定上游的释放操作一致。没有主令牌查找、复制、句柄打开、协议或访问权改动，PsReferencePrimaryToken保持stub。正常路径没有新分配/RPC/锁，沿用对象管理器的清理与真实错误传播。回退仅删除该独立函数/恢复其spec stub；不回滚已修的通用引用机制。

精确函数体的3个定向模型检查验证指针/恰好一次委托、最后引用释放及底层错误传播；依赖是宿主模型，不是实际token后端。32/64真实PE模块链接成功，反汇编分别显示真实函数和i386的stdcall调用约定，均转到ObDereferenceObject而非unimplemented stub。组合队列重建6文件完全一致、zero fuzz/offset，旧取得接口函数体没有重新进入。

父代理复核只变更一个函数及对应spec导出，普通公开API检查及对象管理器函数体保持；没有用宿主token代替目标token。证据在release-wrapper-result/queue-with-release/release-pe-proof；没有重复运行无变化的既有Wine基线，也没有将这些编译/模型结果升级为原生Windows或完整引用/释放对照通过。

## 当前结论

忠实兼容目标本身没有因通用提示而被判为违规；具体实现仍须逐项检查。K2取得接口的源码缺口仍然存在。当前已实施释放半边，并核对/排除三条错误取得路线；不是新增权限通道或宣称修复完成。后续需要针对实际指定对象的引用/生命周期边界给出可审查实现，普通公开API的访问检查继续保持。
