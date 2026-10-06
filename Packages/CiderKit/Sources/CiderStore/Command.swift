import CiderCore
import Darwin
import Foundation

/// Runs a short-lived host tool and returns its combined output. Not for Wine sessions (see CiderRuntime).
public enum Command {
    /// A cancellable host tool (recipe downloads). File-backed output avoids blocking on a pipe read
    /// while the task is cancelled; only this process's own child is terminated.
    @discardableResult
    public static func runCancellable(_ executable: String, _ arguments: [String]) throws -> String {
        try Task.checkCancellation()
        let log = FileManager.default.temporaryDirectory.appendingPathComponent("cider-command-\(UUID().uuidString)")
        guard FileManager.default.createFile(atPath: log.path, contents: nil, attributes: [.posixPermissions: 0o600]) else {
            throw CiderError.invalid("cannot create command log")
        }
        defer { try? FileManager.default.removeItem(at: log) }
        let outputFile = try FileHandle(forWritingTo: log)
        defer { try? outputFile.close() }
        var actions: posix_spawn_file_actions_t?
        posix_spawn_file_actions_init(&actions)
        defer { posix_spawn_file_actions_destroy(&actions) }
        posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0)
        posix_spawn_file_actions_adddup2(&actions, outputFile.fileDescriptor, 1)
        posix_spawn_file_actions_adddup2(&actions, outputFile.fileDescriptor, 2)
        var attributes: posix_spawnattr_t?
        posix_spawnattr_init(&attributes)
        defer { posix_spawnattr_destroy(&attributes) }
        var mask = sigset_t()
        sigemptyset(&mask)
        posix_spawnattr_setsigmask(&attributes, &mask)
        var defaults = sigset_t()
        sigemptyset(&defaults)
        for signal in [SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGPIPE] { sigaddset(&defaults, signal) }
        posix_spawnattr_setsigdefault(&attributes, &defaults)
        posix_spawnattr_setflags(&attributes, Int16(POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF))
        let argv = ([executable] + arguments).map { strdup($0) } + [nil]
        // These are host download tools, not Wine; they keep the caller's ordinary host environment.
        let envp = ProcessInfo.processInfo.environment.map { strdup("\($0.key)=\($0.value)") } + [nil]
        defer { argv.forEach { free($0) }; envp.forEach { free($0) } }
        var pid: pid_t = 0
        let rc = posix_spawn(&pid, executable, &actions, &attributes, argv, envp)
        guard rc == 0 else { throw CiderError.commandFailed(command: executable, status: rc, output: String(cString: strerror(rc))) }
        var status: Int32 = 0
        var cancelTime: TimeInterval?
        var forced = false
        while true {
            let result = waitpid(pid, &status, WNOHANG)
            if result == pid { break }
            if result == -1 {
                if errno == EINTR { continue }
                throw CiderError.invalid("cannot observe command exit: \(String(cString: strerror(errno)))")
            }
            // This child has not been reaped: its PID cannot be reused between this check and signal.
            if Task.isCancelled {
                let now = ProcessInfo.processInfo.systemUptime
                if cancelTime == nil { cancelTime = now; _ = kill(pid, SIGTERM) }
                if let start = cancelTime, now - start >= 1, !forced { _ = kill(pid, SIGKILL); forced = true }
                if let start = cancelTime, now - start >= 3 {
                    // Keep ownership of the unreaped child if exit is delayed by the OS, but report failure.
                    let childPID = pid
                    Task.detached {
                        var delayedStatus: Int32 = 0
                        while waitpid(childPID, &delayedStatus, 0) == -1 && errno == EINTR {}
                    }
                    throw CiderError.invalid("取消未完成：下载进程尚未退出（PID：\(pid)）。")
                }
            }
            Thread.sleep(forTimeInterval: 0.05)
        }
        try Task.checkCancellation()
        let output = String(decoding: try Data(contentsOf: log), as: UTF8.self)
        let signal = status & 0x7f
        let code = signal == 0 ? (status >> 8) & 0xff : 128 + signal
        guard code == 0 else {
            throw CiderError.commandFailed(command: ([executable] + arguments).joined(separator: " "),
                                           status: code, output: output)
        }
        return output
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
