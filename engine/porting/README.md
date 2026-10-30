# Engine R v1 主题迁移

最新[106](../../docs/research/106-r-v1-producer-terminal-cell-20261030.md)：0024七文件含Native源/Makefile，实际whole-producer状态发布、NT owner失败保留和预先恢复驱动；限定源码修复/复审及匹配构建完成。双向正常读回、精确guard负例、真实c0000235关闭失败恢复共6次实际运行独立绑定，普通恢复/保护4一致。仍非完整Wine/release或renderer采用；失败START无watch、临时H、FIFO/epoch尚待实际验收，然后补per-host/取消/资源与像素。旧实验包按清单恢复，不凭manifest混用字节。

最新[105](../../docs/research/105-r-v1-request-inflight-20261030.md)：0023保留实际请求pin到同步处置返回，finalizer要求请求排空；普通/NDEBUG/native链接与精确重建、限定源审完成。最终真实准入pin/typed RELEASE/STOP对照两PE各20/0，省略保护各20/1精准CHECK12，夹具两项P2已修后重验并独立绑定。103/104完成页原语及连续跨位宽发布已有局部证据，但尚未接入真实producer发布。继续取消/未知安装与资源接入；不切默认、不启用foreign。旧实验包已归档差异并清理，参考105清单恢复。

最新[102](../../docs/research/102-r-v1-cocoa-model-retirement-20261026.md)：0022 Cocoa model退役完成owner已实现、链接和限定复审；真实selfprocess/CAContext两PE各20/0，省略保护负例各20/1精准CHECK12，受控false完成重试各20/0；原窗口各1415/56/168通过。raw/组件/清理恢复独立绑定，完成8整树已按差异归档清理。只证明model层，尚非compositor/GPU、跨进程renderer或游戏。下一终端完成发布/取消序号及producer资源仍提案，foreign/默认不启用。


最新[101](../../docs/research/101-r-v1-real-close-inflight-20261026.md)：真实NtClose保护失败/同句柄成功重试双向各28/0；最终在途对照修后保留PENDING，去掉保护条件则在有效句柄仍在途时过早SUCCESS。四客户端28/0且最终关闭，负例依据是不变量而非泄漏。严格时钟/独立源审/raw组件清理绑定完成，已归档清理15整树。下一步先补Cocoa实际退役完成栅栏，再做producer资源/撤销状态、事件reparent及两三进程像素；当前该栅栏是提案，foreign/默认/游戏未启用。


最新[100](../../docs/research/100-r-v1-process-lease-faults-20261025.md)：0021一次STOP丢交付和STOP后真实worker终止已做双向修后/精确错误分支对照，分别68/0对68/1及28/0对28/2；失败观察补证实际PE所有者遗留，源审与独立运行绑定完成。已清理4引擎/4stage/11重复备份并保留精确差异。继续失败NtClose/在途完成实际对照，再到真实CAContext资源/事件/reparent及两三进程呈现；foreign路由、默认及游戏评级未启用。


最新[99](../../docs/research/99-r-v1-typed-producer-leases-20261025.md)：0021类型化消息/真实进程租约及PE恢复实现已精确重建、成组链接、NDEBUG编译和限定复审；最终双向跨位宽各68项、实际退出/主动释放/高32位代次拒绝通过，原窗口受影响双PE各1415/56交换链/168Present通过并有独立log/组件/清理恢复绑定。0020旧PE32清理缺口另已定向补齐，旧failed保持。下一步失败清理/worker死亡真实对照→CAContext资源及事件/reparent→两三进程呈现；foreign路由仍未启用，故障/屏幕/CEF/游戏不计完成，不切默认。


0020最新见[98](../../docs/research/98-r-v1-root-host-coordinator-20261025.md)：真实根实例/宿主ticket、成功后提交、关闭/耗尽撤销和局部事件钩子已重建/链接；双PE高位诊断各32项零失败，旧耗尽分支精确4失败。普通窗口双PE各1415项零失败，但PE32第一次清理回执丢失、整批仍拒绝，最后清理恢复真实确认。producer租约/跨进程传输仍未接入，foreign-child未启用；诊断入口和故障不进普通引擎。完整临时包已按SHA差异归档清理，恢复先查清单。

0019已补foreign-host基础：client frame/祖先clip独立，Cocoa裁剪不缩图、主线程代次匹配及关闭保护；精确重建/固定SDK链接/两P2修后源审闭合。当前未接root coordinator/事件租约/路由，未运行或解除foreign-child拒绝，完整契约与两/三进程验收见[95文末](../../docs/research/95-r-v1-legacy-window-bridge-20261025.md)。原本进程运行证据不自动转给新模块。

注册表往返最新见[96](../../docs/research/96-r-v1-registry-roundtrip-20261025.md)：0018修正底层RegCreate安全属性传递及尾部分隔符叶键；双PE回调各33项、直接A/W/三视图各195项通过，故障所有权、四admission负例和五错误变体拒绝已实际完成。公开夹具在tests/runtime/legacy-registry和registry-security；诊断不进入普通引擎，真实Windows/完整权限及完整桥接仍待，继续窗口生命周期和foreign-child/CEF。

完整legacy窗口前端最新见[95](../../docs/research/95-r-v1-legacy-window-bridge-20261025.md)：0015/16/17保留24槽192B接口及120B快照，使用现有managed surface并修缓存/重入销毁；registry v2按位宽封送完整有效安全属性与动态数据。原M-A双PE各120 Present零失败并进入新前端。API往返/故障清理、反复窗口生命周期、视觉与foreign-child/CEF仍待；未进入默认配方或完整R v1验收。

销毁依赖修复接续[94](../../docs/research/94-r-v1-gpu-teardown-20261025.md)：新增0012/13/14及私有MinGW TLS候选，保留原队列和GPU上传后备。最终两种PE的TLS/allocator新旧对照完成；R2仍失败，增加0014后R3原GPU各575项和offscreen各46通过。候选非默认，完整MF/CRT/legacy窗口/foreign-child/CEF与游戏仍待；原失败保留，不重做无变化同步性能队列。

GPU/窗口最新见[93](../../docs/research/93-r-v1-gpu-shared-and-window-foundation-20261025.md)：Highball共享/keyed边界双PE各46项过，MF真实GPU样本PE64四case575项过，PE32 native异常仍拒绝。设备初始化顺序源修正成组重建，窗口0011基础已编译/修后复审；完整桥接和运行未完成，不默认采用。MF前次结果见[92](../../docs/research/92-r-v1-mf-allocator-20261025.md)，公开夹具在tests/media/mf-source-reader。

最新媒体接续见[91](../../docs/research/91-r-v1-media-preparation-20261024.md)：显式准备后WMA双PE实际数据/耗尽通过，原全冷20s失败保留；产品托管媒体准备、缓存失效与真实Stop路径已实现/验证，导入不执行。完整MF/视频/游戏和R v1默认采用仍待，接续Highball0010。下方早期组件结论为历史记录。

2026-10-18，按计划02在v0 Steam通过后开始。此目录记录新基线的源码迁移、构建及私有运行验收，当前不作为发布配方，不切换用户引擎。最新同步运行状态见[research77](../../docs/research/77-waitall-mac-runtime-20261018.md)，下方原组件阶段结论保留为历史。

最新源码接续（2026-10-24）：原命令行追加和AUTO GL→Vulkan策略已迁移，精确重建/双PE完整翻译单元编译与限定独立源审通过；未链接/运行/采用。真实用户名路径已在固定上游存在。GSBASE与旧单槽镜像按新ABI核对，不直接叠加；见[86](../../docs/research/86-r-v1-profile-renderer-source-20261024.md)。同步扩展U90次运行正常、8/15归因单元未过±3%，完整M1仍未过；原冻结性能/小时不改。

`r-v1-inputs.json`固定Wine11.18 tag object及实际commit、只读官方mirror来源、首个源码文件摘要，并列出当前5份CX配方的21个唯一补丁、主题、实际采用关系和迁移状态。GitHub返回的tag签名验证结果原样记录，不能把固定commit或TLS下载说成已独立验证签名。现有CX源包哈希和各补丁SHA256作为oracle保留。

本机 `out/engine-r-v1/unpack/` 是准备过的工作树，含早期端口，不能仅凭目录名把它当作未修改的上游。原始源码对照应读取 SHA256 已校验的源归档成员，或核对固定 commit 的逐文件摘要；不要覆盖现有工作树去“修复”对照。本轮 WG sample 的归档原件为 `3c58ebdf…`、准备版本为 `241e08c2…`，符合既有端口清单；WMA decoder 和两个媒体向量也已逐字核对归档。

后续实际运行：参数追加双PE各23检查和旧实验基线五项语义负例、D3D9双PE/旧实验基线PE64各16检查和4096RGB读回，以及普通CLI失败后的实际回收均通过。范围与初版故障见86，完整图形、ABI、游戏和U归因仍待。

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

CX与固定上游共享get_sync/inproc缓存模型；迁移保留其所有权/权限路径。上游object_ops已改为指定字段初始化，不能连旧位置表整体复制。共享索引与Unix fd的收发、关闭及协议生成必须成组处理；WaitAny/WaitAll和APC返回也须明确映射。单独移植Highball0005的yield不足以得到msync。迁移前的依赖核对已由下方0100–0102非默认源码实施接续，逐文件摘要在输入JSON。

## PE C++工具链（已补齐）

首次Wine11.18 configure通过时明确报告两架构缺C++17前端，原GCC15.2为C-only；随后按下述独立前端方案补齐。不能以configure返回0当作所有模块启用，也不因缺依赖更换为曾导致Steam登录故障的LLVM后端。

`engine/pe-cxx-toolchain.sh`从同一固定GCC15.2归档构建两个独立C/C++前端prefix，保留原C工具链及其CRT/libgcc。源码逐文件/路径集合校验，处理APFS的Unicode等价名称并拒绝碰撞；编译对象使用独立、固定SDK的目录，实际as/ld与原工具一致。完成标记绑定SDK、目标、输入、g++和cc1plus以及C++17 COFF对象；不构建/注入新的目标libstdc++，Wine使用自己的C++运行库。首次继承缓存方案在安装前停止，旧as/ld路径与未知SDK对象不采用。

双架构前端已构建，Wine configure两次C++17检测均通过；实际Makefile四个变量已核对为原C及新C++的明确路径，没有以PATH顺序替换C。四个受影响组件及两架构Wine自带libc++/C++ABI归档均构建通过，归档机器类型与目标一致，原C驱动摘要保持。构建成功不等于Wine或游戏运行验收。

## 后续次序

先核对CX相对上游的macOS同步/loader/WoW64依赖，再逐主题迁移和构建；保留v0作oracle。v1完整native链构建及受影响验证完成后，才能新增精确EnginePolicy配对和非默认引擎配方。原HY1→HY2→HY3→HY4/HY5顺序不改变，Windows预算与人类交互缺口单独保留。

## 组件构建结果与边界（2026-10-18，运行前历史）

- 固定源码上的`wineserver`、`ntdll`、`wined3d`、`winegstreamer`构建通过；后三项均有native x86_64及两种PE产物。
- Wine自己的两架构libc++/C++ABI也实际由新GCC前端构建；未使用或安装GCC目标libstdc++。显式CC/CXX选择和最终产物摘要见[构建证据](../../docs/research/evidence/engine-r-v1-bootstrap-20261018.json)。
- 新引擎未执行、未安装/发布、未切换用户瓶子；msync/macOS loader/WoW64完整整合仍未完成，不能据此采用新配方或提升游戏裁定。
- 首轮继承缓存、APFS等价路径检查、漏设PE编译器PATH及静态库目标名的失败记录已保留，后续成功未覆盖这些记录。最终配置仍报告可选EGL/libva/FFmpeg缺失；当前组件范围不代表完整引擎依赖验收。

## 非默认候选的源码阶段（历史，接续见下）

顺序为 `0100-macos-inproc-msync.patch` → `0101-ntdll-retain-host-loader.patch` → `0102-msync-atomic-waitall.patch`。前两项原字节保留；0102修复旧WaitAll暂取/回滚与并发Release的原子性缺口，加入状态协调、可恢复提交、代际及退出/fork处理，保留共享槽/Mach/ulock/inproc。当前协议由962→963→964生成，完整链20文件精确重建；server、原生和双PE ntdll编译通过，限定源码复审无剩余P1/P2。

它们均未被配方选择或运行验收。正常状态操作新增2次fcntl和本地准入，暂时串行，稀疏协调映射128MiB+64KiB的RSS/延迟未测；不能以构建和静态审查代替并发、Windows和性能验证。取舍、已修问题、待验收与回退见[research76](../../docs/research/76-r-v1-waitall-coordination-20261018.md)及证据；旧移植来源/固定页表成本见[research75](../../docs/research/75-r-v1-msync-source-candidate-20261018.md)。

接续构建Wine启动器、wow64/wow64cpu、win32u和winemac.drv，9个产物已链接。新增构建有30条来自未修改基线winemac文件的SDK弃用/nonnull警告，未宣称整个范围零警告；原WaitAll四组件最终构建零警告的范围不扩大。平台组件尚未运行，窗口/WoW64主题迁移及完整引擎采用仍待。

## 私有运行与性能接续（2026-10-18，进行中）

0103叠加0102，保留整体原子提交/可恢复通知，用原子owner及独立OFD死亡租约替代正常路径fcntl，并修复实测线程退出的缓存锁、引用和等待节点清理。完整Wine已由独立签名GUI/XPC在空实验瓶实际执行；PE32/64基础、64轮生命周期、八线程短压和45秒预检通过。两轮小时因合盖中断，第三轮连续小时已通过，78检查零失败、约6.54亿次许可获取/归还平衡；两位宽四子进程竞争也通过。原版同新夹具也通过，不能声称复现旧竞态；完整M1仍未整体验收。

v2固定三轮的单事件及RTT均仅比同pair server快4.727倍，未达5；控制归因也未过±3%。独立可选0104只改process-local准入，以每线程SC活动发布/门重查及fork注册表扫描减少共享计数开销；保留父FD/token刷新、子进程退休和原全局事务。它已在另一个独立app/空实验瓶完成两位宽基础/生命周期/短压，连续小时已通过，固定v3性能因用户合盖窗口完成13/15，剩余两轮待续，不能判完整通过。代价是注册记录多一个指针和fork扫描；共享ABI不变。0103与0104均非默认，生产策略未登记，不提升游戏或Windows原生状态。
