import Foundation
import MachO

public enum RPCBundle {
    public static func executableURL() throws -> URL {
        var size: UInt32 = 0
        _ = _NSGetExecutablePath(nil, &size)
        var bytes = [CChar](repeating: 0, count: Int(size))
        guard _NSGetExecutablePath(&bytes, &size) == 0 else { throw RPCFailure("无法定位当前Cider组件。") }
        return URL(fileURLWithPath: String(decoding: bytes.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)).standardizedFileURL.resolvingSymlinksInPath()
    }
    public static func containingApp(for executable: URL) -> URL? {
        let macOS = executable.deletingLastPathComponent(), contents = macOS.deletingLastPathComponent()
        let bundle = contents.deletingLastPathComponent()
        guard macOS.lastPathComponent == "MacOS", contents.lastPathComponent == "Contents",
              bundle.pathExtension.lowercased() == "app" else { return nil }
        return bundle
    }
    /// A development CLI delegates to its packaged copy, never to Wine or a shell.
    public static func appForDevelopmentCLI(executable: URL, environment: [String: String]) -> URL? {
        if let value = environment["CIDER_APP"], !value.isEmpty {
            return URL(fileURLWithPath: value).standardizedFileURL.resolvingSymlinksInPath()
        }
        var parent = executable.deletingLastPathComponent()
        for _ in 0..<10 {
            let candidate = parent.appendingPathComponent("out/Cider.app")
            if FileManager.default.isExecutableFile(atPath: candidate.appendingPathComponent("Contents/MacOS/ciderctl").path),
               FileManager.default.isExecutableFile(atPath: candidate.appendingPathComponent("Contents/MacOS/CiderXPCBridge").path) { return candidate }
            let next = parent.deletingLastPathComponent(); if next == parent { break }; parent = next
        }
        let installed = URL(fileURLWithPath: "/Applications/Cider.app")
        return FileManager.default.isExecutableFile(atPath: installed.appendingPathComponent("Contents/MacOS/CiderXPCBridge").path) ? installed : nil
    }
}
