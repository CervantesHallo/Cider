import CiderCore
import CiderData
@testable import CiderRuntime
import CryptoKit
import Foundation
import Testing
@testable import CiderIntegration

@Suite struct InstallerExitStatusTests {
    private func status(_ code: UInt32, phase: String = "captured", error: UInt32 = 0) throws -> WindowsExitStatus {
        let value: [String: Any] = ["schema": "cider.windows-exit-status/v1", "phase": phase,
            "win32_error": error, "child_pid": 42, "child_created": true, "primary_terminated": true,
            "windows_exit_code": code, "elapsed_ms": 1, "cleanup_scope": "primary-process-only"]
        return try WindowsExitStatus(data: JSONSerialization.data(withJSONObject: value))
    }

    @Test func fullWordDoesNotAliasUnixSuccess() throws {
        for code: UInt32 in [256, 0x80000100, 0xc0000000, 0xc0000005, .max] {
            let captured = try status(code)
            #expect(captured.capturedWindowsExitCode == code)
            #expect(throws: RecipeInstaller.InstallError.self) {
                try RecipeInstaller.validateInstallerStatus(captured, kind: "exe", convention: nil,
                    title: "owned", report: URL(fileURLWithPath: "/tmp/owned-report.json"))
            }
        }
    }

    @Test func rebootCodesRequireDeclaredInstallerConvention() throws {
        let report = URL(fileURLWithPath: "/tmp/owned-report.json")
        #expect(try !RecipeInstaller.validateInstallerStatus(status(0), kind: "exe", convention: nil, title: "owned", report: report))
        for code: UInt32 in [1641, 3010] {
            let captured = try status(code)
            #expect(try RecipeInstaller.validateInstallerStatus(captured, kind: "msi", convention: nil, title: "owned", report: report))
            #expect(try RecipeInstaller.validateInstallerStatus(captured, kind: "exe", convention: "msi", title: "owned", report: report))
            #expect(throws: RecipeInstaller.InstallError.self) {
                try RecipeInstaller.validateInstallerStatus(captured, kind: "exe", convention: nil, title: "owned", report: report)
            }
        }
    }

    @Test func incompleteCaptureAndUnknownConventionAreNotSuccessful() throws {
        let report = URL(fileURLWithPath: "/tmp/owned-report.json")
        for phase in ["timeout", "query-failed", "wait-failed", "create-failed", "setup-failed"] {
            let captured = try status(0, phase: phase)
            #expect(captured.capturedWindowsExitCode == nil)
            #expect(throws: (any Error).self) {
                try RecipeInstaller.validateInstallerStatus(captured, kind: "exe", convention: nil, title: "owned", report: report)
            }
        }
        #expect(throws: (any Error).self) {
            try RecipeInstaller.validateInstallerStatus(status(0), kind: "exe", convention: "unknown", title: "owned", report: report)
        }
        #expect(try status(0, error: 5).capturedWindowsExitCode == nil)
    }

    private func resourceFixture() throws -> (URL, URL) {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("owned-capture-resources-" + UUID().uuidString)
        let tools = root.appendingPathComponent("windows-tools")
        try FileManager.default.createDirectory(at: tools, withIntermediateDirectories: true)
        var bytes = Data(repeating: 0, count: 512)
        func word(_ offset: Int, _ value: UInt16) { bytes[offset] = UInt8(truncatingIfNeeded: value); bytes[offset + 1] = UInt8(value >> 8) }
        word(0, 0x5a4d); word(0x3c, 0x80); word(0x80, 0x4550); word(0x84, 0x8664)
        word(0x94, 112); word(0x96, 0x22); word(0x98, 0x20b); word(0x98 + 68, 3)
        let file = tools.appendingPathComponent("cider-exit-status-x86_64.exe")
        try bytes.write(to: file)
        let sha = SHA256.hash(data: bytes).map { String(format: "%02x", $0) }.joined()
        let manifest: [String: Any] = ["schema": "cider.windows-tools/v1", "collectors": ["x86_64": ["file": file.lastPathComponent, "sha256": sha, "machine": 0x8664]]]
        try JSONSerialization.data(withJSONObject: manifest).write(to: tools.appendingPathComponent("manifest.json"))
        return (root, file)
    }

    @Test func bundledHelperUsesOwnResourceDigest() throws {
        let (root, file) = try resourceFixture()
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(try WindowsExitCollector.bundled(resources: root).executable == file)
        #expect(throws: (any Error).self) { try WindowsExitCollector.bundled(cpuBackend: "fex-arm64", resources: root) }
        try Data("tampered".utf8).write(to: file)
        #expect(throws: (any Error).self) { try WindowsExitCollector.bundled(resources: root) }
    }

    @Test func helperLinksAndDataOverlayFallbackAreRejected() throws {
        let (root, file) = try resourceFixture()
        defer { try? FileManager.default.removeItem(at: root) }
        let saved = root.appendingPathComponent("saved.exe")
        try FileManager.default.moveItem(at: file, to: saved)
        try FileManager.default.createSymbolicLink(at: file, withDestinationURL: saved)
        #expect(throws: (any Error).self) { try WindowsExitCollector.bundled(resources: root) }
        let empty = root.appendingPathComponent("empty-resources")
        try FileManager.default.createDirectory(at: empty.appendingPathComponent("data/windows-tools"), withIntermediateDirectories: true)
        #expect(throws: (any Error).self) { try WindowsExitCollector.bundled(resources: empty) }
    }

    @Test func evidenceAllowsAncestorAliasesAndRejectsForeignOrLinkedSessions() throws {
        let fm = FileManager.default
        let home = fm.temporaryDirectory.appendingPathComponent("owned-evidence-alias-" + UUID().uuidString)
        defer { try? fm.removeItem(at: home) }
        let root = home.appendingPathComponent("real/sessions/b")
        let session = root.appendingPathComponent("current")
        let foreign = home.appendingPathComponent("foreign/current")
        try fm.createDirectory(at: session, withIntermediateDirectories: true)
        try fm.createDirectory(at: foreign, withIntermediateDirectories: true)
        let alias = home.appendingPathComponent("alias")
        try fm.createSymbolicLink(at: alias, withDestinationURL: home.appendingPathComponent("real"))
        let aliasedSession = alias.appendingPathComponent("sessions/b/current")
        #expect(try PreparedWindowsExitCapture.evidenceDestination(sessionDirectory: aliasedSession, root: root).path == session.resolvingSymlinksInPath().path)
        #expect(try PreparedWindowsExitCapture.evidenceDestination(sessionDirectory: session, root: alias.appendingPathComponent("sessions/b")).path == session.resolvingSymlinksInPath().path)
        #expect(throws: (any Error).self) { try PreparedWindowsExitCapture.evidenceDestination(sessionDirectory: foreign, root: root) }
        let linked = root.appendingPathComponent("linked")
        try fm.createSymbolicLink(at: linked, withDestinationURL: foreign)
        #expect(throws: (any Error).self) { try PreparedWindowsExitCapture.evidenceDestination(sessionDirectory: linked, root: root) }
    }
}
