# R v1：根窗口宿主协调器

日期：2026-10-25。接续 [95](95-r-v1-legacy-window-bridge-20261025.md) 的 foreign-host 基础；存储清理后的恢复位置见 [97](97-storage-cleanup-and-coordinator-checkpoint-20261025.md)。这是原 CEF 主题的一步，未完成跨进程呈现或默认采用。

## 普通源码 0020

`0020-winemac-root-local-host-authority.patch` 增加根实例的 epoch、宿主 ticket 和更新序号。它们使用不回绕的 uint64 序列，并绑定实际 HWND、保留的 Cocoa 窗口对象及根修订号。PENDING、INSTALLED、RETIRED 状态与安装/更新 pin 分开；关闭根或撤销宿主后，迟到工作失效，持有的引用仍在完成路径释放。

Cocoa 实际分配成功后才提交替换，失败保留旧宿主。更新在主线程再次核对代次、根修订和最新序号；接受或拒绝均执行 finally 释放。同步 Cocoa 调用不持有协调器或 Win32 window-data 锁。几何仍区分完整 client 图像与祖先 clip，坐标位移检查整数溢出。局部窗口的移动、重设父级及销毁事件已接入失效通知。

真实 CAContext producer factory 与自动宿主创建分开，旧有效 HWND 路径保持。**context ID 不是 producer 租约**：跨进程 producer owner、消息传输、进程退出与重设父级协调尚未接入，foreign-child 分支仍拒绝。

首轮源审发现 P2：命名空间耗尽时，仅返回失败会让旧排队工作继续使用有效的根修订号。修复将该根锁存为 revision0，并在锁内撤销已接纳宿主、锁外清理；不复位序列或伪造新身份。限定复审闭合此项，无新增具体 P1/P2。

七文件 zero-fuzz/offset 重建逐字一致。普通 native 编译与链接使用 macOS15.4 SDK/min14；Cocoa 的10条警告逐项对应原消息和原行，无新增警告。源码、链接和运行证据分别保留。

## 真实诊断运行

公开夹具在 `tests/graphics/remote-coordinator/`。诊断私有组为匹配的 native 模块、两种 PE 驱动和客户端，追加固定80字节 PE/native frame；不进入普通引擎。它使用真实 Win32 根/子窗口、CAContext、Cocoa model layer 和实际协调器 API。序列只向前推进，高位测试之后故意耗尽且不复位。

| 运行 | 检查 | 失败 | model snapshot | 分配/耗尽故障 | CLI |
|---|---:|---:|---:|---:|---:|
| 修后 PE64 | 32 | 0 | 9 | 各1次 | 0 |
| 修后 PE32 | 32 | 0 | 9 | 各1次 | 0 |
| 仅恢复旧耗尽分支，原 PE64 客户端 | 32 | 4 | 9 | 各1次 | 1 |

修后两种位宽均实际返回 epoch8589934592，ticket8589934593、8589934596、8589934599，确认高32位没有在 PE32 边界截断。覆盖分配失败保留旧项、成功替换、旧 ticket 更新/释放、持 pin 的迟到更新、根关闭重开、耗尽撤销及 finally 引用归还。负例精确失败检查27–30，其他项目和清理完整；不是把任意退出当作有效负例。

客户端和诊断源码经过限定对抗性审查，包括高位、实际 client 尺寸、调用者 producer 所有权与故障消费。父代理另核对三份原始运行、session、EXE、16个 PE/2个 native 模块、3次 commit/4次 retire，以及真实 GUI 退出、两次空观察、无 pending、普通配置恢复和四个保护摘要。

初版低计数 PE64 的32项曾通过，但控制器误保留旧 legacy-table 标记要求，整批被拒；原始失败及只接纳旧子集的旁证保留。最终增强运行另有完整回执，没有将旧结果提升为新范围。编译准备阶段的 include/spec/选项位置等失败也未删除。

这些结果证明所列 API、model 几何和所有权路径，**不证明屏幕像素、合成器提交、任意并发、跨进程租约或游戏**。

## 普通窗口受影响观察：整批仍未接纳

未插诊断导出的普通候选复用了原 `d3d11-window-lifecycle.c` 和原两种 EXE。两种 PE 都实际完成1415检查、4窗口、56交换链、168次 Present、零失败/CLI0；原 trace 各有56次 snapshot、detach、destroy，未见所列 native 致命标记。

但 PE32 后的第一次清理触发控制器断言，旧控制器在断言前未保存其原始清理回执，**原因未知，整批仍为 failed**。随后 finally 和本次收尾都确认 GUI 已退出、两次空观察、无 pending，普通配置和模块恢复，四个保护摘要一致。这足以确认本次清理后的安全状态，不能补造丢失事件或追认整批通过。

独立旁证只核对真实程序结果、模块/session/日志和最终状态。首次离线计数器误用了 trace 拼写，修正后的v2精确匹配实际函数名，旧v1保留；没有重跑游戏或夹具。后续控制器须先落盘完整 exit/stdout/stderr/parsed receipt，再判断清理结果；仅在明确新假设或改动下补缺口。

## 存储与接续

结束后将三套诊断引擎、四套 staging 和三份重复模块备份清掉，保留普通工作引擎。所有文件类型/摘要/权限先与保留基底逐项比较，差异字节进入按SHA去重的私有对象库；清单和 manifest 归档。完整暂存包不是必要的长期证据。APFS共享数据使本批实际只再回收约62MiB，不按目录逻辑体积报释放量。

公开指纹见[结构化证据](evidence/r-v1-root-host-coordinator-20261025.json)，原始资料在 `out/engine-dxmt-bridge-20261025/`，清理恢复清单在 `out/storage-cleanup-20261025/coordinator-batch-cleanup.json`。历史 stage 路径可能已经删除，恢复须使用保留基底、差异对象和实际摘要，不能只比较 manifest。

收益是保留原宿主创建/替换语义并明确撤销边界；成本是根实例状态、异步 pin 和成功后提交。候选不切用户引擎，回退仍是稳定版本。接续是先保留上述清理验收缺口，再完成 producer 租约/类型化消息/跨进程退出和 reparent，随后两/三进程真实呈现。任意 region、混合遮挡、完整 ABA、local↔foreign 无重建转换仍待。

原 HY 顺序、U90 的8/15性能归因缺口、Windows忙/待重启、反作弊门禁和游戏评级保持。此次没有 Windows、Actions、自动化或 Goal 操作。
