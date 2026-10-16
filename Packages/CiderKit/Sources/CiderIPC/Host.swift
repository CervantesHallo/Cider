import Foundation

public protocol RPCCommandExecuting: Sendable {
    /// Called off the main thread, in the GUI process. No process-wide cwd/environment changes.
    func execute(_ request: RPCRequest, output: @escaping @Sendable (String, Bool) -> Void) -> Int32
}

public final class RPCGUIHost: NSObject, RPCExecutionProtocol, @unchecked Sendable {
    private let location: RPCLocation
    private let identity: RPCIdentity
    private let executor: any RPCCommandExecuting
    private let dataOverride: String?
    private let queue: OperationQueue
    private let controlQueue: OperationQueue
    private let lock = NSLock()
    private var connection: NSXPCConnection?
    private var stopped = false
    private var connecting = false
    private var recoveryScheduled = false
    private var acknowledgements: [UUID: (ObjectIdentifier, RPCWaitBox<Bool>)] = [:]
    private var admitted = Set<UUID>()
    public init(location: RPCLocation, identity: RPCIdentity, dataOverride: String?, executor: any RPCCommandExecuting) {
        self.location = location; self.identity = identity; self.dataOverride = dataOverride; self.executor = executor
        queue = OperationQueue(); queue.maxConcurrentOperationCount = 8; queue.qualityOfService = .userInitiated
        controlQueue = OperationQueue(); controlQueue.maxConcurrentOperationCount = 2; controlQueue.qualityOfService = .userInitiated
        super.init()
    }
    /// Starts the first registration and bounded recovery without blocking the GUI thread.
    public func start() { reconnect(attempt: 0) }
    public func connect(timeout: TimeInterval = 5) throws {
        lock.lock()
        guard !stopped, !connecting else { lock.unlock(); throw RPCFailure("命令通道已关闭或正在连接。") }
        connecting = true; lock.unlock()
        let c = NSXPCConnection(machServiceName: location.hostService)
        do {
            c.remoteObjectInterface = NSXPCInterface(with: RPCRegistrationProtocol.self)
            c.exportedInterface = NSXPCInterface(with: RPCExecutionProtocol.self); c.exportedObject = self
            c.setCodeSigningRequirement(identity.broker)
            c.interruptionHandler = { [weak c] in c?.invalidate() }
            c.invalidationHandler = { [weak self, weak c] in if let c { self?.disconnected(c) } }
            lock.lock()
            guard !stopped else { lock.unlock(); throw RPCFailure("命令通道已关闭。") }
            connection = c; lock.unlock()
            c.activate()
            let result = RPCWaitBox<Data>()
            let proxy = c.remoteObjectProxyWithErrorHandler { error in result.fail(error) }
            guard let registrar = proxy as? RPCRegistrationProtocol else { throw RPCFailure("命令通道不可用。") }
            registrar.registerHost(try RPCWire.encode(RPCHostDescription(root: location.root))) { result.succeed($0) }
            let hello = try RPCWire.decode(RPCHello.self, result.wait(timeout: timeout))
            guard hello.version == RPCWire.version, hello.root == location.root, hello.hostPID == getpid() else {
                throw RPCFailure("已有另一Cider实例或资料目录不匹配，未接管命令。")
            }
            // The ready transition and disconnect decision share one lock. A disconnect before
            // this point throws to the initial/recovery caller; one after it schedules recovery.
            lock.lock()
            guard connection === c, !stopped else { lock.unlock(); throw RPCFailure("命令通道在登记时中断。") }
            connecting = false; lock.unlock()
        } catch {
            lock.lock(); connecting = false; if connection === c { connection = nil }; lock.unlock()
            c.invalidate(); throw error
        }
    }
    public func invalidate() {
        lock.lock(); stopped = true; let c = connection; connection = nil
        let waiting = acknowledgements.values.map { $0.1 }; acknowledgements.removeAll(); lock.unlock()
        waiting.forEach { $0.succeed(false) }; c?.invalidate()
    }
    private func disconnected(_ c: NSXPCConnection) {
        lock.lock()
        let owned = connection === c
        if owned { connection = nil }
        let keys = acknowledgements.filter { $0.value.0 == ObjectIdentifier(c) }.map(\.key)
        let waiting = keys.compactMap { acknowledgements.removeValue(forKey: $0)?.1 }
        let recover = owned && !stopped && !connecting
        lock.unlock()
        waiting.forEach { $0.succeed(false) }
        if recover { reconnect(attempt: 0) }
    }
    private func reconnect(attempt: Int) {
        let delays: [Double] = [0.25, 1, 3, 8]
        guard attempt < delays.count else { NSLog("Cider command host: reconnect exhausted; restart the GUI before submitting new commands."); return }
        lock.lock()
        guard !stopped, !connecting, connection == nil, !recoveryScheduled else { lock.unlock(); return }
        recoveryScheduled = true; lock.unlock()
        DispatchQueue.global().asyncAfter(deadline: .now() + delays[attempt]) { [weak self] in
            guard let self else { return }
            self.lock.lock(); self.recoveryScheduled = false
            let needed = !self.stopped && self.connection == nil && !self.connecting; self.lock.unlock()
            guard needed else { return }
            do { try self.connect(timeout: 2) }
            catch { self.reconnect(attempt: attempt + 1) }
        }
    }
    public func execute(_ data: Data, withReply reply: @escaping @Sendable (Data) -> Void) {
        let request: RPCRequest
        do {
            request = try RPCWire.decode(RPCRequest.self, data); try request.validate(root: location.root)
            if let requested = request.dataOverride, requested != dataOverride {
                throw RPCFailure("Cider使用的数据覆盖与命令行不同，请先核对运行实例。")
            }
        } catch { reply(Data()); return }
        lock.lock()
        guard !stopped, admitted.count < 65_536, admitted.insert(request.id).inserted else {
            lock.unlock()
            var rejected = RPCResponse.rejected(request.id, "重复请求或本次会话的请求记录已满，未执行。")
            rejected.executorPID = getpid()
            reply((try? RPCWire.encode(rejected)) ?? Data()); return
        }
        lock.unlock()
        let once = RPCOnce(reply)
        // Waiting for an entire bottle must not consume the workers needed by `kill`.
        let selectedQueue = request.arguments.first == "kill" ? controlQueue : queue
        selectedQueue.addOperation { [weak self] in
            guard let self else { once.finish(Data()); return }
            let output = HostOutput(id: request.id) { [weak self] event in self?.send(event) ?? false }
            let code = self.executor.execute(request) { text, stderr in output.append(text, stderr: stderr) }
            let final = output.finish()
            var response = RPCResponse(id: request.id, state: "completed", exitCode: code,
                                       sequenceCount: final.count, executorPID: getpid())
            response.outputIncomplete = final.incomplete
            once.finish((try? RPCWire.encode(response)) ?? Data())
        }
    }
    private func send(_ event: RPCEvent) -> Bool {
        guard let bytes = try? RPCWire.encode(event) else { return false }
        let ack = RPCWaitBox<Bool>(), key = UUID()
        lock.lock()
        guard let c = connection else { lock.unlock(); return false }
        acknowledgements[key] = (ObjectIdentifier(c), ack); lock.unlock()
        defer { lock.lock(); acknowledgements.removeValue(forKey: key); lock.unlock() }
        let proxy = c.remoteObjectProxyWithErrorHandler { ack.fail($0) }
        guard let registrar = proxy as? RPCRegistrationProtocol else { return false }
        registrar.emit(bytes) { ack.succeed($0) }
        // A live terminal/pipe can legitimately pause. Only ACK, an actual disconnect or
        // explicit host shutdown ends this wait; it never holds the host connection lock.
        return (try? ack.wait(timeout: nil)) ?? false
    }
}

/// A bounded in-flight stream, without a lifetime output cap. Acknowledgements provide
/// backpressure; no process-wide stdout redirection or per-frame work is introduced.
private final class HostOutput: @unchecked Sendable {
    private let lock = NSRecursiveLock()
    private let id: UUID
    private let send: @Sendable (RPCEvent) -> Bool
    private var buffer = Data()
    private var stderr = false
    private var count: UInt64 = 0
    private var incomplete = false
    private var closed = false
    private var flushScheduled = false
    init(id: UUID, send: @escaping @Sendable (RPCEvent) -> Bool) { self.id = id; self.send = send }
    func append(_ text: String, stderr: Bool) {
        lock.lock(); defer { lock.unlock() }
        guard !closed, !incomplete else { return }
        let data = Data(text.utf8)
        if self.stderr != stderr { flush(); self.stderr = stderr }
        var offset = 0
        while offset < data.count, !incomplete {
            let size = min(RPCWire.chunkBytes - buffer.count, data.count - offset)
            buffer.append(data[offset..<(offset + size)]); offset += size
            if buffer.count == RPCWire.chunkBytes { flush() }
        }
        if !buffer.isEmpty, !flushScheduled {
            flushScheduled = true
            DispatchQueue.global().asyncAfter(deadline: .now() + 0.05) { [weak self] in
                guard let self else { return }
                self.lock.lock(); defer { self.lock.unlock() }
                self.flushScheduled = false
                if !self.closed { self.flush() }
            }
        }
    }
    func finish() -> (count: UInt64, incomplete: Bool) {
        lock.lock(); defer { lock.unlock() }
        if !closed { flush(); closed = true }
        return (count, incomplete)
    }
    private func flush() {
        guard !buffer.isEmpty, !incomplete, count < UInt64.max else { return }
        count += 1
        let data = buffer; buffer = Data()
        if !send(RPCEvent(id: id, sequence: count, bytes: data, stderr: stderr)) { incomplete = true }
    }
}

final class RPCWaitBox<T: Sendable>: @unchecked Sendable {
    private let semaphore = DispatchSemaphore(value: 0)
    private let lock = NSLock()
    private var value: Result<T, Error>?
    func succeed(_ value: T) { finish(.success(value)) }
    func fail(_ error: Error) { finish(.failure(error)) }
    private func finish(_ result: Result<T, Error>) {
        lock.lock(); guard value == nil else { lock.unlock(); return }
        value = result; lock.unlock(); semaphore.signal()
    }
    func wait(timeout: TimeInterval?) throws -> T {
        let status = timeout.map { semaphore.wait(timeout: .now() + $0) } ?? { semaphore.wait(); return .success }()
        guard status == .success else { throw RPCFailure("等待Cider命令通道超时。") }
        lock.lock(); let result = value; lock.unlock()
        guard let result else { throw RPCFailure("命令通道没有返回结果。") }
        return try result.get()
    }
}
