import CiderCore
import CiderRuntime
import CiderStore
import Foundation

/// Support bundle (docs/plan/05 §15 row 48, `.ciderlog`): a zip with the bottle's recent session logs, its
/// config, the engine manifest, the Mac's OS/hardware and the running Windows processes. The user's name and
/// home path are replaced everywhere, so the file can be posted publicly.
public enum Diagnostics {
    @discardableResult
    public static func bundle(for bottle: Bottle, store: BottleStore, recentSessions: Int = 5, to directory: URL) throws -> URL {
        let fm = FileManager.default
        let stamp = Identifiers.compactTimestamp()
        let name = "Cider-\(bottle.config.id)-\(stamp)"
        let staging = fm.temporaryDirectory.appendingPathComponent(name, isDirectory: true)
        try? fm.removeItem(at: staging)
        try fm.ensureDirectory(staging)
        defer { try? fm.removeItem(at: staging) }

        // Sessions, newest first.
        let sessionsRoot = store.paths.logs.appendingPathComponent("sessions/\(bottle.config.id)", isDirectory: true)
        let sessions = ((try? fm.contentsOfDirectory(atPath: sessionsRoot.path)) ?? []).sorted(by: >).prefix(recentSessions)
        for session in sessions {
            let src = sessionsRoot.appendingPathComponent(session, isDirectory: true)
            let dst = staging.appendingPathComponent("sessions/\(session)", isDirectory: true)
            try fm.ensureDirectory(dst)
            for file in ["wine.log", "session.json"] {
                let from = src.appendingPathComponent(file)
                guard let data = fm.contents(atPath: from.path) else { continue }
                // Wine logs of a long session can be huge; keep the head and the tail.
                try redact(truncated(data)).write(to: dst.appendingPathComponent(file))
            }
        }

        if let data = fm.contents(atPath: bottle.configURL.path) { try redact(data).write(to: staging.appendingPathComponent("cider-bottle.json")) }
        if let engine = try? store.engines.engine(bottle.config.engine.id),
           let data = fm.contents(atPath: engine.directory.appendingPathComponent("manifest.json").path) {
            try redact(data).write(to: staging.appendingPathComponent("engine-manifest.json"))
        }

        var system: [String] = []
        let os = ProcessInfo.processInfo.operatingSystemVersion
        system.append("macOS \(os.majorVersion).\(os.minorVersion).\(os.patchVersion)")
        system.append("chip: \((try? Command.run("/usr/sbin/sysctl", ["-n", "machdep.cpu.brand_string"]))?.trimmingCharacters(in: .whitespacesAndNewlines) ?? "?")")
        system.append("memory: \(ProcessInfo.processInfo.physicalMemory / 1_073_741_824) GB")
        system.append("rosetta: \((try? Command.run("/usr/bin/arch", ["-x86_64", "/usr/bin/true"])) != nil ? "yes" : "no")")
        system.append("gstreamer: \(fm.fileExists(atPath: WineRunner.gstreamerFramework) ? "yes" : "no")")
        system.append("")
        system.append("windows processes:")
        for p in ProcessScanner.scan() where p.bottleID == bottle.config.id {
            system.append("  \(p.pid)  \(p.windowsImage)")
        }
        try redact(Data(system.joined(separator: "\n").utf8)).write(to: staging.appendingPathComponent("system.txt"))

        try fm.ensureDirectory(directory)
        let zip = directory.appendingPathComponent("\(name).zip")
        try Command.run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--noextattr", "--keepParent", staging.path, zip.path])
        return zip
    }

    static func truncated(_ data: Data, limit: Int = 4 << 20) -> Data {
        guard data.count > limit else { return data }
        let half = limit / 2
        return data.prefix(half) + Data("\n\n… [\(data.count - limit) bytes omitted] …\n\n".utf8) + data.suffix(half)
    }

    /// Replaces the home directory and the account name (also inside Wine paths like C:\users\<name>).
    static func redact(_ data: Data, home: String = NSHomeDirectory(), user: String = NSUserName()) -> Data {
        guard var text = String(data: data, encoding: .utf8) else { return data }
        text = text.replacingOccurrences(of: home, with: "~")
        if user.count >= 3 {
            text = text.replacingOccurrences(of: "\\users\\\(user)", with: "\\users\\<user>", options: .caseInsensitive)
            text = text.replacingOccurrences(of: "/users/\(user)", with: "/users/<user>", options: .caseInsensitive)
            text = text.replacingOccurrences(of: user, with: "<user>")
        }
        return Data(text.utf8)
    }
}
