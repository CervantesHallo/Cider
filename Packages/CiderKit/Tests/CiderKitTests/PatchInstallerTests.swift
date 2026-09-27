import Foundation
import Testing
@testable import CiderIntegration
@testable import CiderStore

@Suite struct PatchInstallerTests {
    let root = FileManager.default.temporaryDirectory.appendingPathComponent("cider-patch-\(UUID().uuidString)")
    var game: URL { root.appendingPathComponent("game") }
    var history: URL { root.appendingPathComponent("history") }
    var drop: URL { root.appendingPathComponent("drop") }

    func setUp() throws {
        let fm = FileManager.default
        try fm.createDirectory(at: game, withIntermediateDirectories: true)
        try fm.createDirectory(at: drop, withIntermediateDirectories: true)
        try "original".write(to: game.appendingPathComponent("data.xp3"), atomically: true, encoding: .utf8)
    }

    func read(_ url: URL) -> String? { try? String(contentsOf: url, encoding: .utf8) }

    @Test func installsBacksUpAndUndoes() throws {
        try setUp()
        defer { try? FileManager.default.removeItem(at: root) }
        try "patched".write(to: drop.appendingPathComponent("data.xp3"), atomically: true, encoding: .utf8)
        try "new".write(to: drop.appendingPathComponent("patch.xp3"), atomically: true, encoding: .utf8)
        let installer = PatchInstaller(gameRoot: game, historyRoot: history)

        let manifest = try installer.install([drop.appendingPathComponent("data.xp3"), drop.appendingPathComponent("patch.xp3")])
        #expect(manifest.added == ["patch.xp3"])
        #expect(manifest.replaced == ["data.xp3"])
        #expect(read(game.appendingPathComponent("data.xp3")) == "patched")
        #expect(read(game.appendingPathComponent("patch.xp3")) == "new")

        try installer.undoLast()
        #expect(read(game.appendingPathComponent("data.xp3")) == "original")
        #expect(!FileManager.default.fileExists(atPath: game.appendingPathComponent("patch.xp3").path))
        #expect(installer.lastInstall() == nil)
    }

    @Test func unwrapsZipWithSingleTopFolder() throws {
        try setUp()
        defer { try? FileManager.default.removeItem(at: root) }
        let folder = drop.appendingPathComponent("SenrenBanka_Patch")
        try FileManager.default.createDirectory(at: folder.appendingPathComponent("sub"), withIntermediateDirectories: true)
        try "p2".write(to: folder.appendingPathComponent("patch2.xp3"), atomically: true, encoding: .utf8)
        try "cfg".write(to: folder.appendingPathComponent("sub/extra.ini"), atomically: true, encoding: .utf8)
        let zip = drop.appendingPathComponent("patch.zip")
        try Command.run("/usr/bin/ditto", ["-c", "-k", "--keepParent", folder.path, zip.path])

        let manifest = try PatchInstaller(gameRoot: game, historyRoot: history).install([zip])
        #expect(Set(manifest.added) == ["patch2.xp3", "sub/extra.ini"])
        #expect(read(game.appendingPathComponent("patch2.xp3")) == "p2")
        #expect(read(game.appendingPathComponent("sub/extra.ini")) == "cfg")
    }

    @Test func rejects7zBeforeTouchingTheGame() throws {
        try setUp()
        defer { try? FileManager.default.removeItem(at: root) }
        let archive = drop.appendingPathComponent("patch.7z")
        try Data([0x37, 0x7A]).write(to: archive)
        #expect(throws: PatchInstaller.PatchError.self) { try PatchInstaller(gameRoot: game, historyRoot: history).install([archive]) }
        #expect(read(game.appendingPathComponent("data.xp3")) == "original")
    }
}
