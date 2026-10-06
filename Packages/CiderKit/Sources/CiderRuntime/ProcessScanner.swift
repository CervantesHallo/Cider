import Darwin
import Foundation

/// A running Windows process of some Cider bottle.
public struct WineProcess: Hashable, Sendable {
    public let pid: pid_t
    public let bottleID: String
    /// Windows path of the executable as Wine reports it in argv[0], e.g. `C:\Program Files (x86)\Steam\steam.exe`.
    public let windowsImage: String
    /// Host start time, captured with the PID. A PID alone can be reused after a process exits.
    public let startTime: UInt64?
    public let prefixPath: String?
    /// Kernel-supplied audit identity (includes the PID version), when metadata access is available.
    let auditIdentity: [UInt32]?

    public init(pid: pid_t, bottleID: String, windowsImage: String, startTime: UInt64? = nil) {
        self.init(pid: pid, bottleID: bottleID, windowsImage: windowsImage, startTime: startTime, auditIdentity: nil)
    }

    init(pid: pid_t, bottleID: String, windowsImage: String, startTime: UInt64?, auditIdentity: [UInt32]?, prefixPath: String? = nil) {
        self.pid = pid
        self.bottleID = bottleID
        self.windowsImage = windowsImage
        self.startTime = startTime
        self.auditIdentity = auditIdentity
        self.prefixPath = prefixPath
    }

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
    public static func scan(includeLoaders: Bool = false) -> [WineProcess] {
        let uid = getuid()
        var result: [WineProcess] = []
        for pid in allPIDs() where pid > 0 {
            var info = proc_bsdinfo()
            guard proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size)) > 0,
                  info.pbi_uid == uid, info.pbi_status != SZOMB else { continue }
            guard let args = processArguments(pid), args.executable.contains("/wine"),
                  let bottle = args.environment["CIDER_BOTTLE"],
                  let prefix = args.environment["WINEPREFIX"], let image = args.argv.first else { continue }
            let isWindowsImage = image.count > 2 && image.dropFirst().first == ":"
            guard isWindowsImage || includeLoaders else { continue }
            let identity = auditIdentity(for: pid)
            let process = WineProcess(pid: pid, bottleID: bottle, windowsImage: image, startTime: startTime(info), auditIdentity: identity, prefixPath: prefix)
            // argv and the process metadata were read separately; reject a PID reused between the reads.
            if state(of: process) == .running { result.append(process) }
        }
        return result
    }

    /// Stops captured processes and waits for their exit. Unverifiable identities are never signalled and
    /// remain in the returned list; callers must not claim success or restart while that list is nonempty.
    @discardableResult
    public static func terminate(_ processes: [WineProcess], grace: TimeInterval = 3,
                                 forceWait: TimeInterval = 2) -> [WineProcess] {
        terminate(processes, grace: grace, forceWait: forceWait, state: state(of:), signal: { process, signal in
            // Recheck immediately before every signal, including SIGKILL after the grace period.
            if state(of: process) == .running { _ = signalCaptured(process, signal) }
        }, now: { ProcessInfo.processInfo.systemUptime }, pause: { usleep(100_000) })
    }

    enum State { case running, exited, unknown }

    static func startTime(_ info: proc_bsdinfo) -> UInt64 {
        info.pbi_start_tvsec * 1_000_000 + info.pbi_start_tvusec
    }

    static func state(of process: WineProcess) -> State {
        guard process.pid > 1, let birth = process.startTime else { return .unknown }
        var info = proc_bsdinfo()
        let read = proc_pidinfo(process.pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size))
        guard read == MemoryLayout<proc_bsdinfo>.size else { return errno == ESRCH ? .exited : .unknown }
        guard info.pbi_uid == getuid(), startTime(info) == birth, info.pbi_status != SZOMB else { return .exited }
        return .running
    }

    static func auditIdentity(for pid: pid_t) -> [UInt32]? {
        var task: mach_port_name_t = 0
        guard task_name_for_pid(mach_task_self_, pid, &task) == KERN_SUCCESS else { return nil }
        defer { mach_port_deallocate(mach_task_self_, task) }
        var token = audit_token_t()
        let capacity = MemoryLayout<audit_token_t>.size / MemoryLayout<natural_t>.size
        var count = mach_msg_type_number_t(capacity)
        let result = withUnsafeMutablePointer(to: &token) { pointer in
            pointer.withMemoryRebound(to: integer_t.self, capacity: capacity) {
                task_info(task, task_flavor_t(TASK_AUDIT_TOKEN), $0, &count)
            }
        }
        guard result == KERN_SUCCESS, count == capacity, token.val.5 == UInt32(pid) else { return nil }
        return withUnsafeBytes(of: token) { Array($0.bindMemory(to: UInt32.self)) }
    }

    /// Newer macOS validates the audit identity while holding a process reference, closing the probe/kill race.
    /// Resolve dynamically: the API is absent from older macOS releases which Cider still supports.
    static func signalCaptured(_ process: WineProcess, _ signal: Int32) -> Int32 {
        guard state(of: process) == .running else { return ESRCH }
        if let handle = dlopen(nil, RTLD_LAZY) {
            defer { dlclose(handle) }
            if let symbol = dlsym(handle, "proc_signal_with_audittoken") {
                guard let identity = process.auditIdentity, identity.count == 8, identity[5] == UInt32(process.pid) else { return EACCES }
                var token = audit_token_t()
                withUnsafeMutableBytes(of: &token) { buffer in
                    buffer.copyBytes(from: identity.withUnsafeBytes { Data($0) })
                }
                typealias Signal = @convention(c) (UnsafeMutablePointer<audit_token_t>, Int32) -> Int32
                return unsafeBitCast(symbol, to: Signal.self)(&token, signal)
            }
        }
        // Older hosts have no public atomic signal-by-identity API. Retain the start-time check there;
        // the remaining probe/kill race is documented rather than claimed as eliminated.
        return kill(process.pid, signal) == 0 ? 0 : errno
    }

    // Injectable OS boundary for adversarial checks of PID reuse, inaccessible metadata, and delayed exit.
    static func terminate(_ processes: [WineProcess], grace: TimeInterval, forceWait: TimeInterval,
                          state: (WineProcess) -> State, signal: (WineProcess, Int32) -> Void,
                          now: () -> TimeInterval, pause: () -> Void) -> [WineProcess] {
        var pending = Array(Set(processes)).filter { state($0) != .exited }
        for process in pending where state(process) == .running { signal(process, SIGTERM) }
        func wait(_ duration: TimeInterval) {
            let deadline = now() + max(0, duration)
            pending = pending.filter { state($0) != .exited }
            while !pending.isEmpty, now() < deadline {
                pause()
                pending = pending.filter { state($0) != .exited }
            }
        }
        wait(grace)
        for process in pending where state(process) == .running { signal(process, SIGKILL) }
        wait(forceWait)
        return pending
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
