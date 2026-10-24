# R v1 MF：保留上传后备，完成耗尽读取与 Flush

日期：2026-10-25。继续计划 02 的 Highball0010 媒体主题。当前是非默认候选；没有切换用户应用、引擎、瓶子或游戏，也没有 Windows 操作。

## 为什么没有照贴旧补丁

原补丁来自 [Proton 的 sample allocator 修改](https://github.com/ValveSoftware/wine/commit/164af86d)。它删除 reader 的第二次分配与复制，让调用方取得 processor 自己的 sample。Wine11.18 的逐项审查发现必须补三个前提：

- 原生 CPU 视频直通且没有 transform 时，仍需要原来的 GPU 上传后备；不能把它一起删掉。
- 调用方保留 processor 的有限池样本后，真实 EMPTY 需要完成读取，并在后续读取中先取已有输出。否则会一直等待，或提前取新输入、漏掉结尾帧。
- 此基线仍调用 `MFLockSharedWorkQueue`，因此保留配对的 `MFUnlockWorkQueue`。

Microsoft 的 MFT 标志区分 transform 提供样本和调用方分配；设备管理器也与解码/视频缓冲关联。这些接口约束用于限定迁移，不能从设置一个 manager 推定任意输入都已成为 GPU 视频。[MFT 输出标志](https://learn.microsoft.com/en-us/windows/win32/api/mftransform/ne-mftransform-_mft_output_stream_info_flags)、[Source Reader manager](https://learn.microsoft.com/en-us/windows/win32/medfound/mf-source-reader-d3d-manager)。

## 最终源码范围

`engine/porting/wine-11.18/0010-mfreadwrite-video-processor-sample-allocator.patch` 变更三文件：

1. reader 保留原分配/上传与工作队列生命周期。只有 transform 链已提供单个真实 D3D11 texture，且满足调用方 sharing flags，才转交原 sample，避免二次复制。D3D9、原生直通和不满足条件的缓冲继续走原后备。
2. 每个 transform 用保守 `output_pending` 标记，在取新源帧前探测到 NEED_MORE_INPUT；真实耗尽返回错误完成，不在 EMPTY 上自旋。EOS 优先已有响应；后续读取可取出仍保留的输出。
3. processor 实际处理 Flush：释放外部包装帧；若是内部复制帧，则回收到 `input_frame`。重复空 Flush 不毁掉下次复制所需的存储，也不销毁协商类型和 context。
4. 响应节点分配失败传播真实错误，撤销本次读取的 admission。异步失败用自己的 native error 回调完成，保留旧响应及已有 SAMPLE_READY 的所有权，避免两个请求只完成一次。
5. Makefile 增加静态 `dxguid`，解析新增 texture IID；不是新增运行时组件。

原始 reader、processor 和 Makefile 已从固定归档核对；三文件 zero fuzz/offset 精确重建。reader/provider 各两 PE 完整翻译单元在实际 Wine GCC 参数和 `-Werror` 下通过，四模块链接通过。native `HAVE_FFMPEG` 未定义不代表 PE processor 不可用：这里链接的是 Wine 自带 `libs/ffmpeg`，后面的真实像素验证也证实了该路径。

收益是保留原能力，并消除已复现的等待、旧帧及完成计数问题。成本是每 transform 增加一个内部状态标记，以及取新输入前的输出探测、GPU sample 的接口/属性核对。免复制的实际 GPU 性能收益尚未测量，不宣称 FPS 改善。回退使用原 Media Bundle 引擎/原未修改模块；候选未进入发布配方或默认策略。

## 实际受影响运行

运行通过原独立签名 GUI/XPC，使用 F27 冻结核心和自有实验瓶。没有在登录过 Steam 的用户瓶副本中启动 Steam。

| 范围 | 实际结果 | 对照/限制 |
|---|---|---|
| SourceReader 同步/异步耗尽、明确重试、Flush、Release | PE32/64 各 **300** 检查，0 失败、0 todo、0 跳过 | 原 Media Bundle 命中一次输入/EMPTY 后，读取未完成，被原 20 秒 worker 期限以 125 拒绝 |
| 实际软件 processor 的复制输入与 Flush | PE32/64 各 **63** 检查通过；新亮帧 64 字节、首 RGB 253/253/253 | 原 processor 忽略 Flush，后续输入返回 `0xc00d36b5`；不是完整色彩、视频播放或 GPU 验收 |
| 两个 async admission 遭遇响应分配失败 | 修复诊断模块 PE32/64 各 **99** 检查，两个回调各完成一次 | 旧 v6 诊断模块各仅 1 sample 回调/0 OOM，5 个断言失败；修后 1 sample + 1 native OOM |

普通耗尽夹具用 opt-in fake MFT，通过真实 SourceReader 执行。它验证状态/完成和显式重试，不能当作物理 GPU 池的测量。实际 processor 夹具使用 4×4 NV12 的非 16 字节行宽触发复制后备；先保留未输出暗帧，再 Flush 两次，不重新协商类型，输入亮帧并验证所有 RGB 像素亮度。这覆盖内部存储回收，阈值不作为色域校准。

分配失败使用两份专用诊断 reader，分别基于旧 v6 所有权分支和修复 v7。只让前两个 EMPTY-status 的响应节点分配返回 `E_OUTOFMEMORY`，普通成功帧 Q 不受影响；同一架构的 EXE、相同冻结核心和相同故障机制对照。两次实际故障日志均为 `index=1/2, requests=1`。旧分支撤销当前 admission 后取走 Q，既有 SAMPLE_READY 再无响应；修复分支不取 Q，自己的错误回调和 Q 的预约分别完成。回调顺序只记录，不要求跨线程全局 FIFO。

诊断模块不发布、不选为普通候选。它证明报告中的精确计数反例得到修复，不代表任意系统内存不足、独立单个失败请求、所有并发 teardown 或 Windows 原生契约都已通过。Microsoft 允许异步错误通过回调报告，且回调实现须线程安全；本轮没有用回调返回成功掩盖 native OOM。[ReadSample](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-readsample)、[回调接口](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nn-mfreadwrite-imfsourcereadercallback)。

## 审查与保留的失败

限定独立源码复审发现的问题均保留原报告：v1 的上传/有限池缺口，v3 的嵌套输出、EOS 顺序和 provider Flush，v4 的复制存储回收及响应分配错误，v6 的 callback reservation 丢失。父代理修复最后的 v7 分支，并用上述双位宽故障对照核对；没有把旧报告改成“全范围无问题”。独立审查报告不是运行证据，最终源码和实际模块分别绑定。

首批夹具编译有头文件配置、GUID 导入库和 testlist 声明错误，均保留，最终使用实际 Wine 头/导入库成功。首次运行误用产品保留的 `CIDER_` 环境前缀，未进入限定分支，退出 5 并缺少原完整套件资源；它不算有效对照。已改为 `MF_FIXTURE_SCOPE`、缺失立即失败，并核对实际 session.env。产品保留前缀过滤没有放宽。

第一次 OOM 记录确实出现了丢回调，但默认日志没交付要求的故障标记，控制器按未满足证据拒绝。原件不改；启用明确 `warn+mfplat` 后另建 v2 记录，四个旧/新、双位宽结果才满足全部检查。没有重装已核对一致的诊断引擎，没有通过挑样更改性能结论。

每轮都有真实 GUI 退出与两次空观察。诊断后原普通实验配置和 DLL 字节已恢复；四个受保护摘要一致。两份故障引擎在确认无瓶子引用后移出活动 Engines 到可恢复的实验归档，防止以后误选。原连续小时、U90、Win32/WMA/mixer 通过组没有重跑；Windows、Actions、定时和 Goal 均未操作。

夹具的固定源 patch 与构建入口保存在 [tests/media/mf-source-reader](../../tests/media/mf-source-reader/README.md)。公开生成器仅准备核对了一次，与实际执行 v9 源字节完全一致；既有相同源的编译/运行证据保留，没有为同一源重复编译。资料在 `out/engine-mf-allocator-20261024`，公开摘要见 [证据](evidence/r-v1-mf-allocator-20261025.json)。

## 尚未完成与下一步

实际 D3D11/GPU 共享纹理、免复制分支、完整 MF/视频、全部 Drain/重协商/并发生命周期和游戏会话仍待；不据本轮升级游戏评级、解除门禁或默认采用 R v1。继续原 GPU/共享纹理和 CEF/窗口主题；U 的 8/15 超 ±3% 保持未过。Windows 仍待用户可重启，不回连或改机时。
