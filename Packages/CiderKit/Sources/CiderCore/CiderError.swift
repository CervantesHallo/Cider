import Foundation

public enum CiderError: Error, CustomStringConvertible, Sendable {
    case notFound(String)
    case alreadyExists(String)
    case invalid(String)
    case commandFailed(command: String, status: Int32, output: String)
    case unsupported(String)

    public var description: String {
        switch self {
        case .notFound(let what): return "not found: \(what)"
        case .alreadyExists(let what): return "already exists: \(what)"
        case .invalid(let why): return "invalid: \(why)"
        case .commandFailed(let command, let status, let output):
            let tail = output.split(separator: "\n").suffix(12).joined(separator: "\n")
            return "command failed (\(status)): \(command)\n\(tail)"
        case .unsupported(let why): return "unsupported: \(why)"
        }
    }
}
