import Foundation

/// How to install something (docs/plan/06 §4), the subset Cider runs today: download pinned or official
/// installers, run them, apply typed registry/Windows-version steps, and check that the result is there.
/// Recipes never contain scripts.
public struct Recipe: Codable, Sendable, Equatable, Identifiable {
    public var schema: String
    public var id: String                         // launcher.steam, runtime.vcrun2022, …
    public var kind: Kind
    public var revision: Int
    public var name: [String: String]
    public var summary: [String: String]?
    public var bottle: BottleHint?
    public var dependencies: [String]?
    public var sources: [String: Source]
    public var steps: [Step]
    public var detect: Detect
    public var launch: String?                    // program to start after installing (Windows path)

    public enum Kind: String, Codable, Sendable { case app, component, launcher }

    public struct BottleHint: Codable, Sendable, Equatable {
        public var locale: String?                // zh-Hans | ja | …
        public var name: String?                  // suggested name for a new bottle
    }

    public struct Source: Codable, Sendable, Equatable {
        public var urls: [String]
        /// Accepted hashes. Empty only for `floating` sources: official installers that change behind a stable
        /// URL (Steam, EA app); those must come from an allow-listed vendor domain (`RecipePolicy`).
        public var sha256: [String]
        public var floating: Bool?
        public var filename: String?
    }

    public struct Step: Codable, Sendable, Equatable {
        public var runInstaller: RunInstaller?
        public var registry: [Profile.RegistrySet]?
        public var winver: String?
        /// Accepted profile whose environment is inherited by the installer tree.
        public var environmentProfile: String? = nil

        public struct RunInstaller: Codable, Sendable, Equatable {
            public var source: String
            public var kind: String               // exe | msi
            public var args: [String]?
            /// Only known bootstrapper recipes opt into MSI's reboot return codes.
            public var exitCodeConvention: String? = nil
            enum CodingKeys: String, CodingKey {
                case source, kind, args
                case exitCodeConvention = "exit_code_convention"
            }
        }

        enum CodingKeys: String, CodingKey {
            case runInstaller = "run_installer", registry, winver
            case environmentProfile = "environment_profile"
        }
    }

    public struct Detect: Codable, Sendable, Equatable {
        public var files: [String]                // Windows paths that must all exist
        /// At least one of these must exist (install locations that vary between installer versions).
        public var anyFiles: [String]?
        /// Registry keys that must exist, e.g. `HKLM\\Software\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64`
        /// (for components whose DLL names Wine already ships as built-ins).
        public var registryKeys: [String]?
        /// DWORD values that must be at least `atLeast` — e.g. the VC++ runtime's build number, because Wine
        /// pre-creates the runtime's keys with its own (older) build to satisfy installers' checks.
        public var registryValues: [RegistryValue]?
        /// An empty conjunction is not evidence of an installation.
        public var hasConditions: Bool {
            !files.isEmpty || !(anyFiles ?? []).isEmpty || !(registryKeys ?? []).isEmpty || !(registryValues ?? []).isEmpty
        }
        enum CodingKeys: String, CodingKey {
            case files, anyFiles = "any_files", registryKeys = "registry_keys", registryValues = "registry_values"
        }
    }

    public struct RegistryValue: Codable, Sendable, Equatable {
        public var key: String
        public var name: String
        public var atLeast: UInt32
        enum CodingKeys: String, CodingKey { case key, name, atLeast = "at_least" }
    }

    public func title(_ lang: String = "zh-Hans") -> String { name[lang] ?? name["en"] ?? id }
    public func blurb(_ lang: String = "zh-Hans") -> String { summary?[lang] ?? summary?["en"] ?? "" }
}

/// Where floating installers may come from (docs/plan/06 §4 `policy/domains.yaml`).
public enum RecipePolicy {
    public static let floatingHosts = [
        "steamstatic.com", "steampowered.com", "microsoft.com", "aka.ms", "visualstudio.microsoft.com",
        "akamaized.net", "mihoyo.com", "ea.com", "battle.net", "blizzard.com",
        "origin-a.akamaihd.net",
    ]

    public static func violation(in recipe: Recipe) -> String? {
        for (name, source) in recipe.sources {
            if source.urls.isEmpty { return "source \(name) has no URL" }
            for url in source.urls {
                guard let host = URL(string: url)?.host?.lowercased(), url.hasPrefix("https://") else {
                    return "source \(name): only https URLs"
                }
                if source.sha256.isEmpty {
                    guard source.floating == true else { return "source \(name): sha256 required" }
                    guard floatingHosts.contains(where: { host == $0 || host.hasSuffix("." + $0) }) else {
                        return "source \(name): floating installer from a host outside the vendor allow-list (\(host))"
                    }
                }
            }
        }
        for step in recipe.steps {
            if let run = step.runInstaller, recipe.sources[run.source] == nil { return "step uses unknown source \(run.source)" }
            if let convention = step.runInstaller?.exitCodeConvention, convention != "msi" {
                return "installer exit_code_convention is not supported"
            }
            if let profile = step.environmentProfile {
                guard step.runInstaller != nil, profile.hasPrefix("profile."), profile.utf8.count <= 128,
                      profile.unicodeScalars.allSatisfy({ $0.isASCII && (CharacterSet.alphanumerics.contains($0) || "._-".unicodeScalars.contains($0)) }) else {
                    return "environment_profile requires an installer step and a valid profile id"
                }
            }
        }
        return detectionViolation(in: recipe.detect)
    }

    /// Validate the supported detect subset without accessing a prefix or executing Wine.
    public static func detectionViolation(in detect: Recipe.Detect) -> String? {
        guard detect.hasConditions else { return "detect requires at least one installation condition" }
        if detect.anyFiles?.isEmpty == true { return "detect any_files must not be empty when present" }
        for path in detect.files + (detect.anyFiles ?? []) {
            let head = Array(path.unicodeScalars.prefix(3))
            guard !path.contains("\0"), head.count == 3,
                  (65...90).contains(head[0].value) || (97...122).contains(head[0].value),
                  head[1].value == 58, head[2].value == 47 || head[2].value == 92 else {
                return "detect file requires an absolute Windows drive path without NUL"
            }
            let parts = path.dropFirst(3).split(whereSeparator: { $0 == "\\" || $0 == "/" })
            if parts.contains(where: { $0 == "." || $0 == ".." }) {
                return "detect file requires a normalized path without dot components"
            }
        }
        for key in (detect.registryKeys ?? []) + (detect.registryValues ?? []).map(\.key) {
            let parts = key.split(separator: "\\", maxSplits: 1, omittingEmptySubsequences: false)
            guard parts.count == 2, !parts[1].isEmpty,
                  ["HKLM", "HKEY_LOCAL_MACHINE", "HKCU", "HKEY_CURRENT_USER"].contains(parts[0].uppercased()),
                  !key.contains("\0"), !key.contains("\n"), !key.contains("\r") else {
                return "detect registry key requires a supported hive and a nonempty single-line path"
            }
        }
        for value in detect.registryValues ?? [] {
            guard !value.name.contains("\0"), !value.name.contains("\n"), !value.name.contains("\r") else {
                return "detect registry value name must not contain NUL or line breaks"
            }
        }
        return nil
    }
}
