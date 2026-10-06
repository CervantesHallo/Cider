import CiderBottle
import CiderCore
import Foundation

/// A game Steam knows about inside a bottle (from appmanifest_<appid>.acf).
public struct SteamGame: Equatable, Sendable {
    public let appID: String
    public let name: String
    public let installDir: String
    /// Host path of the game folder.
    public let directory: URL
    public let sizeOnDisk: Int64
    public let stateFlags: Int
    public let lastPlayed: Date?
    public var bytesToDownload: Int64 = 0
    public var bytesDownloaded: Int64 = 0
    /// Steam's local artwork cache for this app (`appcache/librarycache/<appid>/`), if present.
    public var artworkDirectory: URL?

    /// StateFlags bit 2 (value 4) = fully installed; bit 1 (2) = update required; 1024+ = downloading/staging.
    public var isInstalled: Bool { stateFlags & 4 != 0 && stateFlags & 2 == 0 && stateFlags & 1024 == 0 }

    /// Download progress 0…1 while Steam is installing/updating, else nil.
    public var downloadProgress: Double? {
        guard !isInstalled, bytesToDownload > 0 else { return nil }
        return min(1, Double(bytesDownloaded) / Double(bytesToDownload))
    }

    /// Local artwork first (works offline and where Steam's CDN is slow), then Steam's CDN.
    public var headerImageURL: URL { local("header.jpg") ?? cdn("header.jpg") }
    public var capsuleImageURL: URL { local("library_600x900.jpg") ?? cdn("library_600x900.jpg") }
    public var heroImageURL: URL { local("library_hero.jpg") ?? cdn("library_hero.jpg") }
    public var logoImageURL: URL? { local("logo.png") }
    /// The small square game icon Steam caches under a hash-named .jpg.
    public var iconImageURL: URL? {
        guard let dir = artworkDirectory,
              let files = try? FileManager.default.contentsOfDirectory(atPath: dir.path) else { return nil }
        let known: Set<String> = ["header.jpg", "library_600x900.jpg", "library_hero.jpg", "library_hero_blur.jpg", "logo.png"]
        return files.first { $0.hasSuffix(".jpg") && !known.contains($0) && $0.count >= 40 }.map { dir.appendingPathComponent($0) }
    }

    /// Steam caches art under the plain name or a per-language one (`header_schinese.jpg` when the client runs
    /// in Chinese and the game ships localized art); prefer the plain file, then the languages Cider's users read.
    func local(_ file: String) -> URL? {
        guard let dir = artworkDirectory else { return nil }
        let fm = FileManager.default
        let stem = (file as NSString).deletingPathExtension, ext = (file as NSString).pathExtension
        for lang in ["", "schinese", "tchinese", "japanese", "koreana", "english"] {
            let url = dir.appendingPathComponent(lang.isEmpty ? file : "\(stem)_\(lang).\(ext)")
            if fm.fileExists(atPath: url.path) { return url }
        }
        return nil
    }

    /// Akamai's Steam CDN answers directly from mainland China in ~0.1 s; the Cloudflare one is often slow.
    func cdn(_ file: String) -> URL { URL(string: "https://cdn.akamai.steamstatic.com/steam/apps/\(appID)/\(file)")! }
}

/// Reads the Windows Steam client's library inside a bottle.
public struct SteamLibrary: Sendable {
    public let bottle: Bottle

    public init(bottle: Bottle) { self.bottle = bottle }

    /// How the Steam client UI must be started (verified 2026-09-27 on M3 / macOS 26.7 with the Highball engine
    /// + Highball DXMT): its CEF GPU process presents into a child window owned by another process, which needs an
    /// engine with cross-process child swapchains plus DXMT's opt-in. Software rendering (`-cef-disable-gpu`) leaves
    /// the window black on macOS, so no CEF flags are passed. Sync is not set here: it is bottle-wide (`SyncMode`),
    /// and the UI runs fine under msync on this engine (the earlier hang was seen on the CX engine + DXMT).
    public static let uiEnvironment: [String: String] = [
        "DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN": "1",
    ]
    public static let uiArguments: [String] = []
    /// Starting a game while the client is closed: keep the client's main window closed (it stays in the tray),
    /// so its web views don't render next to the game. A running client just forwards `-applaunch`.
    public static func gameArguments(appID: String) -> [String] { uiArguments + ["-silent", "-applaunch", appID] }

    /// Windows paths where the Steam client installs by default.
    public static let steamWindowsPaths = [#"C:\Program Files (x86)\Steam"#, #"C:\Program Files\Steam"#]

    /// Host path of the Steam install, if present.
    public var steamDirectory: URL? {
        Self.steamWindowsPaths.lazy
            .compactMap { try? WindowsPath.hostURL(for: $0, in: bottle) }
            .first { FileManager.default.fileExists(atPath: $0.appendingPathComponent("steam.exe").path) }
    }

    /// Windows path of steam.exe, for launching.
    public var steamExecutableWindowsPath: String? {
        Self.steamWindowsPaths.first { path in
            (try? WindowsPath.hostURL(for: path, in: bottle))
                .map { FileManager.default.fileExists(atPath: $0.appendingPathComponent("steam.exe").path) } ?? false
        }.map { $0 + #"\steam.exe"# }
    }

    /// Library folders from steamapps/libraryfolders.vdf (always includes the Steam folder itself).
    public func libraryFolders() throws -> [URL] {
        guard let steam = steamDirectory else { return [] }
        var folders = [steam]
        let vdfURL = steam.appendingPathComponent("steamapps/libraryfolders.vdf")
        if let text = try? String(contentsOf: vdfURL, encoding: .utf8) {
            let root = try VDF.parse(text)
            let list = root["libraryfolders"] ?? root["LibraryFolders"]
            for entry in list?.entries ?? [] {
                // Modern format: "0" { "path" "C:\\..." }; old format: "1" "D:\\SteamLibrary".
                let windowsPath = entry.value["path"]?.string ?? entry.value.string
                guard let windowsPath, windowsPath.contains(":") else { continue }
                if let url = try? WindowsPath.hostURL(for: windowsPath, in: bottle),
                   !folders.contains(where: { $0.standardizedFileURL == url.standardizedFileURL }) {
                    folders.append(url)
                }
            }
        }
        return folders
    }

    public func games() throws -> [SteamGame] {
        var games: [SteamGame] = []
        for folder in try libraryFolders() {
            let steamapps = folder.appendingPathComponent("steamapps", isDirectory: true)
            guard let files = try? FileManager.default.contentsOfDirectory(atPath: steamapps.path) else { continue }
            for file in files where file.hasPrefix("appmanifest_") && file.hasSuffix(".acf") {
                guard let text = try? String(contentsOf: steamapps.appendingPathComponent(file), encoding: .utf8),
                      let state = try? VDF.parse(text)["AppState"],
                      let appID = state["appid"]?.string, let name = state["name"]?.string else { continue }
                let installDir = state["installdir"]?.string ?? name
                let lastPlayed = state["LastPlayed"]?.string.flatMap(TimeInterval.init).flatMap { $0 > 0 ? Date(timeIntervalSince1970: $0) : nil }
                var game = SteamGame(
                    appID: appID, name: name, installDir: installDir,
                    directory: steamapps.appendingPathComponent("common/\(installDir)", isDirectory: true),
                    sizeOnDisk: Int64(state["SizeOnDisk"]?.string ?? "") ?? 0,
                    stateFlags: Int(state["StateFlags"]?.string ?? "") ?? 0,
                    lastPlayed: lastPlayed
                )
                game.bytesToDownload = Int64(state["BytesToDownload"]?.string ?? "") ?? 0
                game.bytesDownloaded = Int64(state["BytesDownloaded"]?.string ?? "") ?? 0
                if let steam = steamDirectory {
                    let art = steam.appendingPathComponent("appcache/librarycache/\(appID)", isDirectory: true)
                    if FileManager.default.fileExists(atPath: art.path) { game.artworkDirectory = art }
                }
                games.append(game)
            }
        }
        // Steam's own tools (redistributables, Proton-like runtimes) are not games.
        let tools: Set<String> = ["228980"]  // Steamworks Common Redistributables
        return games.filter { !tools.contains($0.appID) }
            .sorted { ($0.lastPlayed ?? .distantPast, $1.name) > ($1.lastPlayed ?? .distantPast, $0.name) }
    }
}

/// Windows ↔ host path mapping through the prefix's dosdevices links.
public enum WindowsPath {
    /// Wine chooses the most specific mapped drive for a host path (normally C: for the prefix, Z: for /).
    public static func windowsPath(forHostPath path: String, in bottle: Bottle) -> String {
        let resolved = URL(fileURLWithPath: path).resolvingSymlinksInPath().path
        var roots: [(drive: String, path: String)] = [("c", bottle.driveC.resolvingSymlinksInPath().path), ("z", "/")]
        let devices = bottle.prefix.appendingPathComponent("dosdevices")
        for name in (try? FileManager.default.contentsOfDirectory(atPath: devices.path)) ?? [] {
            guard name.count == 2, name.last == ":", let letter = name.first, letter.isASCII, letter.isLetter else { continue }
            roots.append((String(letter).lowercased(), devices.appendingPathComponent(name).resolvingSymlinksInPath().path))
        }
        let root = roots.filter { resolved == $0.path || resolved.hasPrefix($0.path == "/" ? "/" : $0.path + "/") }
            .max { $0.path.count < $1.path.count }!
        let suffix = resolved.dropFirst(root.path == "/" ? 0 : root.path.count)
        return root.drive.uppercased() + ":" + (suffix.isEmpty ? "\\" : suffix.replacingOccurrences(of: "/", with: "\\"))
    }

    public static func hostURL(for windowsPath: String, in bottle: Bottle) throws -> URL {
        let normalized = windowsPath.replacingOccurrences(of: "/", with: "\\")
        guard normalized.count >= 2, normalized.dropFirst().first == ":" else {
            throw CiderError.invalid("not an absolute Windows path: \(windowsPath)")
        }
        let drive = normalized.prefix(1).lowercased()
        let rest = normalized.dropFirst(2).split(separator: "\\").map(String.init)
        let driveLink = bottle.prefix.appendingPathComponent("dosdevices/\(drive):")
        let base = drive == "c" ? bottle.driveC : driveLink.resolvingSymlinksInPath()
        return rest.reduce(base) { $0.appendingPathComponent($1) }
    }
}
