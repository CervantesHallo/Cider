import Darwin
import Foundation

/// Routes authenticated command requests to the GUI. This process never runs a command or Wine.
public final class RPCBroker: @unchecked Sendable {
    private let state: BrokerState
    private let delegates: [BrokerListener]
    private let listeners: [NSXPCListener]
    public init(location: RPCLocation, identity: RPCIdentity, onIdle: @escaping @Sendable () -> Void = {}) {
        let state = BrokerState(root: location.root, onIdle: onIdle)
        self.state = state
        delegates = [BrokerListener(state: state, role: .client, requirement: identity.client),
                     BrokerListener(state: state, role: .host, requirement: identity.gui)]
        listeners = [NSXPCListener(machServiceName: location.clientService), NSXPCListener(machServiceName: location.hostService)]
        for (listener, delegate) in zip(listeners, delegates) { listener.delegate = delegate }
    }
    public func activate() { listeners.forEach { $0.activate() }; state.checkIdleLater() }
    public func invalidate() { listeners.forEach { $0.invalidate() }; state.shutdown() }
}

private enum BrokerRole { case client, host }
private final class BrokerListener: NSObject, NSXPCListenerDelegate, @unchecked Sendable {
    let state: BrokerState
    let role: BrokerRole
    let requirement: String
    init(state: BrokerState, role: BrokerRole, requirement: String) { self.state = state; self.role = role; self.requirement = requirement }
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection connection: NSXPCConnection) -> Bool {
        guard connection.effectiveUserIdentifier == getuid(), state.add(connection, role: role) else { return false }
        connection.setCodeSigningRequirement(requirement)
        switch role {
        case .client:
            connection.exportedInterface = NSXPCInterface(with: RPCClientProtocol.self)
            connection.exportedObject = ClientEndpoint(state: state, connection: connection)
            connection.remoteObjectInterface = NSXPCInterface(with: RPCConsoleProtocol.self)
        case .host:
            connection.exportedInterface = NSXPCInterface(with: RPCRegistrationProtocol.self)
            connection.exportedObject = HostEndpoint(state: state, connection: connection)
            connection.remoteObjectInterface = NSXPCInterface(with: RPCExecutionProtocol.self)
        }
        connection.invalidationHandler = { [weak state, weak connection] in
            if let connection { state?.remove(connection) }
        }
        connection.interruptionHandler = { [weak connection] in connection?.invalidate() }
        connection.activate()
        return true
    }
}

private final class ClientEndpoint: NSObject, RPCClientProtocol, @unchecked Sendable {
    let state: BrokerState; weak var connection: NSXPCConnection?
    init(state: BrokerState, connection: NSXPCConnection) { self.state = state; self.connection = connection }
    func hello(withReply reply: @escaping @Sendable (Data) -> Void) { reply(state.hello()) }
    func execute(_ request: Data, withReply reply: @escaping @Sendable (Data) -> Void) {
        guard let connection else { reply(Data()); return }
        state.execute(request, client: connection, reply: reply)
    }
}
private final class HostEndpoint: NSObject, RPCRegistrationProtocol, @unchecked Sendable {
    let state: BrokerState; weak var connection: NSXPCConnection?
    init(state: BrokerState, connection: NSXPCConnection) { self.state = state; self.connection = connection }
    func registerHost(_ description: Data, withReply reply: @escaping @Sendable (Data) -> Void) {
        guard let connection else { reply(Data()); return }
        reply(state.register(description, connection: connection))
    }
    func emit(_ event: Data, withReply reply: @escaping @Sendable (Bool) -> Void) {
        guard let connection else { reply(false); return }
        state.emit(event, host: connection, reply: reply)
    }
}

private final class BrokerState: @unchecked Sendable {
    private struct Active {
        let client: NSXPCConnection
        let host: NSXPCConnection
        let reply: RPCOnce<Data>
        var control = false
        var acceptedEvents: UInt64 = 0
        var acknowledgedEvents: UInt64 = 0
        var response: RPCResponse?
        var failedOutput = false
    }
    private let lock = NSLock()
    private let root: String
    private let onIdle: @Sendable () -> Void
    private var connections: [ObjectIdentifier: NSXPCConnection] = [:]
    private var roles: [ObjectIdentifier: BrokerRole] = [:]
    private var host: NSXPCConnection?
    private var active: [UUID: Active] = [:]
    private var seen = Set<UUID>()
    private var epoch: UInt64 = 0
    init(root: String, onIdle: @escaping @Sendable () -> Void) { self.root = root; self.onIdle = onIdle }
    func add(_ c: NSXPCConnection, role: BrokerRole) -> Bool {
        lock.lock(); defer { lock.unlock() }
        guard connections.count < 24 else { return false }
        connections[ObjectIdentifier(c)] = c; roles[ObjectIdentifier(c)] = role; epoch &+= 1
        return true
    }
    func hello() -> Data {
        lock.lock(); let pid = host?.processIdentifier; lock.unlock()
        return (try? RPCWire.encode(RPCHello(root: root, hostPID: pid))) ?? Data()
    }
    func register(_ bytes: Data, connection c: NSXPCConnection) -> Data {
        guard let value = try? RPCWire.decode(RPCHostDescription.self, bytes), value.version == RPCWire.version,
              value.root == root else { return Data() }
        lock.lock()
        guard roles[ObjectIdentifier(c)] == .host, host == nil || host === c else { lock.unlock(); return Data() }
        host = c; epoch &+= 1; lock.unlock()
        return hello()
    }
    func execute(_ bytes: Data, client: NSXPCConnection, reply: @escaping @Sendable (Data) -> Void) {
        let request: RPCRequest
        do { request = try RPCWire.decode(RPCRequest.self, bytes); try request.validate(root: root) }
        catch { reply(Data()); return }
        lock.lock()
        guard roles[ObjectIdentifier(client)] == .client, let target = host else {
            lock.unlock(); reply(encode(.rejected(request.id, "Cider尚未连接命令通道，未执行。"))); return
        }
        let control = request.arguments.first == "kill"
        let ordinaryCount = active.values.filter { !$0.control }.count
        let controlCount = active.count - ordinaryCount
        guard active[request.id] == nil, !seen.contains(request.id), seen.count < 65_536,
              control ? controlCount < 2 : ordinaryCount < 8 else {
            lock.unlock(); reply(encode(.rejected(request.id, "请求已提交或执行队列已满，未重复执行。"))); return
        }
        seen.insert(request.id)
        active[request.id] = Active(client: client, host: target, reply: RPCOnce(reply), control: control)
        epoch &+= 1; lock.unlock()
        let proxy = target.remoteObjectProxyWithErrorHandler { [weak self] _ in self?.finishUnknown(request.id) }
        guard let executor = proxy as? RPCExecutionProtocol else { finishUnknown(request.id); return }
        let targetID = ObjectIdentifier(target)
        let targetPID = target.processIdentifier
        executor.execute(bytes) { [weak self] response in
            self?.completed(response, id: request.id, hostID: targetID, hostPID: targetPID)
        }
    }
    func emit(_ bytes: Data, host sender: NSXPCConnection, reply: @escaping @Sendable (Bool) -> Void) {
        guard let event = try? RPCWire.decode(RPCEvent.self, bytes, limit: 24_576),
              event.sequence > 0, event.bytes.count <= RPCWire.chunkBytes else { reply(false); return }
        lock.lock()
        guard var item = active[event.id], item.host === sender, item.acceptedEvents < UInt64.max,
              event.sequence == item.acceptedEvents + 1, item.acceptedEvents == item.acknowledgedEvents else {
            lock.unlock(); reply(false); return
        }
        item.acceptedEvents = event.sequence
        active[event.id] = item; let client = item.client; lock.unlock()
        let once = RPCOnce<Bool>(reply)
        let proxy = client.remoteObjectProxyWithErrorHandler { [weak self] _ in
            self?.acknowledge(event.id, event.sequence, failed: true); once.finish(false)
        }
        guard let console = proxy as? RPCConsoleProtocol else {
            acknowledge(event.id, event.sequence, failed: true); once.finish(false); return
        }
        console.output(bytes) { [weak self] in
            self?.acknowledge(event.id, event.sequence, failed: false); once.finish(true)
        }
    }
    private func acknowledge(_ id: UUID, _ sequence: UInt64, failed: Bool) {
        lock.lock()
        if var item = active[id] {
            if item.acknowledgedEvents < UInt64.max, sequence == item.acknowledgedEvents + 1 {
                item.acknowledgedEvents = sequence
            } else { item.failedOutput = true }
            item.failedOutput = item.failedOutput || failed
            active[id] = item
        }
        lock.unlock(); flush(id, expired: false)
    }
    private func completed(_ data: Data, id: UUID, hostID: ObjectIdentifier, hostPID: Int32) {
        guard var result = try? RPCWire.decode(RPCResponse.self, data), result.id == id,
              result.version == RPCWire.version, result.state == "completed" || result.state == "rejected",
              result.executorPID == hostPID else {
            finishUnknown(id); return
        }
        lock.lock()
        guard var item = active[id], ObjectIdentifier(item.host) == hostID else { lock.unlock(); return }
        result.executorPID = hostPID; item.response = result; active[id] = item; lock.unlock()
        flush(id, expired: false)
        DispatchQueue.global().asyncAfter(deadline: .now() + 5) { [weak self] in self?.flush(id, expired: true) }
    }
    private func flush(_ id: UUID, expired: Bool) {
        lock.lock()
        guard let item = active[id], var response = item.response else { lock.unlock(); return }
        let complete = item.acknowledgedEvents == response.sequenceCount && item.acceptedEvents == response.sequenceCount
        guard complete || expired else { lock.unlock(); return }
        active.removeValue(forKey: id)
        response.outputIncomplete = response.outputIncomplete || !complete || item.failedOutput
        lock.unlock(); item.reply.finish(encode(response)); checkIdleLater()
    }
    private func finishUnknown(_ id: UUID) {
        lock.lock(); let item = active.removeValue(forKey: id); lock.unlock()
        item?.reply.finish(encode(.unknown(id))); checkIdleLater()
    }
    func remove(_ c: NSXPCConnection) {
        lock.lock(); connections.removeValue(forKey: ObjectIdentifier(c)); roles.removeValue(forKey: ObjectIdentifier(c))
        if host === c { host = nil }
        let failed = active.filter { $0.value.host === c }.map(\.key)
        let abandoned = active.filter { $0.value.client === c }.map { ($0.key, $0.value.host) }
        // Detach observation, never terminate or repeat admitted programs. The GUI
        // independently accounts for work that cannot be cancelled safely yet.
        epoch &+= 1; lock.unlock()
        failed.forEach { finishUnknown($0) }
        for (id, target) in abandoned {
            let proxy = target.remoteObjectProxyWithErrorHandler { _ in }
            (proxy as? RPCExecutionProtocol)?.detach(id.uuidString)
            finishUnknown(id)
        }
        checkIdleLater()
    }
    func checkIdleLater() {
        lock.lock(); let token = epoch; let idle = host == nil && connections.isEmpty && active.isEmpty; lock.unlock()
        guard idle else { return }
        DispatchQueue.global().asyncAfter(deadline: .now() + 5) { [weak self] in
            guard let self else { return }
            self.lock.lock(); let idle = self.epoch == token && self.host == nil && self.connections.isEmpty && self.active.isEmpty; self.lock.unlock()
            if idle { self.onIdle() }
        }
    }
    func shutdown() {
        lock.lock(); let all = Array(connections.values); let ids = Array(active.keys); connections.removeAll(); roles.removeAll(); host = nil; lock.unlock()
        ids.forEach { finishUnknown($0) }; all.forEach { $0.invalidate() }
    }
    private func encode(_ value: RPCResponse) -> Data { (try? RPCWire.encode(value)) ?? Data() }
}
