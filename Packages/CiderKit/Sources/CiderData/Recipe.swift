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
        /// URL (Steam, Epic); those must come from an allow-listed vendor domain (`RecipePolicy`).
        public var sha256: [String]
        public var floating: Bool?
        public var filename: String?
    }

    public struct Step: Codable, Sendable, Equatable {
        public var runInstaller: RunInstaller?
        public var registry: [Profile.RegistrySet]?
        public var winver: String?

        public struct RunInstaller: Codable, Sendable, Equatable {
            public var source: String
            public var kind: String               // exe | msi
            public var args: [String]?
        }

        enum CodingKeys: String, CodingKey {
            case runInstaller = "run_installer", registry, winver
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
        "epicgames.com", "akamaized.net", "mihoyo.com", "ea.com", "battle.net", "blizzard.com",
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
        }
        return nil
    }
}
