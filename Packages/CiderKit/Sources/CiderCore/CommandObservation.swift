import Foundation

/// Lifetime of a command's external observer. Detaching never terminates a launched program.
public final class CommandObservation: @unchecked Sendable {
    private let lock = NSLock()
    private var detached = false
    private var listeners: [UUID: @Sendable () -> Void] = [:]

    public init() {}
    public var isDetached: Bool { lock.lock(); defer { lock.unlock() }; return detached }
    public func detach() {
        lock.lock()
        guard !detached else { lock.unlock(); return }
        detached = true; let callbacks = Array(listeners.values); listeners.removeAll(); lock.unlock()
        callbacks.forEach { $0() }
    }
    @discardableResult
    public func onDetach(_ listener: @escaping @Sendable () -> Void) -> UUID {
        let id = UUID()
        lock.lock(); let alreadyDetached = detached
        if !alreadyDetached { listeners[id] = listener }
        lock.unlock()
        if alreadyDetached { listener() }
        return id
    }
    public func removeDetachListener(_ id: UUID) { lock.lock(); listeners.removeValue(forKey: id); lock.unlock() }
    public func check() throws { if isDetached { throw CommandObserverDetached() } }
}

public struct CommandObserverDetached: Error, LocalizedError, Sendable {
    public init() {}
    public var errorDescription: String? { "命令行等待已断开；已经启动的程序继续运行。" }
}
