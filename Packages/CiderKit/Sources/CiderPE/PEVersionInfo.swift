import Foundation

/// `VS_VERSIONINFO` (RT_VERSION = 16) of a PE file: string fields and the language/codepage pairs.
public struct PEVersionInfo: Sendable, Equatable {
    public var strings: [String: String]          // CompanyName, ProductName, FileDescription, ProductVersion, …
    public var translations: [UInt16]             // language ids (e.g. 0x0411 Japanese)

    public var productName: String? { strings["ProductName"].flatMap { $0.isEmpty ? nil : $0 } ?? strings["FileDescription"] }
    public var company: String? { strings["CompanyName"] }
    public var version: String? { strings["ProductVersion"] ?? strings["FileVersion"] }

    public static func read(fileAt url: URL) -> PEVersionInfo? {
        guard let data = try? Data(contentsOf: url, options: .mappedIfSafe), let pe = PEImage(data: data),
              let root = pe.resourceRoot, let entry = pe.resourceEntries(type: 16, in: root).first,
              let block = pe.firstLeafData(of: entry) else { return nil }
        var info = parse(block)
        // No Translation var: fall back to the resource's language level (third directory level).
        if info?.translations.isEmpty == true, let dir = entry.directoryOffset,
           let lang = pe.entries(directoryAt: dir, root: root).first?.id {
            info?.translations = [UInt16(truncatingIfNeeded: lang)]
        }
        return info
    }

    /// Parses a VS_VERSIONINFO block. Every node is: wLength, wValueLength, wType, UTF-16 key, padding to 4,
    /// value, padding to 4, children.
    static func parse(_ data: Data) -> PEVersionInfo? {
        let r = ByteReader(data)
        var strings: [String: String] = [:]
        var translations: [UInt16] = []

        func align(_ o: Int) -> Int { (o + 3) & ~3 }

        func key(at offset: Int) -> (String, Int)? {           // returns key and offset after its terminator
            var o = offset, units: [UInt16] = []
            while let u = r.u16(o) { o += 2; if u == 0 { break }; units.append(u); if units.count > 256 { return nil } }
            return (String(decoding: units, as: UTF16.self), o)
        }

        func node(at offset: Int, depth: Int, parent: String) {
            guard depth < 6, let length = r.u16(offset).map(Int.init), length >= 6, offset + length <= data.count,
                  let valueLength = r.u16(offset + 2).map(Int.init), let type = r.u16(offset + 4),
                  let (name, afterKey) = key(at: offset + 6) else { return }
            let valueStart = align(afterKey)
            // wValueLength counts WCHARs for text (type 1), bytes for binary (type 0).
            let valueBytes = type == 1 ? valueLength * 2 : valueLength
            if parent == "StringTable-child", type == 1 || valueLength > 0 {
                var units: [UInt16] = []
                var o = valueStart
                while o + 1 < valueStart + valueBytes, let u = r.u16(o), u != 0 { units.append(u); o += 2 }
                strings[name] = String(decoding: units, as: UTF16.self).trimmingCharacters(in: .whitespaces)
            }
            if name == "Translation" {
                var o = valueStart
                while o + 3 < valueStart + valueBytes, let lang = r.u16(o) { translations.append(lang); o += 4 }
            }
            var child = align(valueStart + valueBytes)
            let end = offset + length
            let childParent: String
            switch name {
            case "VS_VERSION_INFO": childParent = "root"
            case "StringFileInfo": childParent = "StringFileInfo"
            case "VarFileInfo": childParent = "VarFileInfo"
            default: childParent = parent == "StringFileInfo" ? "StringTable-child" : "other"
            }
            while child + 6 < end, let l = r.u16(child).map(Int.init), l > 0 {
                node(at: child, depth: depth + 1, parent: childParent)
                child = align(child + l)
            }
            // A StringTable's key is "LLLLCCCC" (language + codepage) — a translation hint when Translation is absent.
            if parent == "StringFileInfo", name.count == 8, let lang = UInt16(name.prefix(4), radix: 16), !translations.contains(lang) {
                translations.append(lang)
            }
        }

        node(at: 0, depth: 0, parent: "")
        return strings.isEmpty && translations.isEmpty ? nil : PEVersionInfo(strings: strings, translations: translations)
    }
}

/// What kind of installer a Windows setup program is, and which bottle language it wants (docs/plan/05 §15
/// row 5, "Install an unlisted application").
public struct InstallerInfo: Sendable, Equatable {
    public enum Kind: String, Sendable { case inno = "Inno Setup", nsis = "NSIS", installShield = "InstallShield",
                                          msi = "Windows Installer", burn = "WiX Burn", sevenZip = "7-Zip SFX", unknown = "" }
    public var kind: Kind
    public var product: String?
    public var company: String?
    public var version: String?
    /// BottleLocale `ui` id the installer's language maps to (ja, zh-Hans, zh-Hant, ko, en), if known.
    public var locale: String?

    public static func inspect(fileAt url: URL) -> InstallerInfo {
        if url.pathExtension.lowercased() == "msi" { return InstallerInfo(kind: .msi, locale: nil) }
        let version = PEVersionInfo.read(fileAt: url)
        var kind = Kind.unknown
        // Installer stubs are small; data is appended or beside them, so the head is enough.
        if let handle = try? FileHandle(forReadingFrom: url), let head = try? handle.read(upToCount: 6 << 20) {
            try? handle.close()
            let text = String(decoding: head, as: UTF8.self)
            let signatures: [(Kind, [String])] = [
                (.inno, ["Inno Setup"]), (.nsis, ["Nullsoft", "NSIS Error"]), (.installShield, ["InstallShield"]),
                (.burn, [".wixburn", "WixBundle"]), (.sevenZip, ["7-Zip", "7zS.sfx", ";!@Install@!UTF-8!"]),
            ]
            kind = signatures.first { $0.1.contains { text.contains($0) } }?.0 ?? .unknown
        }
        if kind == .unknown, let comments = version?.strings.values.joined(separator: " ") {
            if comments.contains("Inno Setup") { kind = .inno } else if comments.contains("Nullsoft") { kind = .nsis }
        }
        return InstallerInfo(kind: kind, product: version?.productName, company: version?.company,
                             version: version?.version, locale: preferredLocale(version?.translations ?? []))
    }

    /// A locale preference only for single-language installers: multilingual ones (Steam's lists a dozen)
    /// or English/neutral ones run fine in any bottle.
    static func preferredLocale(_ translations: [UInt16]) -> String? {
        let languages = Set(translations.filter { $0 != 0 })
        guard languages.count == 1, let lang = languages.first else { return nil }
        return locale(forLanguage: lang)
    }

    /// Windows LANGID → BottleLocale id; neutral/English ids return nil (no preference).
    static func locale(forLanguage lang: UInt16) -> String? {
        switch lang {
        case 0x0411: return "ja"
        case 0x0804, 0x1004: return "zh-Hans"
        case 0x0404, 0x0C04, 0x1404: return "zh-Hant"
        case 0x0412: return "ko"
        default: return nil
        }
    }
}
