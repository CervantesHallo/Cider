# HY2：同一二进制的 Windows / Cider Win32 对照（2026-10-10）

接续 research/28、33、36。用户提供5个文件：原始user.jsonl、它的SHA256、客户端SHA256以及客户端/驱动构建日志；随后提供仅含cider-reference.exe的ZIP。额外两份构建日志是正常阶段产物，不是多运行了其他参考程序。

## 原生输入与校验

- 原始JSONL为8648bytes，SHA256 `e1cbb1246dfd4a8c9ad99b0ceaa06649d1a29b5dc7b143be581608f73c2ac1e5`，与用户校验文件一致。保留原始CRLF字节，[原始数据](evidence/windows-win32-privilege-reference-20261010.jsonl)。
- ZIP仅有一个681472bytes的x64 PE（machine 0x8664）；EXE SHA256 `16ef0e9e102557f4016e73f9cb7591ed0e9b30f0f7d8e76af10494c2c86452fa`，独立计算后与client.sha256相同。没有在公开源码提交二进制。
- session记录来源提交c81f34c、MSC_FULL_VER=194435209、64位进程、原生OS build26200和win32-only；wine_get_version导出不存在。该编译器实际版本为19.44.35209，不把工具目录14.44.35207当作完整编译器版本。
- 两份日志均为0警告/0错误；明确使用HostX64/x64 CL和link、W4/WX，驱动链接NtosKrnl、Wdmsec等，生成exe/sys。日志中的Using KMDF 1.15是工具配置通知：源码是WDM，没有WDF对象或调用，不能当作KMDF运行时或生命周期通过。含本地路径的原始构建日志未复制到公开报告，只保留文件长度/哈希和相关观察。[校验字段](evidence/windows-win32-privilege-validation-20261010.json)

## 实际观察

32个JSON记录包含1个session、5个令牌快照、25个PrivilegeCheck及完成记录，无error记录，全部调用成功且completed=true。检查结果与各快照中实际存在/启用状态逐项一致。

| 场景 | ChangeNotify状态 | 单项结果 | ANY三项 | ALL三项 |
|---|---|---|---|---|
| primary-before / impersonation-copy / primary-after | 存在，Attributes=3（启用） | true | true | false |
| impersonation-disabled | 存在，Attributes=1（启用位清除） | false | false | false |
| impersonation-removed | 不存在 | false | false | false |

这组输入中Backup/Restore都存在但未启用。前后快照对这三项权限和线程模拟状态一致；不推广为所有令牌字段均已验证。主令牌场景的Win32检查使用明确标注的私有SecurityIdentification副本，不冒充内核直接主令牌检查。

一个必须保留的输出细节：部分满足时ALL返回false，但启用的ChangeNotify仍被标记SE_PRIVILEGE_USED_FOR_ACCESS（0x80000000）。失败不能一律丢弃已用属性。禁用和移除均返回false，但令牌存在性不同。[Win32契约](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-privilegecheck)

## 隔离 Cider 对照

创建全新CIDER_HOME实验目录，APFS复制已验证r2引擎，不复制用户前缀；新建64位Win32 Reference瓶子，en_US.UTF-8、msync。用上述同一EXE经ciderctl运行--user，退出码0；采集结束后仅停止实验瓶子，保留原始日志及实验数据。没有启动游戏或账号启动器。

结果中5个令牌快照、25个检查及完成记录（session之后31个JSON记录）与Windows完全相等。实际输入状态本轮也相等，没有把两边账户默认状态差异当作API缺陷。session仅有两处环境差异：Wine导出可见；Wine配置OS build19045，而参考Windows为26200。相同源码提交、编译器标记及二进制哈希均保持。[对照记录](evidence/windows-cider-r2-privilege-comparison-20261010.json)、[Cider JSONL](evidence/cider-r2-win32-privilege-reference-20261010.jsonl)

这证明指定r2构建在这25个Win32检查/5个快照输入上的现有行为一致；不是ntoskrnl Se* / Ps*内核接口、跨进程锁、资源泄漏、KMDF或游戏会话验收。矩阵disposition及三款游戏门禁不改。

## 实验中修复的产品缺陷

第一次新建实验瓶子的wineboot尚未spawn就被“瓶子设置已改变”拒绝。实际根因：Bottle.prefix明确是directory URL，尚不存在的prefix在FileSafety.child产生不带尾部斜杠的URL；Foundation的URL相等为false，而标准化文件系统path相等。验证后修改WineRunner，先保留FileSafety边界/拒绝链接检查及file URL要求，再比较标准化path。引擎/locale/sync和配置id校验保留。

收益是恢复新瓶子的首次初始化；代价是相同文件路径的目录提示元数据不再影响身份比较。修正后App/CLI构建成功，真实全新瓶子wineboot、目录隔离和字体升级均完成；参考EXE成功运行。回退可撤销独立源码变更；没有以先创建目录或跳过一致性检查掩盖问题。

本机out/Cider.app随后重新打包并签名，通过CUA应用菜单重开，原5项资料库及Steam/HYP运行状态仍可见。新建/运行的实机证明来自隔离CLI瓶子；没有为GUI额外创建用户瓶子或操作登录副本。

## 下一步与边界

原生kernel阶段仍需可加载我们自有参考驱动的Windows测试环境。已经询问用户现有独立VM/签名环境；当前默认未签名SYS没有被加载。微软说明x64内核代码需要签名，测试签名设置属于另一阶段，不由本轮脚本自动改变。[签名与测试模式](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option)

继续按主令牌/模拟令牌、requestor映射、context引用和锁的依赖组推进；Win32一致不能代替内核原生数据。没有触发Actions，没有启用新子代理，也没有新增/运行Swift测试套件。
