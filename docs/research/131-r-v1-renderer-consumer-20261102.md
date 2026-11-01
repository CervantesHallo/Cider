# R v1：实际 D3D11 foreign HWND 前端

接续[130](130-r-v1-three-images-20261101.md)，本次从私有资源夹具进入普通应用实际调用的 legacy renderer 前端。host 拥有两个子窗口，另一位宽的进程仅通过标准 D3D11／DXGI 在同一设备上创建两个交换链，没有调用私有安装入口，也没有让 host 代画。

| 方向 | 实际检查 | 实际结果 |
|---|---|---|
| PE64 host → PE32 renderer | host 244／renderer 301，均零失败 | 两交换链、4 帧逐像素读回、两次窗口取图、本地资源释放、进程正常退出 |
| PE32 host → PE64 renderer | host 244／renderer 301，均零失败 | 同上 |

每帧真实读取 160×100 个像素。WindowServer 图像为 864×468，两个矩形各 69,776 像素，边界分别为 `[22,78,377,273]`、`[422,78,777,273]`，中间有贯穿共同高度的 40 列不透明白色间隔。红／蓝交换后仍分别处于预期位置，补上此前同色图不能证明的空间对应。四张实际图片与精确消费者源码在[renderer-consumer](../../tests/graphics/renderer-consumer/)。

0028 保留原 legacy ABI、120 字节快照和 managed surface 的 used／unused 缓存；远程资源附属于原 entry，没有新增 HWND 缓存。先取得已知 ticket 并映射真实只读完成页，再导入实际 CAContext；通知仅用于唤醒，释放仍依赖完成页许可。Cocoa 最后释放只排交接，NT／PE／缓存清理由 Wine 线程承担。真实窗口进程身份、出生时间和不复用 incarnation 每 250ms 独立检查，避免丢通知留下旧窗口关系。每进程最多一个消息线程，空闲阻塞；这个固定资源代价和轮询开销尚未作正式性能验收。

构建同时更新 Native 与两种 PE 驱动，普通 Unix 表由 6 槽增至 8 槽；不能与旧诊断表按槽数混配。补丁经过限定源码复审，13 个编译单元及配套链接通过；后续仅重编有变化的文件。公开 0028 从固定上游及 13 个既有图形补丁重建普通基底后，零 fuzz、零 offset 应用，24 个候选文件逐字一致，其中 15 个新增或修改。

首个运行失败揭示实验变量用了保留的 `CIDER_` 前缀，被 GUI 的环境构造规则过滤。修为 `WINEMAC_EXPERIMENTAL_REMOTE_RENDERER=1`，不放宽过滤规则；后续实际 session 环境单独核对。默认不启用。

首个反向运行已成功创建两交换链，但额外 `VirtualQuery` 检查将 Metal 映射地址报告为 `MEM_FREE`，故在读回前失败。该失败原样保留，内存元数据差异仍未修复。最终夹具依据成功的 `Map(READ)` 返回值，检查非空指针、pitch 和地址范围后真正读取全部像素，再 `Unmap`；不是伪造查询成功或略过像素检查。微软文档说明 [Map 返回资源数据指针及 CPU 权限](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)，[MappedSubresource 给出数据和行间距](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_mapped_subresource)。这个图形结果不证明 `VirtualQuery` 对宿主映射完整兼容。

两方向在 COM／GPU 工作排空后销毁 child，renderer 保持存活直到真实 Native 的两份 owner 均报告 `context0 routes0`，才放行最终 ACK／零退出／kernel join。这里的本地 owner 归还不等于所有 Cocoa 回调尾部、合成器或全局 GPU 资源均已退出。完整私有进程清理、实验瓶原配置与 DLL 字节或缺失恢复、四个受保护路径另留回执。

本项只接纳普通消费者的创建、像素对应和合作回收；独立复核未发现具体 P1/P2；父代理再核 419 个当前输入，1 项可变项目记忆仅作为历史阅读上下文单列，不冒称其旧字节仍可重现。详见[配套证据](evidence/r-v1-renderer-consumer-20261102.json)。下一步沿原图形迁移顺序验证同一交换链的实际跨 root 迁移与更新，然后做 CEF／启动器回归和可回退实验包。初始 local→foreign 转换、任意故障交错、物理显示、输入、正式性能及游戏会话均未由本项证明。日用引擎、游戏门禁、Windows 参考条件与原 HY 顺序不变。
