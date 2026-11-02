import CiderCore
import CiderSchema
import CiderStore
import Foundation
import Testing

@Suite struct EngineDefaultSelectionTests {
    private func manifest(_ id: String, eligible: Bool? = nil) -> EngineManifest {
        EngineManifest(id: id, channel: "fixture", wine: .init(version: "fixture", tree: "fixture"),
            root: "wine", requires: .init(macos: "14", rosetta: false),
            source: .init(origin: "fixture", sha256: String(repeating: "0", count: 64)),
            defaultEligible: eligible)
    }

    private func store(_ values: [EngineManifest]) throws -> (URL, EngineStore) {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("engine-selection-" + UUID().uuidString)
        let paths = CiderPaths.standard(environment: ["CIDER_HOME": root.path])
        for value in values {
            let directory = paths.engines.appendingPathComponent(value.id)
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            try JSONFile.write(value, to: directory.appendingPathComponent("manifest.json"))
        }
        return (root, EngineStore(paths: paths))
    }

    @Test func previewCannotDisplaceLegacyDefaultButRemainsExplicitlySelectable() throws {
        let (root, engines) = try store([manifest("a-legacy"), manifest("z-preview", eligible: false)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(try engines.defaultEngine().manifest.id == "a-legacy")
        #expect(try engines.engine("z-preview").manifest.defaultEligible == false)
        #expect(try engines.list().count == 2)
    }

    @Test func previewOnlyRequiresExplicitChoice() throws {
        let (root, engines) = try store([manifest("only-preview", eligible: false)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(throws: CiderError.self) { try engines.defaultEngine() }
        #expect(try engines.engine("only-preview").manifest.id == "only-preview")
    }

    @Test func knownPublishedIDDoesNotOverrideOptOut() throws {
        let (root, engines) = try store([
            manifest("cider-cx26.3-r1-x86_64", eligible: false), manifest("a-legacy")
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(try engines.defaultEngine().manifest.id == "a-legacy")
    }

    @Test func manifestRoundTripPreservesExplicitFalseAndLegacyAbsence() throws {
        let encoder = JSONEncoder()
        let decoder = JSONDecoder()
        let legacy = try encoder.encode(manifest("legacy"))
        #expect(!String(decoding: legacy, as: UTF8.self).contains("default_eligible"))
        #expect(try decoder.decode(EngineManifest.self, from: legacy).defaultEligible == nil)
        let preview = try encoder.encode(manifest("preview", eligible: false))
        #expect(try decoder.decode(EngineManifest.self, from: preview).defaultEligible == false)
    }
}
