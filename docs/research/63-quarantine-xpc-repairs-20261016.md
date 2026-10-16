# 隔离续修：保留命令能力，接回原定GUI执行架构

2026-10-16。用户要求尽可能坚持原有代码、性能与效果优先，不能把撤掉功能算作修复。沿research60–62继续处理ADR-007旧债；所有实现仍在`codex/quarantine-fixes-20261014`，没有整包恢复主线。

## 保留的实现及变化

原`ciderctl`的29个命令声明移入`Tools/CiderCommands`，沿用BottleStore、RecipeInstaller、WineRunner等业务实现。GUI在工作线程调用它；CLI只解析本地帮助/版本/错误，然后通过认证XPC提交命令。参数、调用者工作目录、显式环境、stdout/stderr、进度、退出码及`--wait`均保留。使用每次调用的上下文，不改GUI进程的cwd、环境或全局输出文件描述符。

链路为CLI→CiderXPCBridge→GUI→既有WineRunner/posix_spawn。Bridge仅转发，不导入Wine运行模块或代替GUI启动Wine。游戏帧、图形和音频循环没有加入RPC。默认生产引擎和游戏门禁未改。

### 原计划登记方式的具体候选取舍

ADR-007的GUI派生者保持；当前隔离候选用用户会话中的按需launchd MachService登记，代替05中尚未实现的SMAppService长期登记。没有LoginItem、RunAtLoad、KeepAlive或定时唤醒。收益是当前ad-hoc开发包能验证真实Mach/XPC通道；成本是会话登记及App路径/签名升级时的旧实例协调。Developer ID与正式SMAppService发布仍待，不宣称该发布项完成。

服务名绑定规范化App路径与资料根；不同安装、不同CIDER_HOME不互相接管。已有服务不强制卸载，失败不改走CLI本地Wine派生。回退按这组入口/GUI模块/打包整体撤回，不能留下单侧协议。恢复到主线前仍需选定范围、受影响的真实Wine入口与responsible/TCC验证。

## 对抗性审查发现并修复

下表包含本轮新增XPC候选在实现阶段发现的缺口，不能归咎为10月13原隔离批次的问题。父代理实际修改并验证，不把代理报告本身当完成证据。

| 缺口 | 实际修复 |
|---|---|
| 正确签名的进程仍可能带注入代码运行 | GUI/CLI/Bridge均启用hardened runtime，不放开注入/库验证；GUI仅增加原计划摄像头和音频输入entitlements |
| 多个`--wait`挤占停止操作 | 8个普通执行位置，另保留2个`kill`接纳及执行位置 |
| host已登记而输出连接尚未就绪 | 在对外登记前安装连接；失败仅清理该连接 |
| Bridge中断或首次连接失败后不再接回 | GUI先保留host，首次及中断重连使用有界退避；连接就绪转移和失联判断在同一锁内，不重提已接纳命令 |
| CLI冷启动可能选择另一份正在运行的Cider | 禁止NSWorkspace替换并核对返回的App路径 |
| 256个请求后遗忘旧ID导致再次执行 | Broker和GUI均保留会话请求记录，不逐出；到65536条明确拒绝新接纳，现有任务不受影响 |
| 总输出上限/五秒慢读会丢掉合法输出 | 8KiB分块、确认后再发送；无生命周期输出总量上限；健康终端慢读不设任意丢弃期限，真实断线/关闭唤醒等待并标识未完整输出 |
| 不存在的资料目录创建后路径别名变化 | 真实冷启动发现Foundation把`/private/var`/`/var`表示变换，服务名变化、Bridge退出64。改为POSIX realpath解析已有祖先后附加缺失部分，使创建前后身份稳定 |

XPC使用OS对连接进程的code signing requirement校验，分别绑定CLI、GUI和Bridge；不按可复用PID认证。PID只用于回执归属检查。输入版本、资料根、NUL、参数和数据大小在执行前检查。配置使用受限目录描述符、排他临时文件和原子发布；launchctl等待及清理有界。断线返回结果未知，不冒充成功、停止或自动重试。

官方依据：[NSXPCConnection代码签名约束](https://developer.apple.com/documentation/foundation/nsxpcconnection/setcodesigningrequirement(_:))、[Apple Hardened Runtime](https://developer.apple.com/documentation/security/hardened-runtime)。这些文档依据不等于本项目真实TCC或游戏验证。

## 验证与证据

- 15项命令测试通过，其中参数化检查覆盖原29条帮助路径；1项不存在目录创建前后的canonical/service-label测试通过。
- 23项真实macOS NSXPC检查通过：角色代码身份拒绝、畸形/越界/错目录输入、输出与退出码、并发请求去重、断线未知结果及Bridge重连等。1,347,730字节输出与预期SHA256一致；读者暂停6秒后完整送达。8个普通等待任务满载时，控制请求45ms内到达有限夹具执行器；它按白名单返回64，没有调用Wine。经过257个中间请求以及Broker重启后，旧ID均未再次执行。
- 另2项使用真实release CLI与Bridge二进制的NSWorkspace冷启动/同host暖调用通过；独立执行回执确认执行者为夹具AppKit host。该host只允许只读命令，没有产品窗口或Wine。单次冷/暖耗时约1.37/0.16秒，不是游戏启动性能保证或统计基准。
- release GUI/CLI/Bridge完整构建、三个runtime签名标记及deep/strict验证通过；打包CLI从/tmp执行帮助/版本/参数错误通过。没有运行注入攻击探针，不把签名标记检查宣称为全面安全验证。
- 父代理核对最终回执与实际源文件/二进制摘要；独立只读对抗性复审无剩余P1/P2发现。此前的10项静态缺口及1项真实cold-start问题均实际修改，没有只改报告措辞。
- 最终测试job已注销、所记录host/Bridge PID均已退出。第一次cold-start失败记录（包括后续实际清理回执）保留；不存在“首次失败但只保存重跑成功”的情况。

[最终回执及摘要](https://github.com/CervantesHallo/Cider/blob/bb096da/tests/xpc/results/final-handoff.json)、[原生23项](https://github.com/CervantesHallo/Cider/blob/bb096da/tests/xpc/results/native-20261016T105309Z.json)、[真实CLI两项](https://github.com/CervantesHallo/Cider/blob/bb096da/tests/xpc/results/packaged-cli-20261016T105254Z.json)。这些夹具不计为Windows、Wine、游戏或TCC通过。仍缺真实产品会话的responsible/TCC、图形/音频入口回归、正式签名/登记及Windows内核对照。

源码提交：`bb096da`，作者/提交者均CervantesHallo，`[skip ci]`。本次不恢复主线实现。结构化交付见`evidence/quarantine-xpc-repairs-20261016.json`。

## 平台阻断的准确含义

research62所指的是本次Codex会话中K2专项工具的实际自动安全审查拒绝，返回“可能存在网络安全风险”。没有得到规则名称或详细判据；不能据此认定整个项目、所有忠实Windows API或用户目标违规。它也不是GitHub Actions、代码编译器或Windows防火墙的错误。未换措辞、换代理或换通道重试那项已拒绝的替换方案，K2仍明确未完成。

[OpenAI公开说明](https://learn.chatgpt.com/docs/cyber-safety)区分模型/Trusted Access以及敏感动作的自动审查；公开说明不能反推这个具体调用的内部判据或本账户资格。继续完成独立可做的兼容与质量修复；未完成部分按真实状态记录，撤回不是API实现。

## 原队列及保护范围

柚子社两款Control/退出重开已有验收；HY1既有启动器首装、标准/Retina和常用入口证据保留；HY2原生kernel、HY3及三款本地交互会话尚未完成。原神已下载文件保留，执行顺序HY1→HY2→HY3→HY4/HY5不变。常用App、已安装引擎、Steam登录/存档和真实Windows账本不替换。Windows准备0、测试3600及原pending保持；没有Windows/RDP/SSH、扩额或Actions，原定时保持关闭。
