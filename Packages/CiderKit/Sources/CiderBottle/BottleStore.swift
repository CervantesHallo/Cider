import CiderData
import CiderCore
import CiderRuntime
import CiderSchema
import CiderStore
import Foundation

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

    public init(paths: CiderPaths) {
        self.paths = paths
        self.engines = EngineStore(paths: paths)
    }

    public func list() throws -> [Bottle] {
        let fm = FileManager.default
        guard fm.fileExists(atPath: paths.bottles.path) else { return [] }
        return try fm.contentsOfDirectory(at: paths.bottles, includingPropertiesForKeys: nil)
            .compactMap { dir in
                let url = dir.appendingPathComponent("cider-bottle.json")
                guard fm.fileExists(atPath: url.path) else { return nil }
                return Bottle(config: try JSONFile.read(BottleConfig.self, from: url), directory: dir)
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

    public func runner(for bottle: Bottle, compat: CompatDB? = nil) throws -> WineRunner {
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
        let fm = FileManager.default
        let engine = try engineID.map { try engines.engine($0) } ?? engines.defaultEngine()
        let id = Identifiers.make(from: name)
        let dir = paths.bottles.appendingPathComponent(id, isDirectory: true)
        try fm.ensureDirectory(dir)

        var config = BottleConfig(id: id, name: name, createdBy: createdBy, createdAt: Identifiers.timestamp(),
                                  template: template, engine: .init(id: engine.manifest.id), locale: locale)
        config.integration = .init(shellFolders: "isolated", zDrive: true)
        let bottle = Bottle(config: config, directory: dir)
        do {
            try JSONFile.write(config, to: bottle.configURL)
            let runner = WineRunner(paths: paths, engine: engine, prefix: bottle.prefix, bottleID: id, locale: locale)

            progress("wineboot")
            let boot = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["--init"], label: "wineboot"))
            try runner.waitForIdle()
            guard boot.code == 0 else {
                throw CiderError.commandFailed(command: "wineboot --init", status: boot.code,
                                               output: (try? String(contentsOf: boot.session.log, encoding: .utf8)) ?? "")
            }

            progress("shell folders")
            try PrefixSetup.isolateShellFolders(in: bottle.driveC)

            progress("fonts")
            try PrefixSetup.upgrade(bottle: bottle, runner: runner, from: 0)
            config.settings[PrefixSetup.revisionKey] = String(PrefixSetup.revision)
            try JSONFile.write(config, to: bottle.configURL)

            if template != .win10_64 {
                progress("windows version")
                _ = try runner.runToCompletion(runner.plan(program: "winecfg", arguments: ["/v", template.winver], label: "winecfg"))
            }
            try runner.waitForIdle()
            return Bottle(config: config, directory: dir)
        } catch {
            try? fm.removeItem(at: dir)
            throw error
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
        let from = Int(bottle.config.settings[PrefixSetup.revisionKey] ?? "") ?? 1
        guard from < PrefixSetup.revision else { return bottle }
        try PrefixSetup.upgrade(bottle: bottle, runner: try runner(for: bottle), from: from)
        return try update(bottle) { $0.settings[PrefixSetup.revisionKey] = String(PrefixSetup.revision) }
    }

    /// Applies `change` to the bottle's config and saves it. Locale changes take effect at the next launch
    /// (the locale is part of each session's environment).
    @discardableResult
    public func update(_ bottle: Bottle, _ change: (inout BottleConfig) throws -> Void) throws -> Bottle {
        var config = bottle.config
        try change(&config)
        guard config.id == bottle.config.id else { throw CiderError.invalid("a bottle's id cannot change") }
        try JSONFile.write(config, to: bottle.configURL)
        return Bottle(config: config, directory: bottle.directory)
    }

    /// Snapshots the prefix and config with APFS clones (`cp -c`: instant, shares blocks until files change)
    /// into `.cider/snapshots/<timestamp>/`. Stops the bottle first so the registry is flushed.
    @discardableResult
    public func snapshot(_ bottle: Bottle, reason: String) throws -> String {
        try? runner(for: bottle).killAll()
        let root = bottle.directory.appendingPathComponent(".cider/snapshots", isDirectory: true)
        var stamp = Identifiers.compactTimestamp()
        // Two snapshots in the same second (e.g. "before restoring" right after another) get a suffix.
        if FileManager.default.fileExists(atPath: root.appendingPathComponent(stamp).path) {
            let base = stamp
            var n = 2
            while FileManager.default.fileExists(atPath: root.appendingPathComponent("\(base)-\(n)").path) { n += 1 }
            stamp = "\(base)-\(n)"
        }
        let dir = root.appendingPathComponent(stamp, isDirectory: true)
        try FileManager.default.ensureDirectory(dir)
        try Command.run("/bin/cp", ["-c", "-R", bottle.prefix.path, dir.appendingPathComponent("prefix").path])
        try Command.run("/bin/cp", ["-c", bottle.configURL.path, dir.appendingPathComponent("cider-bottle.json").path])
        try reason.write(to: dir.appendingPathComponent("reason.txt"), atomically: true, encoding: .utf8)
        return stamp
    }

    /// Switches the bottle to another engine: snapshot → record history → `wineboot -u` with the new engine.
    /// On failure the snapshot stays available for rollback.
    @discardableResult
    public func switchEngine(_ bottle: Bottle, to engineID: String) throws -> Bottle {
        let engine = try engines.engine(engineID)
        guard engine.manifest.id != bottle.config.engine.id else { return bottle }
        let stamp = try snapshot(bottle, reason: "switch engine \(bottle.config.engine.id) → \(engineID)")
        let updated = try update(bottle) { config in
            config.engineHistory.append(BottleConfig.EngineChange(from: config.engine.id, to: engineID, at: Identifiers.timestamp(), snapshot: stamp))
            config.engine = .init(id: engineID, pin: config.engine.pin)
        }
        let runner = try runner(for: updated)
        let result = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["-u"], label: "wineboot-update"))
        // wineboot also starts the prefix's Run-key programs (e.g. `steam.exe -silent`), so waiting for the
        // prefix to go idle could block forever; stop everything instead.
        try runner.killAll()
        // Some engines (CrossOver-derived) use their own Windows user name; isolate that profile too.
        try PrefixSetup.isolateShellFolders(in: updated.driveC)
        guard result.code == 0 else {
            throw CiderError.commandFailed(command: "wineboot -u", status: result.code,
                                           output: (try? String(contentsOf: result.session.log, encoding: .utf8)) ?? "")
        }
        return updated
    }

    /// Moves the bottle to the Trash (recoverable), after stopping its processes.
    public func delete(_ bottle: Bottle) throws {
        if let runner = try? runner(for: bottle) { try? runner.killAll() }
        try FileManager.default.trashItem(at: bottle.directory, resultingItemURL: nil)
    }
}
