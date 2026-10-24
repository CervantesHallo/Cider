# Cider 项目持久记忆

更新：2026-10-24。用于后续接管时恢复关键事实；最新用户决定优先，实施状态以 `docs/plan/13-roadmap.md` 和实际源码/记录为准。不要把提案、编译通过和真实会话验收混在一起。

## 最新实际运行接续（2026-10-24）

94ca0f2已推送原主题两个源码端口和新探针。实际回执已归档：参数追加PE32/64各23检查零失败，未迁移Wine11.18实验基线五项预期失败；D3D9新双位宽/旧实验基线PE64各16检查、4096RGB读回匹配，非物理后端/呈现/着色器/FPS/游戏证明。清理P2经真实nonce/PID/childStillActive后断CLI，普通失败下9.586秒回收、GUI及两空观察确认；初版--wait按prefix空闲语义触发40秒超时，保留且不作目标通过。研究86及其evidence/out保存完整边界。当前无活跃子代理或实验Wine。更广U8/15超±3%仍未过，不默认采用/不提升游戏评级；接续原GS intrinsic/Fiber/异常/回调及媒体主题。Windows仍busy/零操作，定时/Goal不恢复，原HY顺序与目标保持。

## 当前接续：独立原主题源码准备（2026-10-24）

同步新证据已按用户身份skip-ci提交推送c30c050；更广U90次运行过但8/15超±3%，归因未过，不能默认采用或宣称完整M1。随后准备原R1主题：命令行追加0006修实际REG_SZ边界/保留原行为；自动renderer0004保留原NO3D/显式选项/AUTO GL→VK，均双PE完整翻译单元Werror零诊断、精确重建和独立源审无具体P1/P2，随后四PE模块已独立链接、私有新candidate与两个全新空瓶已初始化并清理，未整体验收/选配方。username路径固定上游已有；GSBASE整体切换与旧0008/0009/0011逐跨界独立源审已完成；新0x340已是syscall_dispatch，记源机制被上游替代、运行待验，不盲叠旧布局或从v0移除补丁。研究86/evidence与out/engine-profile-ports-20261024保存实际状态。Copernicus(01a16ccc-9c70-7f62-abe8-af02a0f7cef5)现在仅做实际父子进程参数probe只读审查（tests/runtime/profile-args/probe.c/run-mac.py新分支），probe双PE已编译、尚未运行，父代理做关键实现；Windows仍busy/零操作，定时/Goal关闭，用户瓶子及F27冻结引擎保持。

## 当前接续：补齐原N/U/S矩阵（2026-10-24）

F27原5.0倍、CS+0.6135%及自己的连续小时均通过；live hint双位宽各64轮回收及缺失unwind负例已闭合，6e8050a已推。U/S四次各114检查/负例与完整回执校验已推586a742。N补充四次各18检查也通过，修复两项夹具误报和解析类型遗漏，两个API-result double被正确拒绝；超时只有QPC观察、不加精度门槛。原U组2/4/8线程固定90次已完成，运行/身份/AC清理过，但8/15单元超±3%，归因未过，不重试挑样或放宽。WaitAny2/8/64与WaitAll2/8四实际运行各214检查正常；首PE32/msync回执因Python3.9 Counter零项差异误报false，旧字节保留，sidecar重验后只运行其余三项。研究85有全部数据；ThreadTimes在Mac可能回退进程CPU，不能冒充每线程计量。下一步核对计量来源与继续独立的原主题迁移，不改用户引擎/瓶子。Windows仍busy/不可重启、零操作，定时/Goal不恢复。原HY顺序和游戏目标保持；完整M1与应用级尚未完成。细节85，实际回执out/sync-nt-extra-20261024和out/sync-user-bench-20261024。

## 用户恢复推进（2026-10-24）

用户明确恢复并要求严格原计划持续推进，撤销下方紧急暂停的停止要求。接续F24真实命中/回退原因诊断，不重跑未变的性能队列、不降低5倍/CS±3%、不借旧小时；Windows仍busy/不能重启、不连接或改预算；不恢复自动任务/Goal，不切用户引擎/瓶子。暂停期间未提交内容继续保留，先归档F24真实结果再推进有证据的实现。

固定覆盖诊断已完成：原F24源码/二进制不改，另加全局原子计数、native退出后报告并按实际Windows PID绑定。ping-only22052次尝试仅148次成功，组合fork/busy/dirty门11927、未就绪9972；其他拒绝项零。manual始终就绪控制10000次全部命中，未就绪轮询正常回退。计数有观察扰动且包含warmup/运行时，只取快照最大值，不相加/不当未插桩占比或性能。诊断8b5b70ad…/1798e22d…，两个程序完成/时钟/身份通过，GUI关闭及两次空观察确认。公开clean源路径两架构已验证。下一步只研究首次owner获取前的有界CPU重试，不对未就绪对象新增循环；脏日志立即完整回退，原门槛保持。

F25/0121 revision2已实现：load-before-CAS、有界1..512，零超时caller传1、其他512；只在首次取得owner前重试，脏日志即释放/返回，原ready检查一次及全部保护不变。独立源码/实际x86二进制复审无具体P1/P2，300导出/四文件重建通过；精确新header恢复/try/ready-body两架构共6次通过，三个提交负例和dirty/mask/无界helper负例拒绝（无界者按2秒外层截止实际回收）。frame代码只复用逐字相同段证据，不借完整sync.c/引擎运行。初版CAS-only组件和负例构造编译错误保留；无Wine运行前已换revision2。实际pair69f27da0…/1798e22d…，自己的13阶段Wine正在运行；性能/小时未开始，不采用，详见80。

后续F25自己的13阶段和11份身份全过。固定AC六轮server10000/candidate2200ns=4.545454545倍速度未过5，CS0.982/0.978ns/op=−0.40733198%通过控制；不采用、不跑小时、不重试挑样。私有GUI及Wine已确认清零。接续先做F25覆盖细分，避免把F24组合门计数全归因为busy；同时只读核对raw __ulock_wake/sched_yield的Darwin取消语义，不在证明前扩大type guard、不改原yield。所有原目标/边界保持。

F25覆盖细分已完成：另一个私有插桩pair0da573e7…/1798e22d…，ping22052次中成功1016、owner预算耗尽11231、未就绪9800；dirty/same-token/fork关闭均0，manual10000次就绪全命中。两个诊断真实完成/身份/时钟及GUI退出+两次空观察通过，只取全局快照最大值，不能当未插桩占比或性能。Darwin限定源码调查与本机arm64/x86_64原生探针支持预解析raw wake/yield不提前交付DEFERRED取消，read正对照会交付；不代表整帧、信号或所有macOS安全。独立审查发现real-waiter有效性缺口，r2改为无rescue先join并要求target/waiter均返回0，双架构通过；v1保留。接续只审查cached普通SetEvent的封闭调用链，不先扩张保护、不删yield；完整M1仍未过。详见80/81。

F26/0122 revision2已进入限定cached普通SetEvent候选：预解析/预热raw wake及yield，原真正commit/通知/调度提示保持；实际Mach消息分支先mask。独立源审P2指出阻塞恢复仍持短control，r2修为完整mask后恢复原control交付边界，再执行原finish；r1从未Wine运行。两架构精确原生各32线程548检查通过，四负例拒绝；静态double对齐SIGBUS及unused构建失败保留，不作引擎结果。原ready caller已直接pin没有closed-cache漏洞，新caller沿用该顺序；早先口头“现有问题”已更正。组件/300导出/重建通过，实际binary复审及自己的Wine仍待，不采用；详见82。

F26 r2后续独立实际source/x86 binary复审无具体P1/P2，r1 control问题闭合；f7589506…/1798e22d…自己的13阶段与11份实际pair/PE/EXE/时钟均过、策略负例1260。固定六轮AC/LPM0性能正在执行，无并行构建/压力、无自己的小时或采用。生产App/两瓶/Windows账本四摘要仍相同。

F26固定六轮已完成：server10000/candidate2100ns=4.761904762倍未达5，CS1.118/1.036ns/op=−7.334525939%超±3%，两项失败；不采用/不跑自己的小时。原记录/原门槛保持，私有GUI内核退出和两次空观察确认。接续仅SetEvent实际命中/回退诊断，原F26产物冻结，不借观察数据当性能。

F26真实Set覆盖已完成：ping22050尝试/22049成功/1 gate回退，无Mach或恢复；manual控制10000次新helper成功。固定raw返回诊断22050调用中102为0、21948非0；不能自动称ENOENT或把0当woken数量。原F26冻结不改；补一个预解析/预热原生errno-getter的固定ping，消除分类歧义，同时限定只读审查等待登记的时机与丢失唤醒风险，尚未实施新组合。私有GUI和两次空观察已确认，各次诊断不是性能验收。

F27/0123已实施限定晚发布：F26r2 + F22饱和/碰撞保守hint布局magic7 + 64次只读预等待；额外fresh owner内发布/复核，EINTR/EFAULT不重新登记/补spin，deadline在late entry后重算，count0借用alert无条件sticky；blocking recovery/MANY/原commit/yield/取消pins保护保持。pair64547f41…/7b0cb109…，独立实际binary复审无P1/P2，8组新header原生、最终公开两架构late14线程321检查/Set38线程676检查和11个负例通过。夹具owner竞态、重试期限缺口、依赖/harness绑定P2均修并独立复审；初版回执/负例构造编译错误保留。取消探针只覆盖显式清理后交付，不代替Wine终止unwind。自己的13阶段Wine正在运行，性能/小时未开始，不采用。详见83；当前生产/用户瓶子/Windows不改，原HY顺序保持。

F27自己的13阶段与11份实际身份均已过，固定AC六轮首次两门都过：server10000/candidate2000ns=5.0倍，CS0.815/0.820ns/op=+0.6134969%。已声明并启动该精确pair自己的首个3600秒/8线程（旧v3 EXE7b77147c…，统计reader严格magic7），小时尚未完成，不借旧小时/不采用/不提升完整M1。状态见out/waitall-late-park/hour-state-v1.json，controller与driver PID在那里；外层3830秒、SIGINT让原runner清理，最后stop-runtime确认GUI退出及两次空观察。不能重建/改该pair源、输入对象/归档、EXE、runner、当前reader或切用户瓶子。可做文档和分离的只读/诊断源准备，保持Windows零操作。

F27自己的连续小时已完成并清理：guest3600016ms、78检查0失败，获取/成功归还均758497967，最终1/max1，无睡眠；3567有效资源快照账平、peak36/fallback0，3份不可用不当0，GUI退出+两次空观察确认。固定输入未变。短功能+5.0倍/CS+0.6135%+自己的小时已过，仍不默认采用/不提升完整M1或游戏评级。当前转补充真实Wine live hint回收诊断（out/waitall-live-hint-reap）：独立trace产物仅观察，原F27冻结；只读--hint reader及PE客户端已构建、纯协议9检查通过，观察器完整复审/实际运行未完成。新Lab前置必须确认hour状态终止且cleanup confirmed并取得wine-runtime.lock。Windows仍零操作、忙/不能重启；不恢复定时Goal。

补充live-hint诊断进度：CIDER_* trace开关被WineRunner过滤，未放宽过滤，改WINEMSYNC_TRACE_WAIT。新positive5e80379c…/7b0cb109…两轮真实0→1→两次0+join42通过；省略unwind的negative9d06aa7f…实际join42后48次读数仍1，被拒绝，STOP/CLI终止状态及GUI退出/两空观察已留证。完整64轮双位宽未跑；观察器四P2（阻塞读/clock/partial pipes/最终总限时）已修，14纯协议通过，真正文件复审仍等Laplace（01a16c99-e89b-79b3-aa68-7b38f5708909），未读文件的Lovelace报告不算复审。实验RootApp允许两个r3诊断pair，仍不切用户瓶子；原F27小时已通过，Lab当前无实验Wine pending。

补充真实Wine live hint已闭合：真正文件复审确认四P2修复后，PE64/PE32各64轮baseline0→登记1→TerminateThread/join42→两次0，128轮均过；故意缺失unwind的negative第0轮join后48读数保持1被正确拒绝，STOP及CLI被终止状态未伪装成功，GUI及两空观察确认。diagnostic正5e80379c…/7b0cb109…、负9d06aa7f…/7b0cb109…；原F27正式5倍/小时证据仍仅64547f41…/7b0cb109…。当前按原research17§7清点更广N/U/S及应用级缺口，不能由一个auto-event gate/CS控制/小时宣布完整M1。Windows仍busy/不能重启，不操作。

已按用户身份skip-ci提交/推送6e8050a（F27原性能+小时+livehint证据/源码）。当前回原research17完整N/U/S矩阵，新增U正确性主程序tests/sync/sync-matrix.c/.h，两架构main组件已编译0警告、尚未链接/运行，不是新性能cohort。run-mac.py只增加matrix correctness入口，旧F27 runner以Git6e8050a/原hash保留。S组代码限定委托Poincare(01a16ca7-6fd0-7292-96c3-270dc5a5441e)写sync-server-cases.c，父代理自己实现U与公共harness；另限定复审U/runner。不重跑原小时/已过Win32基线、不恢复自动化Goal、Windows零操作。详见85。

新增U/S正确性四次（PE32/PE64×msync/server）已各114检查0失败，清单一致，原64547f41…/7b0cb109…不改。新夹具两P2和S即时完成宏契约已修；SRW实际持锁交叉TryAcquire、省略shared锁负例确实失败。完整清单JSON及sum/type校验、离线四回执复核完成；这不是U±3%归因基准或完整M1。实现与结果在85/out/sync-matrix-20261024；下一步按原矩阵补U性能和N/S剩余项或应用级前置，不借旧小时。无子代理活跃、无Windows/定时/Goal操作。

## 用户紧急暂停（2026-10-23）

用户明确要求紧急暂停。已停止当前私有实验GUI（PID42885，内核退出确认）并连续两次观察到无自有Wine进程；旧实验室亦无pending。不要继续研究、构建、测试、提交/推送或启用自动任务，等待用户新的继续指令。

暂停点：F24自己的13阶段及11份native/PE身份已通过，但独立AC六轮4.434782609倍/CS+11.0273327%两项失败，未采用、未开始自己的小时。公开frame/body夹具和源提取器两架构已通过；公开clean夹具是已运行私有源的逐字副本，尚未以公开文件路径重新编译。实际命中/回退原因诊断尚未开始。0120补丁、公开夹具、79报告及记忆/计划改动仍在工作区；最新已推送提交a33eda9，未提交内容均保留。生产引擎/用户瓶子未切换，Windows未操作。

## 当前检查点（2026-10-23）：F22 与真实取消状态

继续原02的WaitAll Mac验收；Windows仍busy/不能重启，零连接/零预算变更，不恢复定时/Goal、不切用户引擎/瓶子，保留原HY顺序及游戏门禁。

F21诊断overlay实际报告198674帧old-state ENABLE、DISABLE零，双方请求QoS21；不代表有效调度/核分布。原guard保持，永久DISABLE未采用。F22/0118基于F21追加65536个保守提示桶（256KiB，magic7），Mach计数字/原槽/pins/取消/事务恢复不变。独立设计/实施复审的借用alert槽类型变化P2已修：非零timeout count0无条件sticky，零超时不登记。两架构精确生产header恢复/提示与实际ulock ABA、五个x86负对照拒绝、自己的11阶段/九份身份核对全过；新增两位宽单事件64轮各2179检查、128线程回收、64次终止/重新唤醒通过。

最终pair c9d5ec14…/f5053062…；六轮固定v4完整结束：server10100/candidate2800ns=3.607142857倍，CS1.379/1.252ns/op=−9.209572%，两项失败，不采用/不跑自己的小时，不借F7/F8小时、不重试挑样。两个独立串行资源观察实际新文件分配上限344064字节/旧81920，总进程RSS差未归因到提示区；F22目录有旧模板文件，不等于活跃映射。所有原始失败和模拟wrapper错误保留。完整M1仍未过，具体见78及汇总证据。

后续先核对固定成本与保持完整取消语义的可行实现；不可借永久DISABLE、私有pthread字段或不可靠线程归属追性能。用户已确认隐藏Codex，不重复要求；原防空闲睡眠断言保持，不能防合盖。锁屏可继续，Windows仍不操作。

同日F23/0119基于F21、只加普通事件64次有界只读预等待轮询；不含F22，共享header/取消/Mach不变。限定独立审查/零警告构建/300导出/精确重建，自己的13阶段及11份实际pair/PE核对通过。六轮v4为4.50倍、CS+33.2061%，均失败，不采用/不跑自己的小时。新wrapper scope旧字样独立注记，实际命令及哈希正确，原回执不改。不要删Highball0005的yield追数字，它有原计划采纳的游戏调度用途。正在核对一种仅帧内DEFERRED type guard的可行性；只作源码调查，未知取消点未证明前不实现/运行，原guard保持。

后续调用图复审和真实native read取消点证据否决“整帧type替换”。F24/0120转为封闭的cached/ready/count1/nonalert普通事件尝试：全程保留短control guard，仅CPU操作，原提交获取；任何slow/recovery/unproved分支在同帧先DISABLE再恢复type和清pin，末引用及kernel wake前也先mask，原完整慢路径保持。r1审查P2发现编译器把TLV解析提前；r2彻底移除ready函数TLV，以native record.retired覆盖fork child，独立反汇编/父代理核查闭合。最终native c97d1a9e…/1798e22d…，新header原生恢复/try/提取ready-body两架构及负例通过；提取实际frame函数两架构28用例/各1448检查通过（有明确test doubles，不是Wine/信号/Windows等价）。原夹具交接timeout、mock编译错及新增场景的错误正常返回预期均保留，不能当引擎通过。自己的13阶段Wine正在运行，性能/小时未开始；详见79。用户再次确认接电开盖，系统读回AC/充电，下一正式队列将独立AC/LPM0，不混旧电池；不恢复自动任务。

同日后续：F24自己的13阶段/11份实际身份全过；独立AC六轮v4完整结束，4.434782609倍/CS+11.0273327%，仍失败，不采用或跑自己的小时。已公开精确源提取器及frame/ready-body/try夹具，公开frame/body两架构构建运行通过；初版提取器结束边界错误保留。接续只先做实际命中/回退原因诊断，避免假定当前负载使用了ready-only分支；所有旧结果和原5倍/±3%不改。

## 前一检查点（2026-10-23）：生命周期失败与运行器修正

用户继续要求严格原计划推进；Windows仍busy/不能重启，本轮零连接、零预算变更，不恢复定时/Goal、不切用户引擎或瓶子。现有防空闲睡眠断言保持，不把它当合盖保活保证。

F20首次功能实际失败：PE64生命周期第46轮child_ready在原5000ms超时，宿主活动38.218秒/墙钟38.219秒，不能归因于睡眠。原11阶段在第5阶段停止；随后只补跑原先未执行的6阶段，均通过；9个driver实际native/双PE身份全部相符，但只有8份通过，F20整体仍失败，不运行性能/小时，不默认采用。

有界诊断全部保留：最初两轮、8轮对照、额外4轮64轮及双方各一次512轮。8轮中两次worker退出20伴随约21527/525秒墙钟间断，另一次F19来宾通过也有约510秒间断；旧passed字段/原始字节不改，离线注记不能将它们计作无中断验收。新4轮64轮均通过，未捕获>1秒启动等待；512轮双方各46100检查零失败，但约6.89MB日志超过4MiB接受上限，runner拒绝，离线只作诊断，不追认正式通过。均未复现原第46轮超时，不以重试抹掉失败。数据见77及汇总证据。

修正tests/sync/run-mac.py：原来只在duration检查宿主时钟，现在所有模式检查正反方向>2秒间断、停止自有实验进程并拒绝验收；最终总时差兜底，记录runner SHA及诊断行。7项纯时差边界检查及实际短运行通过。独立复审进一步修复启动/清理错开与超时输出丢失两项P2：改为主线程独占提交/停止，用run UUID精确绑定session；中断先确认私有GUI内核代次退出，再反复清理Wine并要求两次空观察。6个假transport故障注入通过（不执行Wine），真实8秒timeout最后一轮保留17707行并确认host退出/无pending；前两轮旧helper的AppKit缓存未确认保持失败，不追认。标准夹具原5000ms不改，新增独立诊断EXE只在已经失败后停留10秒采样；迟到就绪不能改成功。原F7/F8小时/性能门槛5倍和CS±3%保持，完整M1/Windows内核/游戏评级不提升。

当前源门不是FIFO，存在热调用反复抢回的理论风险；尚无失败现场栈，不能宣称它造成这次超时，亦不据此盲加yield或更改恢复协议。F21/0117从F19分支，仅让native frame复用data供原control begin/end调用；不包含F20、不移除信号fence/pthread取消/准入/对象pins/原事务。限定独立复审无具体P1/P2，三文件精确重建和SDK15.4/O2组件零警告构建、300导出一致，6b52df80…/1798e22d…。自己的11阶段及9份身份已过；固定6轮v4完成，server9900/candidate2200ns=4.50倍，CS0.884/1.248ns/op=+41.1765%，均失败，不采用/不跑自己的小时。接续先对照上游协调锁设计再选实现，保持现有事务/死亡恢复/SC唤醒序，不继续堆叠缺乏实测收益的局部候选。外部资料连接暂不可用，改先做现有原生成本诊断：单线程取消toggle一对约337ns/getSpecific约3ns，仅为诊断、不能归因实际Wine。Apple本地手册和独立源审确认永久DISABLE会改变frame外POSIX行为、epoch注册不能证明线程归属；该策略未采用。正在构建out/waitall-thread-observer诊断overlay，原disable/restore不动，先收真实old-state/QoS；非性能候选，不提升门槛。

## 前一检查点（2026-10-21 接续）

用户最新再次要求睡觉期间持续推进，除重大问题不停；在本聊天继续，不恢复定时/创建Goal。已收到隐藏Codex确认，不再要求重复准备。Windows仍busy/不能重启，未连接、未改预算；原HY顺序/最终游戏目标不变，候选均非默认。

完整v4 F17/F19固定12次比较结束：F17=4.2083倍/CS−17.1218%，F19=4.3913倍/CS+4.9340%，仍未过5倍/±3%，不采用/不跑自己的小时。F17/F18 v3全队列、旧10份完整队列与两个2/6准备失败都保留。背景只记录，不作为新增验收门槛。F7/F8已通过小时不借给后续候选，完整M1仍未通过。

F20 out/waitall-native-id-prototype/0116基于F19原O2：仅复用同线程native data->tid与已有data指针，省外部PsGetCurrentThreadId包装和重复pthread_getspecific。已核查上游宏本来就用native tid，不存在本轮确认的guest-TEB身份错误；反汇编也不支持“额外Windows ABI桥接”猜测，不能这样归因。header/sync/server均与F19相同，限定独立复审无P1/P2，已构建，自己的11阶段Wine验证正在运行。自己的性能/小时尚未开始，仍须原5倍/CS±3%。

源码调度核对：GUI命令队列已明确userInitiated，Wine正常线程也设置Mach策略，未确认CLI优先级错误；Wine前台GUI本来申请防AppNap活动，console夹具是否受到背景策略影响尚无实测，不据此改宿主设置/调度。继续有证据的实现与验证，不盲改系统策略或放宽门槛。

## 前一检查点（2026-10-20 夜间继续）

用户再次明确“持续推进，睡觉期间除重大问题不停”；继续本聊天，不创建定时或Goal，不再要求重复隐藏Codex。Windows仍busy/不能重启，完全未连接/改账，原HY顺序与最终目标保留。

F17/F18原门槛12次比较完整结束：F17=4.6364倍/CS−15.6450%，F18=4.4348倍/CS+22.3094%，两项均失败、不采用/没有自己小时；10份历史完整失败、F17两个2/6附加CPU-gate失败全部保留。附加quiet/CPU准备门槛不采用为验收标准，原5倍与CS±3%保持，背景如实记录。不得追认旧数据通过。

F19 out/waitall-single-commit-prototype（0115，基于0113原O2，不用0114编译配置）：count1专用提交，CLEAN仅未发布准备、COMMITTED决策、原子64payload、始终持owner正常通知，失败APPLIED交原helper；多对象/恢复/租约/上界/架构不改、3版server兼容。独立源证明无P1/P2，新增原生单槽死亡/未发布输入/待交付/无变化与三负例通过；自己的11阶段Wine短功能及9个native/两PE身份核对通过，pair72851e70…/1798e22d…。仍非默认，无自己小时/完整M1或Windows/游戏结论。

下一步按预声明v4-original-gates-v1固定12次比较F17/F19各三对。双方仅用既有v4 EXE0ec427ff…（取消读替代共享CAS），旧v3数字/早先F8v4失败保留不混用；5倍/CS±3%不变，无额外CPU上限/剔除/自动重试。性能两项都过者才跑该精确引擎自己的连续小时；压力可用原v3 duration夹具，记录不同用途EXE的SHA、不混作性能样本。

## 先前接续（2026-10-20，用户已隐藏Codex）

已收到用户“已隐藏”，不再请求重复操作。严格quiet探针仍失败；两个附加background-cap实验均失败于2/6，原文件保留，不拼接/改为通过。CPU/零pageout是agent额外测量准备提案，现不采用为验收前提；回到原02的5倍/私有CS±3%硬门槛，背景照实记录，没有降低原计划标准。

F18 out/waitall-thinlto-profile：F17源码四文件SHA逐一相等，只将msync.c/sync.c编为-O3+ThinLTO并单worker链接，server/PE沿用，不启用fast-math/新ABI旗标。native3246badf…/1798e22d…、300导出集合一致、独立限定复审无具体P1/P2；自己的两位宽11阶段功能和9个native/两PE身份核对已通过。当前25个链接输入摘要保存，历史未记录输入不能追认。

正在按预声明original-gates-v1固定12轮比较F17/F18各三对，观察背景，不设置额外CPU门禁，不剔除/自动重试；旧10份完整失败与F17两份部分失败均保留。所有候选非默认，无自己的小时/完整M1/Windows/游戏通过；若某一精确pair性能两项均过再跑它自己的连续小时。Windows仍busy/不能重启，未连接或改预算；不改用户引擎或新建定时/Goal。

## 当日先前检查点（2026-10-20）：继续 WaitAll，性能仍未通过

用户最新“继续干”；本轮只在 Mac 的独立实验室推进。Windows仍不能重启/busy，不连接、不改账本；不用新定时/Goal。原HY顺序和最终游戏目标不变，所有同步候选均非默认，生产App/引擎/用户瓶子不切换。

F7/0103与F8/0104各自的一小时真实并发压力及两位宽功能已通过，不重复。十份完整独立电池性能队列均未达到5倍：F8、F9、F8-v4、F10、F11、F12、F13、F14、F15、F16；旧AC13/15保持原字节、不拼接。最新F14/0110实际完成11阶段短功能、精确native pair及三对正式比较：server10200/候选2200ns=4.6364倍，CS−7.5949%也未过±3%；不采用/不跑自己的小时。F13/F14/F15补丁和native回归本轮归档。

新F15只在原F8的owner-held commit内复用已pin的槽地址，避免同槽反复间接查询；全局门、租约、journal及通知路径不变（magic3）。私有out/waitall-cached-slots-prototype两组件编译通过，native header恢复/旧helper/竞争及三负对照通过，11阶段Wine短功能和精确pair核对也通过，三对性能4.5455倍失败/CS−1.4634%通过，不采用/无自己的小时。native fixture已改为真实页保护在第二个payload store截获死亡，不再把中断注入依赖于重复slot回调；增加过滤后槽顺序/空提交检查。一个范围受限代理只读复审无具体P1/P2，父代理整合。F16转为原F8的sync.c native frame缓存当前thread_data（实际反汇编有重复pthread_getspecific），只改本帧私有字段/函数，保留取消/controls/pins；组件和11阶段实际功能/精确pair已过，三对4.3043倍/控制−13.4585%均失败，未采用/无自己的小时。无候选借F7/F8小时，门槛不放宽。

F17/0113只组合原F8上的0107+0111+0112三个机械局部优化（独立三文件补丁，不选入配方），父代理审查交互，复用F15完全同header原生正反结果；组件与11阶段Wine短功能/精确pair已过（73294700…/1798e22d…），新性能/小时未开始。

正式比较的新增外部条件：测后观察见Codex renderer近89%CPU、WindowServer约41%、swap约7.19GB，不能由测后快照直接归因全部旧数据。CUA明确禁止控制com.openai.codex，未绕到其他UI手段；已异步请用户方便时⌘H隐藏窗口、保留应用运行。旧失败不改，新安静条件需独立预声明/同pair三对/同5倍和±3%门槛，未答复不视为已隐藏。实际三次5秒累计CPU为Codex49.12/47.05/58.83%、WindowServer43.78/45.28/44.96%，pageout+72，低背景探针未过；F17正式protocol/result未生成，不能绕过guard或自动重试。先待用户隐藏界面后核资源条件；Windows无需操作。

本轮收尾：F17组合11阶段/精确pair通过，原测试Wine与私有GUI/bridge均已退出，受保护的用户App/两瓶配置/Windows账本4文件SHA仍匹配。现有caffeinate不改，无新定时、后台性能或小时任务。接续先收用户隐藏Codex的状态、做一次新的有界低背景核对，再按事前协议执行F17自己的三对性能；不重跑未变化的Win32/小时基线，不跳过guard。若性能两项过，再启动该精确binary自己的连续小时。

记录勘误：部分后续性能protocol的implementation文字从TLS实验误复制；原始回执不改，须以EXE/native pair、实际补丁SHA和单独erratum核对，不能把错误标签当实现身份。正式测试和诊断的证据仍严格分开。

## 先前检查点（2026-10-18至19，历史）

## 电池正式性能已完成，性能目标仍未达标（2026-10-18 夜间）

用户明确要求继续、将保持电脑开机；不新增定时或Goal。锁屏可做后台命令，合盖/睡眠仍须按真实时钟判中断。**正式性能可以在电池供电下测：单独预先声明同供电/LPM0的三对比较，不混入旧AC样本。** 把交流电说成所有正式性能的绝对前提不准确；旧13/15 AC队列仅自身续测须同条件。该文件SHA4b3311654891a5567edc2a69ee932b051ad4e36a48721c6aa3f3e6fcd03997e6、只读v2 EXE归档都保持。门槛仍为同native pair server/候选单事件p50三轮中位≥5，私有CS控制相差≤3%，不挑样/放宽。

已实际完成、各有独立原始回执的电池比较：F8/0104 v3：server10100ns/候选2200ns，4.5909倍未达5，CS+2.1019%通过控制；F9/TLS机械合并v3：10100/2200ns，4.5909倍未达5，CS−5.0895%控制失败；F8新v4夹具（仅bench stop标志从共享CAS改为acquire读）：10300/2200ns，4.6818倍未达5，CS+10.4962%控制失败。不能用控制失败数据把RTT变化归因于引擎。三个cohort在admission、tls-prototype、benchmark-v4目录各自paired-perf-battery*.json，旧AC和旧电池数据不改。F9两位宽基础/生命周期/八线程/四进程及20秒短压、无害子程序1260共11阶段已过，但没有自己的小时验收；F7/F8两小时结果保留不借给F9。F8最后电池功能已在8c563dc归档。

继续保留原子WaitAll/恢复/原槽/Mach/ulock/inproc，不改用户引擎。只读未就绪快路F10/0106已在out/waitall-snapshot-prototype完成成对magic4构建、独立静态审查、生产header测试/四个负对照及11阶段Wine功能（9个运行阶段均独立核正确native pair）。正式三对电池v3为10100/2200ns，4.5909倍仍失败，CS+1.6209%通过控制。没有主要性能收益，保留非默认、未跑自己的小时，不借F7/F8小时。根下stats-snapshot helper已明确兼容3/4相同统计布局，原3版另存，不把不可用读作0。

当前关键实现转为更小的F8分支out/waitall-local-state-prototype：只在函数调用内复用当前native thread_data，避免重复pthread_getspecific；msync_control_depth只有本执行线程写、信号handler仅读/延迟，使用原子load/store和原signal fence/replay替代多余RELAXED RMW。共享协调头、fork SC准入、pending signals exchange、租约/提交/线程退出语义保留，无F9/10快路。两替换组件构建0警告，独立信号/重入/生命周期审查无具体P1/P2；11阶段实际短功能通过，9个driver阶段正确native d31f2a16…/ce78d67c…已逐一核对。固定三对电池性能也完成：10100/2200ns=4.5909倍，CS−14.4611%控制失败；没有主要收益，不采用/未有自己的小时。继续停止堆叠快路，先用独立非验收二进制做原F8的分段wall-time诊断；有observer effect，不计性能验收，不能当精确CPU归因。TLS静态调用数和Rosetta采样不当性能结论。d05ca5d已按用户身份skip ci推送，旧AC/电池各队列不改。

分段诊断已实际完成：out/waitall-phase-profile，私有非验收native4d34919d…，复用ping-only EXE eebfd4b5…运行1m次/worker、2线程，实际client68076，35检查/0失败，26条两线程报告、50ns桶/每64次采样。Set commit p50桶600–650ns、wait gate950–1000ns，含observer effects/墙钟等待，不当CPU或性能门槛。wrapper最初把complete.ok误读成passed，原失败回执保留；profile-reanalysis.json按原logSHA离线验证ok/failures/output_error及全部桶计数，无重跑/改旧回执。

当前更有针对性的F8分支out/waitall-deferred-notify-prototype：通知提交保持APPLIED，复用既有coord_finish恢复路径在本执行者不持owner时同步完成通知，不提前CLEAN。所有10个生产commit调用点均finish；独立审查无P1/P2，同时指出旧helper要4次owner往返。revision1仅native已过；revision2在匹配sequence/APPLIED并标CLEAN后保留已重新取得的guard返回，省第4次重取，其他情形仍原leave/continue。revision1冻结*.revision1。revision2的生产头死亡/旧helper/通知scope与3个负对照、配对构建及追加审查均已过；Wine11阶段短功能通过，9个运行阶段正确efd16cac…/f7e15666…逐一核对。固定三对电池也完成：server10100ns/候选3900ns，2.5897倍失败，CS−0.1002%通过控制，说明该版本实际更慢，不采用/无自己的小时。共享magic3/原子提交/租约/epoch/信号/取消保持；额外gate往返和重复建议唤醒是明确代价，本次实测未换来改善。下一步从F8原子协调器继续，先审查“提交者在仍持有门时取得自己日志快照，门外通知后以版本/phase原子tag确认完成”的直接finish方案，避免现版本重复取门；目前仅设计，未修改/运行/采用，不借旧结果。

2026-10-19接续（F13）：新的out/waitall-tagged-finish-prototype保持F8全局门/租约/epoch/native frame及COMMITTED决策点，将serial+phase合为一个uint64 decision（magic5、serial最大2^62−1），由确实完成通知的helper以完整APPLIED tag CAS清理，禁止旧helper清掉新代日志。提交者仍持门时取得自己日志/slots快照，离门同步通知后CAS，不重复取门；enter仍返回owner-held/CLEAN，finish只保证自身通知完成，新日志可pending。所有10个生产调用点已审查，均容许spent guard/再次enter；所有成功获取仍走原门，不是API成功空桩。独立只读审查最终header b4512d31…无具体P1/P2。count的短路赋值警告已改为提前读取，最终配对组件0警告；native38fd341b…/c566c542…。

F13生产header原生死亡/旧helper/暂停liveowner/竞争、通知scope、最大serial构造输入均已过；旧under-owner/省通知/省版本guard/CLEAN-before-notify/允许wrap五个负对照全部拒绝。早先告警编译和初版crash注入资料另存initial-compilation/initial-boundary，不算运行通过；最终death-before-notify在callback入口无条件退出，使提前CLEAN负例实际丢通知被检测，而非跳过注入。新的私有成对engine/stats reader（明确兼容3/4/5）和空瓶已准备，私有GUI已构建并开始实际11阶段短功能，Wine11阶段短功能已通过、9个运行阶段native pair核对正确；固定三对电池3.8µs/server10.1µs（2.6579倍），CS+9.6491%控制失败。未采用/无自己的小时，不借其他候选。下一步转回原F8的保守等待者计数提示：省无阻塞者时的ULock唤醒，保留有等待者时原内核路径。仅设计尚未编码/采用；无子代理。f94b3f2已推送，生产App/EnginePolicy/用户瓶子及Windows未改，门禁和原HY顺序保持。

F14已编码/配对构建，原生hint helper测试与3个负对照通过（实际KernelPark=true；clear-count负例真实timeout−60），独立只读审查无剩余具体P1/P2。最终publication实现的magic6 engine/GUI已构建并完成11阶段Wine短功能、9阶段正确bb682b03…/cdb9dff3…逐一核对。固定独立三对电池v3已完成：10200/2200ns=4.6364倍未达5，CS−7.5949%控制失败；未采用/无自己的小时。以下为实现边界：已读真实源码，原thread.c清理顺序本来就是msync_unwind_waits→inproc_thread_cleanup→msync_thread_cleanup，因此新的single注册可借现有frame pin，不需要改thread_data布局或加共享引用。server的multiple_waiters原本是完整节点计数而非boolean，不能盲目改bit0。拟采用共享29位总提示计数+PUBLISH位+Multi标志+sticky原路径fallback、pump局部multi计数，所有活对象提示写均SC RMW；single栈记录沿既有wait_registrations，正常/终止精确退出，SIGKILL只能留下保守高估（不持新增ref、不丢许可）；计数极限只关闭优化、保留原唤醒，不钳制任何Windows对象数据。SC发布/park的ABA及sticky已由确切helper+原生ULock验证；真实生命周期/跨进程和长压仍待。先前无条件移到Mach泵仅审查过源码、未实施，不当交付。

Windows仍busy、不能重启，未连接/改预算/重跑Win32。生产EnginePolicy、用户App/两瓶/Windows账本保持；原HY1→HY2→HY3→HY4/HY5与最终游戏目标不变。源码/构建、Mac运行、Windows kernel和游戏会话分别标状态，所有候选非默认、完整M1未通过。仅一个范围明确只读审查代理，父代理做关键实现；Git作者/提交者仅用户、skip ci、无Actions。

## 先前运行检查点（历史，最新以上方为准）

本轮用户明确授权完成 Mac 并发运行与性能验收。完整 Wine11.18 已构建，在 `out/waitall-mac-20261018` 的私有 CIDER_HOME、独立 bundle id `org.cider.waitall-lab` 的签名 GUI/XPC 中执行；生产 EnginePolicy、常用 App/引擎/瓶子和 Windows 账本未改。0102 OFD 正常路径真实 RTT19.2µs，性能失败；保留原16字节槽/Mach/ulock/inproc、原子提交和内核死亡证明，0103改为原子owner+独立OFD租约，正常状态路径无fcntl。

实际暂停/终止用例发现fd_cache_mutex遗留、inproc调用引用和泵等待节点退出清理问题，已修；补线程代际Mach登记/退休、native映射请求清理、APC在释放引用后分发。两位范围明确代理已完成独立源码/测试审查，发现问题由父代理实际修复；最终fast7限定复审无剩余P1/P2。POSIX取消在完整调用帧统一保护，正常Windows暂停/终止仍单独处理，未禁用功能/钳位许可/放行游戏。候选0103已导出并zero-fuzz/offset重建9文件，最终native pair见私有fast7-native-pair.json。

fast7实际PE32/PE32+基础825项各通过；两位宽生命周期各64轮通过（各5780检查、64暂停/恢复/终止、64子进程、64被终止WaitAny）；8线程100000次/worker并发通过。45秒预检约928万次sem获取/归还、峰值1、实际泵约150万次分配回收保持有界。前两次小时尝试分别被822秒和249.961秒合盖睡眠中断，均退出137并真实清理，不拼接算连续通过；驱动已比较宿主活动/墙钟并拒绝睡眠。用户明确“已开盖接电，可保持65分钟”后，第三次`fast7-duration-one-hour-attended.log`已通过（session9912退出0，guest3600019ms/宿主活动3607.786s，78检查零失败，无检测到暂停），约6.54亿次许可获取/归还相等。3530资源采样中起始一次67不可用，3529有效样本allocation-free=current；节点峰36/回退0，FD启动/退出23–295而非全程恒定，边界见77。用户随后再次报告插电，11:55与11:59 UTC的pmset已实际确认AC/充电；锁屏可继续，合盖仍可能睡眠。

`after-hour-perf.py`只绑定第二次失败尝试，已退出1，没有启动后续；不要误认为仍在监控第三次。独立固定v2三轮短比较已完成：同EXE250k/4线程，单事件signal→peer-return p50候选2.2µs/server10.4µs/旧原版1.3µs，4API RTT4.4/20.8/2.3µs，两者4.727倍均未达5；私有CS控制中位1.113/0.897ns/op，+24.08%也未达±3归因要求。保留全部原始轮次，不将噪声直接定为后端回归，也不剔除或放宽门槛。v1的RTT4.79倍失败独立保留。单事件包含arming/disarm/QPC开销，接收者可能已就绪，不是隔离kernel wake测量。

继续性能工作：单文件process-local准入原型在`out/waitall-admission-prototype`，SC每线程发布/重查取代共享计数，fork仍扫描并等待注册线程、父进程刷新全部FD/token、子进程退休；全局事务/租约/提交/通知不改。原型source685defe8426d、native ntdlle6054a73212a，已零警告编译链接及单代理只读对抗性复审，未采用。它的独立`out/waitall-mac-20261018-admission`/bundle id org.cider.waitall-lab.admission已构建，正在实际功能验收；原F7 GUI/产物/小时运行保持。没有在压力期间测性能。

控制环v2约0.1秒，对同一PE路径易出现离群；v3计划在保留v2失败前提下固定延长CS warmup16m、测量倍率64→1024，wake/RTT采样不变。复审指出旧warmup元数据未同步，父代理已改为单一bench_warmup函数后重建，尚无v3性能结果。新增实际跨Wine进程竞争夹具由同一范围受限代理实现独立文件，父代理审查/集成，不能把代理报告或native coordinator4进程结果当真实Wine跨进程通过。第三次F7小时EXE保持v2原字节，不受源码v3编译影响。

研究77与结构化证据结束后回填；0103非默认、所有M1/应用/游戏/Windows证据独立。现有caffeinate只防idle，不绕过锁屏/合盖限制。Windows busy/原预算保持，不连接、不重启、不加heartbeat或Goal；所有人类环境初始hash在protected.json，结束再核对及清理。CIDER_HOME不隔离NSUserDefaults，今后私有GUI首次启动前使用独立bundle id；早期副本改id前无完整偏好快照，不声称所有用户偏好逐字节未变。

补充：跨Wine进程夹具已完成393行、两架构编译0警告；父代理独立审查与集成，独立复审新增metadata一致性P2已修，抽取函数11正/负输入通过（仅离线检查）。F7小时完成后单次后续批次session40928运行两架构新夹具及原版观察负对照；不假设原版必失败。两版小时均通过后session57639按预先声明v3固定15轮测量，尚未执行；这些是本回合一次性命令，不是定时/heartbeat。Carson已关闭，当前无子代理。两小时实际guest父进程分别核对为各自独立GUI，证据current-hour-parent-proof.json。

F7后的session40928已退出0：四自有子进程×四role×100000，PE32/64分别37176/37177、40714/40715归还/获取，初始1/最终0平衡；原版同夹具也通过，不能声称动态复现旧竞态。父代理归档时发现metadata重构漏写嵌入rows；已补且四个最终serializer AST正/负输入通过。四原receipt不改，从完整原始日志另建recovered-rows sidecar并核SHA/全部断言，公开证据明确标注重建；未因归档重跑。v3未来运行已用修正writer，F8小时旧加载脚本不受此改动影响。F8小时session42042仍跑，预计13:05 UTC；之后session57639固定15轮v3，最终须核同pair/所有行。

另有`out/waitall-tls-prototype`仅静态准备：机械合并原retired/record到一个TLS对象，两个热getter各由2次间接TLS调用降为1次，编译链接0警告；未执行、未私有绑定或公开采用，不能当性能改善。原F7/F8模块与九源摘要再次确认未变，当前无子代理。

下方为上一源码阶段记录；本轮结束须用实际结果更新research77、02/13和本检查点，再按用户身份[skip ci]提交推送。

## 合盖前最新检查点（2026-10-18 21:09 UTC+8）

用户重新接电后明确只有5分钟即合盖。两版小时均完成：F7与0104准入候选各3600秒真实guest，均78检查0失败，无检测到睡眠；后者获取/归还643049167次相等、最终1。F7追加PE32八线程100000/worker也通过，最终driver真实回执含176行/无reportedErrors。0104的最后PE32 stress/两位宽跨进程及新binary child-policy尚待，未自动执行；不得用F7相同用例冒充F8通过。

固定v3仅完成13/15轮，原`paired-perf-v3.json`保持running/部分结果，不能算完整验收或从中挑好结果。已按用户合盖窗口停止唯一controller45992，GUI控制Stop当前瓶子、两根owned helper清理、关闭私有GUI/bootout两私有bridge；`lid-deadline-cleanup.json`确认两根无任何Wine进程、受保护4文件全部匹配。退出/未完成当轮raw记录单列，不算通过。未恢复定时或Windows工作。两版一小时不必重跑。

**下次先修一次性v3续测脚本再执行**：原脚本会从第1轮开始并把当前已是v3的F7 EXE覆盖到v2归档，不得原样重跑！保留13个已完成样本、同协议/同EXE7b77147c929bd5fc1d066a51724db397a0b96989f8bb99127a612c9273c984e5，只补剩余第3轮Admission Server及Fast7，明确保留本次中止的第14轮尝试。验证v2-frozen归档SHA748c44af...不可覆盖。两私有App/IPC均已关闭，续测依旧实际GUI/XPC，开启后先核AC/无人为并行压力。补齐后才计算固定三轮中位/±3归因；当前初步候选仍未达5倍，不能默认采用。随后做0104待验收功能/1260及真实清理。无子代理。

源码检查点fddfdb0已按用户身份skip ci提交推送；本合盖结果接续提交从实际git状态读取。原Windows busy/需重启决定/预算保持。原HY顺序与目标不变，完成当前Mac WaitAll后再接原队列。

## 上一源码阶段检查点（历史）

Windows暂不能重启，只推进Mac原02。0102叠加0100/0101，保留共享槽/Mach/ulock/inproc，删除WaitAll暂取/回滚，统一状态提交、恢复日志和线程代际。源码复审发现的持本地锁映射、SIGABRT/RPC、fork继承未来OFD锁、旧FD整数、无TEB故障、stderr阻塞、饱和提示和retired递归退出均修；父代理按最终摘要读回，限定静态复审无剩余P1/P2。协议964生成幂等，15变更/完整链20文件精确重建，四种组件产物链接0错误0警告。

源码不是运行/原生/性能验收：正常操作2次fcntl+本地准入，独立状态更新暂时串行，128MiB+64KiB稀疏映射RSS/延迟未测。仍非默认，未执行新后端/测试、切引擎/瓶子、改门禁/裁定、连接Windows或使用Actions。Windows busy/准备2309测试3600无pending、定时关闭保持。接续从research76和evidence继续验收及引擎整合，75旧缺口为历史来源；原HY顺序和最终目标保留。

经验：每线程open才是独立OFD；child关闭继承FD仍不足，parent未来锁也会共享旧OFD，须准入隔离后换新描述，并在持门后刷新FD。故障退出不先写可能满的stderr或进入Wine异常RPC；逻辑TERMINATED不等于已停止写状态。

实验隔离经验：CIDER_HOME只重定向业务目录，不自动隔离NSUserDefaults。今后的私有GUI实验必须在首次启动前就使用独立bundle id；本轮早期副本改id前没有全部GUI偏好快照，不将受保护二进制/配置摘要未变扩成所有用户偏好逐字节未变。

070ab56交付后继续本机平台组件构建：Wine启动器、wow64/wow64cpu、win32u和winemac.drv共9产物链接退出0。30条警告来自7个与固定基线字节相同的winemac文件（27弃用/3nonnull），作为待核对项保留，未消音或宣称全范围零警告；WaitAll四组件零警告仅指其最终构建。没有执行新模块或产品会话，完整迁移/采用仍待，见research76补充和同一evidence。Windows及用户环境未动。

## 用户目标与协作

- **R v1同步候选及真实剩余缺口（2026-10-18，检查点75）**：R v1独立候选0100/0101已接macOS msync及same-binary host loader，协议由962生成963（非CX1810），16文件精确重建；server/native ntdll/两PE模块已编译链接，未执行/安装。源修包括真实初始化/映射/分配失败、稳定原子页表、共享ref与alert索引、WaitAll abandoned/owner回滚/期限和登记清理。独立审查仍确认旧WaitAll暂取与并发Release会使semaphore count>max的P2，**未修、阻止默认采用**；不能钳位/丢许可/禁用WaitAll冒充修复。需统一状态协调及线程死亡恢复，原M3为候选机制，不计M1/M3完成。稳定表2/8MiB每原生进程代价、验收及回退见research75/evidence；无Windows/新测试或游戏门禁提升。用户暂不能重启，Windows busy/余准备2309测试3600无pending保持。

- **D盘就绪、重启暂缓（2026-10-18，检查点74）**：用户确认D盘清理完成；原pin/公钥LAN原生查询确认约205.7GiB空闲/NTFS，当前内存约5.6GiB空闲。Hyper-V预检检测既有CBS RebootPending，未调用启用命令/未更改功能。用户暂不能重启，人工交接按明确未执行结果及119.806秒本地间隔关闭，原生两批合计6.397秒；690秒预约全额保留，现余准备2309/测试3600、无pending、busy=true。只待用户明确可重启/已重启再恢复Windows，不叠加未完成组件操作；Mac继续原02同步后端迁移。见research74及windows-disk-ready-restart-20261018.json。

- **R v1工具链接续（2026-10-18）**：Windows用户清理C/D期间busy保持，仅Mac推进。四项源码迁移/原生准备记录已提交推送5739859。Wine11.18配置返回0却报告两PE架构缺C++17；新增独立GCC15.2前端构建，保留原C/CRT，不切LLVM或补目标libstdc++。复审发现继承as/ld缓存、未知旧SDK对象及marker/backend/实际binutils/额外源码核验问题，均改为归档逐文件+完整路径/碰撞安全Unicode校验、全新对象和输入/SDK/g++/cc1plus/COFF绑定；首两批安装前停止，失败保留。双架构前端已构建、Wine C++17均检测通过，实际Makefile原C/新CXX四路径已核；四个组件以及两种PE架构Wine libc++/C++ABI归档最终构建通过，不当作已可用引擎。native wineserver为x86_64、min14/SDK15.4，所有新产物未执行；最初PE GCC PATH缺失导致退出2已修为显式原C路径。四项补丁及脚本经过独立只读对抗性审查，静态无剩余P1/P2；最终组件构建证据在engine-r-v1-bootstrap-20261018.json，未安装新引擎/切瓶子，编译/运行/游戏裁定独立。实际msync传输/等待和object_ops布局已核对依赖，未盲贴yield或CX协议号。

- **Windows实际资源与用户清理（2026-10-18，检查点73）**：用户确认Windows开机/公网未变；Mac实际已在已记录LAN子网且ARP可见，因此改用同一个HostKeyAlias/原Ed25519 pin/项目公钥的已记录LAN，认证/SFTP实际成功。首次SFTP路径被当相对路径、首次PS在不存在E盘Join-Path失败均修并保留真实失败；最终native runner受控Job执行只读PS成功并确认空Job、回执/外层均清理。资源：build26200、20逻辑处理器、约32GB物理内存/7.46GiB当前空闲，Hyper-V两项InstallState2、vmms未装；C约27.3GiB、D约5.0GiB空闲，EWDK仍挂载，原参考client/sys仍在。不改主机保护、不运行driver/VM/RDP。四批全额预扣570秒、真实外层14.846秒，余准备2999/测试3600、无pending。用户决定先释放C/D空间，已按该决定把availability.user_busy置true；用户明确空间/盘符就绪前禁止Windows预约，不因时钟/唤醒自行清busy。 native Job目前仅正常启动/退出/清理通过，超时/STOP/强退和VM控制未通过，不能宣称无人值守接通。地址/密钥/raw日志仅私有；本次预算变更已授权，未来Wine实验须新owner而非篡改旧回执的旧保护摘要。R v1源码准备已有preflight、D3D9、DMO和组件覆盖路径共4个迁移；WinMM新上游已有不同初始化/格式映射，旧0008/0014改记需语义重整，不照贴/不声称已验证。

- **用户确认及Windows新授权（2026-10-18，接续72）**：用户确认恢复后HYP“已登录，画面和按钮正常”，不再索取同一确认；实际新登录/CAPTCHA挑战/完整更新仍无独立证据。用户另明确批准追加准备3600秒并按330.987秒结算旧300秒预约（额外31秒），已核原verified无项目VM/驱动任务清理回执，以稳定ID user-preparation-20261018-hy2-independent60原子追加/升级v2并结算。随后首批预约180秒仅SFTP原pin/公钥入口，8.162秒Operation timed out、没有Windows命令/VM/驱动启动；实际客户端退出/无项目任务回执已关闭，余准备3389秒/测试3600、无pending。已询问Windows是否开机及公网入口是否变化；新信息前不重复连接/不换pin/不重装RDP，继续Mac侧R主题工作。地址/密钥及原始transport日志仅私有，不写公开仓库。Windows原生watchdog仍未部署，不能声称无人值守已接通；定时保持关闭。

- **R v1首个源码准备（2026-10-18，接续72）**：全面恢复提交0137746，正常新版App已重开，Steam原91113/91115保留，原生Cider资料库5项；实际界面点击HYP后状态转运行，PID27008/原r2/profile revision4正确。HY1真实窗口/登录验证码仍需用户本人确认，已发简短问题，不重试此前已知Wine窗口自动化超时。按02开始v1：固定Wine11.18实际commit7b3fff76...及源码SHA，21个配方补丁主题/依赖写engine/porting/r-v1-inputs.json，首个preflight helper与CX oracle相同、源码应用zero-fuzz/offset比对通过。只完成源码准备，尚未构建/运行v1，未新建可用配方或EnginePolicy绑定；CX msync/loader/WoW64和图形依赖须逐主题迁移，kernel新base不能照搬CX协议1810。该步没有新增测试套件/Windows/Actions/定时；不可把后台机时不足改成反复远控或复跑旧基线。

- **全面恢复授权（2026-10-18，检查点72）**：用户明确“隔离的代码先全面恢复吧，然后严格按照原计划推进”，解除10月14的源码隔离等待限制。全部App/Packages/Tools/engine/data/scripts/tests/CI/AGENTS与已修复快照344148a一致恢复，含A/B/C/D和52–59历史材料；不是恢复原缺陷0010。原主线备份codex/pre-full-restore-20261018保留。主线release App/CLI/Bridge已构建签名，常用UI加载另留回执；Steam已有会话保留。候选内核源码已恢复但原生Windows未验收，非默认/不自动切用户瓶子，真实预算/旧pending不改、无Windows/Actions或新定时。继续HY1→HY2→HY3→HY4/HY5。用户询问自研引擎：R v0早已本地自建、当前Steam实际用cider-cx26.3-r1-wma1-mm1；02的旧Highball状态已标历史。下一步按原02进入R v1主题队列整理/固定输入，带符号v0性能研究可继续；Engine A原P3为2027年6–8月及前置probe/entitlement门槛，不把它当R自研的开始时间。

- **检查点71（2026-10-18，隔离K2源码补齐）**：0017–0020已按既有Wine kernel-object模型实现真实指定process的primary token引用/释放，manager归属与process句柄/映射身份绑定，不创建token访问句柄、不改普通DACL检查或替代宿主身份；退休0010不恢复。109项精确C模型、6个编译后破坏变体、14文件zero-fuzz重建、native/两架构模块及隔离release通过；两种真实Wine架构各420项0失败，64正常并发轮、2个明确夹具异常（到达注入点才接纳预期）、关闭child全部句柄后held token仍可查询/归还。服务/所有本批prefix进程/XPC job清理有回执；混配native对被产品spawn前拒绝，0新Wine spawn。独立复审发现核心finally/owner/close三项及夹具保活/异常/故障标记等问题均已修；早期弱证据、1114和NameError保留，不作最终通过。协议1810必须配套server/ntdll Unix/两架构PE，EnginePolicy新增精确pair但旧两组保留。详见research71和结构化证据；不宣称Windows原生契约/游戏/生产验收或整阶段完成。所有源码仍隔离，主线实现374980e、常用App/安装引擎/存档/原神下载和真实预算不变；Windows/Actions/定时0。原HY顺序/此前游戏验收保留。接续需用户既有隔离恢复范围决定及独立Windows/机时条件，不自动整包恢复或改账。通用安全提示无错误码且此次实际工具成功，不能沿用旧“K2一律禁止”说法。

- **检查点70（2026-10-17，K2继续）**：用户转述新提示仅“谨慎开放网络安全”一类、无错误码。最近工具调用均成功，原始近期日志无明确新拒绝；可能产品/模型层提示，不能否定用户看到它，也不能用旧blocked-action摘要断言整个K2不能实现。官方说明承认误判；没有改Codex配置/防护或发送反馈。0017实际补释放接口PsDereferencePrimaryToken→已有ObDereferenceObject/stdcall，3个精确委托模型、32/64模块链接/真实反汇编及6文件重建通过；取得函数仍stub，K2未完成。只读自己进程公开API的两架构观察确认OpenToken(0)拒绝/Query打开成功/复制零权可行但Query仍拒绝；Wine固定测试明确零权打开应拒绝，不能删除allocator/DACL检查来凑成功。当前伪句柄解析调用方且微软禁止复制，不作目标token替代。继续沿既有真实对象引用机制研究取得接口，不重放历史被拒访问替换动作；所有游戏/门禁/原生Windows预算/隔离不变。新代码仍只在隔离分支，无物理Windows、Actions或定时。

- **持续检查点69（2026-10-17）**：用户要求继续修完全部隔离问题，未授权恢复或Windows扩额。隔离源码4e226f6，本轮父代理实做K1/K3的Wine运行，单个只读代理发现两项P2均已修：0015去掉CLOSE OOM对借用ref0文件的未配对释放；0016保留IOCTL context拥有的别名/扩容缓冲，只回收独立输出/替换旧输入。精准CLOSE3路径、IOCTL12路径及旧断言/ASan double-free负对照通过；32/64模块链接及组合7步/6文件重建zero-fuzz匹配。最终两种真实Wine架构各256事件/64callback/22有序pin+21关闭负对照+21关闭引用竞争、各1560项共3120检查0失败，driver加载/卸载、服务STOPPED读回/删除、前缀及job清理均通过。仅复制引擎两ntoskrnl模块，原生server/loader和门禁绑定不改，明确未运行token后端/K2或原生Windows。原16调用者函数体逐项相同，复用既有基线未复跑。退休0010的旧候选README顺序已纠正。源码仍隔离、常用App/游戏/账本不动，无Windows/Actions/定时，HY顺序保持。K2先前平台拒绝仍未实现/未重试，独立Windows kernel/真实失败契约及原pending仍是实际缺口，不得标整批全完成。

- **持续检查点68（2026-10-17）**：继续原HY1后，aab2970同一release产品源码保持；Unicode/CP936快捷方式与宿主EXE保存项分别经过真实Catalog→WindowsPath→AppLifecycle→Wine启动及重开共6次，guest实际argv/cwd/env/退出37一致。产品CLI→XPC→GUI完整创建win7_64新瓶子/前缀revision2也通过，共7项及实际清理，见research68；服务链不冒称SwiftUI点击/Windows Shell。恢复候选已整理为A应用/安装、B音频候选、C预算脚本、D非默认kernel，固定aab2970文件摘要在restore manifest。当前下一交付受用户原“等我指令再恢复”约束，需要明确恢复范围；真实kernel还依赖独立Windows/机时新决定，K2旧工具拒绝保留。不要无变化重复构建/测试或将整批标全完成。常用App/账本/游戏数据保持、无Windows/Actions/定时。

- **持续检查点67（2026-10-17）**：隔离源码aab2970中四个产品新瓶子入口已从预约/config公开前登记并保留停止活动到初始化、刷新、安装/打开；手动EXE/MSI也用完整DWORD。真实旧Stop后两次后续spawn/创建0，最终0后续spawn/创建1且无created；19项Swift、5项手动CiderKit服务实际Wine、release/严格签名及清理通过。复审两项P2均修：同锁停止到删除、install忙标记先检查/取得并正确转移。服务执行不冒称GUI点击/TCC/Windows kernel/游戏。源码仍隔离、常用App/账本未改，无Windows/Actions/定时。接着做原HY1特殊快捷方式参数/guest cwd真实证据，**不要组件完成就发final停止**；原阶段/门禁/K2既有拒绝/Windows旧pending保留。

- **持续检查点66（2026-10-17）**：隔离提交3f55e9c已补完整安装DWORD、最小必要引号/真实MSI阻塞修复、资源打包校验、跨GUI/CLI停止、原子取消完成及刷新/自动打开交接、异步客户端输出。38项Swift/15命令/8原生XPC/24最终PE采集器/10真实配方及2种真实Stop均通过，独立复审范围无P1/P2；异常数值不是故障注入、不是Windows kernel/TCC/游戏验收。失败与修正后回执均保留，常用App/引擎/游戏/真实账本不动，无Windows/Actions/定时。报告66是继续工作检查点，**不结束任务**；现在继续新瓶子初始化与手动安装入口交接，不能因一个组件通过再次发final停下。原HY顺序、隔离及真实Windows预算边界保持。

- **继续工作的当前关键点（2026-10-17）**：不要在组件检查点发final停下；用户再次明确持续推进。隔离的完整安装退出链已经有8项真实EXE/MSI安装、最终打包采集器32/64共24项、25项Swift定向检查；最小必要引号修复Wine msiexec保留选项引号而打开帮助界面的阻塞，诊断scratch移出sessions/附记录/别名比较两项P2已修。尚未提交这组源码，主线只记忆变化。当前进一步实做原L6：已真实复现CLI安装等待detect时Kill返回0后，迟到detect文件使原任务继续启动下一依赖程序（spawn PID43445、旧CLI最终0）；失败及清理在隔离tests/runtime/exit-status/results/installer-stop-before-coordination.json。正在补按瓶子prefix的共享安装活动/取消、GUI与CLI Stop屏障、下载协作取消及WineRunner锁内最后启动检查；不把已有组件通过当整个任务完成。完整活动检查点在隔离out/ACTIVE_CONTINUATION.md。真实Windows未连、账本/旧pending不动，自动化关，原隔离未恢复。

- **持续推进纠正及在做项（2026-10-16，65之后）**：用户再次明确不能因阶段报告停下。一个 Windows/平台依赖项受阻时继续处理原队列中可独立推进的 Mac 实作；不重复同一阻塞或另建定时。当前仍只改隔离分支：正在把48/49已有完整DWORD退出码采集接入RecipeInstaller，旧代码忽略安装器返回值。已在同一已知无账户私有prefix完成32/64各高位/259/重启码/Unicode参数/cwd/env及超时/坏PE/旧结果共24项，真实产品路径与清理通过，结果在隔离tests/runtime/exit-status/results。正在补App源码构建PE工具/摘要资源、完整退出判定、显式MSI约定与待重启结果的GUI/CLI传递；尚未完成整条安装验收/审查，不作为交付或原生Windows通过。无需Windows连接，真实账本/主线实现/常用App/游戏不动，65阶段结论是历史检查点。

- **完全访问后的隔离收尾（2026-10-16）**：本轮实际配置已是danger-full-access/never，未再申请沙箱权限。隔离源码c9a1931补修断开CLI后等待占位、Bridge重连丢失GUI容量、多CIDER_HOME复用错误GUI。观察解除不杀程序/不重放，GUI跨连接保留8普通/2控制容量，冷启动按根flock并创建独立实例；保留原命令与GUI派生架构。3自有子进程测试、5原生XPC、4AppKit多目录项和真实产品Wine断线后仍运行/新命令/ps通过；只读复审范围无剩余P1/P2。最后显式捕获消除警告，3项重验、无警告release/严格签名通过，三组件与实际运行字节相同，无需重跑。首次产品LS清理-10814误判与真实精确路径缺席读回分开保留，未按共享引擎bundleID删其他登记。见research65；所有实现仍隔离，K2旧工具拒绝未重试/未实现、独立kernel/机时结算及隐私/正式签名/游戏验收保留。原HY接续点、常用App/引擎/存档/原神下载/真实账本、定时关闭不变，无Windows/Actions，不整包恢复。

- **真实产品运行续修（2026-10-16）**：隔离提交731267a（初次交付时未推送；未恢复，后续见65）修复不同CIDER_HOME同瓶ID混入、wineserver控制客户端误识别/晚到进程/exec身份刷新/截止误失败，以及旧0008首次waveOutOpen锁循环。18项定向测试与release构建/签名通过，真实产品GUI→Wine参数/cwd/env/退出37、ps归属、正常与启动期停止/重开均通过；PE32/64各40次绘制/像素匹配、真实候选模块和静音waveOut缓冲完成。三项独立复审P2已实际修复，范围内无剩余P1/P2，代理已关闭。0014/mm2仅在私有引擎替换两WinMM模块及manifest，总4832项其他内容相同，原引擎未改。停止12秒为软努力期限，正常实测13.18秒；WindowServer记录不是无遮挡视觉、静音不是音质、GUI原生父进程不是TCC归属。详见research64/结构化证据；失败与成功、真实清理均保留在out/runtime-acceptance-20261016-84kr3z6f及隔离结果目录。常用App/已装引擎/存档/真实Windows账本保持；无Windows/Actions/新定时。K2工具拒绝、独立kernel/预算清理/隐私及游戏验收、所选范围恢复仍待，整批不能标完成，原HY接续点不丢。

- **权限交互教训（2026-10-16）**：用户已授权且报告界面开启完全访问，但本轮实际执行配置仍workspace-write，隔离工作树在可写根外、Git/GUI触发审批；具体不一致原因未查明，不能断言同步故障。不要把运行时限制说成用户没授权。避免同批构建/验证/清理拆成大量重复申请；已有真实证据不复跑，不经其他工具绕过边界。

- **坚持原能力的续修（2026-10-16）**：用户明确尽可能沿用原代码，性能/效果优先；不把撤功能或基线未实现算作修复。隔离分支bb096da接回ADR-007 GUI派生架构，29条原命令抽为GUI后台执行库，CLI经签名身份XPC/纯转发Bridge；不新增游戏帧RPC。审查/运行发现的10+1候选缺口已修；15命令/1路径/23原生XPC/2真实CLI冷暖夹具、release打包签名通过，父代理复核源/二进制/清理摘要，独立复审无P1/P2。原生夹具不等于产品Wine responsible/TCC、Windows kernel、正式签名或游戏会话。K2之前工具自动安全审查拒绝仍未完成，未改措辞重试；不能由拒绝断言全项目违规。见research63。原隔离/主线实现/常用App/引擎/存档/真实Windows账本保持，准备0/测试3600/原pending不变，无Windows/Actions，定时关，原HY接续点不丢。原SMAppService发布设计仍待；当前会话按需MachService登记为隔离候选取舍，不新增定时。

- **隔离续修与用户纠正（2026-10-15）**：用户指出只交付Q1–Q5未满足整批审查后修复。已继续实际修复K1/K3（1a919a8）：稳定同权限句柄、map/ref生命周期锁、逐层异常及构造器句柄清理、真实错误/线程池/回调清理；复核发现的递归锁残留和32位对象头对齐均已改。100双线程交错通过且旧同交错ASanUAF重现，合并313断言/6破坏变体通过，i386/x64模块链接0，补丁重建6文件一致，见research62。K2替换专项工具被自动安全审查以“可能网络安全风险”拦截，没有换措辞/转交重试；旧0010撤出候选保留rejected原字节，最终接口回基线未实现。撤回不等于忠实实现，整批仍不能标完成；XPC旧债/原生对照也未完成。原App/引擎/游戏/账本未动，无Windows操作或扩额，定时关。原HY1→HY2→HY3→HY4/HY5接续点保留，用户仍期待修复后推进；没有主线恢复授权。

- **隔离修复交付（2026-10-15，10-14开始）**：用户在research60后授权修复已发现问题，并提醒保留原计划进度。已在独立managed worktree/分支codex/quarantine-fixes-20261014从eecba4f修复Q1–Q5：预算v2防旧版漏扣、实测/未测量互斥与坏输入、合法ANSI名称/图标不丢目标及显式ACP、打包CLI自身索引。相关块边界、每轮drive缓存、事件过滤及原子打包同步收敛；独立复核追加修复提交不确定/已提交可用状态错误/失败初始化三项。18预算组、32 Swift项/6组、31打包检查、25预算独立探针通过；父代理复核真实lnk离线副本及20,022路径对照，最终release App/CLI构建/严格签名成功。合成性能不作整体加速保证。源码提交1aa7639/02b62e3/bccd03e均用户身份skip ci；结果见research61/结构化证据。主线实现仍374980e、原隔离ref仍eecba4f、常用旧App/引擎/游戏/真实账本未改，Windows准备0/测试3600/原pending保留，无Windows/Wine/产品GUI/Actions，定时仍关。内核K1/K2/K3、ADR007 XPC与native环境仍不计完成。恢复范围待用户指定，不整包合并，预算代码恢复不自动批准扩额/补扣。

- **原计划接续点（同上交付）**：柚子社两款Control/退出重开已有用户验收，不重做既有Win32/WMA/mixer基线；全系/视频有效帧/长期冷启动仍待。HY1当前启动器首装、标准/Retina及按钮、CLI/运行命令/保存项有52/53/55证据，特殊参数/guest cwd、登录验证码/完整更新/长期仍待。HY2已有Windows参考client/sys编译及相同EXE Win32对照，但独立kernel参考、token对象共同所有权/正确主体/异常回滚仍待；HY3许可闭环、HY4/HY5三款本地交互会话未完成，原神下载文件保留。顺序HY1→HY2→HY3→HY4/HY5不变；用户要求修完后继续推进，但此前隔离恢复须新明确范围，Windows机时/清理另行授权，不能从此概括继续恢复或扩额。

- **隔离审查交付（2026-10-14）**：用户因生成期模型性能异常背景要求全面深入检查；本轮仅审查/离线隔离验证，不是恢复或修复授权。research60覆盖4个隔离提交全部27个变更文件，两代理完成Swift/打包及预算专项，父代理独立复核关键结果并核对源码/产物/既有元数据。确认恢复前4项：预算v1跨旧版漏扣超额(P1，私有夹具31/3600秒)、矛盾未测量回执接纳(P2)、合法CP932/CP936兼容ANSI名称/图标导致整lnk拒绝(P2)、打包CLI engine download漏自身index(P2)；另有P3损坏输入异常。内核修假引用/溢出有模型验证改善，但旧lookup/free竞态、宿主主体访问及通用void异常仍不具生产依据，候选不恢复正常引擎。目标/门禁/Epic方向未改；支持工具偏重、native验证债务和旧CLI直接spawn对ADR007偏差须收敛。没有取得模型故障因果遥测，不能把代码缺陷归因该事件。123预算夹具/14抽取C模型案例等都在out审查目录，未操作Windows/产品GUI/Wine或改真实账本；账本锚点及隔离实现保持。主线仅增加报告/控制说明，待用户指定下一步。

- **当前冻结指令（2026-10-14）**：用户要求北京时间10月13日14:00后的代码全部隔离，等新指令恢复。已将024ab36/11a7167/84c4498/eecba4f完整保留在本机及远端codex/quarantine-after-20261013-1400；主线代码回到374980e，保持最新用户AGENTS并新增隔离控制说明。原新App、App/CLI缓存、内核候选源码/构建及相关本机记录已移至私有Quarantine目录，旧App包已恢复、主界面保持关闭。Windows账本/游戏数据/已安装引擎未动；退出后Steam97843/HYP62110仍在。详见docs/QUARANTINE.md。不得按此前继续授权自行恢复、合并或开展开发/VM准备/验收，待用户明确指定范围；定时关闭决定保持。

- 用户在关闭定时任务后授权按正确方向继续，未授权恢复自动化或扩大Windows预算。research/51继续HY1入口：显式cwd不再try?静默丢弃，宿主/绝对盘符目录共用解析，stop不依赖cwd、restart先验证；CLI新增--cwd保留旧默认。WindowsPath要求绝对ASCII盘符/NUL拒绝、点归一化限盘根，非完整Windows路径/guest cwd契约。Spawn检查所有准备返回及CString分配，坏字符串明确失败，未运行故障注入。最终CLI3.28秒/App4.34秒构建0、help已显示选项、debug打包签名0、旧包备份；未重启现有会话。Cider AX可读，当前r2完整宿主路径读取仍timeout，不能视觉验收；现有HYP+五helper只读可见，已询问当下下载任务，答复前保留会话。无Windows操作/改账/Actions/测试套件/子代理，定时仍PAUSED，不把源码推进当三游戏兼容验收。

- 2026-10-12用户明确要求关闭定时发送任务。已通过应用工具将现有cider heartbeat设为PAUSED，并读回automation.toml确认；不自动重新开启或改用另一套定时方案。此前每小时续做/ACTIVE描述是历史状态。项目目标及HY1→HY2→HY3→HY4/HY5顺序保持，手动聊天继续与定时触发分开。当前实现提交ab09122已推送、工作区在本次记录前干净；接管总体审查27与近期49/50对抗性静态自审均有记录，不能称为全代码独立审计或完整运行验收。最近交付主要是HY1入口/安装可靠性，原生kernel/context与三款本地游戏会话仍未闭合；不将辅助实现/构建列为米哈游游戏已适配。Windows账本本机实读仍准备0/测试3600/原pending，没有连接Windows或改账。

- 最新续做research/50：首次安装detect静态缺口已修正，空条件/空any_files、缺失DWORD当零、目录当文件、文本子串当注册表节均不再作为完成证据；元数据接纳/整图预检/直接检测共用条件校验。仅支持规范化绝对盘符路径与HKLM/HKCU，特殊转义名/隐式父键/完整Windows文件语义等仍待完善；未运行测试或安装。最终CLI3.13秒/App3.25秒构建0、debug打包签名0，上一包备份，未重启GUI/Wine。49已按用户身份skip ci提交推送9338898。本机账本实读仍准备0/测试3600/同一pending，未改账/连接Windows；完整32位退出状态链、HY1运行缺项、原生kernel与三游戏会话均未闭合，不升级许可/评级。

- 最新Mac交付见research/49：RecipeInstaller先验证完整依赖图，循环明确失败，共享节点去重，来源/类型/参数/环境归属静态错误前置；检查仍未覆盖真实安装失败/取消回滚，运行验收待做。新增PEHeader共用资源读取边界，以及退出码候选的显式准备/有界读取API；没有App/CLI/RecipeInstaller调用或伴随EXE打包，不声明修复了完整退出状态链。CLI9.17秒/App13.13秒构建0，debug app打包签名0、旧包已备份；未重启GUI/Wine，无测试/Windows/Actions/子代理。候选的目录/计划/Unix零返回不是来源进程或后代清理证明；native kernel和三游戏门禁保持。用户要锁Mac时已说明当前无需手动步骤，保持开机联网/Codex运行即可继续命令/文件工作，屏幕步骤留待解锁，不保证睡眠/关机时运行。45的超额预约仍pending，不自动结算或扩额。

- 用户再次要求不要停下、只有重大问题才停止。已继续Mac代码：research/47新增LaunchPlan/session/audit的自动环境profile id/revision/内容摘要及最终值差异键，launchLocked按accepted DB快照/实际argv-env重算，不把可编辑metadata当授权；没有扩大安装器匹配或断言子进程继承。research/48记录安装退出码被忽略且Unix8位不能推定Windows32位，新增Tools/wine-exit-status候选及CiderCore.WindowsExitStatus有界/严格解码、MSI专用分类；x64/i686 PE严格编译、App/CLI构建及debug打包签名成功，自审补强phase=captured。候选未运行/未接入安装/未bundled，原缺陷仍待真实状态链修复；不是Watchdog/整树清理/内核证明。无测试/Windows/Actions/子代理，45的超额预约仍pending，不自动修账。为落实Mac锁屏后持续推进，复用同一heartbeat cider改为每小时回本聊天，Windows仍09–17且受原预算/busy/pending保护；相同阻塞不反复通知/盲重试，不运行未要求测试、不将构建当验收。Mac/应用须持续运行，锁屏不等于睡眠，不恢复旧paused Goal或另建重复Goal。
- 原计划继续后的Mac交付：research/46复核HY顺序/证据，从HY1快捷方式入口发现旧splitArguments静态缺口，新增CiderCore.WindowsArgumentParser并替换AppCatalog委托。保持空参数、Space/Tab、反斜杠/引号及Unicode；自审补首NUL终止，不解释宿主shell或冒充完整CommandLineToArgvW。最终CLI/App swift build均0，debug out/Cider.app已打包签名，旧包out/backups保留；未重启GUI/Wine、未新增/运行测试/Windows/Actions或子代理。真实特殊快捷方式/Retina/完整入口仍待验；HY2 token/context方案42仍需原生kernel依据，HY3及三款门禁不提升。Windows过额预约45仍pending，不能因本机代码交付自动解除或扩大预算。
- 最新用户确认Windows空闲、允许验收，并要求ToDesk空闲接回、释放控制后不主动锁屏、MacBook锁屏后继续可执行工作。已按消息解除busy。research/45：原内网路由现utun6/无ARP且SSH认证前关闭；同300秒预约内已配置公网原pin/公钥/echo/退出0通过，私有参数保留旧备份后切公网（绑定en0，不改代理）。专用账户原生enabled=true/Local，主账号控制台1 Active。RDP仅显示换用户确认，桌面/输入没通过；内置截图展示和返回JPEG不一致，返回像素+AX均0x204，不能再猜缩放或标通过。隧道已按预约剩余时间截止19:40:11，子PID/回环关闭、客户端主界面已观察，无项目/VM/驱动/锁屏/注销/tscon。外层观察330.987秒超过300秒30.987秒，真实回执close拒绝，ba2ed2b2-3f78-4911-b941-bd5b3d369dd2保持pending，余准备0/测试3600，停止后续Windows操作，不自动改账/挪测试。Windows最终解锁状态未验证，Active不是解锁证据。Mac caffeinate系统保活仍有效；只继续命令/构建/文件及已有heartbeat可执行步骤，不绕过Mac锁屏或保证睡眠关机工作。控制台交接仅候选，见45，不擅自SYSTEM提权/自动登录或改安全策略。
- 最新用户批准追加5分钟准备（两条回复为同一次授权，不能重复加额），且明确主账号正在使用Windows。已追加稳定授权user-preparation-20261011-dedicated-rdp5：有效准备4500秒/测试3600秒，余准备300秒/测试3600秒、无pending，新增额度尚未使用。Mac实际确认保存的CiderReference标签已绑定目标配置，取消查看回电脑列表，未连接/读取密码。私有windows-availability.json记录user_busy=true；budget status暴露该值、reserve在任何阶段/attended模式均拒绝忙碌预约，close/已批准extend在有效记录下仍可进行。缺失旧文件兼容不代表自动确认空闲；未知schema/非bool/异常文件拒绝。只做语法编译及真实status读取，自审检查忙碌保护不能由追加额度、在场标志或测试阶段绕过；它阻断新预约，不声称自动停止既有任务。用户明确确认空闲或给出新可用安排前，不清除此标记、不发起Windows/RDP/SSH工作；继续可在Mac准备，原HY顺序保持。
- 最新用户报告专用账户“已启用”。最后人工60秒预约已按attended unmeasured规则关闭：实际输入命令没有整体计时，交接墙钟间隔单独保留不冒充活跃机时；完成/Enabled证据来自人类报告而非SSH读回，密码未采集，没有启动连接/项目/VM/驱动。准备余0/测试3600、无pending，有效上限4200/3600。Mac只在新凭据表单预填CERVANTES\CiderReference及友好名称，密码框为空/焦点交用户；未添加/保存新凭据或重连。已请求明确再加5分钟准备、只做专用账户首桌面/截图/输入验收，尚无批准，不能apply extend或开Windows任务。用户须先断开当前桌面连接避免重现0x5；账户启用不等于桌面验收。
- 当前人工交接：最后60秒准备已预约c5b7763d-dca0-4ab5-a87d-a2c52411049c，余准备0/测试3600，pending；仅用户在Windows管理员PowerShell以Read-Host SecureString设置CiderReference密码并启用/读回，无RDP隧道或自动新操作。未收到Enabled结果前账户仍按禁用/无密码的已验证状态记载，不能关闭预约或再开展Windows任务。后续读用户回执，按真实人工范围结算；若没有总秒表只能明确unmeasured，不猜0秒。测试额度不得补准备预算，追加需新明确授权。
- 用户明确请求新建管理员账户，并确认自己仍在连接Windows；此前另一连接的具体程序尚未确认。采用专用本地CiderReference，独立身份/配置是收益，新增管理员权限与维护账户是成本，不承诺Win11Pro能并发两个桌面、不修改多会话/保护策略。60秒在场准备44b7d396-48c6-4fc2-b301-056726cb4d82内可信SSH创建NoPassword且Disabled账户并验证禁用、Local来源、Administrators及Remote Desktop Users成员；密码未输入/保存，SSH原AllowUsers未改。31.429秒范围回执关闭预约，全扣，余准备60秒/测试3600秒、无pending。新账户尚不能登录，必须用户在Windows亲自输入至少12字符密码再启用；不把创建/管理员成员等同RDP验收。账户SID/原始回执仅私有；撤回先Disable-LocalUser，保留可能出现的用户文件，不自动删除。接下来人工密码步骤需另预约剩余准备时间，不能免计或切入测试额度。
- 最新重连：用户填写保存后，Mac电脑配置已绑定新邮箱形状的凭据标签（标签不是实际用户名，未读取密码）。240秒准备预约45429c6d-b500-44bb-ba86-c613382f06ca内实际连接，不再出现凭据无效，而是0x5“另一个连接”断开。可信SSH只读同时间记录取得4624/LogonType3/NTLM/目标SID匹配本Windows账户及RCM1149；NLA网络认证通过，但没有已验证桌面截图/点击或类型10登录。LSM有24/25断开重连与40/Reason5，尚未确定由ToDesk、本机操作或其他连接触发，已向用户询问，不能直接归因/停止其他服务。关闭弹窗后电脑列表实读，隧道73.896秒停止、子PID消失/回环关闭、查询0.796秒退出0，99.907秒范围回执结算240秒全扣；余准备120秒/测试3600秒、无pending。没有启动项目/驱动/VM，不注销或终止用户既有Windows会话。不在相同争用条件下盲重开，先收人类连接状态。
- 最新RDP失败已定位到提交身份：CUA实读“凭据无效”，同时该预约内可信SSH采集两条4625（10:12:44/49 UTC），均匹配客户端提交名称、非已确认SAM名，Status0xc000006d/SubStatus0xc0000064（STATUS_NO_SUCH_USER）。所以本轮先纠正账户名称/命名空间；密码正确性尚未验证，不能仍归咎等待超时或宣称密码错误。UI有Caps Lock提示但没有密码内容/输入证据，不把它当根因。取消后另出现0x1f07，随后关闭回电脑列表；这是取消后的状态，不覆盖先前失败。隧道停止156.243秒、原SSH PID消失/回环端口关闭，native查询返回0、未启动项目/VM/驱动；190.816秒范围回执结算240秒预约，不退款，余准备360秒/测试3600秒、无pending。Mac已从电脑配置“添加凭据”打开空白表单（避开崩溃的全局管理页），由用户按Windows设置中的实际微软账户完整身份与密码亲自填写、保存并绑定后再通知；保存授权仍有效，不读取密码/猜邮箱。原始账号/日志仅私有。
- 最新接续：用户退出/重开Windows App到电脑列表后，CUA窗口读取实际恢复。已预约240秒在场准备f1deade7-31b7-407d-95d3-75c92f83ce92，余准备360秒/测试3600秒，pending。严格SSH隧道ready，210秒截止2026-10-11 18:14:50 UTC+8；双击保存配置后出现RDP自签名证书确认，未再次索要凭据，但不据此宣称认证/桌面完成。显示证书的SHA-1与先前可信SSH读取的RDP证书完全相同，未勾永久信任、未代替用户点击继续；交给用户确认本次连接后回报。本轮未运行Windows项目/VM/驱动任务；接续先读取真实登录结果并关闭隧道/UI结算本预约，不另开预约或把隧道退出当所有任务清理。
- 2026-10-11 18:06后用户已重启Codex，Windows App窗口读取仍报native pipe closed；只读本机崩溃报告确认SkyComputerUseService在18:06:07发生EXC_BREAKPOINT/SIGTRAP，故障栈含Swift assertion/Array.remove(at:)。这是界面服务实际崩溃证据，不是Windows App崩溃、密码错误或权限缺失证据；具体数组/触发条件未知。下一步人工用⌘Q退出Windows App并重新打开到电脑列表，脱离此前凭据首选项页，再做一次有变化的窗口读取；不重复要求重启Codex、不杀系统服务/改权限或替换自动化通道。新增Windows额度本轮未用，status仍余准备600秒/测试3600秒、无pending；没有开隧道/Windows操作。保存凭据授权保留，不需再次录入或读取密码。
- 最新用户授权/检查点：用户明确同意Windows App在此Mac保存本Windows账户凭据，随后明确同意准备额度+10分钟，测试仍60分钟。已通过追加式extend记录稳定授权user-preparation-20261011-38a787cc，base60/60和原扣费不变，有效准备4200秒/测试3600秒，余准备600秒/测试3600秒、无pending。不能再重复同授权加额或由heartbeat自动扩额。Mac配置实际确认保存凭据已选中并绑定，名称可能为友好标签，不能用不显示邮箱/前缀判断用户名错误。核对实际用户名时CUA出现Sky native pipe closed，重选应用、重建REPL仍失败；全局应用清单可读、Windows App仍运行，但不能读取窗口/凭据页。新额度尚未用于Windows操作；不在UI失联时盲开隧道。密码仅由用户录入/保存在Windows App，未读取或复制到项目文件；RDP桌面仍未验收。预算改动/自审见research/44后续，工具故障需人类恢复窗口/客户端后继续。
- 用户明确纠正：此前在超时前真实出现“凭据无效”；后来读取的0x1F07不能否定这个早期报告。按时序保留两类证据，当前认证根因未确定，不能再只解释为用户慢或超时。确认的账户来源仍MicrosoftAccount，下一步核对完整微软账户身份/对应密码；本地SAM形式不必然错误，未匹配审计也不能证明密码正确。已在Mac Windows App打开空白“添加凭据”表单，尚未输入密码、保存凭据或改连接配置，也未开展Windows操作。保存会让该Mac客户端保留凭据，需明确用户决定且由用户本人输入；Windows准备预约余0，重连前需明确追加额度。保护NLA，不另建账户/降级保护绕过未确定的认证问题。聊天密码不得写入工具/脚本/仓库/长期记忆。
- 最新RDP实际错误已读清：Mac窗口0x1F07，文字是“未及时提供凭据”，不是已证实的密码校验失败；我此前把用户“未及时”误当操作迟到，后续禁止继续按这个推断盲重开。SSH只读确认当前账户enabled=true/PrincipalSource=MicrosoftAccount；预填本地SAM身份前应核对来源，微软账户完整身份需下一轮确认，不能断言SAM形式一定不支持。按本账户筛选的失败审计未匹配，不推断无失败/密码正确。密码由用户自行输入，未使用或写入脚本/仓库/记忆；不存任何密码内容或摘要。最新240秒预约已按211.462秒控制器/隧道及关闭0x1F07窗口回主界面的范围结算，准备余0秒/测试3600秒、无pending，不自动扩额或改计到测试。下一连接方案先由用户在客户端本地预先录入凭据，保存须明确用户决定；继续Windows准备需用户明确追加额度。NLA维持1，不为失败降级。采集器新增仅当前账户来源字段，邮箱/密码/SID不输出，Windows新版未执行。
- 用户已确认“开始吧”：最后240秒在场准备预约0f0269a6-521e-47d0-a9f6-22cd2b340022 pending，准备余0秒/测试3600秒，不能再开新的Windows准备任务或自动扩额。隧道210秒截止17:34:43，实际凭据窗口已出现；填写账户/定位密码框时工具报告用户已接手，停止UI输入，密码由用户本人处理。未验证登录成功/证书信任/桌面；结束回合让用户继续。此前尝试离线打开表单会直接0x204，不能在无隧道时准备凭据窗口；该离线失败仅Mac客户端，无Windows操作。下一步先处理此预约内的登录结果与真实UI/隧道清理，不重开消耗或把SSH断开当作Windows程序终止。
- 最新RDP交接改为先确认人类就绪再计时：用户第二次表示未能及时登录，210秒隧道已自动结束、CUA确认仍主界面、无已验证桌面/项目程序。240秒重开预约已按实际210.198秒范围结算，无pending，余准备240秒/测试3600秒，不退款。不要第三次直接启动隧道；先等用户准备好Windows账户密码并确认能立即操作，再用剩余额度启动，先把登录表单准备好、隧道放最后一步，及时结束回合。凭据由人类输入；暂无凭据错误码、不能推断密码错误或放宽NLA；SSH/SFTP已有通过证据保持。
- RDP重开当前交接：用户报告无法连接并关闭弹窗回主界面；旧隧道17:22:11按150秒截止退出，CUA确认主界面，前180秒按累计153.686秒控制器/隧道及无项目桌面任务结算。此前错误码未保留，不能断言失败仅由超时引起。新240秒在场准备预约e2e5de8e-7128-40f3-be54-8f1f52374bcf pending，余准备240秒/测试3600秒；新仅回环隧道210秒截止17:28:31，实际凭据窗口已重新出现、账户名填好，密码仍由用户输入。如再失败保留错误窗口读取原因，不重复盲重开；桌面/native watchdog仍未验收，隧道退出后需真实UI状态结算，详见research/44后续。
- 当前RDP人工交接：用户打开Mac Windows App后CUA主窗口/配置可读；确认回环隧道端口、凭据询问、自动重连off及各重定向off。人类在场180秒准备预约3d1b22b8-7b85-4541-a496-3e8c0c3f8a25 pending，余准备480秒/测试3600秒。RDP监听/TLS2已读回；原NLA要求0，保存原值后Set要求1，因方法ReturnValue字段未取得脚本退出1但独立有效读回1/TLS2验证，未重复Set/伪造返回码。已启动150秒自动截止的仅回环SSH隧道并实际显示凭据表单，账户名已填，密码/证书由用户输入/接受。桌面/截图点击/native watchdog未验收，未运行测试/VM/驱动。隧道结束不自动证明UI/任务清理，需实际断开/关闭证据结算pending；控制器/STOP/备份/端点只私有，见research/44后续节。
- 最新Windows接入已完成命令/文件部分（research/44）：Public本地GPO刷新后有效True/Block/True；严格原Ed25519 pin/项目公钥认证、唯一远程标记及退出0通过，8KiB上传下载与Windows SHA256一致，临时目录真实删除。只验证LAN，公网/RDP/native watchdog/kernel未验收。Mac Windows App名称读取noWindowsAvailable、bundle ID timeoutReached，需人工打开主窗口，尚无隧道/会话或凭据输入。余准备660秒/测试3600秒，无pending，所有项目SSH客户端退出，无测试/驱动/VM/游戏任务。重要经验：SSH -o含空格路径须内部引用；WinPS中文进度stderr保留字节、结构化stdout设UTF8；SFTP盘符绝对路径用/C:/。采集器已补MSI实际服务映像/有效FW状态，仅Mac解析通过；新版本未在Windows执行。下一09:00读账本与44，保持HY顺序、不重复旧基线或已通过连接。以下旧网络预约/阻塞为历史检查点。
- 当前Windows接入：用户gpresult为独立工作站/本地计算机，仅应用本地组策略，PartOfDomain=False；dsreg四个标志未显示，不能记为NO或证明没有MDM。结合localhost/Public三项未配置与有效Policies阻断，原作者仍未知。下一方案在既有用户接入配置授权内为localhost/Public明确新设AllowInboundRules=True，先备份NotConfigured；域状态、有效开启/默认Block及原值均保护，gpupdate计算机变化策略wait30后回读。不是宣称原本机GPO有False，也不直接删除Policies键。上一60秒查询已结算，新180秒准备b2d7ea45-5f1c-4fd8-9b74-9ce77b2e1fba pending，余准备960秒/测试3600秒；未在Windows执行/未连通SSH。来源/取舍/失败清理边界见research/43当前末节，混合PowerShell对象/原生输出缺字段不能当否定证据。
- Windows修正实际回执：接口Public，前后有效EnabledTrue/默认Block/AllowInboundRulesFalse，备份路径打印、Set后无报错；本地修改后值/覆盖来源未知，不能直接认定组策略/MDM/Clash。见research/43最新节；前120秒预约已按完成cmdlet/无测试任务结算，新60秒只读Public的Persistent/Active/RSOP、COM阻断状态与相关策略/本地键预约d83462da-0859-4914-a13d-b2f7a27c9085 pending，余准备1380秒/测试3600秒。命令仅本机解析通过，等用户输出；不重试连接/改代理/清除策略，及时结束回合供上传。
- 最新Windows快照见research/43末节：服务/2222监听一致，项目规则启用且程序匹配；到Mac的源地址约束回程走物理以太网。有效FW三配置AllowInboundRules=0是主要嫌疑，原生文字值尚未核对，不定论TUN或最终根因。准备仅当前接口配置、文字False/EnabledTrue/默认Block三重保护、备份原值后AllowInboundRulesTrue的手动修正；它会恢复同配置其他已有放行规则，不是仅2222。Mac解析通过，Windows未执行/SSH未认证。前60秒只读预约已按完整JSON/无运行任务结算；新120秒人工准备c20cbefe-18dd-44df-bdf8-b35b470a3e66 pending，余准备1440秒/测试3600秒。不要先进行连接/测试或重复旧探测，先等输出，及时结束回合供用户上传。地址/原始输出只私有。
- Windows新接入事实见research/43：TUN界面GVisor/Mihomo、auto-route on、strict-route off、auto-detect on、any:53/MTU1500，与Verge2.2.2若干默认值一致，不认定为错误，不再要求关闭已off的严格路由。Mac当前en0有到Windows内网地址的直连路由，ARP MAC匹配用户路由器截图；物理公网查询超时，没有验证同WAN。原只读180秒按已返回参数/没有runtime任务的范围结算；新120秒局域网pin准备实际5.025秒未取得宿主密钥，无认证/shell/测试，客户端退出后结算；没有改代理或接受未知host key。当前下一只读Windows服务/监听/FW/回程JSON命令已本机解析通过，60秒预约18e4f325-733b-46eb-a24e-6b862968764f pending，余准备1560秒/测试3600秒。不要因ARP或TCP本地建立声称已接通，不重复相同失败探测；等新快照。连接信息和原始输出只私有。
- 原队列恢复后的新检查点：HYP无下载/更新获用户确认。CUA在米哈游瓶子r2上off→on→off，用户Retina完整图顶部/底部未裁切（静态显示证据，非点击/游戏证据）；已恢复Retina=n、DPI96、高分辨率0并重开HYP5591，Steam未动。真实KERN_PROCARGS2读取HYP及五个helper均DXMT跨进程=1、msync=1、zh_CN.UTF-8；ps未读到不能记为变量不存在，spawn目标是根launcher.exe而非后续HYP。Windows首页确认Verge2.2.2/Mihomo1.19.3/服务模式，TUN参数未取得；现有180秒人工只读预约7ab55f34-be07-4136-8ef3-69bb539737ff pending，余准备1740秒/测试3600秒，不新开Windows工作。只读锁依赖代理报告42已收齐/关闭，父代理核对十个源码哈希及三条关键链，方案尚未实现/native验证；见research/41。用户需结束当前回合才能上传图片，交接时及时yield。
- 最新用户决定：本轮柚子社专项结束，恢复原HY1→HY2→HY3→HY4/HY5队列；视频有效帧/冷启动/长期质量回归保留，不继续挡住原队列。用户确认HYP当前正常、无下载/更新，允许必要显示验收；Windows代理软件已确认Clash Verge，版本/TUN设置仍未取得。预算status核对仍准备1920秒/测试3600秒且无pending，不重复已有基线/安装/旧连接探测。get_goal仍返回原Goal paused，工具无active恢复接口；按聊天恢复授权推进并更新同一heartbeat，不伪造状态或另建Goal。下方“原队列暂缓”为历史决定，已由本条取代。
- 验收交付已落地：两款兼容数据记录更新到组合引擎/2026-10-11，保持playable-caveats并明确视频/长期边界。debug App重新打包签名，CUA只重开Cider主界面，两款详情实际显示新说明、五项资料库保留，Steam97843/HYP49797保持、无游戏进程；当前留在资料库。同一heartbeat cider已更新到这次完整回执/下一视频与可靠性检查点，ACTIVE/每天09:00/原线程均核对，原Goal与HY/Windows暂缓不恢复。
- 本轮人工游戏验收已收齐：魔女2458530/build11913959与千恋1144400/build9212004分别Control/画面/声音/按钮、正常退出与一次同Steam实例暖重开通过。四次退出码0，最后千恋WinPID2708在13:43:57退出、13:44:01云上传OK；两款均无现存进程，Steam仍97843，未为重开重启客户端。使用原prefix/组合r1-wma1-mm1/msync，Windows主力机与Actions本轮0。按键时长未测量、千恋旧版同根因未证实、视频有效帧/20次冷启动/长期/全柚子社仍独立待验。后续按当前柚子社队列推进视频DMO有效帧和可靠性，原HY/Windows队列保持暂缓；不要重复等待下面历史段落的人工回执。
- 魔女同实例重开用户确认“重开正常，已退出”：13:39:29退出码0、13:39:38云上传OK，Steam97843未重启。该款组合引擎的画面/声音/Control、两次正常退出与一次暖重开已通过，长期/其他引擎不覆盖。当前已进入千恋独立核对：App1144400/build9212004/PE32，13:39:56 CUA详情启动→13:40:05新WinPID2296/宿主2141→13:40:06Completed，Cider显示运行中。等用户千恋剧情Control/松开/画面声音按钮及菜单退出回执；若卡住保留现场取证。原HY/Windows队列不恢复。
- 后续用户回复“正常，已退出”：组合引擎魔女的画面/按钮/声音、Control松开后可操作及正常菜单退出通过。Steam13:36:25记录WinPID1696退出码0，13:36:28云上传OK，本机游戏消失/Cider恢复“可运行”，Steam97843未重启。进程生命周期24分18秒不等于Control按住时长（未测量）。再点Cider：13:37:02请求→13:37:09新WinPID2064/宿主1967→13:37:10Completed，约7秒创建，Cider状态正确；当前只等这次重开窗口可操作/退出回执，随后单独核对千恋。两入口和快进验收限这款/组合引擎，不推广长期或其他版本；原HY/Windows待办仍暂缓。
- 13:10用户回场复测：报告Steam能进入魔女游戏且有操作/声音；该次生命周期未被本机独立捕获。新增日志确认修复后12:26请求等待KickingOtherSession、13:10请求等待ShowInterstitials，不能把这两项正常确认等待归作音频死锁。随后CUA点击Cider启动，13:11:57新请求→13:12:07真实游戏WinPID1696/宿主489→13:12:08Completed，Cider显示“已开始运行”和“运行中”。Steam宿主97843未重启、组合引擎/msync未改。当前等用户这一轮画面/声音/Control与正常退出回执，再核对重新启动；PID不是窗口证据，不自动踢出其他设备。见research/40最新节；下方无游戏/待回场条目为此前阶段。
- 音频阻塞修正已本机落地：0008 WinMM按需真实format mask、局部caps/独立COM、InitOnce暂存/完整发布及失败重试。基线serial/concurrent均20秒退出124；新32/64 mixer4并发、错误ID、mapper、两capture metadata部分请求、真实output及HWAVE分支均退出0（650/1324ms,count3）。原Steam瓶子切组合cider-cx26.3-r1-wma1-mm1，WinMM哈希匹配、原引擎/快照保留；WMA32/64再次实际退出0。12:26:09自动登录OK，正确64位live附加已采到正常IPC事件等待/UI消息循环，无音频阻塞。游戏进程仍未出现、云同步已结束，Wine AX仍timeout；已请用户点Steam开始检查弹窗/启动选项，等待该人类证据，不能标全部入口已修复。完整输入format mask仍走同步HAL，未录音/改系统设备/msync，详见research/40。
- 最新启动失效证据：新版Cider冷启动真实复现，120秒未见游戏，Steam自身BMainLoop停滞/跨线程pipe fatal并退出。原assert dump离线真实上下文：主24等服务；IPC240卡mmdevapi IsFormatSupported→WinMM测试所有设备/格式→mixerOpen。实际params为eCapture/shared/mono11025Hz8bit首项；具体Apple阻塞调用/端点UID尚未确认，msync因果无证据。Steam.exe当前PE64/AMD64，先查架构，不能用常见32位假设；派生线程视图元数据非真实异常。原始dump私有，见research/40。默认内建音频和非默认Continuity输入仅作候选，不写设备姓名/UID、不改系统设置或录音。
- 当前音频源码工作：优先WinMM懒查询真实dwFormats，让mixerOpen只初始化需要的元数据；并复核完成发布/失败重试。真实wave-capability/实际Initialize保留原支持判断，不用AudioConverter成功假造AUHAL支持；底层HAL延迟仍有验收缺口。源码子代理范围仅0008补丁，主代理构建/整合/验证。WMA/Control修复及用户实测通过保持，原HY/Windows队列暂缓。
- 最新修复（2026-10-11）：魔女中文版2458530/build11913959真实挂起为WMA DMO耗尽后遗留INCOMPLETE，qasf持stream_cs空转，游戏主线程Stop媒体等待。Cider0007把dwStatus清理提前；候选cider-cx26.3-r1-wma1只改32/64 WineGStreamer PE，原引擎/完整前缀快照保留，已切换原Steam瓶子并恢复普通日志。32/64各32次空输出及有效WMA→32768 bytes非零PCM→耗尽检查均真实退出0；用户确认长时间Control无卡死、可用。此项已验收，千恋与视频像素/长期重启仍独立待验。
- 当前剩余优先级：用户特别确认Steam按钮和Cider启动游戏迟迟无窗口两入口都需定位。两入口共有Steam-applaunch请求，旧客户端未处理启动及正常退出，新实例可启动；尚无失效时Steam线程证据，不宣称WMA已修复Steam。CUA实际点击Cider已观察到新游戏进程。AppModel原永久“等待窗口”反馈没有确认逻辑，现已实现真实app进程确认/120秒状态提示/重复请求抑制/迟到提示保护，构建及新UI部署/复核继续收尾；PID不等于可见窗口。原HY1–HY5/Windows队列保持暂缓、额度32+60分钟无变化。
- 启动反馈复核/部署后续：单个合并扫描，操作使用dispatch之后扫描的返回结果；提示代际含相同文本赋值/关闭，整瓶停止按保存的瓶子身份清pending；期限独立于扫描处理，目录移除中性提示。四项审查及停止成功/失败代际修正通过源码复核；debug构建/打包签名和CUA重开新界面完成、五项资料库保留。没有做超时/并发故障注入。更新主界面附近原Steam和游戏均退出，用户已确认是其手动关闭；不计异常退出。正在从新版Cider复核完整启动/反馈。
- 后续取证通道：同位数i386 WineDbg取消自动64位重启、设置kill-on-exit=false，仅对无账户lab自建Sleep进程实际附加、主线程栈、detach成功，目标持续tick至结束。私有产物未进入引擎、构建缓存原源码恢复，真实Steam失效仍未取栈。未修改原同步模式/游戏文件，Windows主力机与Actions未使用。
- 诊断教训更新：64→32 WineDbg附加虽关闭kill-on-exit，远程break-in仍产生后加执行异常/默认调试器；不能当作游戏原始错误、不能重复用于健康游戏。本轮全部相关进程在原瓶子切换时结束。独立冷prefix COM超时与builtin按native加载失败未计通过。GStreamer共享DMO helper本次实际影响WMA/video；color/resampler的DMO未实现，MFT走另一helper。原始dump/会话只留私有缓存，公开仅元数据/哈希。见research/39最新节；下面早期“等待复现/未修复”条目为历史阶段。
- 最新优先级（2026-10-11）：用户要求记录现有进度，暂缓原HY1–HY5/Windows接入/参考环境待办，优先修复《魔女的夜宴》按住Control快速跳过一会后卡死、只能退出。用户推测《千恋＊万花》也会出现，尚无该游戏独立故障证据。修复保持存档/登录，先取日志/线程证据，不克隆登录Steam前缀启动。旧Goal已经paused；定时任务在此期间只围绕此故障推进，不恢复原队列或消耗Windows主力机额度。检查点见plan/13临时优先级。
- 用户随后明确：卡死时Mac鼠标能动，游戏窗口已退出；没有现存卡死线程可采样。实际Steam瓶子为r1/msync默认，《魔女的夜宴》目录sanoba witch、SabbatOfTheWitch.exe；游戏单独会话日志0bytes，既有Steam进程的日志不自动等于游戏的独立日志，须建立有效诊断启动/复现。未定位根因，不能用旧米哈游msync pool警告直接定论。
- Control卡死独立复现：全新r1实验瓶子没有复制Steam登录；游戏SteamAPI初始化无法找到客户端，未进入故事场景，实验瓶子已停止。新宿主CUA仍timeout，不复用失败身份/AX路线；已请用户通常方式复现后保留卡死进程，等其回复再读全线程栈。缺少该证据前不盲改同步/渲染或宣称修复。记录见research/39；正常Steam/HYP未重启、用户存档未改。
- 后续启动阻塞：实际中文版Steam AppID2458530/build11913959，非888790。Cider目标正确，但旧Steam启动/正常退出请求均未被处理；游戏存档本机备份后，只结束并重开原Steam瓶子。恢复步骤第一轮遗漏跨进程图形env，已在没有游戏进程时纠正；正确环境重新启动后自动登录OK，10:41:18新增游戏及Steam跟踪，登录未要求重输。启动恢复不等于原快进问题修复。install/WINEDEBUG的seh跟踪在这款游戏内约152MB刷屏，已暂停复现、压缩快照/截短自己生成的日志，等待用户报告当前状态后安全退出并回到正常日志级别。不要在未知未保存进度时强杀游戏，不再以详细seh环境做性能/快进结论。LLDB附加超时，未取得线程栈，不能归咎msync；系统日志的通讯录归因不授权申请通讯录。见research/39更新。

- 目标：Apple Silicon 上达到 CrossOver 质量或更高，舒适美观的原生 UI；柚子社全系，以及原神、崩铁、绝区零官方 Windows 国服的本地适配。逐款验收登录、交互玩法、图形、音视频、输入、更新、可靠重启。
- Epic 已明确放弃。米哈游路线仅 Windows 国服，不用官方 iOS/iPadOS 版本，不外联寻求合作。
- 用户允许为目标复议工程取舍，要求具体证据、代价、验收和回退。2026-10-07 已授权立即实施评审 A01–A17；不要反复索取已有授权。
- 默认自己研究和实现；仅超大任务启用1–2个独立、范围明确的子代理，复用结果并做对抗性复核。节约 GitHub Actions 额度，不例行触发完整引擎构建。
- 2026-10-10 用户愿意自行复制执行简单的 Windows 操作；优先提供简短、可核对的命令和所需输出，不重复已失败的远程长文本输入路线。
- 2026-10-11 用户明确Windows是主力机，要求累计占用时间、方便的远程通道和白天无人值守调度；确认09:00–17:00 UTC+8、与Mac不在同一局域网。维护者将后续使用封顶为累计准备60分钟+测试60分钟，非每日额度，不承诺该额度内完整游戏适配；既往真实机时缺记录，不编造。失败/重试/启动/清理计时，单批最多10分钟，预算耗尽停止Windows工作。
- 已创建当前聊天每天09:00 heartbeat `cider`；配置已核对ACTIVE。SSH还未接通，定时任务在此前只推进Mac工作，相同阻塞保持安静。用户随后说明有公网IP、能开服务，但不知道已有服务/端口。先只读核对SSH/RDP，再评估复用公网SSH+原生RDP桌面通道（可经SSH隧道）；Tailscale是未安装的备选。CUA的RDP截图/点击须实测，RDP不代替物理控制台图形/性能证据，断开不等于清理。密码由用户输入，私钥/地址不记入公开记忆。机时持久账本与待部署本机watchdog边界见plan/14。
- 用户回传服务查询截图：TermService=Running，sshd没有返回记录；两条Select-Object输出列不同，被PowerShell合并格式化，第二项fDenyTSConnections未显示，不能据此判断RDP已启用。后续用单一JSON/明确文本避免混合表格。Mac标准路径未见Windows App/Microsoft Remote Desktop；桌面客户端还须准备。
- 已为当前在场手动SSH开通交接预扣600秒准备预算（预约ssh-attended-setup-20261011，pending，持久账本保存唯一ID）。剩余准备3000秒、测试3600秒；返回安装结果/实际进程清理证据前不关闭预约或新开Windows任务。人工准备可在用户当前在场时进行，自动化不能使用attended-setup标志绕过09:00–17:00窗口。新SSH脚本仅静态审查，尚未Windows运行或外网连接。
- SSH交接脚本a42b3d4在Windows解析失败：26:9 MissingEndParenthesisAfterStatement、28:80 UnexpectedToken。逻辑操作符放在下一行造成断句，脚本体未执行，不能记为安装失败或系统配置已改。已在Mac缓存取得微软PowerShell7.6.6（官方资产SHA256校验），完整复现同样错误；操作符移到上一行末尾后，四份PS脚本均零解析错误。以后发送PS脚本前先本机解析，执行命令再用Windows自身Parser.ParseFile检查，不让用户承担可本机发现的语法问题。Mac语法结果不替代Windows5.1实际cmdlet/安装验收；修正沿用原600秒预留，不新增扣账。见research/evidence/windows-access-powershell-syntax-20261011.json。
- 用户执行906e199后下载/校验/Windows解析通过，进入OpenSSH组件安装；Wait-Job的300秒限额触发，尚未执行密钥/端口配置。Windows servicing后台是否结束未知，原预约保持pending，不重跑安装或并行换另一套部署，不称已清理。用户确认Windows为TUN规则模式（国内直连），终端必须保留代理；不关闭TUN/终端代理，不擅自改WinHTTP/更新组策略。系统组件的Windows Update通道与终端下载成功分开判断，超时不证明代理根因；下一步只读服务/WinHTTP/DISM末段。微软官方独立MSI为未采用候选（约6.6MB、10.0.0.0p2-Preview，维护更新另算），见plan/14。
- 后续原始输出确认WinHTTP配置DIRECT（不等于TUN未作用）、仍无sshd；DISM在01:31:58以0x800704c7取消，随后Finalize/DeletedSession/Shutdown/Ending均出现。结合脚本300秒限额，确认这次请求已取消并关闭；初始耗时原因未证实，日志没有给出代理连接错误。仅观察到309秒DISM片段，不能冒充整个准备总耗时。按真实关闭证据关闭首个600秒预约、不退款；人工交接总时间无测量则明确null/保守上限，不填虚构数字。另预留600秒用于MSI准备，剩余准备2400秒+测试3600秒，新预约pending。
- 已准备UseMsi替代执行：官方10.0.0.0p2-Preview临时测试接入包；用户终端下载、固定长度/SHA256及Windows微软Authenticode双校验、只装Server、不改代理/客户端PATH。安装前只读确认in-box NotPresent，15秒查状态限额、下载120秒、MSI等待120秒；超时保留未知状态不重试、不杀系统安装服务。验证真实服务路径后才管理本次服务，SFTP使用明确路径。Mac语法解析通过，Windows签名/安装/连接仍待回传，见research/evidence/windows-access-capability-cancellation-20261011.json。
- 后续实际成功：用户回传UseMsi结果configured，来源a3f7d089…，脚本体19.92秒（不含外部下载/人工交接），项目公钥指纹匹配，SSH监听/真实服务路径检查通过，RDP启用。第二600秒准备预约已按成功结束证据关闭、不退款。安装不等于外网登录；账号/宿主指纹/地址只在受限本机配置，不写公开记忆。
- Mac Windows App11.4.3(3115)已从微软官方独立pkg准备，Microsoft UBF8T346G9签名/公证和app深度签名均核对；只部署app载荷，未改现有Microsoft AutoUpdate。CUA已完成初始页并保存Cider Windows Reference本地SSH隧道连接，剪贴板/文件夹/打印机/智能卡/相机/麦克风未重定向，无保存凭据。后台App弹出菜单需先调用已暴露的Raise再点击；坐标输入仍报noWindowsAvailable，真实RDP画面操作尚未验收。
- 用户确认真实公网入口后，假定2222的严格握手验证只得到连接关闭，没有宿主密钥，不曾登录或执行Windows命令。用户随后澄清“转发完毕”其实只是Windows防火墙入/出站放行，路由器NAT映射仍缺；不要混淆两层配置，不继续无变化端口探测。额外180秒接入预约已结束，无pending；剩余准备2220秒（37分钟）+测试3600秒（60分钟）。Windows就绪、外网路由、认证、桌面控制、watchdog分别验收，见research/evidence/windows-reference-access-setup-20261011.json。
- 后续新证据替代“路由器映射仍缺”的旧状态：用户路由器截图确认TCP2222正确映射到Windows的2222，WAN与已提供公网入口相同；设备页16KB/s/26KB/s是当前流量，旁边注明无限制，不是已设限速。Windows回环探测返回`SSH-2.0-OpenSSH_for_Windows_10.0 Win32-OpenSSH-GitHub`，证明本机版本响应，尚不证明宿主密钥或公钥认证。Mac系统路径keyscan仍在取宿主密钥前关闭，绑定物理网卡的连接超时；显式本地SOCKS连接获成功回复后未收到SSH标识。控制器未捕获目标规则，不能宣称已查明走DIRECT或代理、不能把物理绑定超时定为Windows故障。没有修改代理/防火墙或执行远程命令。该180秒准备预约已结算，无pending；剩余准备2040秒（34分钟）、测试3600秒（60分钟）。下一步只读Windows SSH日志，不重复安装或改NAT。 已另预扣60秒用于在场只读日志交接，剩余准备1980秒（33分钟），当前该预约pending；回传命令结束结果前不新开Windows任务。
- 用户明确要求结束当前回合后才能上传截图/结果；需要人工交接时给简短具体操作并及时yield，不让连续异步追问阻碍上传。
- 最新只读日志确认Windows在所有IPv4/IPv6接口监听2222，唯一会话记录为用户回环查询；安装期间22的监听属于先前短暂默认服务，不能记成当前22/2222同时开放。Mac随后通过核心`/logs`捕获本次SSH实际选路：`dial DIRECT (match GeoIP/cn)`，随后公网TCP2222的真实拨号`i/o timeout`。这次已确认Mac发起端使用DIRECT；早先本地TCP established/Connection closed可能来自TUN/SOCKS代理端，不足以证明Windows端曾接受再关闭连接。默认Windows事件日志没有公网会话本身不证明包未到达。下一步核对Windows代理软件及TUN配置，区分回程/入站路径与实际转发，不能直接归咎密码、反作弊或断言TUN根因；保留终端代理，不关闭TUN或重装服务。两次60秒准备预约均已结束，无pending，剩余准备1920秒（32分钟）、测试3600秒（60分钟）。
- 2026-10-11 白天heartbeat：仅Mac实现Tools/windows-task-runner，创建时Job绑定、独立截止线程、STOP/窗口检查及未确认/完整回执分开；交叉构建和隔离r2的9项新合成进程检查通过，实验瓶子已停止。主线程故意卡住时仍由独立线程到期终止，不生成成功回执；Unix退出状态180是ERROR_TIMEOUT低8位，不冒充Windows原生结果。尚未Windows原生构建/部署或验证SSH嵌套Job、失联、窗口边界/I/O故障；不管理服务/WMI/已有VM/另一RDP活动。完整机时还需控制器测量，不能仅凭回执关闭失联预约。见research/38。没有使用Windows、复跑权限基线或触发Actions；预算保持32+60分钟且无pending。
- 已更新同一heartbeat的提示，删除“服务/端口未确认”旧事实，按最新记忆复用接入和本机验证结果；相同阻塞不重装、不扫端口、不反复索取原有TUN交接。
- 2026-10-11 用户询问前台目标暂停与方向。get_goal确认原目标仍为HY1→HY2→HY3→三款本地适配，paused更新于10月7日13:03（UTC+8）；原始历史确认是用户当时“你先休息会吧”后的显式暂停。后续聊天/每日heartbeat有实际推进，但这张Goal卡尚未恢复；Goal连续推进与heartbeat状态分开核对。恢复由用户/系统目标控件管理，当前目标工具没有active恢复操作，不改底层数据库、不伪造恢复或另建重复目标。最近接入/限时工具属于用户新增主力机预算要求；工具准备已收敛，继续将可推进工作集中于HY1余项与HY2契约/实现，原生kernel/KMDF及三款本地会话仍未通过。
- Git 作者和提交者只能是 CervantesHallo <227578309+CervantesHallo@users.noreply.github.com>，不加 AI 署名。

## 始终保留的边界

- 不篡改、伪装、隐藏或绕过反作弊；不以安全 API 成功空桩冒充实现。Wine 可识别；未完成兼容验证的三款游戏当前禁止本地启动。
- 不在用户已登录 Steam 的前缀副本里启动 Steam。以前这样做实际使原登录失效，用户被迫重新登录。当前副本初始配置即标记阻断，并在首次 Wine 运行前隔离启动项；保护原前缀。
- Engine R 为 x86_64 Wine/Rosetta，保留 cpu_backend 架构参数化；只建新 WoW64 64位瓶子。Wine 环境显式构造，完整 xx_YY.UTF-8 locale；msync/同步为瓶子级，切换先停止。
- Wine/依赖必须用 macOS15 SDK。macOS27 SDK 的 pipe2 检测曾使 wineboot 跳到空函数地址；本机增量构建使用 MacOSX15.4.sdk。
- D3DMetal 由用户从 GPTK 导入，不提交或打包；文档不新增法律/商业分析章节。

## 已确认的经验与当前事实

- 千恋＊万花、魔女的夜宴及 Steam 有历史运行证据；不能把它们推广成全部柚子社或全部 Windows 程序已经通过。
- 原神本地7.1.0国服安装：7777个本地清单项存在且大小一致，主程序MD5符合本地清单；Windows路径可访问。未做全部资产哈希或游戏会话。当前确定启动障碍为 Cider 门禁，原“找不到文件”弹窗/返回码尚未复现。
- HYP 顶部裁切有真实日志：client.top=82、r1 visible.top=112；本机 r2 修复后 Wine/Cocoa.top=82。2026-10-07 用户完整截图显示顶部/底部内容完整，可拖动不能缩放；用户Windows对照的最新纠正也是“不能改变大小”，以最后回复为准，不强制加缩放。Windows启动器版本未记录；按钮点击、最小化/恢复、不同显示比例仍待验收。
- 米哈游 CEF 必须有跨进程子窗口支持和 DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1。Profile 现在匹配默认安装路径及数字版本目录，不凭裸 launcher.exe/HYP 名称命中；自定义目录需要明确范围。
- 多个灰色 Dock 图标与宿主缺 LSUIElement 有关。已设 agent、保留真窗口前台晋升，7个引擎已迁移。不要改系统 Dock 固定项掩盖问题。
- 自动化读取裸/宿主 Wine 窗口仍 timeout。2026-10-07 独立的逐程序 bundle ID 已生成并实际重启HYP，但按路径/唯一ID读取仍timeout；该方案已撤回。新标识不等于视觉验收，不重复无变化重试。
- 历史 HYP 日志30.76GB且末段有msync pool exhausted。不能把它直接认定为每次白屏根因。诊断已改每文件最多4MiB头尾读取；不要全读巨型日志。
- SePrivilegeCheck/SeSinglePrivilegeCheck 的 KernelMode 成功有微软契约依据；前者还有输出标记义务。UserMode 的令牌/启用状态、集合和输出须独立实现/对照。当前 CX26.3 的 token 引用及 context 捕获/锁定/释放仍为空桩，不能拿 user-mode token API 冒充已完成依赖。矩阵 revision 2 校正了两个权限条目与模型概述；不是运行时修复或全表重新对照。
- 2026-10-07 用户授权通过 ToDesk 访问 Windows 参考机 Cervantes，手动接通并打开 PowerShell。实际只读输出：Windows11专业版10.0.26200.9457、PowerShell5.1.26100.9444、AMD64；用户完成固定提交/哈希校验采集，SDK Include26100.0有Windows.h无km/ntddk.h，VS2022 BuildTools17.14.36518.9/MSVC14.44.35207可用。不是构建或内核对照通过；EWDK VS2022候选待补齐。Include目录`.0`不能当成真实QFE。
- ToDesk坐标仍报noWindowsAvailable；复杂文本/组合键丢失，错误命令执行前清空。关键命令须核对实际输入，不执行丢字符后的代码。普通字符/回车仅能完成有限查询；不绕过执行策略或改变系统保护来补工具通道。记忆不保存设备代码、IP或凭据。
- 用户已确认获取/使用微软25H2 EWDK VS2022，约18.63GiB的ISO已完整下载到Cider缓存，SHA256为9f48251dd24ad31aac206d8256e95bda5f90a9783982c45a8aafeb9054562379。只读检查实际版本26100.6584、VS17.14.5/MSVC14.44.35207和WDK头文件。2026-10-10 用户回传Windows端相同SHA256及EWDK26100.6584/VS17.14.5横幅，但称未出现命令提示符；不能记为初始化完成，实际cl/msbuild路径和构建仍待取得。入口须LaunchBuildEnv.cmd amd64，默认参数会选x86，见research/34。
- 2026-10-10 后续截图已见CMD提示符；`where cl`实际输出镜像内MSVC14.44.35207的Hostx64/x64/cl.exe，随后用户报告5分钟未返回且无法输入，截图也无查询后的提示符。编译器文件定位不等于执行/构建通过；卡点未确认。后续优先用明确工具路径和CMD内置命令，避免再次完整扫描PATH；Ctrl+C恢复效果、Platform及MSBuild仍待回传。
- 后续窗口已恢复提示符（用户未说明使用哪种恢复步骤）；两条粘贴命令被合成`echo %Platform%dir ...`，输出前缀确认Platform=x64，但dir没有执行。改为单行明确路径调用MSBuild后，用户回传17.14.10.27608；Windows初始化及MSBuild版本输出已取得，仍没有cl执行/编译/链接或内核契约结果。简单Windows命令尽量每次一行，见research/34及初始化原始字段记录。
- Windows最大化/英文输入后短命令可用，但可靠长输入未闭合：1000字符的首段预期1002bytes、实际992bytes，哈希不符；未解码/执行，已停止后续传输。Windows镜像传输、挂载和实际工具初始化需人类协助，不重复该失败路线。
- Wine固定快照已有primary token引用/释放，CX对象映射可评估复用；请求线程映射及跨guest token读锁须一并处理，impersonation/context与权限检查仍需实现/对照，见research/33。
- A01–A17 的代码已实施并构建，故障注入、完整回归、真实Windows对照仍待完成。内核矩阵/参考规格不是内核运行时实现，启动器不是游戏可玩性。
- 2026-10-10 用户确认Mac HYP设置、最小化恢复、拖动后顶部完整。HY1路径审查后已实施瓶子感知盘符映射、helper安装范围、保存启动器同一lifecycle、模拟重启完整锁/退出码、官方配方安装环境profile绑定；App/CLI及本机debug app已构建。1×/2×、完整入口回归、首次安装渲染仍待验收，r2未一般发布。见research/35、36。
- Tools/windows-reference 已有独立Win32客户端/只读WDM参考驱动、定长无指针IOCTL和MSBuild脚本。客户端交叉编译、驱动真实WDK头文件语法检查成功，不是Windows原生构建/驱动运行。客户端只改变私有token副本并恢复线程；输出只读观察，不采集SID/用户信息。Win32结果不代替kernel契约，KernelMode合法成功不等于空桩。
- Windows采集脚本必须同时处理正/负退出状态；if errorlevel 1不等于“非零”，可能漏掉崩溃。输出流必须检查fflush与sticky ferror，参考输入/产物有来源提交及哈希；不得把部分文件作为成功记录。build.cmd只构建未签名驱动并运行Win32部分，不改变系统保护或加载驱动；原生kernel环境仍须独立补齐。
- 本轮新版out/Cider.app已通过CUA菜单退出旧主界面并重开，原5项资料库保留、Steam/HYP运行状态仍可见；点击HYP启动返回“已在运行”。米哈游瓶子仍r2、Retina off；这是GUI/已有实例分支观察，不扩大为真实停止/重启或1×/2×通过。
- Windows参考源包已发布为windows-reference-20261010-r1（实验prerelease），源提交c41b3d2，ZIP 14858bytes / SHA256 ce5bd8b0da26890e52f3c12ce0371e8943ce7902a87962cf71460f4da32eab9b；GitHub资产摘要与本地一致。已请用户在EWDK CMD以一行完整路径执行build.cmd，回传user.jsonl及client/user两份SHA256。这是原生执行的人类交接，尚未收到本工程Windows结果；不重复远程长命令、不把代码发布当作契约通过。所有子代理已关闭。
- 用户随后实际执行r1原生编译：client/reference.c第98/108行C4018，有/无符号比较；WX触发C2220，未进入驱动或采集。FIELD_OFFSET的LONG与DWORD长度比较是根因；r2用标准offsetof并显式存入DWORD，保持所有边界检查和W4/WX。本机额外Wsign-compare/Wconversion交叉编译成功；Windows r2结果待回传。不要再把本机交叉编译推广为MSVC警告兼容。
- 修正已提交c81f34c并发布windows-reference-20261010-r2；源包14875bytes、SHA256 5c113ebcaa8044a0330a7c155f384c5a9a8a6024fb82bf0c56f9e5b6b4fde746。用户原解压目录为D:\\windows-reference，重新覆盖源码后可直接在原EWDK CMD执行同一build.cmd路径。r1编译失败作为历史证据保留，r2原生重建待回传。
- r2后续原生截图已回传：客户端exe、WDM驱动sys均生成，Win32采集显示Saved/Completed且回到提示符。Inf2Cat/DrvCat因无INF/catalog跳过是此次仅构建流程的预期状态；截图上方C4018属旧轮次。原始user.jsonl和两份哈希尚未收到，不能确认源码提交或实际API输出/完整性；kernel驱动尚未执行，游戏门禁维持。
- 原始32条Win32数据及两份构建日志已收到，JSONL e1cbb124…哈希匹配；随后ZIP中681472bytes x64 EXE的16ef0e9e…SHA256也独立匹配。实际来源c81f34c、MSVC19.44.35209、Win build26200；目录14.44.35207不是完整编译器版本。两份构建均0警告/0错误。WDM构建通知Using KMDF 1.15不等于WDF运行或KMDF验收。
- 同一EXE已在全新CIDER_HOME/r2/en_US.UTF-8/msync瓶子运行，退出0；5个令牌快照、25个PrivilegeCheck和完成记录与原生一致，session仅Wine可见/OS build不同。原始三项权限状态相同；ALL部分失败仍标记启用项USED_FOR_ACCESS。只证实这组Win32输入，不证实Se*/Ps*、并发锁或游戏；原生kernel测试环境问题待用户回复。实验瓶子已停止，未复制用户前缀或启动游戏/Steam/HYP。
- 实验发现新建prefix尚未存在时URL目录尾斜杠导致一致性校验误拒。WineRunner已保留FileSafety/链接/配置/引擎/locale/sync检查并改比标准化file path；真实新瓶子创建及参考运行通过，App/CLI构建成功。见research/37。后续不要用URL目录hint的相等代替同一文件路径身份。
- 新瓶子修复后的out/Cider.app已打包签名并经CUA菜单重开，原资料库/Steam/HYP运行状态保留；原生kernel环境的问题已发给用户，仍等现有VM/签名环境说明。不要把等待原生kernel环境改成重复执行Win32或重新传20GB EWDK。

## 接管入口

1. 先读 AGENTS.md、`docs/plan/00-strategy-and-decisions.md`、`13-roadmap.md`。
2. 读 research/26–29 获取真实日志证据、评审与取舍；后续报告会继续追加。
3. 检查 git status、实际引擎/瓶子绑定、当前进程；不要盲目重做实验或重启登录副本。
4. 原顺序：收尾 HY1 → HY2 Windows 契约/依赖组 → HY3 证据到许可闭环 → HY4 原神本地会话 → HY5 另两款。遇到视觉或 Windows 参考环境缺口，明确记录范围并继续独立可推进工作，不伪造通过状态。

最新F14发布检查：实际Clang将空OR0 RMW降低为stack lock-or+hint load，旧“同字RMW”解释不适用于该代码gen。已改无语义依赖的PUBLISH位29做真实同字SC xor，count位0–28，MANY30/STICKY31；bit值和翻转周期不参与判断。实际probe为lockcmpxchg hint指针。最终helper58fc51d1…、native bb682b03…/cdb9dff3…；原生ABA及三个负对照再过、独立末次审查无P1/P2，尚未Wine。旧零RMW阶段的记录/二进制另存zero-rmw-probe不当采用结果。私有stage/policy已更新最终pair，GUI需重建后再11阶段。所有旧cohort保持。
