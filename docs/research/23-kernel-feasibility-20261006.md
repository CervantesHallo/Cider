# 用户态驱动宿主与安全语义可行性（2026-10-06）

## 范围与证据版本

只评估 KMDF、对象/进程回调、权限语义，不复列 47 项。[历史审计](../../data/kernel/fidelity.json)为 revision 1、2026-09-28，适用树是 `crossover-sources-26.3.0 + highball 0001-0013 + cider 0001-0002`；[构建配方](../../engine/recipes/cider-cx26.json)标识 `cider-cx26.3-r1-x86_64`、Wine 11.0、Engine R/Rosetta/new WoW64，源码包 SHA-256 为 `ac99c8ca4b3848f3e81784135f023df266b61c2345726ea55a50b3e030dd6872`。审计不是本次二进制实测，也不覆盖 Engine A 或其他 Wine。

本次联网固定 Wine 快照为 [`eba89375a0515957701928faac0f5007ef638b04`][W0]（提交于 2026-10-05 UTC），WDF 为 `b6191d9543441329154da32f7ab9bdd97228dd3c`。[原神](../../data/games/cider-com.mihoyo.ys.json)、[星铁](../../data/games/cider-com.mihoyo.sr.json)、[绝区零](../../data/games/cider-com.mihoyo.zzz.json)均标记内核反作弊且 `verdicts=[]`；条目没有证明当前客户端的全部调用集合或可玩性。以下是契约研究，未加载游戏或反作弊。

## 三类缺口与反证

**KMDF：缺运行时代码，并依赖底层架构。** 历史树没有 WDF loader/运行时。微软源码的 [绑定结构][M1]明确包含版本、函数数量和函数表；[注册实现][M2]校验表规模并分配驱动全局状态。[KMDF 模型][M3]还要求请求队列、同步及 PnP/电源管理。因此补一个同名 DLL 或让 `WdfVersionBind` 返回成功远远不够。支持可行性的证据是框架有公开实现，通用对象与队列可作为移植起点；反证是框架仍以 WDM 为基础，不自动提供宿主内核权限。共享 KMDF/UMDF 源码也不等于二者驱动二进制可互换。结论是“可研究通用运行时”，不是“加载即获得真实保护”。

**回调：当前缺实现；同步裁决需改架构。** 历史审计与本次 [Wine 快照][W1]均可见 `ObRegisterCallbacks` 返回假句柄 `0xdeadbeaf`，`PsSetCreateProcessNotifyRoutine(Ex)` 只返回成功。Windows 的对象前置回调能[削减句柄访问权][M4]，进程 Ex 回调能用 [`CreationStatus`][M5]否决创建；注销还须[等待在途回调][M6]。异步补发事件不能兑现这些条件。还须验证[注册签名条件][M9]与 FORCE_INTEGRITY；审计额外写的 ELAM/AV 证书限定未获该 WDK 契约页支持。

支持突破的反证来自 Wine 自身：[进程创建/打开][W2]及[句柄复制][W3]经过 wineserver；可提出“请求尚未提交时挂起，经驱动宿主裁决后提交或回滚”的通用协议。故历史审计所称“不能同步/不能否决”不能外推为永久不可能。但这只是架构提案：必须解决跨进程对象指针、创建者线程/令牌上下文、重入、锁顺序、注销等待及宿主失联；现有 `winedevice` 的[服务进程模型][W4]不会自然满足这些要求。

**权限：令牌检查有实现基础，必须按模式判定。** 本次 [Wine 快照][W1]已给 `PsReferencePrimaryToken` 提供函数体，历史审计仍将其记作导出桩；这不证明 Cider 已取得该改动。[Wine 令牌代码][W5]已有特权检查基础。关键更正：[SePrivilegeCheck][M7] 与 [SeSinglePrivilegeCheck][M8]在 `KernelMode` 下本就应成功；缺陷是空桩忽略 `UserMode` 的有效令牌、启用状态，以及前者必要的输出标记。不能将所有 TRUE 一律判成欺骗，或以恒 FALSE 充当忠实修复；模拟身份、上下文捕获/引用和参数处理仍需逐项验证。

**宿主保障与服务端是另两层。** 推断：即使瓶内上述契约通过，Wine 回调也不覆盖绕开 wineserver 的 macOS 原生访问，不自动获得 Windows 系统级强制保护边界。macOS 自身拒绝某次访问，也不证明 Windows 回调已生效。等效保障若需宿主隔离、特权组件或虚拟机，须另行证明覆盖范围、成本和性能，本研究未采用这些变更。[CodeWeavers 文档][C1]（2025-03-20）是 CrossOver 不支持底层反作弊的产品口径，不是所有架构物理不可能的证明。上述原始资料不提供三款国服服务端接受规则；接受与否仍未知。

## 可证伪实验与推进条件

仅提议自建 WDK 测试驱动与测试进程，在真实 Windows 建基线后对照指定 Wine 构建，均不接触游戏：

1. KMDF：验证版本/函数表不匹配、对象释放、并发 IOCTL 取消与完成恰好一次；加载成功而生命周期或输出不符即否定该里程碑。
2. 回调：让自建前置回调移除写权限，检查打开和复制后的实际操作；让 Ex 回调拒绝子进程，检查返回状态与测试入口未执行。并发注销、回调内再开句柄、宿主退出必须无悬挂或错误放行。仅收到事件不能算通过。
3. 权限：比较 UserMode 下主/模拟令牌、特权缺失/禁用、全需/任选及输出标记，再测 KernelMode 的文档成功分支；另以授权原生测试进程访问自建对象，记录宿主限制，不能把边界外覆盖记为已验证。

推进须有 Windows 对照记录、明确语义覆盖、可复现构建哈希；提案的收益是通用驱动兼容，成本是协议与同步复杂度。必要契约若只能靠伪成功或隐藏身份继续，停止当前宿主路线；其他无法兑现的契约保留诚实失败与现有门禁；无法承载失败的接口应拒绝相应能力/装载，不能空转。架构实验可撤回补丁并恢复基线。即使通用实验通过，三款游戏仍各需登录、交互玩法、画面、声音、输入、更新和可靠重启证据；本研究不承诺运行。

[W0]: https://github.com/wine-mirror/wine/commit/eba89375a0515957701928faac0f5007ef638b04
[W1]: https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntoskrnl.exe/ntoskrnl.c
[W2]: https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/server/process.c
[W3]: https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/server/handle.c
[W4]: https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/programs/winedevice/device.c
[W5]: https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/server/token.c
[M1]: https://raw.githubusercontent.com/microsoft/Windows-Driver-Frameworks/b6191d9543441329154da32f7ab9bdd97228dd3c/src/framework/shared/inc/private/common/fxldr.h
[M2]: https://raw.githubusercontent.com/microsoft/Windows-Driver-Frameworks/b6191d9543441329154da32f7ab9bdd97228dd3c/src/framework/kmdf/src/librarycommon/fxlibrarycommon.cpp
[M3]: https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/kmdf-as-a-generic-pair-model
[M4]: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_ob_pre_create_handle_information
[M5]: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_ps_create_notify_info
[M6]: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutineex
[M7]: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seprivilegecheck
[M8]: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-sesingleprivilegecheck
[C1]: https://support.codeweavers.com/cn_CN/miscellanous/anti-cheat
[M9]: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obregistercallbacks
