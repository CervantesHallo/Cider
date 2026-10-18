# K2 主令牌真实引用修复及 Wine 运行

2026-10-18。Source: `47c72831803b69f1eb0c24e444b9c463de158dfb`. 承接70继续修复隔离代码。**本轮补齐K2候选的取得/释放实现及受影响的本机验证；仍未取得Windows原生kernel契约结果，没有恢复主线或启用默认引擎。** 69以前的历史“未实现”是当时检查点，当前状态以本报告为准。

## 修复内容与方向

[微软契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psreferenceprimarytoken)要求引用指定process的实际primary token，并由[PsDereferencePrimaryToken](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psdereferenceprimarytoken)平衡。新0018/0019沿用现有Wine server kernel-object映射与强引用；既有0017释放接口保留，0020补公共DDK声明。没有恢复退休0010的NtOpenProcessToken访问句柄路线，也没有token复制、宿主/current token替代或成功空桩。

- server要求device manager归属当前地址空间；9个相关入口统一检查，跨进程复制manager句柄不能更改别处代理的所有权计数。
- 新请求必须带已有process句柄及已登记kernel process映射，二者指向完全相同的实际server对象；没有PID或访问掩码参数。
- server从保留process的primary槽取得真实token强引用。canonical proxy与backend owned位共同协调alias及零到一/最后归还。主槽更换不改变旧token的已持有引用。
- client保持map→ref锁序；逐项检查type、terminal error、计数/溢出、new_owned与本地引用状态，接纳成功才发布结果。失败只回滚本次新取得的backend引用；关闭/回滚异常保留真实失败及不确定状态，不猜测释放结果或重试。
- 一次临时process句柄仅使用既有KernelMode对象能力，在finally关闭。普通NtOpenProcessToken/alloc_handle/check_object_access没有修改；相关源文件摘要逐字节保持。对kernel对象的引用不是用户句柄权限授予。

这保持原HY2忠实Windows实现方向。Wine继续可识别，三款游戏门禁和评级不变，Epic仍弃用。它没有证明三款游戏已经可运行。

## 审查中实际发现并修正的问题

1. map finally回调原型不匹配导致编译失败，改为正确的上下文版本。
2. 原先仅新请求检查manager owner，旧入口仍能跨地址空间改owned状态；统一所有相关入口。
3. 临时process NtClose可能抛异常，原清理会跳过结果引用；先detach句柄、捕获实际关闭状态，异常/普通失败均detach并归还结果。
4. API实现/导出存在而公共ntifs声明缺失，正常调用方无法编译；0020补一致的WINAPI原型，import library实际重建。
5. 验证夹具在child退出后仍保留process/thread句柄，不能充分证明token独立保活；最终先检查关闭两句柄，再查询和释放held token。前期结果保留为较弱证据，不冒充最终验收。
6. callback异常缺完成通知、部分提交失败可能遗留栈引用；worker捕获真实错误/释放临时token并发出通知，dispatch所有队列退出路径都放行start、join实际提交项、回收未提交项。

7. 最终复审发现预期故障若只看配置/异常码会掩盖API自己抛出的OOM；改为实际到达注入点才置标，worker还须持有非空token，底层真实失败不计为预期。混配控制器首次缺shutil局部导入的NameError也已实际修复并重验；首次并未执行产品断言，保留失败回执。

父代理实现及实际编译/运行，独立只读审查逐项复核。模型、构建、真实Wine、原生Windows结果分别记载，代理报告不替代实测。追加夹具链接问题：异常库间接导入ntdll触发Wine“普通DLL”初始化，服务1114失败；最终链接Wine现成异常库在内核import库之前，异常展开经内核导出，不改产品loader判断。该失败回执保留。

## 已取得的证据

| 范围 | 结果 | 不代表什么 |
|---|---|---|
| 精确server C函数+有界依赖，ASan/UBSan | 70项：owner/目标绑定/alias/槽替换/类型/空值/OOM/关闭 | 非原生Windows访问控制或全局调度 |
| 精确client C函数+RPC/SEH模型，ASan/UBSan | 39项：接纳/计数/回滚/close异常/锁与临时资源 | SEH模型不等于实际Windows异常契约 |
| 六个故意破坏变体 | 全部在成功编译后被断言拒绝；编译失败不算检出 | 不证明无其他缺陷 |
| 补丁组合重建 | 9项正向补丁，14文件完全一致，zero fuzz/offset，旧0010仅逆向撤除 | 未加入默认配方 |
| native server/ntdll、两架构ntdll/ntoskrnl | 本机编译/链接完成，public import重建 | 不是Windows原生构建 |
| 协议布局 | 1809→1810，旧request序号不变；两架构generic request/reply/caller结构尺寸不变 | 新旧server/ntdll仍禁止混配 |
| 私有产品App/CLI/Bridge | release打包、严格签名，实际产品CLI→GUI→Wine | 不证明正式签名或TCC权限归属 |
| 最终实际Wine x64 / i386 | 各420项、0失败；各64 alias/零到一轮，64正常并发轮、2个预期夹具故障，child退出并关闭全部外部句柄后held token仍可查询/归还 | 非原生Windowskernel及真实游戏 |
| 产品混配负例 | 新ntdll与旧server组合在spawn前被拒；新增Wine spawn=0，finally恢复原哈希且清理通过 | 没有放宽绑定或模拟拒绝 |
| 两架构子进程门禁 | 自有复制程序被拒1260、marker不存在、清理成功 | 没有运行真实游戏 |

实际token身份以TokenStatistics.TokenId在同一会话内比较：kernel取得结果等于自有Win32 requestor，且不同于winedevice真实宿主；不输出SID/账号。IOCTL无输入，无任意PID/句柄/地址接口。两个预期故障是夹具自身明确raise STATUS_NO_MEMORY（取得token后callback异常、第二项提交前异常），只计预设状态；任何其他状态或清理错误仍为失败，不能称为真实系统OOM注入。

最终两架构均由产品CLI启动，真实.sys加载/卸载；服务STOPPED读回/删除，prefix进程为空、所属XPC job移除。常用App和真实Windows账本摘要未变。原始成功、早期保留句柄的较弱结果及1114失败分开保存在tests/kernel/token-reference-contract/results。

## 配套版本、成本与回退

0019追加协议请求并升1810，必须将来源一致的wineserver、ntdll Unix、x64/i386 ntdll PE、x64/i386 ntoskrnl PE配套使用。EnginePolicy保留原两组native模块绑定，新增精确ntdll:wineserver配对，而非相信manifest的capability声明；loader摘要仍检查。相关访问检查和CIDER_PREFLIGHT_DENY源码字节未改。

收益是取得真实指定对象、共同所有权和可审查失败语义。代价是一次候选proxy分配、临时process句柄/RPC和map→ref串行化；没有向图形/音频帧循环插入新IPC；该API调用本身有上述成本，没有整体性能提升承诺。协议配套增加发布成本，当前仅隔离候选、不进入配方。

回退须整体回退该匹配native对、两架构PE与EnginePolicy新增配对；不能只回退server或放宽哈希检查。0017释放半边和已有K1/K3可独立保留；退休0010不恢复。

## 当前完成边界及原计划接续

- 已发现的Q1–Q5、ADR007及K1/K3源问题有61–69的既有修复/受影响验证；本轮补齐K2源缺口，并完成上述Wine定向验证。不是“所有代码永无缺陷”或“生产可用”保证。
- 原生Windows的有效/失败/异常契约、独立kernel环境及真实三款游戏仍未验收，HY2整阶段未完成；没有拿本机Wine结果替代它们。
- 用户“10月13日14:00后代码隔离，等指令恢复”仍有效。实现只在codex/quarantine-fixes-20261014，正常App/安装引擎/游戏/存档/下载不部署、不改动。
- Windows准备余0、测试3600、旧超额pending保持；本轮Windows/SSH/RDP/VM/Actions/自动化操作均0。后续独立Windows验证需要既有预算及环境问题的人类决定，不自动改账或转用测试额度。
- 继续原HY1→HY2→HY3→HY4/HY5。原HY1首装/显示/入口证据以及柚子社两作Control/重开验收保留，不重复无变化基线；本轮没有扩大为所有柚子社或三款本地已兼容。

关于安全提示：用户所见只有泛化文案、无错误码；当前这些工具调用实际成功。历史局部拒绝不足以证明忠实Windows API本身不许实现，具体内部判定仍未知，70保留官方说明和证据限制。没有通过关闭防护、改提示或重放退休方案来获取结果。
