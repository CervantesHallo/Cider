import Foundation
import Darwin

public enum JSONFile {
    public static func read<T: Decodable>(_ type: T.Type, from url: URL) throws -> T {
        let data = try Data(contentsOf: url)
        return try JSONDecoder().decode(T.self, from: data)
    }

    /// Imported transaction metadata must not follow a link or allocate an unbounded file.
    public static func readMetadata<T: Decodable>(_ type: T.Type, from url: URL, limit: Int = 16 << 20) throws -> T {
        let descriptor = open(url.path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)
        guard descriptor >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        let handle = FileHandle(fileDescriptor: descriptor, closeOnDealloc: true)
        defer { try? handle.close() }
        var info = stat()
        guard fstat(descriptor, &info) == 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        guard info.st_mode & mode_t(S_IFMT) == mode_t(S_IFREG), info.st_size >= 0, info.st_size <= limit else {
            throw CiderError.invalid("恢复元数据必须是大小受限的普通文件。")
        }
        let data = try handle.read(upToCount: limit + 1) ?? Data()
        guard data.count <= limit else { throw CiderError.invalid("恢复元数据超过读取预算。") }
        return try JSONDecoder().decode(type, from: data)
    }

    public static func writeMetadata<T: Encodable>(_ value: T, to url: URL, limit: Int = 16 << 20) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        var data = try encoder.encode(value)
        data.append(0x0A)
        guard data.count <= limit else { throw CiderError.invalid("恢复元数据超过存储预算，操作未提交。") }
        try data.write(to: url, options: .atomic)
    }

    /// Writes pretty, key-sorted JSON atomically.
    public static func write<T: Encodable>(_ value: T, to url: URL) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        var data = try encoder.encode(value)
        data.append(0x0A)
        try data.write(to: url, options: .atomic)
    }
}
