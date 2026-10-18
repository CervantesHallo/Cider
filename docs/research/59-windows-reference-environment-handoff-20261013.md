# HY2：独立参考环境和预算交接提案

2026-10-13。用户询问需要做什么、追加多少机时及独立环境步骤，并要求准备好后仍按原计划推进。本节是具体待确认提案；询问追加量不算批准。没有连接Windows、增加额度或关闭原pending。

## 建议的一次授权

- 追加准备3600秒，测试使用原剩余3600秒，不追加或每日重置。
- 同时允许原预约ba2ed2b2-3f78-4911-b941-bd5b3d369dd2按已保存真实外层330.987秒及已核对清理回执结算。原预约300秒保持，超出30.987秒向上取整额外扣31秒，计入这笔3600秒追加。
- 获批准并完成结算后，准备可用3569秒（59分29秒）；测试仍3600秒。这是下一阶段上限，不是环境或项目必在该时间完成的承诺。Windows上的下载、安装、VM运行、失败/等待/清理均按原规则计时，批次最多600秒；不能把安装留在预算外跑。

预算用于硬件/已有Hyper-V核对、必要功能启用/重启、空白系统安装、隔离与快照、限时执行器及VM停止控制的原生准备。30分钟可能不足以包含完整系统安装和控制验收，建议先封顶60分钟；届时不足保留真实结果，不再次自动追加。已完成的SSH/SFTP及Win32基线不重做，RDP无新证据不盲重试。

## 结算实现已准备，尚未执行

旧close拒绝超过预约的真实回执，extend拒绝pending，直接改JSON会破坏历史。新增`reconcile-preparation`专用于人类明确批准的一笔attended preparation：要求目标唯一pending、精确批准的实测总数、原verified cleanup回执、有限的新追加、稳定authorization-id及user-approved标志；原秒数/限额/回执字节不改。追加和结算同一锁内验证/原子落盘，额外计费参与所有status/validate/reserve计算；重试仅接受完全相同条款/回执摘要，不重复追加。

对抗性静态自审：不能用新命令给测试阶段扩额或原生未测量任务结算；NaN/Infinity、缺验证的清理、错误预约、额度不覆盖超额、STOP/其他pending拒绝；原close继续拒绝超额，原extend规则保持。授权和目标/实测/回执链接双向校验，不能凭添加字段获得退款；extra charge只增加，不减少原预扣。flag不代表已经取得人类授权，维护者仍须对应本聊天的真实批准。该入口只记账，不操作Windows或解决VM清理。

仅Python AST解析、CLI帮助和现有只读status完成；未新增/运行测试套件，未调用reconcile/extend/close/reserve。私有账本在准备前后SHA256相同、仍准备0/测试3600/原pending。真正执行须等用户明确批准这两个条款并再次核对原回执范围。

## 独立环境：优先Hyper-V客机

已确认主机是Windows11专业版；[微软Hyper-V说明](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/get-started/install-hyper-v)支持该版本，但CPU虚拟化、SLAT、内存、磁盘及既有功能状态尚未采集。批准后先核对，不重复部署既有工具。拟用Gen2、4vCPU、8GiB RAM、100GiB动态VHDX，实际依据主机资源收敛；建议主机至少16GiB内存和约100GiB可用磁盘作为本方案准备门槛，不冒充微软最低要求。

安装时使用UEFI/Secure Boot/vTPM的标准Win11配置；[Windows虚拟机要求](https://learn.microsoft.com/en-au/windows/whats-new/windows-11-requirements)须先满足。系统安装ISO与已下载EWDK ISO不同。候选系统来源为[微软Windows11 Enterprise评估](https://www.microsoft.com/en-us/evalcenter/evaluate-windows-11-enterprise)的x64 ISO，90天且需登记/安装激活；账号及协议由用户自行处理。既有EWDK只挂载复用，不再下载一份。真实客机版本/构建单独记录，不把不同版本自动当作主力机同一基线。

用户只需：确认预算/旧超额结算及可用窗口；必要时在“启用或关闭Windows功能”启用Hyper-V后自行重启；取得系统ISO并处理客机Windows安装/账号/协议。父代理在批准/预约后完成硬件核对、命令配置、VM创建、文件和结果通道。桌面暂不能自动化，用户可通过本机或已有ToDesk进行这些少量屏幕步骤，不重新推进失败的RDP自动点击。

客机是独立系统盘和测试账户，不克隆日常Windows或复制游戏；仅放自有参考工程。安装完成先创建干净checkpoint，之后只在该客机内准备自有证书签名及测试模式。[微软测试签名要求](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option)仍要求驱动映像有签名；未签名构建不是可直接加载的成品。若需调整Secure Boot，只改关机客机的设置，日用主机启动保护和游戏保持。

收益：取到真实Windows kernel的token/context行为，参考驱动故障和测试模式作用于可回退客机；不依赖游戏或反作弊，也不要求重下原神。成本：主机功能可能需要一次重启，VM的CPU/内存/磁盘占用及Windows安装交接；评估到期会影响继续运行。回退先停止并保留/导出自己的VM及checkpoint，不自动删除既有VM、全局网络或主机功能。

## 实施前必须闭合的控制

SSH已经有命令/文件证据，但Windows原生限时执行器尚未部署。其Job只覆盖普通子进程，不覆盖Hyper-V VM；不能以它退出推定VM停止。VM创建/运行需另有属于本批的宿主截止和实际Saved/Off清理回执，安装跨批次要先停止/保存并核对后再预约下一批，不能连续越过600秒。Windows屏幕验收仍需用户实际操作，密码由用户输入、不采集。

未落实这些控制前只做在场、有界的准备，不启用无人值守测试或恢复定时任务。准备完成后的首批为既有自有参考驱动的kernel/context观察及新候选的对应验证，原Win32已通过内容不重跑；后续继续HY2→HY3→HY4/HY5，原游戏门禁和证据要求保持。
