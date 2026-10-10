# HY1 启动路径与对抗性审计（2026-10-10）

依据当前源码 `8136d1d6b361a00d6b5957ff53d003a18f06fd05`、AGENTS.md、PROJECT_MEMORY、research/25、29、31 及本轮用户补充。仅静态读取；没有新增或运行测试、构建、启动程序、UI 自动化、提交或推送。只写本报告；修复建议交由父代理评审整合。

## 证据边界

- 最新用户确认：Mac HYP 设置窗口可打开/关闭，最小化/恢复可用，拖动后顶部内容仍完整；此前完整截图也显示下沿内容。此反馈更新 research/31 中这些交互的待验收状态，不能扩展成所有按钮、登录/验证码/更新或游戏会话通过。
- Windows/Mac 当前不能改变窗口大小的反馈一致，不强制增加尺寸能力。1×/2×、Windows 启动器版本、广泛应用回归仍未验证。
- L6 已有实际停止/重启证据：research/25:90 记录旧主 PID 78454 及辅助进程退出、新主 PID 78916 出现。这里保留历史通过范围；本轮没有重新执行，也不能由此宣称当前所有入口/并发组合通过。

## 入口覆盖与环境

以下位置均相对仓库根；行号对应上述源码版本。

| 入口 | 实际调用与判断 |
|---|---|
| GUI 资料库启动/重启/停止 | `App/Sources/Cider/Model/AppModel.swift:290–324` 捕获当前 CompatDB，创建 store/runner，进入 `AppLifecycle`。`Packages/CiderKit/Sources/CiderIntegration/AppLifecycle.swift:18–49` 在瓶子锁内检查归属、停止、重启；完整路径目标由 runner 匹配 profile。 |
| 保存的启动器 → 资料库 | `Packages/CiderKit/Sources/CiderIntegration/AppCatalog.swift:133–145` 保存 program/arguments/cwd/env；host program 另映射为 Windows 目标供进程归属使用。profile helper 名在 `:124–128` 从同一 DB 派生。 |
| Start Menu/Desktop 快捷方式 → 资料库 | `AppCatalog.swift:150–178` 解析目标、参数和 cwd，确认 exe 存在；随后走相同 lifecycle/runner。不是由 Wine 的 `start` 间接运行整个 `.lnk`。 |
| 瓶子页保存启动器的“运行” | `App/Sources/Cider/Views/BottlesView.swift:355` 转入 runCommand；自动 profile 仍能命中完整目标，但保存的 env/cwd 和实例检查缺失，见发现 3。 |
| GUI 运行命令 | `App/Sources/Cider/Model/AppModel+Bottles.swift:88–102` 新建 store，直接 plan/launch；不走 CatalogApp 的归属/已有实例检查。只按 program 匹配，不解析 `cmd /c …` 等参数中的真实目标。 |
| CLI run | `Tools/ciderctl/Sources/ciderctl/ciderctl.swift:307–334` 新建 store；绝对 host 路径展开后设置 cwd，直接 plan/launch。`--env` 覆盖 profile；原始 run 没有 app 级单实例/重启语义。 |
| 配方安装及其子进程 | `Packages/CiderKit/Sources/CiderIntegration/RecipeInstaller.swift:17–19,96–119` 用显式 db 重建 store；匹配的是缓存安装器或 `msiexec`，不是安装结果。子进程继承环境不等于重新查 profile，见发现 5。 |

共享链条已存在：`Packages/CiderKit/Sources/CiderBottle/BottleStore.swift:74–78` 向所有 `runner(for:)` 传入 CompatDB；`Packages/CiderKit/Sources/CiderRuntime/WineRunner.swift:141–151` 将 profile env 与显式 env 合并，显式值优先。`:88–138` 显式构造环境并最终固定 prefix/loader、瓶子级同步和完整 locale；`:177–207` 持锁重读配置、检查引擎/locale/sync，再重建安全环境。初始化瓶子绕过 `runner(for:)` 的 `BottleStore.swift:109` 只用于 wineboot/winecfg，不构成 HYP 入口遗漏。

HYP profile revision 4 的 `data/profiles/profile.launcher.mihoyo-cn.json:6–26` 只匹配 launcher.exe/HYP.exe/HYPHelper.exe，默认两个 C: 安装根及单层数字版本目录。`Packages/CiderKit/Sources/CiderData/CompatDB.swift:224–292` 拒绝裸名、相对路径、任意子产品目录；兼容旧 HYP profile 时仍补默认根，不恢复裸名匹配。候选按范围深度、主 exe、revision、id 排序；同 id 数据则是后目录的已接受条目覆盖（`:160–209`），不能把 revision 排序理解成已实现整个数据通道的防回退。

## 兼容快照与锁的复核

**旧 A13 的长期 store 缓存分裂已有源码修正。** `AppModel.swift:144–173` 一次 refresh 以 store.compat 构建 catalog/UI 并整体发布，sequence 拒绝迟到 refresh；`:274–275` 监听 Bottles 和用户 Data。资料库操作捕获同一快照（`:292,312`），RecipeInstaller 也明确使用传入 db，不能继续报告为“UI 新 DB、长期 runner 永远旧 DB”。

**剩余范围：**Data 事件经过合并延迟（`AppModel.swift:213–220`），CLI/运行命令新建 store 可先读到下一份数据；checkout data/CIDER_DATA 没有对应文件监听。已运行 HYP 的子进程仍继承旧会话环境，热更新不会追溯改环境。CatalogApp 的 helper 名也是构建时的值，API 没有 DB revision 绑定；外部调用者混用旧 app/新 runner 是条件性风险，本轮未证实 GUI 稳定 refresh 后发生该混用。统一 reload 边界、按当前快照重新派生 helper 并记录实际 profile id/revision 是候选完善，不是本轮已采用或已验证的修复。

**同步重入正确有据。** `Packages/CiderKit/Sources/CiderCore/FileSafety.swift:90–122` 用规范路径的 NSRecursiveLock、线程内重入标记和 flock。`BottleStore.swift:158–169` 与 `WineRunner.swift:166–168` 使用同一外部 State 瓶子锁；`AppLifecycle.restart → start → launch` 是同线程同步嵌套，没有跨 await，内层不会重新取得一个 flock 而自锁。当前查到的 launch 顺序为瓶子→引擎→宿主元数据/审计；`EngineStore.swift:144–147,178–189` 持引擎锁只读空闲状态，没有反向获取瓶子锁的源码证据。

**不能扩大锁保证。** flock 只约束同一 State 的合作入口，不约束 Wine 内部 CreateProcess。launch 在 spawn/audit 后释放锁（`WineRunner.swift:222–236`）；app 扫描默认不含尚未改写为 Windows image 的 loader（`ProcessScanner.swift:44–55`，`AppLifecycle.swift:20,35`）。因此初始 loader 窗口及延迟自重生进程仍是实例/静止判断的并发风险；两次静默扫描（`:58–71`）不是整棵进程树永久静止证明。本轮没有运行时复现，不宣称发生了重复实例或丢进程。前缀整体停止额外检查 loader、孤儿和 server（`WineRunner.swift:261–277`）；不得与应用级 stop 混为同一保证。各层锁有独立等待预算，不是整项操作固定 30 秒。

## 五项可行动发现

### 1. P2／源码确定：host 路径匹配未锚定当前瓶子

位置：`Packages/CiderKit/Sources/CiderData/CompatDB.swift:227,269–292`、`Packages/CiderKit/Sources/CiderRuntime/WineRunner.swift:145`。任何绝对 host 路径只要有一个名为 `drive_c` 的分段，就把其后内容当 C:。例如在瓶子 A 运行 `/tmp/drive_c/Program Files/miHoYo Launcher/launcher.exe`，仍会自动取得 HYP env；实际该路径可能在 Z: 或另一映射盘，与 A 的 drive_c 无关。`Packages/CiderKit/Sources/CiderIntegration/SteamLibrary.swift:154–165` 已有按实际瓶子映射选择 C:/Z:/dosdevices 的不同语义。该误匹配可由条件直接推导；未执行该反例。

最小建议：让 profile lookup 接收当前 drive_c/映射上下文；runner 和 catalog 都只将当前瓶子中已确认的 host 根转换为对应 Windows 路径。不能只检查目录名，也不能只修 runner 而保留 catalog 的错误 helper 名。这是范围修复，不要求 PE 身份认证或放宽自定义安装目录。

### 2. P2／源码确定：scoped HYP helper 归属比 profile 范围更宽

位置：`Packages/CiderKit/Sources/CiderIntegration/AppCatalog.swift:73–83`。主程序目录下任意深度的同名声明 helper 都归属，只排除第一个子分段恰为 games/steamapps 的路径。root launcher 可以拥有 `<root>/tools/HYPHelper.exe`，甚至 `<root>/1.18.0/games/Other/HYPHelper.exe`；这些目录在 `CompatDB.swift:254–264` 的 HYP 范围检查中均不匹配。AppLifecycle stop/restart 会对这种误归属执行终止。此为静态范围反例，未证实用户安装里存在这种独立程序或发生误杀。

最小建议：把匹配到的 HYP 安装根/数字版本范围随 CatalogApp 传给 owns，helper 必须位于获准目录，并保留 extensionless HYPHelper 的精确别名。不要全局改成“只能同目录”：既有其他产品有 updater 子目录语义（只读查看 `Packages/CiderKit/Tests/CiderKitTests/CatalogTests.swift:33–38`，未运行）；需要独立显式 helper 目录时另声明，不泛化递归。

### 3. P2／源码确定：同一保存启动器在两个 GUI 入口丢失字段

位置：`App/Sources/Cider/Views/BottlesView.swift:355` → `App/Sources/Cider/Model/AppModel+Bottles.swift:88–102`，对照 `Packages/CiderKit/Sources/CiderIntegration/AppCatalog.swift:142–144`、`AppLifecycle.swift:22–25`。`BottleConfig.Launcher` 的 environment/workingDirectory 是有效字段（`Packages/CiderKit/Sources/CiderSchema/BottleConfig.swift:58–68`），资料库使用它们，瓶子页按钮只传 program/arguments。例如显式 DXMT 值覆盖及依赖 cwd 的命令会产生不同计划；瓶子页也不执行同一启动器的已有实例检查。自动 HYP profile 并非完全缺失，丢失的是保存值和 app 语义。

最小建议：瓶子页按 launcher id 找当前 CatalogApp，转入同一个 launch/lifecycle 操作，保留 env/cwd；不要将保存启动器降成裸 runCommand。普通运行命令的自由多实例语义可以继续独立存在。

### 4. P2／源码确定：模拟重启不是跨进程完整事务

位置：`App/Sources/Cider/Model/AppModel+Bottles.swift:130–135`。killAll 退出时已释放瓶子锁，随后 runToCompletion 另行获取 launch 锁；两者之间外部 CLI 可以启动程序，或保存新配置。GUI busy 只能挡同一 AppModel。实际 AppLifecycle.restart 已持外层锁（`AppLifecycle.swift:46–50`），不能据此宣称 simulateReboot 也有相同保证。此外这里丢弃 wineboot 返回码，非零也会进入“模拟重启完成”的 done 文案。

最小建议：用 `store.withOperation(bottle)` 包住停止与 wineboot 完成，从锁内 current 创建 runner；检查返回码再报告完成。保留停止失败传播。此处没有运行并发复现，也不否定 L6 历史 app 级重启证据。

### 5. 覆盖缺口／待采用提案：安装器启动的 HYP 没有自动取得目标 profile

位置：`Packages/CiderKit/Sources/CiderIntegration/RecipeInstaller.swift:115–119`、`data/recipes/launcher.mihoyo-cn.json:25,28–34`、`WineRunner.swift:145`。当前缓存名为 mihoyo_launcher_setup.exe；它不是 HYP profile 的匹配对象，step 也没有 env/profile 字段（`Packages/CiderKit/Sources/CiderData/Recipe.swift:36–45`）。如果安装器直接启动 HYP，Cider 不会再次调用 plan 为子目标补 DXMT env。GUI 安装后仅对 recipe.launch 明示的程序发起正常资料库启动（`AppModel+Bottles.swift:322–325`），当前 HYP recipe 没有 launch。

子进程继承依据：本地已缓存的 r1 源码 `~/Library/Caches/Cider/engine-build/cider-cx26.3-r1-x86_64/src/dlls/ntdll/unix/process.c:295–320,919–989` 保留 Unix 环境，通过 fork/loader 启动，只有专用 PE 环境提升等显式更新。它支持“正确启动的 HYP 把 DXMT=1 传给 CEF 子进程”，不支持“任何安装器子进程自动查 Cider profile”。此为指定缓存源码阅读，不是重新核验已安装 r2 二进制。

确定的是当前 Cider 计划未注入该值；未观察本版安装器是否自动启动 HYP、自行设置变量或产生显示故障，故不列为已复现渲染缺陷。候选：为指定官方配方增加经校验的安装步骤 profile/env 绑定，或提供安装后从获准实际目标启动的显式流程。收益是覆盖安装后第一次 HYP 会话；代价是前者的 env 会继承整个安装器子树，后者需处理安装器已有实例。验收须分别记录 installer/HYP/CEF 的实际 env 和首次渲染；回退撤销该配方绑定/流程，保持正常直接启动 profile。不要把裸 setup/launcher 名扩进全局 HYP 匹配范围。

## 整合状态

共享 runner、路径范围、CompatDB 快照刷新、同步重入与 app restart 外层锁属于已有实施；上述 1–4 是本轮静态确认的缺陷，5 是覆盖缺口及未采用提案。没有实施任何修复或扩大验收结论。父代理可据此调整 HY1 的交互状态和待修项，并继续独立推进 HY2；源码审计、用户启动器交互和完整游戏兼容各有自己的证据范围。
