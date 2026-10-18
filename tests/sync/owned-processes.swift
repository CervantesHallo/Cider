// Native observation/cleanup through Cider's existing prefix and kernel identity checks.
// This helper never spawns Wine and cannot act outside its owned acceptance home.
import CiderBottle
import CiderCore
import CiderRuntime
import Foundation
import Darwin
let arguments = CommandLine.arguments
guard arguments.count == 3, ["list", "stop", "mono"].contains(arguments[1]) else { exit(64) }
let root = URL(fileURLWithPath: arguments[2]).standardizedFileURL.resolvingSymlinksInPath()
let owner = try JSONSerialization.jsonObject(with: Data(contentsOf: root.appendingPathComponent("owner.json"))) as! [String: Any]
guard root.lastPathComponent.hasPrefix("waitall-mac-"), owner["root"] as? String == root.path else { exit(65) }
let paths = CiderPaths.standard(environment: ["CIDER_HOME": root.appendingPathComponent("private-home").path])
let bottles = try BottleStore(paths: paths).list()
let prefixes = Dictionary(uniqueKeysWithValues: bottles.map { ($0.config.id, $0.prefix) })
guard prefixes.values.allSatisfy({ $0.standardizedFileURL.resolvingSymlinksInPath().path.hasPrefix(root.path + "/private-home/AppSupport/Bottles/") }) else { exit(66) }
let captured = ProcessScanner.scan(prefixes: prefixes, includeLoaders: true)
let selected = arguments[1] == "mono" ? captured.filter { $0.normalizedImage == "c:/windows/system32/control.exe" } : captured
let pending = arguments[1] == "list" ? selected : ProcessScanner.terminate(selected, grace: 2, forceWait: 2)
let report: [String: Any] = ["mode": arguments[1], "captured": selected.map { ["pid": $0.pid, "bottle": $0.bottleID, "image": $0.windowsImage] }, "pending": pending.map { $0.pid }, "scope": root.path]
FileHandle.standardOutput.write(try JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys]))
if arguments[1] != "list" && !pending.isEmpty { exit(1) }
