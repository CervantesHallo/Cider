import CiderBottle
import CiderCore
import Foundation

/// Product GUI and CLI commands execute in the same GUI process. Track their
/// installation work together; an explicit Stop differs from terminal detachment.
enum InstallationActivity {
    @TaskLocal static var current: Ticket?
    private static let registry = Registry()

    static var isCancelled: Bool { Task.isCancelled || (current.map { !$0.isActive } ?? false) }
    static func checkCancellation() throws { try Task.checkCancellation(); try current?.check() }
    static func scope(_ bottle: Bottle) -> String {
        bottle.config.id + "\n" + bottle.directory.standardizedFileURL.resolvingSymlinksInPath().path
    }

    static func perform<T>(in bottle: Bottle, onStop: @escaping @Sendable () -> Void, _ work: () throws -> T) throws -> T {
        if let inherited = current, inherited.scope == scope(bottle) {
            // GUI ownership already spans the whole install/refresh/launch workflow.
            // Do not register a second ticket after Stop has barred new work.
            let listener = inherited.addStopListener(onStop)
            defer { inherited.removeStopListener(listener) }
            try checkCancellation()
            let value = try work()
            try checkCancellation()
            return value
        }
        let ticket = try registry.begin(scope: scope(bottle), onStop: onStop)
        let result = Swift.Result {
            try $current.withValue(ticket) {
                try checkCancellation()
                let value = try work()
                try checkCancellation()
                return value
            }
        }
        switch result {
        case .success(let value):
            if let error = registry.finish(ticket, error: nil) { throw error }
            return value
        case .failure(let error): throw registry.finish(ticket, error: error) ?? error
        }
    }

    static func begin(in bottle: Bottle) throws -> Ticket { try registry.begin(scope: scope(bottle), onStop: {}) }
    static func finish(_ ticket: Ticket, error: Error?) -> Error? { registry.finish(ticket, error: error) }

    static func stopping<T>(in bottle: Bottle, timeout: TimeInterval, _ stopWindows: () throws -> T) throws -> T {
        let key = scope(bottle)
        guard timeout.isFinite, timeout > 0, current?.scope != key else {
            throw CiderError.invalid("停止等待无效，或安装任务不能等待它自身结束。")
        }
        let lease = registry.beginStop(scope: key)
        defer { registry.endStop(scope: key, id: lease.id) }
        lease.tickets.forEach { $0.cancel() }
        let result = Swift.Result(catching: stopWindows)
        let deadline = Date().addingTimeInterval(timeout)
        var joinError: Error?
        for ticket in lease.tickets {
            do { try ticket.wait(until: deadline) } catch { if joinError == nil { joinError = error } }
        }
        let value = try result.get()
        if let joinError { throw joinError }
        return value
    }

    final class Ticket: @unchecked Sendable {
        let id = UUID()
        let scope: String
        private let condition = NSCondition()
        private var cancelled = false
        private var completed = false
        private var error: Error?
        private var listeners: [UUID: @Sendable () -> Void] = [:]
        init(scope: String, onStop: @escaping @Sendable () -> Void) { self.scope = scope; listeners[UUID()] = onStop }
        var isCancelled: Bool { condition.lock(); defer { condition.unlock() }; return cancelled }
        var isActive: Bool { condition.lock(); defer { condition.unlock() }; return !cancelled && !completed }
        func check() throws {
            condition.lock(); let cancelled = cancelled; let completed = completed; condition.unlock()
            if cancelled { throw CancellationError() }
            if completed { throw CiderError.invalid("安装活动已结束，不能继续启动程序。") }
        }
        func addStopListener(_ listener: @escaping @Sendable () -> Void) -> UUID {
            let id = UUID()
            condition.lock(); let call = cancelled && !completed
            if !cancelled && !completed { listeners[id] = listener }
            condition.unlock()
            if call { listener() }
            return id
        }
        func removeStopListener(_ id: UUID) { condition.lock(); listeners.removeValue(forKey: id); condition.unlock() }
        func cancel() {
            condition.lock()
            guard !cancelled, !completed else { condition.unlock(); return }
            cancelled = true; let callbacks = Array(listeners.values); listeners.removeAll()
            condition.broadcast(); condition.unlock()
            callbacks.forEach { $0() }
        }
        func finish(error: Error?) -> Error? {
            condition.lock(); defer { condition.unlock() }
            guard !completed else { return self.error }
            // This lock orders Stop against successful completion. If cancellation
            // won, no caller can publish success in the check-to-finish interval.
            self.error = error ?? (cancelled ? CancellationError() : nil)
            completed = true; listeners.removeAll(); condition.broadcast()
            return self.error
        }
        func wait(until deadline: Date) throws {
            condition.lock(); defer { condition.unlock() }
            while !completed {
                guard condition.wait(until: deadline) || completed else {
                    throw CiderError.invalid("安装任务尚未退出，停止未完成。")
                }
            }
            if let error, !(error is CancellationError) { throw error }
        }
    }

    private final class Registry: @unchecked Sendable {
        struct Slot { var tickets: [UUID: Ticket] = [:]; var stops = Set<UUID>() }
        private let lock = NSLock()
        private var slots: [String: Slot] = [:]
        func begin(scope: String, onStop: @escaping @Sendable () -> Void) throws -> Ticket {
            lock.lock(); defer { lock.unlock() }
            var slot = slots[scope] ?? Slot()
            guard slot.stops.isEmpty, !slot.tickets.values.contains(where: { $0.isCancelled }) else {
                throw CiderError.invalid("瓶子正在停止，或上一次取消尚未完成；安装未开始。")
            }
            let ticket = Ticket(scope: scope, onStop: onStop)
            slot.tickets[ticket.id] = ticket; slots[scope] = slot
            return ticket
        }
        func finish(_ ticket: Ticket, error: Error?) -> Error? {
            let outcome = ticket.finish(error: error)
            lock.lock(); defer { lock.unlock() }
            guard var slot = slots[ticket.scope] else { return outcome }
            slot.tickets.removeValue(forKey: ticket.id)
            if slot.tickets.isEmpty && slot.stops.isEmpty { slots.removeValue(forKey: ticket.scope) }
            else { slots[ticket.scope] = slot }
            return outcome
        }
        func beginStop(scope: String) -> (id: UUID, tickets: [Ticket]) {
            lock.lock(); defer { lock.unlock() }
            var slot = slots[scope] ?? Slot(); let id = UUID()
            slot.stops.insert(id); slots[scope] = slot
            return (id, Array(slot.tickets.values))
        }
        func endStop(scope: String, id: UUID) {
            lock.lock(); defer { lock.unlock() }
            guard var slot = slots[scope] else { return }
            slot.stops.remove(id)
            if slot.tickets.isEmpty && slot.stops.isEmpty { slots.removeValue(forKey: scope) }
            else { slots[scope] = slot }
        }
    }
}

extension RecipeInstaller {
    /// Retain across GUI refresh and auto-launch. Complete it only after the launch
    /// task ends, and pass its cancellation check into that task's Wine runner.
    public final class Activity: @unchecked Sendable {
        private let ticket: InstallationActivity.Ticket
        fileprivate init(_ ticket: InstallationActivity.Ticket) { self.ticket = ticket }
        public func checkCancellation() throws {
            try Task.checkCancellation(); try ticket.check()
        }
        public func performing<T>(_ work: () throws -> T) throws -> T {
            try InstallationActivity.$current.withValue(ticket) {
                try checkCancellation()
                let value = try work()
                try checkCancellation()
                return value
            }
        }
        public func finish(error: Error? = nil) throws {
            if let outcome = InstallationActivity.finish(ticket, error: error) { throw outcome }
        }
        deinit { _ = InstallationActivity.finish(ticket, error: CancellationError()) }
    }

    public static func beginActivity(in bottle: Bottle) throws -> Activity { Activity(try InstallationActivity.begin(in: bottle)) }

    /// Cancel both GUI and CLI installations, stop Windows while new installations
    /// are barred, then confirm the captured workers exited before reporting success.
    public static func stoppingInstallations<T>(in bottle: Bottle, timeout: TimeInterval = 10,
                                                _ stopWindows: () throws -> T) throws -> T {
        try InstallationActivity.stopping(in: bottle, timeout: timeout, stopWindows)
    }
}
