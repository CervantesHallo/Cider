# R v1：安装在途时取消与资源保留

日期：2026-11-01，UTC+8。接续[116](116-r-v1-host-status-cleanup-20261101.md)，继续当前 per-host 安装／取消阶段，普通 0027 实现不改。

## 实际交错

普通 USER setup／PREPARE 创建真实导出 CAContext、已知 ticket 和 RO 完成页。新工作线程通过私有诊断入口进入普通 packet_producer_begin／install_prepared_host／end；普通函数真正取得 request、control 和 model_job 后，在锁外等待自有事件。主线程通过普通 USER CANCEL 取消。

取消实际返回 IO_PENDING。同锁原始帧看到 control/model_job/request、待退役、排队、登记及 producer owner 均为 1，resource_done／terminal 为 0。另一实际快照中图层／登记／选择虽均为零，Host RO 仍有效且非 SAFE，context 和读方继续持有。这验证了不能把物理图层暂时为零当作在途安装已经安全结束。

释放事件并实际 join 后，安装操作返回 INVALID_HANDLE，model_job／control 归零。之后才观察到实际 Host SAFE、模型各零和 producer SAFE，最后真实销毁 context、关闭读方并释放观察引用。未单独记录 commit 回调入口，不能用该错误码追认回调执行。

| 安装在途／取消用例 | PE64 | PE32 |
|---|---|---|
| 实际在途任务 → USER 取消待完成 → 保留资源 → 实际结束／SAFE 后释放 | 36 检查、0 失败、CLI0 | 36 检查、0 失败、CLI0 |

## 审查与证据

限定独立对抗性源码审查无剩余 P1／P2，尤其核对观察引用不加入完成 guard、主／工作线程共享状态、失败 ARM／CreateThread 清理、事件借用直到实际 join、NDEBUG 和时钟边界。初稿普通函数参数数目错误导致编译失败，源码及失败记录保留，修后严格双 PE 编译／链接通过。

最终组件对应两张 9 槽 Unix 表、160B 诊断帧和原 128B live 帧，watch v2，Native coordinator 使用 `-DNDEBUG`。9 组有效依赖闭包、128 项输入及最终组件绑定。独立解析原始日志／帧、EXE／session，另核对 16 PE／2 Native／F27、5 份完整清理、原瓶配置和 DLL 字节或缺失恢复及四保护摘要。首次核对脚本误把本例非零身份要求扩大为全高位；原始身份实际为 1／17／18，请求序号为高位。仅纠正核对脚本，未改夹具、原始结果或重跑。详见[证据](evidence/r-v1-host-status-overlap-20261101.json)。

[公开夹具](../../tests/graphics/host-status-overlap/)复用 112 的五个相同 include，六文件诊断胶水精确重建；没有重复保存相同 helper。完成后 3 棵临时整树、2 份前缀 EXE 按差异归档清理，保留源码、原始结果、组件和普通工作基底。

## 后续

这是同进程、已知 ticket、普通安装函数的指定交错。INSTALL 使用私有函数入口，CANCEL 使用真实 USER 传输；尚不是实际 INSTALL 消息超时／回复未知，也没有像素／跨进程／性能／游戏证明。下一项继续真正未知传输结果下的资源保留及取消，再补当前阶段剩余重入／交接，之后才进入原计划多进程消费者、事件 reparent 与两三进程像素。默认引擎和门禁不变，Windows 无操作。
