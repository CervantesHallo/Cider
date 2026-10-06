import Foundation
import Testing
import CiderRuntime
@testable import CiderBottle
@testable import CiderCore
@testable import CiderData
@testable import CiderIntegration
@testable import CiderPE
@testable import CiderSchema

@Suite struct BottleManagementTests {
    let home = FileManager.default.temporaryDirectory.appendingPathComponent("cider-mgmt-\(UUID().uuidString)")
    var paths: CiderPaths { .standard(environment: ["CIDER_HOME": home.path]) }

    /// A bottle on disk without Wine: a prefix with one file and a config.
    func makeBottle(_ store: BottleStore) throws -> Bottle {
        let config = BottleConfig(id: "test-0001", name: "Test", createdBy: "tests", createdAt: Identifiers.timestamp(),
                                  template: .win10_64, engine: .init(id: "none"), locale: .japanese)
        let bottle = Bottle(config: config, directory: paths.bottles.appendingPathComponent(config.id))
        try FileManager.default.createDirectory(at: bottle.driveC, withIntermediateDirectories: true)
        try "v1".write(to: bottle.driveC.appendingPathComponent("save.dat"), atomically: true, encoding: .utf8)
        try JSONFile.write(config, to: bottle.configURL)
        return bottle
    }

    func read(_ bottle: Bottle) -> String? {
        try? String(contentsOf: bottle.driveC.appendingPathComponent("save.dat"), encoding: .utf8)
    }

    @Test func snapshotRestoreRoundTrip() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try makeBottle(store)
        try store.snapshot(bottle, reason: "first")
        #expect(store.snapshots(of: bottle).map(\.reason) == ["first"])

        try "v2".write(to: bottle.driveC.appendingPathComponent("save.dat"), atomically: true, encoding: .utf8)
        try store.rename(bottle, to: "Renamed")
        let current = try store.bottle("test-0001")
        let restored = try store.restore(current, to: try #require(store.snapshots(of: bottle).last))
        #expect(read(restored) == "v1")
        #expect(restored.config.name == "Renamed")          // the name is not rolled back
        #expect(store.snapshots(of: bottle).count == 2)     // plus "before restoring …"
    }

    @Test func duplicateClonesThePrefix() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try makeBottle(store)
        let copy = try store.duplicate(bottle, name: "Copy of Test")
        #expect(copy.config.id != bottle.config.id)
        #expect(read(copy) == "v1")
        #expect(try store.list().count == 2)
    }

    @Test func launchersShowUpInTheLibrary() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try store.saveLauncher(.init(id: "l1", name: "Tool", program: #"C:\tool.exe"#, arguments: ["-x"]),
                                            in: try makeBottle(store))
        let apps = AppCatalog(iconCache: IconCache(directory: home.appendingPathComponent("icons"))).apps(in: bottle)
        let app = try #require(apps.first { $0.title == "Tool" })
        #expect(app.launchProgram == #"C:\tool.exe"#)
        #expect(app.launchArguments == ["-x"])
        #expect(try store.removeLauncher(id: "l1", from: bottle).config.launchers == [])
    }

    @Test func builtinLauncherShowsUpToo() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try store.saveLauncher(.init(id: "n", name: "记事本", program: "notepad"), in: try makeBottle(store))
        let reloaded = try store.bottle(bottle.config.id)
        let apps = AppCatalog(iconCache: IconCache(directory: home.appendingPathComponent("icons"))).apps(in: reloaded)
        #expect(apps.contains { $0.title == "记事本" })
    }

    @Test func hostPathLauncherUsesWineDriveForOwnership() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let base = try makeBottle(store)
        let host = base.driveC.appendingPathComponent("Tools/a.exe").path
        let bottle = try store.saveLauncher(.init(id: "host", name: "Host", program: host, arguments: []), in: base)
        let app = try #require(AppCatalog(iconCache: IconCache(directory: home.appendingPathComponent("icons"))).apps(in: bottle).first { $0.title == "Host" })
        #expect(app.launchProgram == host)
        #expect(app.owns(WineProcess(pid: 1, bottleID: bottle.config.id, windowsImage: #"C:\Tools\a.exe"#)))
        #expect(WindowsPath.windowsPath(forHostPath: "/opt/Tools/a.exe", in: bottle) == #"Z:\opt\Tools\a.exe"#)
    }
}

extension BottleManagementTests {
    @Test func archiveRoundTripRenamesOnCollision() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try makeBottle(store)
        try store.snapshot(bottle, reason: "stays behind")
        let archive = try store.exportArchive(bottle, to: home.appendingPathComponent("out"))
        #expect(archive.pathExtension == "ciderbottle")
        let imported = try store.importArchive(archive)          // same id exists → new id
        #expect(imported.config.id != bottle.config.id)
        #expect(imported.config.name == "Test (导入)")
        #expect(read(imported) == "v1")
        #expect(store.snapshots(of: imported).isEmpty)
    }

    @Test func findsCrossOverAndWhiskyBottlesAndRefuses32Bit() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let fm = FileManager.default
        let fakeHome = home.appendingPathComponent("home")
        let cx = fakeHome.appendingPathComponent("Library/Application Support/CrossOver/Bottles/Galgame")
        let wk = fakeHome.appendingPathComponent("Library/Containers/com.isaacmarovitz.Whisky/Bottles/1234")
        for (dir, arch) in [(cx, "win32"), (wk, "win64")] {
            try fm.createDirectory(at: dir.appendingPathComponent("drive_c"), withIntermediateDirectories: true)
            try "WINE REGISTRY Version 2\n#arch=\(arch)\n".write(to: dir.appendingPathComponent("system.reg"), atomically: true, encoding: .utf8)
        }
        try (["name": "Whisky Games"] as NSDictionary).write(to: wk.appendingPathComponent("Metadata.plist"))
        let store = BottleStore(paths: paths)
        let found = store.foreignBottles(home: fakeHome)
        #expect(Set(found.map(\.name)) == ["Galgame", "Whisky Games"])
        let cx32 = try #require(found.first { $0.source == .crossover })
        #expect(throws: CiderError.self) { try store.importForeign(cx32, locale: .japanese) }
    }
}

@Suite struct DiagnosticsTests {
    @Test func redactsHomeAndAccountName() {
        let text = #"/Users/alice/Library/x C:\users\alice\AppData Z:\Users\alice alice-mbp"#
        let out = String(decoding: Diagnostics.redact(Data(text.utf8), home: "/Users/alice", user: "alice"), as: UTF8.self)
        #expect(!out.contains("alice"))
        #expect(out.contains("~/Library/x"))
        #expect(out.contains(#"C:\users\<user>\AppData"#))
    }

    @Test func truncatesHugeLogsKeepingBothEnds() {
        let data = Data(repeating: 65, count: 100) + Data(repeating: 66, count: 100)
        let out = Diagnostics.truncated(data, limit: 40)
        #expect(out.first == 65 && out.last == 66)
        #expect(out.count < data.count)
    }
}

extension BottleManagementTests {
    @Test func readsDWORDsFromWineRegistryFiles() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try makeBottle(store)
        let reg = """
        WINE REGISTRY Version 2

        [Software\\\\Microsoft\\\\VisualStudio\\\\14.0\\\\VC\\\\Runtimes\\\\x64] 1790517694
        #time=1dd4e88b4d64f74
        "Bld"=dword:00008681
        "Installed"=dword:00000001

        [Software\\\\Other] 1
        "Bld"=dword:0000ffff
        """
        try reg.write(to: bottle.prefix.appendingPathComponent("system.reg"), atomically: true, encoding: .utf8)
        let installer = RecipeInstaller(store: store, db: CompatDB())
        #expect(installer.registryDWORD(#"HKLM\Software\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"#, "Bld", in: bottle) == 0x8681)
        #expect(installer.registryDWORD(#"HKLM\Software\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"#, "Missing", in: bottle) == nil)
        #expect(installer.registryKeyExists(#"HKLM\Software\Other"#, in: bottle))
    }
}

extension BottleManagementTests {
    @Test func readsPerProgramWindowsVersion() throws {
        defer { try? FileManager.default.removeItem(at: home) }
        let store = BottleStore(paths: paths)
        let bottle = try makeBottle(store)
        try """
        WINE REGISTRY Version 2

        [Software\\\\Wine\\\\AppDefaults\\\\SenrenBanka.exe] 1790517694
        #time=1dd4e88b4d64f74
        "Version"="win7"
        """.write(to: bottle.prefix.appendingPathComponent("user.reg"), atomically: true, encoding: .utf8)
        #expect(store.registryString(#"HKCU\Software\Wine\AppDefaults\SenrenBanka.exe"#, "Version", in: bottle) == "win7")
        #expect(store.registryString(#"HKCU\Software\Wine\AppDefaults\Other.exe"#, "Version", in: bottle) == nil)
    }
}

extension BottleManagementTests {
    @Test func bottleSwitchesBecomeEnvironment() {
        #expect(BottleStore.environment(for: ["metalHUD": "1"]) == ["MTL_HUD_ENABLED": "1"])
        #expect(BottleStore.environment(for: [:]).isEmpty)
        #expect(BottleStore.environment(for: ["advertiseAVX": "1"])["ROSETTA_ADVERTISE_AVX"] == "1")   // dev Macs run macOS 15+
    }
}
