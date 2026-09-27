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
        let fm = FileManager.default
        try? runner(for: bottle).killAll()
        let staging = fm.temporaryDirectory.appendingPathComponent("cider-export-\(UUID().uuidString)", isDirectory: true)
        let root = staging.appendingPathComponent(bottle.config.id, isDirectory: true)
        try fm.ensureDirectory(staging)
        defer { try? fm.removeItem(at: staging) }
        try Command.run("/bin/cp", ["-c", "-R", bottle.directory.path, root.path])   // APFS clone: instant
        try? fm.removeItem(at: root.appendingPathComponent(".cider/snapshots"))
        try fm.ensureDirectory(directory)
        let safeName = bottle.config.name.replacingOccurrences(of: "/", with: "-")
        let archive = directory.appendingPathComponent("\(safeName).\(Self.archiveExtension)")
        try? fm.removeItem(at: archive)
        try Command.run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--noextattr", "--keepParent", root.path, archive.path])
        return archive
    }

    /// Imports a `.ciderbottle`. A bottle with the same id gets a fresh id (and " (导入)" appended to its name).
    @discardableResult
    public func importArchive(_ archive: URL) throws -> Bottle {
        let fm = FileManager.default
        let staging = paths.bottles.appendingPathComponent(".import-\(UUID().uuidString)", isDirectory: true)
        try fm.ensureDirectory(staging)
        defer { try? fm.removeItem(at: staging) }
        try Command.run("/usr/bin/ditto", ["-x", "-k", archive.path, staging.path])
        guard let root = try fm.contentsOfDirectory(at: staging, includingPropertiesForKeys: nil)
            .first(where: { fm.fileExists(atPath: $0.appendingPathComponent("cider-bottle.json").path) }) else {
            throw CiderError.invalid("\(archive.lastPathComponent) is not a Cider bottle archive")
        }
        var config = try JSONFile.read(BottleConfig.self, from: root.appendingPathComponent("cider-bottle.json"))
        var destination = paths.bottles.appendingPathComponent(config.id, isDirectory: true)
        if fm.fileExists(atPath: destination.path) {
            config.id = Identifiers.make(from: config.name)
            config.name += " (导入)"
            destination = paths.bottles.appendingPathComponent(config.id, isDirectory: true)
            try JSONFile.write(config, to: root.appendingPathComponent("cider-bottle.json"))
        }
        try fm.moveItem(at: root, to: destination)
        return Bottle(config: config, directory: destination)
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
        let id = Identifiers.make(from: foreign.name)
        let dir = paths.bottles.appendingPathComponent(id, isDirectory: true)
        try fm.ensureDirectory(dir)
        do {
            progress("copy")
            try Command.run("/bin/cp", ["-c", "-R", foreign.prefix.path, dir.appendingPathComponent("prefix").path])
            var config = BottleConfig(id: id, name: foreign.name, createdBy: "import:\(foreign.source.rawValue)",
                                      createdAt: Identifiers.timestamp(), template: .win10_64,
                                      engine: .init(id: engine.manifest.id), locale: locale)
            config.integration = .init(shellFolders: "isolated", zDrive: true)
            let bottle = Bottle(config: config, directory: dir)
            try JSONFile.write(config, to: bottle.configURL)
            progress("setup")
            // Foreign apps' own files inside the prefix stay; their launch scripts and bottle configs do not apply.
            for leftover in ["cxbottle.conf", "cxmenu.conf", "Metadata.plist", "Program Settings.plist"] {
                try? fm.removeItem(at: bottle.prefix.appendingPathComponent(leftover))
            }
            try PrefixSetup.isolateShellFolders(in: bottle.driveC)
            let runner = try runner(for: bottle)
            let boot = try runner.runToCompletion(runner.plan(program: "wineboot", arguments: ["-u"], label: "wineboot-import"))
            try? runner.killAll()
            guard boot.code == 0 else { throw CiderError.commandFailed(command: "wineboot -u", status: boot.code, output: "") }
            return try upgradePrefix(Bottle(config: config, directory: dir))
        } catch {
            try? fm.removeItem(at: dir)
            throw error
        }
    }
}
