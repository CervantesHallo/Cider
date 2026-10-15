import CoreFoundation
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

    /// ANSI bytes have no code-page tag in a .lnk. Callers must supply their origin
    /// (the bottle's locale.acp); without it only unambiguous ASCII is decoded.
    public static func parse(fileAt url: URL, ansiCodePage: Int? = nil) -> ShellLink? {
        guard let data = try? Data(contentsOf: url) else { return nil }
        return parse(data, ansiCodePage: ansiCodePage)
    }

    public static func parse(_ data: Data, ansiCodePage: Int? = nil) -> ShellLink? {
        try? parseChecked(data, ansiEncoding: ansiEncoding(codePage: ansiCodePage))
    }

    private enum ParseFailure: Error { case invalid }

    private static func ansiEncoding(codePage: Int?) -> String.Encoding? {
        guard let codePage, let value = UInt32(exactly: codePage), value > 0 else { return nil }
        let encoding = CFStringConvertWindowsCodepageToEncoding(value)
        guard encoding != kCFStringEncodingInvalidId else { return nil }
        return String.Encoding(rawValue: CFStringConvertEncodingToNSStringEncoding(encoding))
    }

    private static func decodeANSI(_ data: Data, encoding: String.Encoding?) -> String? {
        if let encoding { return String(data: data, encoding: encoding) }
        guard data.allSatisfy({ $0 < 0x80 }) else { return nil }
        return String(data: data, encoding: .utf8)
    }

    private static func parseChecked(_ data: Data, ansiEncoding: String.Encoding?) throws -> ShellLink {
        let r = ByteReader(data)
        guard data.count >= 0x4C, r.u32(0) == 0x4C, let flags = r.u32(0x14) else { throw ParseFailure.invalid }
        let hasIDList = flags & 0x01 != 0, hasLinkInfo = flags & 0x02 != 0, hasName = flags & 0x04 != 0
        let hasRelative = flags & 0x08 != 0, hasWorkingDir = flags & 0x10 != 0, hasArguments = flags & 0x20 != 0
        let hasIcon = flags & 0x40 != 0, unicode = flags & 0x80 != 0

        var link = ShellLink()
        link.iconIndex = Int32(bitPattern: r.u32(0x38) ?? 0)
        var offset = 0x4C
        var idListTarget: String?
        if hasIDList {
            guard let size = r.u16(offset).map(Int.init), let block = r.slice(offset + 2, size) else { throw ParseFailure.invalid }
            idListTarget = try parseIDList(block, ansiEncoding: ansiEncoding)
            offset += 2 + size
        }
        if hasLinkInfo {
            guard let size = r.u32(offset).map(Int.init), size >= 0x1C,
                  let block = r.slice(offset, size) else { throw ParseFailure.invalid }
            link.target = try parseLinkInfo(block, ansiEncoding: ansiEncoding)
            offset += size
        }
        func string(launchField: Bool) throws -> String? {
            guard let count = r.u16(offset).map(Int.init) else { throw ParseFailure.invalid }
            let bytes = unicode ? count * 2 : count
            guard var slice = r.slice(offset + 2, bytes) else { throw ParseFailure.invalid }
            offset += 2 + bytes
            if unicode {
                guard var value = String(data: slice, encoding: .utf16LittleEndian) else { throw ParseFailure.invalid }
                if value.unicodeScalars.last?.value == 0 { value.unicodeScalars.removeLast() }
                guard !value.contains("\0") else { throw ParseFailure.invalid }
                return value
            }
            // Counted fields are not NUL-terminated per MS-SHLLINK. Tolerate one
            // counted terminal NUL from installed links, never embedded/double NUL.
            if slice.last == 0 { slice.removeLast() }
            guard !slice.contains(0) else { throw ParseFailure.invalid }
            let value = decodeANSI(slice, encoding: ansiEncoding)
            // Name/icon are display metadata; an unsupported code page must not
            // discard a valid target. Launch fields must never silently disappear.
            guard !launchField || value != nil else { throw ParseFailure.invalid }
            return value
        }
        if hasName { _ = try string(launchField: false) }
        let relative = hasRelative ? try string(launchField: true) : nil
        if hasWorkingDir { link.workingDirectory = try string(launchField: true) }
        if hasArguments { link.arguments = try string(launchField: true) }
        if hasIcon { link.iconLocation = try string(launchField: false) }
        if link.target == nil { link.target = idListTarget }
        if link.target == nil, let relative, let dir = link.workingDirectory, relative.hasPrefix(".\\") {
            link.target = dir + "\\" + relative.dropFirst(2)
        }
        return link
    }

    /// Each reader owns only its declared block; even its terminator cannot be
    /// borrowed from a following StringData/ExtraData field (MS-SHLLINK 2.3).
    private static func parseLinkInfo(_ data: Data, ansiEncoding: String.Encoding?) throws -> String? {
        let r = ByteReader(data)
        guard let headerSize = r.u32(4).map(Int.init), headerSize == 0x1C || headerSize >= 0x24,
              headerSize <= data.count, let flags = r.u32(8) else { throw ParseFailure.invalid }
        func fieldOffset(_ field: Int, required: Bool) throws -> Int {
            guard let value = r.u32(field).map(Int.init), value < data.count,
                  value == 0 ? !required : value >= headerSize else { throw ParseFailure.invalid }
            return value
        }
        let local = flags & 1 != 0
        _ = try fieldOffset(12, required: local) // VolumeID, not interpreted here.
        let baseOffset = try fieldOffset(16, required: local)
        _ = try fieldOffset(20, required: flags & 2 != 0) // Network target resolution remains unsupported.
        let suffixOffset = try fieldOffset(24, required: true)
        let suffixBytes = try ansiz(r, suffixOffset)
        let baseBytes = local ? try ansiz(r, baseOffset) : nil
        var unicodeSuffix: String?
        if headerSize >= 0x24 {
            let baseU = try fieldOffset(28, required: false), suffixU = try fieldOffset(32, required: false)
            unicodeSuffix = suffixU > 0 ? try utf16z(r, suffixU) : nil
            if baseU > 0 {
                guard local, let suffix = unicodeSuffix else { throw ParseFailure.invalid }
                let base = try utf16z(r, baseU)
                guard !base.isEmpty else { throw ParseFailure.invalid }
                return join(base, suffix)
            }
        }
        guard let baseBytes else { return nil }
        guard let base = decodeANSI(baseBytes, encoding: ansiEncoding), !base.isEmpty,
              let suffix = unicodeSuffix ?? decodeANSI(suffixBytes, encoding: ansiEncoding) else { throw ParseFailure.invalid }
        return join(base, suffix)
    }

    /// ItemID data is shell-specific. Reconstruct only the supported filesystem
    /// items, while bounding every read to its ItemID and extension block.
    private static func parseIDList(_ data: Data, ansiEncoding: String.Encoding?) throws -> String? {
        let r = ByteReader(data)
        var offset = 0
        var parts: [String] = []
        while let size = r.u16(offset).map(Int.init) {
            if size == 0 {
                guard offset + 2 == data.count else { throw ParseFailure.invalid }
                return parts.count >= 2 ? parts.joined(separator: "\\") : nil
            }
            guard size >= 2, let block = r.slice(offset, size) else { throw ParseFailure.invalid }
            offset += size
            let item = ByteReader(block)
            guard let type = item.slice(2, 1)?.first else { continue }
            switch type & 0x70 {
            case 0x20:
                let bytes = try ansiz(item, 3)
                guard let drive = decodeANSI(bytes, encoding: ansiEncoding) else { throw ParseFailure.invalid }
                if bytes.count >= 2, bytes[0].isASCIILetter, bytes[1] == 0x3A { parts = [String(drive.prefix(2))] }
                else { parts = [] }
            case 0x30:
                let extensionStart = extensionStart(item)
                let primary = extensionStart.flatMap { item.slice(0, $0) }.map(ByteReader.init) ?? item
                let bytes = try ansiz(primary, 14)
                let long = try longName(item, at: extensionStart)
                guard let name = long ?? decodeANSI(bytes, encoding: ansiEncoding) else { throw ParseFailure.invalid }
                if !parts.isEmpty, !name.isEmpty { parts.append(name) }
            default: continue
            }
        }
        throw ParseFailure.invalid // Missing TerminalID, including a truncated final ItemID.
    }

    /// Filesystem ItemIDs can locate their extension chain with a trailing WORD
    /// offset (Wine shell32/pidl.c _ILGetFileStructW). Do not hunt for a signature
    /// in arbitrary name/header bytes: MS-SHLLINK defines ItemID data as opaque.
    private static func extensionStart(_ item: ByteReader) -> Int? {
        guard let start = item.u16(item.data.count - 2).map(Int.init), start >= 16, start % 2 == 0,
              start + 8 <= item.data.count - 2, let size = item.u16(start).map(Int.init) else { return nil }
        guard item.u32(start + 4) == 0xBEEF_0004 || (size >= 8 && size <= item.data.count - start) else { return nil }
        return start
    }

    private static func longName(_ item: ByteReader, at start: Int?) throws -> String? {
        guard var offset = start else { return nil }
        while offset + 8 <= item.data.count - 2 {
            guard let size = item.u16(offset).map(Int.init), let version = item.u16(offset + 2) else { return nil }
            let known = item.u32(offset + 4) == 0xBEEF_0004
            guard size >= 8, size <= item.data.count - offset else {
                if known { throw ParseFailure.invalid }
                return nil // Unrecognized shell data is not a structural error.
            }
            if known {
                // The last WORD belongs to the ItemID's extension offset, never
                // to the name. Some producers include it in the final block size.
                let length = min(size, item.data.count - 2 - offset)
                guard let bytes = item.slice(offset, length) else { throw ParseFailure.invalid }
                let nameOffset = version >= 7 ? 46 : (version >= 3 ? 20 : 18)
                let name = try utf16z(ByteReader(bytes), nameOffset)
                return name.isEmpty ? nil : name
            }
            offset += size
        }
        return nil
    }

    static func join(_ base: String, _ suffix: String) -> String {
        if suffix.isEmpty { return base }
        return base.hasSuffix("\\") ? base + suffix : base + "\\" + suffix
    }

    private static func ansiz(_ r: ByteReader, _ offset: Int) throws -> Data {
        guard offset >= 0, offset < r.data.count else { throw ParseFailure.invalid }
        var end = offset
        while end < r.data.count {
            if r.data[r.data.startIndex + end] == 0 {
                guard let bytes = r.slice(offset, end - offset) else { throw ParseFailure.invalid }
                return bytes
            }
            end += 1
        }
        throw ParseFailure.invalid
    }

    private static func utf16z(_ r: ByteReader, _ offset: Int) throws -> String {
        guard offset >= 0, offset < r.data.count else { throw ParseFailure.invalid }
        var end = offset
        while let unit = r.u16(end) {
            if unit == 0 {
                guard let bytes = r.slice(offset, end - offset),
                      let value = String(data: bytes, encoding: .utf16LittleEndian) else { throw ParseFailure.invalid }
                return value
            }
            end += 2
        }
        throw ParseFailure.invalid
    }
}

private extension UInt8 {
    var isASCIILetter: Bool { (65...90).contains(self) || (97...122).contains(self) }
}
