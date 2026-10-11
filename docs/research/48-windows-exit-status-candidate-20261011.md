# 完整Windows退出码：安装可靠性前置候选

2026-10-11，HY1首次安装/入口审查发现RecipeInstaller丢弃runToCompletion的code。进一步复核Spawn.wait仅返回Unix8位状态或信号约定值，不能用其低字节推定完整Windows32位退出码。这会影响对安装错误与重启要求的判断；本轮没有复现某个用户安装失败。

不能直接把非零全当安装失败，也不能把某个8位值猜成3010/1641。[Windows Installer契约](https://learn.microsoft.com/en-us/windows/win32/msi/error-codes)分别规定这两项为需要/已经发起重启。普通EXE自有协议不能一律套MSI规则；当前检测结果也不能弥补缺失的真实状态。

## 候选源码，尚未接入安装流程

Tools/wine-exit-status/capture.c通过显式CreateProcessW启动目标，等待它的process handle，再读GetExitCodeProcess的完整DWORD。参数尾逐个编码、独立指定image、可写command buffer，保留空参数/转义，不调用shell。结果fresh CREATE_NEW、非继承句柄，检查普通单链接文件/已知reparse路径；输出schema/阶段/Win32错误/自己创建的PID/终止状态/完整退出码/耗时，不写参数、账户或环境值。[CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)、[GetExitCodeProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess)。

helper退出0只表示完整报告已写入、刷新且确实获得终止状态；子进程的非零值仍如实保留。等待成功后才查询，不能将STILL_ACTIVE当已完成。超时/等待失败只对本次创建的primary handle请求终止并最多等5秒，不访问其他进程或伪造成功。

CiderCore.WindowsExitStatus提供4096bytes上限的入口、严格UInt32/Bool/阶段解码，完整代码访问器还检查schema、scope、phase、错误和PID/终止标记。MSI分类只接收该完整DWORD，返回completed/rebootRequired/rebootInitiated/failed，超时/部分/未知输出不产生成功类别。解码器不是二进制来源证明，消费者仍须验证owned输出/完整helper返回；不能拿它解禁游戏。

## 采用/验收边界与回退

仅采用源码候选和解码接口；未接入RecipeInstaller/默认启动、未放入app资源或引擎、未运行该工具。原安装退出结果忽略缺陷仍待完整链条修复，不能标为已解决。

收益是避免8位别名、保留真实32位状态；成本是新增PE伴随工具、打包/身份/生命周期验证与安装重启处理。回退可删除独立候选/解码器，不改现有引擎、配方或游戏门禁。下一验收须覆盖高位/异常/重启值、参数/Unicode、失败/超时/部分记录及Wine/原生Windows对照，再接入安装结果与detect。

本候选只观察primary process，子进程handoff/服务/整棵树清理均未覆盖；没有独立deadline线程，启动/I/O可能超过等待参数，不能代替Tools/windows-task-runner或关闭Windows机时预约。路径检查不是对恶意同用户进程的沙箱/无竞态保证。日志继承、进程身份与installer→HYP环境/渲染仍待真实验证；不通过NativeDriver/反作弊特判改返回。

## 实际本机结果与自审

使用既有Cider mingw-w64 GCC，以-std=c11/-Wall/-Wextra/-Werror编译x86_64与i686均退出0，file确认PE32+/PE32。build.sh语法检查通过；App/CLI Swift最终构建退出0。没有新增测试夹具/运行测试、Wine或Windows程序，没有Actions。

自审确认：命令长度溢出返回失败而非截断；文件打开/短写/刷新失败不返回完成；query-failed/timeout即便晚观察到退出0也不能分类成功；返回259时仅在handle已signaled后视为实际退出值，不以轮询259判断无限等待；不声明后代清理，不对未创建进程使用TerminateProcess；解码器重复验证schema/scope，直接Decodable入口也不能用未知schema取得captured code。

最终自审还补强helper返回0的条件：明确要求phase=captured，不能仅凭created/terminated/exit_known/error=0组合判断，避免异常等待分支的错误值未被正确设置时冒充正常完成。两个架构随后以相同严格编译选项重新编译成功，以下摘要对应补强后的源码/产物。

```text
1acf06f0c3d8dc609bd989313cdaf9f1d085c231e9d2434a8d519fd6fba9dcb0  capture.c
a43b55fb79981f88d3431fd0a2d0cf5508692efaaabe1e7561e7fbd75eadb23b  cider-exit-status-x86_64.exe
f50fa7d728dbcce4bfc72c9942e0740596d268553c00b7b3be0d561e345f14fc  cider-exit-status-i686.exe
```

out二进制为本机构建产物，不提交；源码及构建脚本可复用。参考代码/构建不等于Windows kernel契约、KMDF或三款本地游戏通过。
