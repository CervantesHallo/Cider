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
        var actions: posix_spawn_file_actions_t?
        posix_spawn_file_actions_init(&actions)
        defer { posix_spawn_file_actions_destroy(&actions) }
        posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0)
        posix_spawn_file_actions_addopen(&actions, 1, logPath, O_WRONLY | O_CREAT | O_APPEND, 0o644)
        posix_spawn_file_actions_adddup2(&actions, 1, 2)
        if let workingDirectory { posix_spawn_file_actions_addchdir_np(&actions, workingDirectory) }

        var attributes: posix_spawnattr_t?
        posix_spawnattr_init(&attributes)
        defer { posix_spawnattr_destroy(&attributes) }
        posix_spawnattr_setflags(&attributes, Int16(POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT))
        posix_spawnattr_setpgroup(&attributes, 0)

        let argv = ([executable] + arguments).map { strdup($0) } + [nil]
        let envp = environment.map { strdup("\($0.key)=\($0.value)") } + [nil]
        defer {
            argv.forEach { free($0) }
            envp.forEach { free($0) }
        }

        var pid: pid_t = 0
        let rc = posix_spawn(&pid, executable, &actions, &attributes, argv, envp)
        guard rc == 0 else {
            throw CiderError.commandFailed(command: executable, status: rc, output: String(cString: strerror(rc)))
        }
        return pid
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
