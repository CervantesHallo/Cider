# Cider 项目持久记忆

更新：2026-10-07。用于后续接管时恢复关键事实；最新用户决定优先，实施状态以 `docs/plan/13-roadmap.md` 和实际源码/记录为准。不要把提案、编译通过和真实会话验收混在一起。

## 用户目标与协作

- 目标：Apple Silicon 上达到 CrossOver 质量或更高，舒适美观的原生 UI；柚子社全系，以及原神、崩铁、绝区零官方 Windows 国服的本地适配。逐款验收登录、交互玩法、图形、音视频、输入、更新、可靠重启。
- Epic 已明确放弃。米哈游路线仅 Windows 国服，不用官方 iOS/iPadOS 版本，不外联寻求合作。
- 用户允许为目标复议工程取舍，要求具体证据、代价、验收和回退。2026-10-07 已授权立即实施评审 A01–A17；不要反复索取已有授权。
- 默认自己研究和实现；仅超大任务启用1–2个独立、范围明确的子代理，复用结果并做对抗性复核。节约 GitHub Actions 额度，不例行触发完整引擎构建。
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
- HYP 顶部裁切有真实日志：client.top=82、r1 visible.top=112；本机 r2 修复后 Wine/Cocoa.top=82，30px裁切消除。点击、动态缩放、Retina与完整视觉仍待验收。
- 米哈游 CEF 必须有跨进程子窗口支持和 DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1。Profile 现在匹配默认安装路径及数字版本目录，不凭裸 launcher.exe/HYP 名称命中；自定义目录需要明确范围。
- 多个灰色 Dock 图标与宿主缺 LSUIElement 有关。已设 agent、保留真窗口前台晋升，7个引擎已迁移。不要改系统 Dock 固定项掩盖问题。
- 自动化读取裸/宿主 Wine 窗口仍 timeout。2026-10-07 独立的逐程序 bundle ID 已生成并实际重启HYP，但按路径/唯一ID读取仍timeout；该方案已撤回。新标识不等于视觉验收，不重复无变化重试。
- 历史 HYP 日志30.76GB且末段有msync pool exhausted。不能把它直接认定为每次白屏根因。诊断已改每文件最多4MiB头尾读取；不要全读巨型日志。
- SePrivilegeCheck/SeSinglePrivilegeCheck 的 KernelMode 成功有微软契约依据；UserMode 的令牌/启用状态、集合和输出须独立实现/对照。历史矩阵的“不可达”只描述指定模型，不代表所有未来架构永久不可能。
- A01–A17 的代码已实施并构建，故障注入、完整回归、真实Windows对照仍待完成。内核矩阵/参考规格不是内核运行时实现，启动器不是游戏可玩性。

## 接管入口

1. 先读 AGENTS.md、`docs/plan/00-strategy-and-decisions.md`、`13-roadmap.md`。
2. 读 research/26–29 获取真实日志证据、评审与取舍；后续报告会继续追加。
3. 检查 git status、实际引擎/瓶子绑定、当前进程；不要盲目重做实验或重启登录副本。
4. 原顺序：收尾 HY1 → HY2 Windows 契约/依赖组 → HY3 证据到许可闭环 → HY4 原神本地会话 → HY5 另两款。遇到视觉或 Windows 参考环境缺口，明确记录范围并继续独立可推进工作，不伪造通过状态。
