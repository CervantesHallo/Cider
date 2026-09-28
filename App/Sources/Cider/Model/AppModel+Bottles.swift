import CiderBottle
import CiderCore
import CiderData
import CiderIntegration
import CiderRuntime
import AppKit
import CiderSchema
import CiderStore
import Foundation

/// Wine's own Windows tools, opened from a bottle's page (CrossOver's "Wine Configuration", "Registry Editor", …).
enum WindowsTool: String, CaseIterable, Identifiable {
    case winecfg, regedit, taskmgr, control, cmd, uninstaller, explorer
    var id: String { rawValue }

    var title: String {
        switch self {
        case .winecfg: return "Wine 设置"
        case .regedit: return "注册表编辑器"
        case .taskmgr: return "任务管理器"
        case .control: return "控制面板"
        case .cmd: return "命令提示符"
        case .uninstaller: return "卸载或更改程序"
        case .explorer: return "文件资源管理器"
        }
    }

    var symbol: String {
        switch self {
        case .winecfg: return "slider.horizontal.3"
        case .regedit: return "list.bullet.indent"
        case .taskmgr: return "chart.bar.xaxis"
        case .control: return "switch.2"
        case .cmd: return "terminal"
        case .uninstaller: return "trash.square"
        case .explorer: return "folder"
        }
    }

    var command: (program: String, arguments: [String]) {
        switch self {
        case .cmd: return ("wineconsole", ["cmd"])
        case .explorer: return ("explorer", [#"C:\"#])
        default: return (rawValue, [])
        }
    }
}

extension AppModel {
    func snapshots(of bottle: Bottle) -> [BottleSnapshot] { BottleStore(paths: paths).snapshots(of: bottle) }

    /// Runs a CiderKit operation off the main actor with a busy key and a result message, then refreshes.
    private func perform(_ key: String, _ start: String?, done: @escaping @Sendable (Bottle?) -> String,
                         _ work: @escaping @Sendable (BottleStore) throws -> Bottle?) {
        let paths = self.paths
        busy.insert(key)
        if let start { message = start }
        Task {
            defer { busy.remove(key) }
            do {
                let result = try await Task.detached { try work(BottleStore(paths: paths)) }.value
                message = done(result)
                await refresh()
            } catch {
                message = "操作失败：\(error)"
            }
        }
    }

    func runTool(_ tool: WindowsTool, in bottle: Bottle) {
        let (program, arguments) = tool.command
        perform("tool:\(bottle.config.id):\(tool.rawValue)", nil, done: { _ in "已打开\(tool.title)。" }) { store in
            let runner = try store.runner(for: bottle)
            _ = try runner.launch(runner.plan(program: program, arguments: arguments, label: tool.rawValue))
            return nil
        }
    }

    /// "运行命令" / "带选项运行": starts a program with arguments; `verbose` records a detailed Wine log.
    func runCommand(program: String, arguments: [String], in bottle: Bottle, verbose: Bool) {
        let paths = self.paths
        message = "正在启动 \(program)…"
        Task {
            do {
                let log = try await Task.detached { () -> URL in
                    let store = BottleStore(paths: paths)
                    let runner = try store.runner(for: bottle)
                    let label = URL(fileURLWithPath: program.replacingOccurrences(of: "\\", with: "/")).deletingPathExtension().lastPathComponent
                    return try runner.launch(runner.plan(program: program, arguments: arguments, label: label,
                                                         debug: verbose ? .verbose : .default)).log
                }.value
                message = verbose ? "已启动，详细日志：\(log.path)" : "已启动 \(program)。"
                lastCommandLog = log
                await scanProcesses()
            } catch let block as Preflight.Block {
                message = nil
                showRouteCard(block)
            } catch {
                message = "启动失败：\(error)"
            }
        }
    }

    func saveLauncher(name: String, program: String, arguments: [String], in bottle: Bottle) {
        let launcher = BottleConfig.Launcher(id: Identifiers.make(from: name, fallback: "launcher"), name: name,
                                             program: program, arguments: arguments)
        perform("launcher:\(bottle.config.id)", nil, done: { _ in "已保存启动器“\(name)”，它会出现在资料库里。" }) { store in
            try store.saveLauncher(launcher, in: bottle)
        }
    }

    func removeLauncher(id: String, in bottle: Bottle) {
        perform("launcher:\(bottle.config.id)", nil, done: { _ in "已删除启动器。" }) { store in
            try store.removeLauncher(id: id, from: bottle)
        }
    }

    /// CrossOver's "Simulate Reboot": `wineboot -r` runs what Windows runs at restart (RunOnce, pending renames).
    func simulateReboot(_ bottle: Bottle) {
        perform("reboot:\(bottle.config.id)", "正在模拟重启“\(bottle.config.name)”…", done: { _ in "模拟重启完成。" }) { store in
            let runner = try store.runner(for: bottle)
            try runner.killAll()
            _ = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["-r"], label: "wineboot-restart"))
            return nil
        }
    }

    func takeSnapshot(_ bottle: Bottle) {
        perform("snapshot:\(bottle.config.id)", "正在创建快照（会先结束瓶子里的程序）…", done: { _ in "快照已创建。" }) { store in
            try store.snapshot(bottle, reason: "手动快照")
            return nil
        }
    }

    func restore(_ bottle: Bottle, to snapshot: BottleSnapshot) {
        perform("snapshot:\(bottle.config.id)", "正在恢复到快照…", done: { _ in "已恢复。恢复前的状态也存成了一个快照。" }) { store in
            try store.restore(bottle, to: snapshot)
        }
    }

    func deleteSnapshot(_ snapshot: BottleSnapshot, of bottle: Bottle) {
        perform("snapshot:\(bottle.config.id)", nil, done: { _ in "快照已删除。" }) { store in
            try store.deleteSnapshot(snapshot)
            return nil
        }
    }

    func renameBottle(_ bottle: Bottle, to name: String) {
        perform("rename:\(bottle.config.id)", nil, done: { b in "已改名为“\(b?.config.name ?? name)”。" }) { store in
            try store.rename(bottle, to: name)
        }
    }

    func duplicateBottle(_ bottle: Bottle) {
        let name = "\(bottle.config.name) 副本"
        perform("duplicate:\(bottle.config.id)", "正在复制“\(bottle.config.name)”…", done: { _ in "已复制为“\(name)”。" }) { store in
            try store.duplicate(bottle, name: name)
        }
    }

    func deleteBottle(_ bottle: Bottle) {
        perform("delete:\(bottle.config.id)", nil, done: { _ in "“\(bottle.config.name)”已移到废纸篓。" }) { store in
            try store.delete(bottle)
            return nil
        }
    }

    /// Sync is bottle-wide and fixed when wineserver starts, so the bottle is stopped before the change.
    func setSync(_ mode: SyncMode, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", nil, done: { _ in "同步方式已改为 \(mode == .msync ? "msync" : "wineserver")，下次启动生效。" }) { store in
            try? store.runner(for: bottle).killAll()
            return try store.update(bottle) { $0.settings[SyncMode.settingKey] = mode.rawValue }
        }
    }

    func setHighResolution(_ on: Bool, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", "正在\(on ? "开启" : "关闭")高分辨率模式（会先结束瓶子里的程序）…",
                done: { _ in on ? "高分辨率模式已开启：程序按 Retina 像素渲染，DPI 192。" : "高分辨率模式已关闭。" }) { store in
            try store.setHighResolution(on, for: bottle)
        }
    }

    func setLocale(_ locale: BottleLocale, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", nil, done: { _ in "区域已改为 \(locale.lcAll)，下次启动程序时生效。" }) { store in
            try store.update(bottle) { $0.locale = locale }
        }
    }

    func switchEngine(_ engineID: String, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", "正在切换引擎（先自动快照）…", done: { _ in "已切换到 \(engineID)。可在快照里回滚。" }) { store in
            try store.switchEngine(bottle, to: engineID)
        }
    }

    func setWindowsVersion(_ template: BottleTemplate, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", nil, done: { _ in "Windows 版本已改为 \(template.winver)。" }) { store in
            let runner = try store.runner(for: bottle)
            _ = try runner.runToCompletion(runner.plan(program: "winecfg", arguments: ["/v", template.winver], label: "winecfg-version"))
            return try store.update(bottle) { $0.template = template }
        }
    }

    /// Support bundle in ~/Downloads (user name and home path removed), revealed in Finder.
    func makeDiagnostics(for bottle: Bottle) {
        let paths = self.paths
        message = "正在生成诊断包…"
        Task {
            do {
                let zip = try await Task.detached {
                    try Diagnostics.bundle(for: bottle, store: BottleStore(paths: paths),
                                           to: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Downloads"))
                }.value
                message = "诊断包已存到“下载”：\(zip.lastPathComponent)（已去掉用户名和个人路径）"
                revealInFinder(zip)
            } catch {
                message = "生成诊断包失败：\(error)"
            }
        }
    }

    func exportBottle(_ bottle: Bottle) {
        perform("export:\(bottle.config.id)", "正在导出“\(bottle.config.name)”（会先结束瓶子里的程序）…", done: { _ in "已导出到“下载”。" }) { store in
            let file = try store.exportArchive(bottle, to: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Downloads"))
            Task { @MainActor in NSWorkspace.shared.activateFileViewerSelecting([file]) }
            return nil
        }
    }

    func importArchive(_ url: URL) {
        perform("import", "正在导入 \(url.lastPathComponent)…", done: { b in "已导入瓶子“\(b?.config.name ?? "")”。" }) { store in
            try store.importArchive(url)
        }
    }

    func foreignBottles() -> [ForeignBottle] { BottleStore(paths: paths).foreignBottles() }

    func importForeign(_ foreign: ForeignBottle, locale: BottleLocale) {
        perform("import", "正在从 \(foreign.source.rawValue) 复制“\(foreign.name)”（原瓶子不会改动）…",
                done: { b in "已导入“\(b?.config.name ?? foreign.name)”，里面的程序会出现在资料库。" }) { store in
            try store.importForeign(foreign, locale: locale)
        }
    }

    /// Installs a recipe into an existing bottle, or into a new one made from the recipe's hint (`bottleID == nil`).
    /// Launchers are started afterwards so the user can sign in right away.
    func installRecipe(_ recipe: Recipe, bottleID: String?) {
        let paths = self.paths
        let db = compat
        busy.insert("recipe:\(recipe.id)")
        message = "正在准备安装 \(recipe.title())…"
        Task {
            defer { busy.remove("recipe:\(recipe.id)") }
            do {
                let bottle = try await Task.detached { () -> Bottle in
                    let store = BottleStore(paths: paths)
                    let target = try bottleID.map { try store.bottle($0) } ?? store.create(
                        name: recipe.bottle?.name ?? recipe.title(),
                        locale: recipe.bottle?.locale.flatMap(BottleLocale.named) ?? .simplifiedChinese,
                        createdBy: "Cider.app recipe \(recipe.id)")
                    try RecipeInstaller(store: store, db: db).install(recipe.id, in: target) { step in
                        Task { @MainActor in self.message = step }
                    }
                    return target
                }.value
                message = "\(recipe.title()) 已安装到瓶子“\(bottle.config.name)”。"
                await refresh()
                if let program = recipe.launch, let item = items.first(where: {
                    $0.bottleID == bottle.config.id && $0.launchProgram.caseInsensitiveCompare(program) == .orderedSame
                }) {
                    launch(item)
                }
            } catch {
                message = "安装 \(recipe.title()) 失败：\(error)"
            }
        }
    }

    func programWindowsVersion(_ item: LibraryItem) -> String? {
        guard let exe = item.executableName, let bottle = bottles.first(where: { $0.config.id == item.bottleID }) else { return nil }
        return BottleStore(paths: paths).registryString(#"HKCU\Software\Wine\AppDefaults\"# + exe, "Version", in: bottle)
    }

    func setProgramWindowsVersion(_ winver: String?, for item: LibraryItem) {
        guard let exe = item.executableName, let bottle = bottles.first(where: { $0.config.id == item.bottleID }) else { return }
        perform("settings:\(item.id)", nil, done: { _ in winver.map { "\(exe) 现在按 Windows \($0.dropFirst(3)) 运行（下次启动生效）。" } ?? "\(exe) 恢复为跟随瓶子的 Windows 版本。" }) { store in
            try store.setWindowsVersion(winver, forExecutable: exe, in: bottle)
            return nil
        }
    }

    func setMetalHUD(_ on: Bool, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", nil, done: { _ in on ? "性能 HUD 已开启：之后启动的程序会显示帧率（Metal 渲染的游戏）。" : "性能 HUD 已关闭。" }) { store in
            try store.update(bottle) { $0.settings[BottleStore.metalHUDKey] = on ? "1" : "0" }
        }
    }

    /// Creates a bottle in the installer's language, then runs the installer in it (Japanese galgame installers
    /// in a Chinese bottle often show mojibake or fail).
    func installIntoNewBottle(_ url: URL, locale: BottleLocale, name: String) {
        let paths = self.paths
        busy.insert("install")
        message = "正在新建瓶子“\(name)”（\(locale.displayName)）…"
        Task {
            do {
                let bottle = try await Task.detached {
                    try BottleStore(paths: paths).create(name: name, locale: locale, createdBy: "Cider.app installer")
                }.value
                busy.remove("install")
                await refresh()
                runInstaller(url, in: bottle.config.id)
            } catch {
                busy.remove("install")
                message = "新建瓶子失败：\(error)"
            }
        }
    }

    func importD3DMetal(_ url: URL, completion: @escaping @MainActor () -> Void) {
        let paths = self.paths
        busy.insert("d3dmetal")
        message = "正在导入 D3DMetal…"
        Task {
            defer { busy.remove("d3dmetal") }
            do {
                let info = try await Task.detached { try D3DMetalImporter(paths: paths).importPackage(at: url) }.value
                message = "已导入 D3DMetal \(info.version)\(info.codesignValid ? "" : "（注意：签名未通过验证）")。"
                completion()
            } catch {
                message = "导入失败：\(error)"
            }
        }
    }

    /// Installs Rosetta through the system's own administrator prompt (the user types the password there).
    func installRosetta() {
        message = "正在安装 Rosetta…"
        Task {
            let ok = await Task.detached { () -> Bool in
                let script = #"do shell script "/usr/sbin/softwareupdate --install-rosetta --agree-to-license" with administrator privileges"#
                return (try? Command.run("/usr/bin/osascript", ["-e", script])) != nil
            }.value
            message = ok ? "Rosetta 已安装。" : "没有完成 Rosetta 的安装（可能取消了密码框）。"
            await refresh()
        }
    }

    /// Installs an engine package (.tar.xz) or a built engine directory.
    func importEngine(_ url: URL) {
        let paths = self.paths
        busy.insert("engine")
        message = "正在导入引擎…"
        Task {
            defer { busy.remove("engine") }
            do {
                let engine = try await Task.detached { () -> InstalledEngine in
                    var isDir: ObjCBool = false
                    let store = EngineStore(paths: paths)
                    return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir) && isDir.boolValue
                        ? try store.installBuilt(directory: url) : try store.install(package: url)
                }.value
                message = "已导入引擎 \(engine.manifest.id)。"
                await refresh()
            } catch {
                message = "导入引擎失败：\(error)"
            }
        }
    }

    func repairBottle(_ bottle: Bottle) {
        perform("repair:\(bottle.config.id)", "正在修复“\(bottle.config.name)”（先自动快照）…", done: { _ in "修复完成。如果问题还在，可以在快照里回滚。" }) { store in
            try store.repair(bottle)
        }
    }

    func setAdvertiseAVX(_ on: Bool, for bottle: Bottle) {
        perform("settings:\(bottle.config.id)", nil, done: { _ in on ? "已向程序报告 AVX 支持（之后启动的程序生效）。" : "已关闭 AVX 报告。" }) { store in
            try store.update(bottle) { $0.settings[BottleStore.advertiseAVXKey] = on ? "1" : "0" }
        }
    }

    /// The engine index shipped with the app (data/engines/index.json).
    var engineIndex: EngineIndex? {
        Bundle.main.resourceURL.flatMap { EngineIndex.load(from: $0.appendingPathComponent("data/engines/index.json")) }
    }

    /// Downloads, verifies and installs an engine from the index, reporting progress in `engineDownloadProgress`.
    func downloadEngine(_ entry: EngineIndex.Entry) {
        let paths = self.paths
        busy.insert("engine")
        message = "正在下载引擎 \(entry.id)（\(ByteCountFormatter.string(fromByteCount: entry.size, countStyle: .file))）…"
        engineDownloadProgress = 0
        let downloader = EngineDownloader(store: EngineStore(paths: paths))
        let poll = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            let got = downloader.downloadedBytes(for: entry)
            Task { @MainActor in self?.engineDownloadProgress = min(1, Double(got) / Double(max(entry.size, 1))) }
        }
        Task {
            defer { poll.invalidate(); busy.remove("engine"); engineDownloadProgress = nil }
            do {
                let engine = try await Task.detached { try downloader.install(entry) }.value
                message = "引擎 \(engine.manifest.id) 已安装。"
                await refresh()
            } catch {
                message = "\(error)（可以再点一次继续下载）"
            }
        }
    }
}
