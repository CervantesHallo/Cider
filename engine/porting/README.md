# Engine R v1 主题迁移

2026-10-18，按计划02在v0 Steam通过后开始。此目录是新基线的源码准备，当前不作为可运行配方，不切换用户引擎。

`r-v1-inputs.json`固定Wine11.18 tag object及实际commit、只读官方mirror来源、首个源码文件摘要，并列出当前5份CX配方的21个唯一补丁、主题、实际采用关系和迁移状态。GitHub返回的tag签名验证结果原样记录，不能把固定commit或TLS下载说成已独立验证签名。现有CX源包哈希和各补丁SHA256作为oracle保留。

| 主题 | 当前来源 | v1必须保留/核对 |
|---|---|---|
| 用户目录/命令行/组件覆盖 | Highball0001/0003/0006 | 原瓶子路径、参数语义和DLL查找优先级 |
| msync | Highball0005及CX实际实现 | CX msync依赖整体核对，瓶子级同步；不能只贴yield补丁 |
| macOS/WoW64运行时 | Highball0008/0009/0011/0013、Cider0006 | TEB/QoS/异常/loader布局与新基线逐项比对，保留Wine身份 |
| CEF跨进程/窗口client区域 | Highball0007、Cider0003–0005 | DXMT子窗口及顶部完整性；原r2证据不自动转给v1 |
| D3D9/GL | Highball0002、Cider0002 | OpenGL选择和COPY交换，P-3读回性能另评估 |
| 媒体/音频 | Highball0010、Cider0007/0008/0014 | MF/DMO真实结果、冷启动和设备能力；原mm2证据保留 |
| 子进程策略 | Cider0001 | 相同host deny语义与真实失败1260，匹配native链通过后才登记EnginePolicy |
| x87sidecar | Highball0012 | 继续显式实验、默认关闭；不作游戏兼容依据 |
| kernel对象候选 | 0009/0012/0013/0015–0020 | 真实所有权及原生对照；新base重新生成协议表，不能照搬1810和CX生成头 |

## 已完成的四个源码迁移

`wine-11.18/0001-ntdll-preflight-policy-deny.patch`移植既有Cider0001。preflight helper与CX oracle函数体完全相同，调用位于PE解析成功之后、选择machine和创建进程之前。新代码只新增同一策略函数和检查，不修改普通API访问判断或游戏裁定。

另已迁移Cider0002的D3D9/GL COPY交换评分，以及Cider0007的DMO输出标志提前清零。前者保留原格式约束/评分关系，后者保留新上游的STREAM_CHANGE处理、buffer数据和真实HRESULT，仅调整旧状态清理时点。

另保留Highball0003的组件覆盖目录优先级：`WINEDLLPATH_PREPEND`在引擎builtin之前，原`WINEDLLPATH`仍在之后，空段继续沿原分隔规则忽略；没有增加隐式当前目录查找。

四个补丁均在固定commit的相应源码临时副本上以zero fuzz/offset应用，结果字节与预期一致；逐文件/补丁摘要及状态在输入JSON。父代理核对新增/替换块、原错误分支与资源释放顺序；没有编译整个Wine、执行迁移后引擎或提升兼容状态，不能据此称v1已可用。

## WinMM迁移的实际差异

固定Wine11.18的waveform.c已改变初始化：不存在旧WINMM_GetSupportedFormats初始化探测，InitOnce回调签名及enumerator归属也不同，格式掩码采用另一套映射路径。旧0008及依赖它的0014不能直接照贴；新base未包含旧0008引入的那段发布锁，并不等于新worker/callback组合已经证明无死锁。

输入JSON将这两项改记semantic-rebase-required，而非“已上游修复/等效通过”。后续需按实际格式转换/查询结果、枚举错误回滚和冷启动锁序重整，保持原能力与真实返回；现阶段没有变更用户在用mm1/mm2的实现或采用状态。

## 同步依赖核对

CX与固定上游已经共享get_sync/inproc缓存模型；迁移需要保留该所有权/权限路径。上游object_ops已改为指定字段初始化，CX的macOS分支不能连旧位置表整体复制。共享内存索引与Unix fd的收发、关闭及协议生成必须成组处理；WaitAny/WaitAll与APC返回也须明确映射。单独移植Highball0005的yield不足以得到msync。当前只完成具体依赖核对，尚未修改同步后端，逐文件摘要在输入JSON。

## 后续次序

先核对CX相对上游的macOS同步/loader/WoW64依赖，再逐主题迁移和构建；保留v0作oracle。v1完整native链构建及受影响验证完成后，才能新增精确EnginePolicy配对和非默认引擎配方。原HY1→HY2→HY3→HY4/HY5顺序不改变，Windows预算与人类交互缺口单独保留。
