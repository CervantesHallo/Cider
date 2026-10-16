import AppKit
import Foundation
import CoreGraphics
let args = CommandLine.arguments
if args[1] == "windows" {
    let pids = Set(args[2].split(separator: ",").compactMap { Int32($0) })
    let rows = (CGWindowListCopyWindowInfo([.optionAll, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? [])
        .filter { ($0[kCGWindowOwnerPID as String] as? NSNumber).map { pids.contains($0.int32Value) } ?? false }
        .map { row -> [String: Any] in
            ["ownerPID": row[kCGWindowOwnerPID as String] ?? 0, "windowID": row[kCGWindowNumber as String] ?? 0,
             "onscreen": row[kCGWindowIsOnscreen as String] ?? NSNull(),
             "bounds": row[kCGWindowBounds as String] ?? [:], "layer": row[kCGWindowLayer as String] ?? 0]
        }
    FileHandle.standardOutput.write(try JSONSerialization.data(withJSONObject: rows, options: .sortedKeys)); exit(0)
}
let target = URL(fileURLWithPath: args[2]).standardizedFileURL.resolvingSymlinksInPath()
let matches = NSWorkspace.shared.runningApplications.filter {
    $0.bundleURL?.standardizedFileURL.resolvingSymlinksInPath() == target
}
if args[1] == "terminate" {
    for app in matches { _ = app.terminate() }
    let deadline = ProcessInfo.processInfo.systemUptime + 5
    while matches.contains(where: { !$0.isTerminated }), ProcessInfo.processInfo.systemUptime < deadline {
        RunLoop.current.run(until: Date(timeIntervalSinceNow: 0.02))
    }
    for app in matches where !app.isTerminated { _ = app.forceTerminate() }
}
let rows: [[String: Any]] = matches.map { ["pid": $0.processIdentifier, "bundle": $0.bundleURL?.path ?? "", "terminated": $0.isTerminated] }
FileHandle.standardOutput.write(try JSONSerialization.data(withJSONObject: rows, options: .sortedKeys))
