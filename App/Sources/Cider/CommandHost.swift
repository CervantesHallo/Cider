import CiderCommands
import CiderCore
import CiderIPC
import Foundation
import SwiftUI

private struct GUICommandExecutor: RPCCommandExecuting {
    let paths: CiderPaths
    let dataOverride: String?
    func execute(_ request: RPCRequest, output: @escaping @Sendable (String, Bool) -> Void) -> Int32 {
        CiderCommandExecutor.execute(arguments: request.arguments,
            workingDirectory: URL(fileURLWithPath: request.workingDirectory, isDirectory: true),
            paths: paths, dataOverride: dataOverride, output: output)
    }
}

@MainActor final class CiderAppDelegate: NSObject, NSApplicationDelegate {
    private var commandHost: RPCGUIHost?
    func applicationDidFinishLaunching(_ notification: Notification) {
        Task.detached(priority: .userInitiated) { [weak self] in
            do {
                let executable = try RPCBundle.executableURL()
                guard let app = RPCBundle.containingApp(for: executable) else { throw RPCFailure("Cider应用包不完整。") }
                let paths = CiderPaths.standard()
                let data = ProcessInfo.processInfo.environment["CIDER_DATA"].flatMap { $0.isEmpty ? nil : canonicalPath(URL(fileURLWithPath: $0)) }
                let location = try RPCLocation(bundle: app, paths: paths)
                let identity = try RPCIdentity(bundle: app)
                try location.register()
                let host = RPCGUIHost(location: location, identity: identity, dataOverride: data,
                    executor: GUICommandExecutor(paths: paths, dataOverride: data))
                await MainActor.run { self?.commandHost = host }
                host.start()
            } catch { NSLog("Cider command host: %@", error.localizedDescription) }
        }
    }
    func applicationWillTerminate(_ notification: Notification) { commandHost?.invalidate() }
}
