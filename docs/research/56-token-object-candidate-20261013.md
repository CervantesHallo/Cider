# HY2：令牌对象映射与引用失败的候选修正

2026-10-13。实施54指出的第一组依赖，候选补丁为`engine/patches/candidates/0009-token-kernel-object-mapping.patch`。只在独立源码/构建副本准备，未加入任何引擎配方、安装、运行或改变游戏门禁。原缓存树两份源码摘要在本轮前后相同。

## 源码改动

server token新增kernel object列表、ops getter和创建时初始化，复用[固定Wine快照](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/server/token.c)的机制，适配CX的positional object_ops。列表在任何可能release路径之前初始化；沿用release_object→free_kernel_objects→dispatch_free机制，没有另造令牌副本、绕开访问检查或把宿主令牌冒充目标对象。

ntoskrnl分配路径新增带NTSTATUS的内部函数：检查大小溢出，先初始化header，登记失败释放未成功登记的分配并返回实际状态；只有成功才发布指针。已有指针型分配器保留ABI，失败返回NULL。无constructor的handle映射直接保留登记状态，有constructor的旧NULL→NO_MEMORY行为尚未扩展。

引用路径在服务端grab成功后才把本地ref从0增加为1；已有正引用在同一obref_cs中增计数，MAXLONG不溢出。ObReferenceObjectByHandle只有引用成功才写输出，UserMode仍明确NOT_IMPLEMENTED，未增加访问检查能力。

## 对抗性自审发现与修正

第一版内部函数失败不增计数，但void ObReferenceObject只记日志后返回；调用者仍可能按“已引用”继续并配对解引用。这不能作为完成引用。已改为退出临界区后RtlRaiseStatus，显式中止该失败路径；带状态码接口仍返回NTSTATUS。该异常是候选的兼容失败处置，不是声称Windows对有效对象正常调用存在同样失败契约。[微软接口契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obreferenceobjectbyhandle)要求成功取得实际对象引用，编译或日志不能替代生命周期证据。

自审还检查了token所有创建/复制均经初始化入口、generic object释放先处理mapping、首次grab失败不留下本地引用、输出不提前发布、NULL检查在header指针计算前、数值溢出和所有早退释放锁。没有新增测试夹具或运行套件；这些是静态路径检查。

### 阻止采用的剩余风险

- lookup/grab分开、并发最后handle关闭及dispatch_free交付尚未建立原子生命周期证明；本轮不宣称消除了既有竞态。
- 既有释放失败仍只是日志，constructor NULL仍可能折叠状态，void引用调用者的异常传播/回滚须逐项核查。
- ObReferenceObjectByHandle的UserMode、可选handle-info输出和整个通用API契约并未完成；本轮只修状态/发布依赖。
- 没有请求者/模拟附件state、context共享锁或权限检查实现，没有native alias/slot replacement/失败/释放结果。

## 构建及采用边界

用APFS副本复用已配置r1构建，重定位源码/构建路径，macOS15.4 SDK，独立编译x86_64 wineserver和i386/x86_64 ntoskrnl。首版构建退出0，无warning/error行；[首版证据](evidence/hy2-token-object-candidate-20261013.json)明确标历史迭代，不能用旧patch摘要证明审查后源码。审查修正后与0010一起构建的最终摘要、日志、两架构导出及补丁重建在[57证据](evidence/hy2-primary-token-candidate-20261013.json)。不是全新configure或完整引擎发布，也未执行任何二进制。

收益是补齐上游复用缺失的对象登记，并让失败不再产生已引用输出；代价是通用对象失败行为变化、服务端token结构字段和需要完整生命周期回归。当前采用状态仅为独立候选实现/构建。进入配方前须通过对应原生参考、失败/并发/释放及既有对象回归证据。回退删除候选引用或丢弃独立构建；常用引擎从未切换，无需改用户文件。
