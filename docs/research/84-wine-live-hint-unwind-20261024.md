# Wine 活跃等待登记的终止回收观测

状态：补充诊断准备中，实际两位宽各64轮和故意缺失unwind负例已完成；不代替83精确产物的性能/小时结果，不提升Windows、游戏或默认引擎状态。

## 要补的证据

83的native取消用例在显式移除登记后交付pending请求。它不能证明实际Wine控制信号会移除一个仍活跃的登记。既有worker-ready和Windows线程join也不单独证明原生登记已清理。

因此另建F27派生的非默认trace产物，原64547f41…/7b0cb109…源和自己的小时结果保持不变。普通完整mask路径在真实ResetEvent后、pin仍活跃且owner已释放时，记录Windows句柄与实际共享索引；在晚发布完成后、raw wait前记录实际登记。记录包含Windows PID/TID以及通过实际descriptor fstat取得的coordinator设备/inode，不凭最近文件名猜映射。

trace使用原生CPU缓冲格式化和raw write，避免stdio/locale/heap锁；errno getter在启动解析/预热并保存恢复。注册记录**不是kernel-park证据**。只读reader增加可选`--hint index`，用实际magic7布局的原子读取；不可用不当0。原小时使用的reader没有替换。

## 有界协议

`tests/sync/live-hint-reap.c`只创建、终止并join自己拥有的线程。每轮用nonce和round对应控制文件，最多64轮，API join保持5000ms。

1. 父线程持有同一个manual event，真实Reset后报告阶段。观察器用目标句柄、PID/TID及当轮新日志范围绑定索引，验证bucket为0后才允许START。
2. 实际创建的worker报告ready后等待目标event。观察器仍须看到匹配TID/索引的新原生登记，以及同一设备/inode/producer代际的实际bucket值1，才允许TERMINATE。
3. 父线程调用真实TerminateThread，join并核对退出码42。观察器须另见两次0读数才发CLEARED；不会清写计数器，也不会以join本身充当0。
4. 故意省略hint unwind的独立负例必须留下已观察的登记、join42后持续为1，并被拒绝。失败用STOP/进程退出清理，不发送假CLEARED。

控制文件只在隔离prefix内；只通过既有打包CLI→认证XPC→GUI启动。原hour仍运行、清理未确认或wine-runtime.lock被占用时拒绝操作。全程有150秒外层限时、时钟间断拒绝、精确原GUI退出和两次空观察；实际运行和负例未完成前不称该协议已接通。

## 当前验证

reader新CLI六个只读实机输入检查通过，旧magic3构建明确拒绝不支持的查询；没有重跑旧Win32基线。PE两位宽仅完成构建。纯观察器协议九项通过；其中下一轮记录可能恰从上轮末偏移开始的边界错误已在实际Wine执行前发现并修正，旧源保留。

准备部分的限定独立复审未发现具体P1/P2；完整观察器复审及真实运行仍待。源/构建、纯协议、Wine运行、kernel park、Windows原生对照、性能与小时分别记状态。

## 控制器边界与首轮实机状态

完整观察器审查先发现四项P2：阻塞读后过期仍可确认、最终clock复核缺口、已退出CLI的partial pipes漏收，以及150秒总限时可在最后一次阻塞后越界接受。均按实际接受点修复，新增对应纯协议故障例，共14项通过。一次仅基于描述、未读文件的报告明确不当独立复审证据。

首次两轮实机预检未产生trace：受保护CIDER_*覆盖被现有WineRunner过滤，保留该过滤并改用WINEMSYNC_TRACE_WAIT，另构建新诊断image；该次失败/清理保留，不当engine并发失败。新两轮看到实际0→1→两次0，真实TerminateThread/join42和完整清理通过。独立省略unwind负例实际看到join42后48次读数持续为1，观察器拒绝并用STOP结束，最后GUI退出+两次空观察确认；其CLI被终止的状态保留，不伪装成干净成功。完整64轮两位宽及最终观察器实际文件复审后续均已完成。

## 完整实际结果

真正读取文件的最终限定复审确认四项P2闭合；原未读文件报告未作为验证。x86_64与i686各64轮均观察到baseline0、匹配真实登记1、TerminateThread/join42后两次0，所有客户端计数、实际native/双PE/EXE及reader身份已记录，GUI退出和两次空观察确认。不是由worker-ready、join或缺少数据推定0。

故意缺失unwind的独立image在第0轮join42后48份读数持续为1，得到特定hint_still_positive_after_join拒绝；程序报告observer-stop，CLI随后被终止，状态如实保留。负例未发送假CLEARED，不当正常运行成功。

诊断positive为5e80379c…/7b0cb109…，negative为9d06aa7f…/7b0cb109…；它们没有自己的5倍/小时证据，不代替原F27的64547f41…/7b0cb109…正式结果。早期运行manifest没有直接记录controller源码hash，只能关联当时实际文件复审的相同hash；公开版本补记该字段，路径参数化后的纯协议14检查和两轮实机另列，不改写早期回执。
