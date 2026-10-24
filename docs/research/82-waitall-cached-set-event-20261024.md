# WaitAll：缓存普通事件 SetEvent 的封闭保护候选

状态：F26/0122 revision2非默认候选；组件构建、精确原生夹具和负例已完成，独立实际二进制复审及自己的13阶段Wine/11份身份核对均过；固定六轮AC性能两项失败，不采用/不跑自己的小时。接续80/81及原02，不改变HY顺序、原性能门槛或游戏门禁。

## 选择和证据

F25自己的固定AC队列4.545454545倍仍未达5倍，控制项通过；插桩仅确认本次owner重试耗尽与未就绪回退，不把它当性能归因。81的源码及真实原生探针支持研究已解析raw wake和scheduler调用，不支持整帧替换。

本候选只覆盖缓存命中、类型为普通event且有EVENT_MODIFY_STATE权限的SetEvent。caller沿用已有ready-wait的直接pin顺序：先记入frame，再看closed/type/access；普通cache查询可能自行释放closed项，不能直接用于该短路径。**这不是现有ready-wait的缺陷**，它原来已经避开该分支。

frame与ready-wait caller逐字未变。短frame仍设DEFERRED并持有Wine control guard；缓存未命中、权限/类型不符、冷key/map、退休/错误代际、fork关闭、owner忙或脏日志均在同一个frame内升级DISABLE、清理pin后回原路径。新helper不初始化TLV、不映射、不探测死亡租约或等待kernel。

## 提交与唤醒

取得干净owner后再次检查代际和实际槽类型，用原coordinator写入真正的SetEvent事务及通知义务，不假造成功。提交后不允许返回PENDING而重放操作。

进程初始化阶段解析并预热raw wake及sched_yield；缺少任意符号时不进入新helper。raw wake后，在**实际读取multiple_waiters并进入Mach消息分支处**升级完整mask，不只依赖入口人口判断。gate/fork唤醒及末引用释放继续使用既有完整mask。保留原Highball的0→1 auto-event调度让步，owner/准入先归还，再yield；guest previous-state写入在frame清理以后。

非阻塞消息发送未完成时保留原APPLIED日志并同步完成恢复。独立审查发现revision1在阻塞恢复期间仍保持短Wine control guard，可能无限延后终止/暂停；**该版从未执行Wine**。revision2先完整mask、结束短control guard，再运行原coord_finish；返回后恢复短guard供frame统一离开。owner和准入内仍延后控制，释放到原安全边界后可重放控制；pins及POSIX mask始终保持，不重启已提交SetEvent。

## 验证与局限

revision2 native为 `f758950666cae386f1f7db7e74615db9d3d4a35de7b7a0e33649617b385f2cbb` / `1798e22d…`。SDK15.4/O2组件零警告、300个global定义符号集合相同；三文件patch零fuzz/offset精确重建。共享magic3/header未变，只复用完全相同header和frame段的既有证据，不借整个模块或真实运行。

公开 `msync-set-event.c` 使用提取的真实SetEvent、signal_all、coord_finish及frame函数和原coordinator；Wine布局、控制、fd/token及Mach消息由明确test doubles提供。Mach消息double故意调用真实read取消点：待交付取消不能在仍有义务时逃逸。两架构各32个线程/548检查通过，包括有/无pending取消、首次提交/重复Set、manual/auto、冷/退休/忙/脏/epoch/fork回退、真实发送分支、APPLIED恢复、gate/fork升级及最终义务平衡。恢复检查要求blocking notify的control depth为0。

四个实际编译的x86负例分别删除发送前mask、保持恢复control guard、删除yield、删除owner内epoch复核，均被拒绝。公开提取器生成的两架构集成亦通过。它们不是Wine、真实信号处理器、Windows API或所有macOS版本的证明。

初版fixture因未引用提取的full-entry函数而编译失败；第二版静态event double仅4字节对齐，arm64的实际64位原子触发SIGBUS，原生crash报告明确为event地址对齐错误。改为8字节对齐后才执行有效验证；真实Wine槽原为16字节偏移映射。两项原始失败保留，不算引擎失败或验收通过。revision1源码/组件/原生结果保留，不能借给revision2。

独立revision2实际source/linked x86审查无具体P1/P2，确认r1恢复问题闭合：mask→control end→finish→control begin顺序在实际指令中成立；未发现TLV或未证明取消点被移到mask之前，消息分支及冷fallback升级、直接pin与末引用守卫均保留。这是指定产物的限定复审，不能认证普遍信号安全或所有系统版本。

自己的两位宽13阶段全部通过，11份实际回执native/双PE/EXE、runner/helper身份及原时钟检查逐一匹配；策略负例真实错误1260，未启动替身程序。短功能通过不计自己的连续小时。固定六轮AC/LPM0完整结束：server10000/candidate2100ns=4.761904762倍未达5；CS1.118/1.036ns/op=−7.334525939%亦超出±3%，两项失败。原记录保留，不重试挑样或借小时；私有GUI及Wine清零已确认。接续只做实际SetEvent命中/回退诊断，不由源码推断负载使用了新路径。

## 后续真实覆盖诊断

另建计数产物 `0803a2d3…` / `1798e22d…`，正式F26的源码/native及性能样本不改。固定ping和新manual SetEvent控制各一次；程序/双PE/native身份、真实完成、时钟和私有GUI退出+两次空观察确认。

ping中SetEvent尝试22050次、直接pin/进入helper均22050，成功22049、仅1次gate回退，22049次真正commit及notify-required，零提交恢复、零Mach消息分支。yield22047；所有原始通知共22050次。manual控制实际10000轮Set/Wait/Reset/timeout正常，10000次helper成功；另一次冷cache回退。

这证明该插桩程序大量执行新SetEvent路径，不支持“几乎没命中新分支”的解释。全局快照按最大值读、不相加，包含启动/warmup/退出和计数扰动，不能外推未插桩比例或性能。现有计数没有保留raw wake返回值，**不能由“调用了wake”推断确实唤醒了等待线程，也不能推断全部是空唤醒**。下一步只补一个固定ping诊断，区分raw wake返回0/非0；不读取errno、不新引入取消点，不把非0自动叫做ENOENT。

固定单次返回值诊断亦已完成：`f3bb99fb…` / `1798e22d…`，22050次SetEvent全部走新helper；22050次raw wake中102次返回0、21948次非0，无Mach/恢复，身份/完成/时钟及私有GUI清零通过。这里只能记录返回分类；非0没有errno不能等同ENOENT，返回0也不计被唤醒线程数。Apple所核对wake-all实现不返回wake数量：[XNU实现](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.101.15/bsd/kern/sys_ulock.c#L940)。接续再用初始化阶段解析/预热的`__error`原生TSD getter区分错误，不改变wake参数/重试/API状态；不是正式性能队列。

errno分类的独立固定ping已完成：`9201a186…` / `1798e22d…`，22050次SetEvent均走新helper，raw wake1625次返回0、20425次非0全部为ENOENT，EINVAL/EDOM/其他/不可用均0。真实完成/身份/时钟与GUI退出+两次空观察通过。这是另一产物/另一诊断，不能把1625与前一102的差值归因收益；不拼比例或变成性能样本。它支持研究更靠近真实raw park的保守登记：保持owner内发布/复核，不根据wake结果清登记；F22全程登记与F23预读不能原样拼接。后续候选源与严格接受/回退另记83。

## 代价、接受和回退

代价为短control延期、额外入口分支及有界owner重试；预解析/预热只闭合已核对的调用集合，不声明macOS14及以后每一补丁版本等价。不得推广到read/write、冷映射、恢复、日志、末引用或尚未证明的消息调用；异步信号安全仍须真实Wine验收。

须在自己的精确pair上通过两位宽功能/终止/跨进程/策略门禁，再做预先固定的同条件三对比较，保持5倍和私有CS±3%；两项都通过才跑自己的连续小时。不重试挑样、不延长原超时、不借旧小时。收益未证明不默认选择；回退为F25原完整SetEvent路径，不切生产App/引擎或用户瓶子。
