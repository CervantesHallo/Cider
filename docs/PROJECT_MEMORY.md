# Cider 项目持久记忆

更新：2026-10-10。用于后续接管时恢复关键事实；最新用户决定优先，实施状态以 `docs/plan/13-roadmap.md` 和实际源码/记录为准。不要把提案、编译通过和真实会话验收混在一起。

## 用户目标与协作

- 目标：Apple Silicon 上达到 CrossOver 质量或更高，舒适美观的原生 UI；柚子社全系，以及原神、崩铁、绝区零官方 Windows 国服的本地适配。逐款验收登录、交互玩法、图形、音视频、输入、更新、可靠重启。
- Epic 已明确放弃。米哈游路线仅 Windows 国服，不用官方 iOS/iPadOS 版本，不外联寻求合作。
- 用户允许为目标复议工程取舍，要求具体证据、代价、验收和回退。2026-10-07 已授权立即实施评审 A01–A17；不要反复索取已有授权。
- 默认自己研究和实现；仅超大任务启用1–2个独立、范围明确的子代理，复用结果并做对抗性复核。节约 GitHub Actions 额度，不例行触发完整引擎构建。
- 2026-10-10 用户愿意自行复制执行简单的 Windows 操作；优先提供简短、可核对的命令和所需输出，不重复已失败的远程长文本输入路线。
- Git 作者和提交者只能是 CervantesHallo <227578309+CervantesHallo@users.noreply.github.com>，不加 AI 署名。

## 始终保留的边界

- 不篡改、伪装、隐藏或绕过反作弊；不以安全 API 成功空桩冒充实现。Wine 可识别；未完成兼容验证的三款游戏当前禁止本地启动。
- 不在用户已登录 Steam 的前缀副本里启动 Steam。以前这样做实际使原登录失效，用户被迫重新登录。当前副本初始配置即标记阻断，并在首次 Wine 运行前隔离启动项；保护原前缀。
- Engine R 为 x86_64 Wine/Rosetta，保留 cpu_backend 架构参数化；只建新 WoW64 64位瓶子。Wine 环境显式构造，完整 xx_YY.UTF-8 locale；msync/同步为瓶子级，切换先停止。
- Wine/依赖必须用 macOS15 SDK。macOS27 SDK 的 pipe2 检测曾使 wineboot 跳到空函数地址；本机增量构建使用 MacOSX15.4.sdk。
- D3DMetal 由用户从 GPTK 导入，不提交或打包；文档不新增法律/商业分析章节。

## 已确认的经验与当前事实

- 千恋＊万花、魔女的夜宴及 Steam 有历史运行证据；不能把它们推广成全部柚子社或全部 Windows 程序已经通过。
- 原神本地7.1.0国服安装：7777个本地清单项存在且大小一致，主程序MD5符合本地清单；Windows路径可访问。未做全部资产哈希或游戏会话。当前确定启动障碍为 Cider 门禁，原“找不到文件”弹窗/返回码尚未复现。
- HYP 顶部裁切有真实日志：client.top=82、r1 visible.top=112；本机 r2 修复后 Wine/Cocoa.top=82。2026-10-07 用户完整截图显示顶部/底部内容完整，可拖动不能缩放；用户Windows对照的最新纠正也是“不能改变大小”，以最后回复为准，不强制加缩放。Windows启动器版本未记录；按钮点击、最小化/恢复、不同显示比例仍待验收。
- 米哈游 CEF 必须有跨进程子窗口支持和 DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1。Profile 现在匹配默认安装路径及数字版本目录，不凭裸 launcher.exe/HYP 名称命中；自定义目录需要明确范围。
- 多个灰色 Dock 图标与宿主缺 LSUIElement 有关。已设 agent、保留真窗口前台晋升，7个引擎已迁移。不要改系统 Dock 固定项掩盖问题。
- 自动化读取裸/宿主 Wine 窗口仍 timeout。2026-10-07 独立的逐程序 bundle ID 已生成并实际重启HYP，但按路径/唯一ID读取仍timeout；该方案已撤回。新标识不等于视觉验收，不重复无变化重试。
- 历史 HYP 日志30.76GB且末段有msync pool exhausted。不能把它直接认定为每次白屏根因。诊断已改每文件最多4MiB头尾读取；不要全读巨型日志。
- SePrivilegeCheck/SeSinglePrivilegeCheck 的 KernelMode 成功有微软契约依据；前者还有输出标记义务。UserMode 的令牌/启用状态、集合和输出须独立实现/对照。当前 CX26.3 的 token 引用及 context 捕获/锁定/释放仍为空桩，不能拿 user-mode token API 冒充已完成依赖。矩阵 revision 2 校正了两个权限条目与模型概述；不是运行时修复或全表重新对照。
- 2026-10-07 用户授权通过 ToDesk 访问 Windows 参考机 Cervantes，手动接通并打开 PowerShell。实际只读输出：Windows11专业版10.0.26200.9457、PowerShell5.1.26100.9444、AMD64；用户完成固定提交/哈希校验采集，SDK Include26100.0有Windows.h无km/ntddk.h，VS2022 BuildTools17.14.36518.9/MSVC14.44.35207可用。不是构建或内核对照通过；EWDK VS2022候选待补齐。Include目录`.0`不能当成真实QFE。
- ToDesk坐标仍报noWindowsAvailable；复杂文本/组合键丢失，错误命令执行前清空。关键命令须核对实际输入，不执行丢字符后的代码。普通字符/回车仅能完成有限查询；不绕过执行策略或改变系统保护来补工具通道。记忆不保存设备代码、IP或凭据。
- 用户已确认获取/使用微软25H2 EWDK VS2022，约18.63GiB的ISO已完整下载到Cider缓存，SHA256为9f48251dd24ad31aac206d8256e95bda5f90a9783982c45a8aafeb9054562379。只读检查实际版本26100.6584、VS17.14.5/MSVC14.44.35207和WDK头文件。2026-10-10 用户回传Windows端相同SHA256及EWDK26100.6584/VS17.14.5横幅，但称未出现命令提示符；不能记为初始化完成，实际cl/msbuild路径和构建仍待取得。入口须LaunchBuildEnv.cmd amd64，默认参数会选x86，见research/34。
- 2026-10-10 后续截图已见CMD提示符；`where cl`实际输出镜像内MSVC14.44.35207的Hostx64/x64/cl.exe，随后用户报告5分钟未返回且无法输入，截图也无查询后的提示符。编译器文件定位不等于执行/构建通过；卡点未确认。后续优先用明确工具路径和CMD内置命令，避免再次完整扫描PATH；Ctrl+C恢复效果、Platform及MSBuild仍待回传。
- 后续窗口已恢复提示符（用户未说明使用哪种恢复步骤）；两条粘贴命令被合成`echo %Platform%dir ...`，输出前缀确认Platform=x64，但dir没有执行。改为单行明确路径调用MSBuild后，用户回传17.14.10.27608；Windows初始化及MSBuild版本输出已取得，仍没有cl执行/编译/链接或内核契约结果。简单Windows命令尽量每次一行，见research/34及初始化原始字段记录。
- Windows最大化/英文输入后短命令可用，但可靠长输入未闭合：1000字符的首段预期1002bytes、实际992bytes，哈希不符；未解码/执行，已停止后续传输。Windows镜像传输、挂载和实际工具初始化需人类协助，不重复该失败路线。
- Wine固定快照已有primary token引用/释放，CX对象映射可评估复用；请求线程映射及跨guest token读锁须一并处理，impersonation/context与权限检查仍需实现/对照，见research/33。
- A01–A17 的代码已实施并构建，故障注入、完整回归、真实Windows对照仍待完成。内核矩阵/参考规格不是内核运行时实现，启动器不是游戏可玩性。
- 2026-10-10 用户确认Mac HYP设置、最小化恢复、拖动后顶部完整。HY1路径审查后已实施瓶子感知盘符映射、helper安装范围、保存启动器同一lifecycle、模拟重启完整锁/退出码、官方配方安装环境profile绑定；App/CLI及本机debug app已构建。1×/2×、完整入口回归、首次安装渲染仍待验收，r2未一般发布。见research/35、36。
- Tools/windows-reference 已有独立Win32客户端/只读WDM参考驱动、定长无指针IOCTL和MSBuild脚本。客户端交叉编译、驱动真实WDK头文件语法检查成功，不是Windows原生构建/驱动运行。客户端只改变私有token副本并恢复线程；输出只读观察，不采集SID/用户信息。Win32结果不代替kernel契约，KernelMode合法成功不等于空桩。
- Windows采集脚本必须同时处理正/负退出状态；if errorlevel 1不等于“非零”，可能漏掉崩溃。输出流必须检查fflush与sticky ferror，参考输入/产物有来源提交及哈希；不得把部分文件作为成功记录。build.cmd只构建未签名驱动并运行Win32部分，不改变系统保护或加载驱动；原生kernel环境仍须独立补齐。
- 本轮新版out/Cider.app已通过CUA菜单退出旧主界面并重开，原5项资料库保留、Steam/HYP运行状态仍可见；点击HYP启动返回“已在运行”。米哈游瓶子仍r2、Retina off；这是GUI/已有实例分支观察，不扩大为真实停止/重启或1×/2×通过。
- Windows参考源包已发布为windows-reference-20261010-r1（实验prerelease），源提交c41b3d2，ZIP 14858bytes / SHA256 ce5bd8b0da26890e52f3c12ce0371e8943ce7902a87962cf71460f4da32eab9b；GitHub资产摘要与本地一致。已请用户在EWDK CMD以一行完整路径执行build.cmd，回传user.jsonl及client/user两份SHA256。这是原生执行的人类交接，尚未收到本工程Windows结果；不重复远程长命令、不把代码发布当作契约通过。所有子代理已关闭。
- 用户随后实际执行r1原生编译：client/reference.c第98/108行C4018，有/无符号比较；WX触发C2220，未进入驱动或采集。FIELD_OFFSET的LONG与DWORD长度比较是根因；r2用标准offsetof并显式存入DWORD，保持所有边界检查和W4/WX。本机额外Wsign-compare/Wconversion交叉编译成功；Windows r2结果待回传。不要再把本机交叉编译推广为MSVC警告兼容。

## 接管入口

1. 先读 AGENTS.md、`docs/plan/00-strategy-and-decisions.md`、`13-roadmap.md`。
2. 读 research/26–29 获取真实日志证据、评审与取舍；后续报告会继续追加。
3. 检查 git status、实际引擎/瓶子绑定、当前进程；不要盲目重做实验或重启登录副本。
4. 原顺序：收尾 HY1 → HY2 Windows 契约/依赖组 → HY3 证据到许可闭环 → HY4 原神本地会话 → HY5 另两款。遇到视觉或 Windows 参考环境缺口，明确记录范围并继续独立可推进工作，不伪造通过状态。
