import CiderRuntime
import Darwin
import Foundation

/// Library app operations shared by the UI and future command/shortcut entry points.
/// Work is synchronous; callers run it off the main actor and serialize actions for the bottle.
public enum AppLifecycle {
    public enum Failure: Error, CustomStringConvertible, Sendable {
        case stillRunning([pid_t])
        public var description: String {
            switch self {
            case .stillRunning(let pids): return "仍有进程运行或无法确认退出（PID：\(pids.map(String.init).joined(separator: ", "))），未重新启动。"
            }
        }
    }

    /// Returns nil for an existing instance instead of spawning another single-instance launcher.
    public static func start(_ app: CatalogApp, using runner: WineRunner, cwd: URL?) throws -> SessionHandle? {
        try start(matching: { ProcessScanner.scan().filter { $0.prefixPath == runner.prefix.path && app.owns($0) } }) {
            let isSteam: Bool = { if case .program = app.kind { return false }; return true }()
            return try runner.launch(runner.plan(program: app.launchProgram, arguments: app.launchArguments,
                                                 label: app.title, cwd: cwd,
                                                 extraEnv: (isSteam ? SteamLibrary.uiEnvironment : [:])
                                                    .merging(app.launchEnvironment) { $1 }))
        }
    }

    public static func stop(_ app: CatalogApp, using runner: WineRunner) throws {
        let matching = { ProcessScanner.scan().filter { $0.prefixPath == runner.prefix.path && app.owns($0) } }
        // Do not start Steam just to shut down a client which has already exited.
        if case .steamClient = app.kind, !matching().isEmpty {
            _ = try? runner.launch(runner.plan(program: app.launchProgram, arguments: ["-shutdown"],
                                               label: "steam-shutdown", extraEnv: SteamLibrary.uiEnvironment))
            let deadline = ProcessInfo.processInfo.systemUptime + 8
            while !matching().isEmpty, ProcessInfo.processInfo.systemUptime < deadline { usleep(200_000) }
        }
        try drain(matching: matching, terminate: { ProcessScanner.terminate($0) }, pause: { usleep(200_000) })
    }

    public static func restart(_ app: CatalogApp, using runner: WineRunner, cwd: URL?) throws -> SessionHandle? {
        try stop(app, using: runner)
        return try start(app, using: runner, cwd: cwd)
    }

    static func start<T>(matching: () -> [WineProcess], spawn: () throws -> T) rethrows -> T? {
        guard matching().isEmpty else { return nil }
        return try spawn()
    }

    /// Rescan for helper replacements, and require two quiet samples before declaring completion.
    /// Bound attempts so a self-respawning program produces a visible failure rather than an endless wait.
    static func drain(matching: () -> [WineProcess], terminate: ([WineProcess]) -> [WineProcess],
                      pause: () -> Void) throws {
        for _ in 0..<3 {
            let captured = matching()
            let remaining = terminate(captured)
            guard remaining.isEmpty else { throw Failure.stillRunning(remaining.map(\.pid)) }
            pause()
            if !matching().isEmpty { continue }
            pause()
            if matching().isEmpty { return }
        }
        throw Failure.stillRunning(matching().map(\.pid))
    }
}
