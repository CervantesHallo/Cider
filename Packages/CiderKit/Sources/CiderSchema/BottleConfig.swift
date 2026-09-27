import Foundation

/// `cider-bottle.json`, schema 1 (docs/plan/01-architecture.md §4).
/// Fields not needed yet (components, drives, freeze, profilesApplied) are added with their features.
public struct BottleConfig: Codable, Equatable, Sendable {
    public var schemaVersion: Int
    public var id: String
    public var name: String
    public var createdBy: String
    public var createdAt: String
    public var template: BottleTemplate
    public var arch: String
    public var cpuBackend: String
    public var engine: EnginePin
    public var engineHistory: [EngineChange]
    public var locale: BottleLocale
    public var integration: Integration
    public var settings: [String: String]
    public var overrides: [String: String]
    /// User-made launchers ("运行命令 → 另存为启动器"); they appear in the library like installed programs.
    public var launchers: [Launcher]?

    public static let currentSchemaVersion = 1

    public init(
        id: String, name: String, createdBy: String, createdAt: String,
        template: BottleTemplate, engine: EnginePin, locale: BottleLocale,
        integration: Integration = .init(), settings: [String: String] = [:], overrides: [String: String] = [:]
    ) {
        self.schemaVersion = Self.currentSchemaVersion
        self.id = id
        self.name = name
        self.createdBy = createdBy
        self.createdAt = createdAt
        self.template = template
        self.arch = "win64-wow64"
        self.cpuBackend = "rosetta-x86_64"
        self.engine = engine
        self.engineHistory = []
        self.locale = locale
        self.integration = integration
        self.settings = settings
        self.overrides = overrides
    }

    enum CodingKeys: String, CodingKey {
        case schemaVersion, id, name, createdBy, createdAt, template, arch
        case cpuBackend = "cpu_backend"
        case engine, engineHistory, locale, integration, settings, overrides, launchers
    }

    /// A saved command: a program (Windows path like `C:\Games\x.exe`, or a Mac path) with arguments.
    public struct Launcher: Codable, Equatable, Sendable, Identifiable {
        public var id: String
        public var name: String
        public var program: String
        public var arguments: [String]
        public var workingDirectory: String?
        public var environment: [String: String]?

        public init(id: String, name: String, program: String, arguments: [String] = [], workingDirectory: String? = nil,
                    environment: [String: String]? = nil) {
            self.id = id
            self.name = name
            self.program = program
            self.arguments = arguments
            self.workingDirectory = workingDirectory
            self.environment = environment
        }
    }

    public struct EnginePin: Codable, Equatable, Sendable {
        public var id: String
        /// `exact` | `minor` | `line` — how far auto-updates may move this bottle.
        public var pin: String
        public init(id: String, pin: String = "exact") { self.id = id; self.pin = pin }
    }

    public struct EngineChange: Codable, Equatable, Sendable {
        public var from: String
        public var to: String
        public var at: String
        public var snapshot: String?

        public init(from: String, to: String, at: String, snapshot: String?) {
            self.from = from
            self.to = to
            self.at = at
            self.snapshot = snapshot
        }
    }

    public struct Integration: Codable, Equatable, Sendable {
        /// `isolated`: Windows shell folders stay inside the prefix (no links into ~/Documents etc.).
        public var shellFolders: String
        /// Whether Z: maps to the host root. Off by default for privacy (docs/plan/01 §9).
        public var zDrive: Bool
        public init(shellFolders: String = "isolated", zDrive: Bool = false) {
            self.shellFolders = shellFolders
            self.zDrive = zDrive
        }
    }
}

public enum BottleTemplate: String, Codable, CaseIterable, Sendable {
    case win7_64, win8_64, win10_64, win11_64

    /// Value for `winecfg /v`.
    public var winver: String {
        switch self {
        case .win7_64: return "win7"
        case .win8_64: return "win8"
        case .win10_64: return "win10"
        case .win11_64: return "win11"
        }
    }
}
