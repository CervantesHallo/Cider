import CiderCore
import CiderStore
import Foundation

/// Copies user-supplied files (e.g. a galgame's restoration patch like `patch.xp3`) into a game's root folder.
/// Every file that would be overwritten is moved to a backup first, and each install writes a manifest, so the
/// most recent patch can be undone exactly (added files removed, replaced files restored).
public struct PatchInstaller: Sendable {
    public let gameRoot: URL
    /// `<bottle>/.cider/patches/<app-key>/` — one subfolder per install.
    public let historyRoot: URL

    public init(gameRoot: URL, historyRoot: URL) {
        self.gameRoot = gameRoot
        self.historyRoot = historyRoot
    }

    public struct Manifest: Codable, Sendable {
        public var installedAt: String
        public var sources: [String]
        /// Paths relative to the game root.
        public var added: [String]
        public var replaced: [String]
    }

    public enum PatchError: Error, CustomStringConvertible {
        case unsupportedArchive(String)
        case nothingToInstall
        public var description: String {
            switch self {
            case .unsupportedArchive(let name): return "\(name) 需要先解压（.7z / .rar 可用「归档实用工具」或 The Unarchiver），再拖入解压后的文件。"
            case .nothingToInstall: return "没有可以复制的文件。"
            }
        }
    }

    /// Installs dropped items. `.zip` files are extracted (their contents land in the game root); folders are merged.
    /// A zip whose content is a single top-level folder is unwrapped, since patches are often zipped that way.
    @discardableResult
    public func install(_ items: [URL]) throws -> Manifest {
        let fm = FileManager.default
        let stamp = Identifiers.compactTimestamp()
        let record = historyRoot.appendingPathComponent(stamp, isDirectory: true)
        let backup = record.appendingPathComponent("backup", isDirectory: true)
        let staging = record.appendingPathComponent("staging", isDirectory: true)
        try fm.ensureDirectory(staging)
        defer { try? fm.removeItem(at: staging) }

        // 1. Stage everything so a bad archive fails before the game folder is touched.
        for item in items {
            switch item.pathExtension.lowercased() {
            case "7z", "rar":
                throw PatchError.unsupportedArchive(item.lastPathComponent)
            case "zip":
                let out = staging.appendingPathComponent(UUID().uuidString, isDirectory: true)
                try Command.run("/usr/bin/ditto", ["-x", "-k", item.path, out.path])
                var root = out
                let top = try fm.contentsOfDirectory(atPath: out.path).filter { !$0.hasPrefix(".") && $0 != "__MACOSX" }
                var isDir: ObjCBool = false
                if top.count == 1, fm.fileExists(atPath: out.appendingPathComponent(top[0]).path, isDirectory: &isDir), isDir.boolValue {
                    root = out.appendingPathComponent(top[0])
                }
                for name in try fm.contentsOfDirectory(atPath: root.path) where !name.hasPrefix(".") && name != "__MACOSX" {
                    try fm.moveItem(at: root.appendingPathComponent(name), to: staging.appendingPathComponent(name))
                }
                try? fm.removeItem(at: out)
            default:
                try fm.copyItem(at: item, to: staging.appendingPathComponent(item.lastPathComponent))
            }
        }

        // 2. Merge the staged tree into the game root, backing up anything it replaces.
        var added: [String] = []
        var replaced: [String] = []
        guard let walker = fm.enumerator(at: staging, includingPropertiesForKeys: [.isDirectoryKey]) else { throw PatchError.nothingToInstall }
        for case let file as URL in walker {
            guard (try? file.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) != true else { continue }
            let relative = String(file.standardizedFileURL.path.dropFirst(staging.standardizedFileURL.path.count + 1))
            guard !relative.hasPrefix(".") , !relative.contains("/.") else { continue }
            let destination = gameRoot.appendingPathComponent(relative)
            try fm.ensureDirectory(destination.deletingLastPathComponent())
            if fm.fileExists(atPath: destination.path) {
                let saved = backup.appendingPathComponent(relative)
                try fm.ensureDirectory(saved.deletingLastPathComponent())
                try fm.moveItem(at: destination, to: saved)
                replaced.append(relative)
            } else {
                added.append(relative)
            }
            try fm.copyItem(at: file, to: destination)
        }
        guard !added.isEmpty || !replaced.isEmpty else { throw PatchError.nothingToInstall }
        let manifest = Manifest(installedAt: Identifiers.timestamp(), sources: items.map(\.lastPathComponent), added: added, replaced: replaced)
        try JSONFile.write(manifest, to: record.appendingPathComponent("manifest.json"))
        return manifest
    }

    /// The most recent install, if any.
    public func lastInstall() -> (record: URL, manifest: Manifest)? {
        let fm = FileManager.default
        guard let records = try? fm.contentsOfDirectory(atPath: historyRoot.path) else { return nil }
        for name in records.sorted(by: >) {
            let record = historyRoot.appendingPathComponent(name, isDirectory: true)
            if let manifest = try? JSONFile.read(Manifest.self, from: record.appendingPathComponent("manifest.json")) {
                return (record, manifest)
            }
        }
        return nil
    }

    /// Reverts the most recent install: removes the files it added and restores the ones it replaced.
    @discardableResult
    public func undoLast() throws -> Manifest? {
        guard let (record, manifest) = lastInstall() else { return nil }
        let fm = FileManager.default
        for relative in manifest.added { try? fm.removeItem(at: gameRoot.appendingPathComponent(relative)) }
        for relative in manifest.replaced {
            let destination = gameRoot.appendingPathComponent(relative)
            try? fm.removeItem(at: destination)
            try fm.moveItem(at: record.appendingPathComponent("backup/\(relative)"), to: destination)
        }
        try fm.removeItem(at: record)
        return manifest
    }
}
