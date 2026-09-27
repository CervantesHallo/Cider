import Foundation
import Testing
@testable import CiderBottle
@testable import CiderCore
@testable import CiderRuntime
@testable import CiderSchema
@testable import CiderStore

@Suite struct IdentifierTests {
    @Test func slugKeepsAsciiAndAddsSuffix() {
        let id = Identifiers.make(from: "Steam")
        #expect(id.hasPrefix("steam-"))
        #expect(id.count == "steam-".count + 4)
    }

    @Test func nonAsciiNameFallsBack() {
        #expect(Identifiers.make(from: "日文 galgame").hasPrefix("galgame-"))
        #expect(Identifiers.make(from: "千恋万花").hasPrefix("bottle-"))
    }

    @Test func separatorsCollapse() {
        #expect(Identifiers.make(from: "  Riddle -- Joker!! ").hasPrefix("riddle-joker-"))
    }
}

@Suite struct LocaleTests {
    @Test(arguments: [("ja", 932), ("zh-Hans", 936), ("zh-hant", 950), ("ko", 949), ("en", 1252), ("936", 936)])
    func namedLocales(name: String, acp: Int) throws {
        let locale = try #require(BottleLocale.named(name))
        #expect(locale.acp == acp)
        #expect(locale.lcAll.hasSuffix(".UTF-8"))
    }

    @Test func unknownLocale() {
        #expect(BottleLocale.named("klingon") == nil)
    }

    @Test func environmentSetsFullLocale() {
        #expect(BottleLocale.japanese.environment == ["LANG": "ja_JP.UTF-8", "LC_ALL": "ja_JP.UTF-8"])
    }
}

@Suite struct SchemaTests {
    @Test func bottleConfigRoundTripsWithDocumentedKeys() throws {
        let config = BottleConfig(id: "steam-1a2b", name: "Steam", createdBy: "test", createdAt: "2026-09-27T00:00:00Z",
                                  template: .win10_64, engine: .init(id: "wine-devel-11.18-x86_64"), locale: .simplifiedChinese)
        let data = try JSONEncoder().encode(config)
        let json = try #require(try JSONSerialization.jsonObject(with: data) as? [String: Any])
        #expect(json["cpu_backend"] as? String == "rosetta-x86_64")
        #expect((json["locale"] as? [String: Any])?["LC_ALL"] as? String == "zh_CN.UTF-8")
        #expect(try JSONDecoder().decode(BottleConfig.self, from: data) == config)
    }
}

@Suite struct RegistryFileTests {
    @Test func regFileIsUTF16LEWithBOMAndEscapes() throws {
        let data = PrefixSetup.regFile([(key: #"HKEY_CURRENT_USER\Software\Test"#, values: [("ＭＳ ゴシック", .string(#"a\b"c"#))])])
        #expect(data.prefix(2) == Data([0xFF, 0xFE]))
        let text = try #require(String(data: data.dropFirst(2), encoding: .utf16LittleEndian))
        #expect(text.hasPrefix("Windows Registry Editor Version 5.00\r\n"))
        #expect(text.contains(#""ＭＳ ゴシック"="a\\b\"c""#))
    }

    @Test func multiStringIsHex7UTF16() throws {
        let data = PrefixSetup.regFile([(key: "K", values: [("Tahoma", .multiString(["a,b", "c"]))])])
        let text = try #require(String(data: data.dropFirst(2), encoding: .utf16LittleEndian))
        // "a,b\0c\0\0" in UTF-16LE
        #expect(text.contains(#""Tahoma"=hex(7):61,00,2c,00,62,00,00,00,63,00,00,00,00,00"#))
    }

    @Test func dwordValues() throws {
        let data = PrefixSetup.regFile(PrefixSetup.highResolution(true))
        let text = try #require(String(data: data.dropFirst(2), encoding: .utf16LittleEndian))
        #expect(text.contains(#""LogPixels"=dword:000000c0"#))
        #expect(text.contains(#""RetinaMode"="y""#))
    }

    @Test func uiFontsFollowTheLocale() {
        let zh = PrefixSetup.uiFonts(for: .simplifiedChinese)
        #expect(zh.count == 2)
        guard case .multiString(let links) = zh[0].values.first(where: { $0.0 == "Tahoma" })?.1 else { Issue.record("no Tahoma link"); return }
        #expect(links.first == "Hiragino Sans GB.ttc,Hiragino Sans GB")
        #expect(zh[1].values.first?.1 == .string("Microsoft YaHei UI"))
        #expect(PrefixSetup.uiFonts(for: .english).count == 1)
    }

    @Test func fontTableCoversJapaneseLocalizedNames() {
        let names = PrefixSetup.fontReplacements.flatMap { $0.values.map(\.0) }
        #expect(names.contains("ＭＳ ゴシック"))
        #expect(names.contains("微软雅黑"))
        #expect(Set(names).count == names.count, "duplicate replacement keys")
    }
}

@Suite struct EngineStoreTests {
    @Test func findsWineBinaryInAppBundleLayout() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("cider-test-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: root) }
        let bin = root.appendingPathComponent("Wine Devel.app/Contents/Resources/wine/bin")
        try FileManager.default.createDirectory(at: bin, withIntermediateDirectories: true)
        let wine = bin.appendingPathComponent("wine")
        try Data("#!/bin/sh\n".utf8).write(to: wine)
        try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: wine.path)
        #expect(EngineStore.findWineBinary(in: root)?.standardizedFileURL == wine.standardizedFileURL)
    }

    @Test func sha256MatchesKnownValue() throws {
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("cider-sha-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: file) }
        try Data("abc".utf8).write(to: file)
        #expect(try EngineStore.sha256(of: file) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    }
}

@Suite struct SyncModeTests {
    @Test func defaultsToMsyncAndIsBottleWide() {
        #expect(SyncMode(setting: nil) == .msync)
        #expect(SyncMode(setting: "bogus") == .msync)
        #expect(SyncMode(setting: "server").environment == ["WINEMSYNC": "0", "WINEESYNC": "0"])
        #expect(SyncMode.msync.environment["WINEMSYNC"] == "1")
    }
}
