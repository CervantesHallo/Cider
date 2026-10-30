# R v1：请求在途所有权与原生完成

日期：2026-10-30，UTC+8。接续[104](104-r-v1-status-cell-stress-20261030.md)，修复接入producer资源前的请求完成边界。

## 普通候选0023

原busy布尔值无法向finalizer表达仍持有资源的请求。外层安装尚未返回时，允许进入的撤销请求可能完成STOP和原生清理；待安装host尚无已安装model，pending_retirements也不能代替请求所有权。

[0023](../../engine/porting/wine-11.18/0023-winemac-retain-inflight-producer-requests.patch)改用request_inflight：成功准入同锁递增计数、消费完整64位序号并持有一个引用；非撤销请求继续串行，撤销拥有自己的独立pin，计数饱和拒绝而不回绕。所有成功begin路径在同步操作和失败安装处置返回后，通过统一end归还计数，再尝试finalize、释放原请求引用。非撤销请求被撤销时仍返回真实PROCESS_ABORTED。

普通finalizer与destructor增加请求已排空条件；没有新等待、锁内NT/Cocoa调用或成功空桩。所有权修改在assert外。最后一个请求end主动重试finalize，避免只等无关USER流量。它补充0022的model移除栅栏，没有代替该栅栏。

收益是撤销不能让仍可能使用context的请求过早得到原生终端完成；成本是每个typed请求的计数准入/归还及保守保留。候选未进入默认配方。回退保持0022实验参考，foreign路线仍关闭。普通source限定独立审查无新具体P1/P2；SDK15.4/min14普通和NDEBUG编译、普通native链接成功。诊断PE首稿遗漏Unix enum重命名而失败，原记录保留；修后匹配组完成链接。普通源码没有诊断hook、种子或导出，native ABI和96B packet不变。

## 实际所有权对照

[公开夹具](../../tests/graphics/remote-request-inflight/)获取真实self-process租约、实际创建CAContext，持有普通INSTALL准入pin，再通过实际Win32消息提交更高序号RELEASE，执行真实STOP和完成路径。它没有执行host_install；不得称作实际Cocoa安装重入或renderer呈现。

| 组 | PE64 | PE32 | 同锁决定记录 |
|---|---|---|---|
| 修后 | 20/0，CLI0 | 20/0，CLI0 | 请求1，watched1/finalized0 |
| 仅省略请求保护 | 20/1，CLI1 | 20/1，CLI1 | 请求1，watched0/finalized1；仅CHECK12 |

两种位宽分别执行。其他cleanup/model计数0、process已关闭、completion_requested1；lease8589934607、sequence8589934603的高位完整，context ID来自实际API。随后相同end路径返回PROCESS_ABORTED，实际清理native/PE watch/timer、context和窗口。所有20项与清理均完成，负例不是任意退出失败。

私有hook位于普通finalizer分支之后、解锁之前，只选择实际held producer、请求1、其他资源0的首个决定，记录实际字段，不复制或控制普通判据。负例PE32记录decision_detached0，表示早前已finalize，随后记录到仍有一个请求；这同样是原生不变量违规。**不声称PE已经收到终端ACK。**

## 两项夹具问题与最终重验

独立审查RIF-01指出，旧PE完成调用计数不能排除后一次调用尚未执行finalizer的瞬间，负例可能漏判。改为上述同锁决定记录，88B frame升为version2。另一个旧控制器错误地要求负例的PE返回成功；实际可能先返回PENDING、随后撤销end推进原生完成。原failed记录和单独更正保留，不改写旧结果。

RIF-02指出新增异步hook读取held槽后，原串行夹具的无锁清理转交成为数据竞争。最终begin使用局部输出，再同锁发布；op3/4同锁取出并清槽，解锁后归还同一个引用。header、PE及client字节未变，仅重建受影响native单元和链接。最后限定复审无新增具体P1/P2，最终两位宽修后/精确负例按新helper重新验收。旧快照可以绑定其实际值，但旧夹具不提升为最终确定性验收。

父代理独立核对最终4次raw/session/EXE、有效include helper、16PE+2native/F27核心、两变体共同PE及唯一guard删除、10份实际GUI退出/两空/no-pending回执、普通配置/DLL原字节或缺失恢复与四保护摘要。普通/诊断/负例补丁zero-fuzz/zero-offset重建逐字一致。详见[证据](evidence/r-v1-request-inflight-20261030.json)。父代理启动限定运行时尚未读到最终独立复审，不追写为先审后跑；最终源hash与运行有效输入已绑定。

## 接续与限制

实际清理并归档5旧引擎、5stage、10份冗余备份及24个prefix EXE副本，差异二进制按SHA保留，完整类型/权限清单和manifest另存；观察空闲增加约221MiB。保留fixed-r3工作组与必要Cocoa参考，普通MF Allocator R3恢复，四保护摘要不变。清理后的旧路径不是可直接重用的运行输入，恢复须查证据中的清单/CAS。

下一步接入真实root/producer/host状态发布和资源所有权，处理未知安装结果与取消序号，之后推进事件/reparent、两三进程像素。103/104的完成页仍是独立原语；这次实际请求计数不等于已接入它。

同步调用/dispatch不能由轮询取消，外层60秒控制器负责终止和实际清理；非局部退出若跳过end会保留pin，本补丁没有新增遗弃帧恢复。原生finalized也不等于所有普通引用、异步update pin、compositor/GPU和完整context-ID ABA均排空。没有全量泄漏/性能、原生Windows、CEF或游戏通过结论。原HY顺序、U8/15、Windows忙待重启和门禁保持；无Windows、Actions、定时或Goal操作。
