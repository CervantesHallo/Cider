# R v1：完整 legacy 接口与首次本进程窗口呈现

日期：2026-10-25。接续 93/94 的窗口主题。全部操作在 Mac 私有实验环境中完成，Windows、用户引擎、生产 App 和用户瓶子未改变。

同日后续：注册表运行已由 [96](96-r-v1-registry-roundtrip-20261025.md) 接续。下列首次呈现阶段之后，新增的窗口生命周期检查也已完成，见文末；foreign-child/CEF、截图/输入、精确重入交错仍未验收。历史阶段的“待运行”不能覆盖这些后续结果。

## 实现与状态

| 范围 | 源码/构建 | 实际运行 |
|---|---|---|
| 24 槽、192 字节 native 函数表；120 字节快照 | 完整表已编译、链接、导出；偏移断言通过 | 原 DXMT 消费者实际进入新快照/Metal 前端，双 PE 创建并 Present 通过 |
| 现有 win32u 管理表面的登记、退役和缓存 | 0015/16 已修正所有权及延迟销毁，限定复审闭合 | 本轮只有本进程初次窗口；精确销毁竞态和重复重建仍待 |
| 注册表回调 v2，五个真实 API | PE32/PE64/native 单元和驱动链接通过，限定源审闭合 | 注册成功已随驱动加载；五个 API 的行为/故障往返仍待单独运行 |
| foreign-child/CEF、几何/Retina/视觉交互 | 保留下一项，未解除旧 foreign-child 拒绝 | 未验收 |

完整表保留现有 Highball 和用户导入 D3DMetal 的接口形状。快照真正持有 Cocoa view/window 和拖动事件的引用；没有将当前 Wine 私有窗口结构强转成旧布局。快照不再跨 renderer 调用持有私有窗口数据锁。新表面等待 renderer 创建成功后才显示。

新前端使用现有 win32u client-surface 列表和退役缓存，专用内部 format=-1 防止被 Vulkan(0)/GL 缓存复用。没有另建按 HWND 维持生命周期的替代列表；native token 表只管理快照及 renderer 的已有引用。重复释放、失败创建和 HWND 脱离路径均在限定源码审查中核对，不能由此宣称任意并发已实际运行。

同一 token 同时发起第二次 create 时，候选返回暂不可用，避免在主线程调用期间阻塞等待。每个正常 get_win_data 分配独立表面；该并发行为仍须与实际消费者对照，未作为完整兼容语义采用。

## 审查发现和修复

首次窗口源审发现两个 P2，原报告和候选均保留：

1. 缓存退役与 caller Release 之间，两个窗口更新可以重复消耗同一个缓存引用。修复在缓存淘汰和 HWND 销毁两条路径中，先移除/脱离列表，再消费唯一缓存引用；其他存活引用不会再次被缓存遍历释放。
2. 持有 surfaces_lock 的 finalizer 原先会同步等待主线程，期间 QUERY_EVENT 可重入窗口过程，再次获取同一锁。新 legacy 路径将 Cocoa view 和 Metal view 销毁排入主线程队列；没有在主线程异步块中再次调用同步 OnMainThread。对象所有者保持到相应块执行，现有非 legacy 路径保持。

父代理另补快照拖动事件的 Duplicate/Close，关闭解除私有锁后的借用 HANDLE 窗口。最终限定复审确认两项 P2 在源码闭合，无新增具体 P1/P2；正常路径的几何及显隐 helper 原本已经异步，未扩大为全 Wine 回调重构。

## 回调 v2 保留原功能

子任务首稿的 64 KiB 数据上限和非 NULL SECURITY_ATTRIBUTES 拒绝均没有采用。父代理重整为：

- 栈上只传 32 字节固定 envelope，数据位于显式持有、限制在 PE32 可访问地址范围的 VM 中。字符串、缓冲区和输出地址不直接跨位宽传递；数据用固定宽度字段和相对偏移描述。资源不足正常失败，未缩小调用者给出的查询容量。
- 原生绝对/自相对安全描述符、NULL 描述符、SID/ACL/control 和继承标志按相对表示封送，在 PE 中重建本地 SECURITY_ATTRIBUTES，再调用真实 RegCreateKeyExA。没有丢弃安全属性或改变访问检查。接口要求见微软 [RegCreateKeyExA](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeyexa)。
- 相同 PE 驱动及 callback 地址按进程生命周期固定，native 和两个 PE 驱动使用同一追加 Unix-call 编号及协议版本。原初始化/icon 布局保持。
- 源审又发现一次条件性 P2：真实 Open/Create 成功后若 VM 释放失败，原候选可能丢掉 HKEY 及 mapping 所有者。修复交付真实操作结果/句柄，另保存失败清理的 owner/base/size/status 并记录错误；后续每项操作最多重试一个 FIFO 中的清理，不重放注册表操作。该失败分支不被报告为清理成功。

修后限定源审无新增具体 P1/P2。实际 API 往返、描述符结果和释放故障注入仍待；不能把源码和编译结果称为完整注册表或 Windows 契约通过。

## 双位宽首次运行

使用原计划 M-A 的 `tests/graphics/d3d11-triangle.c`，源码没有修改；每架构运行 120 帧：

| 架构 | Device/Swapchain | feature level | Present | CLI |
|---|---|---|---|---|
| PE64 | HRESULT 0 | 11.0 | 120 帧、0 失败 | 0 |
| PE32 | HRESULT 0 | 11.0 | 120 帧、0 失败 | 0 |

两份日志都有新前端的 `macdrv_legacy_get_win_data` 标记，确认实际走了新 native 表。严格拒绝访问异常、Objective-C 异常、越栈、invalid block 和失败清理标记。本轮日志未见这些标记。

该原探针没有完整资源释放循环或屏幕像素 oracle；这次不证明视觉/输入、销毁重建、跨进程或游戏，也不把探针打印的 fps 当性能基准。驱动注册成功不等于五个 registry wrapper 均已调用。

## 构建和清理身份

本次受影响 native 单元和两个新模块最终使用显式 macOS15.4 SDK/min14 编译和链接；固定的其他组件未重新构建。早期未显式传 SDK 的准备对象未用于这次运行。Cocoa 单元的 10 条 SDK 告警逐一映射到未改的旧源码行；其余受影响 native 单元、两个 PE 驱动及回调编译零诊断。两个 native 副本签名前后所有文件支撑 section 摘要一致，严格签名验证通过。

固定 Wine loader 使用 dlopen(RTLD_NOW)；本机 dlopen(3) 明确此平台默认 GLOBAL。新导出和真实消费路径已经成立，未修改全局 loader 标志。最终 native/PE 组来自同一候选；原冻结 ntdll/wineserver 四摘要保持。

父代理独立核对两个 session、原始 log、16 个 prefix 模块及 2 个新 native 模块。每批 finally 确认 GUI 真实退出、至少两次空观察、无 pending，逐字恢复普通实验瓶配置/DLL；四个生产保护摘要保持。新候选为 `cider-lab-wine11.18-window-legacy-r1-x86_64`。

资料根为 `out/engine-dxmt-bridge-20261025`，公开指纹见[证据摘要](evidence/r-v1-legacy-window-20261025.json)。0015→0016→0017 在基础 0011 后 zero fuzz/offset 重建，共 15 个文件逐字一致。初版格式/语言边界编译失败、源审反例及旧源码均保留。

## 取舍与接续

采用现有管理列表与完整 legacy ABI 的收益是保留图形组件接口及窗口退役语义；成本是两个内部 native API、追加回调协议、进程期 callback 固定及延迟 Cocoa 清理。验收仍须包括回调实际往返、故障所有权、窗口反复创建/释放、位置/尺寸/Retina及 foreign-child；回退为保留的稳定引擎，候选不进入默认配方。

下一步继续上述运行缺口及原 CEF/foreign-child 迁移。U90 原 8/15 单元超 ±3% 仍未通过；原 HY1→HY2→HY3→HY4/HY5、Windows 忙/待重启和游戏门禁保持。此次不是完整 R v1、Windows 内核或三款游戏验收。没有新增定时任务、恢复 Goal 或消耗 Actions。

## 同日后续：显式释放与重建

公开 `tests/graphics/d3d11-window-lifecycle.c` 在同一私有 window-legacy-r1 候选运行，两种 PE 各 **1415 检查、4 个窗口、56 次交换链创建/释放、168 次 Present，零失败，CLI 0**。每个交换链执行初次 96×64、实际 ResizeBuffers 后 128×80、移动/隐藏再显示三个阶段；分别用不同颜色检查全部纹理像素。两种位宽的原始 trace 均有 56 次快照、56 次表面 detach 和 56 次表面 destroy。两种 HWND/renderer 释放顺序均覆盖。

首轮夹具虽然各 1247 项通过，独立审查仍发现两项 P2：final Release 后缺少强制延迟销毁的 Flush，且尺寸只信任 GetDesc，可能接受成功但未缩放的错误后端。父代理修复后重新编译两种 PE，补严格阶段尺寸/格式/采样布局和变化颜色读回；最终限定复审两项闭合、无新增具体 P1/P2，才运行上述 1415 项。Flip 替换需在最后引用释放后执行 ClearState/Flush，依据微软 [Flush 文档](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush)。没有加 sleep 或资源 pin 掩盖释放。

原夹具源码/1247 日志和两份审查保留；不追认原结果为最终加强验收。父代理独立核对最终 log/session、EXE、16 个 PE/2 个 native 模块、真实 GUI 退出/两空和普通实验配置恢复，四个保护摘要保持。资料为 `window-lifecycle-runtime-v2.json`、`window-lifecycle-independent-validation-v2.json` 及 `window-lifecycle-v1/review-v2.json`，均在同一私有资料根。

本轮证明所列本进程资源/API/纹理读回行为；detach/destroy 计数不是完整 native 内存无泄漏证明，纹理像素不是屏幕图像，未控制精确 QUERY_EVENT 竞态或测试同 token 并发创建。下一项仍是 CEF 跨进程子窗口及其几何/上下文退役；未改变原性能、Windows 和游戏门槛。

### 跨进程迁移准备（尚未接入）

限定只读设计已交付 `foreign-child-design-v1.md`，父代理核对旧 oracle、新 managed surface、SetParent 和实际消费者：Metal layer 只在创建时取得一次，不能用每帧 getter 驱动 reparent。候选继续保留原 token/lease/cache，以实际 root owner 路由 CREATE/UPDATE/RELEASE；窗口 owner 必须在 SetParent/DestroyWindow 通知原 host。先成功建立新 host，再按准确 context 退役旧 host。没有把旧私有布局或第二套 pending surface owner 搬入新引擎。

父代理已有 `foreign-geometry-v1/remote_geometry.c` 准备源：使用 child **client rect**，将完整源 frame 与祖先交集 clip 分开，显式隐藏真实零面积；防观察到的重复祖先，不照搬旧固定深度上限。显式 SDK15.4/min14 原生编译零诊断；尚未链接到驱动、没有运行验收，也没有解除 foreign-child 拒绝。不得拿该对象文件作为完成证据。

该方案的收益是保留客户端纹理尺寸和真实父子关系，避免裁剪时缩放整图；成本是 host/context 状态、窗口事件路由和额外裁剪状态。验收要分别覆盖两/三进程、带边框/嵌套、零尺寸恢复、排序、重建失败、先后销毁及 reparent；任意 region、混合本地/远程遮挡、context ID 复用和无重建 local↔foreign 转换仍是未解项。提案不进入默认配方，回退仍是原稳定引擎。所有子代理已关闭，继续以此接续点实施。
