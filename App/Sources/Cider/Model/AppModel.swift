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

/// Pure event-path filtering; normal FSEvents paths need no Foundation URL work.
enum LibraryFileEvents {
    static func isRelevant(_ path: String, bottleRoot: String) -> Bool {
        if path.hasSuffix(".acf") || path.hasSuffix(".lnk") || path.hasSuffix("cider-bottle.json") || path.hasSuffix("libraryfolders.vdf")
            || path.contains("/Data/") || path.hasSuffix("/Data") || path.hasSuffix("/Data/")
            || path.contains("/librarycache/") || path.hasSuffix("/Bottles") || path.hasSuffix("/Bottles/") { return true }
        // NSString performs literal path-marker searches without Swift's
        // grapheme traversal. Only actual dot segments need URL normalization.
        let raw = path as NSString
        if !raw.hasPrefix("/") || raw.range(of: "/./").location != NSNotFound || raw.range(of: "/../").location != NSNotFound
            || raw.hasSuffix("/.") || raw.hasSuffix("/..") || raw.range(of: "//").location != NSNotFound {
            let entry = URL(fileURLWithPath: path).standardizedFileURL
            return entry.path == bottleRoot || entry.deletingLastPathComponent().path == bottleRoot
        }
        var entry = path
        while entry.hasSuffix("/"), entry != "/" { entry.removeLast() }
        return entry == bottleRoot || (entry as NSString).deletingLastPathComponent == bottleRoot
    }
}

@MainActor
@Observable
final class AppModel {
    var section: SidebarSection = .library
    var detail: LibraryItem?
    var items: [LibraryItem] = []
    var bottles: [Bottle] = []
    var environment = EnvironmentStatus()
    @ObservationIgnored private var feedbackGeneration: UInt64 = 0
    var message: String? { didSet { feedbackGeneration &+= 1 } }
    var busy: Set<String> = []
    /// App id → running pids, updated every second from the process scanner.
    var running: [String: [pid_t]] = [:]
    /// Bottles with any live Windows process (including Wine's own services).
    var runningBottles: Set<String> = []
    /// A recipe worker's owner token lets Stop take over its lock without a late defer unlocking another action.
    var recipeWorkers: [String: (owner: UUID, task: Task<(Bottle, RecipeInstaller.Result), Error>, completed: Bool)] = [:]

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
    private var refreshSequence = 0
    private typealias ProcessSnapshot = (map: [String: [pid_t]], bottles: Set<String>, gated: Preflight.Block?)
    private struct ActiveProcessScan {
        let id: UUID
        let startedAt: Double
        let task: Task<ProcessSnapshot, Never>
    }
    @ObservationIgnored private var activeProcessScan: ActiveProcessScan?

    private struct PendingLaunch {
        let title: String
        let bottleID: String
        let steamGame: Bool
        let deadline: Double
        var feedback: UInt64
        let warning: String?
    }
    private var pendingLaunches: [String: PendingLaunch] = [:]

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
        refreshSequence += 1
        let sequence = refreshSequence
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
            let db = store.compat
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
        guard sequence == refreshSequence else { return }
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
    private var failedPrefixUpgrades: Set<String> = []

    private func upgradePrefixes() {
        let paths = self.paths
        let stale = bottles.filter {
            BottleStore(paths: paths).needsPrefixUpgrade($0) && !upgradedBottles.contains($0.config.id)
                && !failedPrefixUpgrades.contains($0.config.id) && !busy.contains("activity:\($0.config.id)")
        }
        for bottle in stale {
            let key = "activity:" + bottle.config.id
            busy.insert(key)
            Task {
                defer { busy.remove(key) }
                do {
                    _ = try await Task.detached(priority: .utility) { try BottleStore(paths: paths).upgradePrefix(bottle) }.value
                    upgradedBottles.insert(bottle.config.id)
                    await refresh()
                } catch {
                    failedPrefixUpgrades.insert(bottle.config.id)
                    message = "瓶子“\(bottle.config.name)”升级失败：\(error)。可在瓶子页重试修复。"
                }
            }
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

    @discardableResult
    func scanProcesses(after minimumStart: Double? = nil) async -> [String: [pid_t]] {
        while true {
            let scan: ActiveProcessScan
            if let activeProcessScan {
                scan = activeProcessScan
            } else {
                let apps = items
                let prefixes = Dictionary(uniqueKeysWithValues: bottles.map { ($0.config.id, $0.prefix) })
                let startedAt = ProcessInfo.processInfo.systemUptime
                let task = Task.detached(priority: .utility) { () -> ProcessSnapshot in
                    let processes = ProcessScanner.scan(prefixes: prefixes)
                    // Preserve the fallback gate for launcher children in this instance's bottles.
                    let blocked = processes.filter { Preflight.check(program: $0.windowsImage, db: nil) != nil }
                    if !blocked.isEmpty { ProcessScanner.terminate(blocked, grace: 0) }
                    var map: [String: [pid_t]] = [:]
                    for app in apps {
                        let pids = processes.filter { app.owns($0) }.map(\.pid)
                        if !pids.isEmpty { map[app.id] = pids }
                    }
                    return (map, Set(processes.map(\.bottleID)), blocked.first.flatMap {
                        Preflight.check(program: $0.windowsImage, db: nil)
                    })
                }
                scan = ActiveProcessScan(id: UUID(), startedAt: startedAt, task: task)
                activeProcessScan = scan
            }
            let snapshot = await scan.task.value
            if activeProcessScan?.id == scan.id {
                activeProcessScan = nil
                if snapshot.map != running { running = snapshot.map }
                if snapshot.bottles != runningBottles { runningBottles = snapshot.bottles }
                reconcilePendingLaunches(with: snapshot.map)
                if let gated = snapshot.gated, routeCard == nil { showRouteCard(gated) }
            }
            // An action needs an observation begun after its dispatch/stop,
            // rather than a coalesced scan from before the action completed.
            if let minimumStart, scan.startedAt < minimumStart { continue }
            return snapshot.map
        }
    }

    private func updateFeedback(_ text: String, owning generation: UInt64) -> UInt64 {
        guard feedbackGeneration == generation else { return generation }
        message = text
        return feedbackGeneration
    }

    /// Process observation acknowledges the real app, not Steam's short-lived
    /// forwarding process. Window visibility is not inferred from a PID.
    private func reconcilePendingLaunches(with processes: [String: [pid_t]]? = nil) {
        let now = ProcessInfo.processInfo.systemUptime
        let catalogIDs = Set(items.map(\.id))
        for id in Array(pendingLaunches.keys).sorted() {
            guard let pending = pendingLaunches[id] else { continue }
            let update: String
            if !catalogIDs.contains(id) {
                update = "\(pending.title) 的资料库条目已变更，请刷新后确认程序状态。"
            } else if let processes, !(processes[id] ?? []).isEmpty {
                update = "\(pending.title) 已开始运行。" + (pending.warning.map { " " + $0 } ?? "")
            } else if now >= pending.deadline {
                update = pending.steamGame
                    ? "尚未检测到 \(pending.title) 运行。请检查 Steam 的提示，或确认游戏是否启动后退出。"
                    : "尚未检测到 \(pending.title) 运行。请检查程序的启动提示。"
            } else {
                continue
            }
            pendingLaunches.removeValue(forKey: id)
            // A delayed observation must not overwrite another operation's
            // feedback or a message the user has already dismissed.
            _ = updateFeedback(update, owning: pending.feedback)
        }
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
            Task { @MainActor in
                // Expiry remains bounded even when a process scan is slow.
                self?.reconcilePendingLaunches()
                await self?.scanProcesses()
            }
        }
    }

    /// FSEvents on the whole Bottles tree; only changes that can alter the library trigger a refresh:
    /// Steam app manifests and artwork, shortcuts, and bottles being added or removed.
    private func startFileEvents() {
        let callback: FSEventStreamCallback = { _, info, count, paths, _, _ in
            guard let info else { return }
            let model = Unmanaged<AppModel>.fromOpaque(info).takeUnretainedValue()
            let list = (unsafeBitCast(paths, to: NSArray.self) as? [String]) ?? []
            let bottleRoot = model.paths.bottles.standardizedFileURL.path
            let relevant = list.prefix(count).contains { LibraryFileEvents.isRelevant($0, bottleRoot: bottleRoot) }
            if relevant { Task { @MainActor in model.scheduleRefresh() } }
        }
        var context = FSEventStreamContext(version: 0, info: Unmanaged.passUnretained(self).toOpaque(),
                                           retain: nil, release: nil, copyDescription: nil)
        guard let stream = FSEventStreamCreate(nil, callback, &context, [paths.bottles.path, paths.appSupport.appendingPathComponent("Data").path] as CFArray,
                                               FSEventStreamEventId(kFSEventStreamEventIdSinceNow), 1.0,
                                               FSEventStreamCreateFlags(kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagUseCFTypes)) else { return }
        FSEventStreamSetDispatchQueue(stream, .main)
        FSEventStreamStart(stream)
        eventStream = stream
    }

    // MARK: Actions

    func launch(_ item: LibraryItem) { perform(item, action: .start) }
    func launch(_ item: LibraryItem, checking check: @escaping @Sendable () throws -> Void) -> Task<Void, Never>? {
        perform(item, action: .start, launchCheck: check)
    }
    func restart(_ item: LibraryItem) { perform(item, action: .restart) }
    func stop(_ item: LibraryItem) { perform(item, action: .stop) }

    private enum AppAction: String, Sendable { case start, stop, restart }

    @discardableResult
    private func perform(_ item: LibraryItem, action: AppAction,
                         launchCheck: (@Sendable () throws -> Void)? = nil) -> Task<Void, Never>? {
        let paths = self.paths
        let snapshot = compat
        // Catalog aliases can refer to the same executable. Serialize by bottle as well as item.
        let bottleKey = "activity:\(item.bottleID)"
        guard !busy.contains(bottleKey) else {
            message = "这个瓶子的操作正在进行，请稍候。"
            return nil
        }
        if action == .start, var pending = pendingLaunches[item.id] {
            let waiting = "\(item.title) 的启动请求仍在处理中，请稍候。"
            message = waiting
            pending.feedback = feedbackGeneration
            pendingLaunches[item.id] = pending
            return nil
        }
        pendingLaunches.removeValue(forKey: item.id)
        busy.insert(bottleKey)
        busy.insert(item.id)
        let actionKey = "\(action.rawValue):\(item.id)"
        busy.insert(actionKey)
        switch action {
        case .start: message = "正在启动 \(item.title)…"
        case .stop: message = "正在停止 \(item.title)…"
        case .restart: message = "正在重启 \(item.title)…"
        }
        let feedback = feedbackGeneration
        return Task {
            defer {
                busy.remove(item.id); busy.remove(actionKey)
                if !busy.contains("stop-bottle:\(item.bottleID)") { busy.remove(bottleKey) }
            }
            do {
                try launchCheck?()
                let started = try await Task.detached { () -> (started: Bool, warning: String?) in
                    let store = BottleStore(paths: paths, compat: snapshot)
                    let bottle = try store.bottle(item.bottleID)
                    var runner = try store.runner(for: bottle)
                    if let launchCheck { runner = runner.checkingLaunches(launchCheck) }
                    let cwd: URL?
                    if action == .stop { cwd = nil }
                    else { cwd = try WindowsPath.workingDirectory(item.workingDirectory, in: bottle) }
                    switch action {
                    case .start:
                        let session = try AppLifecycle.start(item, using: runner, cwd: cwd)
                        return (session != nil, session?.warning)
                    case .restart:
                        let session = try AppLifecycle.restart(item, using: runner, cwd: cwd)
                        return (session != nil, session?.warning)
                    case .stop: try AppLifecycle.stop(item, using: runner); return (false, nil)
                    }
                }.value
                let observed = await scanProcesses(after: ProcessInfo.processInfo.systemUptime)
                try launchCheck?()
                if action == .stop {
                    _ = updateFeedback("已停止 \(item.title)。", owning: feedback)
                } else if started.started {
                    if !(observed[item.id] ?? []).isEmpty {
                        _ = updateFeedback("\(item.title) 已开始运行。" + (started.warning.map { " " + $0 } ?? ""), owning: feedback)
                    } else {
                        let steamGame: Bool = { if case .steamGame = item.kind { return true }; return false }()
                        let waiting = steamGame
                            ? "已发送 \(item.title) 的启动请求，正在等待 Steam 启动游戏。"
                            : "已发送 \(item.title) 的启动请求，正在等待程序响应。"
                        let displayed = waiting + (started.warning.map { " " + $0 } ?? "")
                        let owner = updateFeedback(displayed, owning: feedback)
                        pendingLaunches[item.id] = PendingLaunch(
                            title: item.title, bottleID: item.bottleID, steamGame: steamGame,
                            deadline: ProcessInfo.processInfo.systemUptime + 120,
                            feedback: owner, warning: started.warning)
                    }
                } else {
                    _ = updateFeedback("\(item.title) 已在运行；需要重新启动时，请使用“重启”。", owning: feedback)
                }
            } catch is CancellationError {
                pendingLaunches.removeValue(forKey: item.id)
                await scanProcesses()
                _ = updateFeedback("操作已取消。", owning: feedback)
            } catch let block as Preflight.Block {
                if feedbackGeneration == feedback { showRouteCard(block) }
            } catch {
                await scanProcesses()
                _ = updateFeedback("操作未完成：\(error)", owning: feedback)
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
        pendingLaunches = pendingLaunches.filter { $0.value.bottleID != bottleID }
        message = interrupted == nil ? "正在停止瓶子内的程序…" : "正在取消安装并停止瓶子内的程序…"
        let feedback = feedbackGeneration
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
                    let bottle = try store.bottle(bottleID)
                    try RecipeInstaller.stoppingInstallations(in: bottle) { try store.runner(for: bottle).killAll() }
                }.value
                // Await cancellation so a download/installer cannot start another step after Stop reports success.
                if let interrupted, case .failure(let error) = await interrupted.task.result,
                   !(error is CancellationError) { throw error }
                _ = updateFeedback("瓶子内的程序已停止。", owning: feedback)
            } catch {
                if let interrupted, recipeWorkers[bottleID]?.owner == owner {
                    if recipeWorkers[bottleID]?.completed == true {
                        recipeWorkers.removeValue(forKey: bottleID)
                    } else {
                        recipeWorkers[bottleID] = interrupted
                        releaseActivity = false
                    }
                }
                _ = updateFeedback("停止未完成：\(error)", owning: feedback)
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
        return PatchInstaller(gameRoot: root, historyRoot: bottle.directory.appendingPathComponent(".cider/patches/\(key)", isDirectory: true), stateRoot: paths.state, bottleID: bottle.config.id, prefix: bottle.prefix)
    }

    func lastPatch(for item: LibraryItem) -> PatchInstaller.Manifest? { patchInstaller(for: item)?.lastInstall()?.manifest }

    func installPatch(_ urls: [URL], for item: LibraryItem) {
        let paths = self.paths
        guard !isGated(item) else { message = "这个游戏仍在适配中，不能修改其游戏文件。"; return }
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
                let manifest = try await Task.detached {
                    let store = BottleStore(paths: paths)
                    return try store.withOperation(try store.bottle(item.bottleID)) { _ in
                        guard !ProcessScanner.scan().contains(where: item.owns) else { throw CiderError.invalid("程序仍在运行，请先停止后安装补丁。") }
                        return try installer.install(urls)
                    }
                }.value
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
        let paths = self.paths
        guard !isGated(item) else { message = "这个游戏仍在适配中，不能修改其游戏文件。"; return }
        guard let installer = patchInstaller(for: item) else { return }
        guard !isRunning(item) else { message = "请先停止 \(item.title)，再撤销补丁。"; return }
        let key = "activity:\(item.bottleID)"
        guard !busy.contains(key) else { message = "这个瓶子的操作正在进行，请稍候。"; return }
        busy.insert(key)
        Task {
            defer { busy.remove(key) }
            do {
                if let manifest = try await Task.detached(operation: {
                    let store = BottleStore(paths: paths)
                    return try store.withOperation(try store.bottle(item.bottleID)) { _ in
                        guard !ProcessScanner.scan().contains(where: item.owns) else { throw CiderError.invalid("程序仍在运行，请先停止后撤销补丁。") }
                        return try installer.undoLast()
                    }
                }).value {
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
