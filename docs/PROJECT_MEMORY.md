# Cider 项目持久记忆

更新：2026-10-11。用于后续接管时恢复关键事实；最新用户决定优先，实施状态以 `docs/plan/13-roadmap.md` 和实际源码/记录为准。不要把提案、编译通过和真实会话验收混在一起。

## 用户目标与协作

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
