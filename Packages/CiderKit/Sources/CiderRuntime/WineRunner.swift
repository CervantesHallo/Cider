import CiderCore
import CiderData
import CiderSchema
import CiderStore
import Darwin
import Foundation

/// Everything needed to start one Wine session (docs/plan/01 §2, `LaunchPlan`).
public struct LaunchPlan: Codable, Sendable {
    public var bottleID: String
    public var engineID: String
    public var loader: String
    public var argv: [String]
    public var env: [String: String]
    public var cwd: String?
    public var label: String
}

public struct SessionHandle: Sendable {
    public let pid: pid_t
    public let directory: URL
    public var log: URL { directory.appendingPathComponent("wine.log") }
    public var warning: String? = nil
}

/// WINEDEBUG presets (docs/plan/01 §8).
public enum DebugPreset: String, CaseIterable, Sendable {
    case quiet, `default`, install, verbose

    public var value: String {
        switch self {
        case .quiet: return "-all"
        case .default: return "fixme-all"
        case .install: return "+seh,+tid,+loaddll"
        case .verbose: return "warn+all"
        }
    }
}

/// How Windows synchronization objects are implemented. The choice is bottle-wide: wineserver fixes it when it starts,
/// and every process of the prefix must agree, so it is never set per launch.
public enum SyncMode: String, CaseIterable, Sendable {
    /// Mach-semaphore fast path (CrossOver/Highball engines); engines without it ignore the variable.
    case msync
    /// Every wait goes through wineserver (slowest, most conservative).
    case server

    /// Bottle setting key in `cider-bottle.json` → `settings`.
    public static let settingKey = "sync"
    public static let `default`: SyncMode = .msync

    public init(setting: String?) { self = setting.flatMap(SyncMode.init(rawValue:)) ?? .default }

    public var environment: [String: String] {
        ["WINEMSYNC": self == .msync ? "1" : "0", "WINEESYNC": "0"]
    }
}

/// Builds environments and spawns Wine for a bottle. The environment is constructed, never inherited wholesale,
/// so stray DYLD_* or locale variables from the caller's shell cannot leak in.
public struct WineRunner: Sendable {
    public let paths: CiderPaths
    public let engine: InstalledEngine
    public let prefix: URL
    public let bottleID: String
    public let locale: BottleLocale
    public let sync: SyncMode
    /// Bottle-level switches that are plain environment variables (e.g. the Metal HUD).
    public let bottleEnvironment: [String: String]
    /// Compatibility data, so a profile's `actions.env` reaches the launch it was written for
    /// (docs/plan/00 principle 1: what changes lives in data). Profiles are red-line linted on load.
    public let compat: CompatDB?

    public init(paths: CiderPaths, engine: InstalledEngine, prefix: URL, bottleID: String, locale: BottleLocale,
                sync: SyncMode = .default, bottleEnvironment: [String: String] = [:], compat: CompatDB? = nil) {
        self.paths = paths
        self.engine = engine
        self.prefix = prefix
        self.bottleID = bottleID
        self.locale = locale
        self.sync = sync
        self.bottleEnvironment = bottleEnvironment
        self.compat = compat
    }

    public static let gstreamerFramework = "/Library/Frameworks/GStreamer.framework"

    public func environment(debug: DebugPreset = .default, extra: [String: String] = [:]) -> [String: String] {
        let host = ProcessInfo.processInfo.environment
        var env: [String: String] = [
            "HOME": host["HOME"] ?? NSHomeDirectory(),
            "USER": host["USER"] ?? NSUserName(),
            "LOGNAME": host["LOGNAME"] ?? NSUserName(),
            "TMPDIR": host["TMPDIR"] ?? NSTemporaryDirectory(),
            "PATH": "\(engine.bin.path):/usr/bin:/bin:/usr/sbin:/sbin",
            "WINEPREFIX": prefix.path,
            // Child processes (CreateProcess) are started through WINELOADER; pointing it at the host bundle's loader
            // gives every Windows process of the session the same app identity as the first one.
            "WINELOADER": engine.wine.path,
            "WINEDEBUG": debug.value,
            // Keep Wine from creating .desktop/menu entries or associations on the host; Cider makes launchers itself.
            "WINEDLLOVERRIDES": "winemenubuilder.exe=d",
            "CIDER_BOTTLE": bottleID,
        ]
        // Engines that bundle GStreamer (e.g. Whisky-style runtimes) must not be pointed at the system framework:
        // mixing versions corrupts the plugin registry. Each engine gets its own registry cache either way.
        env["GST_REGISTRY"] = paths.caches.appendingPathComponent("gstreamer/\(engine.manifest.id)/registry.bin").path
        if let gst = engine.bundledGStreamer {
            env["GST_PLUGIN_SYSTEM_PATH"] = gst.appendingPathComponent("lib/gstreamer-1.0").path
            env["GST_PLUGIN_SCANNER"] = gst.appendingPathComponent("libexec/gstreamer-1.0/gst-plugin-scanner").path
        } else if !engine.bundlesGStreamer, FileManager.default.fileExists(atPath: Self.gstreamerFramework) {
            let gst = "\(Self.gstreamerFramework)/Versions/Current"
            env["GST_PLUGIN_SYSTEM_PATH"] = "\(gst)/lib/gstreamer-1.0"
            env["GST_PLUGIN_SCANNER"] = "\(gst)/libexec/gstreamer-1.0/gst-plugin-scanner"
        }
        if let libs = engine.manifest.libraryPaths, !libs.isEmpty {
            // Engine-declared library directories only; the caller's DYLD_* variables are never passed through.
            env["DYLD_FALLBACK_LIBRARY_PATH"] = (libs.map { engine.directory.appendingPathComponent($0).path } + ["/usr/lib"])
                .joined(separator: ":")
        }
        for (key, value) in bottleEnvironment { env[key] = value }
        for (key, value) in extra {
            let upper = key.uppercased()
            if ["DYLD_", "LD_PRELOAD", "WINELOADER", "WINESERVER", "WINEDLLPATH", "CIDER_"].contains(where: { upper.hasPrefix($0) }) { continue }
            env[key] = value
        }
        env["WINEPREFIX"] = prefix.path
        env["WINELOADER"] = engine.wine.path
        env["CIDER_BOTTLE"] = bottleID
        // Bottle-wide settings come last so a per-launch extra can never split the prefix.
        for (key, value) in sync.environment { env[key] = value }
        // R3 preflight inside the engine: Cider's engines refuse these images at NtCreateUserProcess
        // (engine/patches/cider/0001), so a launcher in the bottle cannot start them either.
        var denied = Set(Preflight.gatedExecutables.keys)
        if cloneBlocksSteam { denied.insert("steam.exe") }
        env["CIDER_PREFLIGHT_DENY"] = denied.sorted().joined(separator: ";")
        for (key, value) in locale.environment { env[key] = value }
        return env
    }

    public func plan(program: String, arguments: [String] = [], label: String? = nil, cwd: URL? = nil,
                     debug: DebugPreset = .default, extraEnv: [String: String] = [:]) -> LaunchPlan {
        // A profile hot-fixes the program (e.g. a CEF launcher that needs DXMT's cross-process
        // swapchain opt-in); an explicit per-launch value still wins over it.
        let profileProgram = program.hasPrefix("/")
            ? WinePathMapping.windowsPath(forHostPath: program, prefix: prefix) : program
        let profileEnv = compat?.profile(exe: profileProgram)?.actions.env ?? [:]
        return LaunchPlan(
            bottleID: bottleID,
            engineID: engine.manifest.id,
            loader: engine.wine.path,
            argv: [program] + arguments,
            env: environment(debug: debug, extra: profileEnv.merging(extraEnv) { _, explicit in explicit }),
            cwd: cwd?.path,
            label: label ?? URL(fileURLWithPath: program.replacingOccurrences(of: "\\", with: "/")).deletingPathExtension().lastPathComponent
        )
    }

    /// Spawns the plan, recording `session.json` and an audit line. Returns immediately.
    public func launch(_ plan: LaunchPlan) throws -> SessionHandle {
        try withBottleOperation {
            try FileOperationLock.withLock(at: EngineStore(paths: paths).operationLock(for: engine.manifest.id)) {
                try launchLocked(plan)
            }
        }
    }

    public func withBottleOperation<T>(_ work: () throws -> T) throws -> T {
        _ = try FileSafety.component(bottleID)
        return try FileOperationLock.withLock(at: paths.state.appendingPathComponent("locks/bottles/\(bottleID).lock"), work)
    }

    private var cloneBlocksSteam: Bool {
        let url = paths.bottles.appendingPathComponent(bottleID + "/cider-bottle.json")
        guard let config = try? JSONFile.read(BottleConfig.self, from: url) else { return true }
        return config.settings["cloneSteamBlocked"] == "1"
    }

    private func launchLocked(_ plan: LaunchPlan) throws -> SessionHandle {
        // R3 preflight: gated games never start (any argv element counts, so `cmd /c start x.exe` is caught too).
        if let block = plan.argv.lazy.compactMap({ Preflight.check(program: $0, db: nil) }).first {
            throw block
        }
        let configURL = paths.bottles.appendingPathComponent(bottleID + "/cider-bottle.json")
        let current = try JSONFile.read(BottleConfig.self, from: configURL)
        let expectedPrefix = try FileSafety.child(bottleID + "/prefix", in: paths.bottles, rejectSymlinks: true)
        guard current.id == bottleID, expectedPrefix.standardizedFileURL == prefix.standardizedFileURL,
              current.engine.id == engine.manifest.id, current.locale == locale,
              SyncMode(setting: current.settings[SyncMode.settingKey]) == sync else {
            throw CiderError.invalid("瓶子设置已改变，请重新发起启动。")
        }
        if current.settings["cloneSteamBlocked"] == "1" && plan.argv.contains(where: { Preflight.imageName(of: $0) == "steam.exe" }) {
            throw CiderError.invalid("此瓶子是 Steam 前缀的副本，为保护原登录，不能在副本中启动 Steam。请新建瓶子并重新安装登录。")
        }
        try EnginePolicy.requireChildPreflight(engine)
        try PatchState.requireRecovered(in: expectedPrefix.deletingLastPathComponent())
        // Upgrade host metadata before starting any Wine child, including existing engines.
        // Do this only on launch: stopping a process must not depend on writable engine metadata.
        try EngineHost.ensure(engineDirectory: engine.directory, wineRoot: engine.wineRoot,
                              cpuBackend: engine.manifest.cpuBackend)
        var plan = plan
        // The plan is public data; preserve the engine/prefix authority even when it was supplied directly.
        guard plan.loader == engine.bin.appendingPathComponent("wine").path || plan.loader == engine.wine.path,
              plan.bottleID == bottleID, plan.engineID == engine.manifest.id else { throw CiderError.invalid("启动计划与瓶子引擎不一致。") }
        plan.env = environment(extra: plan.env)
        if plan.loader == engine.bin.appendingPathComponent("wine").path {
            plan.loader = engine.wine.path
            plan.env["WINELOADER"] = engine.wine.path
        }
        let fm = FileManager.default
        let safeLabel = plan.label.replacingOccurrences(of: "/", with: "_")
        let sessionDir = paths.sessions
            .appendingPathComponent(plan.bottleID, isDirectory: true)
            .appendingPathComponent("\(Identifiers.compactTimestamp())-\(safeLabel)-\(UUID().uuidString)", isDirectory: true)
        try fm.ensureDirectory(sessionDir)
        try JSONFile.write(plan, to: sessionDir.appendingPathComponent("session.json"))

        // Open the append target before spawn; the descriptor remains owned until the audit is committed.
        try fm.ensureDirectory(paths.audit)
        let auditURL = paths.audit.appendingPathComponent("spawn.jsonl")
        let auditFD = open(auditURL.path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0o600)
        guard auditFD >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        defer { close(auditFD) }
        let pid = try Spawn.launch(
            executable: plan.loader, arguments: plan.argv, environment: plan.env,
            workingDirectory: plan.cwd, logPath: sessionDir.appendingPathComponent("wine.log").path
        )
        var auditWarning: String?
        do {
            try appendAudit(plan: plan, pid: pid, descriptor: auditFD)
        } catch {
            auditWarning = "程序已启动，但审计记录未写入；会话所有权仍保留。"
            // Retain the true started state and session ownership, even if the post-spawn disk write fails.
            try? JSONFile.write(["warning": "spawn audit write failed", "pid": String(pid)],
                                to: sessionDir.appendingPathComponent("audit-warning.json"))
            FileHandle.standardError.write(Data("Cider: 程序已启动，但审计记录未写入：\(error)\n".utf8))
        }
        return SessionHandle(pid: pid, directory: sessionDir, warning: auditWarning)
    }

    /// Runs a plan to completion and returns its exit code.
    public func runToCompletion(_ plan: LaunchPlan) throws -> (code: Int32, session: SessionHandle) {
        try Task.checkCancellation()
        let session = try launch(plan)
        // Cancellation may race with spawn; stop this prefix before handing the cancelled result back.
        if Task.isCancelled { try killAll() }
        let code = Spawn.wait(session.pid)
        try Task.checkCancellation()
        return (code, session)
    }

    /// `wineserver -w`: waits until every process in the prefix has exited.
    public func waitForIdle() throws {
        try wineserver(["-w"])
    }

    /// `wineserver -k`: kills every process in the prefix. Processes whose wineserver already died (orphans,
    /// which ignore SIGTERM) are found by the bottle tag in their environment and killed as well.
    public func killAll() throws {
        try withBottleOperation { try killAllLocked() }
    }

    private func killAllLocked() throws {
        // Wine returns 1 when no server owns the lock; still clean up tagged orphan processes.
        try wineserver(["-k"], allowNoServer: true)
        let matches = { (process: WineProcess) in
            process.bottleID == bottleID && process.prefixPath == prefix.path
        }
        // Include loaders which have not rewritten argv to a Windows image yet, and tagged host helpers.
        let leftovers = ProcessScanner.scan(includeLoaders: true).filter(matches)
        let remaining = ProcessScanner.terminate(leftovers, grace: 2)
        let deadline = ProcessInfo.processInfo.systemUptime + 2
        while ProcessInfo.processInfo.systemUptime < deadline,
              ProcessScanner.scan(includeLoaders: true).contains(where: matches) || BottleActivity.isRunning(prefix: prefix) {
            usleep(100_000)
        }
        guard remaining.isEmpty, !ProcessScanner.scan(includeLoaders: true).contains(where: matches),
              !BottleActivity.isRunning(prefix: prefix) else {
            throw CiderError.invalid("瓶子仍有进程运行，停止未完成：\(bottleID)")
        }
    }

    private func wineserver(_ args: [String], allowNoServer: Bool = false) throws {
        let log = paths.logs.appendingPathComponent("wineserver.log")
        try FileManager.default.ensureDirectory(paths.logs)
        let pid = try Spawn.launch(executable: engine.wineserver.path, arguments: args,
                                   environment: environment(debug: .quiet), workingDirectory: nil, logPath: log.path)
        let code = Spawn.wait(pid)
        guard code == 0 || (allowNoServer && code == 1) else {
            throw CiderError.commandFailed(command: "wineserver \(args.joined(separator: " "))", status: code,
                                           output: (try? String(contentsOf: log, encoding: .utf8)) ?? "")
        }
    }

    private func appendAudit(plan: LaunchPlan, pid: pid_t, descriptor: Int32) throws {
        let entry: [String: String] = [
            "t": Identifiers.timestamp(), "pid": String(pid), "bottle": plan.bottleID,
            "engine": plan.engineID, "program": plan.argv.first ?? "",
        ]
        let data = try JSONSerialization.data(withJSONObject: entry, options: [.sortedKeys]) + Data([0x0A])
        try FileOperationLock.withLock(at: paths.state.appendingPathComponent("locks/spawn-audit.lock")) {
            try data.withUnsafeBytes { bytes in
                var offset = 0
                while offset < bytes.count {
                    let count = write(descriptor, bytes.baseAddress!.advanced(by: offset), bytes.count - offset)
                    if count < 0 && errno == EINTR { continue }
                    guard count > 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
                    offset += count
                }
            }
        }
    }
}
