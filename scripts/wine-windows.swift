// Lists on-screen and off-screen windows owned by Wine processes: id, owner, title, width, height.
import CoreGraphics
let list = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as? [[String: Any]] ?? []
for w in list {
    let owner = w[kCGWindowOwnerName as String] as? String ?? ""
    guard owner == "wine" || owner.hasSuffix(".exe") else { continue }
    let b = w[kCGWindowBounds as String] as? [String: Any] ?? [:]
    print("\(w[kCGWindowNumber as String] ?? 0)\t\(owner)\t\(w[kCGWindowName as String] as? String ?? "")\t\(b["Width"] ?? 0)\t\(b["Height"] ?? 0)")
}
