# HY2：主令牌引用接口候选

2026-10-13。接续33/54/56的依赖顺序，`0010-ntoskrnl-primary-token-reference.patch`严格依赖0009。只在独立候选源码和构建中新增两个函数体及stdcall导出，不加入默认配方或用户引擎。

## 实现与取舍

[微软引用契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psreferenceprimarytoken)针对指定进程的主令牌取得引用；[释放契约](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-psdereferenceprimarytoken)平衡该引用。候选沿用[固定上游实现](https://raw.githubusercontent.com/wine-mirror/wine/eba89375a0515957701928faac0f5007ef638b04/dlls/ntoskrnl.exe/ntoskrnl.c)的对象→临时handle→token→对象引用机制，增加PsProcessType检查并只请求PROCESS_QUERY_INFORMATION/TOKEN_QUERY。

成功路径从显式process对象打开临时process handle，打开它的实际primary token，经0009取得真实对象引用，然后关闭临时token/process handles。失败保留NULL并记录状态，没有有效令牌副本、宿主身份回退、固定模拟标志或伪成功。解引用调用已有对象释放机制。

与固定上游ALL_ACCESS相比，query-only减少无用权限请求；成本是对原实现的访问范围差异，须取得原生及跨主体结果。现有NtOpenProcessToken仍走Wine当前调用者访问检查，不能声称它已兑现Windows内核的全部访问能力。NULL是当前候选保守失败，不是微软为该有效调用承诺的失败返回；发生此失败仍表示不兼容。未因函数导出存在而改变fidelity矩阵/游戏支持状态。

## 源码审查与实际构建

逐项静态核对：错类型process在打开前拒绝；process打开失败没有待关闭handle；token打开失败仍关闭process；引用失败不发布token，已打开handle仍关闭；取得的引用早于临时handle关闭；函数/spec调用约定一致；没有改模拟token、Se*权限检查或请求主体选择。临时close返回及释放失败的完整处置仍属未通过项，源码调用NtClose不能当作真实清理回执。

0009自审发现void引用失败只记录日志的问题，已改显式异常后重新构建。最终i386/x86_64 ntoskrnl增量构建退出0，日志无warning/error行，静态PE导出检查均有两函数。两补丁在独立基线文件副本按顺序、fuzz=0实际应用，重建源码逐字节符合编译输入。源/补丁/构建产物和日志SHA256见[最终证据](evidence/hy2-primary-token-candidate-20261013.json)。wineserver先前已单独编译，最终通用void修正只改变PE源码；不是重跑无变化构建或运行测试。

## 下一关键依赖

该候选尚无运行、Windows kernel对照、alias/reference/last-handle closure或跨权限结果；模拟附件状态、显式活动请求主体、context锁与延期回复仍未实施。既有自有参考驱动构建有效，但当前协议检查的是完整capture/reference/lock组，不能把未运行的部分或Win32基线算作本补丁证据。没有新建测试/改参考驱动，也没有重跑旧基线。

下一步原生工作必须先合法解决plan/14的超额pending与准备额度，再在独立环境完成参考，不能在日用Windows加载未签名驱动。预算和本机watchdog部署缺口均保留；没有连接Windows或改账。用户离开期间的继续授权没有扩大累计机时。

可独立继续的是把请求主体/模拟附件/延期回复方案整理成可审查协议切分；未取得证据的并发/安全语义不进入正常引擎。回退可单独撤掉0010（0009仍是候选），或丢弃整个独立构建，原游戏文件、常用引擎及门禁不变。
