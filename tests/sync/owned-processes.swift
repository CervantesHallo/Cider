// Native observation/cleanup through Cider's existing prefix and kernel identity checks.
// This helper never spawns Wine and cannot act outside its owned acceptance home.
import CiderBottle
import CiderCore
import CiderRuntime
import Foundation
import Darwin
import AppKit
let arguments = CommandLine.arguments
guard arguments.count == 3, ["list", "stop", "mono", "stop-runtime"].contains(arguments[1]) else { exit(64) }
let root = URL(fileURLWithPath: arguments[2]).standardizedFileURL.resolvingSymlinksInPath()
let owner = try JSONSerialization.jsonObject(with: Data(contentsOf: root.appendingPathComponent("owner.json"))) as! [String: Any]
guard root.lastPathComponent.hasPrefix("waitall-mac-"), owner["root"] as? String == root.path else { exit(65) }
var hostShutdown: [String: Any] = [:]
if arguments[1] == "stop-runtime" {
    // Reap the only spawner before draining Wine. A disconnected CLI and an
    // empty Wine snapshot cannot rule out a GUI request paused before spawn.
    let bundle = root.appendingPathComponent("source/out/Cider.app").resolvingSymlinksInPath()
    guard let identifier = Bundle(url: bundle)?.bundleIdentifier,
          let executable = Bundle(url: bundle)?.executableURL?.resolvingSymlinksInPath(),
          identifier.hasPrefix("org.cider.waitall-lab."),
          bundle.path.hasPrefix(root.path + "/source/out/") else { exit(67) }
    func identity(_ pid: pid_t) -> (ended: Bool, birth: UInt64?) {
        var info = proc_bsdinfo()
        errno = 0
        let result = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size))
        if result <= 0 { return (errno == ESRCH, nil) }
        guard result == MemoryLayout<proc_bsdinfo>.size else { return (false, nil) }
        if info.pbi_status == SZOMB { return (true, nil) }
        return (false, UInt64(info.pbi_start_tvsec) * 1_000_000 + UInt64(info.pbi_start_tvusec))
    }
    var unreadableIdentity = false
    func hosts() -> [(app: NSRunningApplication, birth: UInt64)] {
        var result: [(app: NSRunningApplication, birth: UInt64)] = []
        for app in NSRunningApplication.runningApplications(withBundleIdentifier: identifier)
            where app.bundleURL?.resolvingSymlinksInPath() == bundle {
            let state = identity(app.processIdentifier)
            if state.ended { continue }
            guard let birth = state.birth else { unreadableIdentity = true; continue }
            var path = [CChar](repeating: 0, count: 4 * Int(MAXPATHLEN))
            guard proc_pidpath(app.processIdentifier, &path, UInt32(path.count)) > 0 else {
                unreadableIdentity = true; continue
            }
            guard URL(fileURLWithPath: String(cString: path)).resolvingSymlinksInPath() == executable else { continue }
            // Recheck the kernel birth after reading the path; stale AppKit
            // objects must not turn a reused PID into a termination target.
            guard identity(app.processIdentifier).birth == birth else { unreadableIdentity = true; continue }
            result.append((app, birth))
        }
        return result
    }
    let capturedHosts = hosts()
    let deadline = ProcessInfo.processInfo.systemUptime + 5
    var requested = true
    for host in capturedHosts {
        if identity(host.app.processIdentifier).birth != host.birth { unreadableIdentity = true; continue }
        if !host.app.forceTerminate() { requested = false }
    }
    while !hosts().isEmpty && ProcessInfo.processInfo.systemUptime < deadline {
        RunLoop.current.run(until: Date().addingTimeInterval(0.05))
    }
    let stopped = hosts().isEmpty && !unreadableIdentity && capturedHosts.allSatisfy {
        let state = identity($0.app.processIdentifier)
        return state.ended || (state.birth != nil && state.birth != $0.birth)
    }
    hostShutdown = ["bundle": bundle.path, "capturedPIDs": capturedHosts.map { $0.app.processIdentifier },
                    "terminationRequested": requested, "kernelIdentityReadable": !unreadableIdentity,
                    "confirmation": "kernel birth/exit, not cached AppKit isTerminated", "confirmed": stopped]
}
let paths = CiderPaths.standard(environment: ["CIDER_HOME": root.appendingPathComponent("private-home").path])
let bottles = try BottleStore(paths: paths).list()
let prefixes = Dictionary(uniqueKeysWithValues: bottles.map { ($0.config.id, $0.prefix) })
guard prefixes.values.allSatisfy({ $0.standardizedFileURL.resolvingSymlinksInPath().path.hasPrefix(root.path + "/private-home/AppSupport/Bottles/") }) else { exit(66) }
let captured = ProcessScanner.scan(prefixes: prefixes, includeLoaders: true)
var selected = arguments[1] == "mono" ? captured.filter { $0.normalizedImage == "c:/windows/system32/control.exe" } : captured
var pending = arguments[1] == "list" ? selected : ProcessScanner.terminate(selected, grace: 2, forceWait: 2)
var emptyObservations = 0
if arguments[1] == "stop-runtime" {
    let deadline = ProcessInfo.processInfo.systemUptime + 8
    repeat {
        let late = ProcessScanner.scan(prefixes: prefixes, includeLoaders: true)
        selected = Array(Set(selected + late))
        pending = ProcessScanner.terminate(Array(Set(pending + late)), grace: 0.25, forceWait: 0.5)
        if pending.isEmpty && ProcessScanner.scan(prefixes: prefixes, includeLoaders: true).isEmpty {
            emptyObservations += 1
        } else { emptyObservations = 0 }
        if emptyObservations >= 2 { break }
        Thread.sleep(forTimeInterval: 0.1)
    } while ProcessInfo.processInfo.systemUptime < deadline
}
var report: [String: Any] = ["mode": arguments[1], "captured": selected.map { ["pid": $0.pid, "bottle": $0.bottleID, "image": $0.windowsImage] }, "pending": pending.map { $0.pid }, "scope": root.path]
if arguments[1] == "stop-runtime" {
    report["hostShutdown"] = hostShutdown
    report["emptyObservations"] = emptyObservations
}
FileHandle.standardOutput.write(try JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys]))
if arguments[1] != "list" && !pending.isEmpty { exit(1) }
if arguments[1] == "stop-runtime" && hostShutdown["confirmed"] as? Bool != true { exit(1) }
if arguments[1] == "stop-runtime" && emptyObservations < 2 { exit(1) }
