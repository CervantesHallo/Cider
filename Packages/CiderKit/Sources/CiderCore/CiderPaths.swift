import Foundation

/// On-disk layout (docs/plan/01-architecture.md §4).
public struct CiderPaths: Sendable {
    public let appSupport: URL
    public let caches: URL
    public let logs: URL

    public init(appSupport: URL, caches: URL, logs: URL) {
        self.appSupport = appSupport
        self.caches = caches
        self.logs = logs
    }

    /// Standard per-user locations. `CIDER_HOME` overrides all three (tests, CI).
    public static func standard(environment: [String: String] = ProcessInfo.processInfo.environment) -> CiderPaths {
        if let home = environment["CIDER_HOME"], !home.isEmpty {
            let root = URL(fileURLWithPath: home, isDirectory: true)
            return CiderPaths(
                appSupport: root.appendingPathComponent("AppSupport", isDirectory: true),
                caches: root.appendingPathComponent("Caches", isDirectory: true),
                logs: root.appendingPathComponent("Logs", isDirectory: true)
            )
        }
        let library = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library", isDirectory: true)
        return CiderPaths(
            appSupport: library.appendingPathComponent("Application Support/Cider", isDirectory: true),
            caches: library.appendingPathComponent("Caches/Cider", isDirectory: true),
            logs: library.appendingPathComponent("Logs/Cider", isDirectory: true)
        )
    }

    public var engines: URL { appSupport.appendingPathComponent("Engines", isDirectory: true) }
    public var components: URL { appSupport.appendingPathComponent("Components", isDirectory: true) }
    public var bottles: URL { appSupport.appendingPathComponent("Bottles", isDirectory: true) }
    public var state: URL { appSupport.appendingPathComponent("State", isDirectory: true) }
    public var downloads: URL { caches.appendingPathComponent("downloads", isDirectory: true) }
    public var sessions: URL { logs.appendingPathComponent("sessions", isDirectory: true) }
    public var audit: URL { logs.appendingPathComponent("audit", isDirectory: true) }
}

extension FileManager {
    /// Creates the directory (and parents) if missing.
    public func ensureDirectory(_ url: URL) throws {
        try createDirectory(at: url, withIntermediateDirectories: true)
    }
}
