# HY2：真实 Windows 工具清单与补齐路线（2026-10-07）

## 已取得的证据

用户执行了仓库固定提交 `803b942755ccd0c636b5ed0a04bde38cef33d7d9` 的只读采集脚本，输入包含 SHA256 校验 `35a09ca24dd33035464f2a2122af2d85a0cbf6b648d9bfeab2adfe40ccf46e47`，随后返回 JSON，无采集错误。相关结果也曾在 ToDesk 画面中可见；公开记录只保留用户提供的项目字段，不保存私人桌面截图。

[原始字段](evidence/windows-reference-environment-20261007.json)：Windows11专业版、build26200、64位进程；PowerShell5.1.26100.9444；SDK Include目录10.0.26100.0有Windows.h、没有km/ntddk.h；VS2022生成工具17.14.36518.9，x64 MSVC目录14.44.35207。

这证明采集脚本在该机成功执行以及列出的文件/目录可用性，尚未证明编译器实际构建、WDK完整依赖或内核契约。WDK头文件缺失只针对注册的KitsRoot10及该Include目录，不排除用户另存的EWDK或其他自定义工具位置。

微软说明 Include 目录的 QFE 固定显示 `.0`，不能用目录名识别实际补丁版本；SDK/WDK的build必须匹配，QFE通常不要求相同。因此本次 `10.0.26100.0` 记为目录版本，不误记成完整安装版本。[工具与版本规则](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)

## 工具方案比较与当前推荐

| 方案 | 收益 | 代价/证据限制 | 当前状态 |
|---|---|---|---|
| EWDK VS2022，Windows11 25H2一组 | 独立命令行环境，包含BuildTools/SDK/WDK，避免凭既有BuildTools目录推断驱动集成已齐全 | ISO下载与存储；挂载后仍须核实实际工具版本和构建；不是驱动部署/运行基线 | 用户已确认获取/使用，下载进行中；尚未启用 |
| WDK26100.6584 + 既有工具 | 与当前SDK的26100 build同组，可能减少重复下载 | 需确认VSIX、Spectre库、MSBuild/WDK集成，当前只有生成工具的清单不足以证明满足官方IDE安装路径 | 备选，未安装 |
| 项目内WDK NuGet | 固定包版本、SDK依赖跟随，适合后续可复现构建 | 当前工具集成仍须核实；不能把包下载当作工具链成功 | 备选，未恢复包 |

微软当前支持表将26100.6584列为VS2022的默认驱动工具组；28000.2526默认对应VS2026。保留VS2022参考路线，避免仅因“最新版”让这台机器整体升级。[支持版本表](https://learn.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads)

EWDK是微软提供的独立命令行环境，挂载ISO后运行LaunchBuildEnv即可使用；未将它记为已安装或已经构建。[EWDK说明](https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/using-the-enterprise-wdk) NuGet是另一条官方分发路线，具体工程集成仍需证明。[NuGet说明](https://learn.microsoft.com/en-us/windows-hardware/drivers/install-the-wdk-using-nuget)

## 后续执行与验收

1. 获取推荐组的官方EWDK，记录实际文件名、大小、SHA256；只用于独立Windows参考目录。下载页的用户确认步骤由用户处理，未代为确认。
2. 挂载ISO，在独立终端打开构建环境；记录工具内部的SDK/WDK/MSVC版本及实际构建输出。既有IDE、SDK目录不作为成功替代证据。
3. 先建立通用权限token/context契约的参考程序及独立运行目标，按research/28保留模式、输入/输出和所有权；KMDF请求生命周期随后推进。本轮没有添加或运行测试驱动。
4. 驱动可构建、驱动可加载、契约通过分别验收。工具下载或挂载不授权关闭主系统保护或把驱动放进用户游戏会话。

对抗性复核：没有把头文件缺失推广成所有WDK副本不存在；没有把Include目录`.0`当作QFE；没有把VS生成工具等同完整驱动开发环境；没有把SDK构建或文档契约冒充真实内核输出。HY1未完项、游戏门禁和最终三款本地目标维持原记录。

取舍：EWDK推荐以额外下载/存储换取独立、可撤回的参考工具环境。采用标准为实际工具版本与构建可复现；尚属候选。回退为退出构建终端并卸载ISO挂载，保留日志/哈希；无需为该候选替换现有VS工具。本轮未触发Actions。

获取更新：用户已在本轮明确确认获取/使用25H2 EWDK。官方下载入口`https://go.microsoft.com/fwlink/?linkid=2335681`返回`download.microsoft.com`的`EWDK_ge_release_svc_prod1_26100_250904-1728.iso`；HEAD长度20,002,537,472 bytes（约18.63 GiB），ETag `0x4F6217B721FF5C31A2556CFD4F7D3C202FF522B3549F0981D410871629329287`。ETag不当作独立发布的SHA256。下载进入Cider本机缓存并采用部分文件，完成后才校验大小、计算SHA256并记录；不把下载开始当作工具就绪。实际ISO内SDK/WDK/QFE仍待Windows挂载确认。

依赖实现准备：上游主令牌引用/释放与本机对象映射机制的复用范围已复核，见 [research/33](33-token-dependency-reuse-20261007.md)，没有据此先改权限函数或放行游戏。
