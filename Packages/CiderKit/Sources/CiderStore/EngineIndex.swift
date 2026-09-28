import CiderCore
import Foundation

/// Published engines (docs/plan/00 ADR-009: engine index, separate from compatibility data; signing comes
/// with the release pipeline). The app ships `data/engines/index.json` and installs the recommended engine on
/// first run; downloads are verified by size and sha256 before anything is unpacked.
public struct EngineIndex: Codable, Sendable {
    public var schema: String
    public var engines: [Entry]

    public struct Entry: Codable, Sendable, Equatable, Identifiable {
        public var id: String
        public var channel: String
        public var wine: String
        public var summary: [String: String]?
        public var urls: [String]
        public var sha256: String
        public var size: Int64
        public var requiresMacOS: String
        public var recommended: Bool?

        enum CodingKeys: String, CodingKey {
            case id, channel, wine, summary, urls, sha256, size, recommended
            case requiresMacOS = "requires_macos"
        }
    }

    public static func load(from url: URL) -> EngineIndex? {
        guard let data = try? Data(contentsOf: url), let index = try? JSONDecoder().decode(EngineIndex.self, from: data),
              index.schema == "cider.engines/v1" else { return nil }
        return index
    }

    /// The recommended engine this Mac can run.
    public var recommended: Entry? {
        let major = ProcessInfo.processInfo.operatingSystemVersion.majorVersion
        let usable = engines.filter { (Int($0.requiresMacOS.split(separator: ".").first ?? "0") ?? 0) <= major }
        return usable.first { $0.recommended == true } ?? usable.first
    }
}

/// Downloads an index entry into the cache (resumable), verifies it, and installs it.
public struct EngineDownloader: Sendable {
    public let store: EngineStore
    public init(store: EngineStore) { self.store = store }

    public enum DownloadError: Error, CustomStringConvertible {
        case sizeMismatch(expected: Int64, got: Int64)
        case checksum
        case allMirrorsFailed(String)
        public var description: String {
            switch self {
            case .sizeMismatch(let e, let g): return "下载不完整（\(g) / \(e) 字节）"
            case .checksum: return "引擎包的校验和不匹配，已删除，请重试"
            case .allMirrorsFailed(let why): return "引擎下载失败：\(why)"
            }
        }
    }

    public func partialURL(for entry: EngineIndex.Entry) -> URL {
        store.paths.caches.appendingPathComponent("downloads/\(entry.id).tar.xz.partial")
    }

    /// Bytes downloaded so far (for progress reporting while `install` runs).
    public func downloadedBytes(for entry: EngineIndex.Entry) -> Int64 {
        let attrs = try? FileManager.default.attributesOfItem(atPath: partialURL(for: entry).path)
        return (attrs?[.size] as? NSNumber)?.int64Value ?? 0
    }

    @discardableResult
    public func install(_ entry: EngineIndex.Entry) throws -> InstalledEngine {
        let fm = FileManager.default
        let partial = partialURL(for: entry)
        try fm.ensureDirectory(partial.deletingLastPathComponent())
        var lastError = "no URL"
        var done = false
        for url in entry.urls {
            do {
                // -C - resumes an interrupted download.
                try Command.run("/usr/bin/curl", ["-fL", "--retry", "5", "--retry-delay", "3", "--connect-timeout", "30",
                                                  "-C", "-", "-o", partial.path, url])
                done = true
                break
            } catch { lastError = "\(error)" }
        }
        guard done else { throw DownloadError.allMirrorsFailed(lastError) }
        let size = downloadedBytes(for: entry)
        guard size == entry.size else { throw DownloadError.sizeMismatch(expected: entry.size, got: size) }
        guard try EngineStore.sha256(of: partial) == entry.sha256.lowercased() else {
            try? fm.removeItem(at: partial)
            throw DownloadError.checksum
        }
        let package = partial.deletingPathExtension()                    // …/<id>.tar.xz
        try? fm.removeItem(at: package)
        try fm.moveItem(at: partial, to: package)
        defer { try? fm.removeItem(at: package) }
        return try store.install(package: package, origin: entry.urls.first)
    }
}
