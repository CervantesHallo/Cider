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

    public init(paths: CiderPaths, engine: InstalledEngine, prefix: URL, bottleID: String, locale: BottleLocale,
                sync: SyncMode = .default, bottleEnvironment: [String: String] = [:]) {
        self.paths = paths
        self.engine = engine
        self.prefix = prefix
        self.bottleID = bottleID
        self.locale = locale
        self.sync = sync
        self.bottleEnvironment = bottleEnvironment
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
        for (key, value) in extra { env[key] = value }
        // Bottle-wide settings come last so a per-launch extra can never split the prefix.
        for (key, value) in sync.environment { env[key] = value }
        // R3 preflight inside the engine: Cider's engines refuse these images at NtCreateUserProcess
        // (engine/patches/cider/0001), so a launcher in the bottle cannot start them either.
        env["CIDER_PREFLIGHT_DENY"] = Preflight.gatedExecutables.keys.sorted().joined(separator: ";")
        for (key, value) in locale.environment { env[key] = value }
        return env
    }

    public func plan(program: String, arguments: [String] = [], label: String? = nil, cwd: URL? = nil,
                     debug: DebugPreset = .default, extraEnv: [String: String] = [:]) -> LaunchPlan {
        LaunchPlan(
            bottleID: bottleID,
            engineID: engine.manifest.id,
            loader: engine.wine.path,
            argv: [program] + arguments,
            env: environment(debug: debug, extra: extraEnv),
            cwd: cwd?.path,
            label: label ?? URL(fileURLWithPath: program.replacingOccurrences(of: "\\", with: "/")).deletingPathExtension().lastPathComponent
        )
    }

    /// Spawns the plan, recording `session.json` and an audit line. Returns immediately.
    public func launch(_ plan: LaunchPlan) throws -> SessionHandle {
        // R3 preflight: gated games never start (any argv element counts, so `cmd /c start x.exe` is caught too).
        if let block = plan.argv.lazy.compactMap({ Preflight.check(program: $0, db: nil) }).first {
            throw block
        }
        let fm = FileManager.default
        let safeLabel = plan.label.replacingOccurrences(of: "/", with: "_")
        let sessionDir = paths.sessions
            .appendingPathComponent(plan.bottleID, isDirectory: true)
            .appendingPathComponent("\(Identifiers.compactTimestamp())-\(safeLabel)", isDirectory: true)
        try fm.ensureDirectory(sessionDir)
        try JSONFile.write(plan, to: sessionDir.appendingPathComponent("session.json"))

        let pid = try Spawn.launch(
            executable: plan.loader, arguments: plan.argv, environment: plan.env,
            workingDirectory: plan.cwd, logPath: sessionDir.appendingPathComponent("wine.log").path
        )
        try appendAudit(plan: plan, pid: pid)
        return SessionHandle(pid: pid, directory: sessionDir)
    }

    /// Runs a plan to completion and returns its exit code.
    public func runToCompletion(_ plan: LaunchPlan) throws -> (code: Int32, session: SessionHandle) {
        let session = try launch(plan)
        return (Spawn.wait(session.pid), session)
    }

    /// `wineserver -w`: waits until every process in the prefix has exited.
    public func waitForIdle() throws {
        try wineserver(["-w"])
    }

    /// `wineserver -k`: kills every process in the prefix. Processes whose wineserver already died (orphans,
    /// which ignore SIGTERM) are found by the bottle tag in their environment and killed as well.
    public func killAll() throws {
        try wineserver(["-k"])
        let leftovers = ProcessScanner.scan().filter { $0.bottleID == bottleID }.map(\.pid)
        if !leftovers.isEmpty { ProcessScanner.terminate(leftovers, grace: 2) }
    }

    private func wineserver(_ args: [String]) throws {
        let log = paths.logs.appendingPathComponent("wineserver.log")
        try FileManager.default.ensureDirectory(paths.logs)
        let pid = try Spawn.launch(executable: engine.wineserver.path, arguments: args,
                                   environment: environment(debug: .quiet), workingDirectory: nil, logPath: log.path)
        _ = Spawn.wait(pid)
    }

    private func appendAudit(plan: LaunchPlan, pid: pid_t) throws {
        try FileManager.default.ensureDirectory(paths.audit)
        let entry: [String: String] = [
            "t": Identifiers.timestamp(), "pid": String(pid), "bottle": plan.bottleID,
            "engine": plan.engineID, "program": plan.argv.first ?? "",
        ]
        let data = try JSONSerialization.data(withJSONObject: entry, options: [.sortedKeys]) + Data([0x0A])
        let url = paths.audit.appendingPathComponent("spawn.jsonl")
        if let handle = try? FileHandle(forWritingTo: url) {
            defer { try? handle.close() }
            try handle.seekToEnd()
            try handle.write(contentsOf: data)
        } else {
            try data.write(to: url)
        }
    }
}
