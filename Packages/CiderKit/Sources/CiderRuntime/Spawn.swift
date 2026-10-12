import CiderCore
import Darwin
import Foundation

/// Thin posix_spawn wrapper: own process group, stdin from /dev/null, stdout+stderr appended to a log file,
/// and no inherited descriptors (POSIX_SPAWN_CLOEXEC_DEFAULT).
public enum Spawn {
    public static func launch(
        executable: String,
        arguments: [String],
        environment: [String: String],
        workingDirectory: String?,
        logPath: String
    ) throws -> pid_t {
        guard !executable.isEmpty, !logPath.isEmpty,
              !executable.contains("\0"), !logPath.contains("\0"),
              arguments.allSatisfy({ !$0.contains("\0") }),
              workingDirectory.map({ !$0.isEmpty && !$0.contains("\0") }) ?? true,
              environment.allSatisfy({ key, value in
                  !key.isEmpty && !key.contains("=") && !key.contains("\0") && !value.contains("\0")
              }) else { throw CiderError.invalid("启动路径、参数或环境格式无效。") }
        var actions: posix_spawn_file_actions_t?
        try requireSuccess(posix_spawn_file_actions_init(&actions))
        defer { posix_spawn_file_actions_destroy(&actions) }
        try requireSuccess(posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0))
        try requireSuccess(posix_spawn_file_actions_addopen(&actions, 1, logPath, O_WRONLY | O_CREAT | O_APPEND, 0o644))
        try requireSuccess(posix_spawn_file_actions_adddup2(&actions, 1, 2))
        if let workingDirectory { try requireSuccess(posix_spawn_file_actions_addchdir_np(&actions, workingDirectory)) }

        var attributes: posix_spawnattr_t?
        try requireSuccess(posix_spawnattr_init(&attributes))
        defer { posix_spawnattr_destroy(&attributes) }
        try requireSuccess(posix_spawnattr_setflags(&attributes, Int16(POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT)))
        try requireSuccess(posix_spawnattr_setpgroup(&attributes, 0))

        var argv: [UnsafeMutablePointer<CChar>?] = []
        var envp: [UnsafeMutablePointer<CChar>?] = []
        defer {
            argv.forEach { free($0) }
            envp.forEach { free($0) }
        }
        for argument in [executable] + arguments { argv.append(try allocatedCString(argument)) }
        for (key, value) in environment { envp.append(try allocatedCString("\(key)=\(value)")) }
        argv.append(nil)
        envp.append(nil)

        var pid: pid_t = 0
        let rc = posix_spawn(&pid, executable, &actions, &attributes, argv, envp)
        guard rc == 0 else {
            throw CiderError.commandFailed(command: executable, status: rc, output: String(cString: strerror(rc)))
        }
        return pid
    }

    private static func requireSuccess(_ code: Int32) throws {
        guard code == 0 else { throw POSIXError(POSIXErrorCode(rawValue: code) ?? .EIO) }
    }

    private static func allocatedCString(_ value: String) throws -> UnsafeMutablePointer<CChar> {
        guard let pointer = strdup(value) else { throw POSIXError(.ENOMEM) }
        return pointer
    }

    /// Blocks until `pid` exits. Returns the exit code, or 128+signal when killed by a signal.
    public static func wait(_ pid: pid_t) -> Int32 {
        var status: Int32 = 0
        while waitpid(pid, &status, 0) == -1 {
            if errno != EINTR { return -1 }
        }
        let signal = status & 0x7f
        return signal == 0 ? (status >> 8) & 0xff : 128 + signal
    }
}
