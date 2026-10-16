import Foundation
import Darwin

public enum RPCWire {
    public static let version = 1
    public static let maxRequestBytes = 131_072
    public static let chunkBytes = 8_192

    public static func encode<T: Encodable>(_ value: T) throws -> Data { try JSONEncoder().encode(value) }
    public static func decode<T: Decodable>(_ type: T.Type, _ data: Data, limit: Int = maxRequestBytes) throws -> T {
        guard data.count <= limit else { throw RPCFailure("请求数据超过限制。") }
        return try JSONDecoder().decode(type, from: data)
    }
}

public struct RPCFailure: Error, LocalizedError, Sendable {
    public let message: String
    public init(_ message: String) { self.message = message }
    public var errorDescription: String? { message }
}

public struct RPCRequest: Codable, Sendable {
    public var version: Int = RPCWire.version
    public var id: UUID
    public var arguments: [String]
    public var workingDirectory: String
    public var appSupport: String
    public var dataOverride: String?
    public init(arguments: [String], workingDirectory: String, appSupport: String, dataOverride: String? = nil, id: UUID = UUID()) {
        self.id = id; self.arguments = arguments; self.workingDirectory = workingDirectory
        self.appSupport = appSupport; self.dataOverride = dataOverride
    }
    public func validate(root: String) throws {
        guard version == RPCWire.version, arguments.count <= 1_024,
              arguments.reduce(0, { $0 + $1.utf8.count }) <= 65_536,
              arguments.allSatisfy({ !$0.contains("\0") }),
              workingDirectory.hasPrefix("/"), !workingDirectory.contains("\0"),
              appSupport == root, dataOverride.map({ $0.hasPrefix("/") && !$0.contains("\0") }) ?? true else {
            throw RPCFailure("命令参数或资料目录不匹配，未执行。")
        }
    }
}

public struct RPCHello: Codable, Sendable {
    public var version = RPCWire.version
    public var root: String
    public var hostPID: Int32?
    public init(root: String, hostPID: Int32?) { self.root = root; self.hostPID = hostPID }
}

public struct RPCHostDescription: Codable, Sendable {
    public var version = RPCWire.version
    public var root: String
    public init(root: String) { self.root = root }
}

public struct RPCEvent: Codable, Sendable {
    public let id: UUID
    public let sequence: UInt64
    public let bytes: Data
    public let stderr: Bool
    public init(id: UUID, sequence: UInt64, bytes: Data, stderr: Bool) {
        self.id = id; self.sequence = sequence; self.bytes = bytes; self.stderr = stderr
    }
}

public struct RPCResponse: Codable, Sendable {
    public var version = RPCWire.version
    public var id: UUID
    public var state: String
    public var exitCode: Int32
    public var message: String?
    public var sequenceCount: UInt64
    public var executorPID: Int32?
    public var outputIncomplete: Bool = false
    public init(id: UUID, state: String, exitCode: Int32, message: String? = nil,
                sequenceCount: UInt64 = 0, executorPID: Int32? = nil) {
        self.id = id; self.state = state; self.exitCode = exitCode; self.message = message
        self.sequenceCount = sequenceCount; self.executorPID = executorPID
    }
    public static func rejected(_ id: UUID, _ message: String) -> RPCResponse {
        RPCResponse(id: id, state: "rejected", exitCode: 64, message: message)
    }
    public static func unknown(_ id: UUID) -> RPCResponse {
        RPCResponse(id: id, state: "unknown", exitCode: 75,
                    message: "连接已中断，命令结果尚未确认。请在Cider检查运行状态；本次命令不会自动重试。")
    }
}

@objc public protocol RPCClientProtocol {
    func hello(withReply reply: @escaping @Sendable (Data) -> Void)
    func execute(_ request: Data, withReply reply: @escaping @Sendable (Data) -> Void)
}
@objc public protocol RPCConsoleProtocol {
    func output(_ event: Data, withReply reply: @escaping @Sendable () -> Void)
}
@objc public protocol RPCRegistrationProtocol {
    func registerHost(_ description: Data, withReply reply: @escaping @Sendable (Data) -> Void)
    func emit(_ event: Data, withReply reply: @escaping @Sendable (Bool) -> Void)
}
@objc public protocol RPCExecutionProtocol {
    func execute(_ request: Data, withReply reply: @escaping @Sendable (Data) -> Void)
}

/// Resolve the existing ancestor with POSIX realpath, then append absent components.
/// Foundation's resolvingSymlinksInPath can change /private/var aliases when a missing
/// leaf is created, which would otherwise change a cold-start service's identity.
public func canonicalPath(_ url: URL) -> String {
    let path = url.standardizedFileURL.path
    var probe = path
    var suffix: [String] = []
    while true {
        if let resolved = realpath(probe, nil) {
            defer { free(resolved) }
            var result = String(cString: resolved)
            for component in suffix.reversed() { result += (result.hasSuffix("/") ? "" : "/") + component }
            return result
        }
        let parent = (probe as NSString).deletingLastPathComponent
        guard !parent.isEmpty, parent != probe else { return path }
        suffix.append((probe as NSString).lastPathComponent); probe = parent
    }
}

final class RPCOnce<T>: @unchecked Sendable {
    private let lock = NSLock()
    private var reply: (@Sendable (T) -> Void)?
    init(_ reply: @escaping @Sendable (T) -> Void) { self.reply = reply }
    func finish(_ value: T) {
        lock.lock(); let callback = reply; reply = nil; lock.unlock()
        callback?(value)
    }
}
