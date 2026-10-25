# R v1：类型化跨进程消息与 producer 进程租约

日期：2026-10-25。接续[98](98-r-v1-root-host-coordinator-20261025.md)的根窗口协调器，仍属于原 CEF/foreign-child 迁移主题。普通补丁0021已经重建、编译并审查；限定跨位宽运行已完成。foreign renderer 路由尚未连接，不能据此宣布 CEF 或三款游戏兼容。

## 实现与原有语义

普通源码增加固定96字节 USER 消息。32/64位都传递同一字段布局，线上没有 native 指针；请求/回复形状、操作、保留位和关联字段必须匹配。只允许同步定向调用，拒绝异步、广播和无窗口调用。接收端持有独立对齐副本，同进程跨线程也走复制路径，防止超时后引用发送者栈；嵌套接收各有自己的 frame。同线程同步调用使用本地副本。PID/TID 由发送路径记录，是来源观察，不构成安全认证。

producer 租约绑定真实根实例、进程对象/创建时间、单调64位 generation 和租约身份。打开的实际线程对象先核对 PID/TID，实际进程对象承担退出观察；不以 Unix PID 采样替代存活判断。相同低32位的不同高位代次仍不同。根关闭、命名空间耗尽或主动释放可撤销已安装和排队工作，既有 host 的成功替换后退役及异步 pin 语义保留。

进程等待在 PE Wine 线程执行；native/PE 使用固定宽度回调，普通 Unix 表共6项且两位宽一致。PE worker 持有实际 SYNCHRONIZE 进程句柄与 stop event，周期恢复检查使用实际 worker 线程对象。完成通知、worker引用账和内核终止状态分别处理，全部满足后才移除 registry；不能仅凭通知成功就认定线程/资源已释放。

清理失败保留 FIFO owner，逐项重试，不重放 acquire/install。STOP 回复的有效关联确认与 operation status 分开；未确认的 NOT_FOUND 不当作已清理。`cleanup_inflight` 覆盖原句柄移交、队列弹出和 STOP 调用，finalizer 在同一锁内检查并摘除；完成请求不等于资源已最终释放。计数更新不放在 assert 中，两种 PE 的 NDEBUG 编译都已通过。

## 审查和构建

限定对抗性源审先后指出无恢复入口、NOT_FOUND 回执来源丢失、worker死亡引用归还、FIFO饥饿、过早摘除及清理帧在途竞态。均已按原实现修复。最终 producer v6 与 fixture v2 复审在各自范围内没有新增具体 P1/P2；这不是全项目无缺陷证明。

0021的11个文件相对0020精确重建，无 fuzz/offset，逐字一致。native 显式 macOS15.4 SDK/min14，普通/诊断模块和两种 PE 分开留存；两种 PE 额外以 NDEBUG 编译核对。canonical Wine 核心没有被实际编译替换。准备阶段 `make -n -B` 意外触发配置并失败，仅 config.log 变化，原始失败保留；之后只使用记录参数或目标级 `make -n -W` 并重定向输出。错误地将 canonical M0 与实验 F27 摘要比较的准备失败也保留。

## 真实跨进程运行

公开夹具：[remote-transport](../../tests/graphics/remote-transport/README.md)。私有诊断组只加单调高位初值、只读48字节 owner observer 和 PE owner 计数，不注入故障、不开启 foreign 路由。

| 主/子进程 | 主检查 | 子检查（三轮） | 子退出码 | 失败 |
|---|---:|---|---|---:|
| PE64 → PE32 | 68 | 15 / 13 / 26 | 0 / 77 / 0 | 0 |
| PE32 → PE64 | 68 | 15 / 13 / 26 | 0 / 77 / 0 | 0 |

覆盖活进程主动释放、指定 active/watched 租约后的实际进程退出、generation增加 `2^32` 后拒绝旧请求/旧释放及新项幂等。每方向四个唯一租约为8589934604–8589934607，完成结果1次EXITED、3次CANCELED，error0。子进程位宽从实际执行者写回，与 CreateProcess 的实际 PID 及清理回执匹配；不从文件名猜测。

父代理独立重读原始log/session/EXE、16个PE和2个native模块及F27核心四件套；5份完整清理尝试全部确认GUI实际退出、两次空观察、无pending。普通实验配置与DLL（含原本不存在的文件）恢复，四个生产保护摘要一致。离线 validator 首次错误假设所有原DLL都存在，失败另存，改为比较原字节或原缺失后通过；没有重跑通过的跨进程基线。

这证明所列真实消息/进程对象及有界清理路径。没有注入失败-close、丢回执或非预期worker死亡，不能将源审闭合升级为这些故障已经实测；也未证明屏幕像素、完整堆、任意并发或性能收益。

## 原窗口回归和旧清理缺口

0020此前普通组的PE32清理缺口已用先落盘完整回执的新控制器定向补齐：同一EXE/模块1415检查、56交换链、168次Present均过，3份清理尝试全部接纳。旧失败原因仍未知，旧failed记录不改；已确认的PE64子集与新PE32结果分别绑定，不能把旧整批追认通过。

0021新的匹配诊断组也完成受影响原窗口夹具，两种PE各1415检查、56交换链、168次Present零失败，各56次实际snapshot/detach/destroy。第一轮PE64程序完成后，控制器用错trace名导致断言失败；原记录保留failed。独立核对实际函数名、原log/session和清理后复用其PE64子集，仅另跑未运行的PE32，双位宽证据绑定完成。不是重复跑到通过，也不是屏幕像素证据。

## 接续与采用边界

下一步是受控失败清理/worker终止对照，再连接真实 producer CAContext 资源所有权与 HWND 事件、跨根 reparent、成功替换后的旧host退役，最终两/三进程真实呈现。实际 route 接通前保持 foreign-child 拒绝。早期 acquisition 在 PE timer 建立前的失败可能仍依赖后续 USER 入口恢复；任意锁内终止和析构失败没有被本批证明安全。

收益是保留原窗口路径并用实际对象和完整64位身份约束跨进程生命周期；成本是逐根历史、worker/timer、回调和清理队列。性能尚未采用，回退保留普通稳定实验组。候选不切用户默认引擎，不改游戏评级/反作弊门禁；原HY顺序、U90的8/15归因缺口、Windows忙/待重启不变。无Windows、Actions、定时或Goal操作。

结构化指纹与原始记录索引见[证据](evidence/r-v1-typed-producer-leases-20261025.json)。原始资料在 `out/engine-dxmt-bridge-20261025/`；stage删除后须按确切组件清单恢复，不能仅凭manifest相同。
