# 隔离审查续修：对象生命周期与失败清理

2026-10-15。用户指出上轮只修Q1–Q5、留下同批内核问题就交付，未满足“审查然后修复”的要求。这个指出成立。research61是部分交付，不能作为整批代码已修完或全部可恢复的结论。本轮继续在同一隔离分支实施实际修复。

## 实际处理结果

| 项目 | 本轮处理 | 当前结论 |
|---|---|---|
| K1 查找/FREE/引用竞态 | 新0013：同权限稳定临时句柄保持实际对象；map锁覆盖查找/类型检查到引用；FREE按map→ref锁序处理；借用mapper逐层finally，构造器临时句柄独立finally | 源码修复及离线并发反例通过；不是Windows原生验证 |
| K3 异常/错误清理 | 新0012：状态构造器保留真实错误、先查询后登记；命名查找成功取得引用后才发布；线程池失败归还引用、回调/PnP异常展开清理 | 最终组合313条断言、6个故意破坏变体检查通过；原生异常契约待 |
| K2 主令牌访问语义 | 替换方案的专项工具调用被自动安全审查拦截，理由为“可能存在网络安全风险”；未实现、换措辞或另派代理重试该方案。旧0010移出候选目录，保留拒绝稿和原分支 | **没有完成忠实实现。撤回旧实现不等于修复API。** 修复队列回到基线未实现接口，未伪造对象或成功 |
| XPC架构偏差 | 该偏差早于隔离批次，本轮没有实施跨产品架构重写 | 仍是明确旧债务，不计为已修复，不用补丁/构建替代responsible/TCC依据 |

主令牌替换受阻是工具自动安全审查的真实拒绝，不是因缺原生机器而把可做的源码修复延期。它也不是对“所有Windows API均不可实现”的断言。不能把现在的结果报告为整批已无问题。

## K1的所有权与失败处理

- ByHandle先用`NtDuplicateObject`的`DUPLICATE_SAME_ACCESS`、零新增访问/属性取得私有稳定句柄；原数值句柄被关闭或复用后不再参与后续查找。没有新增令牌通道、权限或身份变化。成功引用后关闭私有句柄，失败不发布输出。
- map→ref锁序用于查找至pin和FREE；FREE不释放正引用对象。借用mapper拆成不自行上锁的内部函数与逐层finally的公开包装；异常不能遗留递归锁。thread构造器的嵌套临时句柄也在finally中归还。
- 最后一次backend释放报错，可能发生在副作用之前或之后。不能盲目补回“调用者仍拥有一次引用”或重试释放；对象记录终止错误，后续引用/释放报告该错误，不再发GRAB/release。若后端确已释放，真实FREE可回收零计数代理；若未释放，可能保留资源到进程结束。这是显式失败策略，**没有证明恢复/原生失败契约**。
- 私有对象头按`MEMORY_ALLOCATION_ALIGNMENT`对齐并有编译期尺寸断言。最终x64尺寸16/对齐16，i386尺寸16/对齐8，避免新增错误字段把FILE_OBJECT等对象放到错位地址。
- Null输出明确失败；非null HandleInformation及UserMode没有冒充支持。复制句柄增加了一次复制/关闭和可能的资源失败，是本候选的成本；不把这组接口标成完整Windows对象管理器。

微软接口依据：[ObReferenceObjectByHandle](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obreferenceobjectbyhandle)要求成功引用阻止删除，[ObfReferenceObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obfreferenceobject)针对已有对象指针增加引用。这里只引用文档契约作为依据，离线模型并非原生对照。

## K3及原生持有链核查

四个私有构造器一起改成NTSTATUS返回；查询失败不先登记半初始化代理。ByName保护driver树的拥有引用直到checked acquisition成功，错误不发布指针；未实现的模式/参数明确失败。线程池提交失败保存真实状态后归还恰好一次引用；成功提交将引用交给回调，回调即使释放work item或展开异常也用预先捕获的device清理。AddDevice的命名driver引用同样有展开清理。

早期“thread->process/TEB借用指针未闭合”的笼统判断经父代理与复核者继续核查后收窄：server/thread.c创建线程时强引用process，析构才归还；IRP强引用thread，current_call持有IRP；客户端在下一次归还current_call的RPC前清空TEB借用。受这些持有区间保护的借用不需要另加全局引用环。超出拥有区间仍须显式引用；这不证明全部异步/关闭场景或主体捕获语义。

## 验证与对抗性复核

- 精确抽取9个对象生命周期函数和实际对象头，ASan/UBSan及真实pthread：100轮确定性关闭/查找交错通过；相同旧代码交错重现ASan heap-use-after-free。失败在副作用前/后两种模型、正引用FREE拒绝、负计数/溢出、输出保持、嵌套异常与清理一并检查。
- 合并源码的调用点模型：313条断言通过；6种故意破坏（含独立嵌套构造器清理破坏）均被拒绝。模型依赖替代backend、句柄及SEH控制转移，不运行驱动或Windows。
- 从固定旧产物逆向恢复基线，再应用0009→0012→0013，零fuzz/offset；6个相关文件与实际构建源码逐字节一致。原0010函数体没有进入最终源码，spec回到基线stub。
- 复用配置在隔离目录增量构建并链接i386/x64的ntoskrnl模块，退出0；使用macOS15.4 SDK及既有MinGW工具链。不是clean configure、整引擎发布或Wine运行验收。
- 独立复核在初稿发现递归锁异常残留和i386对象头对齐两项P2；均已实际修改并回归。最终复核绑定`cc83e07e679642084783288cce6e63e4e76335990251995b8711dd6f234a8ce5`，在其有界通用内存/调用清理范围内无剩余可行动发现。它没有审查或接替被拦截的访问替换方案。

实际Windows原生结果、Wine完整transport/关闭运行、性能及三款游戏会话均未取得，仍不得升级兼容评级或放行游戏。

## 隔离、回退与原队列

- 保留原隔离分支eecba4f和私有原产物；新修复仍在`codex/quarantine-fixes-20261014`。候选目录的[队列说明](https://github.com/CervantesHallo/Cider/blob/1a919a8/engine/patches/candidates/QUARANTINE-REPAIR.md)明确0009→0012→0013；原0010仅在rejected中保留字节追溯，不应再应用。
- 常用App、已安装引擎、游戏/存档、主线实现及真实Windows账本未替换；没有Windows连接、机时预约/结算/扩额、驱动加载、产品GUI或Wine会话，没有调用Actions。定时仍关闭。
- 撤回0012/0013必须整体撤回私有构造器声明/实现及对象头，不能只换部分PE模块；0009/0012/0013都未进入默认配方。本轮没有恢复授权，保持隔离。
- 原接续点仍见61：柚子社两款Control/重开已有验收；HY1剩余真实入口/登录/更新/长期保留；HY2有参考编译/Win32对照但缺独立kernel结果；HY3及三款本地交互会话未完成。原神下载保留，HY1→HY2→HY3→HY4/HY5顺序不变。

本轮实际补修了K1/K3，另撤出已知不满足契约的K2旧稿；**没有完成全部隔离问题**。余下K2受到真实自动审查阻断，XPC/原生验证属于明确未闭合项，不能再用“继续隔离”四个字冒充完成。

Implementation commit: `1a919a8` (CervantesHallo author/committer; skip ci). Durable evidence: `evidence/quarantine-kernel-repairs-20261015.json`.
