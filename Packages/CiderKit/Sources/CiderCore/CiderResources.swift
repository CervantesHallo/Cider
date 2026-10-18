import Foundation

/// Resource roots shared by the app and its packaged CLI. A running app's data belongs to that
/// bundle: another installed app or the working directory must not supply a different version.
public struct CiderResources: Sendable {
    private let overrideData: URL?
    private let bundledData: URL?
    private let checkoutData: URL
    private let installedData: URL
    private let currentDirectory: URL

    public init(environment: [String: String] = ProcessInfo.processInfo.environment,
                bundledResources: URL? = CiderResources.mainBundleResources,
                currentDirectory: URL = URL(fileURLWithPath: FileManager.default.currentDirectoryPath),
                installedData: URL = URL(fileURLWithPath: "/Applications/Cider.app/Contents/Resources/data")) {
        self.currentDirectory = currentDirectory
        // An empty environment variable is treated as unset. Relative overrides resolve against cwd.
        self.overrideData = environment["CIDER_DATA"].flatMap { path in
            path.isEmpty ? nil : Self.fileURL(path, relativeTo: currentDirectory)
        }
        self.bundledData = bundledResources?.appendingPathComponent("data", isDirectory: true)
        self.checkoutData = currentDirectory.appendingPathComponent("data", isDirectory: true)
        self.installedData = installedData
    }

    /// Foundation identifies the enclosing .app for both Cider and Contents/MacOS/ciderctl.
    /// A standalone SwiftPM executable has no app resource root.
    public static var mainBundleResources: URL? {
        Bundle.main.bundleURL.pathExtension.lowercased() == "app" ? Bundle.main.resourceURL : nil
    }

    /// --index, then CIDER_DATA, then this bundle. Explicit paths and bundled data are authoritative:
    /// missing or malformed indexes fail at the caller, without silently choosing another version.
    /// Only standalone binaries fall back to a checkout and then the fixed legacy installation.
    public func engineIndexURL(explicitIndex: String? = nil) -> URL? {
        if let explicitIndex { return Self.fileURL(explicitIndex, relativeTo: currentDirectory) }
        if let root = overrideData ?? bundledData { return root.appendingPathComponent("engines/index.json") }
        return [checkoutData, installedData].map { $0.appendingPathComponent("engines/index.json") }
            .first { FileManager.default.fileExists(atPath: $0.path) }
    }

    /// CompatDB merges in increasing priority. Preserve the user data-channel overlay, with the
    /// explicit CIDER_DATA overlay last. Packaged binaries never merge another app or cwd's data.
    public func compatDirectories(paths: CiderPaths) -> [URL] {
        var roots = bundledData.map { [$0] } ?? [installedData, checkoutData]
        roots.append(paths.appSupport.appendingPathComponent("Data", isDirectory: true))
        if let overrideData { roots.append(overrideData) }
        return roots
    }

    private static func fileURL(_ path: String, relativeTo directory: URL) -> URL {
        URL(fileURLWithPath: (path as NSString).expandingTildeInPath, relativeTo: directory).standardizedFileURL
    }
}
