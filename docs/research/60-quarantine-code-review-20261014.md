# 隔离代码全面审查：2026-10-14

## 结论

**不建议原样整包恢复。** 确认4项恢复前应处理的功能/集成问题：1项P1、3项P2；另有1项新增P3异常处理缺口。内核候选还有独立的采用阻断条件。与此同时，若干修改确实改善了旧实现，不能把所有问题归为新增，也不能凭模型异常背景判定整批代码无效。

最终目标、国服范围、Wine可识别、游戏门禁和Epic弃用方向没有发现实质改变。主要执行风险是：安装/入口和工具准备推进较多，核心原生kernel/context依据仍没有闭合；不能将支持性代码、报告数量或编译成功换算成三款游戏本体兼容进度。

本轮授权范围是审查，**不授权修复、恢复、合并隔离分支或开展Windows工作**。当前主线实现仍为374980e，隔离分支仍为eecba4f，旧应用包保持，Windows账本不变。

## 范围与方法

- 对照：`374980ecab3a6f80b00db950020fad39a8f20e39` → `eecba4febdfc8dd655eacb0d7d826c001f098c15`；涵盖024ab36、11a7167、84c4498、eecba4f的全部27个变更文件。
- 在`out/audits/quarantine-20261014/{base,isolated}`建立只读源码快照。两个范围明确的审查代理分别检查Swift/UI/打包/配方、预算脚本；父代理检查内核候选、计划与原始证据，并独立复核关键发现。代理报告本身没有作为验证通过的依据。
- Swift组逐段检查7个目标文件（隔离版1239行）及22个辅助文件的相关调用链；预算组检查343行脚本及对应提案，完成123个私有夹具。父代理完成14个抽取C函数/模拟后端案例、3个独立预算反例、快捷方式关键案例复跑、补丁重建和历史证据核对。
- 没有启动产品GUI、Wine或Windows进行新验收，没有加载驱动、下载引擎、触发Actions、改真实账本或改实现。仅只读检查既有会话和已运行原启动器的选定元数据；本机探针与占位签名包都位于审查目录，原生探针子进程已回收。
- 这些测试不代表完整Cider回归、Windows Shell对照、原生kernel生命周期或真实游戏性能测试。P1指恢复该组件前必须解决的高影响问题；P2为明确功能/边界问题；P3为较低影响的健壮性或条件性能问题。没有发现P0不等于证明不存在。

## 一、确认的新问题

### Q1 / P1：新预算记录被旧版接受，已补扣时间重新变为可用额度

位置：[隔离预算脚本:33](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/scripts/windows-reference-budget.py:33)、45、158、213–234。新增`overrun_reconciliation.extra_charged_seconds`，却仍使用`cider.windows-machine-budget/v1`。基线validate接受此格式并忽略额外字段，status只累加原seconds。

触发无需伪造批准或修改JSON：新版合法结算后，旧部署、脚本回滚或并存旧版访问同一账本即可。父代理独立复现：

| 合成输入 | 新版剩余准备 | 旧版剩余准备 | 漏扣 |
|---|---:|---:|---:|
| 原300秒预约，观察330.987秒，追加3600秒 | 3569秒 | 3600秒 | 31秒 |
| 原300秒预约，观察3900秒，追加3600秒 | 0秒 | 3600秒 | 3600秒 |

代理还在私有账本中验证旧版能继续合法预约，随后新版因累计超额连status也拒绝。文件锁能避免同时写坏JSON，不能解决版本间计费语义不同。

**建议：** 明确新账本版本和旧版拒绝边界；迁移/补扣/追加同锁同次提交，保持历史及基础限额。不能仅给v1增加旧版会忽略的feature字段。该修改修复前继续隔离。真实账本SHA256仍与原隔离锚点相同，结算记录为0；本轮没有发现实际已经漏扣。

### Q2 / P2：标为“未测量”的矛盾回执能作为实测超额结算

位置：[隔离预算脚本:201](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/scripts/windows-reference-budget.py:201)。新入口检查了数值、ID、清理布尔值和精确批准，但没有检查duration_measurement/elapsed_seconds_upper_bound之间的一致性。

父代理独立构造既有已定义模式`unmeasured-attended-handoff`、上界300，同时填数值330.987，入口仍关闭pending并追加预算。原close会拒绝这一超额输入；它对限额内模式矛盾的弱校验是旧问题，分开记录。真实原回执当前测量字段没有此未测量标志，不据此指控原回执虚假。

**建议：** measured/unmeasured互斥、明确缺省兼容规则，拒绝显式矛盾及未知模式；共用回执解析器。保留真实人工未测量交接的合法null/上界形式。新结算入口启用前应修复。

### Q3 / P2：严格StringData处理误拒合法中文/日文ANSI快捷方式

位置：[隔离ShellLink.swift:44](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/Packages/CiderKit/Sources/CiderPE/ShellLink.swift:44)、53、59；结果被AppCatalog的快捷方式扫描直接跳过。

固定CP1252是旧假设，但旧版可忽略未用于启动的名称/图标解码失败，仍返回有效目标；新版将这种失败升级为整个link返回nil。父代理复跑同一未修改源码探针：CP936兼容名称、CP932全角空格名称及CP936兼容图标三例，基线接受有效目标，隔离版全部拒绝。Unicode普通输入及单个计数内末尾NUL修复均正常，故这是有范围的新兼容回归。

微软[LinkFlags](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-shllink/ae350202-3ba9-4790-9e9e-98935f4ee5af)要求非Unicode字段采用系统默认代码页，并非固定CP1252；[StringData](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-shllink/17b69472-0f34-4bcf-b290-eccdb8de224b)的计数/边界与字段解码应分别处理。这里验证的是合成合法编码边界，没有运行真实Windows Shell。

**建议：** 保留计数消费、UTF-16及坏NUL检查，加入可追踪的代码页依据；不要用未支持的名称/图标解码器否决有效目标，也不要将坏参数默默丢弃。修复并补代码页对照前，建议隔离该组变更。

### Q4 / P2：打包CLI漏接引擎索引定位，任意cwd资源承诺不完整

位置：[新增打包:19](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/scripts/build-app.sh:19)、[CLI旧查找:130](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/Tools/ciderctl/Sources/ciderctl/ciderctl.swift:130)。普通CompatDB能从执行中的App定位Resources/data，`engine download`却仍只找显式index、CIDER_DATA、固定`/Applications/Cider.app`和cwd的data。

隔离签名占位包内的原生Foundation探针确认：自身index文件存在、Bundle资源路径正确、通用compat查找包含该路径，而engine候选列表不包含它。父代理逐字核对实际命令表达式。App位于out或自定义目录、无外部覆盖、从无data目录执行时，将失败；另一版固定安装存在时还可能用到另一版索引。没有为证明这一点真正下载引擎。

**建议：** 共用资源定位及明确覆盖优先级，核对自定义安装路径、无关cwd与另一版固定安装。这是接入旧CLI后的新集成缺口；不能将基础查找代码旧有等同于新增打包没有问题。恢复“独立可用打包CLI”能力前应处理或明确收窄承诺。

### Q5 / P3：新增损坏输入异常未统一处理

位置：[隔离预算脚本:88](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/scripts/windows-reference-budget.py:88)、207、342。嵌套authorization_id为数组导致TypeError，501位整数导致math.isfinite抛OverflowError；两者不在顶层异常处理内。实际均失败、账本未变，不能夸大为批准或预算绕过。

**建议：** 显式验证对象/字段类型和整数范围，再做浮点检查，以受控错误拒绝。可与Q1/Q2一起收敛，不建议为此扩大工具平台。

## 二、内核候选：改善成立，生产采用仍受阻

检查对象是0009/0010及其实际展开源码，而非只看patch标题。

### 已验证的改善

- token私有kernel-object列表、ops getter及创建时初始化与已有generic释放路径衔接；没有新wire协议/公开结构布局变化。对象登记失败释放未发布分配，sizeof相加有溢出检查。
- 精确抽取函数、模拟同一后端失败：旧版grab失败仍返回SUCCESS并增加本地ref；候选返回INVALID_HANDLE、ref保持0、输出不发布。已有正引用只增加计数，行为保留。
- MAXLONG案例：基线触发UBSan并绕回负数，候选返回INTEGER_OVERFLOW且不发布对象。void失败在离开自己的obref锁后显式抛出状态，局部锁未泄漏；这优于继续伪装已引用。
- primary正常模拟路径取得引用并关闭两个临时handle；TOKEN_QUERY、类型检查和stdcall导出与本次意图一致。两个PE、源码和日志摘要可以独立对应。

### K1：查找/获取引用不是原子生命周期操作

位置：[0009:210](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/engine/patches/candidates/0009-token-kernel-object-mapping.patch:210)以及server/device.c的get_kernel_object_ptr、free_kernel_objects；ntoskrnl的dispatch_free没有参与同一锁。

worker先得到未固定的proxy，再读header并grab；另一连接关闭最后handle/替换槽位并由dispatcher处理FREE，可在两步间释放proxy。确定性模型将这条经源码允许的交错插入查找返回前，ASan在基线与候选均检测UAF。它是**旧机制缺口，新增token路径继续依赖它**，不是已复现的Wine现场挂死或仅由此patch引入的漏洞。

[微软对象引用契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obreferenceobjectbyhandle)要求成功引用期间对象不能被删除。恢复为正常引擎前须证明lookup/pin、FREE交付、alias与最后释放的共同所有权，不应只保护本地计数。候选原报告已列这个限制，不能因披露而免除解决条件。

### K2：主令牌引用仍受宿主调用者的访问检查影响

位置：[0010:34](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/engine/patches/candidates/0010-ntoskrnl-primary-token-reference.patch:34)；ntdll NtOpenProcessToken→server open_token→alloc_handle→check_object_access，后者选择winedevice的当前token/primary。

拿显式process对象不等于token打开使用了Windows内核的目标主体/访问语义。query-only降低了不必要权限请求，不能消除身份错位。模拟token打开拒绝时，该函数返回NULL；[微软PsReferencePrimaryToken](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psreferenceprimarytoken)针对指定进程取得主令牌引用，没有将这种宿主DACL导致的NULL承诺为有效调用结果。

这是新增函数体的已知兼容缺口，实验里保守失败合理，但不能仅凭导出存在当作真实接口支持。需不同主体、不同token访问范围和正确引用生命周期的原生对照；本轮不通过修改身份或访问规则让游戏接受环境。

### K3：公共void引用异常改变了通用对象失败行为

位置：[0009:173](/Users/cervantes/Cider/out/audits/quarantine-20261014/isolated/engine/patches/candidates/0009-token-kernel-object-mapping.patch:173)。改变覆盖所有对象，不只token。局部模型证明抛出前已释放obref_cs，但没有证明每个驱动/工作项调用者能够完成自己的异常回滚、资源清理或满足native调用条件。[ObfReferenceObject契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obfreferenceobject)对有效对象描述的是实际引用，而非这个Wine后端失败处置。

将它视为明确的兼容失败策略；不要将NULL等无效输入的差异包装成Windows必需支持，也不要把显式异常本身称伪成功。必须核查全部调用方后再采用。constructor的NULL仍折叠NO_MEMORY、释放失败仍为旧日志处理；这些条件也未因新helper而完成。

### 性能与采用结论

primary正常路径至少涉及process handle、token handle、proxy查找及两个close，共至少5次服务端调用；首次映射/首次pin还增加请求。该静态成本不是游戏帧时间结论。纯计数mock不代表真实IPC速度；当前没有候选Wine/Windows性能结果。

**0009/0010可保留为待审实验稿，不能恢复到常用引擎或提升支持矩阵。** 本轮确认所有默认配方无候选引用、内核矩阵及游戏门禁未变。没有新成功空桩、隐藏Wine或反作弊特判路径；余下capture/impersonation/lock/privilege组仍未完成。

## 三、性能和可靠性：新增成本与旧问题分开

| 项目 | 本机隔离观察 | 正确解释 |
|---|---|---|
| ProcessScanner宿主路径 | 自有一个原生进程、普通2-drive目录：约0.095→0.171ms/轮；10,000无关项极端目录：约0.095→40.805ms/轮 | 新路径每进程重读dosdevices，存在随进程数/目录规模累加的成本；不是普通瓶子或用户卡顿实测 |
| 主队列FSEvents过滤 | 20,000深层无关路径合成批次：约135.8→259.0ms；另有16条真实私有目录事件 | 主队列URL标准化增加成本，但没有证明当前游戏场景会收到这种大批次；真实顶层事件覆盖修正确实成立 |
| App打包失败 | 新旧占位包签名失败时均丢失旧包；新增CLI编译失败则保住旧包 | 非原子发布是旧缺陷，新增签名扩大失败面；不能称全部新增路径已回滚安全 |
| LinkInfo块边界 | 新旧均接受声明块外的目标字符串 | 旧格式解释缺陷；未证明native内存越界或新权限提升 |
| 预算STOP/回执/大小 | 新旧共有检查后STOP插入、检查后换回执、写出超过读取上限的边界 | 原自审承诺应收窄；这些不是本轮新增的跨权限攻击证据 |

性能改善应先共享每轮prefix映射、缩短主队列路径筛选，再用代表性规模确认。无需为了极端合成结果撤销正确的安装器可见性或顶层目录监听，也不应据此声称用户的Control卡死根因又出现。

## 四、与原代码质量的客观比较

| 组件 | 改善 | 退化/欠缺 | 本轮建议 |
|---|---|---|---|
| ShellLink | 精确消费计数字段、修真实单终止NUL、拒绝截断/嵌入NUL | 错误代码页导致整link丢失 | 修Q3后再考虑恢复；基线自身已有严格cwd与终止NUL冲突 |
| ProcessScanner | 安装器宿主argv0可见、原PID身份与作用域保护保留，描述归属更准确 | 重复文件系统映射成本；真实installer完整生命周期未验 | 保留方案，补代表性验证及每轮映射复用条件 |
| FSEvents | CLI移动/删除的直接瓶子目录可识别，深层资产没有全部被当相关 | 主队列额外URL构造成本 | 可以单独评估；真实事件覆盖和大批性能分开 |
| CLI打包 | 通用compat数据不依赖cwd，构建前保包、内外签名顺序合理 | Q4；原非原子发布/TCC责任边界仍待 | 修资源定位，明确分发范围后单独评估 |
| kernel对象/primary | 修假引用成功/溢出，复用真实backend对象 | 所有权/主体/异常/释放及原生依据不完整 | 保持实验隔离 |
| 预算结算 | 同版计费、幂等、锁、原子提交和批准边界均有实验支持 | Q1/Q2/Q5，解析重复/版本语义未统一 | 禁止原样恢复；收敛格式和验证器 |
| 文档/证据 | 明确区分源码、编译、用户按钮、native与游戏结果 | 验证索引/自动可重做流程仍不完整，支持工作记录过多 | 保留有效证据并绑定具体恢复组；不把记录当目标进度 |

所以，**部分局部正确性变好，边界完整性和集成验证仍不够；不能给出“整体都比原版差”或“全都安全”的结论。** 原版在CP编码、LinkInfo、原子打包、回执IO等处也存在问题。

## 五、方向审查

### 未发现实质偏航的部分

- HY1的NUL、安装器观察、目录一致性直接服务程序启动/重启可靠性；当前版本首装和保存入口是有意义的验收范围。
- token对象引用及Windows独立参考服务HY2的忠实契约目标；Hyper-V仅是参考提案，最终Mac本地三款与全柚子社目标仍保留。
- 新源码没有改Preflight、WineRunner内建门禁、游戏Verdict、数据内核矩阵或默认引擎配方；未新增Epic路径、游戏重下载、iOS形态或Wine隐藏。
- 文档没有将新增候选宣称为三款游戏已适配。原生Win32既有结果未被改写成kernel结果。

### 执行需纠正的部分

1. **验证债务没有被编译消除。** 52–59记录多次构建、自审及方案，但没有native kernel结果；新增预算代码当时仅做AST/help/status，正好漏掉Q1/Q2。恢复前应使用已明确的小型对照，而非继续堆支持框架。
2. **旧架构偏差须正式登记。** ADR-007要求ciderctl经XPC；两版实际CLI直接派生Wine，属于旧偏差。新增打包使其成为分发界面，却只记录体积与数据路径，未闭合responsible/TCC责任身份。当前不能用这一点定性为新越权；应在未来采用决定中明确临时方案和验收/回退，不能继续声称完全按该ADR实现。
3. **HY1不能标全阶段完成。** 登录/验证码、完整更新、特殊参数/guest cwd、其他版本及长期会话仍待。允许独立准备HY2，不能绕过阶段缺项或提前改变游戏许可。
4. **准备工具应有止损范围。** 新结算解决真实pending的目的合理，原生watchdog与VM停止也确实必要；约40%的脚本增长不等于兼容进度。下一工作应聚焦一批现成reference kernel结果、一个依赖组和可否证的验收，不再另建调度/远控平台。

方向结论：**目标没有实质换路，但执行中支持性工作偏重、验证纪律不足，已有架构债务未充分闭合。** 当前不足以保证最终三款可达成；需要真实组件组合和游戏会话依据。

## 六、原始证据可信度

- 重新计算两个patch、基线/候选三份源码、两个最终PE及日志摘要；顺序以fuzz=0应用重建后逐字节一致，16项检查全匹配。report42的10份server锚点也全部匹配。支持“有对应源码/构建产物”，不支持“功能已全部通过”。
- 两份完整快照共核对255/271个Git blob。两个Windows cmd的字节差异正好对应仓库gitattributes要求的CRLF转换，重新git archive结果一致；其余字节与Git blob一致。没有发现审查期间改写快照。
- 读取原saved两会话、首装会话和恢复普通入口的session.json，选定env、工作目录、程序和自动profile与描述一致。实际schema把loader与argv[0]分开，后者是送入Wine的程序；没有把计划argv当guest cwd观察。
- 当前既有HYP62110只读元数据仍有中文locale/msync/DXMT，并无临时marker；路径中双反斜杠与实际argv0一致，本轮没有据其格式臆断证据伪造。
- 原图形/按钮确认来自本聊天用户回执，当前工具未独立重新获得那时像素。已退出的首装helper历史环境不能全部从现存父会话证明，也没有重跑用户安装/游戏；这部分保留原观察的证据等级。
- 最终构建为复用配置的增量模块构建，不是clean configure/完整发布。旧迭代二进制被后续构建覆盖，历史hash/log只能作为历史记录；最终产物仍可核对。
- 当前预算无新结算记录，原清理回执实测330.987且没有矛盾未测量标记，账本摘要与隔离前完全相同。Q1/Q2均在私有合成账本发现，不是已经真实多占Windows时间的报告。

没有发现需认定为伪造的上述已核对记录；尚缺的独立运行证据也没有被补成通过。关于生成期模型性能事件，本轮没有模型服务遥测、完整故障公告及逐次生成路由，**源码审查不能确定缺陷与该事件的因果关系**。可以确认的是具体软件行为和缺少的质量门槛。

## 七、逐组处理建议（尚未实施）

| 隔离组 | 建议 | 恢复前必要条件 |
|---|---|---|
| 024ab36 | 部分保留，修后拆分 | 修ANSI回归和engine索引接入；保留NUL/坏参数拒绝；明确CLI架构/TCC临时取舍 |
| 11a7167 | 可保留核心方向，单独验证 | installer/helper归属与停止、目录事件和代表性性能；配方文案绑定当前版本证据 |
| 84c4498 | 保持独立实验隔离 | atomic对象生命周期、正确主体/访问、异常回滚、native参考、整体context组，随后再讨论默认引擎 |
| eecba4f | 暂不恢复实现，提案可保留 | 账本版本拒绝/迁移、实测模式校验、闭合状态和输入错误；真实批准/回执核对另行进行 |

建议先处理4项确认恢复问题，再讨论恢复哪一组；不能通过压低校验或改写真实账本避开问题。用户尚未给恢复/修复授权，因此本轮到审查交付结束。

## 附件

- [Swift专项报告](/Users/cervantes/Cider/out/audits/quarantine-20261014/reports/swift-review.md)及其探针目录。
- [预算专项报告](/Users/cervantes/Cider/out/audits/quarantine-20261014/reports/budget-review.md)、[123夹具结果](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/budget/results.json)。
- [父代理预算复核](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/kernel/parent-budget-results.json)、[快捷方式复跑](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/kernel/parent-shelllink-results.json)。
- [精确C函数/模拟后端](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/kernel/reference-results.json)、[源码/构建重建摘要](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/kernel/provenance.json)。
- [既有会话核对](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/kernel/session-provenance.json)、[原始控制锚点](/Users/cervantes/Cider/out/audits/quarantine-20261014/probes/kernel/original-evidence.json)。

本机审查附件位于gitignored out目录，可在此工作区打开；报告中的隔离源码链接对应eecba4f快照，不能把其行号套在当前回退主线上。可长期跟随仓库的摘要另见`evidence/quarantine-review-20261014.json`。没有恢复实现。
