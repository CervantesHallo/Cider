import CiderIPC
import Foundation
import Darwin

@main struct CiderXPCBridge {
    static func main() {
        do {
            let args = Array(CommandLine.arguments.dropFirst())
            guard args.count == 4, args[0] == "--root", args[2] == "--label", args[1].hasPrefix("/"),
                  let app = RPCBundle.containingApp(for: try RPCBundle.executableURL()) else { exit(64) }
            let root = URL(fileURLWithPath: args[1], isDirectory: true)
            let location = try RPCLocation(bundle: app, paths: .init(appSupport: root, caches: root, logs: root))
            guard location.label == args[3] else { exit(64) }
            let broker = RPCBroker(location: location, identity: try RPCIdentity(bundle: app), onIdle: { exit(0) })
            broker.activate()
            withExtendedLifetime(broker) { RunLoop.current.run() }
        } catch {
            FileHandle.standardError.write(Data((error.localizedDescription + "\n").utf8)); exit(70)
        }
    }
}
