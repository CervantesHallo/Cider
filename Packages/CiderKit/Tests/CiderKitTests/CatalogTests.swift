import Foundation
import Testing
@testable import CiderIntegration
@testable import CiderPE
@testable import CiderRuntime

@Suite struct OwnershipTests {
    func app(_ kind: CatalogApp.Kind) -> CatalogApp {
        CatalogApp(id: "b/x", bottleID: "b", bottleName: "B", title: "X", kind: kind, installState: .installed,
                   sizeOnDisk: 0, lastPlayed: nil, iconURL: nil, headerURL: nil, heroURL: nil,
                   launchProgram: "x", launchArguments: [], workingDirectory: nil)
    }

    func proc(_ image: String, bottle: String = "b") -> WineProcess {
        WineProcess(pid: 1, bottleID: bottle, windowsImage: image)
    }

    @Test func steamClientOwnsItsTreeButNotGames() {
        let steam = app(.steamClient)
        #expect(steam.owns(proc(#"C:\Program Files (x86)\Steam\steam.exe"#)))
        #expect(steam.owns(proc(#"C:\Program Files (x86)\Steam\bin\cef\cef.win64\steamwebhelper.exe"#)))
        #expect(steam.owns(proc(#"C:\Program Files (x86)\Common Files\Steam\steamservice.exe"#)))
        #expect(!steam.owns(proc(#"C:\Program Files (x86)\Steam\steamapps\common\Senren Banka\SenrenBanka.exe"#)))
        #expect(!steam.owns(proc(#"C:\Program Files (x86)\Steam\steam.exe"#, bottle: "other")))
    }

    @Test func steamGameMatchesByInstallDirCaseInsensitively() {
        let game = app(.steamGame(appID: "1144400", installDir: "Senren Banka"))
        #expect(game.owns(proc(#"C:\Program Files (x86)\Steam\steamapps\common\SENREN BANKA\SenrenBanka.exe"#)))
        #expect(!game.owns(proc(#"C:\Program Files (x86)\Steam\steamapps\common\Riddle Joker\RiddleJoker.exe"#)))
    }

    @Test func programMatchesOnlyTargetAndDeclaredHelpersButNeverSystemFolders() {
        var npp = app(.program(target: #"C:\Program Files\Notepad++\notepad++.exe"#))
        npp.ownedProcessNames = ["gup.exe"]
        #expect(npp.owns(proc(#"C:\Program Files\Notepad++\notepad++.exe"#)))
        #expect(npp.owns(proc(#"C:\Program Files\Notepad++\updater\gup.exe"#)))
        #expect(!npp.owns(proc(#"C:\Program Files\Notepad++\independent.exe"#)))
        #expect(!npp.owns(proc(#"C:\windows\system32\explorer.exe"#)))
        let sys = app(.program(target: #"C:\windows\system32\notepad.exe"#))
        #expect(sys.owns(proc(#"C:\windows\system32\notepad.exe"#)))
        #expect(!sys.owns(proc(#"C:\windows\system32\cmd.exe"#)))
    }

    @Test func wineInfrastructureBelongsToNoApp() {
        #expect(proc(#"C:\windows\system32\explorer.exe"#).isWineInfrastructure)
        #expect(proc(#"C:\windows\system32\winedevice.exe"#).isWineInfrastructure)
        #expect(!proc(#"C:\Program Files\App\explorer.exe"#).isWineInfrastructure)
    }

    @Test func launcherDoesNotOwnNestedGameLibraries() {
        var launcher = app(.program(target: #"C:\Program Files\miHoYo Launcher\launcher.exe"#))
        launcher.ownedProcessNames = ["hyp.exe", "hyphelper.exe"]
        launcher.ownedProcessInstallationScopes = ["C:/Program Files/miHoYo Launcher"]
        #expect(launcher.owns(proc(#"C:\Program Files\miHoYo Launcher\1.18.0\HYP.exe"#)))
        #expect(launcher.owns(proc(#"C:\Program Files\miHoYo Launcher\1.18.0\HYPHelper.exe"#)))
        #expect(launcher.owns(proc(#"C:\Program Files\miHoYo Launcher\1.18.0\HYPHelper"#)))
        #expect(!launcher.owns(proc(#"C:\Program Files\miHoYo Launcher\1.18.0\HYPHelper.bak"#)))
        #expect(!launcher.owns(proc(#"C:\Program Files\miHoYo Launcher\games\Genshin Impact Game\YuanShen.exe"#)))
        let game = app(.program(target: #"C:\Program Files\miHoYo Launcher\games\Genshin Impact Game\YuanShen.exe"#))
        #expect(!game.owns(proc(#"C:\Program Files\miHoYo Launcher\games\Genshin Impact Game\UnityCrashHandler64.exe"#)))
        #expect(!game.owns(proc(#"C:\Program Files\miHoYo Launcher\1.18.0\HYP.exe"#)))
    }

    @Test func independentProgramsInSameFolderAreNotOwned() {
        let a = app(.program(target: #"C:\Tools\a.exe"#))
        #expect(!a.owns(proc(#"C:\Tools\b.exe"#)))
    }

    @Test func splitsQuotedArguments() {
        let catalog = AppCatalog(iconCache: IconCache(directory: FileManager.default.temporaryDirectory))
        #expect(catalog.splitArguments(#"-a "two words" -b"#) == ["-a", "two words", "-b"])
        #expect(catalog.splitArguments("") == [])
    }
}

@Suite struct ShellLinkTests {
    /// Builds a minimal .lnk with a LinkInfo LocalBasePath and Unicode string data (working dir + arguments).
    func makeLink(target: String, workingDir: String, arguments: String) -> Data {
        func le16(_ v: Int) -> Data { withUnsafeBytes(of: UInt16(v).littleEndian) { Data($0) } }
        func le32(_ v: Int) -> Data { withUnsafeBytes(of: UInt32(v).littleEndian) { Data($0) } }
        var d = Data()
        d.append(le32(0x4C))
        d.append(Data(repeating: 0, count: 16))                   // CLSID (unchecked)
        d.append(le32(0x02 | 0x10 | 0x20 | 0x80))                  // HasLinkInfo | HasWorkingDir | HasArguments | IsUnicode
        d.append(Data(repeating: 0, count: 0x4C - d.count))        // rest of the header
        // LinkInfo: header 0x1C bytes, then VolumeID stub and the ANSI base path.
        let base = Data(target.utf8) + Data([0])
        let volumeID = le32(0x10) + le32(3) + le32(0) + le32(0x10)
        let headerSize = 0x1C
        let volumeOffset = headerSize
        let baseOffset = volumeOffset + volumeID.count
        let suffixOffset = baseOffset + base.count
        let total = suffixOffset + 1
        d.append(le32(total) + le32(headerSize) + le32(1) + le32(volumeOffset) + le32(baseOffset) + le32(0) + le32(suffixOffset))
        d.append(volumeID + base + Data([0]))
        for s in [workingDir, arguments] {
            let units = Array(s.utf16)
            d.append(le16(units.count))
            for u in units { d.append(le16(Int(u))) }
        }
        return d
    }

    @Test func parsesLinkInfoTargetAndStrings() throws {
        let data = makeLink(target: #"C:\Program Files\Notepad++\notepad++.exe"#, workingDir: #"C:\Program Files\Notepad++"#, arguments: "-multiInst")
        let link = try #require(ShellLink.parse(data))
        #expect(link.target == #"C:\Program Files\Notepad++\notepad++.exe"#)
        #expect(link.workingDirectory == #"C:\Program Files\Notepad++"#)
        #expect(link.arguments == "-multiInst")
    }

    @Test func rejectsNonLinks() {
        #expect(ShellLink.parse(Data("not a link".utf8)) == nil)
    }
}

@Suite struct PEIconTests {
    @Test func rejectsNonPE() {
        #expect(PEIcon.icoData(fromPE: Data(repeating: 0, count: 256)) == nil)
    }
}
