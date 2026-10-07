# 当前源码接管评审（2026-10-07）

基线提交：`c0809d8`。审查包含此前的实现及接管后的改动，按当前源码、实际调用链和交付证据判断。两个范围独立的只读子代理分别检查存储/安装和运行/数据/UI；主代理复核下列关键调用链，并完成 Dock 修复。代理报告本身不作为运行验证。

## 结论与方向

总体分层值得保留：CiderKit 承载业务逻辑，App/CLI 共享能力；Wine 环境显式构造；同步和完整 locale 保持瓶子级；PE 解析已有边界/深度限制；图形和引擎分别建模。没有充分依据重写整个项目。

现有实现属于开发预览，距离可靠的平台还有明显差距。主要问题集中在多文件操作的失败恢复、并发/配置提交、导入边界和证据一致性。资料库、启动器渲染和已安装文件不能代替游戏可玩性；矩阵和路线图不能代替内核实现。

Engine R 作为当前已有运行证据的主线合理。米哈游三款的忠实兼容目标保持，但通用内核契约、宿主保障覆盖与游戏/服务端接受必须分别取得证据。不能宣称“补齐 KMDF 就必然可玩”；也不能仅因当前 Wine 缺某能力断言所有忠实实现路线永远不可能。

本轮只有 Dock 所需的 EngineHost/WineRunner 代码改变；以下其他问题均为**拟议修正，尚未实施**。P1 表示应优先修复的实质风险，P2 表示随可靠性工作修复；静态可触发条件不等于用户已经遭遇损失。

## 已修：Dock 辅助图标

用户图中多枚空白图标符合当前宿主包装缺少 LSUIElement 与备用图标的缺陷。该包装含接管后改动，不能全部归责于之前的代理。

Wine 原 `loader/wine_info.plist.in` 带 LSUIElement；Cider 自建宿主没有。现在宿主默认 agent，Wine 的 `transformProcessToForeground` 在窗口显示/聚焦等实际 UI 路径保留前台晋升能力。没有将所有 Windows GUI 强制隐藏，也没有删除用户 Dock 固定项。

- 7 个已安装引擎的宿主 plist 已迁移；后续每次实际 launch 自动幂等准备，覆盖 GUI、CLI、配方、wineboot 和已有瓶子。
- 备用 Cider 图标在 App 入口复制，Windows 原图标仍由 Wine 设置；已有图标在 CLI 入口保留。
- 链接和 plist 无变化时不重写；引擎级 flock 有 5 秒上限，链接先暂存再原子 rename；可选图标失败只降级记录。
- 对抗性审查纠正了两个实现问题：准备放在 runner getter 会阻碍停止，已移到 launch；并发准备和可选图标失败，分别以锁/原子替换和降级修正。
- 宿主原 plist 备份在本机 `/tmp/cider-dock-hosts-before-20261007`。恢复备份并使用此前 App/CLI 可回退；没有改变引擎 native 模块、游戏文件或反作弊。

依据：[Apple LSUIElement](https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Articles/LaunchServicesKeys.html#//apple_ref/doc/uid/20001402-108256)。本地 App/CLI 构建通过，米哈游在新宿主下重新启动且主/辅助进程均被 Cider 正确识别。自动化工具读取 Dock/该 Wine 窗口仍返回 timeout，故没有取得修复后 Dock 截图或声称逐个 Windows 软件完整视觉验收。通用入口已修，与全目录实际软件验收分开记录。

## 原评审修正清单（已被纳入实施）

2026-10-07 用户随后授权现在实施；下表保留原缺陷证据，当前进度以 `docs/plan/13-roadmap.md` A01–A17 与 research/29 为准。

| # | 优先级 / 类型 | 源码证据与触发 | 后果 | 最小建议与验收 |
|---|---|---|---|---|
| A01 | P1 / 现有缺陷 | `CiderBottle/BottleStore+Archive.swift:50–58`、`CiderStore/EngineStore.swift:138–145`，解码 ID 后直接构造目的路径；缺单目录名与 containment 校验 | 不可信导入 ID 可使安装/替换越出管理根；replace 先移动既有目标 | 统一 ID/路径约束，解析符号链接后的边界检查；所有写操作之前拒绝越界；合成归档覆盖相对路径和链接父目录 |
| A02 | P1 / 现有缺陷 | `CiderIntegration/PatchInstaller.swift:80–94,116–120`，目的目录可沿已有 symlink，undo 信任 imported manifest 的相对路径 | 安装/撤销可能修改游戏根外的文件 | 共用安全路径解析与清单校验；外部哨兵文件须保持原样 |
| A03 | P1 / 现有缺陷 | `PatchInstaller.swift:85–94`，先移动原文件，全部复制后才写 manifest，无错误恢复 | 复制/磁盘失败可留下半更新或缺文件；备份不一定被 UI 识别 | 写前事务记录，失败逆序恢复，中断恢复可见；逐阶段故障注入 |
| A04 | P1 / 现有缺陷 | `RecipeInstaller.swift:137–139`，缓存命中返回目录首项，跳过 159–162 的 hash 校验 | 损坏、替换或额外缓存条目可能直接交给安装器 | 固定文件选择、类型检查、每次消费校验；坏缓存不执行并可重新下载 |
| A05 | P1 / 规则冲突风险 | `BottleStore+Archive.swift:104,118` 复制外部前缀后 wineboot -u；`BottleStore.swift` 已记录 Run 项可启动 Steam | 保存登录的外部前缀被克隆后，首次启动可能刷新 Steam 登录 | 首次克隆运行前抑制自动启动项；仅以无账户合成 Run 项建立基线，禁止用登录克隆实验 |
| A06 | P1 / 现有缺陷 | `CiderCore/Identifiers.swift:22` 只有16位后缀；create/duplicate/import 不排他建目录 | 碰撞可能覆盖已有配置；失败清理可能删除非本次创建目录 | UUID/足够随机性 + 排他创建 + 碰撞重试；明确本次目录所有权；固定碰撞反例 |
| A07 | P1 / 现有缺陷 | `BottleStore.swift:184`、`BottleStore+Archive.swift:23`、`BottleStore+Management.swift:67` 吞掉停止失败；restore 依赖该快照 | 活跃前缀可能被快照/复制/替换，成功提示与实际静止状态不符 | 停止成功和静止确认设为必要条件，传播错误；失败时不得开始复制/替换 |
| A08 | P1 / 现有缺陷 | `EngineStore.swift:143–150` 先撤旧引擎再复制检查；Archive.swift:33 先删同名旧导出 | 新产物失败后旧引擎脱离原位置；旧导出不可恢复且可能只剩半包 | 完成暂存/检查后提交；提交失败恢复旧产物；旧备份字节保持完整 |
| A09 | P1 / 现有缺陷 | `AppModel.swift:181–190` 后台 upgrade 无 activity 锁，`BottleStore.swift:173–176` 以捕获的旧 config 整份写回 | 升级和改名、locale、启动器、引擎设置交错时可丢失已保存改动 | 统一跨入口操作协调，持锁重读配置，只提交变更字段；不同字段交错验证 |
| A10 | P1 / 现有缺陷 | `CompatDB.swift:158–175,302–306` 在枚举时校验 profile，依赖当时 target；最终覆盖后不复查 | 门控专属红线检查受文件/目录顺序影响；当前硬门禁仍独立阻断 | 解析/最终覆盖/校验分阶段，按最终 target 检查；顺序与覆盖交换结果一致 |
| A11 | P1 / 保证缺口 | `WineRunner.swift` 只预检自身 argv；`AppModel.swift:210–213` 事后扫描；导入普通 Wine 未检查 engine gate capability | 无子进程门禁的引擎不能保证“受限 exe 零启动”；CLI 也无 UI 扫描兜底 | 相关启动器要求已验证子进程阻断能力；未知能力拒绝；只用自有普通父子程序验收 |
| A12 | P2 / 现有缺陷 | `WineRunner.swift:170–175` spawn 后 appendAudit 可抛错 | 实际程序启动，但返回失败并丢失 session handle；用户重试可能重复启动 | 启动前准备审计条件；启动后失败保留所有权并明确真实状态；分阶段注入 I/O 失败 |
| A13 | P2 / 现有缺陷 | `BottleStore.swift:24` 缓存 DB，App refresh 单独重新加载且目录不同 | UI 显示新 profile，runtime/helper 归属仍用旧数据 | 同一可替换 revision 快照给 UI/catalog/runner；热更新 env/helper 后三处一致 |
| A14 | P2 / 现有缺陷 | `CompatDB.swift:205–209` 按首个 exe basename 匹配；米哈游匹配通用 launcher.exe | 同名其他程序收到错误的设置、说明与辅助进程归属 | 绑定已识别安装或 PE/渠道信息，匹配冲突确定化；两个同名不同程序隔离 |
| A15 | P2 / 现有缺陷 | `Diagnostics.swift:69` UTF-8 失败原样返回；截断可能切开字符 | 诊断包中的个人路径/用户名可能未脱敏 | 容错解码或安全字节处理，不可原样导出未知文本；坏字节与截断边界反例 |
| A16 | P1 / 资源预算风险 | `Diagnostics.swift:29–31` 完整读取日志后才 truncated；本机历史已有30.76GB日志 | 无有界读取保证，诊断可能长时间阻塞或造成内存压力 | FileHandle 仅读预算内头尾；先 stat 再有界读取；用稀疏大文件测资源预算 |
| A17 | P2 / 维护与方向偏差 | 02 的补丁选择表称不取 Highball0012/待定0013，实际配方已带；KernelFidelity 的 unreachable 注释过于绝对 | 计划与真实构建不一致；历史模式判定被误当永久架构定论 | 保留历史证据、另写当前状态；按构建/模式/宿主能力复核可达性，不改标签冒充实现 |

上述路径在 `Packages/CiderKit/Sources/` 下，AppModel 在 `App/Sources/Cider/Model/` 下。行号对应本轮源文件；范围用于定位，不能替代读完整调用链。

## 明确尚未实现的能力

- HY3 的版本/环境 Verdict → UI/宿主/引擎许可闭环。当前固定 deny 保持阻断；更改 JSON 不会自动完成闭环。
- 真正的 KMDF 运行时和 Windows 对照后的权限/回调契约修复。当前 ntoskrnl 基线存在空桩；矩阵的 disposition 是目标。
- 完整 engine/data 签名通道、D3DMetal 强制来源验签（当前记录结果）、全局多进程操作协调、Game Mode/TCC/逐程序 shim 的完整验收。
- r2 的点击、动态缩放、1×/2×和完整视觉/图形回归；原神/崩铁/绝区零本地会话。

这些不能都叫“已实现能力发生 bug”，也不能因此把目标或模块结构全部推倒。

## 按原计划继续的具体路线

**已采用：**Dock 必要修正及其失败/并发控制，保持引擎基线和游戏门禁；本轮无其他功能代码重构。

**建议先后顺序（尚未实施）：**先处理 A07/A08/A03/A06 的数据保持与停止失败，随后 A01/A02/A04/A10/A11 的输入/来源与红线边界，再 A09/A12/A13/A14/A15/A16。它们支持原计划可靠性目标，具体变更各自留回退点。

继续 HY2 的下一份交付是权限上下文的 Windows 基线规格，见 [research/28](28-security-context-baseline-20261007.md)。Microsoft 明确 KernelMode 的特权检查可合法成功；应检查 UserMode 的令牌、启用状态、集合逻辑和输出，不应把全部 TRUE 统一改 FALSE。[SePrivilegeCheck](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seprivilegecheck)、[SeSinglePrivilegeCheck](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-sesingleprivilegecheck)

规格和源码复核不冒充 Windows 对照结果；没有通过游戏/服务端证据之前继续阻断本地游戏启动。

## 本轮工作边界

没有启动实际三款游戏、修改其文件/保护、生成绕过方案、改变 macOS 安全设置、联系任何外部人员。没有运行测试套件或 GitHub Actions。App/CLI 构建、元数据迁移与启动器运行观察属于已执行项；其余故障注入与 Windows 对照是建议验收项。
