import CiderCore
import Foundation

public enum ArchiveExtractor {
    /// Keep BSD tar's secure path/symlink defaults; never enable -P.
    public static func unpack(_ archive: URL, to directory: URL) throws {
        let listing = try Command.run("/usr/bin/tar", ["-tf", archive.path])
        try FileSafety.validateArchiveListing(listing)
        try Command.run("/usr/bin/tar", ["-xf", archive.path, "-C", directory.path,
                                        "--no-same-owner", "--no-same-permissions", "--safe-writes"])
    }
}
