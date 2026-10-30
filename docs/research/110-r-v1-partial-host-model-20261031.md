# R v1：部分安装的真实图层与移除确认

日期：2026-10-31，UTC+8。接续[109](109-r-v1-recovery-epoch-20261031.md)，为per-host完成状态及未知INSTALL取消补齐模型所有权基础。

## 问题与候选实现

原Cocoa安装先挂上图层、后登记当前context索引。中途异常可能留下已挂上的对象；仅凭索引缺失返回移除成功，无法证明这个对象已经消失。

[0026](../../engine/porting/wine-11.18/0026-winemac-fence-host-jobs-and-own-partial-models.patch)候选增加独立的逐ticket模型登记，先持有对象、后导入真实context。部分失败保留具体对象；实际移除查这份登记，未知析构结果保持未解决，不重放release或假定成功。Native在派发前计入安装任务，更新任务也有计数；最后任务完成才交出等待的退役owner。原请求、进程及producer终端保护保持。

对抗性审查发现并修正三处具体问题：HJF-P1-01中，重入替换能绕过Native任务计数释放正在更新的Cocoa对象；新增精确对象使用pin，所有移除路径都检查它。HJF-P2-02中，旧安装重入后能覆盖更新代次；改为C数据选择表、单调代次及不回绕的独立serial，发布前重新校验。HJF-P2-03中，新接口不能推断旧对象也有Native退役owner；按前任实际归属处理，旧接口对象未能移除时拒绝新发布并保留其可定位状态。几何事务另补finally，避免捕获异常后留下未结束的CATransaction。

最终普通source-v8、Native及NDEBUG协调器、原生链接构建完成；10条Cocoa警告与原消息及原源码语句一致。限定源码、事务、夹具和集成复审均接纳。**重入、legacy交接及一般Retina运行尚未由本轮异常用例验收**；不把源码修正说成全部运行验证。

## 真实异常正反对照

[夹具](../../tests/graphics/host-partial-model/)使用真实Win32 root/child、自身producer lease和实际导出的Metal/CAContext。在真实addSublayer之后抛异常，再让第一次实际回滚抛异常。普通失败安装返回0，但真实对象仍挂在父图层，索引0、独立登记1；Native退役owner及待移除计数各1。测试仅转交这份真实owner，不伪造计数或NT状态。

撤销producer并确认进程handle关闭后，释放该owner走普通checked removal。在实际移除调用返回、其真实Native完成回调之前，冻结该次调用的返回值、父图层实物、精确登记及当前选择。主检查仅判断同一次“移除成功”是否仍留下真实对象。

| 组 | PE64 | PE32 | frozen removed / attached / ledger |
|---|---|---|---|
| 精确对象移除 | 30项、0失败、CLI0 | 同左 | 1 / 0 / 0 |
| 仅恢复索引缺失即成功 | 19项、1失败、CLI1，仅CHECK19 | 同左 | 1 / 1 / 1 |

反例先通过CHECK1–18的真实异常及对应调用校验，才命中CHECK19；不是任意超时。它如实保留实际context/window/producer观察owner，internal_cleanup=false，外层再确认进程退出。正例在窗口仍活着时纯观察实际模型及所测Native/PE资源归零，之后才释放导出context、销毁窗口并归还观察引用。

这里没有RO producer页、PE终端ACK或直接SAFE读取；Native finalized也不作为主证据。没有GPU/compositor、像素、完整泄漏或游戏结论。

## 构建与交付边界

最终diagnostic-source-v2/build-v4、256B/version1帧、48B/version2纯probe、两表8槽、watch2配套。父代理核对10组实际编译依赖闭包及123个项目输入；4份EXE/raw/session、16PE及2native组件、F27核心、10份完整进程清理回执、瓶子原字节或原缺失状态、四个保护摘要另行绑定。[证据](evidence/r-v1-partial-host-model-20261031.json)保留全部范围和失败构建；未修改原始失败记录。普通0026、诊断及唯一负例补丁均零fuzz、零offset逐字重建。

成本是每个模型的登记与使用计数、每context保留到窗口销毁的选择高水位，以及较晚的物理退役；收益是避免部分对象失踪、过早移除和旧安装覆盖新选择。本轮不提供性能数字。回退维持已保留的普通实验基底和用户当前引擎；候选没有切入用户默认。

## 接续

下一步把这套所有权接到per-host只读状态及已知ticket的安装/取消，补实际重入、旧接口交接和显示模式用例。采用“先保留ticket并映射状态、后导入”的实现方向，避免INSTALL回复丢失后连取消对象也未知；具体协议仍未实施。额外成本是一轮准备请求及每host状态页，完整取舍和验收随实现记录，不能把本次普通producer页当作per-host已完成。

随后连接真实producer资源、事件reparent和两三进程呈现。原HY/U8-15、Windows忙待重启及游戏门禁保持；Windows、Actions、定时、Goal零操作。
