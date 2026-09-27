import Foundation
import Testing
@testable import CiderCore
@testable import CiderStore

@Suite struct D3DMetalImporterTests {
    @Test func importsAGPTKStyleFolderUnmodified() throws {
        let fm = FileManager.default
        let home = fm.temporaryDirectory.appendingPathComponent("cider-d3dm-\(UUID().uuidString)")
        defer { _ = try? Command.run("/bin/chmod", ["-R", "u+w", home.path]); try? fm.removeItem(at: home) }
        let lib = home.appendingPathComponent("gptk/redist/lib")
        let resources = lib.appendingPathComponent("external/D3DMetal.framework/Resources")
        try fm.createDirectory(at: resources, withIntermediateDirectories: true)
        try (["CFBundleShortVersionString": "3.0"] as NSDictionary).write(to: resources.appendingPathComponent("Info.plist"))
        try Data("fake".utf8).write(to: lib.appendingPathComponent("external/libd3dshared.dylib"))
        try fm.createDirectory(at: lib.appendingPathComponent("wine/x86_64-windows"), withIntermediateDirectories: true)
        try fm.createDirectory(at: lib.appendingPathComponent("wine/x86_64-unix"), withIntermediateDirectories: true)
        try Data("MZ".utf8).write(to: lib.appendingPathComponent("wine/x86_64-windows/d3d12.dll"))
        try fm.createSymbolicLink(atPath: lib.appendingPathComponent("wine/x86_64-unix/d3d12.so").path,
                                  withDestinationPath: "../../external/libd3dshared.dylib")
        try "{\\rtf1 license}".write(to: home.appendingPathComponent("gptk/License.rtf"), atomically: true, encoding: .utf8)

        let importer = D3DMetalImporter(paths: .standard(environment: ["CIDER_HOME": home.appendingPathComponent("cider").path]))
        let info = try importer.importPackage(at: home.appendingPathComponent("gptk"))
        #expect(info.version == "3.0")
        #expect(info.codesignValid == false)            // the fake isn't signed; recorded, not hidden
        #expect(info.license == "License.rtf")
        let dir = try #require(importer.installed().first?.directory)
        let link = try fm.destinationOfSymbolicLink(atPath: dir.appendingPathComponent("lib/wine/x86_64-unix/d3d12.so").path)
        #expect(link == "../../external/libd3dshared.dylib")
        let hashes = try String(contentsOf: dir.appendingPathComponent("files.sha256"), encoding: .utf8)
        #expect(hashes.contains("lib/wine/x86_64-windows/d3d12.dll"))
        #expect(throws: D3DMetalImporter.ImportError.self) { try importer.importPackage(at: home.appendingPathComponent("gptk")) }
    }
}
