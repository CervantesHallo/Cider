# R v1：NT 清理失败与并发取消

日期：2026-11-01，UTC+8。接续[115](115-r-v1-reply-independent-consumer-20261101.md)，补当前 per-host 阶段此前未运行的指定竞态。

## 原问题与这次实际验收

[111](111-r-v1-host-status-metadata-20261031.md) 的 HS-P1-01 是实际所有权问题：第一次 NtClose 失败后，host 保留部分 NT cell；另一 CANCEL 在途时，恢复重试不能取得 cell。如果此时仅根据先前的 resource_done 处理局部空指针，会覆盖仍待清理的真实 owner。0027 已用逐调用 `owns_cell` 限定重新挂回／注销，本次补实际运行证据，没有更换原实现。

[夹具](../../tests/graphics/host-status-cleanup/)只对本进程创建的真实 section 设置句柄关闭保护。普通 CANCEL 后，真实 NtClose 返回 `0xc0000235`，原 cell、登记和 producer owner 保留。另一线程进入普通 CANCEL、实际取得 status_controls，再在锁外等待自有事件。普通 PE 恢复计时器执行重试，同锁观察器冻结决策：controls/resource_done/busy 各 1，本次 owns_cell/attempted/removed 各 0，原 cell 身份、登记及 owner 各 1。局部 closed=0 没有被当作实际 NT 成功。

随后释放事件、真实等待工作线程结束，只清除一次保护；普通恢复路径完成真实 NtClose，cell／登记／owner 归零，再释放 producer、读方和观察引用。观察引用只保对象寿命，不能代替丢失的 NT owner。资源 SAFE 与 writer NT 清理完成分别核对。

| 指定故障／交错 | PE64 | PE32 |
|---|---|---|
| 真实关闭失败 → CANCEL 在途 → 普通重试保留 → 自主排空 | 42 检查、0 失败、CLI0 | 42 检查、0 失败、CLI0 |

## 审查、构建与证据

独立审查发现两项 P2：BIND／ARM 碰到普通清理暂时转交 cell 的窗口；ARM 后工作线程没有启动会留下辅助状态。修后仅对明确 IO_PENDING 有界等待，并增加只清辅助借用的 DISARM；实际运行也覆盖未使用 ARM 的撤销和重新布置。复审接纳最终源码。Native coordinator 使用 `-DNDEBUG`，没有把引用更新放进 assert。

首次客户端 printf 类型警告、一次构建脚本漏拷贝对象均保留为失败记录；最终只复用已成功、字节匹配的对象，双 PE 严格编译／链接通过。8 组有效编译依赖闭包、104 个项目输入和最终组件绑定。另一解析器重新读取原始帧及 session／EXE，核对实际失败状态、同锁决策、真实线程返回、一次清保护和后续真实关闭；另核对 16 PE／2 Native／F27、5 份完整进程清理、原配置和 DLL 字节或缺失恢复及四个保护摘要。五文件诊断胶水无 fuzz／offset 精确重建。详见[证据](evidence/r-v1-host-status-cleanup-20261101.json)。

该实验 winemac.so 含诊断观察／调度点，明确不是默认组件；stage 继承的 diagnosticOnly=false 注释有误，原始记录保留，证据已纠正其含义。运行后按精确差异清单归档并清理 4 棵临时整树及 2 份前缀 EXE，保留源码、原始结果、差异组件和普通工作基底。

旧子代理的泛化工具错误仍是历史事实；本次父代理的源码、复审、构建和实际运行均成功，不能继续把这项具体故障验收标成未运行，也不据此推断客服处理结论。

## 接续顺序

本次是元数据 host 的指定 NT 故障／取消交错，没有实际导入 CAContext，没有运行前修复版负对照，不代表全部调度或整阶段完成。接下来仍在当前 per-host 阶段验证实际 INSTALL 在途／未知结果时的取消和资源保留，并补重入／legacy 交接等剩余运行证据；然后按原顺序进入多进程资源消费者、事件 reparent、两三进程像素。默认引擎、游戏评级和门禁不变，Windows 没有操作。
