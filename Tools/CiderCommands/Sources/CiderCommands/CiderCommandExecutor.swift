import ArgumentParser
import CiderBottle
import CiderCore
import CiderData
import Foundation

/// A complete local parser response. `message` includes its final newline when nonempty.
public struct CommandParseResult: Sendable, Equatable {
    public let exitCode: Int32
    public let message: String
    public let stderr: Bool

    public init(exitCode: Int32, message: String, stderr: Bool) {
        self.exitCode = exitCode; self.message = message; self.stderr = stderr
    }
}

/// The GUI host's command backend. Arguments exclude the executable name.
/// Call from a worker task: commands such as `run --wait` intentionally block until complete.
public enum CiderCommandExecutor {
    public static var version: String { Ciderctl.version }

    /// Parse only: no command `run`, stores, execution context, or GUI connection.
    /// `nil` means an ordinary command that must be sent to the GUI host.
    public static func preflight(arguments: [String]) -> CommandParseResult? {
        do {
            let command = try Ciderctl.parseAsRoot(arguments)
            let type = type(of: command)
            if type._commandName == "help" {
                // ArgumentParser returns its internal HelpCommand for help flags. The public
                // configuration tree supplies the target without invoking that command's run().
                let explicitHelp = arguments.first == "help"
                let names = explicitHelp
                    ? try PreflightHelpArguments.parse(Array(arguments.dropFirst())).subcommands
                    : arguments
                var target: any ParsableCommand.Type = Ciderctl.self
                for name in names {
                    if name == "help", target == Ciderctl.self { target = type; continue }
                    guard let next = target.configuration.subcommands.first(where: {
                        $0._commandName == name || $0.configuration.aliases.contains(name)
                    }) else { break }
                    target = next
                }
                return helpResult(for: target, includeHidden: !explicitHelp && arguments.contains("--help-hidden"))
            }
            // These grouping commands inherit ParsableCommand's default help-only run().
            if command is Ciderctl || command is EngineCommand || command is BottleCommand
                || command is SteamCommand || command is D3DMetalCommand {
                return helpResult(for: type)
            }
            return nil
        } catch { return parseResult(for: error) }
    }

    private static func helpResult(for command: any ParsableCommand.Type, includeHidden: Bool = false) -> CommandParseResult {
        CommandParseResult(exitCode: 0, message: Ciderctl.helpMessage(for: command, includeHidden: includeHidden) + "\n", stderr: false)
    }

    private static func parseResult(for error: Error) -> CommandParseResult {
        let code = Ciderctl.exitCode(for: error).rawValue
        let message = Ciderctl.fullMessage(for: error)
        return CommandParseResult(exitCode: code, message: message.isEmpty ? "" : message + "\n", stderr: code != 0)
    }

    /// Streams text chunks, including their original newlines; `true` identifies stderr.
    /// Only the host-accepted data override is used, never the caller's process environment.
    /// The host must serialize conflicting operations just as it does for GUI actions.
    public static func execute(
        arguments: [String], workingDirectory: URL, paths: CiderPaths,
        dataOverride: String? = nil, output: @escaping @Sendable (String, Bool) -> Void
    ) -> Int32 {
        let context = CommandExecutionContext(
            workingDirectory: workingDirectory, paths: paths, dataOverride: dataOverride, output: output
        )
        return CiderDiagnostics.$warningSink.withValue({ output($0 + "\n", true) }) {
            CommandContext.$current.withValue(context) {
                do {
                    var command = try Ciderctl.parseAsRoot(arguments)
                    try command.run()
                    return 0
                } catch {
                    return report(error)
                }
            }
        }
    }

    /// ArgumentParser's default `main` and `exit` would terminate the GUI process.
    /// Its message API also handles built-in help/version/completion commands and CleanExit.
    static func report(_ error: Error) -> Int32 {
        let result = parseResult(for: error)
        if !result.message.isEmpty { CommandContext.emit(result.message, terminator: "", stderr: result.stderr) }
        return result.exitCode
    }
}

/// Mirrors ArgumentParser's built-in help arguments using public parser APIs, so
/// repeated/combined help flags and the `--` delimiter retain their normal meaning.
private struct PreflightHelpArguments: ParsableCommand {
    static let configuration = CommandConfiguration(commandName: "help", helpNames: [])
    @Argument var subcommands: [String] = []
    @Flag(name: [.short, .long, .customLong("help", withSingleDash: true)]) var help = false
}

struct CommandExecutionContext: Sendable {
    let workingDirectory: URL
    let paths: CiderPaths
    let resources: CiderResources
    let output: @Sendable (String, Bool) -> Void

    init(workingDirectory: URL, paths: CiderPaths, dataOverride: String? = nil,
         output: @escaping @Sendable (String, Bool) -> Void) {
        self.workingDirectory = workingDirectory.standardizedFileURL
        self.paths = paths
        self.output = output
        self.resources = CiderResources(
            environment: dataOverride.map { ["CIDER_DATA": $0] } ?? [:],
            currentDirectory: workingDirectory.standardizedFileURL
        )
    }

    func compatDB() -> CompatDB { CompatDB(directories: resources.compatDirectories(paths: paths)) }

    func bottleStore() -> BottleStore { BottleStore(paths: paths, compat: compatDB()) }

    func fileURL(_ path: String) -> URL {
        URL(fileURLWithPath: (path as NSString).expandingTildeInPath, relativeTo: workingDirectory).standardizedFileURL
    }

    /// A bare program is a Wine builtin or guest executable. Preserve DOS drive paths,
    /// backslash paths and UNC names; slash-containing host paths resolve at the caller's cwd.
    func hostProgramURL(_ program: String) -> URL? {
        let expanded = (program as NSString).expandingTildeInPath
        if expanded.hasPrefix("/") { return fileURL(expanded) }
        guard !Self.isWindowsPath(expanded), expanded.contains("/") else { return nil }
        return fileURL(expanded)
    }

    func launchWorkingDirectory(_ path: String?) -> String? {
        guard let path, !path.isEmpty else { return path }
        if path.hasPrefix("/") { return fileURL(path).path }
        guard !Self.isWindowsPath(path) else { return path }
        return fileURL(path).path
    }

    private static func isWindowsPath(_ path: String) -> Bool {
        let head = Array(path.unicodeScalars.prefix(2))
        return path.contains("\\") || (head.count == 2 && head[1].value == 58
            && ((65...90).contains(head[0].value) || (97...122).contains(head[0].value)))
    }
}

enum CommandContext {
    @TaskLocal static var current: CommandExecutionContext?

    static var required: CommandExecutionContext {
        guard let current else { preconditionFailure("Commands must run through CiderCommandExecutor.execute") }
        return current
    }

    static func emit(_ text: String, terminator: String = "\n", stderr: Bool = false) {
        required.output(text + terminator, stderr)
    }
}
