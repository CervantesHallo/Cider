import Foundation
import Testing
@testable import CiderBottle
@testable import CiderIntegration
@testable import CiderSchema

@Suite struct VDFTests {
    @Test func parsesNestedObjectsCommentsAndEscapes() throws {
        let vdf = try VDF.parse(#"""
        // comment
        "libraryfolders"
        {
            "0"
            {
                "path"		"C:\\Program Files (x86)\\Steam"
                "apps" { "1144400" "7812345678" }
            }
            Unquoted  value
        }
        """#)
        let folders = try #require(vdf["LibraryFolders"])
        #expect(folders["0"]?["path"]?.string == #"C:\Program Files (x86)\Steam"#)
        #expect(folders["0"]?["apps"]?["1144400"]?.string == "7812345678")
        #expect(folders["Unquoted"]?.string == "value")
    }

    @Test func rejectsUnbalancedInput() {
        #expect(throws: VDF.ParseError.self) { try VDF.parse(#""a" { "b" "c""#) }
        #expect(throws: VDF.ParseError.self) { try VDF.parse("}") }
    }
}

@Suite struct SteamLibraryTests {
    /// A minimal fake bottle with a Steam folder, one installed game and one downloading.
    func makeBottle() throws -> Bottle {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("cider-steam-\(UUID().uuidString)")
        let config = BottleConfig(id: "steam-test", name: "Steam", createdBy: "test", createdAt: "2026-09-27T00:00:00Z",
                                  template: .win10_64, engine: .init(id: "x"), locale: .simplifiedChinese)
        let bottle = Bottle(config: config, directory: dir)
        let steam = bottle.driveC.appendingPathComponent("Program Files (x86)/Steam")
        let steamapps = steam.appendingPathComponent("steamapps")
        try FileManager.default.createDirectory(at: steamapps.appendingPathComponent("common"), withIntermediateDirectories: true)
        try Data().write(to: steam.appendingPathComponent("steam.exe"))
        try #"""
        "libraryfolders" { "0" { "path" "C:\\Program Files (x86)\\Steam" } }
        """#.write(to: steamapps.appendingPathComponent("libraryfolders.vdf"), atomically: true, encoding: .utf8)
        try #"""
        "AppState" { "appid" "1144400" "name" "Senren＊Banka" "installdir" "Senren Banka" "StateFlags" "4" "SizeOnDisk" "6000000000" "LastPlayed" "1790000000" }
        """#.write(to: steamapps.appendingPathComponent("appmanifest_1144400.acf"), atomically: true, encoding: .utf8)
        try #"""
        "AppState" { "appid" "888790" "name" "Sabbat of the Witch" "installdir" "Sabbat of the Witch" "StateFlags" "1026" }
        """#.write(to: steamapps.appendingPathComponent("appmanifest_888790.acf"), atomically: true, encoding: .utf8)
        try #"""
        "AppState" { "appid" "228980" "name" "Steamworks Common Redistributables" "StateFlags" "4" }
        """#.write(to: steamapps.appendingPathComponent("appmanifest_228980.acf"), atomically: true, encoding: .utf8)
        return bottle
    }

    @Test func listsGamesSkippingRedistributables() throws {
        let bottle = try makeBottle()
        defer { try? FileManager.default.removeItem(at: bottle.directory) }
        let library = SteamLibrary(bottle: bottle)
        #expect(library.steamExecutableWindowsPath == #"C:\Program Files (x86)\Steam\steam.exe"#)
        let games = try library.games()
        #expect(games.map(\.appID) == ["1144400", "888790"])
        #expect(games[0].isInstalled)
        #expect(!games[1].isInstalled)
        #expect(games[0].directory.path.hasSuffix("steamapps/common/Senren Banka"))
        #expect(games[0].lastPlayed == Date(timeIntervalSince1970: 1_790_000_000))
    }

    @Test func mapsWindowsPathsIntoDriveC() throws {
        let bottle = try makeBottle()
        defer { try? FileManager.default.removeItem(at: bottle.directory) }
        let url = try WindowsPath.hostURL(for: #"c:\users\Public"#, in: bottle)
        #expect(url.path == bottle.driveC.appendingPathComponent("users/Public").path)
        #expect(throws: (any Error).self) { try WindowsPath.hostURL(for: "relative\\path", in: bottle) }
    }
}
