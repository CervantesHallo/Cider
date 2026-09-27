import ArgumentParser
import CiderBottle
import CiderCore
import CiderData
import CiderIntegration
import CiderPE
import CiderRuntime
import CiderSchema
import CiderStore
import Foundation

@main
struct Ciderctl: ParsableCommand {
    static let version = "0.0.1"

    static let configuration = CommandConfiguration(
        commandName: "ciderctl",
        abstract: "Cider command-line interface.",
        version: version,
        subcommands: [EngineCommand.self, BottleCommand.self, Run.self, Kill.self, SteamCommand.self, PS.self, Icon.self, Diag.self, InstallRecipe.self, Inspect.self, D3DMetalCommand.self]
    )
}

// MARK: - icon

struct Icon: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Extract a Windows executable's icon to a cached PNG and print its path.")
    @Argument(help: "Path to an .exe or .dll on the Mac.") var executable: String
    func run() throws {
        let cache = IconCache(directory: CiderPaths.standard().caches.appendingPathComponent("icons"))
        guard let png = cache.pngURL(forExecutable: URL(fileURLWithPath: (executable as NSString).expandingTildeInPath)) else {
            throw CiderError.notFound("icon resource in \(executable)")
        }
        print(png.path)
    }
}

// MARK: - ps

struct PS: ParsableCommand {
    static let configuration = CommandConfiguration(commandName: "ps", abstract: "List running Windows processes of Cider bottles.")
    @Flag(help: "Include Wine's own service processes.") var all = false
    func run() throws {
        for p in ProcessScanner.scan().sorted(by: { ($0.bottleID, $0.pid) < ($1.bottleID, $1.pid) }) where all || !p.isWineInfrastructure {
            print("\(p.pid)\t\(p.bottleID)\t\(p.windowsImage)")
        }
    }
}

// MARK: - steam

struct SteamCommand: ParsableCommand {
    static let configuration = CommandConfiguration(
        commandName: "steam", abstract: "Work with the Windows Steam client inside a bottle.",
        subcommands: [List.self, Launch.self, Install.self]
    )


    static func steam(_ bottleName: String) throws -> (BottleStore, Bottle, SteamLibrary, String) {
        let store = BottleStore(paths: .standard())
        let bottle = try store.bottle(bottleName)
        let library = SteamLibrary(bottle: bottle)
        guard let exe = library.steamExecutableWindowsPath else { throw CiderError.notFound("Steam in bottle \(bottle.config.name)") }
        return (store, bottle, library, exe)
    }

    struct List: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "List Steam games in the bottle.")
        @Option(name: .shortAndLong, help: "Bottle id or name.") var bottle: String
        func run() throws {
            let (_, _, library, _) = try SteamCommand.steam(bottle)
            let games = try library.games()
            if games.isEmpty { print("no Steam games"); return }
            for g in games {
                let gb = Double(g.sizeOnDisk) / 1_000_000_000
                print("\(g.appID)\t\(g.name)\t\(g.isInstalled ? "installed" : "state \(g.stateFlags)")\t\(String(format: "%.1f GB", gb))")
            }
        }
    }

    struct Launch: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Start a game through Steam (Steam starts too if needed).")
        @Option(name: .shortAndLong, help: "Bottle id or name.") var bottle: String
        @Argument(help: "Steam app id, e.g. 1144400.") var appID: String
        func run() throws {
            let (store, b, _, exe) = try SteamCommand.steam(bottle)
            let runner = try store.runner(for: b)
            let session = try runner.launch(runner.plan(program: exe, arguments: SteamLibrary.gameArguments(appID: appID), label: "steam-\(appID)", extraEnv: SteamLibrary.uiEnvironment))
            print("started pid \(session.pid) · log \(session.log.path)")
        }
    }

    struct Install: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Ask Steam to install a game (opens Steam's install dialog).")
        @Option(name: .shortAndLong, help: "Bottle id or name.") var bottle: String
        @Argument(help: "Steam app id, e.g. 1144400.") var appID: String
        func run() throws {
            let (store, b, _, exe) = try SteamCommand.steam(bottle)
            let runner = try store.runner(for: b)
            let session = try runner.launch(runner.plan(program: exe, arguments: SteamLibrary.uiArguments + ["steam://install/\(appID)"], label: "steam-install-\(appID)", extraEnv: SteamLibrary.uiEnvironment))
            print("sent install request · log \(session.log.path)")
        }
    }
}

// MARK: - engine

struct EngineCommand: ParsableCommand {
    static let configuration = CommandConfiguration(
        commandName: "engine", abstract: "Manage Wine engines.",
        subcommands: [Install.self, List.self, Host.self]
    )

    struct Host: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Create or repair an engine's CiderWineHost.app (app identity for Wine processes).")
        @Argument(help: "Engine id.") var id: String
        func run() throws {
            let store = EngineStore(paths: .standard())
            let engine = try store.engine(id)
            try store.ensureHost(for: engine)
            print("host ready: \(engine.wine.path)")
        }
    }

    struct Install: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Install an engine from a package (.tar.xz) or a built engine directory (engine/build.sh output).")
        @Argument(help: "Path to the engine package or directory.") var package: String
        @Flag(help: "Replace an installed engine with the same id (the old one goes to the Trash).") var replace = false
        @Option(help: "Engine id (default: derived from the Wine version).") var id: String?
        @Option(help: "Release channel label.") var channel = "devel"
        @Option(help: "Where the package was downloaded from (recorded in the manifest).") var origin: String?
        @Option(help: "Source tree label, e.g. upstream-devel, cx-rebase (recorded in the manifest).") var tree: String?

        func run() throws {
            let store = EngineStore(paths: .standard())
            let url = URL(fileURLWithPath: (package as NSString).expandingTildeInPath)
            var isDir: ObjCBool = false
            let engine = FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir) && isDir.boolValue
                ? try store.installBuilt(directory: url, replacing: replace)
                : try store.install(package: url, id: id, channel: channel, origin: origin, tree: tree)
            print("installed \(engine.manifest.id) (\(engine.manifest.wine.version)) → \(engine.directory.path)")
        }
    }

    struct List: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "List installed engines.")
        func run() throws {
            let engines = try EngineStore(paths: .standard()).list()
            if engines.isEmpty { print("no engines installed"); return }
            for e in engines { print("\(e.manifest.id)\t\(e.manifest.wine.version)\t\(e.manifest.channel)") }
        }
    }
}

// MARK: - bottle

struct BottleCommand: ParsableCommand {
    static let configuration = CommandConfiguration(
        commandName: "bottle", abstract: "Manage bottles (Wine prefixes).",
        subcommands: [Create.self, List.self, Delete.self, Show.self, Set.self, Upgrade.self, Duplicate.self, Repair.self]
    )

    struct Set: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Change a bottle's settings.")
        @Argument(help: "Bottle id or name.") var bottle: String
        @Option(help: "New display name.") var name: String?
        @Option(help: "Windows locale: ja, zh-Hans, zh-Hant, ko, en (applies from the next launch).") var locale: String?
        @Option(help: "Switch to this engine id (snapshots the bottle first, then runs wineboot -u).") var engine: String?

        func run() throws {
            let loc = try locale.map { value -> BottleLocale in
                guard let l = BottleLocale.named(value) else { throw ValidationError("unknown locale \(value)") }
                return l
            }
            guard name != nil || loc != nil || engine != nil else { throw ValidationError("nothing to change") }
            let store = BottleStore(paths: .standard())
            var updated = try store.bottle(bottle)
            if name != nil || loc != nil {
                updated = try store.update(updated) { config in
                    if let name { config.name = name }
                    if let loc { config.locale = loc }
                }
            }
            if let engine {
                FileHandle.standardError.write(Data("… snapshot + wineboot -u with \(engine)\n".utf8))
                updated = try store.switchEngine(updated, to: engine)
            }
            print("\(updated.config.id)\t\(updated.config.name)\t\(updated.config.locale.lcAll)")
        }
    }

    struct Create: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Create a 64-bit bottle (32-bit programs run too).")
        @Argument(help: "Display name.") var name: String
        @Option(help: "Windows locale: ja, zh-Hans, zh-Hant, ko, en.") var locale = "zh-Hans"
        @Option(help: "Template: \(BottleTemplate.allCases.map(\.rawValue).joined(separator: ", ")).") var template = "win10_64"
        @Option(help: "Engine id (default: newest installed).") var engine: String?

        func run() throws {
            guard let loc = BottleLocale.named(locale) else { throw ValidationError("unknown locale \(locale)") }
            guard let tpl = BottleTemplate(rawValue: template) else { throw ValidationError("unknown template \(template)") }
            let store = BottleStore(paths: .standard())
            let bottle = try store.create(name: name, template: tpl, locale: loc, engineID: engine,
                                          createdBy: "ciderctl/\(Ciderctl.version)") { step in
                FileHandle.standardError.write(Data("… \(step)\n".utf8))
            }
            print("created \(bottle.config.id) (\(bottle.config.name), \(bottle.config.locale.lcAll), \(bottle.config.engine.id))")
        }
    }

    struct List: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "List bottles.")
        func run() throws {
            let bottles = try BottleStore(paths: .standard()).list()
            if bottles.isEmpty { print("no bottles"); return }
            for b in bottles { print("\(b.config.id)\t\(b.config.name)\t\(b.config.locale.lcAll)\t\(b.config.engine.id)") }
        }
    }

    struct Show: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Print a bottle's config and paths.")
        @Argument(help: "Bottle id or name.") var bottle: String
        func run() throws {
            let b = try BottleStore(paths: .standard()).bottle(bottle)
            print(try String(contentsOf: b.configURL, encoding: .utf8), terminator: "")
            print("prefix: \(b.prefix.path)")
        }
    }

    struct Repair: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Repair a bottle: snapshot, wineboot -u, re-apply Cider's prefix setup.")
        @Argument(help: "Bottle id or name.") var bottle: String
        func run() throws {
            let store = BottleStore(paths: .standard())
            try store.repair(try store.bottle(bottle))
            print("repaired")
        }
    }

    struct Duplicate: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Copy a bottle (APFS clone: instant, shares disk blocks until either copy changes).")
        @Argument(help: "Bottle id or name.") var bottle: String
        @Argument(help: "Name of the copy.") var name: String
        func run() throws {
            let store = BottleStore(paths: .standard())
            let copy = try store.duplicate(try store.bottle(bottle), name: name)
            print("created \(copy.config.id) (\(copy.config.name))")
        }
    }

    struct Upgrade: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Apply prefix setup added since the bottle was created (e.g. dialog fonts).")
        @Argument(help: "Bottle id or name.") var bottle: String
        func run() throws {
            let store = BottleStore(paths: .standard())
            let b = try store.bottle(bottle)
            guard store.needsPrefixUpgrade(b) else { print("\(b.config.id) is up to date"); return }
            let upgraded = try store.upgradePrefix(b)
            print("\(b.config.id): prefix revision \(upgraded.config.settings[PrefixSetup.revisionKey] ?? "?")")
        }
    }

    struct Delete: ParsableCommand {
        static let configuration = CommandConfiguration(abstract: "Move a bottle to the Trash.")
        @Argument(help: "Bottle id or name.") var bottle: String
        func run() throws {
            let store = BottleStore(paths: .standard())
            let b = try store.bottle(bottle)
            try store.delete(b)
            print("moved \(b.config.id) to the Trash")
        }
    }
}

// MARK: - run / kill

struct Run: ParsableCommand {
    static let configuration = CommandConfiguration(
        abstract: "Run a Windows program in a bottle.",
        discussion: "PROGRAM may be a Mac path (/Users/…/setup.exe), a Windows path (C:\\…), or a builtin such as winecfg, regedit, notepad, cmd."
    )
    @Option(name: .shortAndLong, help: "Bottle id or name.") var bottle: String
    @Flag(help: "Wait for the program (and the whole bottle) to exit.") var wait = false
    @Option(help: "WINEDEBUG preset: \(DebugPreset.allCases.map(\.rawValue).joined(separator: ", ")).") var debug = "default"
    @Option(name: .customLong("env"), help: "Extra KEY=VALUE environment (repeatable).") var env: [String] = []
    @Argument(help: "Program to run.") var program: String
    @Argument(parsing: .captureForPassthrough, help: "Arguments passed to the program.") var arguments: [String] = []

    func run() throws {
        guard let preset = DebugPreset(rawValue: debug) else { throw ValidationError("unknown debug preset \(debug)") }
        var extra: [String: String] = [:]
        for pair in env {
            guard let eq = pair.firstIndex(of: "=") else { throw ValidationError("--env expects KEY=VALUE") }
            extra[String(pair[..<eq])] = String(pair[pair.index(after: eq)...])
        }
        let store = BottleStore(paths: .standard())
        let b = try store.bottle(bottle)
        let runner = try store.runner(for: b)

        var cwd: URL?
        var target = program
        let expanded = (program as NSString).expandingTildeInPath
        if expanded.hasPrefix("/") {
            guard FileManager.default.fileExists(atPath: expanded) else { throw CiderError.notFound(expanded) }
            cwd = URL(fileURLWithPath: expanded).deletingLastPathComponent()
            target = expanded
        }
        let args = arguments.first == "--" ? Array(arguments.dropFirst()) : arguments
        let plan = runner.plan(program: target, arguments: args, cwd: cwd, debug: preset, extraEnv: extra)
        if wait {
            let (code, session) = try runner.runToCompletion(plan)
            try runner.waitForIdle()
            print("exit \(code) · log \(session.log.path)")
            if code != 0 { throw ExitCode(code) }
        } else {
            let session = try runner.launch(plan)
            print("started pid \(session.pid) · log \(session.log.path)")
        }
    }
}

struct Kill: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Stop every process in a bottle (wineserver -k).")
    @Option(name: .shortAndLong, help: "Bottle id or name.") var bottle: String
    func run() throws {
        let store = BottleStore(paths: .standard())
        try store.runner(for: try store.bottle(bottle)).killAll()
        print("stopped")
    }
}

// MARK: - diag

struct Diag: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Write a support bundle for a bottle (logs, config, system info; user name removed).")
    @Argument(help: "Bottle id or name.") var bottle: String
    @Option(name: .shortAndLong, help: "Directory for the .zip (default: ~/Downloads).") var output: String?
    func run() throws {
        let store = BottleStore(paths: .standard())
        let dir = output.map { URL(fileURLWithPath: ($0 as NSString).expandingTildeInPath) }
            ?? FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Downloads")
        let zip = try Diagnostics.bundle(for: try store.bottle(bottle), store: store, to: dir)
        print(zip.path)
    }
}

// MARK: - install (recipes)

/// Compatibility data: $CIDER_DATA, the user's data directory, an installed Cider.app, and ./data in a checkout.
func loadCompatDB() -> CompatDB {
    let paths = CiderPaths.standard()
    var dirs = [paths.appSupport.appendingPathComponent("Data"),
                URL(fileURLWithPath: "/Applications/Cider.app/Contents/Resources/data"),
                URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("data")]
    if let env = ProcessInfo.processInfo.environment["CIDER_DATA"] { dirs.append(URL(fileURLWithPath: env)) }
    return CompatDB(directories: dirs)
}

struct InstallRecipe: ParsableCommand {
    static let configuration = CommandConfiguration(commandName: "install", abstract: "Install something from the app directory (a recipe) into a bottle.")
    @Argument(help: "Recipe id, e.g. runtime.vcrun2022, launcher.steam. Use --list to see all.") var recipe: String?
    @Option(name: .shortAndLong, help: "Bottle id or name.") var bottle: String?
    @Flag(help: "List the available recipes.") var list = false

    func run() throws {
        let db = loadCompatDB()
        if list || recipe == nil {
            for r in db.recipes.values.sorted(by: { $0.id < $1.id }) { print("\(r.id)\t\(r.title())\t\(r.blurb())") }
            return
        }
        guard let recipe, let bottle else { throw ValidationError("need a recipe id and --bottle") }
        let store = BottleStore(paths: .standard())
        let installer = RecipeInstaller(store: store, db: db)
        try installer.install(recipe, in: try store.bottle(bottle)) { print($0) }
        print("installed \(recipe)")
    }
}

// MARK: - inspect

struct Inspect: ParsableCommand {
    static let configuration = CommandConfiguration(abstract: "Show what an installer or program is: type, product, company, language, suggested bottle locale.")
    @Argument(help: "Path to an .exe or .msi.") var file: String
    func run() throws {
        let url = URL(fileURLWithPath: (file as NSString).expandingTildeInPath)
        let info = InstallerInfo.inspect(fileAt: url)
        print("type:     \(info.kind == .unknown ? "unknown" : info.kind.rawValue)")
        print("product:  \(info.product ?? "—")")
        print("company:  \(info.company ?? "—")")
        print("version:  \(info.version ?? "—")")
        print("locale:   \(info.locale ?? "no preference")")
    }
}

// MARK: - d3dmetal

struct D3DMetalCommand: ParsableCommand {
    static let configuration = CommandConfiguration(commandName: "d3dmetal", abstract: "Import or list Apple's D3DMetal (from a Game Porting Toolkit download).",
                                                    subcommands: [Import.self, List.self])
    struct Import: ParsableCommand {
        @Argument(help: "GPTK .dmg or a folder containing redist/lib.") var path: String
        func run() throws {
            let info = try D3DMetalImporter(paths: .standard()).importPackage(at: URL(fileURLWithPath: (path as NSString).expandingTildeInPath))
            print("imported D3DMetal \(info.version) (\(info.archs.joined(separator: "/")), signature \(info.codesignValid ? "valid" : "NOT valid"))")
        }
    }
    struct List: ParsableCommand {
        func run() throws {
            for e in D3DMetalImporter(paths: .standard()).installed() { print("\(e.version)\t\(e.directory.path)") }
        }
    }
}
