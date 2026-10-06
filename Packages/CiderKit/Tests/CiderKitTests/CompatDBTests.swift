import Foundation
import Testing
@testable import CiderBottle
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

@Suite struct KernelFidelityTests {
    /// A matrix with one entry, so each test can state exactly the combination it is about.
    func matrixJSON(reachability: String, disposition: String, severity: String,
                    symbol: String = "ObRegisterCallbacks", extra: String = "") -> String {
        let entry = #"{"symbol":"\#(symbol)","dll":"ntoskrnl.exe","location":"dlls/ntoskrnl.exe/ntoskrnl.c:3152","wine_behaviour":"fabricates a handle and returns STATUS_SUCCESS","windows_contract":"registers object-manager callbacks","reachability":"\#(reachability)","disposition":"\#(disposition)","severity":"\#(severity)"}"#
        return #"{"schema":"cider.kernel-fidelity/v1","revision":1,"generated_from":"t","audited":"2026-09-28","wine_model_constraints":"no ring 0","entries":[\#(entry)\#(extra.isEmpty ? "" : "," + extra)]}"#
    }

    func decode(_ json: String) throws -> KernelFidelity {
        try JSONDecoder().decode(KernelFidelity.self, from: Data(json.utf8))
    }

    @Test func repositoryMatrixLoadsAndRecordsWhatWineActuallyDoes() throws {
        let db = CompatDB(directories: [CompatDBTests.repoData])
        #expect(db.rejected.isEmpty, "\(db.rejected)")
        let matrix = try #require(db.kernelFidelity)
        #expect(matrix.entries.count > 40)
        #expect(!matrix.deceptive.isEmpty, "the audited tree fakes success somewhere; that is the point")
        // The API the whole track turns on: object-manager callbacks cannot be intercepted from
        // winedevice.exe, and the tree currently hands back a fake handle and reports success.
        let ob = try #require(matrix.entry(for: "ObRegisterCallbacks"))
        #expect(ob.reachability == .unreachable)
        #expect(ob.severity == .deceptiveSuccess)
        #expect(ob.disposition == .honestFailure)
    }

    @Test func unreachableApisMayOnlyFailHonestly() throws {
        for claimed in ["implemented", "needs_implementation"] {
            let m = try decode(matrixJSON(reachability: "unreachable", disposition: claimed, severity: "missing"))
            #expect(KernelFidelityPolicy.violation(in: m)?.contains("honest_failure") == true,
                    "unreachable + \(claimed) must be rejected")
        }
        let honest = try decode(matrixJSON(reachability: "unreachable", disposition: "honest_failure", severity: "missing"))
        #expect(KernelFidelityPolicy.violation(in: honest) == nil)
    }

    @Test func aStubThatFakesSuccessCannotBeCalledImplemented() throws {
        let m = try decode(matrixJSON(reachability: "faithful", disposition: "implemented", severity: "deceptive_success"))
        #expect(KernelFidelityPolicy.violation(in: m)?.contains("deceptive_success") == true)
    }

    @Test func duplicateSymbolsAreRejected() throws {
        let dupe = #"{"symbol":"ObRegisterCallbacks","dll":"ntoskrnl.exe","location":"x","wine_behaviour":"y","windows_contract":"z","reachability":"partial","disposition":"needs_implementation","severity":"missing"}"#
        let m = try decode(matrixJSON(reachability: "partial", disposition: "needs_implementation",
                                      severity: "missing", extra: dupe))
        #expect(KernelFidelityPolicy.violation(in: m)?.contains("duplicate") == true)
    }

    @Test func aMatrixThatOverstatesWineIsNotLoaded() throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("cider-kf-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: dir) }
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        try matrixJSON(reachability: "unreachable", disposition: "implemented", severity: "implemented")
            .write(to: dir.appendingPathComponent("fidelity.json"), atomically: true, encoding: .utf8)
        let db = CompatDB(directories: [dir])
        #expect(db.kernelFidelity == nil)
        #expect(db.rejected.count == 1)
    }
}

@Suite struct ProfileLookupTests {
    /// A profile is matched by the program's Windows image name, whatever path form the caller used.
    @Test func profilesMatchAProgramByImageName() {
        let db = CompatDB(directories: [CompatDBTests.repoData])
        #expect(db.rejected.isEmpty, "\(db.rejected)")
        for form in [#"C:\Program Files\miHoYo Launcher\launcher.exe"#,
                     "/Users/x/Bottles/b/drive_c/Program Files/miHoYo Launcher/launcher.exe",
                     "LAUNCHER.EXE"] {
            #expect(db.profile(exe: form)?.id == "profile.launcher.mihoyo-cn", "did not match \(form)")
        }
        #expect(db.profile(exe: "notepad.exe") == nil)
        #expect(db.profile(exe: "") == nil)
        #expect(db.profile(exe: #"C:\Program Files\miHoYo Launcher\1.18.0\HYP.exe"#)?.id == "profile.launcher.mihoyo-cn")
        #expect(db.profile(exe: "HYPHelper.exe")?.actions.processNames == ["HYP.exe", "HYPHelper.exe", "HYSafeMode.exe"])
    }

    /// The hot-fix this profile exists for: the CN launcher's CEF GPU process presents into a child
    /// window owned by another process, so it needs DXMT's cross-process swapchain opt-in or the
    /// window stays white. Verified on bottle-ca8f, engine cider-cx26.3-r1-x86_64, 2026-09-28.
    @Test func theMihoyoLauncherProfileCarriesTheCrossProcessSwapchainOptIn() throws {
        let db = CompatDB(directories: [CompatDBTests.repoData])
        let profile = try #require(db.profile(exe: "launcher.exe"))
        #expect(profile.actions.env?["DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN"] == "1")
        #expect(profile.knownIssues?.isEmpty == false)
        // It must stay clean under the red lines: no injection, no spoofing, no DLL overrides.
        #expect(RedLines.violation(in: profile, target: nil) == nil)
        #expect(profile.actions.dllOverrides == nil)
    }
}

@Suite struct BottleStoreCompatTests {
    /// Regression: the profile hot-fix first shipped wired into one CLI call site only, so every other
    /// launch path (the app, launcher shims, recipe installs) silently ran without it and the CN
    /// launcher came back white. The store owns the data so `runner(for:)` cannot forget it.
    @Test func theStoreLoadsCompatDataForEveryLaunchPath() {
        let store = BottleStore(paths: .standard(), compat: CompatDB(directories: [CompatDBTests.repoData]))
        #expect(store.compat.profile(exe: "launcher.exe")?.id == "profile.launcher.mihoyo-cn")
    }

    @Test func compatDirectoriesArePriorityOrdered() {
        let dirs = BottleStore.compatDirectories(paths: .standard()).map(\.path)
        // The user's own data directory wins, so the signed data channel can override what we bundled.
        #expect(dirs.last?.hasSuffix("/Data") == true)
        #expect(dirs.contains { $0.hasSuffix("/Applications/Cider.app/Contents/Resources/data") })
        #expect(dirs.count >= 3)
    }
}
