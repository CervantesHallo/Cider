import Foundation
import Testing
@testable import CiderCore
@testable import CiderRuntime

@Suite struct QuarantinePathTests {
    func fixture() throws -> URL {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("cider-quarantine-path-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root.appendingPathComponent("prefix/drive_c"), withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: root.appendingPathComponent("prefix/dosdevices"), withIntermediateDirectories: true)
        return root
    }

    @Test func longestDriveRootAndLexicalTiesRemainDeterministic() throws {
        let root = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        let prefix = root.appendingPathComponent("prefix"), c = prefix.appendingPathComponent("drive_c")
        let nested = c.appendingPathComponent("Assets")
        try FileManager.default.createDirectory(at: nested, withIntermediateDirectories: true)
        for drive in ["e:", "d:"] {
            try FileManager.default.createSymbolicLink(at: prefix.appendingPathComponent("dosdevices/" + drive), withDestinationURL: nested)
        }
        let map = WinePathMapping.DriveMap(prefix: prefix)
        #expect(map.windowsPath(forHostPath: nested.appendingPathComponent("game.exe").path) == #"D:\game.exe"#)
        #expect(map.windowsPath(forHostPath: nested.path) == #"D:\"#)
        #expect(map.windowsPath(forHostPath: c.appendingPathComponent("Tools/app.exe").path) == #"C:\Tools\app.exe"#)
        #expect(map.windowsPath(forHostPath: c.path) == #"C:\"#)
        #expect(map.windowsPath(forHostPath: c.path + "-outside/app.exe").hasPrefix("Z:"))
        let alias = root.appendingPathComponent("alias.exe")
        try FileManager.default.createSymbolicLink(at: alias, withDestinationURL: nested.appendingPathComponent("game.exe"))
        try Data().write(to: nested.appendingPathComponent("game.exe"))
        #expect(map.windowsPath(forHostPath: alias.path) == #"D:\game.exe"#)
    }

    @Test func oneDriveSnapshotPerPrefixPerScan() throws {
        let root = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        let a = root.appendingPathComponent("prefix"), b = root.appendingPathComponent("another")
        var builds: [String] = []
        var cache = ProcessScanner.HostPathCache()
        for prefix in [a, a, b, a, b] {
            let image = cache.windowsPath(forHostPath: prefix.appendingPathComponent("drive_c/Tools/app.exe").path,
                                         prefixPath: prefix.path, makeMap: { url in
                builds.append(url.path)
                return WinePathMapping.DriveMap(prefix: url)
            })
            #expect(image == #"C:\Tools\app.exe"#)
        }
        #expect(builds == [a.path, b.path])
    }

    @Test func nextScanObservesChangedDrives() throws {
        let root = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        let prefix = root.appendingPathComponent("prefix"), old = root.appendingPathComponent("old"), new = root.appendingPathComponent("new")
        for directory in [old, new] { try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true) }
        let drive = prefix.appendingPathComponent("dosdevices/d:")
        try FileManager.default.createSymbolicLink(at: drive, withDestinationURL: old)
        var scan = ProcessScanner.HostPathCache()
        #expect(scan.windowsPath(forHostPath: old.appendingPathComponent("app.exe").path, prefixPath: prefix.path) == #"D:\app.exe"#)
        try FileManager.default.removeItem(at: drive)
        try FileManager.default.createSymbolicLink(at: drive, withDestinationURL: new)
        #expect(scan.windowsPath(forHostPath: old.appendingPathComponent("app.exe").path, prefixPath: prefix.path) == #"D:\app.exe"#)
        var next = ProcessScanner.HostPathCache()
        #expect(next.windowsPath(forHostPath: new.appendingPathComponent("app.exe").path, prefixPath: prefix.path) == #"D:\app.exe"#)
        #expect(next.windowsPath(forHostPath: old.appendingPathComponent("app.exe").path, prefixPath: prefix.path).hasPrefix("Z:"))
    }

    @Test func unrelatedDosdevicesEntriesDoNotChangeMapping() throws {
        let root = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        let prefix = root.appendingPathComponent("prefix")
        for name in ["not-a-drive", "abc:", "1:", "界:", "x::"] {
            try Data().write(to: prefix.appendingPathComponent("dosdevices/" + name))
        }
        #expect(WinePathMapping.DriveMap(prefix: prefix).windowsPath(forHostPath: prefix.appendingPathComponent("drive_c/app.exe").path) == #"C:\app.exe"#)
    }
}

// The product AppModel owns this pure filter; CiderKit deliberately has no UI
// dependency. A native probe compiles the exact LibraryFileEvents enum from the
// AppModel source with this file and the define below, without launching an app.
#if QUARANTINE_FSEVENT_PROBE
@Suite struct QuarantineFileEventTests {
    let root = "/private/native-fixture/Bottles"

    @Test func topLevelAndMetadataCoverage() {
        for path in [root, root + "/", root + "/new", root + "/new/", root + "/.staging",
                     root + "/new/a.lnk", root + "/new/appmanifest_1.acf", root + "/new/cider-bottle.json",
                     root + "/new/libraryfolders.vdf", root + "/new/librarycache/cover.jpg", "/fixture/Data/new.json"] {
            #expect(LibraryFileEvents.isRelevant(path, bottleRoot: root))
        }
        for path in [root + "2/new", root + "/new/prefix/drive_c/Game/assets/1.dat",
                     root + "/new/.cache/asset.dat", root + "/new/.git/objects/blob", root + "/new/GAME.LNK"] {
            #expect(!LibraryFileEvents.isRelevant(path, bottleRoot: root))
        }
    }

    @Test func onlyRealDotSegmentsNormalize() {
        #expect(LibraryFileEvents.isRelevant(root + "/old/../new", bottleRoot: root))
        #expect(LibraryFileEvents.isRelevant(root + "/new/.", bottleRoot: root))
        #expect(LibraryFileEvents.isRelevant(root + "/new/..", bottleRoot: root))
        #expect(LibraryFileEvents.isRelevant(root + "//new", bottleRoot: root))
        #expect(!LibraryFileEvents.isRelevant(root + "/new/.cache/blob", bottleRoot: root))
        #expect(!LibraryFileEvents.isRelevant(root + "/new/prefix/./asset.dat", bottleRoot: root))
        #expect(!LibraryFileEvents.isRelevant(root + "/..", bottleRoot: root))
    }
}
#endif
