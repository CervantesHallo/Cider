import Darwin
import Foundation

/// Boundaries for imported metadata and cooperative filesystem transactions.
public enum FileSafety {
    public static func component(_ value: String) throws -> String {
        guard !value.isEmpty, value.utf8.count <= 128, !value.hasPrefix("."),
              value.unicodeScalars.allSatisfy({ $0.isASCII && (CharacterSet.alphanumerics.contains($0) || "._-".unicodeScalars.contains($0)) }) else {
            throw CiderError.invalid("无效的目录标识：\(value)")
        }
        return value
    }

    public static func relativePath(_ value: String) throws -> String {
        let parts = value.split(separator: "/", omittingEmptySubsequences: false)
        guard !value.isEmpty, !value.contains("\\"), !value.contains("\0"),
              !value.contains("\n"), !value.contains("\r"),
              parts.allSatisfy({ !$0.isEmpty && $0 != "." && $0 != ".." }) else {
            throw CiderError.invalid("无效的相对路径：\(value)")
        }
        return value
    }

    /// Resolve existing ancestors too. Game-file mutations additionally reject all symlink components.
    /// This guards metadata and cooperating operations; it is not isolation from hostile native processes.
    public static func child(_ relative: String, in root: URL, rejectSymlinks: Bool = false) throws -> URL {
        let path = try relativePath(relative)
        if rejectSymlinks, (try? FileManager.default.attributesOfItem(atPath: root.path)[.type] as? FileAttributeType) == .typeSymbolicLink {
            throw CiderError.invalid("目标根目录不能是符号链接。")
        }
        let base = root.standardizedFileURL.resolvingSymlinksInPath()
        let result = base.appendingPathComponent(path).standardizedFileURL
        let resolved = result.resolvingSymlinksInPath()
        guard resolved.path.hasPrefix(base.path + "/") else { throw CiderError.invalid("路径越出目标目录：\(relative)") }
        if rejectSymlinks {
            var cursor = base
            for part in path.split(separator: "/") {
                cursor.appendPathComponent(String(part))
                if (try? FileManager.default.attributesOfItem(atPath: cursor.path)[.type] as? FileAttributeType) == .typeSymbolicLink {
                    throw CiderError.invalid("不能通过符号链接修改文件：\(relative)")
                }
            }
        }
        return result
    }

    public static func exists(_ url: URL) -> Bool {
        (try? FileManager.default.attributesOfItem(atPath: url.path)) != nil
    }

    /// mkdir is exclusive. Only a successful caller owns and may clean up this directory.
    public static func reserveDirectory(in root: URL, name: () -> String) throws -> URL {
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        for _ in 0..<16 {
            let id = try component(name())
            let directory = try child(id, in: root, rejectSymlinks: true)
            if mkdir(directory.path, 0o700) == 0 { return directory }
            guard errno == EEXIST else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        }
        throw CiderError.invalid("无法创建独立目录，请重试。")
    }

    /// Same-filesystem atomic commit. Existing content remains recoverable in staging after a swap.
    @discardableResult
    public static func commit(_ staging: URL, to destination: URL) throws -> URL? {
        if exists(destination) {
            guard renamex_np(staging.path, destination.path, UInt32(RENAME_SWAP)) == 0 else {
                throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
            }
            return staging
        }
        guard renamex_np(staging.path, destination.path, UInt32(RENAME_EXCL)) == 0 else {
            throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
        }
        return nil
    }

    public static func validateArchiveListing(_ names: String) throws {
        for name in names.split(separator: "\n") {
            var path = String(name)
            while path.hasPrefix("./") { path.removeFirst(2) }
            if path.hasSuffix("/") { path.removeLast() }
            if path.isEmpty { continue }
            _ = try relativePath(path)
        }
    }
}

/// Recursive within a synchronous thread, serialized between threads and independent GUI/CLI processes.
public enum FileOperationLock {
    private final class Registry: @unchecked Sendable {
        let guardLock = NSLock()
        var locks: [String: NSRecursiveLock] = [:]
        func lock(for key: String) -> NSRecursiveLock {
            guardLock.lock(); defer { guardLock.unlock() }
            if let lock = locks[key] { return lock }
            let lock = NSRecursiveLock(); locks[key] = lock; return lock
        }
    }
    private static let registry = Registry()

    public static func withLock<T>(at url: URL, timeout: TimeInterval = 30, _ body: () throws -> T) throws -> T {
        let key = url.standardizedFileURL.resolvingSymlinksInPath().path
        let local = registry.lock(for: key)
        guard local.lock(before: Date().addingTimeInterval(timeout)) else { throw CiderError.invalid("另一个操作正在处理此数据，请稍后重试。") }
        defer { local.unlock() }
        let threadKey = "org.cider.operation-lock:" + key
        if Thread.current.threadDictionary[threadKey] != nil { return try body() }
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        let descriptor = open(url.path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0o600)
        guard descriptor >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        defer { close(descriptor) }
        let deadline = ProcessInfo.processInfo.systemUptime + timeout
        while flock(descriptor, LOCK_EX | LOCK_NB) != 0 {
            guard errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
            guard ProcessInfo.processInfo.systemUptime < deadline else { throw CiderError.invalid("另一个操作正在处理此数据，请稍后重试。") }
            usleep(20_000)
        }
        defer { _ = flock(descriptor, LOCK_UN) }
        Thread.current.threadDictionary[threadKey] = true
        defer { Thread.current.threadDictionary.removeObject(forKey: threadKey) }
        return try body()
    }
}
