import ArgumentParser
import CiderBottle
import CiderCore
import CiderSchema
import Foundation
import Testing
@testable import CiderCommands

private final class OutputCapture: @unchecked Sendable {
    private let lock = NSLock()
    private var chunks: [(String, Bool)] = []

    func append(_ text: String, _ stderr: Bool) {
        lock.lock()
        defer { lock.unlock() }
        chunks.append((text, stderr))
    }

    func text(stderr: Bool) -> String {
        lock.lock()
        defer { lock.unlock() }
        return chunks.filter { $0.1 == stderr }.map(\.0).joined()
    }
}

private struct Fixture {
    let root: URL
    let paths: CiderPaths

    init() throws {
        root = FileManager.default.temporaryDirectory.appendingPathComponent("cider-commands-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        paths = CiderPaths(appSupport: root.appendingPathComponent("support"),
                           caches: root.appendingPathComponent("cache"), logs: root.appendingPathComponent("logs"))
    }

    func write(_ text: String, at relative: String) throws {
        let file = root.appendingPathComponent(relative)
        try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try text.write(to: file, atomically: true, encoding: .utf8)
    }

    func run(_ arguments: [String], cwd: URL? = nil) -> (code: Int32, stdout: String, stderr: String) {
        let capture = OutputCapture()
        let code = CiderCommandExecutor.execute(arguments: arguments, workingDirectory: cwd ?? root,
                                                 paths: paths, output: capture.append)
        return (code, capture.text(stderr: false), capture.text(stderr: true))
    }

    func remove() { try? FileManager.default.removeItem(at: root) }
}

@Suite struct CiderCommandsTests {
    static let commandPaths = [
        [], ["engine"], ["engine", "install"], ["engine", "download"], ["engine", "list"], ["engine", "host"],
        ["bottle"], ["bottle", "create"], ["bottle", "list"], ["bottle", "delete"], ["bottle", "show"],
        ["bottle", "set"], ["bottle", "upgrade"], ["bottle", "duplicate"], ["bottle", "repair"],
        ["run"], ["kill"], ["steam"], ["steam", "list"], ["steam", "launch"], ["steam", "install"],
        ["ps"], ["icon"], ["diag"], ["install"], ["inspect"], ["d3dmetal"], ["d3dmetal", "import"], ["d3dmetal", "list"],
    ]

    @Test(arguments: commandPaths)
    func allExistingCommandsHaveHelp(_ command: [String]) throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let result = fixture.run(command + ["--help"])
        #expect(result.code == 0)
        #expect(result.stderr.isEmpty)
        #expect(result.stdout.contains("USAGE: ciderctl" + (command.isEmpty ? "" : " " + command.joined(separator: " "))))
        #expect(result.stdout.hasSuffix("\n"))
        #expect(!FileManager.default.fileExists(atPath: fixture.paths.appSupport.path))
        let local = try #require(CiderCommandExecutor.preflight(arguments: command + ["--help"]))
        #expect(local == CommandParseResult(exitCode: result.code, message: result.stdout, stderr: false))
        let explicit = try #require(CiderCommandExecutor.preflight(arguments: ["help"] + command))
        #expect(explicit == local)
    }

    @Test func rootHelpVersionAndBuiltInHelp() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let root = fixture.run([])
        #expect(root.code == 0)
        #expect(root.stdout.contains("SUBCOMMANDS:"))
        #expect(root.stderr.isEmpty)
        let version = fixture.run(["--version"])
        #expect(version.code == 0)
        #expect(version.stdout == CiderCommandExecutor.version + "\n")
        #expect(version.stderr.isEmpty)
        let nested = fixture.run(["help", "bottle", "set"])
        #expect(nested.code == 0)
        #expect(nested.stdout.contains("USAGE: ciderctl bottle set"))
        #expect(nested.stderr.isEmpty)
        let group = fixture.run(["engine"])
        #expect(group.code == 0)
        #expect(group.stdout.contains("USAGE: ciderctl engine"))
        let completions = fixture.run(["--generate-completion-script", "zsh"])
        #expect(completions.code == 0 && completions.stderr.isEmpty)
        #expect(completions.stdout.contains("#compdef ciderctl"))
    }

    @Test(arguments: [["no-such-command"], ["engine", "list", "--bogus"], ["run"], ["bottle", "show"],
                      ["bottle", "create", "test", "--locale"], ["inspect"]])
    func parserFailuresReturnUsageOnStderr(_ arguments: [String]) throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let result = fixture.run(arguments)
        #expect(result.code == ExitCode.validationFailure.rawValue)
        #expect(result.stdout.isEmpty)
        #expect(result.stderr.contains("Error:"))
        #expect(result.stderr.contains("Usage: ciderctl"))
        #expect(CiderCommandExecutor.preflight(arguments: arguments) == CommandParseResult(exitCode: result.code, message: result.stderr, stderr: true))
    }

    @Test func metadataMatchesParserWithoutRunningCommandsOrAccessingStores() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        for arguments in [[], ["engine"], ["bottle"], ["steam"], ["d3dmetal"], ["--version"], ["engine", "--version"],
                          ["--help-hidden"], ["help", "help"], ["help", "-h", "engine", "list"], ["--help", "engine"],
                          ["engine", "--help", "download"], ["help", "unknown"], ["help", "--", "engine", "list"],
                          ["help", "-hh", "engine", "list"], ["help", "--", "engine", "-h", "list"],
                          ["help", "--", "--help-hidden"],
                          ["--generate-completion-script", "zsh"], ["--generate-completion-script", "bash"],
                          ["--experimental-dump-help"]] {
            let result = fixture.run(arguments)
            #expect(CiderCommandExecutor.preflight(arguments: arguments) == CommandParseResult(
                exitCode: result.code, message: result.stderr.isEmpty ? result.stdout : result.stderr, stderr: !result.stderr.isEmpty
            ))
        }
        for arguments in [["engine", "install", "missing-package"], ["engine", "download"], ["engine", "host", "missing"],
                          ["engine", "list"], ["bottle", "create", "no-side-effects"], ["bottle", "list"],
                          ["bottle", "delete", "missing"], ["bottle", "set", "missing", "--name", "new"],
                          ["bottle", "repair", "missing"], ["run", "-b", "missing", "cmd", "--help"],
                          ["kill", "-b", "missing"], ["steam", "launch", "-b", "missing", "123"],
                          ["inspect", "missing.exe"], ["diag", "missing"], ["install"], ["install", "--list"],
                          ["d3dmetal", "import", "missing.dmg"]] {
            #expect(CiderCommandExecutor.preflight(arguments: arguments) == nil)
        }
        #expect(CommandContext.current == nil)
        #expect(try FileManager.default.contentsOfDirectory(atPath: fixture.root.path).isEmpty)
    }

    @Test func executionBindsAndRestoresCoreWarningSink() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let capture = OutputCapture()
        let outer = OutputCapture()
        CiderDiagnostics.$warningSink.withValue({ outer.append($0, true) }) {
            let code = CiderCommandExecutor.execute(arguments: ["engine", "list"], workingDirectory: fixture.root, paths: fixture.paths) { text, stderr in
                capture.append(text, stderr)
                if !stderr { CiderDiagnostics.warning("fixture warning") }
            }
            #expect(code == 0)
            CiderDiagnostics.warning("outer warning")
        }
        #expect(capture.text(stderr: false) == "no engines installed\n")
        #expect(capture.text(stderr: true) == "fixture warning\n")
        #expect(outer.text(stderr: true) == "outer warning")
    }

    @Test func runRetainsWaitEnvironmentCwdAndPassthrough() throws {
        let parsed = try Ciderctl.parseAsRoot([
            "run", "-b", "test", "--wait", "--debug", "default", "--cwd", "C:\\Games",
            "--env", "FIRST=one=two", "--env", "SECOND=", "C:\\Games\\game.exe", "--", "/S", "", "a b", "--wait",
        ])
        let run = try #require(parsed as? Run)
        #expect(run.bottle == "test")
        #expect(run.wait)
        #expect(run.debug == "default")
        #expect(run.workingDirectory == "C:\\Games")
        #expect(run.env == ["FIRST=one=two", "SECOND="])
        #expect(run.program == "C:\\Games\\game.exe")
        #expect(run.arguments == ["--", "/S", "", "a b", "--wait"])
        let builtin = try #require(try Ciderctl.parseAsRoot(["run", "--bottle", "test", "cmd", "/c", "--wait"]) as? Run)
        #expect(!builtin.wait)
        #expect(builtin.arguments == ["/c", "--wait"])
    }

    @Test func remainingOptionsArePreservedByParser() throws {
        let install = try #require(try Ciderctl.parseAsRoot([
            "engine", "install", "build dir", "--replace", "--id", "engine-id", "--channel", "stable",
            "--origin", "https://example.invalid/package", "--tree", "cx-rebase",
        ]) as? EngineCommand.Install)
        #expect(install.package == "build dir")
        #expect(install.replace && install.id == "engine-id" && install.channel == "stable")
        #expect(install.origin == "https://example.invalid/package" && install.tree == "cx-rebase")
        let download = try #require(try Ciderctl.parseAsRoot(["engine", "download", "engine-id", "--index", "indexes/local.json"]) as? EngineCommand.Download)
        #expect(download.id == "engine-id" && download.index == "indexes/local.json")
        let create = try #require(try Ciderctl.parseAsRoot(["bottle", "create", "Name", "--locale", "ja", "--template", "win11_64", "--engine", "engine-id"]) as? BottleCommand.Create)
        #expect(create.name == "Name" && create.locale == "ja" && create.template == "win11_64" && create.engine == "engine-id")
        let set = try #require(try Ciderctl.parseAsRoot(["bottle", "set", "id", "--name", "New", "--locale", "ko", "--engine", "new-engine"]) as? BottleCommand.Set)
        #expect(set.bottle == "id" && set.name == "New" && set.locale == "ko" && set.engine == "new-engine")
        let diag = try #require(try Ciderctl.parseAsRoot(["diag", "id", "-o", "reports"]) as? Diag)
        #expect(diag.bottle == "id" && diag.output == "reports")
        let recipe = try #require(try Ciderctl.parseAsRoot(["install", "runtime.example", "-b", "id", "--list"]) as? InstallRecipe)
        #expect(recipe.recipe == "runtime.example" && recipe.bottle == "id" && recipe.list)
        #expect(try (Ciderctl.parseAsRoot(["ps", "--all"]) as? PS)?.all == true)
    }

    @Test func emptyStoresUseOnlySuppliedPaths() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let engines = fixture.run(["engine", "list"])
        #expect(engines.code == 0 && engines.stdout == "no engines installed\n" && engines.stderr.isEmpty)
        let bottles = fixture.run(["bottle", "list"])
        #expect(bottles.code == 0 && bottles.stdout == "no bottles\n" && bottles.stderr.isEmpty)
        #expect(!FileManager.default.fileExists(atPath: fixture.paths.appSupport.path))
    }

    @Test func engineAndBottleFixturesRetainOutputAndConfigFormatting() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let manifest = EngineManifest(id: "fixture-engine", channel: "test", wine: .init(version: "wine-fixture", tree: "fixture"),
                                      root: "wine", requires: .init(macos: "14.0", rosetta: true), source: .init(origin: "fixture", sha256: ""))
        try FileManager.default.createDirectory(at: fixture.paths.engines.appendingPathComponent("fixture-engine/wine"), withIntermediateDirectories: true)
        try JSONFile.write(manifest, to: fixture.paths.engines.appendingPathComponent("fixture-engine/manifest.json"))
        let config = BottleConfig(id: "fixture-bottle", name: "Fixture 瓶子", createdBy: "fixture", createdAt: "2026-01-01T00:00:00Z",
                                 template: .win10_64, engine: .init(id: "fixture-engine"), locale: try #require(BottleLocale.named("zh-Hans")))
        let configURL = fixture.paths.bottles.appendingPathComponent("fixture-bottle/cider-bottle.json")
        try FileManager.default.createDirectory(at: configURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try JSONFile.write(config, to: configURL)
        let engine = fixture.run(["engine", "list"])
        #expect(engine.code == 0 && engine.stdout == "fixture-engine\twine-fixture\ttest\n" && engine.stderr.isEmpty)
        let bottle = fixture.run(["bottle", "list"])
        #expect(bottle.code == 0 && bottle.stdout == "fixture-bottle\tFixture 瓶子\tzh_CN.UTF-8\tfixture-engine\n" && bottle.stderr.isEmpty)
        let show = fixture.run(["bottle", "show", "Fixture 瓶子"])
        let raw = try String(contentsOf: configURL, encoding: .utf8)
        let expectedPrefix = try BottleStore(paths: fixture.paths).bottle("fixture-bottle").prefix.path
        #expect(show.code == 0 && show.stderr.isEmpty)
        #expect(show.stdout == raw + "prefix: " + expectedPrefix + "\n")
        let missing = fixture.run(["bottle", "show", "missing-fixture"])
        #expect(missing.code == 1 && missing.stdout.isEmpty && missing.stderr.contains("missing-fixture"))
    }

    @Test func inspectUsesCallerDirectoryWithoutChangingProcessState() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        try fixture.write("Nullsoft synthetic fixture", at: "caller dir/setup.exe")
        let cwd = FileManager.default.currentDirectoryPath
        let environment = ProcessInfo.processInfo.environment
        let result = fixture.run(["inspect", "./setup.exe"], cwd: fixture.root.appendingPathComponent("caller dir", isDirectory: true))
        #expect(result.code == 0)
        #expect(result.stdout == "type:     NSIS\nproduct:  —\ncompany:  —\nversion:  —\nlocale:   no preference\n")
        #expect(result.stderr.isEmpty)
        #expect(FileManager.default.currentDirectoryPath == cwd)
        #expect(ProcessInfo.processInfo.environment == environment)
    }

    @Test func contextResolvesHostPathsAndPreservesGuestNames() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let context = CommandExecutionContext(workingDirectory: fixture.root, paths: fixture.paths, dataOverride: "channel", output: { _, _ in })
        #expect(context.fileURL("folder/file.exe").path == fixture.root.appendingPathComponent("folder/file.exe").path)
        #expect(context.fileURL("~/fixture").path == FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("fixture").path)
        #expect(context.hostProgramURL("./setup.exe")?.path == fixture.root.appendingPathComponent("setup.exe").path)
        #expect(context.hostProgramURL("folder/setup.exe")?.path == fixture.root.appendingPathComponent("folder/setup.exe").path)
        let macWithBackslash = fixture.root.appendingPathComponent("game\\name.exe").path
        #expect(context.hostProgramURL(macWithBackslash)?.path == macWithBackslash)
        #expect(context.launchWorkingDirectory(macWithBackslash) == macWithBackslash)
        for program in ["winecfg", "notepad", "cmd", "game.exe", "C:\\Games\\game.exe", "C:/Games/game.exe", "C:game.exe", "\\\\server\\share\\game.exe", "dir\\game.exe"] {
            #expect(context.hostProgramURL(program) == nil)
        }
        #expect(context.launchWorkingDirectory("folder") == fixture.root.appendingPathComponent("folder").path)
        #expect(context.launchWorkingDirectory("C:/Games") == "C:/Games")
        #expect(context.launchWorkingDirectory("C:\\Games") == "C:\\Games")
        #expect(context.launchWorkingDirectory("") == "")
        #expect(context.launchWorkingDirectory(nil) == nil)
        #expect(context.resources.engineIndexURL()?.path == fixture.root.appendingPathComponent("channel/engines/index.json").path)
        #expect(context.resources.engineIndexURL(explicitIndex: "indexes/custom.json")?.path == fixture.root.appendingPathComponent("indexes/custom.json").path)
        let roots = context.resources.compatDirectories(paths: fixture.paths).map(\.path)
        #expect(Array(roots.suffix(2)) == [fixture.paths.appSupport.appendingPathComponent("Data").path, fixture.root.appendingPathComponent("channel").path])
    }

    @Test func launchLocationsPreserveCallerAndExecutableDirectories() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let config = BottleConfig(id: "location-fixture", name: "Fixture", createdBy: "fixture", createdAt: "2026-01-01T00:00:00Z",
                                 template: .win10_64, engine: .init(id: "unused"), locale: try #require(BottleLocale.named("ja")))
        let directory = fixture.paths.bottles.appendingPathComponent(config.id)
        try FileManager.default.createDirectory(at: directory.appendingPathComponent("prefix/drive_c/Games"), withIntermediateDirectories: true)
        try JSONFile.write(config, to: directory.appendingPathComponent("cider-bottle.json"))
        try fixture.write("synthetic fixture", at: "installers/setup.exe")
        let bottle = try BottleStore(paths: fixture.paths).bottle(config.id)
        let context = CommandExecutionContext(workingDirectory: fixture.root, paths: fixture.paths, output: { _, _ in })
        try CommandContext.$current.withValue(context) {
            for program in ["cmd", "winecfg", "C:\\Games\\game.exe", "C:/Games/game.exe", "C:game.exe"] {
                let run = try #require(try Ciderctl.parseAsRoot(["run", "-b", config.id, program]) as? Run)
                let location = try run.launchLocation(in: bottle)
                #expect(location.program == program)
                #expect(location.cwd == context.workingDirectory)
            }
            let host = try #require(try Ciderctl.parseAsRoot(["run", "-b", config.id, "./installers/setup.exe"]) as? Run)
            let hostLocation = try host.launchLocation(in: bottle)
            #expect(hostLocation.program == fixture.root.appendingPathComponent("installers/setup.exe").path)
            #expect(hostLocation.cwd.path == fixture.root.appendingPathComponent("installers").path)
            let explicit = try #require(try Ciderctl.parseAsRoot(["run", "-b", config.id, "--cwd", "installers", "cmd"]) as? Run)
            #expect(try explicit.launchLocation(in: bottle).cwd.path == fixture.root.appendingPathComponent("installers").path)
            let guest = try #require(try Ciderctl.parseAsRoot(["run", "-b", config.id, "--cwd", "C:\\Games", "cmd"]) as? Run)
            #expect(try guest.launchLocation(in: bottle).cwd.path == bottle.driveC.appendingPathComponent("Games").path)
        }
    }

    @Test func exitCodeAndProgressAreDeliveredWithoutProcessExit() throws {
        let fixture = try Fixture()
        defer { fixture.remove() }
        let capture = OutputCapture()
        let context = CommandExecutionContext(workingDirectory: fixture.root, paths: fixture.paths, output: capture.append)
        CommandContext.$current.withValue(context) {
            CommandContext.emit("… wineboot", stderr: true)
            #expect(CiderCommandExecutor.report(ExitCode(42)) == 42)
            #expect(CiderCommandExecutor.report(ExitCode.success) == 0)
        }
        #expect(capture.text(stderr: true) == "… wineboot\n")
        #expect(capture.text(stderr: false).isEmpty)
        #expect(CommandContext.current == nil)
    }

    @Test func taskLocalContextRestoresAfterReentrantInvocation() throws {
        let outer = try Fixture()
        defer { outer.remove() }
        let inner = try Fixture()
        defer { inner.remove() }
        let capture = OutputCapture()
        let code = CiderCommandExecutor.execute(arguments: ["engine", "list"], workingDirectory: outer.root, paths: outer.paths) { text, stderr in
            #expect(CommandContext.required.paths.appSupport == outer.paths.appSupport)
            let nested = inner.run(["--version"])
            #expect(nested.code == 0 && nested.stdout == CiderCommandExecutor.version + "\n")
            #expect(CommandContext.required.paths.appSupport == outer.paths.appSupport)
            capture.append(text, stderr)
        }
        #expect(code == 0 && capture.text(stderr: false) == "no engines installed\n")
        #expect(CommandContext.current == nil)
    }

    @Test func concurrentInspectRequestsKeepCwdAndOutputIsolated() async throws {
        let first = try Fixture()
        defer { first.remove() }
        let second = try Fixture()
        defer { second.remove() }
        try first.write("Nullsoft synthetic fixture", at: "setup.exe")
        try second.write("Inno Setup synthetic fixture", at: "setup.exe")
        await withTaskGroup(of: Void.self) { group in
            for index in 0..<12 {
                group.addTask {
                    let fixture = index.isMultiple(of: 2) ? first : second
                    let result = fixture.run(["inspect", "setup.exe"])
                    #expect(result.code == 0 && result.stderr.isEmpty)
                    #expect(result.stdout.contains(index.isMultiple(of: 2) ? "type:     NSIS\n" : "type:     Inno Setup\n"))
                    #expect(CommandContext.current == nil)
                }
            }
        }
    }
}
