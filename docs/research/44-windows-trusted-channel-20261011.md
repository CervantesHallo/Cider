# Windows可信命令/文件通道接通（2026-10-11）

接续research/43。原HY1→HY2→HY3→HY4/HY5顺序、游戏门禁及Windows累计预算保持。[脱敏证据](evidence/windows-trusted-channel-20261011.json)；连接地址、账户、密钥、原始日志和备份路径仅私有。

## 实际结果

- 用户给localhost/Public明确配置AllowInboundRules=True并成功刷新计算机策略；最终有效Enabled=True、DefaultInboundAction=Block、AllowInboundRules=True。此方案已采用；它让Public所有已有允许规则恢复评估，不能称为仅开放2222。原值NotConfigured的唯一备份保留，按research/43回退。原始阻断值作者仍未知，不归因Clash。
- 新局域网连接取得原始Ed25519宿主pin，项目公钥与安装记录匹配。严格宿主验证、仅公钥登录后，本次唯一远程echo标记返回，SSH/远端均退出0。这是实际认证/命令通道证据；不等于公网转发、RDP或kernel执行已通过。
- 8KiB随机文件SFTP上传/下载后Mac两份SHA256与Windows Get-FileHash一致；Windows临时文件/目录已实际删除，命令返回0。Windows只读构建目录观测确认EWDK仍挂载、已有VS2022 BuildTools；不是新构建/任务执行器原生验收。
- Mac Windows App按名称读取报noWindowsAvailable，按实际bundle ID报timeoutReached；应用列表显示它已运行。没有建立RDP隧道/会话或输入凭据，屏幕通道仍需人工打开主窗口后再定位。不开其他UI通道绕过工具，不重复无变化失败。

## 操作经验与修正

1. OpenSSH的`-o UserKnownHostsFile=...`仍经过配置值解析；路径含空格须在选项内部引用。仅让subprocess保持单独argv不足。首轮严格检查拒绝读取拆成两段的路径，未认证；引用修正后保持严格检查并成功登录，没有接受未知宿主密钥。
2. Windows PowerShell模块自动加载可能把中文CLIXML进度写到stderr，编码并非UTF8。传输先保留原始字节，显式设置结构化stdout为UTF8、关闭普通进度；stderr不作为JSON解码。首轮解码失败后确认并删除本次空探测目录才继续。
3. Windows OpenSSH SFTP的盘符绝对路径需`/C:/...`；`C:/...`本次被拼到用户主目录下而失败。更正后实际put/get/native hash/cleanup均通过，不笼统归为权限或服务错误。
4. 只看System32会漏掉已安装微软MSI提供者。`collect-access-readiness.ps1`现在读取真实sshd服务映像，仅验证路径、从不执行服务命令行，并输出有效防火墙文字状态/精确Shields-Up条件；读取失败保留null与错误，不假称未安装或允许。新增字段兼容原JSON，未改变任何设置。仅Mac PowerShell7.6.6解析通过，Windows新版采集器未执行。

对抗性自审覆盖：单个本地Public策略且备份；既有firewall/default Block保留；宿主pin与项目公钥双重核对；仅验证命令/文件；探测文件目录归本次唯一预约，非递归清理；未知路径/策略状态不执行；新版采集器CIM/防火墙查询失败不破坏其余报告。没有触发Actions、运行原Win32基线、加载驱动、修改启动保护或启动游戏/VM。

## 预算与下一步

本轮后续连接预约120秒、客户端路径修正60秒、文件/构建只读准备120秒都已按真实结果结算；文件批次包含解码/路径修正及清理，累计观察3.240秒。全额扣账、不退款；余准备660秒（11分钟）、测试3600秒（60分钟），无pending，没有保留项目SSH客户端、探测文件或Windows测试进程。

下一顺序：人工显示Mac Windows App主窗口→在预约内准备既有配置的SSH限定RDP隧道，用户自行登录→真实截图/点击验证；原生CLI限时执行器的构建/新验证可走已通SSH文件通道。Windows native watchdog/真实清理回执闭合前不启用无人值守测试；随后独立VM/kernel参考与token/context实现，不能在日用Windows加载未签名驱动。保留原生RDP不替代物理控制台图形/性能的边界。工作窗口外不自动开展Windows操作，下一09:00按账本继续，不重做已通过的连接/基线。

## 人工在场的RDP登录交接（后续）

用户手动打开Mac Windows App后CUA实际取得主窗口与配置表单。保存配置为回环隧道端口、需要时询问凭据、自动重连off；剪贴板/文件夹/打印机/智能卡/麦克风/摄像头均不重定向。只查看并取消配置表单，未修改这些设置。此前noWindowsAvailable/timeout现因用户打开窗口而解除，不当作屏幕会话已验收。

在当前人类在场准备例外下预扣180秒预约3d1b22b8-7b85-4541-a496-3e8c0c3f8a25，余准备480秒/测试3600秒，pending。SSH只读确认RDP监听与TLS层2，但NLA要求原为0、策略来源0；按原计划保持NLA要求，先保存本机原值，再调用[微软SetUserAuthenticationRequired方法](https://learn.microsoft.com/en-us/windows/win32/termserv/setuserauthenticationrequired-win32-tsgeneralsetting)设1。脚本误以ReturnValue字段判断结果而退出1，字段未取得；随后独立读回required=1/security_layer=2，实际状态验证通过，没有重复Set或伪造方法返回0。备份仅私有；采用此NLA设置，要求支持RDP6及以上客户端，回退须明确用户决定并按备份恢复该项，不因登录失败自动降级。

已用严格pin/项目公钥启动仅绑定Mac回环、只转发Windows回环RDP的隧道；Mac控制器150秒截止/STOP会终止本次SSH客户端，隧道返回也不自动证明RDP UI/进程已清理。CUA双击保存配置实际出现凭据表单，仅填写既有Windows账户名，密码/证书接受交给用户，不保存凭据。未建立已认证桌面、未执行桌面点击/原生watchdog/测试/VM/驱动。预约需隧道停止及实际UI关闭/断开证据后结算，不能先自动标完成；下一自动运行先处理pending与用户回执。

用户随后报告连接失败并关闭提示回主界面。旧隧道17:22:11按时结束/客户端退出0，CUA实际主界面已核对，前180秒预约按累计153.686秒控制器操作/隧道和无项目桌面任务范围结算；没有保留错误码，不把失败只归因截止。用户授权重开后另扣240秒人工准备，新预约e2e5de8e-7128-40f3-be54-8f1f52374bcf pending，余准备240秒/测试3600秒。新隧道210秒截止17:28:31、严格认证/回环监听已就绪，凭据表单真实重现；密码仍人类输入。再失败需保留错误窗口定位，不盲重试/放宽NLA。新预约尚未结算，屏幕/native watchdog仍未验收。

第二次用户仍未能及时完成登录，隧道按210秒截止退出（控制器210.198秒），CUA在退出后确认仍为主界面，没有已验证桌面或本项目桌面任务。该240秒预约已结算，余准备240秒/测试3600秒、无pending，全扣不退款。后续先确认用户已准备好账户密码且可立即操作，再预约并把隧道启动放到交接最后一步；不直接第三次重开消耗额度，不把没有登录结果当作凭据错误，也不放宽NLA。当前等待人类就绪信息。
