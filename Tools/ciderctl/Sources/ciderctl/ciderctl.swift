import AppKit
import CiderIPC
import CiderCommands
import Darwin
import Foundation

@main struct Ciderctl {
    static func main() {
        do { exit(try run()) }
        catch {
            FileHandle.standardError.write(Data((error.localizedDescription + "\n").utf8)); exit(75)
        }
    }
    private static func run() throws -> Int32 {
        if let local = CiderCommandExecutor.preflight(arguments: Array(CommandLine.arguments.dropFirst())) {
            if !local.message.isEmpty {
                (local.stderr ? FileHandle.standardError : FileHandle.standardOutput).write(Data(local.message.utf8))
            }
            return local.exitCode
        }
        let executable = try RPCBundle.executableURL()
        let env = ProcessInfo.processInfo.environment
        guard let app = RPCBundle.containingApp(for: executable) else {
            guard let app = RPCBundle.appForDevelopmentCLI(executable: executable, environment: env) else {
                throw RPCFailure("请先构建Cider应用，或用CIDER_APP指定完整应用包。命令未执行。")
            }
            let target = app.appendingPathComponent("Contents/MacOS/ciderctl").resolvingSymlinksInPath()
            guard target != executable else { throw RPCFailure("Cider命令行组件位置无效。") }
            _ = try RPCIdentity(bundle: app)
            var args = ([target.path] + Array(CommandLine.arguments.dropFirst())).map { strdup($0) }
            defer { args.forEach { free($0) } }; args.append(nil)
            guard args.dropLast().allSatisfy({ $0 != nil }) else { throw RPCFailure("无法分配命令参数。") }
            execv(target.path, &args)
            throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
        }
        let location = try RPCLocation(bundle: app, paths: .standard())
        let identity = try RPCIdentity(bundle: app)
        try location.register()
        let client = RPCClient(location: location, identity: identity)
        var hello = try client.hello()
        if hello.hostPID == nil {
            // The service label includes both the exact App URL and canonical data root.
            // Keep the lock through registration, then let other cold callers recheck hello.
            let launchLock = try RootLaunchLock(location: location)
            defer { launchLock.unlock() }
            hello = try client.hello()
            if hello.hostPID == nil {
                let configuration = NSWorkspace.OpenConfiguration()
                configuration.activates = false
                configuration.allowsRunningApplicationSubstitution = false
                // Reusing this App URL can return a host bound to another CIDER_HOME.
                // No host owns our root yet; the root lock serializes CLI cold launches.
                configuration.createsNewApplicationInstance = true
                var launchEnvironment: [String: String] = [:]
                let cwd = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
                for key in ["CIDER_HOME", "CIDER_DATA"] {
                    if let value = env[key], !value.isEmpty {
                        launchEnvironment[key] = canonicalPath(URL(fileURLWithPath: value, relativeTo: cwd))
                    }
                }
                configuration.environment = launchEnvironment
                let result = LaunchResult()
                NSWorkspace.shared.openApplication(at: app, configuration: configuration) { application, error in
                    let mismatch = application?.bundleURL.map { canonicalPath($0) != canonicalPath(app) } ?? true
                    result.complete(error ?? (mismatch ? RPCFailure("启动了另一位置的Cider，命令未提交。") : nil))
                }
                try result.wait(seconds: 15)
                let deadline = ProcessInfo.processInfo.systemUptime + 15
                repeat {
                    hello = try client.hello(timeout: 2)
                    if hello.hostPID != nil { break }
                    Thread.sleep(forTimeInterval: 0.1)
                } while ProcessInfo.processInfo.systemUptime < deadline
            }
        }
        guard hello.hostPID != nil else {
            throw RPCFailure("Cider未连接到当前资料目录。请核对已运行的Cider实例；命令未提交。")
        }
        let cwd = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
        let override = env["CIDER_DATA"].flatMap { $0.isEmpty ? nil : canonicalPath(URL(fileURLWithPath: $0, relativeTo: cwd)) }
        let request = RPCRequest(arguments: Array(CommandLine.arguments.dropFirst()), workingDirectory: cwd.path,
                                 appSupport: location.root, dataOverride: override)
        let response = try client.execute(request) { data, stderr in
            try (stderr ? FileHandle.standardError : FileHandle.standardOutput).write(contentsOf: data)
        }
        if let message = response.message { FileHandle.standardError.write(Data((message + "\n").utf8)) }
        if response.outputIncomplete {
            FileHandle.standardError.write(Data("命令输出未完整收到，请查看Cider日志；命令不会自动重试。\n".utf8))
            if response.exitCode == 0 { return 74 }
        }
        return response.exitCode
    }
}

/// A persistent inode is shared by cold callers; never unlink a lock another caller
/// may already have open. CLOEXEC prevents the launched application inheriting it.
private final class RootLaunchLock {
    private var descriptor: Int32
    init(location: RPCLocation, timeout: TimeInterval = 35) throws {
        let directory = location.paths.state.appendingPathComponent("XPC", isDirectory: true)
        let directoryFD = open(directory.path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)
        guard directoryFD >= 0 else { throw RPCFailure("无法打开Cider启动锁目录，命令未提交。") }
        defer { close(directoryFD) }
        var directoryInfo = stat()
        guard fstat(directoryFD, &directoryInfo) == 0,
              directoryInfo.st_mode & S_IFMT == S_IFDIR, directoryInfo.st_uid == getuid(),
              directoryInfo.st_mode & 0o077 == 0 else { throw RPCFailure("Cider启动锁目录权限无效。") }
        let fd = openat(directoryFD, location.label + ".launch.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, mode_t(0o600))
        guard fd >= 0 else { throw RPCFailure("无法打开Cider启动锁，命令未提交。") }
        do {
            var info = stat()
            guard fstat(fd, &info) == 0, info.st_mode & S_IFMT == S_IFREG, info.st_uid == getuid(),
                  info.st_nlink == 1, fchmod(fd, 0o600) == 0 else { throw RPCFailure("Cider启动锁类型或所有者无效。") }
            let deadline = ProcessInfo.processInfo.systemUptime + timeout
            while flock(fd, LOCK_EX | LOCK_NB) != 0 {
                let code = errno
                guard code == EINTR || code == EWOULDBLOCK || code == EAGAIN else {
                    throw POSIXError(POSIXErrorCode(rawValue: code) ?? .EIO)
                }
                guard ProcessInfo.processInfo.systemUptime < deadline else {
                    throw RPCFailure("等待当前资料目录的Cider启动超时，命令未提交。")
                }
                Thread.sleep(forTimeInterval: 0.05)
            }
        } catch { close(fd); throw error }
        descriptor = fd
    }
    func unlock() {
        guard descriptor >= 0 else { return }
        _ = flock(descriptor, LOCK_UN); close(descriptor); descriptor = -1
    }
    deinit { unlock() }
}

private final class LaunchResult: @unchecked Sendable {
    private let lock = NSLock()
    private var error: Error?
    private var done = false
    func complete(_ error: Error?) { lock.lock(); self.error = error; done = true; lock.unlock() }
    func wait(seconds: Double) throws {
        let deadline = ProcessInfo.processInfo.systemUptime + seconds
        while ProcessInfo.processInfo.systemUptime < deadline {
            lock.lock(); let done = done; let error = error; lock.unlock()
            if done { if let error { throw error }; return }
            // AppKit may deliver workspace completion on the caller's main run loop.
            RunLoop.current.run(until: Date(timeIntervalSinceNow: 0.02))
        }
        throw RPCFailure("等待Cider启动超时，命令未提交。")
    }
}
