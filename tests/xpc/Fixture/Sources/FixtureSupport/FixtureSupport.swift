import CiderCommands
import CiderCore
import CiderIPC
import Darwin
import Foundation

public enum FixtureSupport {
    public static func main(role: String) {
        do {
            // A stuck native connection must never leave a test process alive indefinitely.
            DispatchQueue.global().asyncAfter(deadline: .now() + (role == "broker" ? 180 : 90)) { _exit(124) }
            let arguments = Array(CommandLine.arguments.dropFirst())
            guard arguments.count >= 2, arguments[0] == "--root", arguments[1].hasPrefix("/"),
                  let bundle = RPCBundle.containingApp(for: try RPCBundle.executableURL()) else {
                throw RPCFailure("fixture requires its signed .app and an explicit private root")
            }
            let root = URL(fileURLWithPath: arguments[1], isDirectory: true)
            // Refuse accidental use with product locations. The runner creates this marker first.
            guard FileManager.default.fileExists(atPath: root.appendingPathComponent("fixture-owned").path) else {
                throw RPCFailure("private fixture ownership marker is absent")
            }
            let paths = CiderPaths(appSupport: root, caches: root.appendingPathComponent("FixtureCaches"), logs: root.appendingPathComponent("FixtureLogs"))
            let location = try RPCLocation(bundle: bundle, paths: paths)
            let identity = try RPCIdentity(bundle: bundle)
            let tail = Array(arguments.dropFirst(2))
            if role == "broker" {
                guard tail == ["--label", location.label] else { throw RPCFailure("fixture label mismatch") }
                let broker = RPCBroker(location: location, identity: identity, onIdle: { _exit(0) })
                broker.activate()
                withExtendedLifetime(broker) { RunLoop.current.run() }
                return
            }
            guard let operation = tail.first else { throw RPCFailure("fixture operation missing") }
            switch operation {
            case "describe":
                try json(["label": location.label, "root": location.root, "bundle": location.bundle.path,
                          "canonicalBundle": canonicalPath(location.bundle), "clientService": location.clientService, "hostService": location.hostService])
            case "unregister":
                try location.unregister()
                try json(["unregistered": true])
            case "register":
                try location.register()
                try json(["registered": true])
            case "launch-host", "launch-host-late":
                guard role == "client" else { throw RPCFailure("launch-host requires the client fixture") }
                let log = root.appendingPathComponent("host-\(UUID().uuidString).log")
                FileManager.default.createFile(atPath: log.path, contents: nil)
                let handle = try FileHandle(forWritingTo: log)
                defer { try? handle.close() }
                let process = Process()
                process.executableURL = bundle.appendingPathComponent("Contents/MacOS/Cider")
                process.arguments = ["--root", root.path, operation == "launch-host-late" ? "serve-late" : "serve"]
                process.environment = ["PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "HOME": NSHomeDirectory(), "TMPDIR": NSTemporaryDirectory()]
                process.standardInput = FileHandle.nullDevice
                process.standardOutput = handle; process.standardError = handle
                try process.run()
                try json(["hostPID": process.processIdentifier, "log": log.path])
            case "serve", "serve-late":
                guard role == "host" else { throw RPCFailure("serve requires the host fixture") }
                if operation == "serve" { try location.register() }
                let host = RPCGUIHost(location: location, identity: identity, dataOverride: nil, executor: FixtureExecutor(paths: paths))
                if operation == "serve-late" { host.start() } else { try host.connect(timeout: 5) }
                let ready: [String: Any] = ["pid": getpid(), "root": location.root, "label": location.label]
                try JSONSerialization.data(withJSONObject: ready).write(to: root.appendingPathComponent("ready.json"), options: .atomic)
                withExtendedLifetime(host) { RunLoop.current.run() }
            case "hello", "probe-client":
                let hello = try RPCClient(location: location, identity: identity).hello(timeout: 3)
                try encoded(hello)
            case "probe-host":
                let host = RPCGUIHost(location: location, identity: identity, dataOverride: nil, executor: FixtureExecutor(paths: paths))
                defer { host.invalidate() }
                try host.connect(timeout: 3)
                try json(["unexpectedHostAcceptance": true])
            case "execute", "execute-slow":
                guard tail.count == 2 else { throw RPCFailure("execute requires a request file") }
                let request = try JSONDecoder().decode(RPCRequest.self, from: Data(contentsOf: URL(fileURLWithPath: tail[1])))
                let capture = FixtureCapture(firstChunkDelay: operation == "execute-slow" ? 6 : 0)
                let response: RPCResponse
                var transportFailure: String?
                do {
                    response = try RPCClient(location: location, identity: identity).execute(request) { capture.append($0, stderr: $1) }
                } catch {
                    // Production RPCClient reports a broken connection as this explicit unknown
                    // failure. Preserve the actual error as evidence and mirror CLI exit 75.
                    guard error.localizedDescription == RPCResponse.unknown(request.id).message else { throw error }
                    response = .unknown(request.id)
                    transportFailure = error.localizedDescription
                }
                var object = try object(response)
                object["resultSource"] = transportFailure == nil ? "RPCResponse" : "RPCClientTransportFailure"
                if let transportFailure { object["transportFailure"] = transportFailure }
                object["stdoutBase64"] = capture.data(stderr: false).base64EncodedString()
                object["stderrBase64"] = capture.data(stderr: true).base64EncodedString()
                object["chunks"] = capture.count
                try json(object)
                exit(response.exitCode)
            case "raw":
                guard tail.count == 2 else { throw RPCFailure("raw requires an input file") }
                let bytes = try Data(contentsOf: URL(fileURLWithPath: tail[1]))
                let response = try rawRequest(bytes, location: location, identity: identity)
                try json(["replyBytes": response.count, "replyBase64": response.base64EncodedString()])
            case "burst":
                guard tail.count == 2 else { throw RPCFailure("burst requires a request array file") }
                let requests = try JSONDecoder().decode([RPCRequest].self, from: Data(contentsOf: URL(fileURLWithPath: tail[1])))
                guard requests.count <= 300 else { throw RPCFailure("fixture burst is bounded to 300 requests") }
                let client = RPCClient(location: location, identity: identity)
                var responses: [[String: Any]] = []
                for request in requests {
                    let response = try client.execute(request) { _, _ in }
                    responses.append(try object(response))
                    Thread.sleep(forTimeInterval: 0.001)
                }
                try json(["responses": responses])
            default: throw RPCFailure("unknown fixture operation")
            }
        } catch {
            try? json(["error": error.localizedDescription])
            exit(70)
        }
    }

    private static func object<T: Encodable>(_ value: T) throws -> [String: Any] {
        try JSONSerialization.jsonObject(with: JSONEncoder().encode(value)) as! [String: Any]
    }
    private static func encoded<T: Encodable>(_ value: T) throws { try json(object(value)) }
    private static func json(_ object: [String: Any]) throws {
        var bytes = try JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])
        bytes.append(10)
        try FileHandle.standardOutput.write(contentsOf: bytes)
    }
    private static func rawRequest(_ bytes: Data, location: RPCLocation, identity: RPCIdentity) throws -> Data {
        // Only the fixture sends malformed bytes. Production RPCClient validation stays intact.
        let connection = NSXPCConnection(machServiceName: location.clientService)
        connection.remoteObjectInterface = NSXPCInterface(with: RPCClientProtocol.self)
        connection.exportedInterface = NSXPCInterface(with: RPCConsoleProtocol.self)
        connection.exportedObject = FixtureConsole()
        connection.setCodeSigningRequirement(identity.broker)
        connection.activate()
        defer { connection.invalidate() }
        let result = FixtureWait<Data>()
        guard let client = connection.remoteObjectProxyWithErrorHandler({ result.finish(.failure($0)) }) as? RPCClientProtocol else {
            throw RPCFailure("fixture raw proxy unavailable")
        }
        client.execute(bytes) { result.finish(.success($0)) }
        return try result.wait(seconds: 5)
    }
}

private final class FixtureCapture: @unchecked Sendable {
    private let lock = NSLock()
    private var stdout = Data(), stderr = Data()
    private var chunks = 0
    private let firstChunkDelay: TimeInterval
    init(firstChunkDelay: TimeInterval = 0) { self.firstChunkDelay = firstChunkDelay }
    var count: Int { lock.lock(); defer { lock.unlock() }; return chunks }
    func append(_ bytes: Data, stderr: Bool) {
        lock.lock(); defer { lock.unlock() }
        if chunks == 0, firstChunkDelay > 0 { Thread.sleep(forTimeInterval: firstChunkDelay) }
        chunks += 1
        if stderr { self.stderr.append(bytes) } else { stdout.append(bytes) }
    }
    func data(stderr: Bool) -> Data { lock.lock(); defer { lock.unlock() }; return stderr ? self.stderr : stdout }
}

private final class FixtureExecutor: RPCCommandExecuting, @unchecked Sendable {
    private let paths: CiderPaths
    private let lock = NSLock()
    init(paths: CiderPaths) { self.paths = paths }
    func execute(_ request: RPCRequest, output: @escaping @Sendable (String, Bool) -> Void) -> Int32 {
        execute(request, observation: CommandObservation(), output: output)
    }
    func execute(_ request: RPCRequest, observation: CommandObservation, output: @escaping @Sendable (String, Bool) -> Void) -> Int32 {
        do {
            // This records admission in the host, independently of client/broker replies.
            lock.lock()
            do {
                let file = paths.appSupport.appendingPathComponent("invocations.jsonl")
                if !FileManager.default.fileExists(atPath: file.path) { FileManager.default.createFile(atPath: file.path, contents: nil) }
                let handle = try FileHandle(forWritingTo: file)
                defer { try? handle.close() }
                try handle.seekToEnd()
                var bytes = try RPCWire.encode(request); bytes.append(10)
                try handle.write(contentsOf: bytes)
                lock.unlock()
            } catch { lock.unlock(); throw error }
            switch request.arguments.first {
            case "fixture-echo":
                output(request.arguments.dropFirst().joined(separator: "|") + "\n", false)
                output("fixture-stderr\n", true)
                return 37
            case "fixture-stream":
                for index in 0..<64 { output("\(index):" + String(repeating: "流🙂", count: 500) + "\n", index % 3 == 0) }
                return 23
            case "fixture-long-output":
                for index in 0..<384 { output("long-\(index):" + String(repeating: "流🙂", count: 500) + "\n", false) }
                return 19
            case "fixture-block", "fixture-block-long", "fixture-observation-block":
                output("admitted\n", false)
                try Data().write(to: paths.appSupport.appendingPathComponent("admitted-\(request.id.uuidString)"))
                let release = paths.appSupport.appendingPathComponent("release-\(request.id.uuidString)")
                let deadline = ProcessInfo.processInfo.systemUptime + (request.arguments.first == "fixture-block-long" ? 30 : 10)
                while !FileManager.default.fileExists(atPath: release.path), ProcessInfo.processInfo.systemUptime < deadline {
                    if request.arguments.first == "fixture-observation-block", observation.isDetached {
                        try Data("75".utf8).write(to: paths.appSupport.appendingPathComponent("completed-\(request.id.uuidString)"))
                        return 75
                    }
                    Thread.sleep(forTimeInterval: 0.02)
                }
                let code: Int32 = FileManager.default.fileExists(atPath: release.path) ? 0 : 124
                try Data(String(code).utf8).write(to: paths.appSupport.appendingPathComponent("completed-\(request.id.uuidString)"))
                return code
            default:
                guard request.arguments == ["engine", "list"] || request.arguments == ["bottle", "list"] else {
                    output("fixture permits only readonly lists and fixture commands\n", true)
                    return 64
                }
                return CiderCommandExecutor.execute(arguments: request.arguments,
                    workingDirectory: URL(fileURLWithPath: request.workingDirectory), paths: paths, output: output)
            }
        } catch { output(error.localizedDescription + "\n", true); return 70 }
    }
}

private final class FixtureConsole: NSObject, RPCConsoleProtocol, @unchecked Sendable {
    func output(_ event: Data, withReply reply: @escaping @Sendable () -> Void) { reply() }
}

private final class FixtureWait<Value: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private let semaphore = DispatchSemaphore(value: 0)
    private var result: Result<Value, Error>?
    func finish(_ value: Result<Value, Error>) {
        lock.lock()
        guard result == nil else { lock.unlock(); return }
        result = value; lock.unlock(); semaphore.signal()
    }
    func wait(seconds: Double) throws -> Value {
        guard semaphore.wait(timeout: .now() + seconds) == .success else { throw RPCFailure("fixture native response timed out") }
        lock.lock(); let value = result; lock.unlock()
        guard let value else { throw RPCFailure("fixture native response absent") }
        return try value.get()
    }
}
