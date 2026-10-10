# HY2：EWDK 文件交付与输入完整性（2026-10-07）

## 已完成的文件获取

用户已明确确认获取/使用微软25H2 EWDK。官方HTTPS镜像下载进程以exit0结束，长度为20,002,537,472 bytes，与HEAD响应一致；完成后计算完整SHA256并将`.part`发布为`.iso`。

- 文件：`EWDK_ge_release_svc_prod1_26100_250904-1728.iso`
- SHA256：`9f48251dd24ad31aac206d8256e95bda5f90a9783982c45a8aafeb9054562379`
- [下载与内部字段记录](evidence/ewdk-25h2-download-20261007.json)

范围：官方HTTPS来源、长度和本地完整哈希已记录；ETag没有当作独立发布的SHA256。文件可供后续传输核对，不代表Windows构建或内核契约通过。

## 镜像内部的实际观察

只读挂载完整ISO（UDF），读取Version和SetupBuildEnv，不运行Windows二进制。实际版本为ge_release_svc_prod1.26100.6584，VS17.14.5，默认MSVC14.44.35207；Kits Include目录26100.0有km/ntddk.h、um/Windows.h，另有x64驱动MSBuild props。

镜像根部的LaunchBuildEnv.cmd转入独立cmd环境，SetupBuildEnv未带参数时选择x86；`amd64`分支设置x64目标和amd64宿主。因此Windows参考入口明确为：

```cmd
LaunchBuildEnv.cmd amd64
```

目录`.0`仍不是QFE证据，实际版本依据镜像Version文件；Windows上的构建命令、结果和驱动执行仍未取得。

## 远程输入完整性失败

ToDesk通过CUA能读取画面。用户将PowerShell最大化、退出选择状态并切英文后，索引窗口定位和短命令可完成部分操作；普通typeText仍丢字，粘贴出现单独的v而不是目标文本。

按字符发送并穿插状态观察也未完成可靠传输：发送1000个ASCII十六进制字符，以Set-Content的CRLF计应得到1002 bytes；实际文件仅992 bytes，SHA256与预期不一致。尝试只写数据文件，没有解码或执行不一致的内容，停止该路线，不重复发送后续片段。

Windows仅新增独立临时目录`cider-reference-c33b35e92c114ec7b765390818adbb75`及一个未通过校验的collector.hex。它不是参考程序或驱动；保留用于复核，不以它开始构建或执行。

这项失败阻止可靠的远程长命令/源码传送，不能把偶尔成功的短查询当作稳定执行通道。没有另开SSH/WinRM服务，没有改系统保护、执行策略、游戏或反作弊。

## 必须由用户补齐的 Windows 步骤

1. 经已有ToDesk文件传输或用户选择的本地传输方式，将已完成ISO放到Windows本地目录；对传输后文件核对上面的SHA256。
2. 在Windows挂载该ISO，在磁盘根目录打开cmd，运行`LaunchBuildEnv.cmd amd64`。
3. 提供该终端的初始化结果及`where cl`、`where msbuild`输出，确认实际选择了镜像内的工具；随后才能建立独立通用契约参考目标。

文件获取与检查已完成；操作工具仍无法可靠执行这些Windows步骤，不能自行把它们记成成功。未添加或运行测试驱动，三款游戏许可维持原状态。原顺序继续HY1交互收尾→HY2依赖/Windows对照→HY3许可闭环→HY4/HY5。

## 2026-10-10 用户执行结果

用户报告Windows端下载已完成，并复制回传SHA256 `9F48251DD24AD31AAC206D8256E95BDA5F90A9783982C45A8AAFEB9054562379`，与此前完整ISO哈希一致；启动输出含EWDK `ge_release_svc_prod1.26100.6584`和VS2022 Developer Command Prompt `17.14.5`。用户同时表示没有进入命令提示符，尚未提供`where cl`和`where msbuild`输出。

为解释终端行为，再次只读挂载相同ISO检查脚本：LaunchBuildEnv调用`%comspec% /k`，没有用于另开构建窗口的`start`；VS横幅先于core/ext初始化调用输出。因此横幅证明启动代码已执行，不能证明初始化完成或x64工具已就绪。是否仍在初始化、处于控制台选择状态或仅缺少可见提示符，需要用户当前终端证据，尚无确定根因。只读检查后卸载Mac镜像；未运行Windows代码或构建/测试。

用户愿意执行简单Windows操作，后续优先交付短命令及明确的返回内容，避免重新使用已失败的远程长输入通道。

同日后续截图显示`C:\Windows\System32>`提示符，以及`where cl`找到镜像内MSVC14.44.35207的`bin\Hostx64\x64\cl.exe`。查询之后没有返回提示符，用户报告等待5分钟且无法输入。当前只能确认进入过CMD和编译器文件定位，不能确认编译器执行、Platform、MSBuild或构建结果。

微软说明[`where`](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/where)默认搜索当前目录及PATH各目录。继续扫描某个慢路径是待核实的解释，不能根据截图当作根因。优先尝试取消当前只读查询，再用CMD内置命令和明确文件路径取得环境；不继续重复无范围的`where`查询。

后续截图已显示恢复的CMD提示符，不能确定用户选择了取消查询还是另开窗口。`echo %Platform%`和`dir /b ...`在粘贴时合并为一行，输出展开为`x64dir ...`，仅证实Platform=x64，未执行文件查询。随后提供单行、明确路径的MSBuild `-version -nologo`命令，用户回传`17.14.10.27608`。该结果证明对应程序给出了版本输出；没有退出码或MSBuild进程位数证据，也没有编译器执行/编译/链接或内核契约结果。[结构化观察](evidence/ewdk-windows-initialization-20261010.json)

当前环境前置：镜像内容哈希一致、EWDK/VS版本横幅、CMD提示符、x64目标环境、Hostx64/x64编译器文件位置及MSBuild版本输出已取得。下一交付按research/28、33准备与游戏独立的Windows参考工程，构建、契约结果和Wine候选实现各自留证。单行手动命令替代长文本远程输入；本轮没有新增或运行测试、驱动或游戏。
