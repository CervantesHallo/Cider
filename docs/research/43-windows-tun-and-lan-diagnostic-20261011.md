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
