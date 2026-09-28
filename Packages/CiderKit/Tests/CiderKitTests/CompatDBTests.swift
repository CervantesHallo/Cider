import Foundation
import Testing
@testable import CiderData
@testable import CiderStore

@Suite struct CompatDBTests {
    /// The repository's own data directory (Packages/CiderKit/Tests/CiderKitTests → ../../../../data).
    static let repoData = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
        .appendingPathComponent("../../../../data").standardizedFileURL

    func key(_ game: String, engine: String = "11", macos: String = "26") -> VerdictKey {
        VerdictKey(gameID: game, channel: "steam", engineMajor: engine, macosMajor: macos, cpuBackend: "rosetta-x86_64")
    }

    @Test func repositoryDataLoadsCleanly() {
        let db = CompatDB(directories: [Self.repoData])
        #expect(db.rejected.isEmpty, "\(db.rejected)")
        #expect(db.game(steamAppID: "1144400")?.id == "umu-1144400")
        #expect(db.profile(steamAppID: "1144400")?.actions.bottleLocale == "zh-Hans")
        #expect(db.verdict(for: key("umu-1144400")).result == .playableCaveats)
        #expect(db.verdict(for: key("umu-1144400", engine: "12")).result == .unverified)
        #expect(db.verdict(for: key("umu-2458530")).result == .playable)
        #expect(db.verdict(for: key("umu-1277930")).result == .unverified)
    }

    func write(_ json: String, into dir: URL, as name: String) throws {
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        try json.write(to: dir.appendingPathComponent(name), atomically: true, encoding: .utf8)
    }

    func verdictJSON(_ result: String, channel: String = "steam", engine: String = "*", source: String = "maintainer",
                     verified: String = "2026-09-01", validUntil: String? = nil, envModified: Bool = false) -> String {
        let until = validUntil.map { #","valid_until":"\#($0)""# } ?? ""
        return #"{"channel":"\#(channel)","game_version":"*","engine_major":"\#(engine)","macos_major":"*","cpu_backend":"*","result":"\#(result)","last_verified":"\#(verified)"\#(until),"provenance":{"source":"\#(source)","envModified":\#(envModified)}}"#
    }

    @Test func mostSpecificVerdictWinsAndExpiredOnesDoNot() throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("cider-db-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: dir) }
        let verdicts = [verdictJSON("broken"), verdictJSON("playable", engine: "11"),
                        verdictJSON("playable-caveats", engine: "11", verified: "2026-09-20", validUntil: "2026-09-25")]
        try write(#"{"schema":"cider.game/v1","id":"g","names":{"en":"G"},"verdicts":[\#(verdicts.joined(separator: ","))]}"#,
                  into: dir, as: "g.json")
        let db = CompatDB(directories: [dir])
        #expect(db.verdict(for: key("g"), today: "2026-09-27").result == .playable)     // the caveats one expired
        #expect(db.verdict(for: key("g"), today: "2026-09-22").result == .playableCaveats)
        #expect(db.verdict(for: key("g", engine: "12"), today: "2026-09-27").result == .broken)
    }

    @Test func redLinesDropInjectingProfilesAndNonLabGatedVerdicts() throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("cider-db-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: dir) }
        let gating = #""gating":{"anticheat":{"vendor":"hoyoverse","kernel":true}}"#
        try write(#"{"schema":"cider.game/v1","id":"hoyo","names":{"en":"H"},\#(gating),"verdicts":[\#(verdictJSON("playable"))]}"#,
                  into: dir, as: "a-hoyo.json")
        try write(#"{"schema":"cider.game/v1","id":"lab","names":{"en":"L"},\#(gating),"verdicts":[\#(verdictJSON("playable", source: "cider-lab", envModified: true))]}"#,
                  into: dir, as: "b-lab.json")
        try write(#"{"schema":"cider.profile/v1","id":"p","revision":1,"target":"x","match":{},"actions":{"env":{"DYLD_INSERT_LIBRARIES":"/x.dylib"}}}"#,
                  into: dir, as: "c-profile.json")
        let db = CompatDB(directories: [dir])
        #expect(db.games["hoyo"] == nil)
        #expect(db.profiles.isEmpty)
        #expect(db.rejected.count == 2)
        // A lab verdict from a modified environment never makes a gated game playable.
        #expect(db.verdict(for: VerdictKey(gameID: "lab", channel: "steam", engineMajor: "11", macosMajor: "26",
                                           cpuBackend: "rosetta-x86_64")).result == .unverified)
    }

    @Test func versionRanges() {
        #expect(VersionRange(">=3.2.0 <3.3.0").contains("3.2.5"))
        #expect(!VersionRange(">=3.2.0 <3.3.0").contains("3.3"))
        #expect(VersionRange("11.18").contains("11.18.0"))
    }
}

@Suite struct PreflightTests {
    @Test func gatedImagesAreBlockedWhateverThePathForm() {
        #expect(Preflight.check(program: #"C:\Program Files\Genshin Impact\Genshin Impact Game\YuanShen.exe"#, db: nil) != nil)
        #expect(Preflight.check(program: "/Users/x/StarRail.exe", db: nil)?.gameID == "cider:com.mihoyo.sr")
        #expect(Preflight.check(program: "ZenlessZoneZero.EXE", db: nil) != nil)
        #expect(Preflight.check(program: #"C:\Program Files (x86)\Steam\steam.exe"#, db: nil) == nil)
        #expect(Preflight.check(program: "notepad", db: nil) == nil)
    }

    @Test func repositoryRoutesAreOfficialCloudWebOnly() {
        let db = CompatDB(directories: [CompatDBTests.repoData])
        for id in Set(Preflight.gatedExecutables.values) {
            let block = Preflight.check(program: Preflight.gatedExecutables.first { $0.value == id }!.key, db: db)
            #expect(block?.routes.isEmpty == false, "no route for \(id)")
            #expect(block?.routes.allSatisfy { $0.kind == "official-cloud" && $0.url.hasPrefix("https://") && $0.url.contains("mihoyo.com") } == true)
        }
    }
}

@Suite struct RecipeTests {
    @Test func repositoryRecipesPassPolicy() {
        let db = CompatDB(directories: [CompatDBTests.repoData])
        #expect(db.rejected.isEmpty, "\(db.rejected)")
        #expect(db.recipes["launcher.steam"]?.detect.files.first?.hasSuffix("steam.exe") == true)
        #expect(db.recipes["runtime.vcrun2022"]?.sources["x64"]?.sha256.count == 1)
    }

    @Test func floatingInstallersMustComeFromVendorHosts() throws {
        let json = #"{"schema":"cider.recipe/v1","id":"x","kind":"app","revision":1,"name":{"en":"X"},"sources":{"i":{"urls":["https://evil.example.com/x.exe"],"sha256":[],"floating":true}},"steps":[{"run_installer":{"source":"i","kind":"exe"}}],"detect":{"files":[]}}"#
        let recipe = try JSONDecoder().decode(Recipe.self, from: Data(json.utf8))
        #expect(RecipePolicy.violation(in: recipe)?.contains("allow-list") == true)
    }
}

@Suite struct EngineIndexTests {
    @Test func repositoryIndexHasARecommendedEngine() throws {
        let index = try #require(EngineIndex.load(from: CompatDBTests.repoData.appendingPathComponent("engines/index.json")))
        let engine = try #require(index.recommended)
        #expect(engine.sha256.count == 64)
        #expect(engine.urls.allSatisfy { $0.hasPrefix("https://") })
        #expect(engine.size > 0)
    }
}
