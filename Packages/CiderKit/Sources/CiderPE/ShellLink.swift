import Foundation

/// A parsed Windows shortcut (.lnk, [MS-SHLLINK]).
public struct ShellLink: Equatable, Sendable {
    /// Absolute Windows path of the target, e.g. `C:\Program Files\App\app.exe`.
    public var target: String?
    public var arguments: String?
    public var workingDirectory: String?
    /// Icon path as stored in the link (may contain environment variables such as %SystemRoot%).
    public var iconLocation: String?
    public var iconIndex: Int32 = 0

    public static func parse(fileAt url: URL) -> ShellLink? {
        guard let data = try? Data(contentsOf: url) else { return nil }
        return parse(data)
    }

    public static func parse(_ data: Data) -> ShellLink? {
        let r = ByteReader(data)
        guard r.u32(0) == 0x4C, let flags = r.u32(0x14) else { return nil }
        let hasIDList = flags & 0x01 != 0, hasLinkInfo = flags & 0x02 != 0, hasName = flags & 0x04 != 0
        let hasRelative = flags & 0x08 != 0, hasWorkingDir = flags & 0x10 != 0, hasArguments = flags & 0x20 != 0
        let hasIcon = flags & 0x40 != 0, unicode = flags & 0x80 != 0

        var link = ShellLink()
        link.iconIndex = Int32(bitPattern: r.u32(0x38) ?? 0)
        var offset = 0x4C
        var idListTarget: String?
        if hasIDList {
            guard let size = r.u16(offset) else { return nil }
            idListTarget = parseIDList(r, start: offset + 2, end: offset + 2 + Int(size))
            offset += 2 + Int(size)
        }
        if hasLinkInfo {
            guard let size = r.u32(offset) else { return nil }
            link.target = parseLinkInfo(r, at: offset)
            offset += Int(size)
        }
        func string() -> String? {
            guard let count = r.u16(offset).map(Int.init) else { return nil }
            let bytes = unicode ? count * 2 : count
            guard let slice = r.slice(offset + 2, bytes) else { return nil }
            offset += 2 + bytes
            return unicode ? String(data: slice, encoding: .utf16LittleEndian) : String(data: slice, encoding: .windowsCP1252)
        }
        if hasName { _ = string() }
        let relative = hasRelative ? string() : nil
        if hasWorkingDir { link.workingDirectory = string() }
        if hasArguments { link.arguments = string() }
        if hasIcon { link.iconLocation = string() }
        if link.target == nil { link.target = idListTarget }
        if link.target == nil, let relative, let dir = link.workingDirectory, relative.hasPrefix(".\\") {
            link.target = dir + "\\" + relative.dropFirst(2)
        }
        return link
    }

    /// LinkInfo: LocalBasePath (+ CommonPathSuffix), preferring the Unicode variants when present.
    static func parseLinkInfo(_ r: ByteReader, at start: Int) -> String? {
        guard let headerSize = r.u32(start + 4), let infoFlags = r.u32(start + 8), infoFlags & 0x1 != 0,
              let baseOffset = r.u32(start + 16), let suffixOffset = r.u32(start + 24) else { return nil }
        if headerSize >= 0x24, let baseU = r.u32(start + 28), let suffixU = r.u32(start + 32), baseU > 0 {
            let base = utf16z(r, start + Int(baseU)) ?? ""
            let suffix = suffixU > 0 ? (utf16z(r, start + Int(suffixU)) ?? "") : ""
            if !base.isEmpty { return join(base, suffix) }
        }
        let base = ansiz(r, start + Int(baseOffset)) ?? ""
        let suffix = ansiz(r, start + Int(suffixOffset)) ?? ""
        return base.isEmpty ? nil : join(base, suffix)
    }

    /// Reconstructs a filesystem path from shell item IDs: a drive item ("C:\") followed by folder/file items,
    /// taking each item's long Unicode name from its 0xBEEF0004 extension block when available.
    static func parseIDList(_ r: ByteReader, start: Int, end: Int) -> String? {
        var offset = start
        var parts: [String] = []
        while offset + 2 <= end, let size = r.u16(offset).map(Int.init), size >= 2 {
            let item = offset
            offset += size
            guard let type = r.slice(item + 2, 1)?.first else { continue }
            switch type & 0x70 {
            case 0x20:  // drive
                if let drive = ansiz(r, item + 3), drive.count >= 2 { parts = [String(drive.prefix(2))] }
            case 0x30:  // file or folder
                guard !parts.isEmpty else { continue }
                var name = ansiz(r, item + 14) ?? ""
                if let long = longName(r, item: item, size: size) { name = long }
                if !name.isEmpty { parts.append(name) }
            default:
                continue
            }
        }
        guard parts.count >= 2 else { return nil }
        return parts.joined(separator: "\\")
    }

    static func longName(_ r: ByteReader, item: Int, size: Int) -> String? {
        var i = item + 14
        while i + 8 <= item + size {
            if r.u32(i) == 0xBEEF_0004, let blockSize = r.u16(i - 4).map(Int.init), let version = r.u16(i - 2) {
                let base = i - 4
                let nameOffset = base + (version >= 7 ? 46 : (version >= 3 ? 20 : 18))
                if let name = utf16z(r, nameOffset), !name.isEmpty, nameOffset < base + blockSize { return name }
                return nil
            }
            i += 2
        }
        return nil
    }

    static func join(_ base: String, _ suffix: String) -> String {
        if suffix.isEmpty { return base }
        return base.hasSuffix("\\") ? base + suffix : base + "\\" + suffix
    }

    static func ansiz(_ r: ByteReader, _ offset: Int) -> String? {
        var end = offset
        while let byte = r.slice(end, 1)?.first, byte != 0 { end += 1 }
        guard end > offset, let slice = r.slice(offset, end - offset) else { return nil }
        // Shortcuts made on Chinese/Japanese Windows store ANSI paths in the system code page.
        return String(data: slice, encoding: .utf8)
            ?? String(data: slice, encoding: String.Encoding(rawValue: CFStringConvertEncodingToNSStringEncoding(CFStringEncoding(CFStringEncodings.GB_18030_2000.rawValue))))
            ?? String(data: slice, encoding: .windowsCP1252)
    }

    static func utf16z(_ r: ByteReader, _ offset: Int) -> String? {
        var end = offset
        while let unit = r.u16(end), unit != 0 { end += 2 }
        guard end > offset, let slice = r.slice(offset, end - offset) else { return nil }
        return String(data: slice, encoding: .utf16LittleEndian)
    }
}
