import CiderData
import CiderCore
import CiderRuntime
import CiderSchema
import CiderStore
import Foundation
import Darwin

public struct Bottle: Sendable {
    public let config: BottleConfig
    public let directory: URL

    public var prefix: URL { directory.appendingPathComponent("prefix", isDirectory: true) }
    public var configURL: URL { directory.appendingPathComponent("cider-bottle.json") }
    /// C:\ on the host.
    public var driveC: URL { prefix.appendingPathComponent("drive_c", isDirectory: true) }
}

public struct BottleStore: Sendable {
    public let paths: CiderPaths
    public let engines: EngineStore
    /// Compatibility data. Held by the store rather than passed per call, so *every* launch that goes
    /// through `runner(for:)` gets the target program's profile — a hot-fix that only reaches one
    /// call site is not a hot-fix (docs/plan/00 principle 1).
    public let compat: CompatDB

    public init(paths: CiderPaths, compat: CompatDB? = nil) {
        self.paths = paths
        self.engines = EngineStore(paths: paths)
        self.compat = compat ?? CompatDB(directories: Self.compatDirectories(paths: paths))
    }

    /// Bundled data (or standalone fallbacks), user data-channel updates, then explicit CIDER_DATA.
    public static func compatDirectories(paths: CiderPaths) -> [URL] {
        CiderResources().compatDirectories(paths: paths)
    }

    public func list() throws -> [Bottle] {
        let fm = FileManager.default
        guard fm.fileExists(atPath: paths.bottles.path) else { return [] }
        return try fm.contentsOfDirectory(at: paths.bottles, includingPropertiesForKeys: nil)
            .filter { !$0.lastPathComponent.hasPrefix(".") }
            .compactMap { dir in
                let url = dir.appendingPathComponent("cider-bottle.json")
                guard fm.fileExists(atPath: url.path) else { return nil }
                _ = try FileSafety.component(dir.lastPathComponent)
                _ = try FileSafety.child(dir.lastPathComponent + "/cider-bottle.json", in: paths.bottles, rejectSymlinks: true)
                let config = try JSONFile.read(BottleConfig.self, from: url)
                guard config.id == dir.lastPathComponent else { throw CiderError.invalid("瓶子标识与目录不一致。") }
                return Bottle(config: config, directory: dir)
            }
            .sorted { $0.config.createdAt < $1.config.createdAt }
    }

    /// Finds a bottle by id, or by exact name.
    public func bottle(_ idOrName: String) throws -> Bottle {
        let all = try list()
        if let hit = all.first(where: { $0.config.id == idOrName }) { return hit }
        let named = all.filter { $0.config.name == idOrName }
        if named.count == 1 { return named[0] }
        if named.count > 1 { throw CiderError.invalid("several bottles are named \(idOrName); use the id") }
        throw CiderError.notFound("bottle \(idOrName)")
    }

    public func runner(for bottle: Bottle) throws -> WineRunner {
        WineRunner(paths: paths, engine: try engines.engine(bottle.config.engine.id), prefix: bottle.prefix,
                   bottleID: bottle.config.id, locale: bottle.config.locale,
                   sync: SyncMode(setting: bottle.config.settings[SyncMode.settingKey]),
                   bottleEnvironment: Self.environment(for: bottle.config.settings), compat: compat)
    }

    /// Bottle switches that are plain environment variables.
    static func environment(for settings: [String: String]) -> [String: String] {
        var env: [String: String] = [:]
        if settings[metalHUDKey] == "1" { env["MTL_HUD_ENABLED"] = "1" }
        // Rosetta reports AVX/AVX2 to x86 programs only when asked (macOS 15+), for games that require AVX.
        if settings[advertiseAVXKey] == "1", ProcessInfo.processInfo.operatingSystemVersion.majorVersion >= 15 {
            env["ROSETTA_ADVERTISE_AVX"] = "1"
        }
        return env
    }

    /// Creates the bottle directory and an initialised 64-bit (new WoW64) prefix.
    /// `progress` receives short human-readable step names.
    @discardableResult
    public func create(name: String, template: BottleTemplate = .win10_64, locale: BottleLocale,
                       engineID: String? = nil, createdBy: String,
                       progress: (String) -> Void = { _ in }) throws -> Bottle {
        try create(name: name, template: template, locale: locale, engineID: engineID, createdBy: createdBy,
                   onReserved: { _ in { try Task.checkCancellation() } }, progress: progress)
    }

    public typealias InitializationCheck = @Sendable () throws -> Void

    /// The reservation is owned before its config becomes visible. A product
    /// coordinator can register cancellation now and retain it through handoff.
    @discardableResult
    public func create(name: String, template: BottleTemplate = .win10_64, locale: BottleLocale,
                       engineID: String? = nil, createdBy: String,
                       onReserved: (Bottle) throws -> InitializationCheck,
                       progress: (String) -> Void = { _ in }) throws -> Bottle {
        let fm = FileManager.default
        let engine = try engineID.map { try engines.engine($0) } ?? engines.defaultEngine()
        let dir = try FileSafety.reserveDirectory(in: paths.bottles) { Identifiers.make(from: name) }
        let id = dir.lastPathComponent
        var identity = stat()
        guard lstat(dir.path, &identity) == 0, identity.st_mode & mode_t(S_IFMT) == mode_t(S_IFDIR) else {
            throw CiderError.invalid("无法确认新瓶子目录的所有权，保留目录：\(dir.path)")
        }
        let device = identity.st_dev, inode = identity.st_ino
        let checkOwnership: InitializationCheck = {
            var current = stat()
            guard lstat(dir.path, &current) == 0, current.st_mode & mode_t(S_IFMT) == mode_t(S_IFDIR),
                  current.st_dev == device, current.st_ino == inode else {
                throw CiderError.invalid("初始化期间瓶子目录已更改，未接管或删除：\(dir.path)")
            }
        }

        var config = BottleConfig(id: id, name: name, createdBy: createdBy, createdAt: Identifiers.timestamp(),
                                  template: template, engine: .init(id: engine.manifest.id), locale: locale)
        config.integration = .init(shellFolders: "isolated", zDrive: true)
        let bottle = Bottle(config: config, directory: dir)
        var initializationStarted = false
        do {
            let callerCheck = try onReserved(bottle)
            let check: InitializationCheck = { try Task.checkCancellation(); try checkOwnership(); try callerCheck() }
            try check()
            try JSONFile.write(config, to: bottle.configURL)
            let runner = WineRunner(paths: paths, engine: engine, prefix: bottle.prefix, bottleID: id, locale: locale).checkingLaunches(check)

            progress("wineboot")
            initializationStarted = true
            let boot = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["--init"], label: "wineboot"))
            try check()
            try runner.waitForIdle()
            try check()
            guard boot.code == 0 else {
                throw CiderError.commandFailed(command: "wineboot --init", status: boot.code,
                                               output: (try? String(contentsOf: boot.session.log, encoding: .utf8)) ?? "")
            }

            progress("shell folders")
            try check()
            try PrefixSetup.isolateShellFolders(in: bottle.driveC)

            progress("fonts")
            try check()
            try PrefixSetup.upgrade(bottle: bottle, runner: runner, from: 0)
            try check()
            config.settings[PrefixSetup.revisionKey] = String(PrefixSetup.revision)
            try JSONFile.write(config, to: bottle.configURL)

            if template != .win10_64 {
                progress("windows version")
                let changed = try runner.runToCompletion(runner.plan(program: "winecfg", arguments: ["/v", template.winver], label: "winecfg"))
                guard changed.code == 0 else { throw CiderError.commandFailed(command: "winecfg /v", status: changed.code, output: "") }
            }
            try check()
            try runner.waitForIdle()
            try check()
            return Bottle(config: config, directory: dir)
        } catch {
            let original = error
            do {
                // The same lock used by launch must span stop AND removal. It
                // cannot require a config: reservation may fail before writing it.
                try FileOperationLock.withLock(at: paths.state.appendingPathComponent("locks/bottles/\(id).lock")) {
                    try checkOwnership()
                    if initializationStarted { try runner(for: bottle).killAll() }
                    try checkOwnership()
                    try fm.removeItem(at: dir)
                }
            } catch {
                throw CiderError.invalid("创建瓶子未完成：\(original)。停止或目录清理未确认：\(error)。保留目录：\(dir.path)")
            }
            throw original
        }
    }

    /// Whether the bottle's prefix predates the current `PrefixSetup.revision`.
    public func needsPrefixUpgrade(_ bottle: Bottle) -> Bool {
        (Int(bottle.config.settings[PrefixSetup.revisionKey] ?? "") ?? 1) < PrefixSetup.revision
    }

    /// Applies the prefix setup steps added since the bottle was created (e.g. dialog fonts) and records the
    /// new revision. Programs already running pick the changes up at their next start.
    @discardableResult
    public func upgradePrefix(_ bottle: Bottle) throws -> Bottle {
        try withOperation(bottle) { current in
            let from = Int(current.config.settings[PrefixSetup.revisionKey] ?? "") ?? 1
            guard from < PrefixSetup.revision else { return current }
            try PrefixSetup.upgrade(bottle: current, runner: try runner(for: current), from: from)
            return try update(current) { $0.settings[PrefixSetup.revisionKey] = String(PrefixSetup.revision) }
        }
    }

    /// A cross-process operation lock outside the bottle survives prefix/directory replacement.
    public func withOperation<T>(_ bottle: Bottle, _ work: (Bottle) throws -> T) throws -> T {
        let id = try FileSafety.component(bottle.config.id)
        let directory = try FileSafety.child(id, in: paths.bottles, rejectSymlinks: true)
        guard directory.standardizedFileURL == bottle.directory.standardizedFileURL else {
            throw CiderError.invalid("瓶子目录与标识不一致。")
        }
        return try FileOperationLock.withLock(at: paths.state.appendingPathComponent("locks/bottles/\(id).lock")) {
            let config = try JSONFile.read(BottleConfig.self, from: directory.appendingPathComponent("cider-bottle.json"))
            guard config.id == id else { throw CiderError.invalid("瓶子标识与目录不一致。") }
            _ = try FileSafety.child("prefix", in: directory, rejectSymlinks: true)
            _ = try FileSafety.child(".cider", in: directory, rejectSymlinks: true)
            return try work(Bottle(config: config, directory: directory))
        }
    }

    /// Reload while locked, then commit only the requested change; stale UI objects cannot overwrite other fields.
    @discardableResult
    public func update(_ bottle: Bottle, _ change: (inout BottleConfig) throws -> Void) throws -> Bottle {
        try withOperation(bottle) { current in
            var config = current.config
            try change(&config)
            guard config.id == current.config.id else { throw CiderError.invalid("a bottle's id cannot change") }
            try JSONFile.write(config, to: current.configURL)
            return Bottle(config: config, directory: current.directory)
        }
    }

    /// Publish only a complete, stopped-prefix snapshot.
    @discardableResult
    public func snapshot(_ bottle: Bottle, reason: String) throws -> String {
        try withOperation(bottle) { current in
            try runner(for: current).killAll()
            let root = try FileSafety.child(".cider/snapshots", in: current.directory, rejectSymlinks: true)
            try FileManager.default.ensureDirectory(root)
            let stamp = Identifiers.compactTimestamp() + "-" + UUID().uuidString.lowercased()
            let staging = root.appendingPathComponent(".creating-" + UUID().uuidString)
            try FileManager.default.ensureDirectory(staging)
            defer { try? FileManager.default.removeItem(at: staging) }
            try Command.run("/bin/cp", ["-c", "-R", current.prefix.path, staging.appendingPathComponent("prefix").path])
            try Command.run("/bin/cp", ["-c", current.configURL.path, staging.appendingPathComponent("cider-bottle.json").path])
            let history = try FileSafety.child(".cider/patches", in: current.directory, rejectSymlinks: true)
            if FileSafety.exists(history) { try Command.run("/bin/cp", ["-c", "-R", history.path, staging.appendingPathComponent("patches").path]) }
            try reason.write(to: staging.appendingPathComponent("reason.txt"), atomically: true, encoding: .utf8)
            try FileManager.default.moveItem(at: staging, to: root.appendingPathComponent(stamp))
            return stamp
        }
    }

    /// Snapshot first. Failure remains visible, with a recoverable prefix/config baseline.
    @discardableResult
    public func switchEngine(_ bottle: Bottle, to engineID: String) throws -> Bottle {
        try withOperation(bottle) { current in
            let engine = try engines.engine(engineID)
            guard engine.manifest.id != current.config.engine.id else { return current }
            let stamp = try snapshot(current, reason: "switch engine \(current.config.engine.id) → \(engineID)")
            let updated = try update(current) { config in
                config.engineHistory.append(.init(from: config.engine.id, to: engineID, at: Identifiers.timestamp(), snapshot: stamp))
                config.engine = .init(id: engineID, pin: config.engine.pin)
            }
            let runner = try runner(for: updated)
            let result = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["-u"], label: "wineboot-update"))
            try runner.killAll()
            try PrefixSetup.isolateShellFolders(in: updated.driveC)
            guard result.code == 0 else { throw CiderError.commandFailed(command: "wineboot -u", status: result.code, output: "see session log") }
            return updated
        }
    }

    /// Moves the bottle to the Trash (recoverable), after stopping its processes.
    public func delete(_ bottle: Bottle) throws {
        try withOperation(bottle) { current in
            try runner(for: current).killAll()
            try FileManager.default.trashItem(at: current.directory, resultingItemURL: nil)
        }
    }
}
