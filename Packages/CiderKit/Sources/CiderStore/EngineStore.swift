import CiderCore
import CiderSchema
import CryptoKit
import Foundation

/// An engine unpacked under `Engines/<id>/`.
public struct InstalledEngine: Sendable {
    public let manifest: EngineManifest
    public let directory: URL

    public var wineRoot: URL { directory.appendingPathComponent(manifest.root, isDirectory: true) }
    public var bin: URL { wineRoot.appendingPathComponent("bin", isDirectory: true) }

    /// `CiderWineHost.app/Contents/MacOS` when the host bundle exists, else the engine's plain `bin/`.
    public var loaderDirectory: URL {
        let host = EngineHost.bundleURL(in: directory).appendingPathComponent("Contents/MacOS", isDirectory: true)
        return FileManager.default.isExecutableFile(atPath: host.appendingPathComponent("wine").path) ? host : bin
    }

    /// Whether the engine ships its own libgstreamer (then the system GStreamer.framework must not be mixed in).
    /// Cider's layout (`engine/bundle-gstreamer.sh`): the framework's lib/ and libexec/ under frameworks/gstreamer.
    public var bundledGStreamer: URL? {
        let dir = directory.appendingPathComponent("frameworks/gstreamer", isDirectory: true)
        return FileManager.default.fileExists(atPath: dir.appendingPathComponent("lib/libgstreamer-1.0.0.dylib").path) ? dir : nil
    }

    public var bundlesGStreamer: Bool {
        if bundledGStreamer != nil { return true }
        // Other runtimes (e.g. Whisky-style) put the libraries next to Wine's own.
        let lib = wineRoot.appendingPathComponent("lib", isDirectory: true)
        return ((try? FileManager.default.contentsOfDirectory(atPath: lib.path)) ?? []).contains { $0.hasPrefix("libgstreamer-1.0") }
    }

    public var wine: URL { loaderDirectory.appendingPathComponent("wine") }
    public var wineserver: URL { loaderDirectory.appendingPathComponent("wineserver") }
}

/// Installs and lists engines. v0 accepts WineHQ-style macOS tarballs (e.g. Gcenx `wine-devel-*-osx64.tar.xz`);
/// Cider's own signed engine packages (manifest + files.sha256, docs/plan/01 §5) reuse the same staging/rename flow.
public struct EngineStore: Sendable {
    public let paths: CiderPaths

    public init(paths: CiderPaths) { self.paths = paths }

    public func list() throws -> [InstalledEngine] {
        let fm = FileManager.default
        guard fm.fileExists(atPath: paths.engines.path) else { return [] }
        return try fm.contentsOfDirectory(at: paths.engines, includingPropertiesForKeys: nil)
            .filter { !$0.lastPathComponent.hasPrefix(".") }
            .compactMap { dir in
                let manifestURL = dir.appendingPathComponent("manifest.json")
                guard fm.fileExists(atPath: manifestURL.path) else { return nil }
                let manifest = try JSONFile.read(EngineManifest.self, from: manifestURL)
                guard manifest.id == dir.lastPathComponent else { throw CiderError.invalid("引擎标识与目录不一致。") }
                try Self.validate(manifest, in: dir)
                return InstalledEngine(manifest: manifest, directory: dir)
            }
            .sorted { $0.manifest.id < $1.manifest.id }
    }

    public func engine(_ id: String) throws -> InstalledEngine {
        guard let engine = try list().first(where: { $0.manifest.id == id }) else {
            throw CiderError.notFound("engine \(id)")
        }
        return engine
    }

    /// The engine new bottles use when none is given: the newest installed one.
    public func defaultEngine() throws -> InstalledEngine {
        let available = try list()
        if let published = available.first(where: { $0.manifest.id == "cider-cx26.3-r1-x86_64" }),
           (try? EnginePolicy.supportsChildPreflight(published)) == true { return published }
        if let verified = available.reversed().first(where: { (try? EnginePolicy.supportsChildPreflight($0)) == true }) { return verified }
        guard let engine = available.last else {
            throw CiderError.notFound("no engine installed — run `ciderctl engine install <package>`")
        }
        return engine
    }

    /// Unpacks `package` into a staging directory, locates the Wine tree, smoke-tests it and renames it into place.
    @discardableResult
    public func install(package: URL, id requestedID: String? = nil, channel: String = "devel", origin: String? = nil, tree requestedTree: String? = nil) throws -> InstalledEngine {
        let fm = FileManager.default
        try fm.ensureDirectory(paths.engines)
        let sha = try Self.sha256(of: package)

        let staging = paths.engines.appendingPathComponent(".staging-\(UUID().uuidString)", isDirectory: true)
        try fm.ensureDirectory(staging)
        defer { try? fm.removeItem(at: staging) }

        try ArchiveExtractor.unpack(package, to: staging)
        // Files fetched by a browser carry quarantine; engines are verified by hash, not Gatekeeper.
        _ = try? Command.run("/usr/bin/xattr", ["-dr", "com.apple.quarantine", staging.path])

        // Cider's own engine packages (engine/build.sh) carry their manifest: keep it as is.
        if let built = try fm.contentsOfDirectory(at: staging, includingPropertiesForKeys: nil)
            .first(where: { fm.fileExists(atPath: $0.appendingPathComponent("manifest.json").path) }),
           var manifest = try? JSONFile.read(EngineManifest.self, from: built.appendingPathComponent("manifest.json")) {
            manifest.source = .init(origin: origin ?? package.path, sha256: sha)
            try JSONFile.write(manifest, to: built.appendingPathComponent("manifest.json"))
            return try installBuilt(directory: built)
        }

        guard let wineBin = Self.findWineBinary(in: staging) else {
            throw CiderError.invalid("no bin/wine found in \(package.lastPathComponent)")
        }
        let wineRoot = wineBin.deletingLastPathComponent().deletingLastPathComponent()
        let rootRelative = String(wineRoot.path.dropFirst(staging.path.count + 1))

        // Import inspects bytes only. An unknown native loader must never execute merely to identify itself.
        let pattern = #"(?:wine[-_](?:devel[-_]|staging[-_])?)([0-9]+\.[0-9]+(?:\.[0-9]+)?)"#
        let regex = try NSRegularExpression(pattern: pattern)
        let name = package.lastPathComponent
        let range = NSRange(name.startIndex..., in: name)
        let hinted = regex.firstMatch(in: name, range: range).flatMap { Range($0.range(at: 1), in: name) }.map { String(name[$0]) }
        let version = hinted.map { "wine-" + $0 } ?? "wine-unverified"

        let tree = requestedTree ?? (package.lastPathComponent.contains("staging") ? "upstream-staging" : "upstream-devel")
        let id = requestedID ?? "imported-" + UUID().uuidString.lowercased()
        _ = try FileSafety.component(id)
        let destination = try FileSafety.child(id, in: paths.engines, rejectSymlinks: true)
        if fm.fileExists(atPath: destination.path) { throw CiderError.alreadyExists("engine \(id)") }

        let manifest = EngineManifest(
            id: id, channel: channel,
            wine: .init(version: version, tree: tree),
            root: rootRelative,
            requires: .init(macos: "14.0", rosetta: true),
            source: .init(origin: origin ?? package.path, sha256: sha)
        )
        try JSONFile.write(manifest, to: staging.appendingPathComponent("manifest.json"))
        return try installBuilt(directory: staging)
    }

    /// Installs an engine directory that already carries its manifest (the output of `engine/build.sh`):
    /// APFS-clones it into place, keeping the manifest's library paths, then builds the host bundle.
    @discardableResult
    public func installBuilt(directory: URL, replacing: Bool = false) throws -> InstalledEngine {
        let fm = FileManager.default
        let manifest = try JSONFile.read(EngineManifest.self, from: directory.appendingPathComponent("manifest.json"))
        _ = try FileSafety.component(manifest.id)
        try Self.validate(manifest, in: directory)
        try fm.ensureDirectory(paths.engines)
        return try FileOperationLock.withLock(at: operationLock(for: manifest.id)) {
            let destination = try FileSafety.child(manifest.id, in: paths.engines, rejectSymlinks: true)
            if FileSafety.exists(destination) && !replacing { throw CiderError.alreadyExists("engine \(manifest.id)") }
            if FileSafety.exists(destination) { try requireIdleBottles(using: manifest.id) }
            let staging = paths.engines.appendingPathComponent(".install-" + UUID().uuidString)
            var keepBackup = false
            defer { if !keepBackup { try? fm.removeItem(at: staging) } }
            try Command.run("/bin/cp", ["-c", "-R", directory.path, staging.path])
            try Self.validate(manifest, in: staging)
            let staged = InstalledEngine(manifest: manifest, directory: staging)
            guard fm.isExecutableFile(atPath: staged.bin.appendingPathComponent("wine").path) else {
                throw CiderError.invalid("引擎缺少 Wine 加载器。")
            }
            try EngineHost.ensure(engineDirectory: staging, wineRoot: staged.wineRoot, cpuBackend: manifest.cpuBackend)
            if let backup = try FileSafety.commit(staging, to: destination) {
                keepBackup = true
                // If Trash is unavailable, retain the hidden old engine instead of reporting a false failure.
                try? fm.trashItem(at: backup, resultingItemURL: nil)
            }
            return InstalledEngine(manifest: manifest, directory: destination)
        }
    }

    public func operationLock(for id: String) -> URL {
        paths.state.appendingPathComponent("locks/engines/" + id + ".lock")
    }

    private static func validate(_ manifest: EngineManifest, in directory: URL) throws {
        _ = try FileSafety.component(manifest.id)
        _ = try FileSafety.relativePath(manifest.root)
        _ = try FileSafety.child(manifest.root, in: directory)
        for path in manifest.libraryPaths ?? [] { _ = try FileSafety.child(path, in: directory) }
    }

    private func requireIdleBottles(using id: String) throws {
        let fm = FileManager.default
        guard FileSafety.exists(paths.bottles) else { return }
        for directory in try fm.contentsOfDirectory(at: paths.bottles, includingPropertiesForKeys: nil) {
            guard !directory.lastPathComponent.hasPrefix(".") else { continue }
            let file = directory.appendingPathComponent("cider-bottle.json")
            guard FileSafety.exists(file) else { continue }
            let config = try JSONFile.read(BottleConfig.self, from: file)
            if config.engine.id == id && (PrefixServer.isRunning(prefix: directory.appendingPathComponent("prefix"))
                || PrefixServer.hasProcesses(prefix: directory.appendingPathComponent("prefix"), bottleID: config.id)) {
                throw CiderError.invalid("这个引擎仍有瓶子运行，请先停止后再替换。")
            }
        }
    }

    /// Creates or repairs the host bundle of an installed engine.
    public func ensureHost(for engine: InstalledEngine) throws {
        try EngineHost.ensure(engineDirectory: engine.directory, wineRoot: engine.wineRoot, cpuBackend: engine.manifest.cpuBackend)
    }

    static func findWineBinary(in root: URL) -> URL? {
        let fm = FileManager.default
        guard let walker = fm.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey]) else { return nil }
        for case let url as URL in walker where url.lastPathComponent == "wine" && url.deletingLastPathComponent().lastPathComponent == "bin" {
            if fm.isExecutableFile(atPath: url.path) { return url }
        }
        return nil
    }

    public static func sha256(of url: URL) throws -> String {
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        var hasher = SHA256()
        while let chunk = try handle.read(upToCount: 1 << 20), !chunk.isEmpty { hasher.update(data: chunk) }
        return hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }
}
