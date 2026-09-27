import Foundation

/// Extracts the application icon from a Windows PE file (.exe/.dll) as an .ico byte stream.
///
/// Walks the resource directory (types RT_GROUP_ICON = 14 and RT_ICON = 3), takes the first icon group — the one
/// Explorer shows for the file — and reassembles its images into an ICO container that ImageIO can decode.
public enum PEIcon {
    public static func icoData(fromFileAt url: URL) -> Data? {
        guard let data = try? Data(contentsOf: url, options: .mappedIfSafe) else { return nil }
        return icoData(fromPE: data)
    }

    public static func icoData(fromPE data: Data) -> Data? {
        guard let pe = PEImage(data: data), let resources = pe.resourceRoot else { return nil }
        guard let groups = pe.resourceEntries(type: 14, in: resources).first,
              let groupData = pe.firstLeafData(of: groups) else { return nil }
        let icons = pe.resourceEntries(type: 3, in: resources)
        var iconByID: [UInt32: Int] = [:]
        for (index, entry) in icons.enumerated() { if let id = entry.id { iconByID[id] = index } }

        let g = ByteReader(groupData)
        guard let count = g.u16(4), count > 0, count < 256 else { return nil }
        var entries: [(header: Data, image: Data)] = []
        for i in 0..<Int(count) {
            let base = 6 + i * 14
            guard let header = g.slice(base, 12), let id = g.u16(base + 12),
                  let iconIndex = iconByID[UInt32(id)], let image = pe.firstLeafData(of: icons[iconIndex]) else { continue }
            entries.append((header, image))
        }
        guard !entries.isEmpty else { return nil }

        // ICONDIR + ICONDIRENTRY[] (16 bytes: the 12-byte header, then a 4-byte file offset) + images.
        var ico = Data()
        ico.append(le16(0)); ico.append(le16(1)); ico.append(le16(UInt16(entries.count)))
        var offset = 6 + 16 * entries.count
        for e in entries {
            var header = e.header
            header.replaceSubrange(8..<12, with: le32(UInt32(e.image.count)))  // dwBytesInRes from the real size
            ico.append(header)
            ico.append(le32(UInt32(offset)))
            offset += e.image.count
        }
        for e in entries { ico.append(e.image) }
        return ico
    }

    static func le16(_ v: UInt16) -> Data { withUnsafeBytes(of: v.littleEndian) { Data($0) } }
    static func le32(_ v: UInt32) -> Data { withUnsafeBytes(of: v.littleEndian) { Data($0) } }
}

/// Minimal little-endian reader over a byte buffer with bounds checks.
struct ByteReader {
    let data: Data
    init(_ data: Data) { self.data = data }

    func u16(_ offset: Int) -> UInt16? {
        guard offset >= 0, offset + 2 <= data.count else { return nil }
        let i = data.startIndex + offset
        return UInt16(data[i]) | UInt16(data[i + 1]) << 8
    }

    func u32(_ offset: Int) -> UInt32? {
        guard let lo = u16(offset), let hi = u16(offset + 2) else { return nil }
        return UInt32(lo) | UInt32(hi) << 16
    }

    func slice(_ offset: Int, _ count: Int) -> Data? {
        guard offset >= 0, count >= 0, offset + count <= data.count else { return nil }
        return data.subdata(in: (data.startIndex + offset)..<(data.startIndex + offset + count))
    }
}

/// Just enough PE parsing to reach the resource section.
struct PEImage {
    let bytes: ByteReader
    let sections: [(virtualAddress: UInt32, virtualSize: UInt32, rawOffset: UInt32, rawSize: UInt32)]
    let resourceRVA: UInt32

    init?(data: Data) {
        bytes = ByteReader(data)
        guard bytes.u16(0) == 0x5A4D, let peOffset = bytes.u32(0x3C).map(Int.init),
              bytes.u32(peOffset) == 0x0000_4550,
              let sectionCount = bytes.u16(peOffset + 6), let optionalSize = bytes.u16(peOffset + 20) else { return nil }
        let optional = peOffset + 24
        guard let magic = bytes.u16(optional) else { return nil }
        let dataDirectories = optional + (magic == 0x20B ? 112 : 96)
        resourceRVA = bytes.u32(dataDirectories + 2 * 8) ?? 0
        var sections: [(UInt32, UInt32, UInt32, UInt32)] = []
        let table = optional + Int(optionalSize)
        for i in 0..<Int(sectionCount) {
            let s = table + i * 40
            guard let vsize = bytes.u32(s + 8), let va = bytes.u32(s + 12),
                  let rawSize = bytes.u32(s + 16), let raw = bytes.u32(s + 20) else { return nil }
            sections.append((va, vsize, raw, rawSize))
        }
        self.sections = sections.map { (virtualAddress: $0.0, virtualSize: $0.1, rawOffset: $0.2, rawSize: $0.3) }
    }

    func fileOffset(rva: UInt32) -> Int? {
        for s in sections {
            let size = max(s.virtualSize, s.rawSize)
            if rva >= s.virtualAddress, rva < s.virtualAddress &+ size {
                return Int(s.rawOffset) + Int(rva - s.virtualAddress)
            }
        }
        return nil
    }

    var resourceRoot: Int? { resourceRVA == 0 ? nil : fileOffset(rva: resourceRVA) }

    struct Entry {
        let id: UInt32?
        /// Offset (relative to the resource section start) of a subdirectory, or nil for a data leaf.
        let directoryOffset: Int?
        let dataEntryOffset: Int?
    }

    func entries(directoryAt offset: Int, root: Int) -> [Entry] {
        guard let named = bytes.u16(offset + 12), let ids = bytes.u16(offset + 14) else { return [] }
        let total = Int(named) + Int(ids)
        guard total < 4096 else { return [] }
        return (0..<total).compactMap { i in
            let e = offset + 16 + i * 8
            guard let name = bytes.u32(e), let target = bytes.u32(e + 4) else { return nil }
            let id: UInt32? = name & 0x8000_0000 == 0 ? name : nil
            if target & 0x8000_0000 != 0 {
                return Entry(id: id, directoryOffset: root + Int(target & 0x7FFF_FFFF), dataEntryOffset: nil)
            }
            return Entry(id: id, directoryOffset: nil, dataEntryOffset: root + Int(target))
        }
    }

    /// Second-level entries (names/ids) under a resource type.
    func resourceEntries(type: UInt32, in root: Int) -> [Entry] {
        guard let typeEntry = entries(directoryAt: root, root: root).first(where: { $0.id == type }),
              let dir = typeEntry.directoryOffset else { return [] }
        return entries(directoryAt: dir, root: root)
    }

    /// Follows subdirectories (typically the language level) to the first data leaf and returns its bytes.
    func firstLeafData(of entry: Entry, depth: Int = 0) -> Data? {
        guard depth < 4 else { return nil }
        if let leaf = entry.dataEntryOffset {
            guard let rva = bytes.u32(leaf), let size = bytes.u32(leaf + 4), size < 16 << 20,
                  let offset = fileOffset(rva: rva) else { return nil }
            return bytes.slice(offset, Int(size))
        }
        guard let dir = entry.directoryOffset, let root = resourceRoot,
              let first = entries(directoryAt: dir, root: root).first else { return nil }
        return firstLeafData(of: first, depth: depth + 1)
    }
}
