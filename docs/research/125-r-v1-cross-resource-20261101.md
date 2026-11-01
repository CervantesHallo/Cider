# R v1：真实双进程资源交接

日期：2026-11-01，UTC+8。接续[124](124-r-v1-native-async-20261101.md)，按原顺序从per-host指定验收进入多进程真实资源消费者。旧parked草稿v1未改，仅新副本编译运行。

宿主拥有真实Root及自己的child区域，普通Windows CreateProcess启动另一PE架构生产者。生产者实际导出context，普通USER QUERY／ACQUIRE／PREPARE先取得已知ticket／RO，再一次INSTALL到另一个进程。宿主Native／Cocoa独立观察模型三项1；真实CANCEL／HostSAFE后再独立观察0，此时producer context仍持有；ProducerSAFE后才销毁context／关闭读方。

| 实际方向 | 宿主 | 生产者 | 结束 |
|---|---|---|---|
| PE64 host → PE32 producer | 55检查、0失败 | 28检查、0失败 | CLI0、kerneljoin／exit0，无强制 |
| PE32 host → PE64 producer | 55检查、0失败 | 28检查、0失败 | 同左 |

两端所有权字段分开，完整三握手绑定nonce／严格序号及实际PID／birth／epoch／lease／ticket／CID。宿主不能把收到的清理报告当退出，必须等待真实进程对象后读退出码，再fresh probe缺失、释放观察引用。受限HANDLE_LIST只有child pipe ends及有意输出，窗口／进程／管道均由本次实验创建。

一个限定代码子代理只实现客户端，父代理处理Native／组装／验证；独立审查确认无producer时成功及部分HOST_SETUP清理有P2。v4改以实际Native回执而非完整setup成功为准，允许未绑定观察引用清理，不要求失败的birth／epoch查询、不伪造模型缺失／SAFE。原错误仍失败；Native与客户端最终无剩余P1／P2，旧false报告保留，该分支未fault运行。

第一方向程序全部成功，但控制器误要求参数解析前两条日志已有nonce，原failure保持。修为每role最初两条checks可为0，之后全部完整记录与wire必须精确nonce／PID／身份，独立重判原始执行通过，未重跑；随后仅运行反向。这是核对规则修正。

严格双PE编译，Native NDEBUG，private160-v2／pipe200-v1／Unix8槽／watch2配套；9组有效闭包／131项输入绑定，10条旧Cocoa警告语句及消息一致。独立重读原始session／双EXE／角色全部checks、实际模型1→0、RO许可和kerneljoin；另核对16PE／2Native／F27、7份完整进程清理、两批原配置／DLL字节或缺失恢复、四保护摘要。五文件精确重建，见[夹具](../../tests/graphics/cross-resource/)与[证据](evidence/r-v1-cross-resource-20261101.json)。4棵整树、4份前缀EXE差异归档清理，源码／完整raw／组件／普通基底保留。

这里只用host自有child slot，未启用producer HWND的foreign-child前端；CALayerHost与contextID消费不证明像素／GPU／compositor接受或性能。正常合作退出不覆盖任意杀进程／失联／未知host终止。下一事件／reparent，再两三进程像素；默认／评级／门禁不改，Windows参考机无操作。
