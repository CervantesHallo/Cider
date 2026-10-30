# R v1：真实 producer 终端完成页

日期：2026-10-30，UTC+8。接续[105](105-r-v1-request-inflight-20261030.md)。将103/104原语接到普通producer生命周期，仍不启用foreign renderer或默认配方。

## 普通0024

在真实进程租约初始化中，返回ACQUIRE前创建当前session的NT section；身份含实际root/producer PID、birth、epoch、generation和lease。初始ACTIVE的零序号只表示bootstrap，不声称资源计量或逐请求进度。全部请求/model/process/cleanup guard排空后，普通finalizer发布RETIRED/SAFE，再在Wine线程关闭writer view/handle。关闭失败保留部分owner，原生watch不宣告完成。

发布在remote_lock下只做内存操作；关闭先同锁转移owner并增加cleanup_inflight，实际NT调用解锁后执行。raw Cocoa回调和destructor没有NT清理。SAFE_ONLY的values_valid=0，不是全资源归零、数值终端序号或PE最终ACK；writer NT清理可在安全标志之后继续。

实际成本为每producer一个4K section及生命周期控制，收益是读方view超过HWND和writer生命周期保留。当前只采用为实验候选，尚无per-host、未知安装/取消序号或renderer资源消费者；回退保留0023。七文件[0024](../../engine/porting/wine-11.18/0024-winemac-publish-producer-terminal-cell-and-own-failed-setup.patch)包括Native源登记/pragma、Makefile和模块，不依赖外部手工补对象。普通/NDEBUG、Native及两PE init/watch/link通过，完整Wine/release构建仍待；未切默认。

## 失败路径审查修复

PS-01：准备/START失败时没有PE watch，旧恢复依赖会断开。现先登记setup，再取得PE恢复定时器实际确认，之后才取得NT资源。Native pending覆盖setup、全部watch及队列；PE用锁内代次拒绝旧zero结果撤销新任务，空闲后可退休，没有常驻新线程/窗口。

PS-02：继承的早期NtClose丢弃返回值会丢失临时thread/process handle。现setup明确记录owner，只在关闭成功后清除，注册producer时同锁转交process；失败setup保留在主列表并由FIFO重试，避免新失败挤掉旧owner。每次只驱动一个，关闭不持remote_lock，active/busy防止误关正在使用的句柄。非局部退出跳过收尾仍未修复。

最终普通v4限定复审无新具体P1/P2；v5仅加入Native构建登记/config，生命周期/header/PE字节不变，由父代理核对并重建新cell对象及匹配模块，再执行以下最终运行。失败PE首稿误取旧Unix enum include的编译拒绝保留，修正优先级后匹配构建成功。原报告/失败不改写。

## 实际运行

[夹具](../../tests/graphics/producer-status-cell/)由真实普通acquisition创建页；bootstrap control只传身份。两方向各开相反位宽RO子进程，在RELEASE之前完成open/map并关闭自己的section handle。父持实际请求pin，真实typed RELEASE/STOP，锁内记录普通finalizer决定；Win32子/root窗口真实销毁后，才归还pin。

| 组 | 64→32 | 32→64 | 观察 |
|---|---|---|---|
| 修后 | 父29/0、读22/0、CLI0 | 同左 | 持pin时ACTIVE；窗口销毁后SAFE |
| 仅省略请求条件 | 父29/0、读22/1、CLI1 | 同左 | 仅读方CHECK16，过早SAFE5及native finalized1 |
| 真实NtClose失败/恢复 | 父33/0、读22/0、CLI0 | 同左 | c0000235保留owner，清保护后自主归零 |

正常初始/primary均READ_SNAPSHOT0，最终SAFE_ONLY5；RO实际PAGE_READONLY2。真实lease8589934608，高位完整；context由实际API创建但未导入Cocoa宿主，不当作安装重入/renderer证据。故障组用真实handle保护，让unmap成功、close失败，不伪造错误；只读op7本身不做清理，清除保护后由恢复机制使owner/watch/timer归零，之后才做最终清理。

PCF-01旧夹具单读可能赶在延迟发布之前，改为带前后时钟检查的纯只读轮询。PCF-02迟到成功清保护可能被cleanup重放，改为先记录真实副作用，NT flag操作另计控制pin阻止H被回收。最终限定复审无新具体P1/P2；新helper/reader与Makefile集成对象已重新运行，旧首轮不借用为最终资格。

独立核对6次raw/session/实际EXE、有效include/helper及Native cell新对象、16PE+2native/F27、14完整清理回执、普通配置/DLL恢复及四保护摘要；普通七文件、诊断和唯一负例均zero-fuzz/zero-offset逐字重建。见[证据](evidence/r-v1-producer-cell-20261030.json)。普通源/诊断复审与运行是分别的证据，父代理启动时未读到最终报告的顺序保留，不回写为先审后跑。

## 下一门槛

失败准备/START且无PE watch、早期临时handle真实关闭失败、FIFO公平及旧zero代次时序仍需各自实际验证；本次正常watch关闭故障不能替代它们。继续这些失败路径后，再做per-host、未知安装/取消、真实producer资源、事件reparent和两三进程像素。

没有全量泄漏、compositor/GPU、完整context-ID ABA、性能、原生Windows、CEF/游戏通过或完整R v1结论。原HY顺序、U8/15、Windows忙待重启和游戏门禁保持；Windows、Actions、定时、Goal零操作。
