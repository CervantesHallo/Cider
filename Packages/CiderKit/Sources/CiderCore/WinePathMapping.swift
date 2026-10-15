import Foundation

/// Shared host-to-Windows mapping. The selected prefix supplies drive identity;
/// a directory merely named drive_c elsewhere is never treated as this C: drive.
public enum WinePathMapping {
    public static func windowsPath(forHostPath path: String, prefix: URL) -> String {
        DriveMap(prefix: prefix).windowsPath(forHostPath: path)
    }

    /// Immutable drive roots for a single observation. A scanner can reuse this
    /// for every process in one prefix, then discard it before the next scan.
    public struct DriveMap: Sendable {
        private let roots: [(drive: String, path: String)]

        public init(prefix: URL) {
            var roots: [(drive: String, path: String)] = [
                ("c", prefix.appendingPathComponent("drive_c").resolvingSymlinksInPath().path), ("z", "/"),
            ]
            let devices = prefix.appendingPathComponent("dosdevices")
            // Filter before sorting: only drive links affect precedence, even
            // when an imported prefix contains many unrelated directory entries.
            let drives = ((try? FileManager.default.contentsOfDirectory(atPath: devices.path)) ?? []).filter { name in
                name.count == 2 && name.last == ":" && name.first.map { $0.isASCII && $0.isLetter } == true
            }.sorted()
            for name in drives {
                roots.append((String(name.prefix(1)).lowercased(), devices.appendingPathComponent(name).resolvingSymlinksInPath().path))
            }
            self.roots = roots.sorted {
                if $0.path.count != $1.path.count { return $0.path.count > $1.path.count }
                return $0.drive < $1.drive
            }
        }

        public func windowsPath(forHostPath path: String) -> String {
            let resolved = URL(fileURLWithPath: path).standardizedFileURL.resolvingSymlinksInPath().path
            // Z:/ always matches an absolute file URL; preserve the existing
            // longest-root and lexical-drive tie ordering.
            let root = roots.first {
                resolved == $0.path || resolved.hasPrefix($0.path == "/" ? "/" : $0.path + "/")
            }!
            let suffix = resolved.dropFirst(root.path == "/" ? 0 : root.path.count)
            return root.drive.uppercased() + ":" + (suffix.isEmpty ? "\\" : suffix.replacingOccurrences(of: "/", with: "\\"))
        }
    }
}
