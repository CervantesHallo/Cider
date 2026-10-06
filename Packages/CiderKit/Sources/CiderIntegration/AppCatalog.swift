import CiderBottle
import CiderCore
import CiderData
import CiderPE
import CiderRuntime
import Foundation

/// One launchable thing in a bottle, as shown in Cider's library.
public struct CatalogApp: Identifiable, Hashable, Sendable {
    public enum Kind: Hashable, Sendable {
        case steamClient
        case steamGame(appID: String, installDir: String)
        case program(target: String)
    }

    public enum InstallState: Hashable, Sendable {
        case installed
        case downloading(progress: Double?)
    }

    public let id: String
    public let bottleID: String
    public let bottleName: String
    public let title: String
    public let kind: Kind
    public let installState: InstallState
    public let sizeOnDisk: Int64
    public let lastPlayed: Date?
    /// Square app icon (PNG/JPEG file on disk).
    public let iconURL: URL?
    /// Wide artwork for tiles and heroes; remote URLs are allowed (Steam CDN fallback).
    public let headerURL: URL?
    public let heroURL: URL?
    /// What to run: a Windows path (or builtin) plus arguments and an optional Windows working directory.
    public let launchProgram: String
    public let launchArguments: [String]
    public let workingDirectory: String?
    /// Host folder the app is installed in (Steam: steamapps/common/<dir>; programs: the executable's folder).
    public var installDirectory: URL? = nil
    /// Extra environment for this program's launch (user launchers); sync and locale stay bottle-wide.
    public var launchEnvironment: [String: String] = [:]
    /// Helper names explicitly declared by a profile. Arbitrary same-directory programs are independent.
    public var ownedProcessNames: Set<String> = []
    public var prefixPath: String? = nil
    /// File name of the program's main executable (the key for per-program Wine settings, AppDefaults).
    public var executableName: String? {
        switch kind {
        case .steamClient: return "steam.exe"
        case .program(let target): return target.replacingOccurrences(of: "\\", with: "/").split(separator: "/").last.map(String.init)
        case .steamGame: return mainExecutableName
        }
    }
    public var mainExecutableName: String? = nil

    public var isGame: Bool { if case .steamGame = kind { return true }; return false }

    /// Whether a running Windows process belongs to this app.
    public func owns(_ process: WineProcess) -> Bool {
        guard process.bottleID == bottleID, !process.isWineInfrastructure else { return false }
        if let prefixPath, let processPrefix = process.prefixPath, prefixPath != processPrefix { return false }
        let image = process.normalizedImage
        switch kind {
        case .steamClient:
            let isSteamTree = image.hasPrefix("c:/program files (x86)/steam/") || image.hasPrefix("c:/program files/steam/")
                || image.hasPrefix("c:/program files (x86)/common files/steam/")
            return isSteamTree && !image.contains("/steamapps/")
        case .steamGame(_, let installDir):
            return image.contains("/steamapps/common/\(installDir.lowercased())/")
        case .program(let target):
            let t = target.replacingOccurrences(of: "\\", with: "/").lowercased()
            if image == t { return true }
            // Launchers often start the real program from the same folder; never widen matching to system folders.
            let dir = t.split(separator: "/").dropLast().joined(separator: "/")
            guard !dir.hasPrefix("c:/windows"), dir.count > 3, image.hasPrefix(dir + "/") else { return false }
            // Installed games can live underneath their launcher's folder. Stopping the launcher must
            // not recursively stop those independent applications (miHoYo's default is <root>/games).
            let child = image.dropFirst(dir.count + 1).split(separator: "/").first
            // Wine may preserve an extensionless CreateProcess image (e.g. CEF's HYPHelper).
            // Accept only the corresponding explicitly declared .exe, within this app's folder.
            let name = process.imageName
            let declaredHelper = ownedProcessNames.contains(name)
                || (!name.contains(".") && ownedProcessNames.contains(name + ".exe"))
            return child != "games" && child != "steamapps" && declaredHelper
        }
    }
}

/// Builds the library for a bottle: the Steam client, its games, and programs from Start Menu / Desktop shortcuts.
public struct AppCatalog: Sendable {
    public let iconCache: IconCache
    public let compat: CompatDB

    public init(iconCache: IconCache, compat: CompatDB = CompatDB()) { self.iconCache = iconCache; self.compat = compat }

    public func apps(in bottle: Bottle) -> [CatalogApp] {
        var apps: [CatalogApp] = []
        let steam = SteamLibrary(bottle: bottle)
        var steamRoots: [String] = []
        if let steamExe = steam.steamExecutableWindowsPath, let steamDir = steam.steamDirectory {
            steamRoots.append(String(steamExe.dropLast("\\steam.exe".count)).lowercased())
            for game in (try? steam.games()) ?? [] {
                let exeGuess = mainExecutable(in: game.directory)
                var app = CatalogApp(
                    id: "\(bottle.config.id)/steam/\(game.appID)", bottleID: bottle.config.id, bottleName: bottle.config.name,
                    title: game.name, kind: .steamGame(appID: game.appID, installDir: game.installDir),
                    installState: game.isInstalled ? .installed : .downloading(progress: game.downloadProgress),
                    sizeOnDisk: game.sizeOnDisk, lastPlayed: game.lastPlayed,
                    iconURL: game.iconImageURL ?? exeGuess.flatMap { iconCache.pngURL(forExecutable: $0) },
                    headerURL: game.headerImageURL, heroURL: game.heroImageURL,
                    launchProgram: steamExe, launchArguments: SteamLibrary.gameArguments(appID: game.appID),
                    workingDirectory: nil, installDirectory: game.directory)
                app.mainExecutableName = exeGuess?.lastPathComponent
                apps.append(app)
            }
            apps.append(CatalogApp(
                id: "\(bottle.config.id)/steam", bottleID: bottle.config.id, bottleName: bottle.config.name,
                title: "Steam", kind: .steamClient, installState: .installed, sizeOnDisk: 0, lastPlayed: nil,
                iconURL: iconCache.pngURL(forExecutable: steamDir.appendingPathComponent("steam.exe")),
                headerURL: nil, heroURL: nil, launchProgram: steamExe, launchArguments: SteamLibrary.uiArguments,
                workingDirectory: nil, installDirectory: steamDir))
        }
        apps.append(contentsOf: launchers(in: bottle))
        apps.append(contentsOf: shortcutPrograms(in: bottle, excludingRoots: steamRoots))
        return apps.map { original in
            var app = original
            app.prefixPath = bottle.prefix.path
            app.ownedProcessNames = Set(compat.profile(exe: app.launchProgram)?.actions.processNames?.map { $0.lowercased() } ?? [])
            return app
        }
    }

    /// The bottle's saved launchers (`BottleConfig.launchers`).
    func launchers(in bottle: Bottle) -> [CatalogApp] {
        (bottle.config.launchers ?? []).map { launcher in
            let host = launcher.program.hasPrefix("/") ? URL(fileURLWithPath: launcher.program)
                : try? WindowsPath.hostURL(for: launcher.program, in: bottle)
            var app = CatalogApp(
                id: "\(bottle.config.id)/launcher/\(launcher.id)", bottleID: bottle.config.id, bottleName: bottle.config.name,
                title: launcher.name, kind: .program(target: launcher.program.hasPrefix("/")
                    ? WindowsPath.windowsPath(forHostPath: launcher.program, in: bottle) : launcher.program), installState: .installed, sizeOnDisk: 0,
                lastPlayed: nil, iconURL: host.flatMap { iconCache.pngURL(forExecutable: $0) }, headerURL: nil, heroURL: nil,
                launchProgram: launcher.program, launchArguments: launcher.arguments,
                workingDirectory: launcher.workingDirectory, installDirectory: host?.deletingLastPathComponent())
            app.launchEnvironment = launcher.environment ?? [:]
            return app
        }
    }

    /// Programs reachable from the bottle's Start Menu and Desktop shortcuts, deduplicated by target.
    func shortcutPrograms(in bottle: Bottle, excludingRoots: [String]) -> [CatalogApp] {
        let fm = FileManager.default
        var roots = [bottle.driveC.appendingPathComponent("ProgramData/Microsoft/Windows/Start Menu/Programs")]
        let users = bottle.driveC.appendingPathComponent("users")
        for user in (try? fm.contentsOfDirectory(atPath: users.path)) ?? [] {
            roots.append(users.appendingPathComponent("\(user)/AppData/Roaming/Microsoft/Windows/Start Menu/Programs"))
            roots.append(users.appendingPathComponent("\(user)/Desktop"))
        }
        let skipWords = ["uninstall", "卸载", "readme", "manual", "help", "website", "homepage", "license", "说明", "帮助"]
        var seen = Set<String>()
        var apps: [CatalogApp] = []
        for root in roots {
            guard let walker = fm.enumerator(at: root, includingPropertiesForKeys: nil) else { continue }
            for case let url as URL in walker where url.pathExtension.lowercased() == "lnk" {
                let name = url.deletingPathExtension().lastPathComponent
                guard !skipWords.contains(where: { name.lowercased().contains($0) }),
                      let link = ShellLink.parse(fileAt: url), let target = link.target,
                      target.lowercased().hasSuffix(".exe") else { continue }
                let key = target.lowercased()
                guard !seen.contains(key), !excludingRoots.contains(where: { key.hasPrefix($0 + "\\") }),
                      !key.hasPrefix("c:\\windows\\"),
                      let host = try? WindowsPath.hostURL(for: target, in: bottle), fm.fileExists(atPath: host.path) else { continue }
                seen.insert(key)
                apps.append(CatalogApp(
                    id: "\(bottle.config.id)/program/\(key)", bottleID: bottle.config.id, bottleName: bottle.config.name,
                    title: name, kind: .program(target: target), installState: .installed, sizeOnDisk: 0, lastPlayed: nil,
                    iconURL: iconCache.pngURL(forExecutable: host), headerURL: nil, heroURL: nil,
                    launchProgram: target, launchArguments: splitArguments(link.arguments ?? ""),
                    workingDirectory: link.workingDirectory, installDirectory: host.deletingLastPathComponent()))
            }
        }
        return apps.sorted { $0.title.localizedStandardCompare($1.title) == .orderedAscending }
    }

    /// Best guess of a game's main .exe (for its icon): the largest .exe in the top two folder levels,
    /// skipping installers and crash reporters.
    func mainExecutable(in directory: URL) -> URL? {
        let fm = FileManager.default
        guard let walker = fm.enumerator(at: directory, includingPropertiesForKeys: [.fileSizeKey], options: [.skipsHiddenFiles]) else { return nil }
        var best: (URL, Int)?
        for case let url as URL in walker {
            if walker.level > 2 { walker.skipDescendants(); continue }
            let name = url.lastPathComponent.lowercased()
            guard name.hasSuffix(".exe"), !["unins", "setup", "crash", "report", "redist", "vc_", "dxsetup"].contains(where: { name.contains($0) }) else { continue }
            let size = (try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0
            if size > (best?.1 ?? 0) { best = (url, size) }
        }
        return best?.0
    }

    /// Splits a Windows command-line argument string, honouring double quotes.
    func splitArguments(_ line: String) -> [String] {
        var args: [String] = []
        var current = ""
        var quoted = false
        for ch in line {
            if ch == "\"" { quoted.toggle(); continue }
            if ch == " ", !quoted { if !current.isEmpty { args.append(current); current = "" }; continue }
            current.append(ch)
        }
        if !current.isEmpty { args.append(current) }
        return args
    }
}
