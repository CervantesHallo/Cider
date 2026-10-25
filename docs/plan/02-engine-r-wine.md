# 02 Engine R：自建 Wine 引擎、补丁队列与性能

最新[101](../research/101-r-v1-real-close-inflight-20261026.md)：真实NtClose保护失败/同句柄成功重试双向各28/0；最终在途对照修后保留PENDING，去掉保护条件则在有效句柄仍在途时过早SUCCESS。四客户端28/0且最终关闭，负例依据是不变量而非泄漏。严格时钟/独立源审/raw组件清理绑定完成，已归档清理15整树。下一步先补Cocoa实际退役完成栅栏，再做producer资源/撤销状态、事件reparent及两三进程像素；当前该栅栏是提案，foreign/默认/游戏未启用。


最新[100](../research/100-r-v1-process-lease-faults-20261025.md)：0021一次STOP丢交付和STOP后真实worker终止已做双向修后/精确错误分支对照，分别68/0对68/1及28/0对28/2；失败观察补证实际PE所有者遗留，源审与独立运行绑定完成。已清理4引擎/4stage/11重复备份并保留精确差异。继续失败NtClose/在途完成实际对照，再到真实CAContext资源/事件/reparent及两三进程呈现；foreign路由、默认及游戏评级未启用。


最新[99](../research/99-r-v1-typed-producer-leases-20261025.md)：0021类型化消息/真实进程租约及PE恢复实现已精确重建、成组链接、NDEBUG编译和限定复审；最终双向跨位宽各68项、实际退出/主动释放/高32位代次拒绝通过，原窗口受影响双PE各1415/56交换链/168Present通过并有独立log/组件/清理恢复绑定。0020旧PE32清理缺口另已定向补齐，旧failed保持。下一步失败清理/worker死亡真实对照→CAContext资源及事件/reparent→两三进程呈现；foreign路由仍未启用，故障/屏幕/CEF/游戏不计完成，不切默认。


95窗口生命周期后续双PE各1415项/56交换链/168Present通过，资源释放两顺序/resize尺寸与变色读回已覆盖；夹具两P2修后实际重验，原1247子集不提升。继续foreign-child/CEF，完整屏幕/精确竞态/原生与游戏门槛保留，详见[95文末](../research/95-r-v1-legacy-window-bridge-20261025.md)。

回调真实运行接续[96](../research/96-r-v1-registry-roundtrip-20261025.md)：0018保留真实安全属性/继承和错误，修尾部分隔符叶键；双PE回调各33项、直接A/W及视图各195项通过，故障所有权/精确负例/错误变体拒绝已实际闭合。公开源码与运行输入精确绑定，私有诊断不作为普通采用；下一项仍是窗口释放重建与foreign-child/CEF。完整R v1、原生Windows/游戏和U8/15缺口不提升。

完整legacy接口接续[95](../research/95-r-v1-legacy-window-bridge-20261025.md)：24槽192B表/120B快照及成组驱动已链接；原M-A真实双PE各120 Present零失败。3项限定源审P2修后闭合；registry v2真实往返/故障所有权、窗口生命周期/视觉与CEF跨进程继续。源码/链接/局部运行与完整桥接采用分开，不借初次呈现替代这些门槛，原U8/15不变。

GPU销毁依赖最新见[94](../research/94-r-v1-gpu-teardown-20261025.md)：TLS/allocator双PE最小新旧对照完成；R2集成失败保持，0014修rtworkq payload/callback释放依赖后R3原GPU各575项及offscreen各46通过，原生异常严格拒绝。保留原图形/队列/上传后备，无测试pin或sleep。限定组件运行不提升为完整MF/CRT/R v1/游戏；接续完整legacy/foreign-child/CEF窗口，U8/15原失败不变。

GPU/窗口最新接续见[93](../research/93-r-v1-gpu-shared-and-window-foundation-20261025.md)：固定Highball7421共享/keyed/所有权边界双PE各46项通过；MF真实GPU样本PE64四case575项过，PE32 native异常使整批仍未通过。已定位DXMT设备成员初始化顺序，保留管线的两文件候选正成组重建，不默认采用。0011窗口设备/分配基础经修复复审、编译链接及重建通过，完整legacy ABI/foreign-child/CEF运行仍待。原U8/15、HY顺序和Windows待重启条件保持。

MF主题接续（2026-10-25）见[92](../research/92-r-v1-mf-allocator-20261025.md)：0010已按新base语义迁移，保留原上传/queue生命周期，补有限池完成/输出pending/EOS及processor真实Flush。双PE各300 SourceReader和各63真实CPU帧检查通过；最后异步reservation反例用同故障DIAG双PE各99检查闭合，原负例与失败保留。源码/组件与三文件重建已核，候选不默认采用，GPU/共享纹理和完整MF仍待。此工作属于原媒体迁移，U8/15不变，原HY顺序及Windows待重启条件保持。


媒体主题实际结果与产品准备接入见[91](../research/91-r-v1-media-preparation-20261024.md)：原冷入口20s失败保留；显式准备后WMA双PE各32空输出/32768PCM/耗尽/参考metric7/refs通过。托管媒体包授权首次执行时在瓶子锁外准备，导入仍不执行；缓存逐文件校验、Stop实际回收与发布后备份均已实现。原生正负验证及5项底层检查、真GUI/XPC创建/正常运行/两种准备期Stop通过，全部私有清理与四摘要确认；源码限定复审无具体P1/P2，App/Commands构建过。非视频/完整MF/首次全冷游戏或默认采用证明。接续原MF allocator/视频及CEF主题，U8/15仍未过，Windows仍busy/不操作。

## 当前Mac接续（2026-10-24）

F27/0123精确64547f41…/7b0cb109…已过自己的13阶段/11份身份、原AC固定六轮5.0倍/CS+0.6135%、自己的连续3600秒78检查0失败与资源核对（3567有效快照账平、peak36/fallback0）。它仍为非默认，完整M1与游戏/Windows状态不提升；补充真实Wine live hint双位宽各64轮回收已过；扩展U/N/S正确性已通过。完整U组固定90次比较运行正常，但8/15单元超±3%，归因未通过、不默认采用；继续核对计量来源，旧失败保持，见83–85与路线图顶部。后续继续原HY顺序，Windows仍忙/不能重启且未操作。

独立主题准备继续：原命令行追加/自动渲染器选择已迁移并双PE组件编译、精确重建及限定源审通过，尚未链接/运行/采用；用户名上游路径已存在，旧槽镜像与新整体GSBASE方案仍按跨界核对。见[86](../research/86-r-v1-profile-renderer-source-20261024.md)。这不解除U性能归因和完整M1门禁，也不改变原HY顺序。

随后实际参数双位宽各23检查、未迁移实验基线五项语义负例，以及D3D9新双位宽/旧实验基线各16检查和4096像素读回均通过；普通CLI失败下的自有进程清理也已实测。范围与初版故障见86，不上升为完整图形、ABI或游戏验收。接续原GS intrinsic/Fiber/异常/回调与媒体主题，U归因门槛保留。

GS/FS限定实际回归见[87](../research/87-r-v1-gs-boundary-20261024.md)：两位宽各108检查通过，错误读数双负例拒绝且清理确认，限定独立源审无具体P1/P2。上游整体GSBASE保留；原v0槽镜像补丁不改、不叠进新布局。所有异步信号/宿主QoS仍未覆盖，完整M1/U±3%及游戏状态不提升；接续原媒体主题与性能归因。

WinMM/MMDevice的原媒体迁移见[88](../research/88-r-v1-winmm-metadata-20261024.md)：新base生命周期/就绪/有效角色依赖已修，四次cold双位宽各39检查过，私有候选保留、格式策略与全部通知并发不计完成。共享runner实际SIGINT清理过。U已有90回执离线归因见[89](../research/89-u-cohort-offline-attribution-20261024.md)，8/15仍超门槛，不能把变化自动归因为噪声；下一步先验证线程计量来源，原正式门槛不改。

线程计量前置[90](../research/90-darwin-thread-cpu-source-20261024.md)已有两架构真实观察与进程CPU负例，Rosetta CLOCK_THREAD量级失败原件保留。libproc15与显式sec/usec子检查不提升为整组通过；需kernel64身份、正确Wine宿主边界后才能用于新诊断。旧90队列未使用该clock，其8/15结论保持。下一媒体受影响运行复用已修DMO的实际组件，避免重复构建。

## 最新状态（2026-10-24）

用户已恢复推进。F24/0120自己的13阶段/11份身份通过，但AC六轮4.434782609倍/CS+11.0273327%失败，不采用、不跑自己的小时。真实覆盖诊断中ping的22052次尝试只148次成功；组合门回退11927次、事件未就绪9972次，不能把组合项都归因于owner竞争。始终就绪控制的10000次获取全命中。当前先基于该证据研究有界owner重试，不变更获取/恢复/取消保护，不把插桩比例当未插桩性能证明。完整M1仍未通过；Windows仍busy/零操作，原HY顺序保持。细节见79与路线图顶部。

## 先前状态（2026-10-23）

完整M1仍未通过。F20原生命周期第46轮5000ms就绪超时不含明显宿主时钟间断，仍未定位；补跑未执行阶段和扩大诊断不覆盖原失败。所有模式现拒绝宿主时钟间断；原失败、超限日志与独立队列均保留，详见13顶部/77。F21短功能通过但正式4.50倍/CS+41.1765%均失败。随后真实guard诊断及F22普通事件保守提示候选已完成；F22短功能、两位宽单事件生命周期通过，固定六轮3.607142857倍/CS−9.209572%均失败，不采用/不借小时。取消策略和5倍/CS±3%不改，详见78。Windows仍busy/零操作。

## 先前状态（2026-10-21）

详见13顶部/77。原M1尚未通过：后续源码/编译/短功能通过与性能失败分开记录，所有候选非默认。F17/F18-v3与F17/F19-v4各12次已完成，均未过5倍/CS±3%；不借F7/F8小时。当前F20只复用相同native线程数据，保持0115原O2事务/恢复，实际Wine验证进行中。Windows仍busy，其他原HY阶段状态不提升。

## 2026-10-20 接续

当前源码和Mac运行状态见[路线图顶部](13-roadmap.md)与[77](../research/77-waitall-mac-runtime-20261018.md)。F7/F8各自小时与功能通过；十份完整电池性能队列均未达5倍，全部保留、不默认采用。F15槽缓存控制通过但只有4.5455倍；继续原F8的F16 native frame线程数据复用，11阶段功能通过但正式性能4.3043倍/控制失败；F17只组合0107/0111/0112机械优化，11阶段功能及pair通过；低背景探针未过，新条件性能/小时未开始。Windows仍busy/暂不能重启。下节为历史检查点，完整M1尚未通过。

## 2026-10-18 WaitAll Mac运行验收（进行中）

最新：电池供电可以做正式性能，采用独立预声明同条件队列，不拼接旧AC13/15。F8 v3三对完成，单事件4.5909倍未达5、CS+2.1019%通过控制；机械TLS候选F9三对4.5909倍、CS−5.0895%，新v4只读stop夹具F8三对4.6818倍、CS+10.4962%，后两者控制失败，全部保留。旧AC13/15及v2归档原字节未变，自身续测仍须同条件，不作为阻止电池工作的理由。取舍：增加独立三对机时换取不依赖接电的可比较证据；电源切换/睡眠则该轮拒绝，门槛不变。F7/F8各一小时和F8剩余功能均已过，不重跑；F9只有短功能，不能借小时。只读未就绪候选F10实际11阶段短功能通过，但三对性能仍4.5909倍（控制+1.6209%通过），无主要收益，保持非默认/无自己的小时。现继续更小的0104分支：复用函数内native数据、优化本线程控制计数，原事务不改；独立信号/生命周期审查及11阶段短功能已过，固定三对比较4.5909倍且控制−14.4611%未过。候选不采用，转为单独的原F8分段诊断，先取得热点证据，诊断不计性能验收。通知分支0108 revision2也已完成native guard及11阶段Wine短功能，但固定三对只有2.5897倍（控制−0.1002%通过），实际更慢，不采用。接续从原F8审查提交者直接finish和带版本的phase原子确认，减少重复取门；目前仅设计，未修改/运行/采用。性能验收和完整M1仍未通过。

本轮按用户授权在独立GUI/XPC、私有CIDER_HOME中执行完整Wine11.18。0102的OFD正常路径实测性能不足，0103保留原槽/Mach/ulock/inproc、整体提交及死亡恢复，改为原子准入和内核租约证明，修复真实线程终止时的fd_cache锁、调用pins和等待节点回收；正常状态路径无fcntl。最终变更9文件精确重建、匹配native pair和双PE组件构建及限定复审完成。

PE32/PE32+基础各825检查、生命周期各64轮/5780检查、八线程100000次/worker竞争通过。45秒预检许可平衡，真实泵约150万次回收保持有界。F7第三次连续小时压力已通过：78检查零失败，guest3600019ms、宿主活动3607.786s，无检测到的睡眠，约6.54亿次获取/归还相等；真实节点峰值36/fallback0。两位宽四子进程竞争也通过，原版同用例亦通过，不声称复现旧版失败。前两轮822秒/249.961秒合盖中断分别保留，不拼接为通过。固定v2三轮单事件signal→peer-return p50候选2.2µs/server10.4µs/旧非原子1.3µs，RTT4.4/20.8/2.3µs，均4.727倍未达5；私有CS控制+24.08%未达±3归因要求，全部原始轮次保留，v1的4.79倍失败不覆盖。另导出非默认0104本地准入候选：保留全局事务/死亡证明/提交/通知，SC每线程发布与fork扫描减少共享计数开销；构建、两位宽基础/64轮生命周期及短压通过，在独立bundle id/空瓶运行另一小时，原F7不替换。0104连续小时也已通过；v3保持门槛固定延长控制，因用户5分钟后合盖仅完成13/15轮，剩余两轮同协议续测，不能判完整性能验收。两根实验进程已清零/桥接卸载，私有候选未采用；F7追加PE32八线程竞争已通过。取舍及证据边界见[77](../research/77-waitall-mac-runtime-20261018.md)。

候选非默认，生产EnginePolicy和配方未采用，用户App/瓶子及Windows未改。先闭合当前冻结构建的连续压力/预定义性能结果，不将组件通过上升为完整M1、游戏Verdict或Windows kernel通过；原HY顺序与目标保留。

## 2026-10-18 WaitAll协调源码修复（上一阶段）

按用户“Windows暂不能重启，先修WaitAll”推进原02。非默认0102移除逐对象暂取/回滚，保留16字节共享槽/Mach/ulock/inproc接口，统一等待、释放/查询、事件、server信号/abandon和槽复用；加入可恢复提交、线程代际、退出及fork隔离。协议963→964由当前工具生成，15个变更文件/完整候选链20文件精确重建；server/native及双PE ntdll最终构建0错误0警告，限定源码复审未再发现P1/P2。**源码修复完成，不等于运行、Windows和性能验收完成。**正常状态操作新增2次fcntl及局部准入，状态更新暂时串行，另有128MiB+64KiB稀疏映射，实际RSS/延迟未测。候选保持未选中，不切用户引擎/瓶子或提升游戏裁定。收益、代价、验收及回退见[research76](../research/76-r-v1-waitall-coordination-20261018.md)。原HY顺序/目标保留；Windows busy、准备2309/测试3600/无pending未变，未新增或运行测试、未触发Actions。

接续平台构建也已完成：Wine启动器、wow64/wow64cpu、win32u和winemac.drv共9产物链接。新增范围30条未修改基线winemac源码警告单独保留，未运行或扩成完整引擎通过，详见76补充。

## 2026-10-18 同步源候选（历史，续修见76）

R v1独立候选0100/0101已接macOS msync及same-binary host loader，协议由962生成963（非CX1810），16文件精确重建；server/native ntdll/两PE模块已编译链接，未执行/安装。源修包括真实初始化/映射/分配失败、稳定原子页表、共享ref与alert索引、WaitAll abandoned/owner回滚/期限和登记清理。独立审查仍确认旧WaitAll暂取与并发Release会使semaphore count>max的P2，**未修、阻止默认采用**；不能钳位/丢许可/禁用WaitAll冒充修复。需统一状态协调及线程死亡恢复，原M3为候选机制，不计M1/M3完成。稳定表2/8MiB每原生进程代价、验收及回退见research75/evidence；无Windows/新测试或游戏门禁提升。用户暂不能重启，Windows busy/余准备2309测试3600无pending保持。

> v1 · 2026-09-27 · 依赖 00（ADR-002/004/005/006/007）、研究 02/12/17/18/20。只写现在要做和马上要做的；后续按窗口卡补充。

## 目标

1. **自己构建 Engine R**：不再依赖别人的二进制引擎。构建要可复现：固定输入 + sha256，本机与 CI 使用同一套脚本。
2. **把现在能用的组合（Highball 引擎 + DXMT）变成自己的引擎**：Steam、千恋＊万花都已在上面跑通。
3. **在自己的引擎上解决性能与身份问题**：这类问题只能改引擎才能解决（见“性能工作项”）。

## 当前状态（2026-10-18）

- **Engine R v0 已自建并在用**：本机 Steam 的实际进程使用 `cider-cx26.3-r1-wma1-mm1-x86_64`。已安装 manifest 记录 CX26.3/Wine11.0 + Highball0001–0013 + Cider0001/0002/0007/0008 的本地构建来源。工具链、源码哈希、构建脚本和可重复配方均在本仓库；原路线图E0/E1已有构建、冒烟及游戏证据。
- **引擎层研发已持续进行**：窗口顶部/Dock、同步、媒体、WinMM冷初始化及对象生命周期/主令牌引用分别有源码和证据。mm2音频配方及K1–K3完整修复源码本次全部恢复；K2的Windows原生对照仍待，候选kernel补丁保持非默认，不因源码恢复切换用户瓶子。
- **R v1尚未宣称完成**：按原计划在v0 Steam跑通后进入主题化迁移。当前开始整理实际配方、补丁依赖与复用证据，再固定新Wine输入与主题逐一迁移；不会把旧CX树重命名当作v1。下表的wine-11.18是原计划基线，实际采用前须核对来源/哈希，后续Wine12目标同样是计划门槛。
- **v1首步已执行**：`engine/porting/r-v1-inputs.json`固定wine-11.18 tag及实际commit `7b3fff76fa5178f6ce0141b2c776afa2a822f101`、源码SHA256和现有5份CX配方的21补丁主题/依赖。preflight、D3D9 COPY评分、组件覆盖路径及DMO状态清理共四项已迁移，zero fuzz/offset比对通过；其他主题仍按依赖推进。四项受影响组件和两架构Wine C++/C++ABI归档已编译，但未构建完整新v1或执行它，不替换现有引擎。
- **Engine A 是后续阶段**：00/ADR-003和P3原排期为2027年6–8月，在此前做probe/FEXUnixLib/A-lite，依赖arm64平台支持、entitlement和实际探测结果。当前主线仍为自建R，以柚子社全系及三款官方国服Windows本地兼容为目标。
- **接续顺序**：完整源码恢复及主线构建 → HY1剩余真实交互/入口收尾 → HY2原生契约与依赖组 → HY3许可闭环 → HY4/HY5游戏会话。独立可做的R主题整理和P-3性能研究随原02推进；Windows机时受既有账本限制，恢复源码不追加额度。

### 2026-09-27 初始状态（历史记录）

- 在用：`highball-11.0-0011b-dxmthb-x86_64`，即 Highball 0011b（CX 26.3 源码 + Highball 0001–0011）+ Highball 版 DXMT。这是别人的二进制，被剥离过，依赖靠 APFS 克隆的 Gcenx dylib 补齐（`libraryPaths: [frameworks]`）。
- Steam 在 msync 下工作正常，已改为瓶子级默认（`SyncMode`）。
- 限制：
  - 只有会话里第一个进程带 CiderWineHost 身份，子进程直接 exec 真实 loader；
  - 引擎没有调试符号，Rosetta 下无法 profile。

## 基线与版本线

| 线 | 源码 | 用途 | 时间 |
|---|---|---|---|
| **v0 `cider-cx26`** | `crossover-sources-26.3.0.tar.gz`（Wine 11.0 底座，sha256 固定）+ 挑选的 Highball 补丁 + Cider 补丁 | 第一个自建引擎，替换当前的 Highball 二进制；同时充当 00 里的 CX oracle | 现在 |
| v1 `cider/devel` | wine-11.18 + CX 26.3 补丁按主题拆分（ADR-004） | 1.0 主线，逐 tag rebase，12.0 定为 1.0 基线 | v0 跑通 Steam 后开始 |

v0 先行的理由：它与现在跑通 Steam 的组合在源码上等价，差异可控。先把构建管线、布局、签名、冒烟跑通，v1 的 rebase 只需替换源码输入。

### v0 补丁选择（来自 highball-engine/patches）

| 补丁 | 取舍 | 理由 |
|---|---|---|
| 0001 use-the-real-user-name | 取 | 瓶子用户目录 = macOS 用户名，与现有瓶子一致 |
| 0002 wined3d auto renderer opengl-first | 取 | macOS 上 wined3d-vk 在 32 位程序下崩溃（千恋＊万花实测：MoltenVK 外部内存 buffer 报错） |
| 0003 ntdll WINEDLLPATH prepend | 取 | 让组件覆盖目录优先于引擎 builtin，DXMT/D3DMetal 不再需要克隆引擎变体 |
| 0005 yield after auto event set | 取 | Highball 针对启动器卡顿的修复 |
| 0006 kernelbase per-exe CommandLineAppend | 取 | profile 的 argv 追加动作（ADR-009 `cider-rules` 的一部分）先用它实现 |
| 0007 winemac 跨进程子窗口 swapchain（C1） | 取 | Steam CEF 能渲染的根本原因 |
| 0008/0009 TEB FiberData/QoS 槽位 | 取 | 0011b 带着它们时 Steam 已验证可用；逐个复核后再决定是否上游化 |
| 0010 mfreadwrite video processor | 取 | 视频播放（开场动画）相关 |
| 0011 LastError 放进 gs 槽 | 取 | 0011b 已含 |
| 0012 x87sidecar | 已在实际 v0 配方编译，默认不启用 | 实验入口须显式环境配置；未验收、不能作为米哈游适配依据 |
| 0013 NX compat under Rosetta | 已在实际 v0 配方编译 | 保留单独验收要求；配方状态与最初提案分别记录 |

Cider 自己的补丁（主题化，尾注 `Cider-Topic:`）：

- `r3/preflight`（`cider/0001`，已写）：`NtCreateUserProcess` 拒绝 `CIDER_PREFLIGHT_DENY` 列出的映像，返回 `STATUS_ACCESS_DISABLED_BY_POLICY_DEFAULT`（与 Windows AppLocker/SRP 拦截一致，Win32 错误 1260）。瓶子里的启动器也无法拉起被门控的游戏。

- `macos-ux/host-identity`：所有 Windows 进程都从 CiderWineHost.app 的 loader 启动（修改 loader 的自我 exec 路径与 `WINELOADER` 处理）。目标是 Dock、通知和 TCC 只显示一个身份。
- `cjk/systemlink`：Tahoma / MS Shell Dlg 的 SystemLink 默认回退到 macOS 的 CJK 字体。修掉 Steam 引导程序对话框里中文不显示的问题。
- `perf/*`：见下文。

## 工具链（本机与 CI 相同）

**原则**（研究 12）：
- 宿主工具用 arm64 原生；
- x86_64 依赖用 Xcode clang `-arch x86_64` 交叉编译，不用 x86 Homebrew；
- Wine 本体用 `arch -x86_64 configure/make`，这是上游验证过的做法。这棵源码树的 unix 侧只能构建 x86_64（`d3dmetal_objc.h` 位于 `__x86_64__` 宏之下）。

| 组件 | 来源 | 备注 |
|---|---|---|
| bison ≥3、pkgconf、ccache | 源码构建（arm64） | 装到 `~/Library/Caches/Cider/toolchain/host` |
| mingw-w64 gcc（i686 + x86_64） | binutils + gcc + mingw-w64 源码自建 | llvm-mingw 编出的 kernelbase 会让 Steam 登录卡住（研究 17）；llvm-mingw 只留给 Engine A 的 arm64ec |
| freetype、gnutls（gmp/nettle/libtasn1/libunistring）、SDL2、libpng | 源码，`-arch x86_64` | 装进 `toolchain/x86_64`；进引擎的 dylib 一律改成 `@rpath` |
| MoltenVK | 固定到带 shadow-import 的 1.4.2 之前（ADR-006），官方 release | universal |
| GStreamer | 1.28 官方 framework（universal） | 引擎内置所需插件（ADR-012 之后），暂时沿用系统 framework |
| Wine Mono / Gecko | dl.winehq.org，按 `addons.c` 里的版本号 | 免去首次启动时的下载弹窗 |

**SDK 必须与部署目标匹配**（2026-09-27 实测）：用 Xcode 27 的 macOS 27 SDK 构建时，configure 检测到了 `pipe2`（macOS 27 才有）。它被弱链接，在 macOS 26 上解析为空指针，`wineboot` 一启动就在 pc=0 崩溃。因此依赖和 Wine 一律用 Command Line Tools 里的 MacOSX15.x SDK（`CIDER_SDK` 可覆盖），部署目标 14.0。CI 的 macos-26 runner 同样要显式指定。

2026-10-18：v1配置实测发现两架构PE C++17前端缺失。已准备`engine/pe-cxx-toolchain.sh`，以同一固定GCC15.2归档、新prefix/新对象补C++前端，原C工具链保持；双架构前端已构建、Wine C++17检测通过，已实际构建Wine自带C++/C++ABI；尚未接入默认引擎配方。取舍、来源及核验边界见`engine/porting/README.md`。

构建入口：`engine/build.sh <recipe>`。recipe 是 `engine/recipes/<id>.json`，包含源码 URL + sha256、补丁列表、configure 参数、组件。产物是 `out/engines/<id>/`，外加 `manifest.json`（Ed25519 签名在 CI 做）和 tarball。本机用它做冷构建（8 GB 可行，但慢）和单 DLL 增量构建；等有了 GitHub 仓库，由 CI 接手冷构建。

## 引擎布局

```
Engines/<id>/
  manifest.json            # EngineManifest；libraryPaths 只指向引擎内部
  engine/{bin,lib,share}   # wine、wineserver、lib/wine/{i386,x86_64}-{windows,unix}
  frameworks/              # 引擎自带的 dylib（@rpath），不再从别的引擎克隆
  CiderWineHost.app        # 由 EngineHost 生成；host-identity 补丁让所有进程都走这里
```

DXMT 作为组件（ADR-006），不再克隆整个引擎：0003 补丁生效后，组件目录放在 `WINEDLLPATH` 前部即可。`winemetal.dll` 的占位按 highball 的做法放进引擎 builtin。

## 性能工作项（来自 2026-09-27 的卡顿问题）

实测背景：M3 / 8 GB，Steam 下载 千恋＊万花 时出现按键延迟和加载卡顿。

| # | 问题 | 现状与证据 | 做法 | 验收 |
|---|---|---|---|---|
| P-1 | Steam UI 卡顿 | 旧配置对 Steam 关闭 msync，所有同步都往返 wineserver；开启 msync 后用户确认不卡 | 已改为瓶子级 msync 默认 ✔。补 ADR-005 的门禁：msync 下 Steam UI 冷启动 20 次零看门狗 | 脚本化 20 次冷启动，截图非纯色，webhelper 无挂起 |
| P-2 | 下载时 steam.exe 占 119% CPU | 解压与校验在 Rosetta 下进行；3.9 GB swap | 自建引擎后先有符号再 profile；检查 bcrypt/crypt32 路径（gnutls 版本、哈希实现） | 相同下载速率下 CPU 降低 ≥30%，或给出不可降的证据 |
| P-3 | Kirikiri（千恋＊万花）标题画面主线程约 90% | 2026-09-27 用带符号的 v0 引擎 + `scripts/wine-profile.py`（lldb）定位：① 全屏黑边导致每帧“部分呈现”，走 wined3d 的 GDI 回退（`glGetTexImage` + `StretchBlt`）→ `cider/0002` 优先选 COPY 交换格式，已消除；② 游戏每帧 `GetRenderTargetData` + `LockRect` 读回渲染目标（游戏本身的设计），Apple 的 GL 在 CPU 上还原分块布局并同步等 GPU。PBO 读回可以去掉 CPU 还原，但总 CPU 没有下降，已撤回；③ 0013（NX）让辅助线程从约 13% 降到约 6%。总体从 120–150% 降到 100–115% | 下一步：GetRenderTargetData 的读回改为异步或走共享内存纹理（Apple 的 client storage / `GL_STORAGE_SHARED_APPLE`）；把 Kirikiri 自身合成的开销单独量出来（Rosetta 下的 SSE 路径） | 标题画面主线程 ≤40%，画面无回归（截图 SSIM） |
| P-4 | 8 GB 内存压力 | Steam CEF ≈1.5 GB，游戏 ≈1 GB | Cider 启动游戏时 Steam 带 `-silent` ✔；在 UI 提示 Steam 设置（库为首页、低性能模式、关特惠弹窗）；研究 CEF 在隐藏窗口时的节流（winemac 遮挡通知 → `WM_SHOWWINDOW`/occlusion） | Steam 窗口隐藏时 webhelper CPU ≈0，内存不再增长 |
| P-5 | 代理 TUN 的额外开销 | mihomo gVisor 栈在下载时占 34% | 不属于引擎问题：在环境检查里提示 TUN 协议栈可选 mixed/system；Steam CDN 直连规则写进 Cider 的“网络建议”文档 | — |
| P-6 | Rosetta 首次运行卡顿 | AOT 翻译缓存（`/var/db/oah`）在首次运行时生成 | 安装程序或首次运行时预热常用 PE/unix 模块（只读访问触发翻译），与引擎安装一起做 | 第二次启动与第一次的差距 ≤20% |

性能工作使用**带符号的自建引擎**，该构建前提已在v0具备；P-2/P-3余项是进一步分析和修复，不能继续记为“等待自建引擎”。发布版本剥离符号，另存一份 dSYM 与 PE `.debug` 包供诊断下载。

## 验收（v0）

1. 用 `engine/build.sh cider-cx26` 从零构建成功，产物里没有 `/usr/local`、`/opt/homebrew` 引用（`otool -L` 与 `DYLD_PRINT_LIBRARIES` 检查）。
2. 冒烟：`wineboot --init`、`cmd /c ver`、`syswow64\cmd /c ver`。
3. Steam 瓶子切换到 v0：登录记忆仍在；商店页渲染；千恋＊万花能启动并显示标题画面。
4. 所有 Windows 进程都在 CiderWineHost 身份下（`ps` + `lsappinfo` 检查）。
5. 旧的 Highball 引擎保留为回退，`switchEngine` 前自动快照。
