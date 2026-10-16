import CiderCore
import CiderSchema
import CiderStore
import Foundation
import Testing
@testable import CiderBottle
@testable import CiderIntegration

@Suite struct BottlePreparationTests {
    private func fixture() throws -> (URL, BottleStore) {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("owned-reservation-" + UUID().uuidString)
        let paths = CiderPaths.standard(environment: ["CIDER_HOME": root.path])
        let engine = paths.engines.appendingPathComponent("owned-no-runtime")
        try FileManager.default.createDirectory(at: engine, withIntermediateDirectories: true)
        let manifest = EngineManifest(id: "owned-no-runtime", channel: "fixture", wine: .init(version: "fixture", tree: "fixture"),
            root: "wine", requires: .init(macos: "14", rosetta: false), source: .init(origin: "fixture", sha256: String(repeating: "0", count: 64)))
        try JSONFile.write(manifest, to: engine.appendingPathComponent("manifest.json"))
        return (root, BottleStore(paths: paths))
    }

    @Test func reservationIsRegisteredBeforeVisibilityAndPrelaunchFailureRemovesOnlyItsDirectory() throws {
        let (root, store) = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        var reserved: Bottle?
        #expect(throws: CancellationError.self) {
            try store.create(name: "owned", locale: .japanese, createdBy: "fixture", onReserved: { bottle in
                reserved = bottle
                #expect(!FileManager.default.fileExists(atPath: bottle.configURL.path))
                let visible = try store.list()
                #expect(visible.isEmpty)
                return { throw CancellationError() }
            })
        }
        let bottle = try #require(reserved)
        #expect(!FileManager.default.fileExists(atPath: bottle.directory.path))
        #expect(!FileManager.default.fileExists(atPath: store.paths.logs.appendingPathComponent("audit/spawn.jsonl").path))
    }

    @Test func rejectingReservationDoesNotNeedWineToCleanEmptyDirectory() throws {
        let (root, store) = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(throws: CancellationError.self) {
            try store.create(name: "owned", locale: .japanese, createdBy: "fixture", onReserved: { _ in throw CancellationError() })
        }
        #expect(try FileManager.default.contentsOfDirectory(atPath: store.paths.bottles.path).isEmpty)
    }

    @Test func replacedReservationIsPreservedAndNeverCleanedAsOurPrefix() throws {
        let (root, store) = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        var reserved: Bottle?
        let original = root.appendingPathComponent("original-reservation")
        #expect(throws: CiderError.self) {
            try store.create(name: "owned", locale: .japanese, createdBy: "fixture", onReserved: { bottle in
                reserved = bottle
                try FileManager.default.moveItem(at: bottle.directory, to: original)
                try FileManager.default.createDirectory(at: bottle.directory, withIntermediateDirectories: false)
                try Data("replacement must survive".utf8).write(to: bottle.directory.appendingPathComponent("sentinel"))
                throw CancellationError()
            })
        }
        let bottle = try #require(reserved)
        #expect(try String(contentsOf: bottle.directory.appendingPathComponent("sentinel"), encoding: .utf8) == "replacement must survive")
        #expect(FileManager.default.fileExists(atPath: original.path))
        #expect(!FileManager.default.fileExists(atPath: bottle.configURL.path))
    }

    @Test func activityPresenceIsScopedAndRemovedOnlyAfterFinish() throws {
        let (root, store) = try fixture()
        defer { try? FileManager.default.removeItem(at: root) }
        let config = BottleConfig(id: "owned", name: "owned", createdBy: "tests", createdAt: "owned",
            template: .win10_64, engine: .init(id: "owned-no-runtime"), locale: .japanese)
        let a = Bottle(config: config, directory: root.appendingPathComponent("a"))
        let b = Bottle(config: config, directory: root.appendingPathComponent("b"))
        let activity = try RecipeInstaller.beginActivity(in: a)
        #expect(RecipeInstaller.hasActiveActivity(in: a))
        #expect(!RecipeInstaller.hasActiveActivity(in: b))
        try activity.finish()
        #expect(!RecipeInstaller.hasActiveActivity(in: a))
        #expect(try store.list().isEmpty)
    }
}
