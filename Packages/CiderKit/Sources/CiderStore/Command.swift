import CiderCore
import Darwin
import Foundation

/// Runs a short-lived host tool and returns its combined output. Not for Wine sessions (see CiderRuntime).
public enum Command {
    /// A cancellable host tool (recipe downloads). File-backed output avoids blocking on a pipe read
    /// while the task is cancelled; only this process's own child is terminated.
    @discardableResult
    public static func runCancellable(_ executable: String, _ arguments: [String],
                                       environment: [String: String]? = nil,
                                       timeout: TimeInterval? = nil, outputLimit: Int? = nil,
                                       retainUntilReaped: Bool = false,
                                       cancellationRequested: @Sendable () -> Bool = { false }) throws -> String {
        guard !executable.isEmpty, !executable.contains("\0"), arguments.allSatisfy({ !$0.contains("\0") }),
              timeout.map({ $0.isFinite && $0 > 0 }) ?? true,
              outputLimit.map({ $0 > 0 }) ?? true,
              environment?.allSatisfy({ !$0.key.isEmpty && !$0.key.contains("=") && !$0.key.contains("\0") && !$0.value.contains("\0") }) ?? true
        else { throw CiderError.invalid("本机组件启动参数无效。") }
        try Task.checkCancellation()
        if cancellationRequested() { throw CancellationError() }
        let log = FileManager.default.temporaryDirectory.appendingPathComponent("cider-command-\(UUID().uuidString)")
        guard FileManager.default.createFile(atPath: log.path, contents: nil, attributes: [.posixPermissions: 0o600]) else {
            throw CiderError.invalid("cannot create command log")
        }
        defer { try? FileManager.default.removeItem(at: log) }
        let outputFile = try FileHandle(forWritingTo: log)
        defer { try? outputFile.close() }
        var actions: posix_spawn_file_actions_t?
        try requireSpawnSuccess(posix_spawn_file_actions_init(&actions))
        defer { posix_spawn_file_actions_destroy(&actions) }
        try requireSpawnSuccess(posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0))
        try requireSpawnSuccess(posix_spawn_file_actions_adddup2(&actions, outputFile.fileDescriptor, 1))
        try requireSpawnSuccess(posix_spawn_file_actions_adddup2(&actions, outputFile.fileDescriptor, 2))
        var attributes: posix_spawnattr_t?
        try requireSpawnSuccess(posix_spawnattr_init(&attributes))
        defer { posix_spawnattr_destroy(&attributes) }
        var mask = sigset_t()
        sigemptyset(&mask)
        try requireSpawnSuccess(posix_spawnattr_setsigmask(&attributes, &mask))
        var defaults = sigset_t()
        sigemptyset(&defaults)
        for signal in [SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGPIPE] { sigaddset(&defaults, signal) }
        try requireSpawnSuccess(posix_spawnattr_setsigdefault(&attributes, &defaults))
        try requireSpawnSuccess(posix_spawnattr_setflags(&attributes, Int16(POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF)))
        var argv: [UnsafeMutablePointer<CChar>?] = []
        var envp: [UnsafeMutablePointer<CChar>?] = []
        defer { argv.forEach { free($0) }; envp.forEach { free($0) } }
        for argument in [executable] + arguments { argv.append(try allocatedCString(argument)) }
        // Host downloads retain their ordinary environment. Media setup passes
        // an explicit environment and cannot inherit loader/proxy overrides.
        for (key, value) in environment ?? ProcessInfo.processInfo.environment {
            envp.append(try allocatedCString("\(key)=\(value)"))
        }
        argv.append(nil); envp.append(nil)
        var pid: pid_t = 0
        let rc = posix_spawn(&pid, executable, &actions, &attributes, argv, envp)
        guard rc == 0 else { throw CiderError.commandFailed(command: executable, status: rc, output: String(cString: strerror(rc))) }
        var status: Int32 = 0
        var cancelTime: TimeInterval?
        var forced = false
        let clock = ContinuousClock()
        let started = clock.now
        var limitFailure: String?
        while true {
            let result = waitpid(pid, &status, WNOHANG)
            if result == pid { break }
            if result == -1 {
                if errno == EINTR { continue }
                throw CiderError.invalid("cannot observe command exit: \(String(cString: strerror(errno)))")
            }
            // This child has not been reaped: its PID cannot be reused between this check and signal.
            if let timeout, started.duration(to: clock.now) >= .seconds(timeout) {
                limitFailure = "本机组件准备超时。"
            }
            if let outputLimit,
               let size = try? FileManager.default.attributesOfItem(atPath: log.path)[.size] as? NSNumber,
               size.intValue > outputLimit { limitFailure = "本机组件输出超过限制。" }
            if Task.isCancelled || cancellationRequested() || limitFailure != nil {
                let now = ProcessInfo.processInfo.systemUptime
                if cancelTime == nil { cancelTime = now; _ = kill(pid, SIGTERM) }
                if let start = cancelTime, now - start >= 1, !forced { _ = kill(pid, SIGKILL); forced = true }
                if let start = cancelTime, now - start >= 3, !retainUntilReaped {
                    // Keep ownership of the unreaped child if exit is delayed by the OS, but report failure.
                    let childPID = pid
                    Task.detached {
                        var delayedStatus: Int32 = 0
                        while waitpid(childPID, &delayedStatus, 0) == -1 && errno == EINTR {}
                    }
                    throw CiderError.invalid("取消未完成：本机操作进程尚未退出（PID：\(pid)）。")
                }
            }
            Thread.sleep(forTimeInterval: 0.05)
        }
        try Task.checkCancellation()
        if cancellationRequested() { throw CancellationError() }
        if let timeout, started.duration(to: clock.now) >= .seconds(timeout) { limitFailure = "本机组件准备超时。" }
        if let outputLimit,
           let size = try? FileManager.default.attributesOfItem(atPath: log.path)[.size] as? NSNumber,
           size.intValue > outputLimit { limitFailure = "本机组件输出超过限制。" }
        if let limitFailure { throw CiderError.invalid(limitFailure) }
        let output = String(decoding: try Data(contentsOf: log), as: UTF8.self)
        let signal = status & 0x7f
        let code = signal == 0 ? (status >> 8) & 0xff : 128 + signal
        guard code == 0 else {
            throw CiderError.commandFailed(command: ([executable] + arguments).joined(separator: " "),
                                           status: code, output: output)
        }
        return output
    }

    private static func requireSpawnSuccess(_ code: Int32) throws {
        guard code == 0 else { throw POSIXError(POSIXErrorCode(rawValue: code) ?? .EIO) }
    }

    private static func allocatedCString(_ value: String) throws -> UnsafeMutablePointer<CChar> {
        guard let pointer = strdup(value) else { throw POSIXError(.ENOMEM) }
        return pointer
    }

    @discardableResult
    public static func run(_ executable: String, _ arguments: [String], environment: [String: String]? = nil) throws -> String {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        if let environment { process.environment = environment }
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        process.standardInput = FileHandle.nullDevice
        try process.run()
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        let output = String(decoding: data, as: UTF8.self)
        guard process.terminationStatus == 0 else {
            throw CiderError.commandFailed(command: ([executable] + arguments).joined(separator: " "), status: process.terminationStatus, output: output)
        }
        return output
    }
}
