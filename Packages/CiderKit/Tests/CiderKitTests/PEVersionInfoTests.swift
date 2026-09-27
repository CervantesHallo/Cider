import Foundation
import Testing
@testable import CiderPE

@Suite struct PEVersionInfoTests {
    /// Builds one version-info node: wLength, wValueLength, wType, key, padding, value, padding, children.
    func node(_ key: String, text: String? = nil, binary: Data? = nil, children: [Data] = []) -> Data {
        func pad(_ d: inout Data) { while d.count % 4 != 0 { d.append(0) } }
        var body = Data()
        let keyUnits = Array(key.utf16) + [0]
        for u in keyUnits { body.append(contentsOf: withUnsafeBytes(of: u.littleEndian, Array.init)) }
        var header = 6 + body.count
        var valueData = Data(), valueLength = 0, type: UInt16 = 0
        if let text {
            let units = Array(text.utf16) + [0]
            for u in units { valueData.append(contentsOf: withUnsafeBytes(of: u.littleEndian, Array.init)) }
            valueLength = units.count; type = 1
        } else if let binary { valueData = binary; valueLength = binary.count }
        var out = Data(count: 6) + body
        pad(&out); header = out.count
        out += valueData
        for child in children { pad(&out); out += child }
        _ = header
        let length = UInt16(out.count)
        out.replaceSubrange(0..<2, with: withUnsafeBytes(of: length.littleEndian, Array.init))
        out.replaceSubrange(2..<4, with: withUnsafeBytes(of: UInt16(valueLength).littleEndian, Array.init))
        out.replaceSubrange(4..<6, with: withUnsafeBytes(of: type.littleEndian, Array.init))
        return out
    }

    @Test func parsesStringsAndTranslation() throws {
        let strings = node("041104B0", children: [
            node("CompanyName", text: "YUZUSOFT"), node("ProductName", text: "千恋＊万花"), node("ProductVersion", text: "1.0"),
        ])
        let translation = Data([0x11, 0x04, 0xB0, 0x04])          // 0x0411, codepage 0x04B0
        let block = node("VS_VERSION_INFO", binary: Data(count: 52), children: [
            node("StringFileInfo", children: [strings]),
            node("VarFileInfo", children: [node("Translation", binary: translation)]),
        ])
        let info = try #require(PEVersionInfo.parse(block))
        #expect(info.company == "YUZUSOFT")
        #expect(info.productName == "千恋＊万花")
        #expect(info.translations.first == 0x0411)
        #expect(InstallerInfo.locale(forLanguage: 0x0411) == "ja")
        #expect(InstallerInfo.locale(forLanguage: 0x0409) == nil)
        #expect(InstallerInfo.preferredLocale([0x0411]) == "ja")
        #expect(InstallerInfo.preferredLocale([0x0404, 0x0409, 0x0411]) == nil)
    }

    @Test func realWindowsProgramsIfAvailable() throws {
        // Real signed Windows binaries on this machine (Steam in a bottle, the VC++ installer in the download
        // cache); skipped where they don't exist.
        let home = FileManager.default.homeDirectoryForCurrentUser
        let candidates = [
            "Library/Application Support/Cider/Bottles/steam-hb-2574/prefix/drive_c/Program Files (x86)/Steam/steam.exe",
        ].map { home.appendingPathComponent($0) }
            + ((FileManager.default.enumerator(at: URL(fileURLWithPath: "/tmp/claude-501/recipe-test/Caches/downloads/cas"),
                                                includingPropertiesForKeys: nil)?.compactMap { $0 as? URL }) ?? [])
                .filter { $0.pathExtension == "exe" }
        for exe in candidates where FileManager.default.fileExists(atPath: exe.path) {
            let info = try #require(PEVersionInfo.read(fileAt: exe), "\(exe.lastPathComponent)")
            #expect(info.company?.isEmpty == false, "\(exe.lastPathComponent): \(info.strings)")
            #expect(!info.translations.isEmpty)
        }
    }
}
