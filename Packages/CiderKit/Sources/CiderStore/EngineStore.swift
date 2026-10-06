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
                return InstalledEngine(manifest: try JSONFile.read(EngineManifest.self, from: manifestURL), directory: dir)
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
        guard let engine = try list().last else {
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
        var keepStaging = false
        defer { if !keepStaging { try? fm.removeItem(at: staging) } }

        try Command.run("/usr/bin/tar", ["-xf", package.path, "-C", staging.path])
        // Files fetched by a browser carry quarantine; engines are verified by hash, not Gatekeeper.
        _ = try? Command.run("/usr/bin/xattr", ["-dr", "com.apple.quarantine", staging.path])

        // Cider's own engine packages (engine/build.sh) carry their manifest: keep it as is.
        if let built = try fm.contentsOfDirectory(at: staging, includingPropertiesForKeys: nil)
            .first(where: { fm.fileExists(atPath: $0.appendingPathComponent("manifest.json").path) }),
           var manifest = try? JSONFile.read(EngineManifest.self, from: built.appendingPathComponent("manifest.json")) {
            let destination = paths.engines.appendingPathComponent(manifest.id, isDirectory: true)
            if fm.fileExists(atPath: destination.path) { throw CiderError.alreadyExists("engine \(manifest.id)") }
            manifest.source = .init(origin: origin ?? package.path, sha256: sha)
            try JSONFile.write(manifest, to: built.appendingPathComponent("manifest.json"))
            try fm.moveItem(at: built, to: destination)
            let engine = InstalledEngine(manifest: manifest, directory: destination)
            try EngineHost.ensure(engineDirectory: destination, wineRoot: engine.wineRoot, cpuBackend: engine.manifest.cpuBackend)
            return engine
        }

        guard let wineBin = Self.findWineBinary(in: staging) else {
            throw CiderError.invalid("no bin/wine found in \(package.lastPathComponent)")
        }
        let wineRoot = wineBin.deletingLastPathComponent().deletingLastPathComponent()
        let rootRelative = String(wineRoot.path.dropFirst(staging.path.count + 1))

        let versionOutput = try Command.run(wineBin.path, ["--version"], environment: ["HOME": NSHomeDirectory(), "PATH": "/usr/bin:/bin"])
        let version = versionOutput.trimmingCharacters(in: .whitespacesAndNewlines)
        guard version.hasPrefix("wine-") else { throw CiderError.invalid("unexpected `wine --version` output: \(version)") }

        let tree = requestedTree ?? (package.lastPathComponent.contains("staging") ? "upstream-staging" : "upstream-devel")
        let id = requestedID ?? "\(tree == "upstream-staging" ? "wine-staging" : "wine-devel")-\(version.dropFirst(5))-x86_64"
        let destination = paths.engines.appendingPathComponent(id, isDirectory: true)
        if fm.fileExists(atPath: destination.path) { throw CiderError.alreadyExists("engine \(id)") }

        let manifest = EngineManifest(
            id: id, channel: channel,
            wine: .init(version: version, tree: tree),
            root: rootRelative,
            requires: .init(macos: "14.0", rosetta: true),
            source: .init(origin: origin ?? package.path, sha256: sha)
        )
        try JSONFile.write(manifest, to: staging.appendingPathComponent("manifest.json"))
        try fm.moveItem(at: staging, to: destination)
        keepStaging = true
        let engine = InstalledEngine(manifest: manifest, directory: destination)
        try EngineHost.ensure(engineDirectory: destination, wineRoot: engine.wineRoot, cpuBackend: engine.manifest.cpuBackend)
        return engine
    }

    /// Installs an engine directory that already carries its manifest (the output of `engine/build.sh`):
    /// APFS-clones it into place, keeping the manifest's library paths, then builds the host bundle.
    @discardableResult
    public func installBuilt(directory: URL, replacing: Bool = false) throws -> InstalledEngine {
        let fm = FileManager.default
        let manifest = try JSONFile.read(EngineManifest.self, from: directory.appendingPathComponent("manifest.json"))
        try fm.ensureDirectory(paths.engines)
        let destination = paths.engines.appendingPathComponent(manifest.id, isDirectory: true)
        if fm.fileExists(atPath: destination.path) {
            guard replacing else { throw CiderError.alreadyExists("engine \(manifest.id)") }
            try fm.trashItem(at: destination, resultingItemURL: nil)
        }
        try Command.run("/bin/cp", ["-c", "-R", directory.path, destination.path])
        let engine = InstalledEngine(manifest: manifest, directory: destination)
        guard fm.isExecutableFile(atPath: engine.wineRoot.appendingPathComponent("bin/wine").path) else {
            throw CiderError.invalid("no bin/wine under \(manifest.root) in \(directory.path)")
        }
        try EngineHost.ensure(engineDirectory: destination, wineRoot: engine.wineRoot, cpuBackend: engine.manifest.cpuBackend)
        return engine
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
