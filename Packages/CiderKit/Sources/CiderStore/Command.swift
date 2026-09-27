import CiderCore
import Foundation

/// Runs a short-lived host tool and returns its combined output. Not for Wine sessions (see CiderRuntime).
public enum Command {
    @discardableResult
    public static func run(_ executable: String, _ arguments: [String], environment: [String: String]? = nil) throws -> String {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        if let environment { process.environment = environment }
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        process.standardInput = FileHandle.nullDevice
        try process.run()
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        let output = String(decoding: data, as: UTF8.self)
        guard process.terminationStatus == 0 else {
            throw CiderError.commandFailed(command: ([executable] + arguments).joined(separator: " "), status: process.terminationStatus, output: output)
        }
        return output
    }
}
