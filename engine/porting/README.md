# Engine R v1 主题迁移

2026-10-18，按计划02在v0 Steam通过后开始。此目录记录新基线的源码迁移及组件构建，当前不作为可运行配方，不切换用户引擎。

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

## PE C++工具链

Wine11.18的configure已实际通过，但明确报告两架构缺少C++17前端。现有GCC15.2工具链是C-only；不能以configure返回0当作所有模块均启用，也不能因缺依赖更换为曾导致Steam登录故障的LLVM后端。

`engine/pe-cxx-toolchain.sh`从同一固定GCC15.2归档构建两个独立C/C++前端prefix，保留原C工具链及其CRT/libgcc。源码逐文件/路径集合校验，处理APFS的Unicode等价名称并拒绝碰撞；编译对象使用独立、固定SDK的目录，实际as/ld与原工具一致。完成标记绑定SDK、目标、输入、g++和cc1plus以及C++17 COFF对象；不构建/注入新的目标libstdc++，Wine使用自己的C++运行库。首次继承缓存方案在安装前停止，旧as/ld路径与未知SDK对象不采用。

双架构前端已构建，Wine configure两次C++17检测均通过；实际Makefile四个变量已核对为原C及新C++的明确路径，没有以PATH顺序替换C。四个受影响组件及两架构Wine自带libc++/C++ABI归档均构建通过，归档机器类型与目标一致，原C驱动摘要保持。构建成功不等于Wine或游戏运行验收。

## 后续次序

先核对CX相对上游的macOS同步/loader/WoW64依赖，再逐主题迁移和构建；保留v0作oracle。v1完整native链构建及受影响验证完成后，才能新增精确EnginePolicy配对和非默认引擎配方。原HY1→HY2→HY3→HY4/HY5顺序不改变，Windows预算与人类交互缺口单独保留。

## 组件构建结果与边界（2026-10-18）

- 固定源码上的`wineserver`、`ntdll`、`wined3d`、`winegstreamer`构建通过；后三项均有native x86_64及两种PE产物。
- Wine自己的两架构libc++/C++ABI也实际由新GCC前端构建；未使用或安装GCC目标libstdc++。显式CC/CXX选择和最终产物摘要见[构建证据](../../docs/research/evidence/engine-r-v1-bootstrap-20261018.json)。
- 新引擎未执行、未安装/发布、未切换用户瓶子；msync/macOS loader/WoW64完整整合仍未完成，不能据此采用新配方或提升游戏裁定。
- 首轮继承缓存、APFS等价路径检查、漏设PE编译器PATH及静态库目标名的失败记录已保留，后续成功未覆盖这些记录。最终配置仍报告可选EGL/libva/FFmpeg缺失；当前组件范围不代表完整引擎依赖验收。

## 下一主题的非默认候选

顺序为 `0100-macos-inproc-msync.patch` → `0101-ntdll-retain-host-loader.patch` → `0102-msync-atomic-waitall.patch`。前两项原字节保留；0102修复旧WaitAll暂取/回滚与并发Release的原子性缺口，加入状态协调、可恢复提交、代际及退出/fork处理，保留共享槽/Mach/ulock/inproc。当前协议由962→963→964生成，完整链20文件精确重建；server、原生和双PE ntdll编译通过，限定源码复审无剩余P1/P2。

它们均未被配方选择或运行验收。正常状态操作新增2次fcntl和本地准入，暂时串行，稀疏协调映射128MiB+64KiB的RSS/延迟未测；不能以构建和静态审查代替并发、Windows和性能验证。取舍、已修问题、待验收与回退见[research76](../../docs/research/76-r-v1-waitall-coordination-20261018.md)及证据；旧移植来源/固定页表成本见[research75](../../docs/research/75-r-v1-msync-source-candidate-20261018.md)。
