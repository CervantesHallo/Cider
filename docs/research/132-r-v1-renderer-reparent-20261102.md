# R v1：同一交换链跨根窗口迁移并返回

接续[131](131-r-v1-renderer-consumer-20261102.md)。host 拥有两个真实顶层窗口及两个 child，renderer 保持同一 device／immediate context／两份 swapchain，用普通 `SetParent` 让两个 child 依次 Root1→Root2→Root1。没有私有迁移、手工刷新或重建交换链。

| 方向 | 实际运行 | 窗口与资源 |
|---|---|---|
| PE64 host → PE32 renderer | host 501／renderer 635 检查，均零失败 | 6 次全像素读回／Present，三次取图，零退出／kernel join |
| PE32 host → PE64 renderer | 同上 | 同上；外层清理的首次未知结果另列 |

实际图案为红蓝→蓝红→红红，第三种区别于先前两幅，避免返回原窗口时将旧图算作新图案。各矩形仍为 69,776 像素，中间 40 列贯穿白间隔，第三幅共 139,552 红像素、蓝像素为零。两方向各有三张[真实 WindowServer PNG](../../tests/graphics/renderer-reparent/)。捕获尺寸原样保留：高度均 536，宽度随阶段为 880 或 884；本项不声称宿主外框几何完全不变，客户区与色块检查独立通过。

不是仅靠截图或休眠判定迁移。0028 的只读诊断增量在成功切换实际 route 后记录同一 context、新 root／lease／ticket／generation，在 `retire_route` 真正返回后、释放其本地记录前记录实际 SAFE 位。第 2／3 次截图前分别观察两／四次精确迁移及旧关系退役；最终 gate 前六个精确 `(child,CID,root,lease,ticket)` 均已退役，两份 renderer owner 均 `context0 routes0`。本次六份实际退役的 host-safe／producer-safe 均为 1。之后才允许最终 ACK、renderer 零退出及 host 对真实进程句柄的 join。不同对象全宽身份不被截断。

限定源码复审发现父控制器有一项 P2：Native `%p` 的合法符号扩展 USER 句柄与 wire 的 canonical32 表示会被误判为不同对象。修后仅 HWND／Root 接受合法零扩展或符号扩展，拒绝任意高位及保留值；CID、lease、ticket、generation 保持完整宽度。原报告保留，父代理针对最终字节闭合此项。未实际分配 bit31=1 的 USER 句柄，不将源码修复说成该指定运行已通过。

首轮在迁移前遇到非阻塞 `Map` 的 `DXGI_ERROR_WAS_STILL_DRAWING`，旧夹具直接失败，原件保留。最终仅对此 API 明确允许的暂未就绪结果在既有 5 秒／10,000 次上限及全局期限内重新调用实际 Map；每次保留 HRESULT，其他错误立即失败。成功后先记 owner，再检查时钟，迟到成功仍拒绝且保留 Unmap 清理。实际逐像素读取、GPU event、Present、对象身份、图像门禁和回收条件均不删除。依据见[微软 Map 契约](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)。

反向运行功能及 CLI 均零退出，第一次私有 GUI 清理查询却返回 `kernelIdentityReadable=false`，所以原 `runtime-v5` 整体仍为 failed。finally 的真实清理已确认两次空观察、无 pending、GUI 退出及原瓶字节恢复；父代理随后对该旧 GUI PID 两次查询均获内核 `ESRCH`。最终绑定另行接纳功能和清理，不把第一次未知改成成功、不重跑整批挑结果。正向运行清理直接通过。229 项源码输入、原始 session／日志／双 EXE／Native／配套 PE／ready-gate／六张 PNG、四个保护摘要及回退状态见[证据](evidence/r-v1-renderer-reparent-20261102.json)。

这补上普通前端的指定跨 root 往返及真实合作回收。它不证明所有 Cocoa 回调尾部、全局 GPU 空闲、任意故障竞态、初始 local→foreign 转换、resize、CEF、输入、正式性能或游戏会话。下一仍在原图形迁移阶段补生命周期转换与更新，再做 CEF／启动器回归和可回退实验包；日用引擎、门禁、原 HY 顺序和 Windows 待重启条件保持。
