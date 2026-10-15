import Foundation
import Testing
@testable import CiderBottle
@testable import CiderIntegration
@testable import CiderPE
import CiderSchema

@Suite struct QuarantineShellLinkTests {
    struct ANSIExample: Sendable {
        let locale: BottleLocale
        let bytes: [UInt8]
        let text: String
    }
    static let ansiExamples: [ANSIExample] = [
        .init(locale: .japanese, bytes: [0x81, 0x40], text: "　"),
        .init(locale: .simplifiedChinese, bytes: [0x81, 0x40], text: "丂"),
        .init(locale: .traditionalChinese, bytes: [0xA4, 0xA4], text: "中"),
        .init(locale: .korean, bytes: [0xB0, 0xA1], text: "가"),
        .init(locale: .english, bytes: [0xE9], text: "é"),
    ]

    func le16(_ value: Int) -> Data { withUnsafeBytes(of: UInt16(value).littleEndian) { Data($0) } }
    func le32(_ value: Int) -> Data { withUnsafeBytes(of: UInt32(value).littleEndian) { Data($0) } }
    func wide(_ string: String) -> Data { string.utf16.reduce(into: Data()) { $0.append(le16(Int($1))) } }
    func header(_ flags: Int) -> Data {
        var data = le32(0x4C)
        data.append(contentsOf: [1, 0x14, 2, 0, 0, 0, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46])
        data.append(le32(flags)); data.append(Data(repeating: 0, count: 0x4C - data.count))
        return data
    }
    func info(base: Data = Data(#"C:\Tools\app.exe"#.utf8), suffix: Data = Data(),
              unicodeBase: String? = nil, unicodeSuffix: String? = nil, extended: Bool = false) -> Data {
        let headerSize = extended || unicodeBase != nil || unicodeSuffix != nil ? 36 : 28
        let volume = le32(17) + le32(3) + le32(0) + le32(16) + Data([0])
        let baseOffset = headerSize + volume.count, suffixOffset = baseOffset + base.count + 1
        var body = volume + base + Data([0]) + suffix + Data([0])
        var baseU = 0, suffixU = 0
        if let unicodeBase { baseU = headerSize + body.count; body.append(wide(unicodeBase) + le16(0)) }
        if let unicodeSuffix { suffixU = headerSize + body.count; body.append(wide(unicodeSuffix) + le16(0)) }
        var result = le32(headerSize + body.count) + le32(headerSize) + le32(1) + le32(headerSize)
            + le32(baseOffset) + le32(0) + le32(suffixOffset)
        if headerSize == 36 { result.append(le32(baseU) + le32(suffixU)) }
        return result + body
    }
    func link(flags: Int, fields: [Data] = [], linkInfo: Data? = nil, idList: Data? = nil) -> Data {
        var data = header(flags | (linkInfo == nil ? 0 : 2) | (idList == nil ? 0 : 1))
        if let idList { data.append(le16(idList.count) + idList) }
        if let linkInfo { data.append(linkInfo) }
        for field in fields { data.append(le16(flags & 0x80 == 0 ? field.count : field.count / 2) + field) }
        return data + le32(0)
    }
    func driveItem() -> Data { le16(7) + Data([0x2F]) + Data("C:\\".utf8) + Data([0]) }
    func fileItem(name: Data, extensionBytes: Data = Data(), extensionOffset: Int? = nil) -> Data {
        var data = Data(repeating: 0, count: 14); data[2] = 0x32
        data.append(name + Data([0]))
        if !extensionBytes.isEmpty {
            if data.count % 2 != 0 { data.append(0) }
            let offset = extensionOffset ?? data.count
            data.append(extensionBytes + le16(offset))
        }
        data.replaceSubrange(0..<2, with: le16(data.count))
        return data
    }
    func extensionBlock(_ name: String, version: Int = 3, terminated: Bool = true) -> Data {
        let nameOffset = version >= 7 ? 46 : 20
        let payload = wide(name) + (terminated ? le16(0) : Data())
        return le16(nameOffset + payload.count) + le16(version) + le32(0xBEEF_0004)
            + Data(repeating: 0, count: nameOffset - 8) + payload
    }

    @Test(arguments: ansiExamples) func explicitBottleACPDecodesAllCountedFields(_ example: ANSIExample) throws {
        let character = Data(example.bytes)
        let data = link(flags: 0x04 | 0x08 | 0x10 | 0x20 | 0x40,
                        fields: [character, Data(".\\".utf8) + character,
                                 Data("C:\\".utf8) + character, character, character + Data(".ico".utf8)], linkInfo: info())
        let value = try #require(ShellLink.parse(data, ansiCodePage: example.locale.acp))
        #expect(value.target == #"C:\Tools\app.exe"#)
        #expect(value.workingDirectory == "C:\\" + example.text)
        #expect(value.arguments == example.text)
        #expect(value.iconLocation == example.text + ".ico")
    }

    @Test func displayMetadataWithoutSupportedACPDoesNotEraseTarget() throws {
        let data = link(flags: 0x04 | 0x40, fields: [Data([0x81, 0x40]), Data([0x81, 0x40])], linkInfo: info())
        for acp in [nil, 99999, 1252] as [Int?] {
            let value = try #require(ShellLink.parse(data, ansiCodePage: acp))
            #expect(value.target == #"C:\Tools\app.exe"#)
            #expect(value.iconLocation == nil)
        }
        for flag in [0x08, 0x10, 0x20] {
            #expect(ShellLink.parse(link(flags: flag, fields: [Data([0x81, 0x40])], linkInfo: info())) == nil)
        }
        #expect(ShellLink.parse(link(flags: 0x20, fields: [Data([0x81])], linkInfo: info()), ansiCodePage: 932) == nil)
    }

    @Test func explicitACPDoesNotGuessUTF8OrAnotherAsianCodePage() throws {
        let data = link(flags: 0x20, fields: [Data([0xC3, 0xA9])], linkInfo: info())
        #expect(try #require(ShellLink.parse(data, ansiCodePage: 1252)).arguments == "Ã©")
        #expect(try #require(ShellLink.parse(data, ansiCodePage: 65001)).arguments == "é")
        #expect(ShellLink.parse(data) == nil)
    }

    @Test func singleTerminalNULConsumptionPreservesSubsequentFields() throws {
        for unicode in [false, true] {
            let fields = ["C:\\Tools\0", "-a\0", "app.ico\0"].map { unicode ? wide($0) : Data($0.utf8) }
            let value = try #require(ShellLink.parse(link(flags: 0x10 | 0x20 | 0x40 | (unicode ? 0x80 : 0), fields: fields, linkInfo: info()), ansiCodePage: 1252))
            #expect(value.workingDirectory == #"C:\Tools"#)
            #expect(value.arguments == "-a")
            #expect(value.iconLocation == "app.ico")
        }
    }

    @Test func malformedCountedFieldsFailIncludingOptionalMetadata() {
        for flag in [0x04, 0x08, 0x10, 0x20, 0x40] {
            for text in ["a\0b", "a\0\0"] {
                #expect(ShellLink.parse(link(flags: flag, fields: [Data(text.utf8)], linkInfo: info()), ansiCodePage: 936) == nil)
                #expect(ShellLink.parse(link(flags: flag | 0x80, fields: [wide(text)], linkInfo: info()), ansiCodePage: 936) == nil)
            }
            #expect(ShellLink.parse(link(flags: flag | 0x80, fields: [le16(0xD800)], linkInfo: info())) == nil)
            let truncated = header(flag | 2) + info() + le16(30) + Data([0x61])
            #expect(ShellLink.parse(truncated, ansiCodePage: 936) == nil)
        }
    }

    @Test func extendedUnicodeAndMixedSuffixRemainUsable() throws {
        let extended = link(flags: 0, linkInfo: info(base: Data([0x81]), unicodeBase: "C:\\测试", unicodeSuffix: "游戏.exe"))
        #expect(try #require(ShellLink.parse(extended)).target == "C:\\测试\\游戏.exe")
        let mixed = link(flags: 0, linkInfo: info(base: Data(#"C:\Tools"#.utf8), suffix: Data([0x81]), unicodeSuffix: "游戏.exe"))
        #expect(try #require(ShellLink.parse(mixed)).target == "C:\\Tools\\游戏.exe")
        #expect(try #require(ShellLink.parse(link(flags: 0, linkInfo: info(extended: true)))).target == #"C:\Tools\app.exe"#)
        #expect(ShellLink.parse(link(flags: 0, linkInfo: info(unicodeBase: "C:\\测试"))) == nil)
    }

    @Test func linkInfoStringsCannotBorrowOutsideDeclaredBlock() {
        var outside = header(2)
        for word in [28, 28, 1, 28, 32, 0, 28, 0] { outside.append(le32(word)) }
        outside.append(Data(#"C:\Outside\side.exe"#.utf8) + Data([0]))
        #expect(ShellLink.parse(outside) == nil)
        for field in [12, 16, 20, 24, 28, 32] {
            var block = info(unicodeBase: #"C:\Tools"#, unicodeSuffix: "app.exe")
            block.replaceSubrange(field..<(field + 4), with: le32(block.count))
            #expect(ShellLink.parse(link(flags: 0, linkInfo: block)) == nil)
        }
        var block = info(base: Data(#"C:\Tools\app.exe"#.utf8), unicodeBase: #"C:\Tools"#, unicodeSuffix: "app.exe")
        block.removeLast(2); block.replaceSubrange(0..<4, with: le32(block.count))
        #expect(ShellLink.parse(link(flags: 0, linkInfo: block)) == nil) // ExtraData zero is not the string's terminator.
    }

    @Test func localVolumeOffsetFollowsLinkInfoFlag() {
        var block = info(); block.replaceSubrange(12..<16, with: le32(0))
        #expect(ShellLink.parse(link(flags: 0, linkInfo: block)) == nil)
        block = info(); block.replaceSubrange(4..<8, with: le32(32))
        #expect(ShellLink.parse(link(flags: 0, linkInfo: block)) == nil)
    }

    @Test func emptyAndTerminalOnlyIDListsAndUnknownItemsRemainValid() throws {
        let empty = link(flags: 0, idList: le16(0))
        #expect(try #require(ShellLink.parse(empty)).target == nil)
        let unknown = le16(4) + Data([0x1F, 0x00]) + le16(0)
        #expect(try #require(ShellLink.parse(link(flags: 0, linkInfo: info(), idList: unknown))).target == #"C:\Tools\app.exe"#)
        #expect(ShellLink.parse(link(flags: 0, idList: Data())) == nil)
        #expect(ShellLink.parse(link(flags: 0, idList: le16(0) + le16(0))) == nil)
    }

    @Test func IDListItemsAndExtensionsHaveIndependentBounds() throws {
        for version in [3, 7] {
            let item = fileItem(name: Data("SHORT.EXE".utf8), extensionBytes: extensionBlock("游戏.exe", version: version))
            #expect(try #require(ShellLink.parse(link(flags: 0, idList: driveItem() + item + le16(0)))).target == "C:\\游戏.exe")
        }
        let unterminated = fileItem(name: Data("SHORT.EXE".utf8), extensionBytes: extensionBlock("游戏.exe", terminated: false))
        #expect(ShellLink.parse(link(flags: 0, idList: driveItem() + unterminated + le16(0))) == nil)
        let missingItemNUL = le16(19) + Data([0x32]) + Data(repeating: 1, count: 11) + Data("a.exe".utf8)
        #expect(ShellLink.parse(link(flags: 0, idList: driveItem() + missingItemNUL + le16(0))) == nil)
        #expect(ShellLink.parse(link(flags: 0, idList: driveItem())) == nil)
        #expect(ShellLink.parse(link(flags: 0, idList: driveItem() + le16(0xFFFF) + le16(0))) == nil)
    }

    @Test func incidentalSignatureWithoutExtensionPointerIsNotAnError() throws {
        // ItemID is namespace-defined opaque data; a coincidental signature in
        // additional metadata must not create a new invalid-extension refusal.
        var item = fileItem(name: Data("app.exe".utf8))
        item.append(le32(0xBEEF_0004) + Data(repeating: 0, count: 8))
        item.replaceSubrange(0..<2, with: le16(item.count))
        let value = try #require(ShellLink.parse(link(flags: 0, idList: driveItem() + item + le16(0))))
        #expect(value.target == #"C:\app.exe"#)
    }

    @Test(arguments: ansiExamples) func catalogPassesBottleLocaleACP(_ example: ANSIExample) throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("cider-quarantine-link-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: root) }
        let config = BottleConfig(id: "fixture", name: "Fixture", createdBy: "tests", createdAt: "2026-10-14T00:00:00Z",
                                  template: .win10_64, engine: .init(id: "unused"), locale: example.locale)
        let bottle = Bottle(config: config, directory: root.appendingPathComponent("fixture"))
        let desktop = bottle.driveC.appendingPathComponent("users/fixture/Desktop")
        try FileManager.default.createDirectory(at: desktop, withIntermediateDirectories: true)
        let tools = bottle.driveC.appendingPathComponent("Tools")
        try FileManager.default.createDirectory(at: tools, withIntermediateDirectories: true)
        try Data("not a PE".utf8).write(to: tools.appendingPathComponent("app.exe"))
        let data = link(flags: 0x04 | 0x20 | 0x40, fields: Array(repeating: Data(example.bytes), count: 3), linkInfo: info())
        try data.write(to: desktop.appendingPathComponent("Game.lnk"))
        let catalog = AppCatalog(iconCache: IconCache(directory: root.appendingPathComponent("icons")))
        let app = try #require(catalog.shortcutPrograms(in: bottle, excludingRoots: []).first)
        #expect(app.launchArguments == [example.text])
        #expect(app.launchProgram == #"C:\Tools\app.exe"#)
    }
}
