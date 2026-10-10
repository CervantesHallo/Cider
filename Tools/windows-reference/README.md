# 独立 Windows 权限上下文参考工程

对应 HY2 / research/28、33。工程与 Cider 的游戏瓶子独立，输出原始观察，不把 Win32 API、驱动编译或原生内核结果互相替代。

## 现在执行的步骤

在已启用的 EWDK amd64 CMD 中运行本目录的 `build.cmd`。它按顺序构建普通程序、构建未签名参考驱动，再运行普通程序的 `--user` 采集。命令使用明确 MSBuild 路径；脚本只接受 x64 的 EWDK 环境，不搜索整个 PATH。

```cmd
"完整路径\windows-reference\build.cmd"
```

将 `results/user.jsonl`、`results/client.sha256.txt` 和 `results/user.sha256.txt` 发回。失败时提供终端错误；阶段日志分别为 `results/client-build.log`、`results/driver-build.log` 和 `results/user.stderr.txt`。构建日志只保存在本机，里面可能有本地路径；不要直接作为公开源码证据提交。

普通程序只读取本进程/线程的令牌，为对照创建私有令牌副本，在副本中禁用或移除 SeChangeNotifyPrivilege，再恢复原线程状态。它不修改原进程令牌、不采集账号 SID 或用户名。五个输入为 primary-before、impersonation-copy、impersonation-disabled、impersonation-removed、primary-after；同时记录 SeBackupPrivilege 和 SeRestorePrivilege 的实际状态。

主令牌的 Win32 PrivilegeCheck 使用明确标注的 SecurityIdentification 私有模拟副本；调用线程不切换到该副本。用户模式检查失败也不会阻止独立内核观察；聚合失败保留在完成记录与退出码中。

## 原生内核阶段

`driver/reference.c` 是独立 WDM 参考驱动，直接调用 Windows 的 SeSinglePrivilegeCheck、SePrivilegeCheck、PsReferencePrimaryToken / PsReferenceImpersonationToken 和安全上下文捕获/锁定/释放接口。它使用 System/Admin ACL，只有一个只读、定长 METHOD_BUFFERED IOCTL，并要求调用线程与请求线程、调用进程与请求进程一致。输出没有指针、账号标识或任意内存接口。

`build.cmd` 的签名模式为 Off；它不安装/加载驱动，也不改变 Windows 的驱动验证或系统保护。原生内核采集需要在另外确认的可加载自有参考驱动的环境中进行。普通程序的 `--kernel` 只连接已经准备好的参考设备，不创建服务或加载驱动。

编译通过不是驱动运行通过。单次平衡的引用/锁调用记录也不能证明没有泄漏、跨进程读锁、令牌变化期间的一致性、异步上下文或完整 KMDF 生命周期；这些是后续独立输入。

## 当前证据范围

- Windows ISO 哈希、EWDK/VS 横幅、Platform=x64、编译器文件路径及 MSBuild 版本已由用户回传。
- 本地 GCC 交叉编译客户端及使用真实 WDK 头文件的 Clang 语法检查分别记录；不冒充 Windows 的 MSVC/链接结果。
- Windows 上这份工程的编译、Win32 原始输出和内核输出尚待取得。

后续 Wine 候选使用相同输入。程序记录 wine_get_version 导出是否存在，保留实际运行环境身份。
