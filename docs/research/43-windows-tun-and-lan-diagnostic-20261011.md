# Windows接入：TUN实参与局域网验证（2026-10-11）

## 新截图与版本核对

用户TUN设置弹窗实际显示：网卡Mihomo、GVisor、自动全局路由on、严格路由off、自动选择出口on、DNS劫持any:53、MTU1500。此前首页为Verge2.2.2/Mihomo1.19.3。这是界面实参，不替代运行内核或实际回程路由观测。

[Verge2.2.2配置源码](https://raw.githubusercontent.com/clash-verge-rev/clash-verge-rev/v2.2.2/src-tauri/src/config/clash.rs)的默认stack/auto-route/strict-route/auto-detect-interface/dns-hijack与这些值相同，不能只因使用GVisor就断言配置错误。[Mihomo1.19.3依赖](https://raw.githubusercontent.com/MetaCubeX/mihomo/v1.19.3/go.mod)固定sing-tun0.4.5；[该版Windows实现](https://raw.githubusercontent.com/MetaCubeX/sing-tun/v0.4.5/tun_windows.go)可用于后续路由审查。没有切换stack、关闭TUN或改防火墙；根因仍未知。

## 新Mac侧观测

Mac当前到用户Windows内网地址的路由指向en0，已有ARP缓存的MAC与用户路由器设备截图一致。地址/硬件标识只留私有配置；这是新局域网路径线索，不当作已认证Windows连接。物理en0公网IP查询12秒超时，没有取得新公网地址，不据此判断两端同WAN或NAT回流。

TUN只读查看的180秒预约根据完整参数截图结算：未启动Windows命令、安装、捕获、VM或测试任务；既有Clash/sshd设施保留，不声称已关闭这些服务。人工实际查看时长未测量，原180秒全扣、不退款。

随后预扣120秒做一次新局域网宿主密钥/认证准备。项目本地公钥指纹与已安装公钥匹配；目标只请求Ed25519宿主密钥，并要求与用户原始pin一致才允许认证。5.025秒后没有收到宿主密钥，因此未认证、未请求Windows shell/channel或部署任何测试任务；本机探测客户端已退出，预约按实际范围结算。保留原pin，没有接受新密钥或忽略校验。此失败不能定为TUN、密码或反作弊问题。

以上只完成了接入诊断，公网/局域网可信连接、命令/文件通道和RDP屏幕尚未接通。两次预约后余准备1620秒、测试3600秒，无pending。

## 下一项人类交接

准备一次管理员PowerShell只读快照，合并当前sshd服务/映像、22与2222监听、自机IPv4、有效防火墙及项目规则/程序范围、到Mac的源地址约束选路。使用一个JSON避免混合表格丢字段。[Find-NetRoute契约](https://learn.microsoft.com/en-us/powershell/module/nettcpip/find-netroute?view=windowsserver2025-ps)

命令已由Mac缓存PowerShell7.6.6仅解析、零语法错误；没有在Mac执行Windows cmdlet，也未产生Windows结果。另预扣60秒人工只读准备，预约18e4f325-733b-46eb-a24e-6b862968764f pending；余准备1560秒（26分钟）、测试3600秒。输出和返回提示符后核对完成再结算，不新开Windows批次。原HY顺序与游戏门禁保持，未复跑原生Win32基线、启动游戏或触发Actions。

## 快照已返回：防火墙允许规则总开关（最新）

用户完整JSON确认：sshd Running，IPv4/IPv6的2222监听进程与服务一致；项目允许规则启用且程序路径匹配服务。源地址约束的到Mac选路落在物理以太网的本地子网，未经过TUN。以上只覆盖本次快照，不能证明公网回程、实际包处理或可信SSH已经通过。原始地址、PID、输出及其摘要仅存私有控制目录。

三种有效防火墙配置均Enabled=1、DefaultInboundAction=4、AllowInboundRules=0、AllowLocalFirewallRules=1。`0`需要原生文字值核对，不能只把数值未经解释直接作最终根因。[微软Set-NetFirewallProfile契约](https://learn.microsoft.com/en-us/powershell/module/netsecurity/set-netfirewallprofile?view=windowsserver2025-ps)明确：AllowInboundRules=False且默认入站Block时进入Shields-Up，入站允许规则被忽略；[CIM属性说明](https://learn.microsoft.com/en-us/windows/win32/fwp/wmi/wfascimprov/msft-netfirewallprofile)同样说明False使允许规则无效。这是当前主要嫌疑；TUN参数本身没有新增错误证据。

准备的人工修正先发现该物理接口唯一NetworkCategory并映射Domain/Private/Public，读取ActiveStore文字状态。只有Enabled=True、默认入站Block、AllowInboundRules=False全部成立才继续；先保存该配置PersistentStore原AllowInboundRules，再仅设置该配置的AllowInboundRules=True并回读有效状态。未知分类、多个配置或状态不符均停止，不猜数值。脚本仅Mac PowerShell7.6.6解析通过，SHA256 `0b87cf9934fb9341513844175eb417ff9ce8a38a6f8927c7f1f043b6fd1416ea`；尚未在Windows执行。

取舍：收益是让已有SSH允许规则被评估，保留防火墙开启和默认入站拒绝；代价是同一配置中其他已有允许规则也恢复生效，不能把它称为只开放2222。当前为待用户执行的具体方案，不记已采用或连接修复。验收需要有效状态回读True、既有防火墙/default Block保持，以及一次严格宿主pin/公钥认证；组策略覆盖或仍失败时保留新证据，不反复切换代理。回退使用打印的备份Profile及原AllowInboundRules恢复PersistentStore，随后回读；不关闭防火墙、不改代理或启动保护。

对抗性自审：不用全profile命令，不改变网络分类，不修改默认入站/其他规则；备份唯一文件避免覆盖原值，修改前任何错误停止；有效策略可能覆盖本地改动，回读不是可达/认证成功；此次准备未启动测试、驱动、VM或额外服务。

前60秒只读预约按完整JSON、无项目运行任务的完成范围结算，人工时长未测量，仍全额扣账。新120秒在场修正预约c20cbefe-18dd-44df-bdf8-b35b470a3e66 pending；余准备1440秒（24分钟）、测试3600秒。待用户操作输出后才结算和预约新连接批次。

## 本地修正未改变有效值：核对来源（最新）

用户实际输出确认接口对应Public；前后均Enabled=True、DefaultInboundAction=Block、AllowInboundRules=False，备份文件路径已打印，Set命令后没有报告错误。False文字值已确认，但本地设置没有改变最后读到的有效策略；没有采集PersistentStore修改后的值，不能据此断言已写成功、由域管理、MDM或Clash覆盖。没有进行新的网络重试。

[微软Get-NetFirewallProfile](https://learn.microsoft.com/en-us/powershell/module/netsecurity/get-netfirewallprofile?view=windowsserver2022-ps)区分本地PersistentStore、合并后的ActiveStore和只读组策略RSOP；[Firewall CSP的Shielded](https://learn.microsoft.com/en-us/windows/client-management/mdm/firewall-csp#mdmstorepublicprofileshielded)说明开启值在合并时优先。这说明覆盖是候选解释，不是本机已验证来源。下一条仅查询Public在三个存储的文字值、[COM阻断全部入站状态](https://learn.microsoft.com/en-us/windows/win32/api/netfw/nf-netfw-inetfwpolicy2-get_blockallinboundtraffic)、[本地规则修改有效性](https://learn.microsoft.com/en-us/windows/win32/api/netfw/nf-netfw-inetfwpolicy2-get_localpolicymodifystate)，以及Public本地/Policies键中相关两个值；不写注册表、清空策略、停服务或再修改防火墙。

该查询仅在Mac PowerShell7.6.6解析通过，SHA256 `47e9393b7d3a52dd9c393a3ebdc75d7768343efde04815adbfb947e6717bbff7`，Windows未执行。对抗性自审：存储读取失败保留错误；只看Public，不读取完整GPO/设备管理报告中的身份信息；本地策略与有效策略不同不直接归因于企业管理；新来源未取得前不重复修正或探测。备份/真实连接信息只私有。

前120秒人工准备已根据最终读回状态和无项目运行任务完成结算，不声称配置回滚或SSH成功。另预扣60秒在场只读来源查询，预约d83462da-0859-4914-a13d-b2f7a27c9085 pending；余准备1380秒（23分钟）/测试3600秒。待用户输出；未触发Actions、测试、VM或驱动加载。
