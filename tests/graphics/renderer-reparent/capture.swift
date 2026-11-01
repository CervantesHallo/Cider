import Foundation
import AppKit
import ScreenCaptureKit
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
import CryptoKit
import Darwin

struct CaptureFailure: Error, CustomStringConvertible {
    let description: String
    init(_ description: String) { self.description = description }
}

// Open every component relative to a held directory. No pathname read/write
// follows a later symlink substitution. Parent enforces a25s process watchdog.
final class PinnedParent {
    let fd: Int32
    let leaf: String
    init(_ url: URL) throws {
        let components = url.path.split(separator: "/").map(String.init)
        guard let last = components.last, components.allSatisfy({ !$0.isEmpty && $0 != "." && $0 != ".." }) else { throw CaptureFailure("invalid absolute path") }
        var current = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)
        guard current >= 0 else { throw CaptureFailure("open filesystem root failed") }
        do {
            for name in components.dropLast() {
                let next = openat(current, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)
                guard next >= 0 else { throw CaptureFailure("open pinned no-follow parent failed") }
                close(current); current = next
            }
        } catch { close(current); throw error }
        fd = current; leaf = last
    }
    deinit { close(fd) }
}
func exactRead(_ fd: Int32, count: Int) throws -> Data {
    guard count >= 0, count <= 20 * 1024 * 1024 else { throw CaptureFailure("read size bound") }
    var buffer = [UInt8](repeating: 0, count: count + 1)
    var used = 0
    try buffer.withUnsafeMutableBytes { bytes in
        while used < bytes.count {
            let n = pread(fd, bytes.baseAddress!.advanced(by: used), bytes.count - used, off_t(used))
            if n < 0 && errno == EINTR { continue }
            guard n >= 0 else { throw CaptureFailure("descriptor read failed") }
            if n == 0 { break }
            used += n
        }
    }
    guard used == count else { throw CaptureFailure("descriptor exact length/EOF mismatch") }
    return Data(buffer.prefix(count))
}

// Candidate only: do not run until the user explicitly selects this native
// window-capture method. Never request new screen permissions in this tool.
@main struct HostCapture {
    @MainActor static func main() async {
        do { try await capture() }
        catch {
            let message = "capture failed: \(error)\n"
            FileHandle.standardError.write(Data(message.utf8))
            exit(1)
        }
    }
    @MainActor static func capture() async throws {
        _ = NSApplication.shared // Establish AppKit/WindowServer connection on MainActor; no windows or activation.
        let args = CommandLine.arguments
        guard args.count == 5, args[1] == "--consumer" else {
            throw CaptureFailure("usage: capture --consumer PRIVATE_ENGINE_DIRECTORY READY_FILE OUTPUT.png")
        }
        let engine = URL(fileURLWithPath: args[2]).standardizedFileURL
        guard engine.path.hasPrefix("/Users/cervantes/Cider/out/waitall-mac-20261018-admission/private-home/AppSupport/Engines/"),
              engine.resolvingSymlinksInPath().path == engine.path else { throw CaptureFailure("expected retained private engine directory") }
        let ready = URL(fileURLWithPath: args[3]).standardizedFileURL
        let output = URL(fileURLWithPath: args[4]).standardizedFileURL
        let allowed = URL(fileURLWithPath: "/Users/cervantes/Cider/out").standardizedFileURL.path + "/"
        guard ready.path.hasPrefix(allowed), output.path.hasPrefix(allowed),
              ready.lastPathComponent.hasPrefix("consumer-ready-"), output.pathExtension == "png",
              !FileManager.default.fileExists(atPath: output.path) else {
            throw CaptureFailure("expected isolated ready-file and fresh owned PNG destination")
        }
        let clock = DispatchTime.now().uptimeNanoseconds
        let (deadline, overflow) = clock.addingReportingOverflow(20_000_000_000)
        guard !overflow else { throw CaptureFailure("monotonic deadline overflow") }
        func inTime() throws {
            guard DispatchTime.now().uptimeNanoseconds < deadline else { throw CaptureFailure("capture exceeded20s; parent must enforce25s process watchdog") }
        }
        let readyParent = try PinnedParent(ready)
        let outputParent = try PinnedParent(output)
        let inputFD = openat(readyParent.fd, readyParent.leaf, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)
        guard inputFD >= 0 else { throw CaptureFailure("open ready no-follow failed") }
        defer { close(inputFD) }
        var initial = stat()
        guard fstat(inputFD, &initial) == 0, initial.st_mode & S_IFMT == S_IFREG, initial.st_size == 64 else {
            throw CaptureFailure("expected regular64-byte ready descriptor")
        }
        let token = try exactRead(inputFD, count: 64)
        func readyValid() throws {
            try inTime()
            var named = stat(), held = stat()
            guard fstatat(readyParent.fd, readyParent.leaf, &named, AT_SYMLINK_NOFOLLOW) == 0,
                  fstat(inputFD, &held) == 0, named.st_dev == initial.st_dev, named.st_ino == initial.st_ino,
                  held.st_dev == initial.st_dev, held.st_ino == initial.st_ino,
                  named.st_mode & S_IFMT == S_IFREG, held.st_mode & S_IFMT == S_IFREG,
                  named.st_size == 64, held.st_size == 64,
                  held.st_mtimespec.tv_sec == initial.st_mtimespec.tv_sec,
                  held.st_mtimespec.tv_nsec == initial.st_mtimespec.tv_nsec,
                  held.st_ctimespec.tv_sec == initial.st_ctimespec.tv_sec,
                  held.st_ctimespec.tv_nsec == initial.st_ctimespec.tv_nsec,
                  abs(Double(held.st_mtimespec.tv_sec) + Double(held.st_mtimespec.tv_nsec) / 1e9 - Date().timeIntervalSince1970) < 60,
                  try exactRead(inputFD, count: 64) == token else {
                throw CaptureFailure("ready identity/type/bytes/freshness changed")
            }
        }
        try readyValid()
        func word(_ offset: Int) -> UInt32 {
            (0..<4).reduce(UInt32(0)) { $0 | UInt32(token[offset + $1]) << (8 * $1) }
        }
        guard token.count == 64, word(0) == 0x43465231, word(4) == 64, word(8) == 1,
              [1, 2, 3].contains(word(12)), word(28) == word(12), word(16) > 0,
              word(20) > 0, word(24) > 0, word(20) != word(24), word(48) > 0,
              word(52) > 0, word(48) != word(52), word(56) == 160, word(60) == 100 else {
            throw CaptureFailure("ordinary consumer token mismatch")
        }
        func quad(_ offset: Int) -> UInt64 {
            (0..<8).reduce(UInt64(0)) { $0 | UInt64(token[offset + $1]) << (8 * $1) }
        }
        let nonce = String(format: "%016llx%016llx", quad(32), quad(40))
        guard ready.lastPathComponent == "consumer-ready-\(nonce)-\(word(12)).bin" else {
            throw CaptureFailure("exact consumer nonce/phase filename mismatch")
        }
        let expectedTitle = "Cider renderer consumer " + nonce
        func executable(_ pid: pid_t) throws -> String {
            var bytes = [CChar](repeating: 0, count: 4096)
            let count = bytes.withUnsafeMutableBytes { proc_pidpath(pid, $0.baseAddress, UInt32($0.count)) }
            guard count > 0 else { throw CaptureFailure("native owner executable unavailable") }
            let utf8 = bytes.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }
            guard let path = String(bytes: utf8, encoding: .utf8) else { throw CaptureFailure("native owner path encoding") }
            return path
        }
        guard CGPreflightScreenCaptureAccess() else {
            throw CaptureFailure("existing screen-capture permission unavailable; no permission requested")
        }
        let content = try await SCShareableContent.excludingDesktopWindows(true, onScreenWindowsOnly: false)
        let matches = content.windows.filter { $0.title == expectedTitle }
        guard matches.count == 1, let window = matches.first,
              let owner = window.owningApplication, owner.processID > 0 else {
            throw CaptureFailure("exact nonce consumer window unavailable/ambiguous")
        }
        let windowID = window.windowID
        let processID = owner.processID
        let nativeExecutable = try executable(processID)
        guard nativeExecutable.hasPrefix(engine.path + "/") else {
            throw CaptureFailure("consumer owner is outside verified private engine")
        }
        let filter = SCContentFilter(desktopIndependentWindow: window)
        let size = filter.contentRect.size
        let scale = Double(filter.pointPixelScale)
        guard size.width > 0, size.height > 0, size.width <= 4096, size.height <= 4096,
              scale > 0, scale <= 2 else { throw CaptureFailure("unexpected test window size/scale") }
        let config = SCStreamConfiguration()
        config.width = Int(ceil(size.width * scale))
        config.height = Int(ceil(size.height * scale))
        config.showsCursor = false
        config.colorSpaceName = CGColorSpace.sRGB
        let image = try await SCScreenshotManager.captureImage(contentFilter: filter, configuration: config)
        try readyValid()
        let after = try await SCShareableContent.excludingDesktopWindows(true, onScreenWindowsOnly: false)
        guard after.windows.contains(where: { $0.windowID == windowID && $0.owningApplication?.processID == processID && $0.title == window.title }) else {
            throw CaptureFailure("test window owner disappeared during capture")
        }
        guard try executable(processID) == nativeExecutable else { throw CaptureFailure("native owner executable changed") }
        try readyValid() // after the final awaited query
        let bytes = NSMutableData()
        guard let destination = CGImageDestinationCreateWithData(bytes, UTType.png.identifier as CFString, 1, nil) else {
            throw CaptureFailure("PNG destination failed")
        }
        CGImageDestinationAddImage(destination, image, nil)
        guard CGImageDestinationFinalize(destination) else { throw CaptureFailure("PNG encoding failed") }
        let png = bytes as Data
        try readyValid() // final check immediately before exclusive descriptor-relative commit
        let outputFD = openat(outputParent.fd, outputParent.leaf, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0o600)
        guard outputFD >= 0 else { throw CaptureFailure("exclusive pinned PNG create failed") }
        defer { close(outputFD) }
        try png.withUnsafeBytes { buffer in
            var used = 0
            while used < buffer.count {
                try inTime()
                let n = Darwin.write(outputFD, buffer.baseAddress!.advanced(by: used), buffer.count - used)
                if n < 0 && errno == EINTR { continue }
                guard n > 0 else { throw CaptureFailure("PNG write failed; partial owned output retained") }
                used += n
            }
        }
        guard fsync(outputFD) == 0, try exactRead(outputFD, count: png.count) == png else {
            throw CaptureFailure("PNG disk commit/readback failed")
        }
        try inTime()
        let digest = SHA256.hash(data: png).map { String(format: "%02x", $0) }.joined()
        let receipt: [String: Any] = ["window": windowID, "ownerPID": processID, "phase": word(12), "nativeExecutable": nativeExecutable, "windowsHostPID": word(20), "windowsRendererPID": word(24), "nonce": nonce,
            "readySHA256": SHA256.hash(data: token).map { String(format: "%02x", $0) }.joined(),
            "png": output.path, "pngSHA256": digest, "width": image.width, "height": image.height,
            "source": "ScreenCaptureKit single desktop-independent test window", "screenPixelsAccepted": false,
            "windowOnScreenBefore": window.isOnScreen, "windowOnScreenAfter": after.windows.first(where: { $0.windowID == windowID && $0.owningApplication?.processID == processID })?.isOnScreen ?? false,
            "physicalDisplayAccepted": false]
        let json = try JSONSerialization.data(withJSONObject: receipt, options: [.sortedKeys])
        FileHandle.standardOutput.write(json + Data([10]))
    }
}
