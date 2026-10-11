# HY1 启动环境档案记录

2026-10-11，继续research/35、36、46的入口一致性工作。只在Mac改源码/构建，没有新启动Wine、测试或Windows操作。

## 实施

WineRunner的LaunchPlan新增可选matchingEnvironmentProfile。记录当前runner的已接受CompatDB快照为实际启动目标匹配的id、revision、target、映射后Windows程序路径、序列化内容SHA256，以及profile环境键和最终值不同的键。审计行同步增加id/revision/hash及差异键；不新增环境值副本。

摘要是本机JSONEncoder sortedKeys对已解码Profile的哈希，包含该对象的完整内容；不是原JSON文件摘要、签名或安装来源证明。目标及最终环境从原启动流程取得，环境合并、瓶子级locale/sync、加载链及门禁校验保持。

launchLocked在现有权限/加载链校验及环境重建后重新计算记录，覆盖传入计划里旧的/手动填写的字段。没有匹配则清为nil；旧session缺可选字段仍可解码。这是诊断信息，不赋予程序执行许可，不把数据档案当内核能力证据。

## 对抗性自审

- 手写/反序列化计划不能用自己的profile字段冒充实际查询；实际argv首项、选定prefix映射与当前runner数据快照重新查询。
- SHA编码失败保留nil，不写假摘要。差异键来自最终env比较；相同值不能证明供应者，故没有把它称为未被显式覆盖。
- profile其他registry/winver/helper动作即使在摘要内，也不声称这些动作已执行；仅说明自动环境匹配。
- 安装步骤environment_profile是另外的显式来源；缓存安装器不会被扩大为HYP名称匹配，可能没有自动匹配记录。其继承/首渲染仍按research/36单独验收。
- 根launcher匹配记录不能证明HYP/CEF每个子进程实际继承；进程环境需要独立读取。相同revision改内容可见摘要变化，但没有新增签名/防回退机制。
- 可选字段不改变旧数据/未知匹配的行为。可整体撤回本独立源码修改，旧会话保留。

## 构建与待验

最终CLI/App构建均成功（合并退出码候选解码器后的最终构建8.15秒/9.22秒），debug app打包/ad-hoc签名成功（复用构建0.48秒）。没有新增/运行测试或真实启动，所以当前为源码/构建完成；旧JSON解码、手动计划字段覆盖及各GUI/CLI/安装入口的真实记录仍待运行验收。HY1、HY2/KMDF与三款兼容状态不提升。

WineRunner.swift SHA256：cbe38d1f4c115f769a4d38c971efde9aca8e456062e3d05ca3fc45d7c03021ab。
