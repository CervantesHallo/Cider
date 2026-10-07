# HY2：令牌依赖的上游复用范围（2026-10-07）

目的：准备 research/28 权限组的实现顺序。没有改运行时或建立新的测试；Windows工具获取正在进行，内核契约参考结果尚未取得。

## 固定源码与观察

继续使用 research/23 的 Wine 快照 `eba89375a0515957701928faac0f5007ef638b04`，不把上游代码状态外推为本机已安装引擎能力。下载后记录：

| 文件 | SHA256 |
|---|---|
| ntoskrnl.c | bb7f2f48e5646a73988c068d456105c468c10e4874851d2d8ca99e3dd8deb8bb |
| ntoskrnl.exe.spec | d5a6b0c3db5fbfa167c4927042160799746a44638336f9b5c03d302bd70920cf |

该快照已实现主令牌引用/解引用：经进程对象打开token handle、转为SeTokenObjectType对象引用，再关闭临时handles；解引用交给已有对象机制。[固定实现](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntoskrnl.exe/ntoskrnl.c) 其spec已导出两个函数体，但模拟令牌引用/释放及context捕获/锁定/释放仍是桩；两项权限检查依旧直接TRUE。[固定导出表](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntoskrnl.exe/ntoskrnl.exe.spec)

本机CX26.3缓存树的known_types已有SeTokenObjectType，kernel_object_from_handle和ObReferenceObjectByHandle可做对象映射/引用。后者当前只支持KernelMode；不能将其UserMode失败忽略后继续。context与primary/impersonation接口的缺失仍以fidelity rev2所记的本机构建为准。

## 采用的推进顺序

1. 对照主令牌引用的返回对象、引用保持与平衡释放，评估复用上游两函数及spec，而非重做对象映射。
2. 补齐模拟令牌、调用线程与primary回退语义；不能用winedevice进程登录身份替代请求发起者。
3. 上下文捕获、锁定、解锁、释放作为一组验证；对象引用不自动保证查询期间token状态一致。
4. 在真实输入和生命周期就绪后实现UserMode privilege检查与逐项输出，保留KernelMode合法成功语义。

复用候选仍要求对抗性验证：上游函数请求PROCESS_ALL_ACCESS/TOKEN_ALL_ACCESS，经当前handle路径可能受到访问检查；源码有函数体不等于兑现Windows kernel对象引用的全部条件。主令牌可引用也不等于当前线程模拟token正确，更不等于完整权限或KMDF能力。

## 请求上下文与锁的依赖补充

当前CX26.3的`wine_ntoskrnl_main_loop`在领取请求时设置client_tid和TEB Instrumentation[1]，下一轮清空该slot；`KeGetCurrentThread`使用此对象或client_tid映射，`IoGetCurrentProcess`及当前线程/进程ID再经该对象取得信息（ntoskrnl.c:925、994–995、2479、2562、3206）。这是既有请求者映射的实现线索。直接在winedevice线程调用NtOpenThreadToken(GetCurrentThread())不自动沿用这条映射；不能由宿主线程有token推导请求者token正确。

后续须分别记录同步dispatch、无活动请求的调用、系统/工作线程、异步完成的实际上下文与引用保持；ID、guest对象指针和token handle不能互换。线程所属进程与附加的进程上下文在Windows也可能不同，不能把两个ID一律合并。[微软进程ID契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-psgetcurrentprocessid)

SeLockSubjectContext取得primary与impersonation token的读锁，每次调用须平衡解锁；它锁的是token，不是仅保护驱动宿主自己的字典。[锁](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-selocksubjectcontext)、[解锁](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seunlocksubjectcontext) 架构推断：若其他guest进程仍能直接通过wineserver改变同一token，只加winedevice内的互斥锁不够。把token复制为永不变化的缓存也不自动兑现“捕获引用后、锁定前”的可观测状态变化。须对实际对象读写、锁顺序、生命周期和失联释放一并设计并取得Windows对照，不能以查询结果偶然一致代替锁语义。

收益是复用实际对象所有权机制，减少重复实现；成本是验证访问范围、引用和线程请求上下文。验收需要research/28的Windows参考、相同输入的候选输出、明确失败范围和构建哈希。未取得这些证据前不升级矩阵disposition或游戏许可。后续补丁独立提交，可撤回并恢复已记录基线；本轮只采用依赖/复用顺序，没有采用运行时补丁。
