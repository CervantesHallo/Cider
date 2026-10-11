# Steam与Cider启动无响应：音频枚举现场（2026-10-11）

## 已确认的失效路径

在用户手动退出前一轮Steam/游戏后，通过新版Cider真正点击开始游戏：客户端存在，但120秒后仍没有目标游戏进程；重复点击已被新UI抑制，超时提示如实显示未观察到运行。Steam启动日志确认实际参数为`-silent -applaunch 2458530`，更新/安装检查完成。

11:50:16原始assert dump及会话日志记录`CSteamEngine::BMainLoop`超过15秒没有事件；随后记录`fatal stalled cross-thread pipe`并退出。捕获失效状态时没有游戏进程。新的崩溃不同于此前用户手动关闭，两者按时间分别记录。

Steam当前文件为PE32+ x86_64，dump架构9（AMD64），加载steamclient64；不能沿用旧Steam通常为32位的假设。对死进程的附加请求被WineDbg自身PID检查拒绝，未附加到新复用的PID；32位查看器不能正确展开此dump。正确采用64位离线查看器。

原始assert dump没有异常流可选线程，WinDbg的非当前线程backtrace又会尝试挂起不存在的线程。因此私有工作副本添加用于选择各线程真实捕获上下文的视图元数据，原堆栈/寄存器/模块内容和原dump不改；视图的零异常码不当作真实异常。所有原始/派生dump留在本机缓存，仅记录哈希及相关调用链。

- 原assert SHA256：`a5d25b4ff226d025e50efd21fe85ea1caf588525e72787975dcfe731036710e3`，279413 bytes。
- 原fatal dump SHA256：`26c4d931735dc8ee6da0992105e8972649e0b950ce1534646b4ec7a8889ea721`，277563 bytes。
- 主线程24：`WaitForSingleObject → tier0_s64 → steamclient64 → steamui`，等待服务回复。
- `IPC:CSteamEngine`线程240：`mmdevapi client_IsFormatSupported/Unix调用 → WINMM_TestFormat → WINMM_GetSupportedFormats → InitMMDevice → EnumDevices → InitMMDevices → mixerOpen → steamclient64`。
- 依据同版本PE反汇编定位params并读取原dump实际堆栈，确认`flow=eCapture`、`share=SHARED`、PCM mono 11025Hz/8bit、blockAlign1、cbSize0，恰为WinMM格式列表首项。调用尚未返回，result字段不能当作HRESULT。

## 范围与候选原因

同步调用边界内部包括UID解析、HAL实例创建、启用输入、绑定设备、格式读写、转换器及释放。尚未取得原生Unix栈，不能只凭边界确定哪一个Apple调用在等待；也没有msync因果证据。

系统可用输入包括默认内建麦克风及一个非默认Continuity输入，输出为内建扬声器。名称/设备UID不写公开记录；dump没有完整UID，尚未把失效端点确认成Continuity设备。只读AudioObject格式属性查询在两输入端点分别约0.128/0.083ms完成，报告48kHz、单声道32bit；软件PCM及采样率转换器构建均返回成功。未创建/启用HAL实例或开始录音。TCC日志的另一PID预检不证明该Steam线程在等待权限。

原`IsFormatSupported`路径不调用AudioUnitInitialize/Start，也无已确认Wine音频回调锁循环。11025Hz并非必然不兼容48kHz：原实现使用原生速率中间格式再转换。Apple [TN2091](https://developer.apple.com/library/archive/technotes/tn2091/_index.html)描述HAL输入转换；微软[IsFormatSupported契约](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-isformatsupported)要求真正支持/最近支持格式与失败语义。

## 当前修复选择与验收

源码证实`mixerOpen`不需要waveform格式mask，却因共有初始化主动逐端点测试20种格式，进入捕获的HAL路径。采用范围明确的WinMM懒查询：metadata初始化保留设备身份/名称/通道和mapper，只有waveIn/OutGetDevCaps真正需要dwFormats时才针对请求端点按原方法查询。用局部caps副本，避免持有设备锁执行COM/HAL操作；不能依赖仅worker创建的g_devenum，也不把失败缓存成支持空集合。

独立审查还发现原设备count在map/锁完工前已可见；实施时同时处理完成发布/失败重试，避免并发使用半初始化元数据。具体源码补丁、构建和运行证据继续追加。

收益：mixer元数据调用不触发无关音频格式配置，解决当前捕获的启动依赖。代价：实际wave-capability请求承担真实查询成本，尚未消除底层HAL潜在延迟。不得以转换器成功直接替换AUHAL的格式接受判断；实际Initialize/buffer布局必须保持一致。不能借此宣称所有输入格式查询或录音已验收。

验收顺序：无账户独立prefix的mixerOpen/并发初始化/mapper与错误路径 → 新模块哈希/可回退引擎 → 原Steam冷启动与两个游戏入口 → 游戏声音/正常退出再开。Windows主力机与Actions继续不使用。

## 实施与本机结果

已实施[0008](../../engine/patches/cider/0008-winmm-lazy-format-probing.patch)：真实格式mask按需计算到局部副本；小于dwFormats字段的部分请求不探测；枚举器为当前调用独立COM对象，失败不伪装成成功空mask。InitOnce串行完成初始化，两方向metadata先暂存、失败清理可重试、完整发布后注册通知。保留无默认端点时的全部设备、mapper及A→W委托。主代理对完整补丁/接口与锁范围复核，并在macOS15 SDK增量编译两种PE架构。

[无录音检查](../../tests/audio/winmm-mixer-init.c)的旧r1-wma1单调用与4并发调用均20秒watchdog、实际退出124；新模块两种架构均实际退出0：

| 检查 | x86_64 | i386 |
|---|---|---|
| 4并发mixerOpen/getCaps/close | 每调用1324ms，count3一致 | 每调用650ms，count3一致 |
| 错误mixer ID | BADDEVICEID=2 | BADDEVICEID=2 |
| Mapper mask | 原0xffffffff | 原0xffffffff |
| 两输入端点metadata部分请求 | 均0，保留名称字段，未请求format mask | 同左 |
| 真实输出mask及open-HWAVE分支 | 0xfffff，数值/handle一致 | 同左 |

没有打开捕获流、读取麦克风缓冲区或修改音频默认设备。输出handle只打开/关闭，不写音频buffer。

本机组合引擎`cider-cx26.3-r1-wma1-mm1-x86_64`仅在已接受WMA引擎上替换32/64 WinMM PE；其余引擎文件逐项哈希一致，正常安装与wineboot更新后原prefix内DLL哈希匹配。原引擎与切换前缀快照保留。初始化/发布补丁约束及限度已复核；底层CoreAudio、msync与安全API未改。

原Steam冷启动12:26:09自动登录OK，当前原生IPC线程等待正常事件集合、主线程运行UI消息循环，未再见捕获的音频阻塞及新fatal。组合引擎的32/64 WMA有效数据/耗尽检查再次取得实际退出0；独立冷prefix WMA的COM起步仍触发watchdog，不作通过记录。

然而当前仍未观察到目标游戏进程，云同步已完成，Cider提示等待结果未确认。Win窗口自动化继续timeout，已请用户在Steam内实际点开始并提供确认/启动选项/云同步提示状态。此剩余入口/UI证据不能被正常登录、mixer fixture或“线程看起来空闲”替代；不把两入口全部修好写成完成。

原始日志/内存私有，公开仅[元数据与哈希](evidence/steam-winmm-20261011.json)。此记录是具体阻塞路径的修正/检查范围，非全音频接口、所有Steam版本或长期可靠性认证。

## 用户回场后的真实入口复测（13:10起，UTC+8）

新增日志明确了修复后的剩余等待：12:26:53请求停在`KickingOtherSession`，等待用户处理另一设备的游戏会话；13:10:32手动启动请求又停在`ShowInterstitials`确认。它们是Steam等待用户响应的阶段，不是此前音频枚举阻塞的证据。没有自动确认踢出其他设备，也没有用直接启动游戏绕过Steam确认。

用户先后回复“可以启动”“对的”，报告Steam入口可进入游戏并操作、有声音；该次游戏生命周期未由本机扫描独立捕获，保留用户报告与机器观察的区别。

随后通过CUA实际点击Cider“开始游戏”：13:11:57 Steam收到新的`-silent -applaunch 2458530`请求，13:12:06确认继续，13:12:07创建游戏（Windows PID1696，本机PID489），13:12:08启动任务Completed。Cider AX界面从“正在启动”变为“魔女的夜宴 已开始运行。”，资料库与继续游玩均显示“运行中”。仍为同一个Steam实例（本机PID97843），没有重启客户端或改变引擎/同步配置。

这是Cider入口、Steam实际创建游戏及进程反馈的对应证据；不能仅凭Completed/PID宣称看见游戏窗口。用户随后明确回复“正常，已退出”，确认这一轮画面/按钮/声音、Control快进松开后操作及菜单退出均正常。Steam在13:36:25记录原游戏PID1696退出码0，13:36:28记录云存档上传OK；本机游戏进程消失、Cider恢复“可运行/开始游戏”，Steam PID97843保持。可测量的进程生命周期为24分18秒，Control实际按住时长未测量，不把它写成24分钟快进。

再通过CUA点击Cider启动：13:37:02请求，13:37:09创建新游戏（Windows PID2064，本机PID1967），13:37:10 Completed；请求到创建进程约7秒，期间13:37:05–09仍有正常ShowInterstitials确认。Cider再次显示“已开始运行/运行中”。用户回复“重开正常，已退出”；13:39:29游戏退出码0，13:39:38云上传OK，本机游戏消失、Steam原实例仍在。这一轮同实例重开的实际窗口/操作和退出已通过；不重启客户端掩盖间歇性失效，不在用户未知未保存状态强杀游戏。

当前魔女的组合引擎画面/声音/Control/正常退出已取得用户回执和对应退出记录。长期稳定性、冷启动重复轮次、千恋＊万花及其他柚子社游戏独立验收仍不由此覆盖。

## 千恋＊万花独立检查的起步

安装的Steam App1144400/build9212004，SenrenBanka.exe为PE32/i386，使用同一原Steam瓶子和组合引擎。魔女完全退出、云上传OK后，CUA打开千恋详情并点“开始游戏”：13:39:56请求→13:40:05真实游戏创建（Windows PID2296、本机PID2141）→13:40:06 Completed，Cider详情与提示均显示运行中。Steam宿主仍97843。

用户回复“千恋正常，已退出”，确认剧情Control松开后画面/声音/按钮可用及正常菜单退出；13:42:25游戏PID2296退出码0，13:42:29云上传OK。进程生命周期2分20秒，不等于已测量的Control按住时长。

再通过Cider启动：13:43:04请求→13:43:08创建新游戏（Windows PID2708、本机PID2333）→13:43:11 Completed。Cider详情/提示显示运行中，用户确认“正常，已退出”；13:43:57游戏退出码0，13:44:01云上传OK，本机两款游戏均不在运行，Steam仍为原PID97843。

本轮两款分别完成Control/交互、两次正常退出及一次同客户端暖重开，全部四次退出码0，云上传有成功记录。使用本机组合引擎、原登录前缀与msync，没有为重开重启Steam、录音、改默认音频设备、启动登录副本或消耗Windows主力机/Actions。

不把魔女WMA机制直接作为千恋原本也存在同故障的证明，也不把四次成功扩展为全部柚子社、所有视频帧、长时间稳定性或20次冷启动门禁。下阶段仍按当前柚子社队列推进视频DMO有效帧与可靠性；已暂缓的HY/Windows队列保留检查点。

交付：两款兼容数据已记录组合引擎与这次验收日期，保留playable-caveats及长期/视频限制；debug App打包签名成功，源数据与bundle数据SHA256一致。CUA只重开Cider主界面，确认两款详情展示新说明/日期/引擎、五项资料库保留；Steam97843与HYP49797保持、无游戏进程，留在资料库。现有每日heartbeat更新到本次完整回执与视频/可靠性检查点，ACTIVE、09:00及原线程核对；未创建重复任务或恢复暂停Goal。
