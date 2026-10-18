import CiderBottle
import CiderCore
import CiderData
import CiderStore
import Foundation
import Testing

@Suite struct QuarantineResourcesTests {
    private func temporaryDirectory() throws -> URL {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("cider-resources-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        return root
    }

    private func write(_ contents: String, at file: URL) throws {
        try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try contents.write(to: file, atomically: true, encoding: .utf8)
    }

    private let emptyIndex = #"{"schema":"cider.engines/v1","engines":[]}"#

    private func paths(_ root: URL) -> CiderPaths {
        CiderPaths(appSupport: root.appendingPathComponent("support"), caches: root.appendingPathComponent("cache"), logs: root.appendingPathComponent("logs"))
    }

    @Test(arguments: ["Custom.app", "arbitrary location/Renamed Cider.app", "out/Cider.app", "Uppercase.APP"])
    func ownBundleWinsOverCheckoutAndOtherInstallation(_ location: String) throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let resources = root.appendingPathComponent(location + "/Contents/Resources")
        let cwd = root.appendingPathComponent("unrelated cwd", isDirectory: true)
        let installed = root.appendingPathComponent("Applications/Cider.app/Contents/Resources/data")
        let ownIndex = resources.appendingPathComponent("data/engines/index.json")
        try write(emptyIndex, at: ownIndex)
        try write(emptyIndex, at: cwd.appendingPathComponent("data/engines/index.json"))
        try write(emptyIndex, at: installed.appendingPathComponent("engines/index.json"))
        let locator = CiderResources(environment: [:], bundledResources: resources, currentDirectory: cwd, installedData: installed)
        #expect(locator.engineIndexURL()?.path == ownIndex.path)
        #expect(EngineIndex.load(from: try #require(locator.engineIndexURL())) != nil)
        #expect(locator.compatDirectories(paths: paths(root)).map(\.path) == [resources.appendingPathComponent("data").path, paths(root).appSupport.appendingPathComponent("Data").path])
    }

    @Test func explicitIndexAndDataOverridesAreAuthoritative() throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let resources = root.appendingPathComponent("Custom.app/Contents/Resources")
        try write(emptyIndex, at: resources.appendingPathComponent("data/engines/index.json"))
        let data = root.appendingPathComponent("override data")
        let overridden = data.appendingPathComponent("engines/index.json")
        let explicit = root.appendingPathComponent("specific index.json")
        try write(emptyIndex, at: overridden)
        try write(emptyIndex, at: explicit)
        let locator = CiderResources(environment: ["CIDER_DATA": "override data"], bundledResources: resources, currentDirectory: root)
        #expect(locator.engineIndexURL()?.path == overridden.path)
        #expect(locator.engineIndexURL(explicitIndex: "specific index.json")?.path == explicit.path)
        #expect(locator.engineIndexURL(explicitIndex: explicit.path)?.path == explicit.path)
        #expect(locator.engineIndexURL(explicitIndex: "absent.json")?.path == root.appendingPathComponent("absent.json").path)
        try write("malformed", at: overridden)
        #expect(EngineIndex.load(from: try #require(locator.engineIndexURL())) == nil)
        try FileManager.default.removeItem(at: overridden)
        #expect(locator.engineIndexURL()?.path == overridden.path)
        #expect(EngineIndex.load(from: try #require(locator.engineIndexURL())) == nil)
    }

    @Test func missingOrMalformedBundledIndexNeverUsesAnotherApp() throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let resources = root.appendingPathComponent("Custom.app/Contents/Resources")
        let index = resources.appendingPathComponent("data/engines/index.json")
        let installed = root.appendingPathComponent("installed-data")
        try write(emptyIndex, at: installed.appendingPathComponent("engines/index.json"))
        try write(emptyIndex, at: root.appendingPathComponent("data/engines/index.json"))
        let locator = CiderResources(environment: [:], bundledResources: resources, currentDirectory: root, installedData: installed)
        #expect(locator.engineIndexURL()?.path == index.path)
        #expect(EngineIndex.load(from: try #require(locator.engineIndexURL())) == nil)
        try write("malformed", at: index)
        #expect(EngineIndex.load(from: try #require(locator.engineIndexURL())) == nil)
    }

    @Test func standaloneUsesCheckoutThenLegacyInstallation() throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let installed = root.appendingPathComponent("installed-data")
        let checkoutIndex = root.appendingPathComponent("data/engines/index.json")
        let installedIndex = installed.appendingPathComponent("engines/index.json")
        let locator = CiderResources(environment: ["CIDER_DATA": ""], bundledResources: nil, currentDirectory: root, installedData: installed)
        #expect(locator.engineIndexURL() == nil)
        try write(emptyIndex, at: installedIndex)
        #expect(locator.engineIndexURL()?.path == installedIndex.path)
        try write(emptyIndex, at: checkoutIndex)
        #expect(locator.engineIndexURL()?.path == checkoutIndex.path)
        #expect(locator.compatDirectories(paths: paths(root)).map(\.path) == [installed.path, root.appendingPathComponent("data").path, paths(root).appSupport.appendingPathComponent("Data").path])
    }

    @Test func compatibilityOverlaysUseUserThenExplicitData() throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let resources = root.appendingPathComponent("Custom.app/Contents/Resources")
        let data = root.appendingPathComponent("override-data")
        let userData = paths(root).appSupport.appendingPathComponent("Data")
        let installed = root.appendingPathComponent("installed-data")
        let bundled = resources.appendingPathComponent("data")
        let locator = CiderResources(environment: ["CIDER_DATA": data.path], bundledResources: resources, currentDirectory: root, installedData: installed)
        for (dir, name) in [(bundled, "bundle"), (userData, "user"), (data, "override"), (installed, "other-app"), (root.appendingPathComponent("data"), "cwd")] {
            try write(#"{"schema":"cider.game/v1","id":"resource-probe","names":{"en":"\#(name)"},"verdicts":[]}"#, at: dir.appendingPathComponent("game.json"))
        }
        let directories = locator.compatDirectories(paths: paths(root))
        #expect(directories.map(\.path) == [bundled.path, userData.path, data.path])
        #expect(CompatDB(directories: directories).games["resource-probe"]?.names["en"] == "override")
        try FileManager.default.removeItem(at: data)
        #expect(CompatDB(directories: directories).games["resource-probe"]?.names["en"] == "user")
        try FileManager.default.removeItem(at: userData)
        #expect(CompatDB(directories: directories).games["resource-probe"]?.names["en"] == "bundle")
    }

    @Test func bottleStoreUsesSharedLocator() {
        let p = paths(FileManager.default.temporaryDirectory)
        #expect(BottleStore.compatDirectories(paths: p) == CiderResources().compatDirectories(paths: p))
    }
}
