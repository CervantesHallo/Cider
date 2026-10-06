import CiderCore
import Foundation

/// A small app bundle inside each engine (`Engines/<id>/CiderWineHost.app`) whose `Contents/MacOS` holds the Wine
/// loader and wineserver (hard links, so no extra disk space). Launching Wine from there gives every Wine process an
/// app identity (`org.cider.winehost`): a named Dock entry, TCC prompts attributed to Cider, and windows that
/// accessibility and automation tools can address — a bare `bin/wine` process has none of that (docs/plan/01 §1, ADR-007).
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
            try? fm.removeItem(at: link)
            try fm.linkItem(at: source, to: link)  // hard link: same inode, stays inside the bundle
        }

        let ntdll = macOS.appendingPathComponent("ntdll.so")
        try? fm.removeItem(at: ntdll)
        if usesNativeLoader, let nativeLoader {
            let target = nativeLoader.deletingLastPathComponent().appendingPathComponent("ntdll.so")
            try fm.createSymbolicLink(atPath: ntdll.path,
                                      withDestinationPath: relativePath(from: macOS, to: target))
        }

        let relativeRoot = relativePath(from: contents, to: wineRoot)
        for dir in ["lib", "share"] {
            let link = contents.appendingPathComponent(dir)
            try? fm.removeItem(at: link)
            try fm.createSymbolicLink(atPath: link.path, withDestinationPath: "\(relativeRoot)/\(dir)")
        }

        let plist: [String: Any] = [
            "CFBundleIdentifier": bundleIdentifier,
            "CFBundleName": "Cider",
            "CFBundleDisplayName": "Cider",
            "CFBundleExecutable": "wine",
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": "0.0.1",
            "LSMinimumSystemVersion": "14.0",
            "LSApplicationCategoryType": "public.app-category.games",
            "NSHighResolutionCapable": true,
            "NSMicrophoneUsageDescription": "Windows 程序（例如游戏语音）需要使用麦克风。",
            "NSCameraUsageDescription": "Windows 程序需要使用摄像头。",
            "NSLocalNetworkUsageDescription": "局域网联机和部分启动器需要访问本地网络。",
        ]
        let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
        try data.write(to: contents.appendingPathComponent("Info.plist"), options: .atomic)
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
