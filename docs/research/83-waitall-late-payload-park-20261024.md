# WaitAll：在原生阻塞入口前发布保守等待登记

状态：F27源候选及组件构建完成，非默认；限定实际二进制复审、最终原生边界及负例已通过，自己的13阶段Wine与11份实际身份核对通过；固定六轮AC两门已过，精确产物自己的连续小时亦已通过；完整M1及默认采用仍未声明。接续82与原02，Windows仍busy/不能重启，原HY顺序、游戏门禁及5倍/CS±3%保持。

## 依据

F26确实大量使用新SetEvent路径，仍未过原性能门槛。独立固定errno诊断22050次raw wake中20425次ENOENT，1625次返回0、无其他错误；这是插桩观测，不能外推正式样本比例。它支持研究减少没有对应等待条目时的内核调用，不支持按一次wake结果清除潜在等待者。

F22的保守hint登记在原协调快照中发布，覆盖整个后续等待过程；F23的64次只读预等待发生在这个登记之后。原样拼接会让正在CPU预读的线程仍保持positive hint，不能达到缩小空唤醒窗口的目的。限定独立设计审查建议：晚发布仍须在原owner内，之后复核payload；不删除owner要求或发明owner-free内存序。

## 本次实验选择

- 基于F26 revision2，只复用F22的共享hint布局与饱和/碰撞/死亡/fork保守规则。magic7新增256KiB映射，client/server必须匹配；不直接复制旧header而丢掉F26 before_wake、bounded try或恢复保护。
- 普通完整mask等待路径，在原owner内确认对象来自真实pin、普通auto/manual event且非alertable，并选定WaitAll的blocked leg。只传递eligibility，不发布hint。
- 原生single-wait入口最多64次只读预等待，owner未持有、原控制交付仍可工作；观察ready只返回PENDING，让原事务真正获取，不消费对象。
- 预算耗尽后通过原coord_enter重新验证代际/准入，在owner内发布caller持有的node并重新检查payload。释放owner后再做最终payload检查、重算原绝对deadline，才进入原ulock wait。
- node一直保留到helper返回或原生unwind，EINTR/EFAULT不重新发布、不重置预读预算；normal pop与unwind都在pins释放前完成。fork child只断开继承链，不减父进程共享计数。
- count0借用alert槽的非零等待仍无条件置sticky，不能仅看当前类型；零超时不登记。alertable/Mach、mutex、semaphore、server对象及多对象WaitAny保留原路径。
- SetEvent封闭路径只读取预映射hint，不访问新TLS或初始化。blocking/recovery和非普通对象仍无条件wake；独立MANY检查前保留SC fence及实际Mach消息分支的完整mask、F26恢复控制边界、原yield。

不能把hint称为“当前实际睡眠线程数”：入kernel前必须先发布，碰撞/饱和/死亡也允许false positive。它只保证有可能进入或仍在raw wait时不会false zero。

## 成本、接受与回退

代价是256KiB共享提示区、额外owner往返、64次CPU预读及计数更新，可能增加功耗、竞争或尾延迟；不因为每次有界就声称全局公平。收益未证明，候选不默认选择，不切用户App/引擎/瓶子。

必须验证精确新header、真正的发布/检查/park间竞态、ABA及后续唤醒、取消/控制/unwind、绝对期限和负例；实际source/binary复审通过后，才跑自己的两位宽短功能及固定三对原性能。两门均过再跑这个精确产物自己的连续小时，不借F7/F8或F26结果。回退为F26原完整通知路径。

当前组件SDK15.4/O2零警告；native `64547f41…` / `7b0cb109…`。sync.c与msync.h及其直接依赖未变，复用精确sync.o；client与匹配magic7的server已重新构建。构建不是运行、性能、Windows或游戏验收。

## 已完成的限定验证

独立source/实际x86复审无具体P1/P2：确认晚登记的fresh owner/epoch检查，pins/unwind/fork顺序、原absolute deadline，以及SetEvent通知中的纯hint读取/SC fence/真实消息mask，实际client/server的magic7、offset `0x8010000` 和size `0x8050000`一致。300个global定义符号集合相同，三个实际变更文件精确重建；sync.c/msync.h逐字不变。

精确新header的coordinator恢复、try、ready-body及single-hint共八组原生执行在两架构通过。最终公开late-park夹具两架构各14线程/321检查，SetEvent/hint/frame夹具各38线程/676检查，全部join。五个late变体及六个SetEvent变体实际编译并拒绝；其中unconditional raw wake仅为丢失优化的对照，不称其行为不安全。

夹具初审发现三项P2和一项默认harness绑定缺口，均已修复并经独立复审：worker只能断言自己不持有owner，不能对仍在提交的parent断言全局0；最终在join后再检查全局0/CLEAN/parent准入归还。新增EINTR/EFAULT之间deadline到期的构造time-double场景，真实编译的cached-relative-time变体被拒绝；原late-owner到期仍是真实elapsed-clock场景。运行回执绑定fixture、提取body、frame（SetEvent）和coordinator header全部输入，构建/运行后复核不变；公开默认harness已与实际源一致。初版私有扩展夹具/默认元数据不一致及旧回执保留，不改写成新条件通过。失败cancel/join会直接退出私有进程，避免在自有worker仍可能存活时退过栈存储。

取消原生场景只证明pending请求在显式清理后交付；不是Wine控制信号实际驱动live登记移除的证明。fd/token/control/time/notification均有明确native doubles，完整Wine回归仍独立。首个无publication负例因未消费参数编译失败，修正负例构造后才实际执行拒绝，原失败保留。临时提取脚本一次语法错误未产生运行结果，亦不计引擎结果。

自己的两位宽13阶段Wine已通过，11份实际native/双PE/EXE/runner/helper身份及原时钟检查逐一匹配；策略负例真实1260，未启动替身。固定六轮AC/LPM0完整结束：server10000/candidate2000ns=5.0倍，CS0.815/0.820ns/op=+0.6134969%，两门均通过。首个自己的3600秒/8线程压力已预先声明并启动；不借旧小时，完成及清理前不计通过。没有用旧候选或native doubles代替本次运行、性能或连续小时。原F26结果和Windows状态保持。

## 自己的连续小时结果

首个精确pair自己的3600秒/8线程完成：guest3600016ms、78检查0失败，宿主活动约3612.5秒（含准备/清理），无检测到睡眠间断；获取与成功归还均758497967次，最终计数1/max1。532269729次并发超上限释放返回原错误，未钳位/丢弃许可冒充成功。

3567份有效producer快照分配减回收均等于current，峰值36/fallback0；3份启动或边界快照不可用，不当0。FD23–295、RSS339968–14336000字节为包含启动/运行/退出的观察范围，不据范围差值归因hint成本或宣称全进程无泄漏。原GUI内核退出及两次空观察确认，冻结源/对象/归档/native/PE/EXE/runner/reader输入均未变。

已有同组六轮的辅助描述性分析：单信号量、ready WaitAll2 manual、WaitAll2 semaphore和每线程独立semaphore的server/candidate均摊耗时比分别约6.77、9.48、6.14、3.51。不是新增门槛、每调用尾延迟或游戏FPS，不用来改写主要5.0倍/+0.6135%结果。

Mac短功能、固定性能及自己的连续压力已过，但native pending-cancel探针只覆盖显式清理后交付，不能代替真实Wine控制驱动live hint移除。补充诊断正在准备：用独立trace产物和受控Win32父/子线程、只读计数器观测，确认登记后TerminateThread、join42后计数回落，并用缺失unwind负例检验。trace不是本次小时产物，不借其结果替代64547f41…/7b0cb109…自己的验收；游戏与Windows内核、默认引擎状态不提升。
