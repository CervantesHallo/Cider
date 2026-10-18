import AppKit
import CiderCommands
import CiderCore
import CiderIPC
import Darwin
import Foundation

private struct FixtureConfiguration: Sendable {
    let home: URL
    let paths: CiderPaths
    let location: RPCLocation

    init() throws {
        let environment = ProcessInfo.processInfo.environment
        guard let home = environment["CIDER_HOME"], home.hasPrefix("/"),
              FileManager.default.fileExists(atPath: URL(fileURLWithPath: home).appendingPathComponent("cold-fixture-owned").path),
              let bundle = RPCBundle.containingApp(for: try RPCBundle.executableURL()) else {
            throw RPCFailure("cold fixture requires an explicit marked CIDER_HOME and its signed app")
        }
        self.home = URL(fileURLWithPath: home, isDirectory: true)
        paths = CiderPaths.standard(environment: ["CIDER_HOME": home])
        location = try RPCLocation(bundle: bundle, paths: paths)
    }
}

private final class ReadonlyExecutor: RPCCommandExecuting, @unchecked Sendable {
    private let configuration: FixtureConfiguration
    private let lock = NSLock()
    init(_ configuration: FixtureConfiguration) { self.configuration = configuration }

    func execute(_ request: RPCRequest, output: @escaping @Sendable (String, Bool) -> Void) -> Int32 {
        guard request.arguments == ["engine", "list"] || request.arguments == ["bottle", "list"] else {
            output("cold fixture permits readonly engine/bottle lists only\n", true)
            return 64
        }
        do {
            lock.lock()
            do {
                let file = configuration.home.appendingPathComponent("executions.jsonl")
                if !FileManager.default.fileExists(atPath: file.path) { FileManager.default.createFile(atPath: file.path, contents: nil) }
                let handle = try FileHandle(forWritingTo: file)
                defer { try? handle.close() }
                try handle.seekToEnd()
                let receipt: [String: Any] = ["requestID": request.id.uuidString, "arguments": request.arguments,
                    "executorPID": getpid(), "appSupport": request.appSupport, "workingDirectory": request.workingDirectory]
                var bytes = try JSONSerialization.data(withJSONObject: receipt, options: .sortedKeys)
                bytes.append(10); try handle.write(contentsOf: bytes)
                lock.unlock()
            } catch { lock.unlock(); throw error }
            return CiderCommandExecutor.execute(arguments: request.arguments,
                workingDirectory: URL(fileURLWithPath: request.workingDirectory), paths: configuration.paths, output: output)
        } catch { output(error.localizedDescription + "\n", true); return 70 }
    }
}

@MainActor private final class FixtureDelegate: NSObject, NSApplicationDelegate {
    private let configuration: FixtureConfiguration
    private var host: RPCGUIHost?
    init(_ configuration: FixtureConfiguration) { self.configuration = configuration }

    func applicationDidFinishLaunching(_ notification: Notification) {
        let configuration = configuration
        Task.detached { [weak self] in
            do {
                try configuration.location.register()
                let host = RPCGUIHost(location: configuration.location, identity: try RPCIdentity(bundle: configuration.location.bundle),
                    dataOverride: nil, executor: ReadonlyExecutor(configuration))
                await MainActor.run { self?.host = host }
                host.start()
                let ready: [String: Any] = ["hostPID": getpid(), "label": configuration.location.label,
                    "root": configuration.location.root, "launchRoute": "NSApplicationDelegate", "startup": "bounded start()"]
                try JSONSerialization.data(withJSONObject: ready).write(to: configuration.home.appendingPathComponent("ready.json"), options: .atomic)
            } catch {
                try? Data(error.localizedDescription.utf8).write(to: configuration.home.appendingPathComponent("host-error.txt"))
                await MainActor.run { NSApplication.shared.terminate(nil) }
            }
        }
    }
    func applicationWillTerminate(_ notification: Notification) { host?.invalidate() }
}

@main struct FixtureAppHost {
    @MainActor static func main() {
        do {
            let configuration = try FixtureConfiguration()
            switch Array(CommandLine.arguments.dropFirst()) {
            case ["--describe-fixture"]:
                let description: [String: Any] = ["root": configuration.location.root, "label": configuration.location.label,
                    "bundle": configuration.location.bundle.path, "canonicalBundle": canonicalPath(configuration.location.bundle),
                    "home": configuration.home.path]
                var bytes = try JSONSerialization.data(withJSONObject: description, options: .sortedKeys); bytes.append(10)
                try FileHandle.standardOutput.write(contentsOf: bytes)
                return
            case ["--unregister-fixture"]:
                try configuration.location.unregister()
                return
            case []: break
            default: throw RPCFailure("unsupported cold fixture argument")
            }
            DispatchQueue.global().asyncAfter(deadline: .now() + 60) { _exit(124) }
            let app = NSApplication.shared
            app.setActivationPolicy(.prohibited)
            let delegate = FixtureDelegate(configuration)
            app.delegate = delegate
            withExtendedLifetime(delegate) { app.run() }
        } catch {
            FileHandle.standardError.write(Data((error.localizedDescription + "\n").utf8))
            exit(70)
        }
    }
}
