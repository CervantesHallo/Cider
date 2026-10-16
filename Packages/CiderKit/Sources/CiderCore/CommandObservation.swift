import Foundation

/// Lifetime of a command's external observer. Detaching never terminates a launched program.
public final class CommandObservation: @unchecked Sendable {
    private let lock = NSLock()
    private var detached = false

    public init() {}
    public var isDetached: Bool { lock.lock(); defer { lock.unlock() }; return detached }
    public func detach() { lock.lock(); detached = true; lock.unlock() }
    public func check() throws { if isDetached { throw CommandObserverDetached() } }
}

public struct CommandObserverDetached: Error, LocalizedError, Sendable {
    public init() {}
    public var errorDescription: String? { "命令行等待已断开；已经启动的程序继续运行。" }
}
