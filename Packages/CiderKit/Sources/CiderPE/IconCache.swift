import CryptoKit
import Foundation
import ImageIO
import UniformTypeIdentifiers

/// Turns Windows executables' icons into cached PNG files (largest image in the icon group).
public struct IconCache: Sendable {
    public let directory: URL

    public init(directory: URL) { self.directory = directory }

    /// PNG for the icon of the PE file at `executable`, extracting and caching it on first use.
    /// The cache key covers path, size and modification date, so updated executables get fresh icons.
    public func pngURL(forExecutable executable: URL) -> URL? {
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: executable.path) else { return nil }
        let size = (attrs[.size] as? NSNumber)?.int64Value ?? 0
        let mtime = (attrs[.modificationDate] as? Date)?.timeIntervalSince1970 ?? 0
        let key = SHA256.hash(data: Data("\(executable.path)|\(size)|\(mtime)".utf8)).prefix(16)
            .map { String(format: "%02x", $0) }.joined()
        let png = directory.appendingPathComponent("\(key).png")
        if FileManager.default.fileExists(atPath: png.path) { return png }
        let miss = directory.appendingPathComponent("\(key).none")
        if FileManager.default.fileExists(atPath: miss.path) { return nil }

        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        guard let ico = PEIcon.icoData(fromFileAt: executable), Self.writeLargestImage(ico: ico, to: png) else {
            FileManager.default.createFile(atPath: miss.path, contents: nil)  // remember misses (no icon resource)
            return nil
        }
        return png
    }

    static func writeLargestImage(ico: Data, to url: URL) -> Bool {
        guard let source = CGImageSourceCreateWithData(ico as CFData, nil) else { return false }
        let count = CGImageSourceGetCount(source)
        var best: CGImage?
        for i in 0..<count {
            guard let image = CGImageSourceCreateImageAtIndex(source, i, nil) else { continue }
            if image.width > (best?.width ?? 0) || (image.width == best?.width && image.bitsPerPixel > (best?.bitsPerPixel ?? 0)) {
                best = image
            }
        }
        guard let best,
              let dest = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil) else { return false }
        CGImageDestinationAddImage(dest, best, nil)
        return CGImageDestinationFinalize(dest)
    }
}
