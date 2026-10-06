import CiderCore
import Darwin
import Foundation

/// A small app bundle inside each engine (`Engines/<id>/CiderWineHost.app`) whose `Contents/MacOS` holds the Wine
/// loader and wineserver (hard links, so no extra disk space). Launching Wine from there gives every Wine process an
/// app identity (`org.cider.winehost`). It starts as an agent: services and CEF helpers must not occupy
/// Dock slots. Wine promotes processes with real windows to regular applications. TCC/automation and
/// Game Mode behavior still need separate acceptance (docs/plan/01 §1, ADR-007).
///
/// Wine locates its libraries relative to the loader (`<bindir>/../lib/wine`, `<bindir>/../share/wine`), so the bundle
/// carries `Contents/lib` and `Contents/share` symlinks into the engine's Wine tree.
public enum EngineHost {
    public static let bundleName = "CiderWineHost.app"
    public static let bundleIdentifier = "org.cider.winehost"

    public static func bundleURL(in engineDirectory: URL) -> URL {
        engineDirectory.appendingPathComponent(bundleName, isDirectory: true)
    }

    /// Creates (or repairs) the host bundle for an engine whose Wine tree is at `wineRoot`.
    public static func ensure(engineDirectory: URL, wineRoot: URL, cpuBackend: String = "rosetta-x86_64") throws {
        let fm = FileManager.default
        // Bottles and CLI processes can share an engine; serialize first-time creation and repair.
        let lock = open(engineDirectory.appendingPathComponent(".winehost.lock").path,
                        O_RDONLY | O_CREAT | O_CLOEXEC, 0o600)
        guard lock >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        defer { close(lock) }
        let deadline = ProcessInfo.processInfo.systemUptime + 5
        while flock(lock, LOCK_EX | LOCK_NB) != 0 {
            let code = errno
            guard code == EINTR || code == EWOULDBLOCK || code == EAGAIN else {
                throw POSIXError(POSIXErrorCode(rawValue: code) ?? .EIO)
            }
            guard ProcessInfo.processInfo.systemUptime < deadline else {
                throw CiderError.invalid("Windows 应用宿主正在准备，请稍后重试。")
            }
            usleep(20_000)
        }
        defer { _ = flock(lock, LOCK_UN) }
        let contents = bundleURL(in: engineDirectory).appendingPathComponent("Contents", isDirectory: true)
        let macOS = contents.appendingPathComponent("MacOS", isDirectory: true)
        try fm.ensureDirectory(macOS)

        // Recent Wine's bin/wine is a bootstrap wrapper; its actual Unix loader
        // must live in the bundle or exec immediately loses the application identity.
        let unixArchitecture = ["rosetta-x86_64": "x86_64", "fex-arm64": "aarch64"][cpuBackend]
        let nativeLoader = unixArchitecture.map { wineRoot.appendingPathComponent("lib/wine/\($0)-unix/wine") }
        let usesNativeLoader = nativeLoader.map { fm.isExecutableFile(atPath: $0.path) } ?? false
        for tool in ["wine", "wineserver"] {
            let source: URL
            if tool == "wine", usesNativeLoader, let nativeLoader { source = nativeLoader }
            else { source = wineRoot.appendingPathComponent("bin/\(tool)") }
            let link = macOS.appendingPathComponent(tool)
            let sourceAttributes = try fm.attributesOfItem(atPath: source.path)
            let linkAttributes = try? fm.attributesOfItem(atPath: link.path)
            if sourceAttributes[.systemFileNumber] as? NSNumber != linkAttributes?[.systemFileNumber] as? NSNumber
                || sourceAttributes[.systemNumber] as? NSNumber != linkAttributes?[.systemNumber] as? NSNumber {
                let temporary = macOS.appendingPathComponent(".\(UUID().uuidString)-\(tool)")
                defer { try? fm.removeItem(at: temporary) }
                try fm.linkItem(at: source, to: temporary)
                try replaceAtomically(temporary, at: link)
            }
        }

        let ntdll = macOS.appendingPathComponent("ntdll.so")
        if usesNativeLoader, let nativeLoader {
            let target = nativeLoader.deletingLastPathComponent().appendingPathComponent("ntdll.so")
            try ensureSymlink(ntdll, destination: relativePath(from: macOS, to: target))
        } else if (try? fm.attributesOfItem(atPath: ntdll.path)) != nil {
            try fm.removeItem(at: ntdll)
        }

        let relativeRoot = relativePath(from: contents, to: wineRoot)
        for dir in ["lib", "share"] {
            let link = contents.appendingPathComponent(dir)
            try ensureSymlink(link, destination: "\(relativeRoot)/\(dir)")
        }

        let resources = contents.appendingPathComponent("Resources", isDirectory: true)
        let icon = resources.appendingPathComponent("CiderHost.icns")
        do {
            if let source = Bundle.main.url(forResource: "AppIcon", withExtension: "icns") {
                let data = try Data(contentsOf: source)
                if (try? Data(contentsOf: icon)) != data {
                    try fm.ensureDirectory(resources)
                    try data.write(to: icon, options: .atomic)
                }
            }
        } catch {
            // A fallback image is cosmetic; preserve launch and the engine's required metadata.
            FileHandle.standardError.write(Data("Cider: 宿主备用图标未更新：\(error)\n".utf8))
        }

        var plist: [String: Any] = [
            "CFBundleIdentifier": bundleIdentifier,
            "CFBundleName": "Cider",
            "CFBundleDisplayName": "Cider",
            "CFBundleExecutable": "wine",
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": "0.0.1",
            "LSMinimumSystemVersion": "14.0",
            "LSApplicationCategoryType": "public.app-category.games",
            // Match Wine's own embedded plist: only a process presenting UI is promoted to the Dock.
            "LSUIElement": true,
            "NSHighResolutionCapable": true,
            "NSMicrophoneUsageDescription": "Windows 程序（例如游戏语音）需要使用麦克风。",
            "NSCameraUsageDescription": "Windows 程序需要使用摄像头。",
            "NSLocalNetworkUsageDescription": "局域网联机和部分启动器需要访问本地网络。",
        ]
        if fm.fileExists(atPath: icon.path) { plist["CFBundleIconFile"] = "CiderHost.icns" }
        let info = contents.appendingPathComponent("Info.plist")
        let existing = (try? Data(contentsOf: info)).flatMap {
            try? PropertyListSerialization.propertyList(from: $0, options: [], format: nil) as? [String: Any]
        }
        if existing.map({ NSDictionary(dictionary: $0).isEqual(to: plist) }) != true {
            let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
            try data.write(to: info, options: .atomic)
        }
    }

    private static func ensureSymlink(_ link: URL, destination: String) throws {
        let fm = FileManager.default
        if (try? fm.destinationOfSymbolicLink(atPath: link.path)) == destination { return }
        let temporary = link.deletingLastPathComponent().appendingPathComponent(".\(UUID().uuidString)-link")
        defer { try? fm.removeItem(at: temporary) }
        try fm.createSymbolicLink(atPath: temporary.path, withDestinationPath: destination)
        try replaceAtomically(temporary, at: link)
    }

    private static func replaceAtomically(_ source: URL, at destination: URL) throws {
        guard rename(source.path, destination.path) == 0 else {
            throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
        }
    }

    /// `../../Wine Devel.app/Contents/Resources/wine` style path from `base` to `target` (both absolute).
    static func relativePath(from base: URL, to target: URL) -> String {
        let b = base.standardizedFileURL.pathComponents
        let t = target.standardizedFileURL.pathComponents
        var common = 0
        while common < min(b.count, t.count), b[common] == t[common] { common += 1 }
        let ups = Array(repeating: "..", count: b.count - common)
        return (ups + t[common...]).joined(separator: "/")
    }
}
