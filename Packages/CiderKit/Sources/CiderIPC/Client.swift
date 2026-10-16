import Foundation

public final class RPCClient: @unchecked Sendable {
    private let location: RPCLocation
    private let identity: RPCIdentity
    public init(location: RPCLocation, identity: RPCIdentity) { self.location = location; self.identity = identity }
    public func hello(timeout: TimeInterval = 3) throws -> RPCHello {
        let c = connection(); c.activate(); defer { c.invalidate() }
        let result = RPCWaitBox<Data>()
        guard let proxy = c.remoteObjectProxyWithErrorHandler({ result.fail($0) }) as? RPCClientProtocol else {
            throw RPCFailure("Cider命令通道不可用。")
        }
        proxy.hello { result.succeed($0) }
        let value = try RPCWire.decode(RPCHello.self, result.wait(timeout: timeout))
        guard value.version == RPCWire.version, value.root == location.root else { throw RPCFailure("Cider命令通道版本或资料目录不匹配。") }
        return value
    }
    /// Once sent, a disconnect is unknown outcome. Never retry or fall back to local execution.
    public func execute(_ request: RPCRequest, output: @escaping @Sendable (Data, Bool) throws -> Void) throws -> RPCResponse {
        try request.validate(root: location.root)
        let console = ClientConsole(id: request.id, output: output)
        let c = connection(console: console); defer { c.invalidate() }
        let result = RPCWaitBox<Data>()
        let unknown = RPCResponse.unknown(request.id).message ?? "命令结果未知，未重试。"
        c.invalidationHandler = { result.fail(RPCFailure(unknown)) }
        c.activate()
        guard let proxy = c.remoteObjectProxyWithErrorHandler({ _ in result.fail(RPCFailure(unknown)) }) as? RPCClientProtocol else {
            throw RPCFailure("Cider命令通道不可用。")
        }
        proxy.execute(try RPCWire.encode(request)) { result.succeed($0) }
        let response = try RPCWire.decode(RPCResponse.self, result.wait(timeout: nil))
        guard response.version == RPCWire.version, response.id == request.id,
              ["completed", "rejected", "unknown"].contains(response.state) else { throw RPCFailure("Cider命令结果无效，未重试。") }
        var actual = response
        if console.failed { actual.outputIncomplete = true }
        return actual
    }
    private func connection(console: ClientConsole? = nil) -> NSXPCConnection {
        let c = NSXPCConnection(machServiceName: location.clientService)
        c.remoteObjectInterface = NSXPCInterface(with: RPCClientProtocol.self)
        if let console { c.exportedInterface = NSXPCInterface(with: RPCConsoleProtocol.self); c.exportedObject = console }
        c.setCodeSigningRequirement(identity.broker)
        c.interruptionHandler = { [weak c] in c?.invalidate() }
        return c
    }
}

private final class ClientConsole: NSObject, RPCConsoleProtocol, @unchecked Sendable {
    private let id: UUID
    private let sink: @Sendable (Data, Bool) throws -> Void
    private let lock = NSLock()
    private var expected: UInt64 = 1
    private var pending: [UInt64: (RPCEvent, @Sendable () -> Void)] = [:]
    private var writeFailed = false
    var failed: Bool { lock.lock(); defer { lock.unlock() }; return writeFailed }
    init(id: UUID, output: @escaping @Sendable (Data, Bool) throws -> Void) { self.id = id; sink = output }
    func output(_ data: Data, withReply reply: @escaping @Sendable () -> Void) {
        guard let event = try? RPCWire.decode(RPCEvent.self, data, limit: 24_576), event.id == id,
              event.sequence > 0, event.sequence < UInt64.max, event.bytes.count <= RPCWire.chunkBytes else { reply(); return }
        lock.lock()
        guard event.sequence >= expected, pending.count < 8, pending[event.sequence] == nil else { lock.unlock(); reply(); return }
        pending[event.sequence] = (event, reply)
        var acknowledgements: [@Sendable () -> Void] = []
        while let (next, ack) = pending.removeValue(forKey: expected) {
            do { try sink(next.bytes, next.stderr) } catch { writeFailed = true }
            acknowledgements.append(ack); expected += 1
        }
        lock.unlock(); acknowledgements.forEach { $0() }
    }
}
