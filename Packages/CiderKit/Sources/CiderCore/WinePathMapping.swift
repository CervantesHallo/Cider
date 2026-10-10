import Foundation

/// Shared host-to-Windows mapping. The selected prefix supplies drive identity;
/// a directory merely named drive_c elsewhere is never treated as this C: drive.
public enum WinePathMapping {
    public static func windowsPath(forHostPath path: String, prefix: URL) -> String {
        let resolved = URL(fileURLWithPath: path).standardizedFileURL.resolvingSymlinksInPath().path
        var roots: [(drive: String, path: String)] = [
            ("c", prefix.appendingPathComponent("drive_c").resolvingSymlinksInPath().path), ("z", "/"),
        ]
        let devices = prefix.appendingPathComponent("dosdevices")
        for name in ((try? FileManager.default.contentsOfDirectory(atPath: devices.path)) ?? []).sorted() {
            guard name.count == 2, name.last == ":", let letter = name.first,
                  letter.isASCII, letter.isLetter else { continue }
            roots.append((String(letter).lowercased(), devices.appendingPathComponent(name).resolvingSymlinksInPath().path))
        }
        let matches = roots.filter {
            resolved == $0.path || resolved.hasPrefix($0.path == "/" ? "/" : $0.path + "/")
        }
        let root = matches.sorted {
            if $0.path.count != $1.path.count { return $0.path.count > $1.path.count }
            return $0.drive < $1.drive
        }.first!
        let suffix = resolved.dropFirst(root.path == "/" ? 0 : root.path.count)
        return root.drive.uppercased() + ":" + (suffix.isEmpty ? "\\" : suffix.replacingOccurrences(of: "/", with: "\\"))
    }
}
