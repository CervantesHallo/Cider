import CiderCore
import Foundation

/// A copied prefix must not refresh the original user's saved Steam login during its first wineboot.
enum CloneStartup {
    static func prepare(_ bottle: Bottle) throws -> Bottle {
        let fm = FileManager.default
        var config = bottle.config
        // Copy identity itself is the protection boundary, including custom Steam installations.
        config.settings["cloneSteamBlocked"] = "1"
        _ = try FileSafety.child("prefix", in: bottle.directory, rejectSymlinks: true)
        _ = try FileSafety.child("prefix/drive_c", in: bottle.directory, rejectSymlinks: true)
        _ = try FileSafety.child(".cider", in: bottle.directory, rejectSymlinks: true)
        let keys: Set<String> = ["run", "runonce", "runonceex", "runservices", "runservicesonce"]
        for name in ["user.reg", "system.reg"] {
            let file = try FileSafety.child(name, in: bottle.prefix, rejectSymlinks: true)
            guard fm.fileExists(atPath: file.path) else { continue }
            let original = try String(contentsOf: file, encoding: .utf8)
            var changed = false
            let lines = original.components(separatedBy: "\n").map { line -> String in
                guard line.hasPrefix("["), let end = line.firstIndex(of: "]") else { return line }
                let key = String(line[line.index(after: line.startIndex)..<end])
                guard !key.lowercased().hasPrefix("software\\\\cider\\\\disabledstartup"), key.lowercased().contains("microsoft\\\\windows\\\\currentversion\\\\"),
                      let last = key.split(separator: "\\").last, keys.contains(last.lowercased()) else { return line }
                changed = true
                return "[Software\\\\Cider\\\\DisabledStartup\\\\" + key + String(line[end...])
            }
            if changed { try lines.joined(separator: "\n").write(to: file, atomically: true, encoding: .utf8) }
        }
        let users = bottle.driveC.appendingPathComponent("users")
        var startup = [bottle.driveC.appendingPathComponent("ProgramData/Microsoft/Windows/Start Menu/Programs/Startup")]
        for user in (try? fm.contentsOfDirectory(at: users, includingPropertiesForKeys: nil)) ?? [] {
            startup.append(user.appendingPathComponent("AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup"))
        }
        let disabled = try FileSafety.child(".cider/disabled-startup", in: bottle.directory, rejectSymlinks: true)
        for folder in startup where FileSafety.exists(folder) {
            let relative = String(folder.path.dropFirst(bottle.driveC.path.count + 1))
            _ = try FileSafety.child(relative, in: bottle.driveC, rejectSymlinks: true)
            try fm.ensureDirectory(disabled)
            try fm.moveItem(at: folder, to: disabled.appendingPathComponent(UUID().uuidString))
            try fm.ensureDirectory(folder)
        }
        try JSONFile.write(config, to: bottle.configURL)
        return Bottle(config: config, directory: bottle.directory)
    }
}
