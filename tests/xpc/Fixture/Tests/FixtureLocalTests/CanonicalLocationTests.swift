import CiderCore
import CiderIPC
import Foundation
import Testing

@Suite struct FixtureLocalTests {
    @Test func canonicalRootAndServiceLabelSurviveMissingDirectoryCreation() throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("cider-canonical-fixture-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let bundle = directory.appendingPathComponent("Fixture.app", isDirectory: true)
        try FileManager.default.createDirectory(at: bundle, withIntermediateDirectories: true)
        var aliases = [directory]
        if directory.path.hasPrefix("/var/") { aliases.append(URL(fileURLWithPath: "/private" + directory.path)) }
        for (index, alias) in aliases.enumerated() {
            let missing = alias.appendingPathComponent("absent-\(index)/nested/AppSupport", isDirectory: true)
            let paths = CiderPaths(appSupport: missing, caches: directory, logs: directory)
            let canonicalBefore = canonicalPath(missing)
            let before = try RPCLocation(bundle: bundle, paths: paths)
            #expect(!FileManager.default.fileExists(atPath: missing.path))
            try FileManager.default.createDirectory(at: missing, withIntermediateDirectories: true)
            let after = try RPCLocation(bundle: bundle, paths: paths)
            #expect(canonicalPath(missing) == canonicalBefore)
            #expect(before.root == after.root)
            #expect(before.label == after.label)
            #expect(before.clientService == after.clientService)
            #expect(before.hostService == after.hostService)
        }
    }
}
