# R v1：GPU 样本、异步结果与 TLS 销毁

日期：2026-10-25。接续研究 93 的实际失败。所有 Wine 运行均经私有 GUI/XPC，使用无账户实验瓶；未切换用户引擎、启动游戏或访问 Windows。

## 三条释放依赖

本次保持原有图形、样本转交、上传后备和工作队列实现，修正所有权结束的次序：

1. **CRT TLS**：原 MinGW `tlsthrd` 在调用析构前保留 TLS 槽值；GCC emutls 析构释放存储后，后续输出流静态析构再次取得填充为 FEEEFEEE 的指针。私有补丁先清除槽，再调用注册析构。若清除失败，不调用该析构；不声称此分支已完成清理。
2. **MF allocator**：最后 manager 引用原先先于缓存样本释放。Highball 共享纹理的析构还会通过弱父设备访问 `GetLocalD3DKMT`。0012 先销毁 allocator 自有样本，再释放 manager；0013 将 reader 已有 manager 引用保持到自有 streams/transforms/allocators/queued responses 清理结束。
3. **异步 result**：仅改前两处后，原 GPU 用例仍在第四条路径的共享纹理销毁中访问已释放设备。进一步源码核对发现 `rtworkq` 原先先释放 callback，再释放 result object 和 state。MF tracking callback 的引用就是 allocator 的引用；allocator 可以因此先释放 manager，而 result 仍持有最后的 sample 引用。0014 将 object/state 的释放移到现有 callback 引用之前，关闭这条跨对象依赖。

没有新增任意 sleep、测试侧设备 pin、虚假返回或取消像素检查。0013、0014 的最终限定独立源码审查均无具体新增 P1/P2；父代理另核对真实源码、模块、会话和原始日志。源审不能替代运行。

## 实际新旧对照

| 范围 | 旧输入 | 修后输入 | 可得结论 |
|---|---|---|---|
| 最小 C++ DLL，PE32/PE64 | 各卸载 fault=1、退出 1 | 各卸载 fault=0、退出 0 | TLS 槽释放缺陷可独立于图形和 MF 复现 |
| allocator 最后 manager 所有者，PE32/PE64 | 各在 final release 访问异常、退出 117 | 各 46 项、2 路径、0 失败、退出 0 | 缓存样本先于 manager 销毁的限定反例闭合 |
| 原 GPU SourceReader fixture，R2 | PE64 第四路径访问异常、退出 5；不继续 PE32 | R3 双 PE 各 575 项，4 路径，0 失败/TODO/跳过，退出 0 | 增加 rtworkq 次序修复后的受影响集成运行通过 |
| 同一 offscreen fixture，新后端组 | 复用原 fixture | 双 PE 各 46 项、2 模式、256 RGBA 像素/模式 | 共享/keyed 所有权边界没有观察到回归 |

TLS 的最终公开探针增加递归故障保护和不依赖日志的 watchdog 后，重新运行两架构新旧四项对照，未借旧探针结果。DLL 源及旧/新 DLL 保持；每架构替换的是同一个 CRT 对象，归档其余各 30 个成员逐字相同。

allocator 的初版观察器在终止前还尝试日志和栈回溯，独立审查指出故障时可能阻塞。最终 v2 保留观察记录，递归异常立即终止，独立 watchdog 不再经过 CRT 日志；普通故障日志仍可能阻塞到 watchdog 终止。双 PE 新旧四项对照使用该最终源重新执行。旧源、旧审查与原始运行均保留。

原 GPU fixture SHA256 `295181cd48d35de4432fc48d631333edfe3cf0f6367c75f0c08f067797810700`、两份 EXE、样本身份、sharing flags、元数据与全部像素检查未改。R2→R3 只增加两种 PE 的 rtworkq 模块修复。运行验收同时拒绝访问异常、Objective-C 异常、越栈、invalid block 等日志；不能只看普通断言和 CLI 退出。

R2 增加观察 handler/trace 的另一诊断轮曾 575 项通过，这是改变取证时序后未复现，单独保留，不用来覆盖无 handler 的 R2 失败。R3 使用原 fixture，在相同 heap/SEH/module 日志条件下双 PE 通过。以上仍不追认所有早期无 trace 退出 5 的精确栈归因，也不证明任意生命周期竞态已经覆盖。

## 构建、清理和证据身份

- reader、rtworkq、mfplat 改动各完成双 PE 完整翻译单元 Werror 编译和组件链接。reader/rtworkq 的补丁以 zero fuzz/offset 重建后与构建源逐字一致；allocator/CRT 原重建证据复用。
- Highball 设备构造次序修复的完整 PE 依赖闭包先前已构建。本次只重链接该组，使用私有 CRT 对象；不改 Metal library、native winemetal 或冻结 F27 Wine 核心。原 unsupported callback 的 no-return 警告仍列为采用前待修，不称整组零告警。
- 每批在 finally 确认真实 GUI 退出、至少两次空观察、无 pending，再逐字恢复普通实验瓶配置及受影响模块。生产 App、Windows 账本、用户 Steam/HYP 配置四摘要保持。
- 新私有候选为 `cider-lab-wine11.18-mf-gpu-cleanup-candidate-r3-x86_64`。父代理重新读取原始日志及 session、核对 14 个模块的安装身份和全部摘要；没有以代理报告替代验证。

原始资料在 `out/engine-dxmt-bridge-20261025`，公开记录为[证据摘要](evidence/r-v1-gpu-teardown-20261025.json)。初版/诊断失败不覆写。

## 取舍与后续

本次采用现有引用覆盖所依赖对象的销毁，收益是关闭已识别的 use-after-free 顺序，成本是 callback/manager 析构稍后执行；没有新增长期引用。验收依据为上述限定新旧对照、原集成用例与独立源码审查；回退保留原组件及全部实验版本。候选尚未进入默认配方。

这些结果是 Mac 上的限定组件运行，**不是完整 MF/CRT、Windows 内核、完整 R v1 或游戏验收**。完整 legacy 窗口桥接、foreign-child/CEF 及真实窗口运行继续按研究 93 推进。U90 原 8/15 单元超 ±3% 仍未通过，不重跑无变化基线或修改门槛。原 HY1→HY2→HY3→HY4/HY5、Windows 忙/待重启和游戏门禁保持；Windows、Actions、定时及 Goal 均未操作。
