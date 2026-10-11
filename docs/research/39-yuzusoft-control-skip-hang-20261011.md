# 柚子社Control快速跳过卡死：根因与修复（2026-10-11）

## 最新结论

《魔女的夜宴》中文版2458530/build11913959的快速跳过卡死已定位并修复。用户在新引擎下确认：“我按了足够长的时间都没有卡死，确认可用”。随后组合引擎下魔女与《千恋＊万花》1144400/build9212004均取得独立Control/画面/声音/按钮、正常退出与一次同Steam实例重开的用户验收，四次退出码0、云上传OK。具体按键时长没有测量，不填虚构数值；千恋没有旧引擎同故障现场，不能宣布两款原始根因相同。时间线与范围见 [research/40](40-steam-audio-enumeration-stall-20261011.md) 最新节。

当前原Steam瓶子使用本机组合候选`cider-cx26.3-r1-wma1-mm1-x86_64`，包含WMA清理与WinMM按需格式查询修正；原r1及WMA单补丁引擎保留，切换前缀快照可回退。正常日志已恢复。以下早期诊断段落是历史记录，最新状态以本节及research/40最新验收为准。

## 用户确认与检查点

用户要求暂缓原HY1–HY5/Windows接入队列，优先修复《魔女的夜宴》按住Control快速跳过一段时间后卡死。用户确认Mac鼠标仍能移动，卡死窗口已退出；没有现存挂起线程可取证。《千恋＊万花》同样卡死目前仅是用户推测，不能推广为两款同一根因。原队列/机时检查点见plan/13，旧Goal仍paused，heartbeat已改为只处理此故障。

## 只读基线

后续版本身份校正：用户实际中文版的Steam AppID为**2458530**（appmanifest与真实启动会话均确认），build为11913959；888790是另一发行条目，不能用它的缺失manifest判断当前游戏安装。Cider本次实际请求`-applaunch 2458530`，目标正确。

- 实际Steam瓶子绑定`cider-cx26.3-r1-x86_64`，同步未覆盖，默认msync；已有Steam客户端继续运行，未重启或复制其登录前缀启动。
- 游戏为`sanoba witch/SabbatOfTheWitch.exe`，资源版本显示TVP(KIRIKIRI) Z core 1.2.0.3；这是引擎资源版本，不冒充游戏补丁/build版本。常规导入包括WINMM/USER32，插件有音频与D3D绘制设备；这些只能作为待排查路径，不能据此宣布音频或图形根因。
- 用户既有游戏会话wine.log为0bytes。已有Steam客户端启动子进程时的标准输出可能仍关联原Steam会话；空日志不能说明无错误，也不能当作卡死线程证据。
- 旧米哈游日志中的msync pool警告不是本次游戏证据。没有将网络/认证日志、先前高CPU或别人的msync问题直接归为本次原因。

## 独立复现尝试与限制

另建全新CIDER_HOME/r1实验瓶子，不复制用户前缀。仅复用读取已安装游戏文件，指定独立datapath，并禁止实验瓶子启动Steam。Kirikiri的[原始参数/路径实现](https://github.com/krkrz/krkrz/blob/master/base/win32/SysInitImpl.cpp)支持datapath分离；运行时仍须检查实际保存位置，不能仅靠参数字符串断言所有Steam Cloud写入均被隔离。

实验prefix第一次初始化超过45秒等待限额，父CLI结束后保留其日志；随后只停止该实验瓶子并检查部分状态，没有在用户瓶子重试初始化。该插曲未认定为快速跳过的根因。

实验游戏启动时原始诊断出现`SteamAPI_IsSteamRunning() did not locate a running instance of Steam`及无法取得客户端安装位置，因此未取得可交互故事场景。没有伪造Steam接口、启动登录副本或把这次认证失败当作用户正常游玩的卡死。实验瓶子已按其精确ID/prefix停止，原Steam及米哈游实例保留。

CUA读取这个新实验游戏宿主同样返回timeoutReached；不再次重复既往Wine窗口AX/身份改造失败路线，不通过其他GUI注入工具绕过。已请用户按通常方式在原环境复现，卡住后保留进程并回复；接下来只读采集该游戏的全线程等待/调用栈，再按实际证据选择修复。

## 下一步

1. 取得仍在卡死状态的游戏PID，先采样，再读取Rosetta/Win32全线程栈与模块范围；保留等待线程，避免CPU热点过滤把死锁等待者全部丢掉。
2. 分清输入/脚本推进、音频回调/锁、msync等待与D3D读回/GPU提交；只有具体证据支持时才改引擎或配置。
3. 先完成《魔女的夜宴》故障修复与同场景快速跳过/正常推进/音频/退出再开，再单独核对《千恋＊万花》。目前未改游戏文件/存档、未发布引擎修复、未宣称故障已解决。

## 新启动阻塞与恢复

用户随后无法从Cider或Steam开始游戏，其他按钮仍能操作。真实启动会话目标正确，游戏文件存在；原Steam主实例仍为10月8日启动，当前没有游戏进程，gameprocess日志未记录本次启动。正常`-shutdown`请求也未在10秒内结束这个实例。采样/LLDB在限额内未取得栈，因此不能把请求未处理的根因定为某个锁或msync。

先在受限本机缓存备份当前游戏的Roaming及Steam云同步本地目录，再确认没有运行游戏，按精确Steam瓶子结束旧实例。只重启用户原前缀，没有启动登录副本。第一轮恢复启动遗漏SteamLibrary要求的跨进程图形环境；在尚无游戏进程时已停止该实例，并用`DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1`、原瓶子msync/zh_CN.UTF-8及实际AppID重新启动。此遗漏是诊断恢复步骤的问题，不能记为用户最初启动阻塞根因。

10月11日10:40:31（UTC+8）Steam自动登录返回OK；10:41:04起该游戏云同步完成，10:41:18新增真实游戏进程及Steam跟踪记录。启动处理恢复，画面/交互另请用户确认；重启仅是本轮恢复，不证明长期故障已修复。

本次为取证使用的install调试级别开启了seh跟踪，游戏主线程持续产生大量C++异常日志，已观察到约152MB。这会增加开销，已暂停快速跳过复现；在本机压缩保留已写诊断快照后截短自身生成的活动日志，未改游戏/存档。已请用户报告标题/可保存退出/已卡死状态，随后安全恢复普通日志级别，不能强制关闭仍有未保存进度的游戏或把详细日志造成的开销混入原始故障。

原Control卡死仍未定位。后续优先取得正常日志环境下的全线程等待证据；LLDB在附加阶段超时并提示共享缓存读取问题，关闭外部符号查找也未在15秒内完成。未因此请求通讯录权限或改系统保护。Wine内部调试器的源码可作为后续途径，但先核对其附加/退出是否可能终止目标，再用于真实挂起进程。

## 真实卡死现场与根因

用户随后保留了卡死进程，确认此前可正常操作、长按Control后无响应。Wine线程栈及有限只读媒体状态采集取得：

- 主线程`04f4`：`krmovie Stop → quartz MediaFilter_Stop → qasf dmo_wrapper_cleanup_stream → RtlEnterCriticalSection`，等待`strmbase_filter.stream_cs`。
- 锁所有者`051c`：`krmovie → qasf MemInputPin_Receive → process_output → release_output_samples → quartz BaseMemAllocator_ReleaseBuffer → ReleaseSemaphore`。
- 媒体过滤器处于paused，锁递归计数1；输出长度0，却保留`0x01000007`（INCOMPLETE及旧的时间/同步标志）。
- DMO的IUnknown表位于winegstreamer.dll的RVA`0x5cd48`；原i386 PE的符号/源信息将其定位到`wma_decoder.c`的`unknown_vtbl`，确认是WMA解码器。

`wg_transform_read_dmo()`原先只在读取成功后清理`dwStatus`。WMA输出会设置INCOMPLETE以继续排出剩余数据；下一次读取需要新输入时，共用函数返回`MF_E_TRANSFORM_NEED_MORE_INPUT`，WMA层转为成功状态`S_FALSE`，但旧INCOMPLETE仍在。qasf的排出循环继续申请/释放零长度缓冲区，并持有stream_cs，停止媒体的游戏主线程无法取得锁。

独立的WMA DMO调用复现原r1错误：没有输入、描述符携带上一轮INCOMPLETE时，返回`S_FALSE`而标志仍为`0x01000000`。这与现场状态、调用链及源码相符。微软的[ProcessOutput契约](https://learn.microsoft.com/en-us/previous-versions/ms785950(v=vs.85))明确描述输出状态和更多输入/输出的关系；[输出缓冲区契约](https://learn.microsoft.com/en-us/previous-versions/ms783394(v=vs.85))要求按有效标志解释时间字段。`S_OK`也可能表示没有输出，因此本次S_FALSE断言限定于Wine这一实现分支，不能推广为所有原生DMO的合规要求。

补丁[0007](../../engine/patches/cider/0007-winegstreamer-clear-dmo-output-status.patch)把状态清理移到读取前。成功输出仍重建原有时间、同步与INCOMPLETE标志，媒体数据和缓冲区长度的处理保持原有契约。该共用函数还用于video DMO；color/resampler的DMO输出在此快照中未实现，其MFT路径不受这处修改影响。

## 构建、检查和回退

- 新配方：[cider-cx26-wma1.json](../../engine/recipes/cider-cx26-wma1.json)。本机只增量编译两个WineGStreamer PE模块，其余引擎文件与原已安装r1逐文件哈希一致；没有分发取证时修改过的WineDbg，没有新Actions任务。
- 通过正常引擎安装和`wineboot -u`切换原瓶子，实际prefix内i386/x86_64 DLL哈希均与新产物一致。原引擎及完整前缀快照保留，不在快照中启动Steam。正常启动原Steam自动登录恢复、游戏重新产生进程。
- [WMA检查程序](../../tests/media/wma-dmo-empty-output.c)在i386/x86_64各做32次空输出检查，交替携带`0x01000000`与现场的`0x01000007`；均返回零状态标志。
- 两种架构均读取固定CX26.3源码中的Wine MF WMA压缩夹具，产生32768 bytes非零、4-byte对齐的PCM，保留成功输出的有效标志；实际描述符重复使用后，第5次调用正常耗尽，返回S_FALSE/flags0/length0。首块与Wine参考PCM的归一化差异为7%，处于既有FFmpeg版本差异容限；不是逐位一致或完整音质测评。CMD实际取得两个退出码0。
- 通用DirectShow检查已改为每轮seek、确认Run/Pause状态并交替Pause→Stop，32轮Stop均返回，最大9ms。该图可能选择其他解码路径，并有GL framebuffer警告；不计为目标WMA根因复现、视频像素正确性或游戏验收。
- 用户在普通日志下长时间Control快进确认不再卡死。另一次通过CUA点击Cider“开始游戏”，确认实际新游戏进程产生；这一进程证据不等于工具已读取游戏窗口或全部入口长期可靠性通过。

公开记录仅保存检查元数据/哈希：[yuzu-wma-dmo-20261011.json](evidence/yuzu-wma-dmo-20261011.json)。原始线程/日志及局部媒体状态在本机缓存，不上传完整会话、内存或账户资料。

取证限制与操作经验：原生ARM/x86 LLDB附加均在等待限额内未完成。为WineDbg增加了退出不杀目标的临时保护，但64→32远程break-in仍触发取证线程执行异常及默认调试器，属于后加的诊断故障；不能作为原始游戏崩溃证据，也不再对健康游戏重复此附加路线。它们在原瓶子切换时已结束。独立新prefix的COM初始化在20秒限制内未完成、强制把builtin PE当native DLL加载失败，均保留为失败实验；最终检查使用正常builtin安装路径和已初始化的原环境，没有把失败实验写成通过。

## Steam与Cider启动迟迟无窗口（仍在定位）

用户明确两条入口都发生：Steam“开始游戏”失效，Cider点击启动也没有窗口。Cider的Steam游戏入口使用同一个客户端的`-silent -applaunch 2458530`；最早故障时两个入口都没有产生游戏进程，旧Steam的正常退出请求也未处理。目标/AppID/安装文件正确，新客户端能实际启动游戏，说明共有请求处理路径值得优先调查，但尚无失效时Steam原生线程栈，不能将它归因于WMA或msync。

复查10月8日Steam会话头尾还发现strmbase stream_cs超时；该日志也承接游戏子进程输出，至少一个线程ID与当日游戏主线程对应，不能因为文件夹叫Steam就把它认作Steam主线程锁。

Cider源码另有已确认的反馈缺陷：收到spawn结果就留下“等待窗口就绪”，没有跟踪真实游戏进程，也没有等待边界。已修补为观察真正app-owned进程、120秒未观察到时给出Steam状态提示、等待期间抑制重复启动、停止/重启清理待确认请求，迟到反馈不覆盖其他提示；PID不作为窗口可见性的证据。UI构建/实际部署及对抗性审查结果继续记录；这不是Steam客户端自身失效的根因修复。

UI对抗性复核发现并已修正：丢弃旧扫描导致使用缓存PID误报重启、相同标题跨瓶子提示串扰、整瓶停止残留待确认请求、超时文字把“未观察到”写成“没有启动”。改为单个可合并扫描、操作取得开始于dispatch之后的结果；定时器独立处理期限；全局提示赋值/关闭均推进代际，异步回执只更新所属代际；pending保存瓶子身份以便整瓶停止清理，资料库移除用中性取消提示。整瓶停止的成功/失败回执也使用同一提示归属。复核者确认四项在源码中已解决。Swift debug构建、打包签名及CUA打开新主界面完成，原五项资料库保留；故障注入的超时、同标题并发和长期入口回归仍不计为运行验收。

另为后续Steam取证准备同位数i386 WineDbg，保留退出不杀目标并取消自动重启为64位。仅在无账户独立prefix内对自建Sleep进程附加、读取主线程栈、明确detach；目标随后继续输出tick至正常结束，没有重现先前跨位数异常。产物/原始输出仅在本机缓存，构建缓存源码已恢复，未替换任何引擎的WineDbg。这不是Steam失效栈或Steam根因证据。

更新Cider主界面附近发现原Steam及新游戏进程均已退出；用户随后确认两项均为其手动关闭。记录为人工退出，不作为异常退出或Steam根因证据。Cider应用源码没有退出时killAll逻辑。

当前剩余：核对共有启动入口，取得再次失效时的请求/线程证据；单独复测千恋＊万花和视频DMO有效帧、正常退出再启动。原HY1–HY5与Windows接入队列仍暂缓。
