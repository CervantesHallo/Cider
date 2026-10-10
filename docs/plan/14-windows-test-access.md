# 14 Windows 主力机的时间预算与无人值守访问

更新：2026-10-11。适用于 HY2 的 Windows 参考采集和后续确需 Windows 的验证，沿用原项目目标与 HY1→HY2→HY3→HY4/HY5 顺序。

## 用户决定与当前执行限额

- 用户要求明确累计 Windows 占用时间，不能无限占用主力机；白天不在家，授权定时自主规划测试。
- 用户确认窗口：每天 **09:00–17:00，UTC+8**。Windows 与本机 Mac 不在同一局域网。
- 随后用户说明有公网IP、可以开服务，但不知道已有端口对应哪个服务；没有取得地址、端口映射或外网可达证据。不能把这句话记成SSH/RDP已经配置成功。
- 从此安排起，维护者自设的后续使用累计上限为 **120 分钟**：准备最多60分钟、实际测试最多60分钟。不按天重置，不自动调剂/扩大。这是停止线，不是完整游戏适配的工期或成功承诺。
- 既往手动安装/编译的真实机时没有完整记录，不编造历史总数。已完成的 ISO 下载、EWDK 配置、编译和 Win32 对照不重复。
- 启动、执行、失败、重试、等待本机服务、VM运行及清理都计入所属批次；电脑仅保持通电而没有本项目工作不算测试占用。用户亲自操作时间另外记录，不把前面准备成本省略掉。
- 每批最多10分钟，含终止和清理预留；预约的整个上限先扣账、不退款。实际观察耗时另记。这会保守地提前停止，不因短任务/失败重新获得额度。
- 累计额度用完时停止 Windows 工作，报告已用额度、结果、缺项与替代方案；继续 Mac 侧不依赖它的工作。增加额度需要用户明确变更，不能以“严格按计划”为理由无限延长。

## 已采用的调度与账本

- 当前聊天 heartbeat：`cider` / “Cider 白天自主测试与实现”，每天09:00启动。已通过应用工具创建并核对 ACTIVE；用户后续时间修订优先。
- 优先规划并完成一个有新信息的必要批次，再分析/实现；不定时重跑已通过的基线。无连接/驱动环境时，只推进本机工作；相同阻塞不重复通知。
- `scripts/windows-reference-budget.py status` 初始化并读取持久账本；路径为 `~/Library/Application Support/Cider/WindowsReferenceControl/budget.json`。预算不放在临时目录、实验瓶子或自动化运行目录。
- `reserve --phase preparation|test --seconds N --label task-id` 在窗口内先扣账、记录唯一预约；批次全长不得超过距离17:00的剩余时间，文件锁和原子写防止重入。未解决的预约阻止新任务，断线不能视为退出；仍须在已有预约内核对/清理本次进程，不能以pending为由放弃清理。
- 当前用户亲自操作的连接准备交接，可用`--attended-setup`预扣准备预算，不限定于后台09:00–17:00窗口。此标志只适用于当前在场人工准备，不用于自动化唤醒、任何实际测试或扩大额度；heartbeat不得自行使用它。脚本/回执失败时预约继续pending。准备流程保留的已配置SSH服务是接入设施，不宣称已停止，不等同于留下测试进程。
- `close --id UUID --receipt FILE` 只接受同一预约的 `cider.windows-run-cleanup/v1` 完成记录：包含 `reservation_id`、`cleanup_confirmed: true`、`elapsed_seconds`，不返还已扣时间。维护者须核对真实进程/VM已停止，不凭 SSH 返回码生成“清理成功”。超预算观察保持阻塞，不能擦除记录继续跑。
- 用户要停止时，在同一控制目录创建 `STOP` 即拒绝后续预约；撤销停止须依用户指令。已经运行的 Windows 任务还须由其本机 watchdog 负责终止。
- **当前账本不是远程执行器**。Windows 本机超时、进程归属和清理回执尚未部署；落实之前不启用 Windows 无人值守测试。不能将脚本存在写成总时限已实机验证。

## 跨网络连接与屏幕操作（准备中）

优先核对用户已有的公网接入和Windows服务，选择 **Windows OpenSSH + 原生远程桌面（RDP）** 的两条操作通道。SSH负责构建、采集、文件传输；RDP负责普通窗口、登录/弹窗及输入交互。用户的Win11专业版支持RDP主机，但本机自动化工具在RDP窗口的截图/点击仍须实测，不能承诺换软件就修好ToDesk的noWindowsAvailable。

公网服务如果未部署或配置更复杂，再评估 **Tailscale 私有网络 + Windows OpenSSH/RDP**。Windows不是Tailscale SSH服务器支持平台，不能用`tailscale set --ssh`代替Windows OpenSSH。此前Tailscale提案未安装/采用，不要求用户先登录新服务。

- 若复用公网SSH，则采用专用密钥并核对宿主密钥；桌面可在SSH限定转发的本地端口上连接RDP，避免为了屏幕操作再开放公网RDP。现有规则适合才复用，不能猜外网端口。
- RDP不作为物理控制台图形/性能证据的自动替代；真正图形验证需记录会话、显示和GPU条件。远程窗口断开也不表示程序/VM已停止。屏幕操作和VM运行计入同一累计预算，停止线仍适用。

- 收益：不依赖ToDesk画面焦点/输入法，传文件不丢字符；不必让用户保持终端前台或每天复制命令。复用已有公网入口或使用Tailscale时可减少额外端口配置；尚未有公网转发时不能省略其准备成本。
- 代价：Windows配置SSH访问，Mac配置RDP客户端，一次账号/宿主认证交接；仅在采用Tailscale时两台设备额外安装并登录。初次准备耗时未知，纳入60分钟准备额度；登录耗时不是已测量的10分钟承诺。
- 证据：此前ToDesk长文本实际丢10字符、哈希失败，已停止；微软说明Windows支持OpenSSH公钥认证；Tailscale支持普通SSH通过私有网络、Windows无人登录模式。当前Mac未在标准路径发现Tailscale。未确认Windows端安装/认证，未连接成功。
- 配置要求：保留已有代理/公网出口配置；采用Tailscale时用户完成同一网络登录，不启用exit node/子网路由，Windows可启用Run unattended。Mac需保持登录会话、联网和Codex应用运行；Windows需通电联网。不能保证两台机器睡眠/关机时本地自动化仍执行。
- SSH用本项目专用公钥，私钥仅保存在Mac的受限本地目录；记录并核对Windows宿主公钥，不使用跳过宿主校验的选项。限制访问来源和所需端口；不在记忆/公开仓库记录连接地址、设备代码或凭据。RDP保留NLA，账号密码由用户在客户端自行输入，不能通过聊天收集或脚本打印。
- 优先普通权限执行采集和构建。内核参考仍在独立测试VM/合适环境准备后运行，不在日用Windows加载未签名参考驱动或为了接通SSH改动启动保护。SSH可用不代表内核环境可用。
- 回退：移除本项目SSH公钥/规则、停用本项目新增服务配置；断开或卸载本次专用Tailscale节点。若机器已有SSH/Tailscale部署，先读取并复用适合的配置，不整体覆盖或删除。
- 验收：可信宿主密钥/公钥连接；原始文件双端SHA256一致；锁屏/注销状态下普通只读查询可完成；窗口外拒绝任务；断线/超时后本机停止且留下回执；账本跨自动化重启持续累计。

## 一次性交接与实现顺序

1. `scripts/windows/collect-access-readiness.ps1` 提供只读准备采集：本机overlay IPv4（仅已装时）、sshd/RDP状态、常见服务监听端口、RAM/虚拟化。不安装软件或读取网络其他成员/用户名，不改变系统设置。它不能验证路由器公网转发，连接地址/端口另在私有本机配置交接，结果不原样公开提交。
   用户回传首次查询截图：TermService=Running、sshd未返回；两条不同列的Select-Object被格式化为同一表格，RDP启用标志没有显示。后续单一JSON输出，不能把服务Running扩大为RDP已启用。
2. 根据实际状态准备一次性SSH/RDP接入、核对两端宿主密钥，并验证CUA在桌面客户端的截图/点击。需要时才引入Tailscale，避免已经有公网入口时先添加依赖。用户自己完成必要的密码/账号输入。
   `scripts/windows/setup-reference-ssh.ps1` 为新部署的在场手动准备脚本：仅接受专用ed25519公钥，拒绝覆盖已有服务/配置；组件安装等待最多5分钟，超时停止准备并标记Windows servicing状态未知（不能声称后台安装已停）；配置2222端口、仅公钥登录/当前账户、RDP目的端口限定转发，输出真实宿主指纹和RDP状态。未启用RDP、未改路由器、公网可达及桌面工具可用性仍待验证。配置失败撤销本次访问规则/密钥，组件与故障记录保留。不作为Windows测试watchdog或原生验证结果。
   本轮自审覆盖：已有SSH/宿主密钥不覆盖；账户/公钥禁止注入配置文本；本次私钥ACL只限SYSTEM/Administrators；安装失败/不确定状态不重复；配置写入失败恢复原文件；混合表格改JSON。仅静态审查，当前没有本机PowerShell解析器或Windows运行结果，执行失败即保留交接，不宣称脚本已实机验收。
   后续实测：用户执行a42b3d4时在脚本体前解析失败（26:9/28:80），SSH安装/配置未执行。已用本机微软PowerShell7.6.6复现同样两项错误；改为行末逻辑操作符后四个PS源文件均零解析错误，结果见 [语法修复记录](../research/evidence/windows-access-powershell-syntax-20261011.json)。新增`check-powershell-syntax.ps1`仅解析不执行源码；后续用户命令也先用Windows自身Parser.ParseFile校验。语法通过不记为Windows组件安装或连接已验收，本次仍沿用原10分钟预留。
3. 部署Windows本机的限时执行/所属进程清理，再闭合断线、窗口和累计预算。能连接之前不“测试”原生行为；桌面点击自动化没有通过时不能承诺全自动图形验证。
4. Windows实际测试优先Se*/Ps*/context的第一批原生结果；VM准备是否可在剩余额度完成，以实际硬件/配置判断，卡住即明确交接，不反复远程试键盘。
5. 后续并发、异步、KMDF生命周期只随对应实现推进，合并成必要批次。六批10分钟是当前实际测试预算的最多批次数，不代表只需六批就能完成所有契约或游戏适配。

## 本轮对抗性自审与证据范围

- 反例：16:59仍预约10分钟会越过窗口。账本已改为同时检查17:00前的剩余秒数，批次包含清理不能跨界。
- 反例：每天唤醒或断线后重建临时状态可偷偷获得新额度。账本使用固定持久路径、文件锁、先全额扣账，不提供清零/退款命令；pending阻止新任务。
- 反例：SSH退出、RDP断开或预算脚本存在就宣称Windows已停止。账本不执行远程任务，完成回执还需真实清理证据，本机watchdog未部署前不启动无人值守批次。
- 反例：用户说有公网IP就当作端口已经可达，或者RDP看到了画面就当作物理GPU验收。计划明确服务/路由/宿主密钥、CUA图形操作及控制台证据各自确认。
- 实际完成：Python语法解析、账本status初始化（两类均3600秒且无预约）、专用本地SSH密钥0600权限、heartbeat ACTIVE及09:00配置检查。未连接Windows、未执行采集脚本、未装SSH/Tailscale/RDP、未运行测试套件；上述反例为代码/设计审查，不称为故障注入通过。

## 官方依据

- [Windows OpenSSH 安装与服务](https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_install_firstuse)
- [Windows OpenSSH 公钥与服务器配置](https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_server_configuration)
- [Windows 远程桌面支持与连接条件](https://learn.microsoft.com/en-us/windows-server/remote/remote-desktop-services/remotepc/remote-desktop-allow-access)
- [Tailscale 跨设备访问服务](https://tailscale.com/kb/1452/connect-to-devices)
- [Tailscale SSH 支持平台与普通 SSH 叠加](https://tailscale.com/docs/features/tailscale-ssh)
- [Windows Tailscale 无人登录模式](https://tailscale.com/docs/how-to/run-unattended)
- [Mac 安装与登录](https://tailscale.com/docs/install/mac)
- [Windows 安装与登录](https://tailscale.com/docs/install/windows)
- [Codex 定时任务的本机运行条件](https://learn.chatgpt.com/docs/automations?surface=app)
