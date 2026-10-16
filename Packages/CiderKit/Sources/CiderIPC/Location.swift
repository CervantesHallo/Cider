import CiderCore
import CryptoKit
import Darwin
import Foundation

public struct RPCLocation: Sendable {
    public let bundle: URL
    public let paths: CiderPaths
    public let root: String
    public let label: String
    public var clientService: String { label + ".client" }
    public var hostService: String { label + ".host" }
    public var brokerExecutable: URL { bundle.appendingPathComponent("Contents/MacOS/CiderXPCBridge") }
    public var cliExecutable: URL { bundle.appendingPathComponent("Contents/MacOS/ciderctl") }
    public init(bundle: URL, paths: CiderPaths) throws {
        guard bundle.pathExtension.lowercased() == "app", !bundle.path.contains("\0") else {
            throw RPCFailure("请使用完整的Cider应用包。")
        }
        self.bundle = bundle.standardizedFileURL.resolvingSymlinksInPath()
        self.paths = paths; root = canonicalPath(paths.appSupport)
        let key = canonicalPath(bundle) + "\n" + root
        let digest = SHA256.hash(data: Data(key.utf8)).map { String(format: "%02x", $0) }.joined()
        label = "org.cider.ipc." + digest.prefix(24)
    }

    /// Session-only on-demand registration. No LoginItem, timer, shell, or Wine process.
    public func register() throws {
        _ = try RPCIdentity(bundle: bundle)
        let directory = paths.state.appendingPathComponent("XPC", isDirectory: true)
        let directoryFD = try secureDirectory(directory)
        defer { close(directoryFD) }
        let file = directory.appendingPathComponent(label + ".plist")
        var info = stat()
        if lstat(file.path, &info) == 0, info.st_mode & S_IFMT != S_IFREG || info.st_uid != getuid() {
            throw RPCFailure("XPC配置路径无效。")
        }
        let plist: [String: Any] = ["Label": label,
            "ProgramArguments": [brokerExecutable.path, "--root", root, "--label", label],
            "MachServices": [clientService: true, hostService: true],
            "ProcessType": "Interactive", "RunAtLoad": false, "KeepAlive": false]
        let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
        try writeConfiguration(data, directoryFD: directoryFD, name: file.lastPathComponent)
        let domain = "gui/\(getuid())"
        // An existing job is not removed or restarted: active commands/older GUI may still own it.
        if try Self.launchctl(["print", domain + "/" + label]) == 0 { return }
        let code = try Self.launchctl(["bootstrap", domain, file.path])
        if code != 0, try Self.launchctl(["print", domain + "/" + label]) != 0 {
            throw RPCFailure("无法注册Cider命令通道（\(code)）。")
        }
    }

    /// Only for explicitly owned session cleanup; normal clients never unload an active service.
    public func unregister() throws {
        let code = try Self.launchctl(["bootout", "gui/\(getuid())/" + label])
        if code != 0, try Self.launchctl(["print", "gui/\(getuid())/" + label]) == 0 {
            throw RPCFailure("Cider命令通道尚未退出。")
        }
    }

    private func secureDirectory(_ url: URL) throws -> Int32 {
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        let fd = open(url.path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)
        guard fd >= 0 else { throw RPCFailure("XPC目录类型无效。") }
        var info = stat()
        guard fstat(fd, &info) == 0, info.st_mode & S_IFMT == S_IFDIR,
              info.st_uid == getuid(), fchmod(fd, 0o700) == 0 else {
            close(fd); throw RPCFailure("XPC目录所有者或权限无效。")
        }
        return fd
    }
    private func writeConfiguration(_ data: Data, directoryFD: Int32, name: String) throws {
        let temporary = "." + UUID().uuidString + ".plist"
        let fd = openat(directoryFD, temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode_t(0o600))
        guard fd >= 0 else { throw RPCFailure("无法写入XPC配置。") }
        defer { close(fd); unlinkat(directoryFD, temporary, 0) }
        try data.withUnsafeBytes { bytes in
            var offset = 0
            while offset < bytes.count {
                let count = write(fd, bytes.baseAddress!.advanced(by: offset), bytes.count - offset)
                if count < 0, errno == EINTR { continue }
                guard count > 0 else { throw RPCFailure("XPC配置写入未完成。") }
                offset += count
            }
        }
        guard fsync(fd) == 0, renameat(directoryFD, temporary, directoryFD, name) == 0 else {
            throw RPCFailure("XPC配置发布未确认，未注册。")
        }
    }
    private static func launchctl(_ args: [String]) throws -> Int32 {
        let p = Process(); p.executableURL = URL(fileURLWithPath: "/bin/launchctl"); p.arguments = args
        p.standardInput = FileHandle.nullDevice; p.standardOutput = FileHandle.nullDevice; p.standardError = FileHandle.nullDevice
        p.environment = ["PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "HOME": NSHomeDirectory()]
        try p.run()
        let deadline = ProcessInfo.processInfo.systemUptime + 5
        while p.isRunning, ProcessInfo.processInfo.systemUptime < deadline { Thread.sleep(forTimeInterval: 0.02) }
        if p.isRunning {
            p.terminate()
            let termDeadline = ProcessInfo.processInfo.systemUptime + 0.5
            while p.isRunning, ProcessInfo.processInfo.systemUptime < termDeadline { Thread.sleep(forTimeInterval: 0.01) }
            if p.isRunning { kill(p.processIdentifier, SIGKILL) }
            let killDeadline = ProcessInfo.processInfo.systemUptime + 0.5
            while p.isRunning, ProcessInfo.processInfo.systemUptime < killDeadline { Thread.sleep(forTimeInterval: 0.01) }
            throw RPCFailure(p.isRunning ? "launchctl超时且退出未确认，未重复注册。" : "注册Cider命令通道超时。")
        }
        p.waitUntilExit(); return p.terminationStatus
    }
}
