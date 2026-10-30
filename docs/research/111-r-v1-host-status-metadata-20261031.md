# R v1：真实宿主状态页与已知ticket控制

日期：2026-10-31，UTC+8。接续[110](110-r-v1-partial-host-model-20261031.md)。本轮完成协议及NT页的正常元数据运行；真实图层导入与故障并发仍待。

## 候选实现与审查

[0027](../../engine/porting/wine-11.18/0027-winemac-reserve-host-status-and-cancel-known-tickets.patch)保持USER96布局，显式v2新增PREPARE_HOST、INSTALL_PREPARED_HOST、CANCEL_HOST。旧sender始终v1，prepared sender明确v2，回复版本必须一致；producer在发布前绑定协议。v2不允许旧单步INSTALL回退。

PREPARE先登记独立完成owner，再在Wine线程创建真实NT section；初始化失败的部分owner仍保留。调用方应先映射该ticket的只读页，随后才发送INSTALL。取消使ticket不可复活；完成页只在实际control/model/物理退役排空后发布SAFE。Cocoa仅登记待处理工作，不调用NT关闭。NT关闭失败保留精确部分owner，producer完成还等待宿主NT owner清空。

HS-P1-01：首次源码审查发现已发布SAFE、NT清理失败后，另一次取消会令重试跳过detach，却仍把默认NULL当作本次NT关闭结果。修为逐调用owns_cell，只有实际取得了该owner的调用才能回填/注销。HS-P2-02：幂等ACQUIRE未核对已绑定协议；现跨版本返回明确REVISION_MISMATCH且不改原lease。修后限定复审接纳。

动态页序号目前全0，SAFE读取values_valid=0；不虚构尚未验收的逐操作完成前缀。状态页包含完整session/root/producer出生时间、epoch、generation、lease、ticket及context身份；名称匹配不能替代页内身份校验。

## 实际正常流程

[客户端](../../tests/graphics/host-status-metadata/)在真实同进程Root上通过USER消息操作服务器，实际NT打开并映射只读页。

| 验收 | PE64 | PE32 |
|---|---|---|
| 正常元数据流程 | 28检查、0失败、CLI0 | 28检查、0失败、CLI0 |

实际覆盖协议幂等/错版本拒绝、只读保护、同名错误身份拒绝、v2旧导入接口拒绝、已知ticket取消、保留RO映射读取HostSAFE、旧ticket安装拒绝，以及producer完成页。原始page事件保留实际完整身份及SAFE/values_valid；没有把控制回复、NOT_FOUND或超时当作页完成。

context_id=1只作元数据；取消前未发送可用ticket的INSTALL。**这不是实际CAContext、图层、跨进程、GPU/compositor或游戏证明。**

首次PE64在CHECK18失败：服务器实际返回NOT_SUPPORTED=50，但其操作验证错误路径在写入root_pid/出生时间之前返回。客户端保留了拒绝不完整回复的条件；source-v5只把真实身份赋值移到公共请求校验之后、操作校验之前。限定独立复审及新模块双位宽重验完成；原失败记录未回写。

## 输入与剩余缺口

最终source-v5、Native build-v5，普通六槽PE/watch2配套，客户端v2/build-v3。NT库只对PE路径跳过Native config.h，函数体未另造一套实现。8组编译依赖/158个项目输入、两轮raw/session/EXE、组件、5份完整进程清理、原字节或缺失恢复和四个保护摘要另行绑定；见[证据](evidence/r-v1-host-status-metadata-20261031.json)。六文件补丁零fuzz、零offset逐字重建，未切用户引擎或默认路由。

清理并发故障夹具的子代理被平台自动安全审查拦截，原文仅提示可能存在网络安全风险，没有具体错误码。已关闭该代理；4个部分文件仅保留摘要，没有构建/运行，也未换代理或改名重试。**HS-P1-01的真实失败/并发运行验收仍未完成**；源码修复和正常流程不能替代它。本轮不宣称完整协议采用。

额外成本为每宿主一页NT资源、登记/清理控制及一轮准备请求；收益是INSTALL回复丢失后仍持有明确ticket和只读观察通道。实际INSTALL前先持有RO页的消费者及多处共享context的最终释放策略尚未接入。下一步按计划验证真实图层的已知ticket安装/取消，再到资源、事件reparent和两三进程像素；保留故障并发缺口及原HY/U8-15、Windows忙待重启和游戏门禁。Windows、Actions、定时、Goal零操作。
