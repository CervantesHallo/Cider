import CiderCore
import CiderStore
import CryptoKit
import Foundation

/// Staged file changes with durable originals and recovery records.
public struct PatchInstaller: Sendable {
    public let gameRoot: URL
    public let historyRoot: URL
    public let stateRoot: URL
    public let bottleID: String?
    public let prefix: URL?
    public init(gameRoot: URL, historyRoot: URL, stateRoot: URL = CiderPaths.standard().state, bottleID: String? = nil, prefix: URL? = nil) {
        self.gameRoot = gameRoot; self.historyRoot = historyRoot
        self.stateRoot = stateRoot; self.bottleID = bottleID; self.prefix = prefix
    }
    public struct Manifest: Codable, Sendable {
        public var installedAt: String
        public var sources: [String]
        public var added: [String]
        public var replaced: [String]
        public var sequence: UInt64? = nil
    }
    private struct Entry: Codable { var relative: String; var existed: Bool; var expectedHash: String? }
    private struct Journal: Codable {
        var operation: String
        var state: String
        var entries: [Entry]
        var sourceRecord: String? = nil
    }
    public enum PatchError: Error, CustomStringConvertible {
        case unsupportedArchive(String), nothingToInstall, recoveryRequired(String)
        public var description: String {
            switch self {
            case .unsupportedArchive(let name): return "\(name) 需要先解压后再安装。"
            case .nothingToInstall: return "没有可以复制的文件。"
            case .recoveryRequired(let path): return "补丁操作未完成，恢复记录保留在：\(path)"
            }
        }
    }
    private func locked<T>(_ body: () throws -> T) throws -> T {
        try withoutActuallyEscaping(body) { body in
            let digest = SHA256.hash(data: Data(historyRoot.standardizedFileURL.path.utf8)).map { String(format: "%02x", $0) }.joined()
            let work = {
                try FileOperationLock.withLock(at: stateRoot.appendingPathComponent("locks/patches/" + digest + ".lock")) {
                    if let prefix, PrefixServer.isRunning(prefix: prefix) || PrefixServer.hasProcesses(prefix: prefix, bottleID: bottleID ?? "") {
                        throw CiderError.invalid("此瓶子仍有进程运行，请先停止瓶子再修改补丁。")
                    }
                    if let prefix {
                        let directory = prefix.deletingLastPathComponent()
                        let relative = String(historyRoot.path.dropFirst(directory.path.count + 1))
                        guard historyRoot.path.hasPrefix(directory.path + "/") else { throw CiderError.invalid("补丁历史不属于此瓶子。") }
                        _ = try FileSafety.child(relative, in: directory, rejectSymlinks: true)
                    }
                    return try body()
                }
            }
            if let bottleID {
                _ = try FileSafety.component(bottleID)
                return try FileOperationLock.withLock(at: stateRoot.appendingPathComponent("locks/bottles/" + bottleID + ".lock"), work)
            }
        return try work()
        }
    }
    private func nextSequence() throws -> UInt64 {
        let file = try path("sequence.json", in: historyRoot)
        let previous = FileSafety.exists(file) ? try JSONFile.readMetadata(UInt64.self, from: file) : 0
        let maximum = try metadataRecords().compactMap { $0.manifest?.sequence }.max() ?? 0
        guard max(previous, maximum) < UInt64.max else { throw CiderError.invalid("补丁提交序号已耗尽。") }
        let next = max(previous, maximum) + 1
        try JSONFile.writeMetadata(next, to: file)
        return next
    }
    private func path(_ relative: String, in root: URL) throws -> URL {
        try FileSafety.child(relative, in: root, rejectSymlinks: true)
    }
    private func read<T: Decodable>(_ type: T.Type, name: String, record: URL) throws -> T {
        try JSONFile.readMetadata(type, from: path(name, in: record))
    }
    private func existingJournal(record: URL) throws -> Journal? {
        let file = try path("journal.json", in: record)
        return FileSafety.exists(file) ? try JSONFile.readMetadata(Journal.self, from: file) : nil
    }
    private func metadataRecords() throws -> [(record: URL, manifest: Manifest?, journal: Journal?)] {
        let fm = FileManager.default
        guard FileSafety.exists(historyRoot) else { return [] }
        var result: [(record: URL, manifest: Manifest?, journal: Journal?)] = []
        for name in try fm.contentsOfDirectory(atPath: historyRoot.path) where !name.hasPrefix(".") && name != "sequence.json" {
            let record = try path(name, in: historyRoot)
            guard (try fm.attributesOfItem(atPath: record.path)[.type] as? FileAttributeType) == .typeDirectory else {
                throw CiderError.invalid("补丁恢复目录损坏。")
            }
            let manifestURL = try path("manifest.json", in: record)
            let journalURL = try path("journal.json", in: record)
            let manifest = FileSafety.exists(manifestURL) ? try read(Manifest.self, name: "manifest.json", record: record) : nil
            let journal = FileSafety.exists(journalURL) ? try read(Journal.self, name: "journal.json", record: record) : nil
            result.append((record, manifest, journal))
        }
        return result
    }
    private func regular(_ url: URL) throws {
        guard (try FileManager.default.attributesOfItem(atPath: url.path)[.type] as? FileAttributeType) == .typeRegular else {
            throw CiderError.invalid("补丁仅支持普通文件：\(url.lastPathComponent)")
        }
    }
    private func copyAtomically(_ source: URL, to destination: URL) throws {
        let fm = FileManager.default
        try regular(source)
        try fm.ensureDirectory(destination.deletingLastPathComponent())
        let temporary = destination.deletingLastPathComponent().appendingPathComponent(".patch-" + UUID().uuidString)
        defer { try? fm.removeItem(at: temporary) }
        try fm.copyItem(at: source, to: temporary)
        _ = try FileSafety.commit(temporary, to: destination) // original is already backed up
    }
    private func validate(_ manifest: Manifest, record: URL) throws {
        guard Set(manifest.added + manifest.replaced).count == manifest.added.count + manifest.replaced.count else {
            throw CiderError.invalid("补丁清单包含重复路径。")
        }
        for relative in manifest.added + manifest.replaced { _ = try path(relative, in: gameRoot) }
        for relative in manifest.replaced { try regular(path("backup/" + relative, in: record)) }
    }
    private func backup(_ entries: [Entry], into record: URL) throws {
        for entry in entries where entry.existed {
            let source = try path(entry.relative, in: gameRoot)
            try regular(source)
            let destination = try path("rollback/" + entry.relative, in: record)
            try FileManager.default.ensureDirectory(destination.deletingLastPathComponent())
            try FileManager.default.copyItem(at: source, to: destination)
        }
    }
    private func rollback(_ journal: Journal, record: URL) throws {
        for entry in journal.entries {
            _ = try path(entry.relative, in: gameRoot)
            if entry.existed { try regular(path("rollback/" + entry.relative, in: record)) }
        }
        for entry in journal.entries.reversed() {
            let destination = try path(entry.relative, in: gameRoot)
            if FileSafety.exists(destination) {
                try regular(destination)
                let hash = try EngineStore.sha256(of: destination)
                let original = entry.existed ? try EngineStore.sha256(of: path("rollback/" + entry.relative, in: record)) : nil
                if hash != entry.expectedHash && hash != original {
                    let conflict = try path("conflicts/" + UUID().uuidString + "/" + entry.relative, in: record)
                    try FileManager.default.ensureDirectory(conflict.deletingLastPathComponent())
                    try FileManager.default.copyItem(at: destination, to: conflict)
                }
            }
            if entry.existed { try copyAtomically(path("rollback/" + entry.relative, in: record), to: destination) }
            else if FileSafety.exists(destination) { try FileManager.default.removeItem(at: destination) }
        }
        if journal.operation == "undo", let source = journal.sourceRecord {
            _ = try FileSafety.component(source)
            let installedRecord = try path(source, in: historyRoot)
            var installed = (try existingJournal(record: installedRecord))
                ?? Journal(operation: "install", state: "installed", entries: [])
            installed.state = "installed"
            try JSONFile.writeMetadata(installed, to: installedRecord.appendingPathComponent("journal.json"))
        }
        var completed = journal; completed.state = "rolled_back"
        try JSONFile.writeMetadata(completed, to: record.appendingPathComponent("journal.json"))
    }
    public func recoverPending() throws { try locked { try recoverLocked() } }
    private func recoverLocked() throws {
        for entry in try metadataRecords() {
            if let journal = entry.journal, journal.state == "mutating" { try rollback(journal, record: entry.record) }
        }
    }
    @discardableResult
    public func install(_ items: [URL]) throws -> Manifest {
        try locked {
            try recoverLocked()
            let fm = FileManager.default
            let record = try FileSafety.reserveDirectory(in: historyRoot) { Identifiers.compactTimestamp() + "-" + UUID().uuidString }
            let staging = record.appendingPathComponent("staging")
            try fm.ensureDirectory(staging)
            var mutation: Journal?
            do {
                for item in items {
                    switch item.pathExtension.lowercased() {
                    case "7z", "rar": throw PatchError.unsupportedArchive(item.lastPathComponent)
                    case "zip":
                        let extraction = record.appendingPathComponent("extract-" + UUID().uuidString)
                        try fm.ensureDirectory(extraction)
                        try ArchiveExtractor.unpack(item, to: extraction)
                        let names = try fm.contentsOfDirectory(atPath: extraction.path).filter { !$0.hasPrefix(".") && $0 != "__MACOSX" }
                        var source = extraction
                        if names.count == 1 {
                            let top = try path(names[0], in: extraction)
                            if (try top.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true { source = top }
                        }
                        for name in try fm.contentsOfDirectory(atPath: source.path) where !name.hasPrefix(".") && name != "__MACOSX" {
                            try fm.moveItem(at: path(name, in: source), to: path(name, in: staging))
                        }
                    default: try fm.copyItem(at: item, to: path(item.lastPathComponent, in: staging))
                    }
                }
                var files: [String] = []
                guard let walker = fm.enumerator(at: staging, includingPropertiesForKeys: [.isDirectoryKey]) else { throw PatchError.nothingToInstall }
                for case let file as URL in walker {
                    let relative = String(file.path.dropFirst(staging.path.count + 1))
                    _ = try path(relative, in: staging)
                    if (try file.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true { continue }
                    if relative.split(separator: "/").contains(where: { $0.hasPrefix(".") }) { continue }
                    try regular(file); files.append(relative)
                }
                guard !files.isEmpty else { throw PatchError.nothingToInstall }
                let entries = try files.sorted().map { relative -> Entry in
                    let destination = try path(relative, in: gameRoot)
                    if FileSafety.exists(destination) { try regular(destination) }
                    return Entry(relative: relative, existed: FileSafety.exists(destination), expectedHash: try EngineStore.sha256(of: path(relative, in: staging)))
                }
                try backup(entries, into: record)
                for entry in entries where entry.existed {
                    let destination = try path("backup/" + entry.relative, in: record)
                    try fm.ensureDirectory(destination.deletingLastPathComponent())
                    try fm.copyItem(at: path("rollback/" + entry.relative, in: record), to: destination)
                }
                let journal = Journal(operation: "install", state: "mutating", entries: entries)
                try JSONFile.writeMetadata(journal, to: record.appendingPathComponent("journal.json")); mutation = journal
                for entry in entries { try copyAtomically(path(entry.relative, in: staging), to: path(entry.relative, in: gameRoot)) }
                let manifest = Manifest(installedAt: Identifiers.timestamp(), sources: items.map(\.lastPathComponent),
                                        added: entries.filter { !$0.existed }.map(\.relative), replaced: entries.filter(\.existed).map(\.relative), sequence: try nextSequence())
                try JSONFile.writeMetadata(manifest, to: record.appendingPathComponent("manifest.json"))
                var completed = journal; completed.state = "installed"
                try JSONFile.writeMetadata(completed, to: record.appendingPathComponent("journal.json"))
                try? fm.removeItem(at: staging)
                return manifest
            } catch {
                if let mutation {
                    do { try rollback(mutation, record: record) }
                    catch { throw PatchError.recoveryRequired(record.path) }
                } else { try? fm.removeItem(at: record) }
                throw error
            }
        }
    }
    private func latestInstall() throws -> (record: URL, manifest: Manifest)? {
        let records = try metadataRecords().filter { $0.manifest != nil && ($0.journal == nil || $0.journal?.state == "installed") }.sorted { a, b in
            let left = a.manifest!, right = b.manifest!
            if left.sequence != right.sequence { return (left.sequence ?? 0) > (right.sequence ?? 0) }
            return a.record.lastPathComponent > b.record.lastPathComponent
        }
        guard let first = records.first, let manifest = first.manifest else { return nil }
        if manifest.sequence == nil && records.dropFirst().contains(where: { $0.manifest?.sequence == nil && $0.manifest?.installedAt == manifest.installedAt }) {
            throw CiderError.invalid("旧补丁记录的先后顺序不明确，请保留记录并人工复核后恢复。")
        }
        try validate(manifest, record: first.record)
        return (first.record, manifest)
    }
    public func lastInstall() -> (record: URL, manifest: Manifest)? { try? latestInstall() }
    @discardableResult
    public func undoLast() throws -> Manifest? {
        try locked {
            try recoverLocked()
            guard let (record, manifest) = try latestInstall() else { return nil }
            try validate(manifest, record: record)
            let recovery = try FileSafety.reserveDirectory(in: historyRoot) { "undo-" + UUID().uuidString }
            let entries = try (manifest.added + manifest.replaced).map { relative -> Entry in
                let destination = try path(relative, in: gameRoot)
                if FileSafety.exists(destination) { try regular(destination) }
                let expected = manifest.replaced.contains(relative) ? try EngineStore.sha256(of: path("backup/" + relative, in: record)) : nil
                return Entry(relative: relative, existed: FileSafety.exists(destination), expectedHash: expected)
            }
            try backup(entries, into: recovery)
            let journal = Journal(operation: "undo", state: "mutating", entries: entries, sourceRecord: record.lastPathComponent)
            try JSONFile.writeMetadata(journal, to: recovery.appendingPathComponent("journal.json"))
            do {
                for relative in manifest.added {
                    let destination = try path(relative, in: gameRoot)
                    if FileSafety.exists(destination) { try FileManager.default.removeItem(at: destination) }
                }
                for relative in manifest.replaced { try copyAtomically(path("backup/" + relative, in: record), to: path(relative, in: gameRoot)) }
                var installed = (try existingJournal(record: record))
                    ?? Journal(operation: "install", state: "installed", entries: [])
                installed.state = "undone"
                try JSONFile.writeMetadata(installed, to: record.appendingPathComponent("journal.json"))
                var completed = journal; completed.state = "undone"
                try JSONFile.writeMetadata(completed, to: recovery.appendingPathComponent("journal.json"))
                return manifest
            } catch {
                do { try rollback(journal, record: recovery) }
                catch { throw PatchError.recoveryRequired(recovery.path) }
                throw error
            }
        }
    }
}
