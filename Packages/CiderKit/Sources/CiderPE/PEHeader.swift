import Foundation

/// Bounded structural PE header inspection, not a loader or a trusted-source decision.
public struct PEHeader: Sendable {
    public let machine: UInt16
    public let characteristics: UInt16
    public let sectionCount: UInt16
    public let optionalHeaderOffset: Int
    public let optionalHeaderSize: Int
    public let isPE32Plus: Bool
    public let subsystem: UInt16
    public let dataDirectoryOffset: Int
    public let dataDirectoryCount: UInt32

    public var isExecutableImage: Bool { characteristics & 0x0002 != 0 }
    public var isDLL: Bool { characteristics & 0x2000 != 0 }
    public var sectionTableOffset: Int { optionalHeaderOffset + optionalHeaderSize }

    public init?(data: Data) {
        let reader = ByteReader(data)
        guard reader.u16(0) == 0x5a4d, let pe = reader.u32(0x3c).map(Int.init),
              reader.u32(pe) == 0x0000_4550,
              let machine = reader.u16(pe + 4), let sections = reader.u16(pe + 6),
              let optionalSize = reader.u16(pe + 20).map(Int.init),
              let flags = reader.u16(pe + 22) else { return nil }
        let optional = pe + 24
        guard optionalSize >= 2, let magic = reader.u16(optional), magic == 0x10b || magic == 0x20b else { return nil }
        let prefixSize = magic == 0x20b ? 112 : 96
        guard optionalSize >= prefixSize, optional <= data.count,
              optionalSize <= data.count - optional,
              let subsystem = reader.u16(optional + 68),
              let count = reader.u32(optional + prefixSize - 4),
              Int(count) <= (optionalSize - prefixSize) / 8 else { return nil }
        let table = optional + optionalSize
        guard Int(sections) <= (data.count - table) / 40 else { return nil }
        self.machine = machine
        self.characteristics = flags
        self.sectionCount = sections
        self.optionalHeaderOffset = optional
        self.optionalHeaderSize = optionalSize
        self.isPE32Plus = magic == 0x20b
        self.subsystem = subsystem
        self.dataDirectoryOffset = optional + prefixSize
        self.dataDirectoryCount = count
    }

    /// Directory entries must lie inside both the optional header and the declared directory count.
    public func directoryRVA(_ index: Int, in data: Data) -> UInt32? {
        guard index >= 0, index < Int(dataDirectoryCount),
              index < (optionalHeaderSize - (isPE32Plus ? 112 : 96)) / 8 else { return nil }
        return ByteReader(data).u32(dataDirectoryOffset + index * 8)
    }
}
