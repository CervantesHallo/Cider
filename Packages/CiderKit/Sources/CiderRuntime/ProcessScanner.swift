import Darwin
import Foundation

/// A running Windows process of some Cider bottle.
public struct WineProcess: Hashable, Sendable {
    public let pid: pid_t
    public let bottleID: String
    /// Windows path of the executable as Wine reports it in argv[0], e.g. `C:\Program Files (x86)\Steam\steam.exe`.
    public let windowsImage: String

    /// Lower-cased, forward-slash image path for matching.
    public var normalizedImage: String { windowsImage.replacingOccurrences(of: "\\", with: "/").lowercased() }
    public var imageName: String { normalizedImage.split(separator: "/").last.map(String.init) ?? normalizedImage }

    /// Wine's own service processes, which belong to the bottle rather than to any app.
    public var isWineInfrastructure: Bool {
        let infra: Set<String> = ["winedevice.exe", "services.exe", "explorer.exe", "plugplay.exe", "rpcss.exe", "svchost.exe",
                                  "conhost.exe", "start.exe", "winemenubuilder.exe", "wineboot.exe", "tabtip.exe", "rundll32.exe"]
        return normalizedImage.hasPrefix("c:/windows/") && infra.contains(imageName)
    }
}

/// Finds Wine processes launched by Cider by reading each process's argv/environment (KERN_PROCARGS2).
/// Cider sets `CIDER_BOTTLE` for every session and Wine children inherit it, so attribution is exact.
public enum ProcessScanner {
    public static func scan() -> [WineProcess] {
        let uid = getuid()
        var result: [WineProcess] = []
        for pid in allPIDs() where pid > 0 {
            var info = proc_bsdinfo()
            guard proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size)) > 0,
                  info.pbi_uid == uid else { continue }
            guard let args = processArguments(pid), args.executable.contains("/wine"),
                  let bottle = args.environment["CIDER_BOTTLE"],
                  let image = args.argv.first, image.count > 2, image.dropFirst().first == ":" else { continue }
            result.append(WineProcess(pid: pid, bottleID: bottle, windowsImage: image))
        }
        return result
    }

    /// Sends SIGTERM, waits up to `grace` seconds, then SIGKILLs whatever is left.
    public static func terminate(_ pids: [pid_t], grace: TimeInterval = 3) {
        guard !pids.isEmpty else { return }
        for pid in pids { kill(pid, SIGTERM) }
        let deadline = Date().addingTimeInterval(grace)
        var alive = pids
        while !alive.isEmpty, Date() < deadline {
            usleep(100_000)
            alive = alive.filter { kill($0, 0) == 0 }
        }
        for pid in alive { kill(pid, SIGKILL) }
    }

    static func allPIDs() -> [pid_t] {
        let count = proc_listallpids(nil, 0)
        guard count > 0 else { return [] }
        var pids = [pid_t](repeating: 0, count: Int(count) + 64)
        let filled = pids.withUnsafeMutableBufferPointer { buf in
            proc_listallpids(buf.baseAddress, Int32(buf.count * MemoryLayout<pid_t>.size))
        }
        return Array(pids.prefix(Int(max(filled, 0))))
    }

    struct Arguments {
        let executable: String
        let argv: [String]
        let environment: [String: String]
    }

    /// Parses KERN_PROCARGS2: argc, exec path, argv strings, environment strings.
    /// Wine rewrites argv to the Windows command line, so argc may not match the string count; environment
    /// variables are recognised by their KEY=VALUE shape instead of by position.
    static func processArguments(_ pid: pid_t) -> Arguments? {
        var mib: [Int32] = [CTL_KERN, KERN_PROCARGS2, pid]
        var size = 0
        guard sysctl(&mib, 3, nil, &size, nil, 0) == 0, size > 4 else { return nil }
        var buffer = [UInt8](repeating: 0, count: size)
        guard sysctl(&mib, 3, &buffer, &size, nil, 0) == 0 else { return nil }
        let argc = Int(buffer.withUnsafeBytes { $0.load(as: Int32.self) })
        let strings = buffer[4..<size].split(separator: 0, omittingEmptySubsequences: true)
            .map { String(decoding: $0, as: UTF8.self) }
        guard let executable = strings.first else { return nil }
        let argv = Array(strings.dropFirst().prefix(max(argc, 1)))
        var environment: [String: String] = [:]
        for entry in strings.dropFirst() {
            guard let eq = entry.firstIndex(of: "="), eq != entry.startIndex else { continue }
            let key = entry[..<eq]
            guard key.allSatisfy({ $0.isUppercase || $0.isNumber || $0 == "_" }) else { continue }
            environment[String(key)] = String(entry[entry.index(after: eq)...])
        }
        return Arguments(executable: executable, argv: argv, environment: environment)
    }
}
