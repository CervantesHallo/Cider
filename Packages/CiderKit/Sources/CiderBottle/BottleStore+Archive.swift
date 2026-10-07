import CiderCore
import CiderSchema
import CiderStore
import Foundation

/// A Wine prefix that belongs to another app (CrossOver, Whisky) and can be copied into Cider.
public struct ForeignBottle: Sendable, Identifiable, Equatable {
    public enum Source: String, Sendable { case crossover = "CrossOver", whisky = "Whisky" }
    public let source: Source
    public let name: String
    public let prefix: URL            // directory holding drive_c and system.reg
    public var id: String { prefix.path }
}

extension BottleStore {
    public static let archiveExtension = "ciderbottle"

    /// Writes `<name>.ciderbottle` (a zip of the bottle without its snapshots). The bottle is stopped first so
    /// the registry is on disk.
    @discardableResult
    public func exportArchive(_ bottle: Bottle, to directory: URL) throws -> URL {
        try withOperation(bottle) { current in
            let fm = FileManager.default
            try runner(for: current).killAll()
            let staging = fm.temporaryDirectory.appendingPathComponent("cider-export-" + UUID().uuidString)
            let root = staging.appendingPathComponent(current.config.id)
            try fm.ensureDirectory(staging)
            defer { try? fm.removeItem(at: staging) }
            try Command.run("/bin/cp", ["-c", "-R", current.directory.path, root.path])
            let snapshots = root.appendingPathComponent(".cider/snapshots")
            if FileSafety.exists(snapshots) { try fm.removeItem(at: snapshots) }
            try fm.ensureDirectory(directory)
            let safeName = current.config.name.replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: "\\", with: "-")
                .replacingOccurrences(of: "\n", with: " ").replacingOccurrences(of: "\r", with: " ")
            let archive = try FileSafety.child(safeName + "." + Self.archiveExtension, in: directory, rejectSymlinks: true)
            let temporary = directory.appendingPathComponent(".export-" + UUID().uuidString)
            var keepBackup = false
            defer { if !keepBackup { try? fm.removeItem(at: temporary) } }
            try Command.run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--noextattr", "--keepParent", root.path, temporary.path])
            if let backup = try FileSafety.commit(temporary, to: archive) {
                keepBackup = true
                try? fm.trashItem(at: backup, resultingItemURL: nil)
            }
            return archive
        }
    }

    /// Imported ids and root paths are validated before any commit. Startup is disabled in the copy.
    @discardableResult
    public func importArchive(_ archive: URL) throws -> Bottle {
        let fm = FileManager.default
        try fm.ensureDirectory(paths.bottles)
        let staging = paths.bottles.appendingPathComponent(".import-" + UUID().uuidString)
        try fm.ensureDirectory(staging)
        defer { try? fm.removeItem(at: staging) }
        try ArchiveExtractor.unpack(archive, to: staging)
        let roots = try fm.contentsOfDirectory(at: staging, includingPropertiesForKeys: nil)
            .filter { fm.fileExists(atPath: $0.appendingPathComponent("cider-bottle.json").path) }
        guard roots.count == 1, let root = roots.first else { throw CiderError.invalid("此归档没有唯一的 Cider 瓶子。") }
        _ = try FileSafety.child(root.lastPathComponent, in: staging, rejectSymlinks: true)
        let configFile = try FileSafety.child("cider-bottle.json", in: root, rejectSymlinks: true)
        var config = try JSONFile.read(BottleConfig.self, from: configFile)
        _ = try FileSafety.component(config.id)
        guard config.arch == "win64-wow64" else { throw CiderError.invalid("Cider 只导入 64 位 WoW64 瓶子。") }
        _ = try FileSafety.child("prefix/drive_c", in: root)
        if let reg = try? String(contentsOf: root.appendingPathComponent("prefix/system.reg"), encoding: .utf8), reg.contains("#arch=win32") {
            throw CiderError.invalid("此归档包含 32 位前缀。")
        }
        // Every import has a new exclusive identity; a crash cannot expose a half-import as a bottle.
        let destination = try FileSafety.reserveDirectory(in: paths.bottles) { Identifiers.make(from: config.name) }
        var committed = false
        defer { if !committed { try? fm.removeItem(at: destination) } }
        config.id = destination.lastPathComponent
        config.settings["cloneSteamBlocked"] = "1"
        try JSONFile.write(config, to: root.appendingPathComponent("cider-bottle.json"))
        _ = try CloneStartup.prepare(Bottle(config: config, directory: root))
        let backup = try FileSafety.commit(root, to: destination)
        committed = true
        if let backup { try? fm.removeItem(at: backup) } // owned empty reservation
        return Bottle(config: try JSONFile.read(BottleConfig.self, from: destination.appendingPathComponent("cider-bottle.json")), directory: destination)
    }

    /// CrossOver and Whisky bottles on this Mac.
    public func foreignBottles(home: URL = FileManager.default.homeDirectoryForCurrentUser) -> [ForeignBottle] {
        let fm = FileManager.default
        var found: [ForeignBottle] = []
        let crossover = home.appendingPathComponent("Library/Application Support/CrossOver/Bottles", isDirectory: true)
        for name in (try? fm.contentsOfDirectory(atPath: crossover.path)) ?? [] {
            let dir = crossover.appendingPathComponent(name, isDirectory: true)
            if Self.isPrefix(dir) { found.append(ForeignBottle(source: .crossover, name: name, prefix: dir)) }
        }
        let whisky = home.appendingPathComponent("Library/Containers/com.isaacmarovitz.Whisky/Bottles", isDirectory: true)
        for uuid in (try? fm.contentsOfDirectory(atPath: whisky.path)) ?? [] {
            let dir = whisky.appendingPathComponent(uuid, isDirectory: true)
            guard Self.isPrefix(dir) else { continue }
            let plist = NSDictionary(contentsOf: dir.appendingPathComponent("Metadata.plist"))
            let name = (plist?["name"] as? String) ?? uuid
            found.append(ForeignBottle(source: .whisky, name: name, prefix: dir))
        }
        return found
    }

    static func isPrefix(_ dir: URL) -> Bool {
        let fm = FileManager.default
        return fm.fileExists(atPath: dir.appendingPathComponent("drive_c").path)
            && fm.fileExists(atPath: dir.appendingPathComponent("system.reg").path)
    }

    /// Copies a CrossOver/Whisky prefix into a new Cider bottle (APFS clone; the original is left untouched),
    /// then applies Cider's own prefix setup. Only 64-bit prefixes can be imported (ADR-001).
    @discardableResult
    public func importForeign(_ foreign: ForeignBottle, locale: BottleLocale, engineID: String? = nil,
                              progress: (String) -> Void = { _ in }) throws -> Bottle {
        let fm = FileManager.default
        if let reg = try? String(contentsOf: foreign.prefix.appendingPathComponent("system.reg"), encoding: .utf8),
           reg.contains("#arch=win32") {
            throw CiderError.invalid("“\(foreign.name)”是 32 位瓶子；Cider 只支持 64 位瓶子（32 位程序在其中运行）。请在 Cider 里新建瓶子后重新安装该程序。")
        }
        let engine = try engineID.map { try engines.engine($0) } ?? engines.defaultEngine()
        guard !PrefixServer.isRunning(prefix: foreign.prefix) else { throw CiderError.invalid("原瓶子仍在运行，请先停止原瓶子后再导入。") }
        let dir = try FileSafety.reserveDirectory(in: paths.bottles) { Identifiers.make(from: foreign.name) }
        let id = dir.lastPathComponent
        do {
            progress("copy")
            try Command.run("/bin/cp", ["-c", "-R", foreign.prefix.path, dir.appendingPathComponent("prefix").path])
            var config = BottleConfig(id: id, name: foreign.name, createdBy: "import:\(foreign.source.rawValue)",
                                      createdAt: Identifiers.timestamp(), template: .win10_64,
                                      engine: .init(id: engine.manifest.id), locale: locale)
            config.integration = .init(shellFolders: "isolated", zDrive: true)
            config.settings["cloneSteamBlocked"] = "1"
            let bottle = Bottle(config: config, directory: dir)
            try JSONFile.write(config, to: bottle.configURL)
            return try withOperation(bottle) { _ in
            progress("setup")
            // Foreign apps' own files inside the prefix stay; their launch scripts and bottle configs do not apply.
            for leftover in ["cxbottle.conf", "cxmenu.conf", "Metadata.plist", "Program Settings.plist"] {
                try? fm.removeItem(at: bottle.prefix.appendingPathComponent(leftover))
            }
            let prepared = try CloneStartup.prepare(bottle)
            try PrefixSetup.isolateShellFolders(in: prepared.driveC)
            let runner = try runner(for: prepared)
            let boot = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["-u"], label: "wineboot-import"))
            try runner.killAll()
            guard boot.code == 0 else { throw CiderError.commandFailed(command: "wineboot -u", status: boot.code, output: "") }
                return try upgradePrefix(prepared)
            }
        } catch {
            let partial = try? JSONFile.read(BottleConfig.self, from: dir.appendingPathComponent("cider-bottle.json"))
            if let partial { try runner(for: Bottle(config: partial, directory: dir)).killAll() }
            try fm.removeItem(at: dir)
            throw error
        }
    }
}
