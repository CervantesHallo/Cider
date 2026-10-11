import Foundation

/// Decoder for the candidate wine-exit-status collector, not a process-tree cleanup
/// receipt, verified binary provenance, or permission to launch a gated program.
public struct WindowsExitStatus: Decodable, Sendable {
    public enum Phase: String, Decodable, Sendable {
        case captured
        case setupFailed = "setup-failed"
        case createFailed = "create-failed"
        case queryFailed = "query-failed"
        case timeout
        case waitFailed = "wait-failed"
    }

    public let schema: String
    public let phase: Phase
    public let win32Error: UInt32
    public let childPID: UInt32
    public let childCreated: Bool
    public let primaryTerminated: Bool
    public let elapsedMilliseconds: UInt64
    public let cleanupScope: String
    private let exitCode: UInt32?

    public init(data: Data) throws {
        guard data.count <= 4096 else { throw CiderError.invalid("Windows 退出状态记录过大。") }
        self = try JSONDecoder().decode(Self.self, from: data)
        guard schema == "cider.windows-exit-status/v1", cleanupScope == "primary-process-only" else {
            throw CiderError.invalid("Windows 退出状态记录的格式不受支持。")
        }
    }

    /// Available only after a complete normal capture; timeout/partial/query failure
    /// cannot turn a late observed zero exit into installation success.
    public var capturedWindowsExitCode: UInt32? {
        guard schema == "cider.windows-exit-status/v1", cleanupScope == "primary-process-only",
              phase == .captured, win32Error == 0, childCreated, primaryTerminated, childPID != 0 else { return nil }
        return exitCode
    }

    /// MSI-specific classification of the complete DWORD. Do not apply to an arbitrary EXE.
    /// Requires separate installation detection and, where indicated, reboot handling.
    public var msiDisposition: MSIDisposition? {
        guard let code = capturedWindowsExitCode else { return nil }
        switch code {
        case 0: return .completed
        case 1641: return .rebootInitiated
        case 3010: return .rebootRequired
        default: return .failed(code)
        }
    }

    public enum MSIDisposition: Sendable, Equatable {
        case completed, rebootInitiated, rebootRequired
        case failed(UInt32)
    }

    private enum CodingKeys: String, CodingKey {
        case schema, phase
        case win32Error = "win32_error"
        case childPID = "child_pid"
        case childCreated = "child_created"
        case primaryTerminated = "primary_terminated"
        case exitCode = "windows_exit_code"
        case elapsedMilliseconds = "elapsed_ms"
        case cleanupScope = "cleanup_scope"
    }
}
