import AppKit
import CiderBottle
import CiderCore
import CiderData
import CiderIntegration
import CiderPE
import CiderRuntime
import CiderSchema
import CiderStore
import CoreServices
import CryptoKit
import Foundation
import Observation

typealias LibraryItem = CatalogApp

extension CatalogApp {
    var source: String {
        switch kind {
        case .steamGame: return "Steam 游戏"
        case .steamClient: return "客户端 · \(bottleName)"
        case .program: return "应用 · \(bottleName)"
        }
    }

    var installed: Bool { installState == .installed }

    var downloadProgress: Double? {
        if case .downloading(let p) = installState { return p ?? 0 }
        return nil
    }

    /// Steam's App ID for Steam games.
    var steamAppID: String? { if case .steamGame(let id, _) = kind { return id }; return nil }
}

enum SidebarSection: String, CaseIterable, Identifiable, Sendable {
    case library, install, bottles, environment
    var id: String { rawValue }

    var title: String {
        switch self {
        case .library: return "资料库"
        case .install: return "安装软件"
        case .bottles: return "瓶子"
        case .environment: return "运行环境"
        }
    }

    var symbol: String {
        switch self {
        case .library: return "square.grid.2x2"
        case .install: return "square.and.arrow.down"
        case .bottles: return "flask"
        case .environment: return "cpu"
        }
    }
}

struct EnvironmentStatus: Sendable {
    var rosetta = false
    var engines: [String] = []
    var gstreamer = false
    var gstreamerBundled = false
    var ready: Bool { rosetta && !engines.isEmpty }
}

@MainActor
@Observable
final class AppModel {
    var section: SidebarSection = .library
    var detail: LibraryItem?
    var items: [LibraryItem] = []
    var bottles: [Bottle] = []
    var environment = EnvironmentStatus()
    var message: String?
    var busy: Set<String> = []
    /// App id → running pids, updated every second from the process scanner.
    var running: [String: [pid_t]] = [:]
    /// Bottles with any live Windows process (including Wine's own services).
    var runningBottles: Set<String> = []
    /// A recipe worker's owner token lets Stop take over its lock without a late defer unlocking another action.
    var recipeWorkers: [String: (owner: UUID, task: Task<Bottle, Error>, completed: Bool)] = [:]

    let paths = CiderPaths.standard()
    /// Compatibility data (bundled + user data directory) and the verdict for each library item.
    var compat = CompatDB()
    var compatDecisions: [String: VerdictDecision] = [:]
    /// First-run guide: shown once, or whenever Cider cannot run anything yet.
    var showWelcome = false
    /// 0…1 while an engine download runs.
    var engineDownloadProgress: Double?
    private var welcomeOffered = false
    /// Set when a launch was refused by the R3 preflight; shows the route card.
    var routeCard: Preflight.Block?
    /// Session log of the last "运行命令", for "显示日志".
    var lastCommandLog: URL?
    private var eventStream: FSEventStreamRef?
    private var scanTimer: Timer?
    private var activationObserver: NSObjectProtocol?
    private var refreshPending = false
    private var lastRefresh = Date.distantPast

    var games: [LibraryItem] { items.filter(\.isGame) }
    var continueItem: LibraryItem? {
        games.filter(\.installed).max { ($0.lastPlayed ?? .distantPast) < ($1.lastPlayed ?? .distantPast) }
    }

    func isRunning(_ item: LibraryItem) -> Bool { !(running[item.id] ?? []).isEmpty }

    /// Shows the route card for a refused launch, with the routes from the compatibility data.
    func showRouteCard(_ block: Preflight.Block) {
        routeCard = Preflight.check(program: block.image, db: compat) ?? block
    }

    func isGated(_ item: LibraryItem) -> Bool {
        if case .program(let target) = item.kind { return Preflight.check(program: target, db: nil) != nil }
        return false
    }

    /// The badge for an item: the compatibility database's verdict; the Steam client is verified by Cider itself.
    func verdict(for item: LibraryItem) -> Verdict {
        if case .steamClient = item.kind { return .good }
        if isGated(item) { return .unsupported }
        switch compatDecisions[item.id]?.result {
        case .playable: return .good
        case .playableCaveats: return .ok
        case .brokenLauncher: return .tune
        case .broken, .blockedAnticheat, .blockedDRM: return .unsupported
        case .unverified, nil: return .unknown
        }
    }

    // MARK: Refresh

    private var upgradedBottles: Set<String> = []

    func refresh() async {
        lastRefresh = Date()
        let paths = self.paths
        let result = await Task.detached(priority: .userInitiated) { () -> ([Bottle], [CatalogApp], EnvironmentStatus, CompatDB, [String: VerdictDecision]) in
            let store = BottleStore(paths: paths)
            let bottles = (try? store.list()) ?? []
            let catalog = AppCatalog(iconCache: IconCache(directory: paths.caches.appendingPathComponent("icons")), compat: store.compat)
            let apps = bottles.flatMap { catalog.apps(in: $0) }
            var env = EnvironmentStatus()
            env.rosetta = (try? Command.run("/usr/bin/arch", ["-x86_64", "/usr/bin/true"])) != nil
            let engines = (try? EngineStore(paths: paths).list()) ?? []
            env.engines = engines.map(\.manifest.id)
            env.gstreamerBundled = engines.contains { $0.bundledGStreamer != nil }
            env.gstreamer = env.gstreamerBundled || FileManager.default.fileExists(atPath: WineRunner.gstreamerFramework)
            let db = CompatDB(directories: [Bundle.main.resourceURL?.appendingPathComponent("data"),
                                            paths.appSupport.appendingPathComponent("Data")].compactMap { $0 })
            var decisions: [String: VerdictDecision] = [:]
            let macos = String(ProcessInfo.processInfo.operatingSystemVersion.majorVersion)
            for app in apps {
                guard let appID = app.steamAppID, let game = db.game(steamAppID: appID),
                      let bottle = bottles.first(where: { $0.config.id == app.bottleID }),
                      let engine = engines.first(where: { $0.manifest.id == bottle.config.engine.id }) else { continue }
                let major = engine.manifest.wine.version.split { !$0.isNumber }.first.map(String.init) ?? "*"
                decisions[app.id] = db.verdict(for: VerdictKey(gameID: game.id, channel: "steam", engineMajor: major,
                                                               macosMajor: macos, cpuBackend: engine.manifest.cpuBackend))
            }
            return (bottles, apps, env, db, decisions)
        }.value
        bottles = result.0
        items = result.1
        environment = result.2
        compat = result.3
        compatDecisions = result.4
        // Once per launch at most; people who already have a working setup are not asked again.
        if environment.ready && !bottles.isEmpty { UserDefaults.standard.set(true, forKey: WelcomeView.doneKey) }
        if !welcomeOffered, !UserDefaults.standard.bool(forKey: WelcomeView.doneKey) || !environment.ready {
            welcomeOffered = true
            showWelcome = true
        }
        if let detail { self.detail = items.first { $0.id == detail.id } }
        await scanProcesses()
        upgradePrefixes()
    }

    /// Brings bottles created by older Cider versions up to the current prefix setup, once per launch, in the
    /// background (a registry import per bottle; running programs see it at their next start).
    private func upgradePrefixes() {
        let paths = self.paths
        let stale = bottles.filter { BottleStore(paths: paths).needsPrefixUpgrade($0) && !upgradedBottles.contains($0.config.id) }
        guard !stale.isEmpty else { return }
        stale.forEach { upgradedBottles.insert($0.config.id) }
        Task.detached(priority: .utility) {
            for bottle in stale { _ = try? BottleStore(paths: paths).upgradePrefix(bottle) }
        }
    }

    /// Coalesces bursts of filesystem events (a Steam download touches many files) into one refresh every ≥2 s.
    func scheduleRefresh() {
        guard !refreshPending else { return }
        refreshPending = true
        let delay = max(0.5, 2 - Date().timeIntervalSince(lastRefresh))
        Task {
            try? await Task.sleep(for: .seconds(delay))
            refreshPending = false
            await refresh()
        }
    }

    func scanProcesses() async {
        let apps = items
        let (map, bottlesUp, gated) = await Task.detached(priority: .utility) { () -> ([String: [pid_t]], Set<String>, Preflight.Block?) in
            let processes = ProcessScanner.scan()
            // A launcher inside the bottle can start a gated game itself; stop it before its anti-cheat comes up.
            // (Cider's own engine refuses these at CreateProcess; this covers engines without that patch.)
            let blocked = processes.filter { Preflight.check(program: $0.windowsImage, db: nil) != nil }
            if !blocked.isEmpty { ProcessScanner.terminate(blocked, grace: 0) }
            var map: [String: [pid_t]] = [:]
            for app in apps {
                let pids = processes.filter { app.owns($0) }.map(\.pid)
                if !pids.isEmpty { map[app.id] = pids }
            }
            return (map, Set(processes.map(\.bottleID)), blocked.first.flatMap { Preflight.check(program: $0.windowsImage, db: nil) })
        }.value
        if map != running { running = map }
        if bottlesUp != runningBottles { runningBottles = bottlesUp }
        if let gated, routeCard == nil { showRouteCard(gated) }
    }

    // MARK: Monitoring

    func startMonitoring() {
        guard scanTimer == nil else { return }
        try? FileManager.default.ensureDirectory(paths.bottles)
        startFileEvents()
        activationObserver = NotificationCenter.default.addObserver(
            forName: NSApplication.didBecomeActiveNotification, object: nil, queue: .main
        ) { [weak self] _ in Task { @MainActor in self?.scheduleRefresh() } }
        scanTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in await self?.scanProcesses() }
        }
    }

    /// FSEvents on the whole Bottles tree; only changes that can alter the library trigger a refresh:
    /// Steam app manifests and artwork, shortcuts, and bottles being added or removed.
    private func startFileEvents() {
        let callback: FSEventStreamCallback = { _, info, count, paths, _, _ in
            guard let info else { return }
            let model = Unmanaged<AppModel>.fromOpaque(info).takeUnretainedValue()
            let list = (unsafeBitCast(paths, to: NSArray.self) as? [String]) ?? []
            let relevant = list.prefix(count).contains { p in
                p.hasSuffix(".acf") || p.hasSuffix(".lnk") || p.hasSuffix("cider-bottle.json") || p.hasSuffix("libraryfolders.vdf")
                    || p.contains("/librarycache/") || p.hasSuffix("/Bottles") || p.hasSuffix("/Bottles/")
            }
            if relevant { Task { @MainActor in model.scheduleRefresh() } }
        }
        var context = FSEventStreamContext(version: 0, info: Unmanaged.passUnretained(self).toOpaque(),
                                           retain: nil, release: nil, copyDescription: nil)
        guard let stream = FSEventStreamCreate(nil, callback, &context, [paths.bottles.path] as CFArray,
                                               FSEventStreamEventId(kFSEventStreamEventIdSinceNow), 1.0,
                                               FSEventStreamCreateFlags(kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagUseCFTypes)) else { return }
        FSEventStreamSetDispatchQueue(stream, .main)
        FSEventStreamStart(stream)
        eventStream = stream
    }

    // MARK: Actions

    func launch(_ item: LibraryItem) { perform(item, action: .start) }
    func restart(_ item: LibraryItem) { perform(item, action: .restart) }
    func stop(_ item: LibraryItem) { perform(item, action: .stop) }

    private enum AppAction: String, Sendable { case start, stop, restart }

    private func perform(_ item: LibraryItem, action: AppAction) {
        let paths = self.paths
        // Catalog aliases can refer to the same executable. Serialize by bottle as well as item.
        let bottleKey = "activity:\(item.bottleID)"
        guard !busy.contains(bottleKey) else {
            message = "这个瓶子的操作正在进行，请稍候。"
            return
        }
        busy.insert(bottleKey)
        busy.insert(item.id)
        let actionKey = "\(action.rawValue):\(item.id)"
        busy.insert(actionKey)
        switch action {
        case .start: message = "正在启动 \(item.title)…"
        case .stop: message = "正在停止 \(item.title)…"
        case .restart: message = "正在重启 \(item.title)…"
        }
        Task {
            defer { busy.remove(item.id); busy.remove(actionKey); busy.remove(bottleKey) }
            do {
                let started = try await Task.detached { () -> Bool in
                    let store = BottleStore(paths: paths)
                    let bottle = try store.bottle(item.bottleID)
                    let runner = try store.runner(for: bottle)
                    let cwd = item.workingDirectory.flatMap { try? WindowsPath.hostURL(for: $0, in: bottle) }
                    switch action {
                    case .start: return try AppLifecycle.start(item, using: runner, cwd: cwd) != nil
                    case .restart: return try AppLifecycle.restart(item, using: runner, cwd: cwd) != nil
                    case .stop: try AppLifecycle.stop(item, using: runner); return false
                    }
                }.value
                await scanProcesses()
                if action == .stop {
                    message = "已停止 \(item.title)。"
                } else if started {
                    message = "已发起\(action == .restart ? "重启" : "启动") \(item.title)，等待窗口就绪。"
                } else {
                    message = "\(item.title) 已在运行；需要重新启动时，请使用“重启”。"
                }
            } catch let block as Preflight.Block {
                showRouteCard(block)
            } catch {
                await scanProcesses()
                message = "操作未完成：\(error)"
            }
        }
    }

    func runInstaller(_ url: URL, in bottleID: String) {
        let paths = self.paths
        let key = "activity:\(bottleID)"
        guard !busy.contains(key), !busy.contains("install") else { message = "这个瓶子的操作正在进行，请稍候。"; return }
        busy.insert(key)
        busy.insert("install")
        message = "正在运行 \(url.lastPathComponent)…"
        Task {
            var holdsActivity = true
            defer { busy.remove("install"); if holdsActivity { busy.remove(key) } }
            do {
                let session = try await Task.detached { () -> SessionHandle in
                    let store = BottleStore(paths: paths)
                    let runner = try store.runner(for: try store.bottle(bottleID))
                    let isMSI = url.pathExtension.lowercased() == "msi"
                    let plan = isMSI
                        ? runner.plan(program: "msiexec", arguments: ["/i", url.path], label: url.deletingPathExtension().lastPathComponent,
                                      cwd: url.deletingLastPathComponent())
                        : runner.plan(program: url.path, label: url.deletingPathExtension().lastPathComponent,
                                      cwd: url.deletingLastPathComponent())
                    return try runner.launch(plan)
                }.value
                // Installation is now a running program. Keep the install indicator, but allow Stop.
                busy.remove(key)
                holdsActivity = false
                let code = await Task.detached { Spawn.wait(session.pid) }.value
                message = code == 0 ? "安装程序已结束。" : "安装程序退出，代码 \(code)。"
                await refresh()
            } catch {
                message = "安装失败：\(error)"
            }
        }
    }

    func createBottle(name: String, locale: BottleLocale, template: BottleTemplate = .win10_64) {
        let paths = self.paths
        busy.insert("create-bottle")
        message = "正在创建瓶子“\(name)”…"
        Task {
            defer { busy.remove("create-bottle") }
            do {
                try await Task.detached {
                    _ = try BottleStore(paths: paths).create(name: name, template: template, locale: locale, createdBy: "Cider.app")
                }.value
                message = "已创建瓶子“\(name)”。"
                await refresh()
            } catch {
                message = "创建失败：\(error)"
            }
        }
    }

    func stopBottle(_ bottleID: String) {
        let paths = self.paths
        let key = "activity:\(bottleID)"
        let stopKey = "stop-bottle:\(bottleID)"
        guard !busy.contains(stopKey) else { return }
        let owner = UUID()
        let interrupted = recipeWorkers[bottleID]
        if busy.contains(key) {
            guard let interrupted else {
                message = "这个瓶子的操作正在进行，请稍候。"
                return
            }
            interrupted.task.cancel()
            recipeWorkers[bottleID] = (owner, interrupted.task, interrupted.completed)
        } else {
            busy.insert(key)
        }
        busy.insert(stopKey)
        message = interrupted == nil ? "正在停止瓶子内的程序…" : "正在取消安装并停止瓶子内的程序…"
        Task {
            var releaseActivity = true
            defer {
                busy.remove(stopKey)
                if releaseActivity {
                    if interrupted != nil, recipeWorkers[bottleID]?.owner == owner { recipeWorkers.removeValue(forKey: bottleID) }
                    busy.remove(key)
                }
            }
            do {
                try await Task.detached {
                    let store = BottleStore(paths: paths)
                    try store.runner(for: try store.bottle(bottleID)).killAll()
                }.value
                // Await cancellation so a download/installer cannot start another step after Stop reports success.
                if let interrupted, case .failure(let error) = await interrupted.task.result,
                   !(error is CancellationError) { throw error }
                message = "瓶子内的程序已停止。"
            } catch {
                if let interrupted, recipeWorkers[bottleID]?.owner == owner {
                    if recipeWorkers[bottleID]?.completed == true {
                        recipeWorkers.removeValue(forKey: bottleID)
                    } else {
                        recipeWorkers[bottleID] = interrupted
                        releaseActivity = false
                    }
                }
                message = "停止未完成：\(error)"
            }
            await scanProcesses()
        }
    }

    // MARK: Patches

    /// Patch history lives in the bottle (`.cider/patches/<app>/`) so it travels with bottle exports/snapshots.
    func patchInstaller(for item: LibraryItem) -> PatchInstaller? {
        guard let root = item.installDirectory, let bottle = bottles.first(where: { $0.config.id == item.bottleID }) else { return nil }
        let key: String
        switch item.kind {
        case .steamGame(let appID, _): key = "steam-\(appID)"
        case .steamClient: key = "steam-client"
        case .program(let target):
            // Stable across launches (String.hashValue is randomly seeded per process).
            let digest = SHA256.hash(data: Data(target.lowercased().utf8))
            key = "program-" + digest.prefix(8).map { String(format: "%02x", $0) }.joined()
        }
        return PatchInstaller(gameRoot: root, historyRoot: bottle.directory.appendingPathComponent(".cider/patches/\(key)", isDirectory: true))
    }

    func lastPatch(for item: LibraryItem) -> PatchInstaller.Manifest? { patchInstaller(for: item)?.lastInstall()?.manifest }

    func installPatch(_ urls: [URL], for item: LibraryItem) {
        guard let installer = patchInstaller(for: item) else { message = "找不到 \(item.title) 的游戏目录。"; return }
        guard !isRunning(item) else { message = "请先停止 \(item.title)，再安装补丁（运行中的文件无法替换）。"; return }
        // Some patches ship as an installer instead of loose files: run it inside the bottle, starting in the game folder.
        let installers = urls.filter { ["exe", "msi"].contains($0.pathExtension.lowercased()) }
        if !installers.isEmpty {
            guard urls.count == 1 else { message = "补丁安装程序（.exe）请单独拖入，不要和其他文件混在一起。"; return }
            runPatchInstaller(urls[0], gameRoot: installer.gameRoot, for: item)
            return
        }
        let key = "activity:\(item.bottleID)"
        guard !busy.contains(key) else { message = "这个瓶子的操作正在进行，请稍候。"; return }
        busy.insert(key)
        busy.insert("patch:\(item.id)")
        message = "正在把 \(urls.count) 个项目复制到 \(item.title) 的游戏目录…"
        Task {
            defer { busy.remove("patch:\(item.id)"); busy.remove(key) }
            do {
                let manifest = try await Task.detached { try installer.install(urls) }.value
                message = "补丁已安装：新增 \(manifest.added.count) 个文件，替换 \(manifest.replaced.count) 个（原文件已备份，可撤销）。"
            } catch {
                message = "补丁安装失败：\(error)"
            }
        }
    }

    private func runPatchInstaller(_ url: URL, gameRoot: URL, for item: LibraryItem) {
        let paths = self.paths
        let key = "activity:\(item.bottleID)"
        guard !busy.contains(key) else { message = "这个瓶子的操作正在进行，请稍候。"; return }
        busy.insert(key)
        busy.insert("patch:\(item.id)")
        message = "正在瓶子「\(item.bottleName)」里运行补丁安装程序 \(url.lastPathComponent)，按它的提示把目标目录选为游戏目录…"
        Task {
            var holdsActivity = true
            defer { busy.remove("patch:\(item.id)"); if holdsActivity { busy.remove(key) } }
            do {
                let session = try await Task.detached { () -> SessionHandle in
                    let store = BottleStore(paths: paths)
                    let runner = try store.runner(for: try store.bottle(item.bottleID))
                    let isMSI = url.pathExtension.lowercased() == "msi"
                    let label = "patch-" + url.deletingPathExtension().lastPathComponent
                    let plan = isMSI
                        ? runner.plan(program: "msiexec", arguments: ["/i", url.path], label: label, cwd: gameRoot)
                        : runner.plan(program: url.path, label: label, cwd: gameRoot)
                    return try runner.launch(plan)
                }.value
                // Installation is now a running program. Keep the install indicator, but allow Stop.
                busy.remove(key)
                holdsActivity = false
                let code = await Task.detached { Spawn.wait(session.pid) }.value
                message = code == 0 ? "补丁安装程序已结束（安装程序改动的文件由它自己管理，Cider 无法撤销）。" : "补丁安装程序退出，代码 \(code)。"
            } catch {
                message = "补丁安装程序运行失败：\(error)"
            }
        }
    }

    func undoPatch(for item: LibraryItem) {
        guard let installer = patchInstaller(for: item) else { return }
        guard !isRunning(item) else { message = "请先停止 \(item.title)，再撤销补丁。"; return }
        let key = "activity:\(item.bottleID)"
        guard !busy.contains(key) else { message = "这个瓶子的操作正在进行，请稍候。"; return }
        busy.insert(key)
        Task {
            defer { busy.remove(key) }
            do {
                if let manifest = try await Task.detached(operation: { try installer.undoLast() }).value {
                    message = "已撤销补丁（\(manifest.sources.joined(separator: "、"))），原文件已恢复。"
                }
            } catch {
                message = "撤销失败：\(error)"
            }
        }
    }

    func revealInFinder(_ url: URL) {
        NSWorkspace.shared.activateFileViewerSelecting([url])
    }
}
