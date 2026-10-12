# HY1 安装准备：依赖预检与退出状态接口

2026-10-12。继续原 HY1→HY2→HY3→HY4/HY5 顺序。本轮只在 Mac 修改源码、审查及构建，没有连接 Windows、运行 Wine 或新增/运行测试。安装退出状态被忽略的问题见 [48](48-windows-exit-status-candidate-20261011.md)，本轮没有把它标为已解决。

## 已接入的配方依赖预检

旧 RecipeInstaller 遇到依赖循环直接 return；依赖按递归顺序边验证边安装，较晚出现的未知依赖或错误环境绑定可能在前面的依赖已执行后才报错。

现先在同一 CompatDB 值快照上生成完整依赖顺序，再开始执行：迭代 DFS 检测循环、未知依赖、身份不一致及现有 RecipePolicy 违规；检查实际引用来源的文件名/摘要格式、exe/msi 类型、参数 NUL，以及安装环境档案归属和变量格式。循环报告具体路径，共享依赖只加入顺序一次。已安装依赖同样参与图验证，执行阶段仍用原 detect 跳过。

采用收益：这些静态错误在本安装器下载或修改前缀前报告；不再默许不完整依赖。代价：即使部分组件已安装，错误依赖图仍会被拒绝；新增失败类别，校验成本随图大小增长。保持配方声明的兄弟依赖顺序，逐帧检查取消，不为深依赖消耗递归调用栈。

这不是整个安装事务的回滚：UI 可能在调用安装器前创建瓶子；下载失败、运行时错误和中途取消仍可能留下此前已完成的依赖。未新增成功判据，detect 和完整 Windows 退出码的联动仍待完成。

对抗性静态审查覆盖自循环、间接循环、重复边、菱形共享依赖、已安装但循环的依赖、缺失的后续依赖和被拒绝/跨配方环境档案。依据遍历状态和副作用的调用顺序检查，没有运行合成夹具或宣称故障注入通过。真实安装/取消回归尚未验收；可单独回退 RecipeInstaller 此次预检改动。

## 退出码候选的显式准备接口

新增 WineRunner.prepareWindowsExitCapture，仅准备候选工具的 LaunchPlan 和私有输出目录，不启动程序。默认 App/CLI/RecipeInstaller 没有调用它；伴随 EXE 仍未放入应用或引擎。

- 要求目标属于同一瓶子、引擎和 loader；检查 argv/环境 NUL、变量名和超时 1–3600000ms；保留正常 preflight。准备结果仍须走正常 runner.launch，不能代替瓶子/引擎锁、副本 Steam 保护或游戏门禁。
- 调用方提供已知工具 SHA256，按不跟随末级链接的文件描述符读取，限制 2MiB、普通单链接、读取期间大小与修改时间稳定；核对内容摘要及 PE 用户程序字段后复制到独占 0700 目录。
- 目标只接收宿主绝对路径或绝对 Windows 盘符路径；不搜索 PATH，不解释 shell，尚不支持 UNC。工作目录和目标环境通过正常 runner 重新生成，固定同步/区域/loader 规则继续生效。
- request.json 保存工具摘要、machine、目标、超时及计划匹配档案；没有复制参数或环境值。计划档案不是已观察到的子进程继承证据，也不代表安装步骤显式档案绑定的完整审计。
- readStatus 要求调用方提供 collector 的零 Unix 返回，并有界读取独立 result.json，完整状态守卫通过才提供 DWORD。这个 API 自身没有等待/核实来源进程；调用方仍须保有正确 SessionHandle 和真实完成证据。普通同用户文件检查不是恶意进程隔离或来源签名。
- 部分准备目录和结果保留；不自动递归删除、不承诺后代停止、不生成 Windows 机时清理回执。超时启动/I/O及日志继承等缺口仍按 48 保留。

收益是让后续集成有明确的输入/输出边界；成本是 Runtime 增加 CiderPE 依赖和伴随文件生命周期。只采用准备接口，原生/Wine 运行、工具打包、参数/环境继承、完整状态和 installer detect/reboot 验收前不切换默认安装。回退可单独移除准备接口，不修改当前配方和引擎。

## 共用 PE 结构检查

新增 PEHeader，识别 PE32/PE32+，约束 OptionalHeader/NumberOfRvaAndSizes/section table 的实际文件边界；PEImage 复用它获取资源目录。旧资源解析对其他 magic 按 PE32 处理且未约束目录声明范围，此次改为无法识别时返回 nil。字段依据 [微软 PE 格式](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)。

这是结构读取，不是 Windows loader 实现、签名判断或兼容能力证明；不限制所有 PE 读取为当前 Engine R 的 CPU 类型。更严格检查可能使非规范文件失去图标/资源元数据，实际导入显示回归尚待验收；此元数据读取不会自行执行程序。

## 实际构建与剩余工作

- CLI `swift build`：退出 0，9.17 秒。
- App `swift build`：退出 0，13.13 秒。
- `scripts/build-app.sh debug`：退出 0，构建阶段 0.63 秒，out/Cider.app 完成 ad-hoc 签名；未重启 GUI/Wine。此前应用保存在 out/backups/hy1-install-20261012-DTsH1o/Cider.app。
- `git diff --check` 通过。没有测试、Windows 机时、Actions 或子代理调用。

上述构建不等于运行验收。HY1 的特殊快捷方式、Retina 交互、完整入口和首次安装显示仍有缺口；完整退出码工具仍需 48 的对照与生命周期验收。HY2 原生 kernel/context 基线、HY3 真正许可闭环、三款本地会话没有因此提升状态。Windows 的超额预约保持 pending，不能用本机交付解除、改账或挪用测试额度。
