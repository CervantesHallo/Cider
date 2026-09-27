import Foundation

/// `Engines/<id>/manifest.json` (docs/plan/01-architecture.md §5), reduced to what v0 uses.
public struct EngineManifest: Codable, Equatable, Sendable {
    public var schemaVersion: Int
    public var id: String
    /// `R` = x86_64 Wine under Rosetta; `A` = arm64 Wine + FEX (later).
    public var flavor: String
    public var channel: String
    public var wine: WineInfo
    public var cpuBackend: String
    /// Path of the Wine tree inside the engine directory, e.g. `wine` (contains bin/, lib/, share/).
    public var root: String
    public var requires: Requirements
    public var source: Source
    /// Directories (relative to the engine directory) holding dylibs the engine loads by bare name
    /// (e.g. stripped engines that expect libgnutls/libMoltenVK/libSDL2/libfreetype to be provided).
    public var libraryPaths: [String]?

    enum CodingKeys: String, CodingKey {
        case schemaVersion, id, flavor, channel, wine
        case cpuBackend = "cpu_backend"
        case root, requires, source, libraryPaths
    }

    public init(id: String, flavor: String = "R", channel: String, wine: WineInfo, root: String, requires: Requirements, source: Source) {
        self.schemaVersion = 1
        self.id = id
        self.flavor = flavor
        self.channel = channel
        self.wine = wine
        self.cpuBackend = flavor == "A" ? "fex-arm64" : "rosetta-x86_64"
        self.root = root
        self.requires = requires
        self.source = source
    }

    public struct WineInfo: Codable, Equatable, Sendable {
        /// e.g. `wine-11.18`
        public var version: String
        /// `upstream-devel`, `upstream-staging`, `cider` (own builds) …
        public var tree: String
        public init(version: String, tree: String) { self.version = version; self.tree = tree }
    }

    public struct Requirements: Codable, Equatable, Sendable {
        public var macos: String
        public var rosetta: Bool
        public init(macos: String, rosetta: Bool) { self.macos = macos; self.rosetta = rosetta }
    }

    public struct Source: Codable, Equatable, Sendable {
        /// Where the package came from (URL or local path) and its sha256.
        public var origin: String
        public var sha256: String
        public init(origin: String, sha256: String) { self.origin = origin; self.sha256 = sha256 }
    }
}
