import Foundation

// Compatibility data (docs/plan/06 §3–§5, §10–§11), the subset Cider uses today:
// game entries with verdicts, and profiles with registry/env/winver actions. JSON files, one object each,
// loaded from the app's bundled data plus the user's data directory (later: the signed data channel).

public struct GameEntry: Codable, Sendable, Equatable {
    public var schema: String
    public var id: String
    public var aliases: [String: String]?          // "steam": "1144400"
    public var names: [String: String]
    public var developer: String?
    public var engine: String?                     // e.g. "kirikiri-z" (informational)
    public var gating: Gating?
    /// Windows image names of the game (for launches that don't come from a store manifest).
    public var executables: [String]?
    public var routes: [Route]?
    public var verdicts: [Verdict]

    public struct Gating: Codable, Sendable, Equatable {
        public var anticheat: AntiCheat?
        public struct AntiCheat: Codable, Sendable, Equatable {
            public var vendor: String
            public var kernel: Bool
        }
        /// "Gated" entries (docs/plan/06 §11) get the strictest lint and only lab-verified verdicts.
        public var isGated: Bool { anticheat.map { $0.vendor == "hoyoverse" || $0.kernel } ?? false }
    }

    public func name(for languages: [String] = Locale.preferredLanguages) -> String {
        for lang in languages {
            if lang.hasPrefix("zh-Hans") || lang == "zh-CN", let n = names["zh-Hans"] { return n }
            if lang.hasPrefix("zh-Hant") || lang == "zh-TW", let n = names["zh-Hant"] { return n }
            if lang.hasPrefix("ja"), let n = names["ja"] { return n }
        }
        return names["en"] ?? names.values.first ?? id
    }
}

public struct Verdict: Codable, Sendable, Equatable {
    public enum Result: String, Codable, Sendable {
        case playable, playableCaveats = "playable-caveats", unverified
        case blockedAnticheat = "blocked-anticheat", blockedDRM = "blocked-drm"
        case brokenLauncher = "broken-launcher", broken
    }
    public var channel: String                    // steam | mihoyo-launcher | installer | * …
    public var gameVersion: String                // version range or "*"
    public var engineMajor: String                // "11" or "*"
    public var macosMajor: String                 // "26" or "*"
    public var cpuBackend: String                 // rosetta-x86_64 | *
    public var result: Result
    public var notes: [String: String]?           // localized caveats
    public var lastVerified: String               // yyyy-MM-dd
    public var validUntil: String?
    public var provenance: Provenance

    public struct Provenance: Codable, Sendable, Equatable {
        public var source: String                 // cider-lab | maintainer | community | import
        public var hw: String?                    // "M3 8GB"
        public var engine: String?
        public var envModified: Bool?
    }

    enum CodingKeys: String, CodingKey {
        case channel, result, notes, provenance
        case gameVersion = "game_version", engineMajor = "engine_major", macosMajor = "macos_major"
        case cpuBackend = "cpu_backend", lastVerified = "last_verified", validUntil = "valid_until"
    }
}

/// What the lookup is about: one game in one environment.
public struct VerdictKey: Sendable, Equatable {
    public var gameID: String
    public var channel: String
    public var gameVersion: String?
    public var engineMajor: String
    public var macosMajor: String
    public var cpuBackend: String

    public init(gameID: String, channel: String, gameVersion: String? = nil, engineMajor: String, macosMajor: String,
                cpuBackend: String) {
        self.gameID = gameID
        self.channel = channel
        self.gameVersion = gameVersion
        self.engineMajor = engineMajor
        self.macosMajor = macosMajor
        self.cpuBackend = cpuBackend
    }
}

public struct VerdictDecision: Sendable, Equatable {
    public var result: Verdict.Result
    public var verdict: Verdict?                  // nil when nothing covers the key
}

public struct Profile: Codable, Sendable, Equatable {
    public var schema: String
    public var id: String
    public var revision: Int
    public var target: String
    public var match: Match
    public var actions: Actions
    public var knownIssues: [KnownIssue]?

    public struct Match: Codable, Sendable, Equatable {
        public var steamAppID: String?
        public var exe: String?
        enum CodingKeys: String, CodingKey { case steamAppID = "steam_appid", exe }
    }

    public struct Actions: Codable, Sendable, Equatable {
        public var env: [String: String]?
        public var winver: String?
        public var dllOverrides: [String: String]?        // dll → "native,builtin" …
        public var registry: [RegistrySet]?
        public var bottleLocale: String?                   // recommendation shown before install/launch
        enum CodingKeys: String, CodingKey {
            case env, winver, registry
            case dllOverrides = "dll_overrides", bottleLocale = "bottle_locale"
        }
    }

    public struct RegistrySet: Codable, Sendable, Equatable {
        public var key: String
        public var name: String
        public var value: String
    }

    public struct KnownIssue: Codable, Sendable, Equatable {
        public var symptom: [String: String]
        public var fix: [String: String]?
    }

    enum CodingKeys: String, CodingKey {
        case schema, id, revision, target, match, actions
        case knownIssues = "known_issues"
    }
}

public struct CompatDB: Sendable {
    public private(set) var games: [String: GameEntry] = [:]
    public private(set) var profiles: [Profile] = []
    public private(set) var recipes: [String: Recipe] = [:]
    public private(set) var rejected: [(file: String, reason: String)] = []

    public init() {}

    /// Loads every `*.json` under the directories (later ones override earlier entries with the same id).
    /// Entries that fail to decode or break a red line are dropped and listed in `rejected`.
    public init(directories: [URL]) {
        let decoder = JSONDecoder()
        for dir in directories {
            guard let walker = FileManager.default.enumerator(at: dir, includingPropertiesForKeys: nil) else { continue }
            for case let url as URL in walker where url.pathExtension == "json" {
                guard let data = try? Data(contentsOf: url),
                      let head = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                      let schema = head["schema"] as? String else { continue }
                do {
                    switch schema {
                    case "cider.game/v1":
                        let game = try decoder.decode(GameEntry.self, from: data)
                        if let reason = RedLines.violation(in: game) { rejected.append((url.lastPathComponent, reason)); continue }
                        games[game.id] = game
                    case "cider.profile/v1":
                        let profile = try decoder.decode(Profile.self, from: data)
                        if let reason = RedLines.violation(in: profile, target: games[profile.target]) {
                            rejected.append((url.lastPathComponent, reason)); continue
                        }
                        profiles.removeAll { $0.id == profile.id }
                        profiles.append(profile)
                    case "cider.recipe/v1":
                        let recipe = try decoder.decode(Recipe.self, from: data)
                        if let reason = RecipePolicy.violation(in: recipe) { rejected.append((url.lastPathComponent, reason)); continue }
                        recipes[recipe.id] = recipe
                    default: continue
                    }
                } catch {
                    rejected.append((url.lastPathComponent, "\(error)"))
                }
            }
        }
    }

    public func game(steamAppID: String) -> GameEntry? {
        games.values.first { $0.aliases?["steam"] == steamAppID }
    }

    public func profile(steamAppID: String) -> Profile? {
        profiles.first { $0.match.steamAppID == steamAppID }
    }

    /// Most specific verdict covering the key (fewest `*` dimensions; ties → most recently verified).
    /// Expired verdicts count as unverified; gated games only accept lab verdicts from an unmodified environment.
    public func verdict(for key: VerdictKey, today: String = CompatDB.today()) -> VerdictDecision {
        guard let game = games[key.gameID] else { return VerdictDecision(result: .unverified, verdict: nil) }
        let candidates = game.verdicts.filter { v in
            matches(v.channel, key.channel) && matches(v.engineMajor, key.engineMajor) && matches(v.macosMajor, key.macosMajor)
                && matches(v.cpuBackend, key.cpuBackend)
                && (v.gameVersion == "*" || key.gameVersion.map { VersionRange(v.gameVersion).contains($0) } == true)
                && (v.validUntil.map { $0 >= today } ?? true)
                && (!(game.gating?.isGated ?? false) || (v.provenance.source == "cider-lab" && v.provenance.envModified == false))
        }
        let best = candidates.max { a, b in
            let sa = specificity(a), sb = specificity(b)
            return sa != sb ? sa < sb : a.lastVerified < b.lastVerified
        }
        return VerdictDecision(result: best?.result ?? .unverified, verdict: best)
    }

    private func matches(_ pattern: String, _ value: String) -> Bool { pattern == "*" || pattern == value }

    private func specificity(_ v: Verdict) -> Int {
        [v.channel, v.gameVersion, v.engineMajor, v.macosMajor, v.cpuBackend].filter { $0 != "*" }.count
    }

    public static func today() -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "yyyy-MM-dd"
        return f.string(from: Date())
    }
}

/// Version ranges like ">=3.2.0 <3.3.0" (space = AND); versions compare numerically part by part.
public struct VersionRange: Sendable {
    let constraints: [(op: String, version: [Int])]

    public init(_ text: String) {
        constraints = text.split(separator: " ").compactMap { part in
            let s = String(part)
            for op in [">=", "<=", ">", "<", "="] where s.hasPrefix(op) {
                return (op, VersionRange.parse(String(s.dropFirst(op.count))))
            }
            return ("=", VersionRange.parse(s))
        }
    }

    public func contains(_ version: String) -> Bool {
        let v = VersionRange.parse(version)
        return constraints.allSatisfy { c in
            let cmp = VersionRange.compare(v, c.version)
            switch c.op {
            case ">=": return cmp >= 0
            case "<=": return cmp <= 0
            case ">": return cmp > 0
            case "<": return cmp < 0
            default: return cmp == 0
            }
        }
    }

    static func parse(_ s: String) -> [Int] { s.split { !$0.isNumber }.compactMap { Int($0) } }

    static func compare(_ a: [Int], _ b: [Int]) -> Int {
        for i in 0..<max(a.count, b.count) {
            let x = i < a.count ? a[i] : 0, y = i < b.count ? b[i] : 0
            if x != y { return x < y ? -1 : 1 }
        }
        return 0
    }
}

/// Red lines enforced on load (docs/plan/06 §11, 00 "不做清单"): data can never inject into processes, point at
/// iOS forms, or spoof/hide from anti-cheat. Profiles for gated games get the strict set.
public enum RedLines {
    static let bannedEnvPrefixes = ["DYLD_", "LD_PRELOAD", "WINELOADER", "WINESERVER", "WINEDLLPATH", "CIDER_", "WINEMSYNC", "WINEESYNC"]
    static let spoofEnv = ["SteamAppId", "SteamGameId", "SteamDeck", "SteamOS", "WINE_HIDE_", "HideWineExports"]
    static let iosMarkers = ["apps.apple.com", "itms-apps:", "playcover", ".ipa"]

    public static func violation(in game: GameEntry) -> String? {
        let text = (try? String(data: JSONEncoder().encode(game), encoding: .utf8))?.lowercased() ?? ""
        if let m = iosMarkers.first(where: { text.contains($0) }) { return "RL-IOS: \(m)" }
        if game.gating?.isGated == true,
           game.verdicts.contains(where: { $0.result == .playable && $0.provenance.source != "cider-lab" }) {
            return "RL-OVERRIDE: gated game with a non-lab playable verdict"
        }
        return nil
    }

    public static func violation(in profile: Profile, target: GameEntry?) -> String? {
        for key in (profile.actions.env ?? [:]).keys {
            if let p = bannedEnvPrefixes.first(where: { key.uppercased().hasPrefix($0) }) { return "RL-INJECT: \(p)" }
            if target?.gating?.isGated == true, let s = spoofEnv.first(where: { key.hasPrefix($0) }) { return "RL-SPOOF: \(s)" }
        }
        if target?.gating?.isGated == true, profile.actions.dllOverrides?.isEmpty == false {
            return "RL-SPOOF: DLL overrides on a gated game"
        }
        return nil
    }
}
