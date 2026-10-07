import CiderCore
import CiderSchema
import CiderStore
import Foundation

/// A saved copy of a bottle's prefix and config (see `BottleStore.snapshot`).
public struct BottleSnapshot: Sendable, Identifiable, Equatable {
    public let id: String          // compact timestamp, also the directory name
    public let date: Date?
    public let reason: String
    public let directory: URL
}

extension BottleStore {
    /// Snapshots of the bottle, newest first.
    public func snapshots(of bottle: Bottle) -> [BottleSnapshot] {
        let root = bottle.directory.appendingPathComponent(".cider/snapshots", isDirectory: true)
        let names = (try? FileManager.default.contentsOfDirectory(atPath: root.path)) ?? []
        let parser = DateFormatter()
        parser.locale = Locale(identifier: "en_US_POSIX")
        parser.timeZone = TimeZone(identifier: "UTC")
        parser.dateFormat = "yyyyMMdd'T'HHmmss"
        return names.filter { !$0.hasPrefix(".") }.sorted(by: >).compactMap { name in
            let dir = root.appendingPathComponent(name, isDirectory: true)
            guard FileManager.default.fileExists(atPath: dir.appendingPathComponent("prefix").path) else { return nil }
            let reason = (try? String(contentsOf: dir.appendingPathComponent("reason.txt"), encoding: .utf8)) ?? ""
            return BottleSnapshot(id: name, date: parser.date(from: String(name.prefix(15))), reason: reason, directory: dir)
        }
    }

    /// Puts the bottle back to a snapshot: the current state is snapshotted first ("before restore"), then the
    /// snapshot's prefix is cloned into place and its config restored (so an engine switch is undone too).
    /// The bottle keeps its current name.
    @discardableResult
    public func restore(_ bottle: Bottle, to snapshot: BottleSnapshot) throws -> Bottle {
        try withOperation(bottle) { current in
            let fm = FileManager.default
            let snapshotsRoot = try FileSafety.child(".cider/snapshots", in: current.directory, rejectSymlinks: true)
            _ = try FileSafety.component(snapshot.id)
            let source = try FileSafety.child(snapshot.id, in: snapshotsRoot, rejectSymlinks: true)
            guard source.standardizedFileURL == snapshot.directory.standardizedFileURL else { throw CiderError.invalid("快照不属于此瓶子。") }
            var config = try JSONFile.read(BottleConfig.self, from: source.appendingPathComponent("cider-bottle.json"))
            config.id = current.config.id
            config.name = current.config.name
            config.launchers = current.config.launchers
            if current.config.settings["cloneSteamBlocked"] == "1" { config.settings["cloneSteamBlocked"] = "1" }
            try self.snapshot(current, reason: "before restoring \(snapshot.id)")
            let staging = paths.bottles.appendingPathComponent(".restoring-" + UUID().uuidString)
            var keepBackup = false
            defer { if !keepBackup { try? fm.removeItem(at: staging) } }
            // Commit prefix and configuration together, not as two independent replacements.
            try Command.run("/bin/cp", ["-c", "-R", current.directory.path, staging.path])
            try fm.removeItem(at: staging.appendingPathComponent("prefix"))
            try Command.run("/bin/cp", ["-c", "-R", source.appendingPathComponent("prefix").path, staging.appendingPathComponent("prefix").path])
            let history = try FileSafety.child(".cider/patches", in: staging, rejectSymlinks: true)
            if FileSafety.exists(history) {
                let isolated = staging.appendingPathComponent(".cider/isolated-patches-" + UUID().uuidString)
                try fm.moveItem(at: history, to: isolated)
            }
            let savedHistory = try FileSafety.child("patches", in: source, rejectSymlinks: true)
            if FileSafety.exists(savedHistory) {
                try fm.ensureDirectory(history.deletingLastPathComponent())
                try Command.run("/bin/cp", ["-c", "-R", savedHistory.path, history.path])
            }
            try JSONFile.write(config, to: staging.appendingPathComponent("cider-bottle.json"))
            if config.settings["cloneSteamBlocked"] == "1" { _ = try CloneStartup.prepare(Bottle(config: config, directory: staging)) }
            if let backup = try FileSafety.commit(staging, to: current.directory) {
                keepBackup = true
                try? fm.trashItem(at: backup, resultingItemURL: nil)
            }
            return Bottle(config: config, directory: current.directory)
        }
    }

    public func deleteSnapshot(_ snapshot: BottleSnapshot) throws {
        try FileManager.default.removeItem(at: snapshot.directory)
    }

    @discardableResult
    public func rename(_ bottle: Bottle, to name: String) throws -> Bottle {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { throw CiderError.invalid("a bottle needs a name") }
        return try update(bottle) { $0.name = trimmed }
    }

    /// Copies a bottle with APFS clones (instant; blocks are shared until either copy changes them).
    /// Snapshots stay with the original.
    @discardableResult
    public func duplicate(_ bottle: Bottle, name: String) throws -> Bottle {
        try withOperation(bottle) { current in
            try runner(for: current).killAll()
            try PatchState.requireRecovered(in: current.directory)
            let dir = try FileSafety.reserveDirectory(in: paths.bottles) { Identifiers.make(from: name) }
            do {
                try Command.run("/bin/cp", ["-c", "-R", current.prefix.path, dir.appendingPathComponent("prefix").path])
                let history = try FileSafety.child(".cider/patches", in: current.directory, rejectSymlinks: true)
                if FileSafety.exists(history) {
                    let destination = try FileSafety.child(".cider/patches", in: dir, rejectSymlinks: true)
                    try FileManager.default.ensureDirectory(destination.deletingLastPathComponent())
                    try Command.run("/bin/cp", ["-c", "-R", history.path, destination.path])
                }
                var config = current.config
                config.id = dir.lastPathComponent
                config.name = name
                config.createdAt = Identifiers.timestamp()
                config.engineHistory = []
                config.settings["cloneSteamBlocked"] = "1"
                let copy = Bottle(config: config, directory: dir)
                try JSONFile.write(config, to: copy.configURL)
                return try CloneStartup.prepare(copy)
            } catch {
                try FileManager.default.removeItem(at: dir)
                throw error
            }
        }
    }

    @discardableResult
    public func saveLauncher(_ launcher: BottleConfig.Launcher, in bottle: Bottle) throws -> Bottle {
        try update(bottle) { config in
            var list = config.launchers ?? []
            if let i = list.firstIndex(where: { $0.id == launcher.id }) { list[i] = launcher } else { list.append(launcher) }
            config.launchers = list
        }
    }

    @discardableResult
    public func removeLauncher(id: String, from bottle: Bottle) throws -> Bottle {
        try update(bottle) { $0.launchers = ($0.launchers ?? []).filter { $0.id != id } }
    }

    public static let highResolutionKey = "highResolution"

    /// Turns High Resolution Mode on or off. The bottle is stopped first: the Mac driver reads the setting when
    /// a process starts.
    @discardableResult
    public func setHighResolution(_ on: Bool, for bottle: Bottle) throws -> Bottle {
        return try withOperation(bottle) { current in
            let runner = try runner(for: current)
            try runner.killAll()
            try PrefixSetup.importRegistry(PrefixSetup.highResolution(on), named: "high-resolution", bottle: current, runner: runner)
            try runner.killAll()
            return try update(current) { $0.settings[Self.highResolutionKey] = on ? "1" : "0" }
        }
    }

    /// Per-program Windows version (Wine's AppDefaults; read by the program itself, so it also applies when a
    /// launcher such as Steam starts it). `nil` removes the override.
    public func setWindowsVersion(_ winver: String?, forExecutable exe: String, in bottle: Bottle) throws {
        try withOperation(bottle) { current in
            let key = #"HKEY_CURRENT_USER\Software\Wine\AppDefaults\"# + exe
            try PrefixSetup.importRegistry([(key, [("Version", winver.map(RegValue.string) ?? .delete)])],
                                           named: "appdefaults", bottle: current, runner: try runner(for: current))
        }
    }

    public static let metalHUDKey = "metalHUD"
    public static let advertiseAVXKey = "advertiseAVX"

    /// A string value from Wine's registry files (`HKCU\…` → user.reg, `HKLM\…` → system.reg), or nil.
    public func registryString(_ key: String, _ name: String, in bottle: Bottle) -> String? {
        let parts = key.split(separator: "\\", maxSplits: 1).map(String.init)
        guard parts.count == 2 else { return nil }
        let file = ["HKCU", "HKEY_CURRENT_USER"].contains(parts[0].uppercased()) ? "user.reg" : "system.reg"
        guard let text = try? String(contentsOf: bottle.prefix.appendingPathComponent(file), encoding: .utf8),
              let start = text.range(of: "[" + parts[1].replacingOccurrences(of: "\\", with: "\\\\") + "]",
                                     options: .caseInsensitive) else { return nil }
        for line in text[start.upperBound...].split(separator: "\n").dropFirst() {
            if line.hasPrefix("[") { break }
            let prefix = "\"\(name)\"=\""
            if line.lowercased().hasPrefix(prefix.lowercased()), line.hasSuffix("\"") {
                return String(line.dropFirst(prefix.count).dropLast())
            }
        }
        return nil
    }

    /// CrossOver's "Repair Bottle": stop everything, let wineboot rewrite system files and registry defaults, then
    /// re-apply Cider's prefix setup (fonts, isolated shell folders). A snapshot is taken first.
    @discardableResult
    public func repair(_ bottle: Bottle) throws -> Bottle {
        return try withOperation(bottle) { current in
            try snapshot(current, reason: "before repair")
            let runner = try runner(for: current)
            try runner.killAll()
            let result = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["-u"], label: "wineboot-repair"))
            try runner.killAll()
            try PrefixSetup.isolateShellFolders(in: current.driveC)
            try PrefixSetup.upgrade(bottle: current, runner: runner, from: 0)
            guard result.code == 0 else {
                throw CiderError.commandFailed(command: "wineboot -u", status: result.code, output: "")
        }
        return try update(current) { $0.settings[PrefixSetup.revisionKey] = String(PrefixSetup.revision) }
        }
    }


}
