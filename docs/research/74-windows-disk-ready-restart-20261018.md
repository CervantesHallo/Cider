# HY2：D盘空间就绪与既有重启交接

2026-10-18，接续73及R v1组件构建。用户明确D盘清理完成并要求继续；核对同一已记录LAN路由/宿主pin/项目公钥后恢复只读准备，没有重做已通过Win32或游戏基线。

## 实际结果

D盘可用220885598208字节，约205.7GiB；随后功能预检确认NTFS，满足当前约100GiB空闲的参考VM准备门槛。主机当时仅约5.6GiB空闲内存，Hyper-V角色Disabled、vmms未安装，原EWDK本轮未挂载。不能把磁盘就绪当作VM或kernel参考已就绪。

Hyper-V准备脚本先核对管理员token、D盘和现有功能/重启状态；检测到先前已有CBS RebootPending，因此按保护条件在调用Enable-WindowsOptionalFeature之前返回。实际结果为restart-required-before-install、changed=false、servicing_started=false、功能前后均Disabled。没有执行启用命令、重启、启动VM、加载驱动或改启动保护。

所准备的功能命令使用微软明确提供的NoRestart与LimitAccess，以避免自动重启及新一次Windows Update来源查询；[参数说明](https://learn.microsoft.com/en-us/powershell/module/dism/enable-windowsoptionalfeature)支持这两项行为。当前不是该命令被系统拒绝的证据，而是预检主动保留既有待重启状态。

## 人工交接及账本

用户回复暂时不能重启。人工交接已按真实拒绝结果关闭：本地预约到答复关闭间隔119.806秒，无Windows命令、重启或远程会话派生；不把这个本地等待时段写为Windows实际执行时间。之前两批原生准备的完整控制器外层分别3.242/3.155秒，共6.397秒（含传输/回执，不是纯Windows子进程CPU时间），均有空Job及完整传输返回的真实清理回执。

三项预约90+300+300共690秒全部按原规则扣账，未退款/重置/扩额。当前余准备2309秒（38分29秒）、测试3600秒，无pending；按用户当前不能重启的决定保留Windows会话并置busy。用户明确准备好/正常重启后才恢复Windows工作，不反复查询或叠加组件修改。

交接操作教训：先确定人工步骤能开始，再预约并发执行指令，避免对尚不能开始的人工动作预扣机时。本轮请求消息早于人工预约，但用户确认没有执行，记录原顺序及实测本地等待，不伪造操作开始时间。

## 后续原顺序

正常重启确认后，在已有预算内核对功能/资源，再完成Hyper-V及D盘独立Gen2参考环境。客机系统镜像与EWDK不同；微软当前下载页已更新为Windows11 26H2，尚未取得/采用，不能自动当成主机26200同一版本。[官方评估页](https://www.microsoft.com/en-us/evalcenter/evaluate-windows-11-enterprise)。实际选择、构建及客机安装交互仍需记录，原主系统和游戏不复制。

用户暂不能重启期间，继续原02的Mac引擎工作：msync客户端/服务端/协议成组迁移，保留原等待算法并修实际错误路径；新源/编译与真实Windows参考、默认采用、游戏会话分别记状态，不解除门禁。公开脱敏数据见[evidence](evidence/windows-disk-ready-restart-20261018.json)，原始连接与日志仅私有。
