import Foundation
import Testing
import CiderData
@testable import CiderRuntime

@Suite struct ProcessScopeTests {
    @Test func collidingBottleIDsDoNotCrossDataRoots() {
        let root = URL(fileURLWithPath: "/tmp/cider-scope-owned/Bottles/shared/prefix")
        let foreign = "/tmp/cider-scope-other/Bottles/shared/prefix"
        let owned = WineProcess(pid: 101, bottleID: "shared", windowsImage: "C:\\app.exe", startTime: 1, auditIdentity: nil, prefixPath: root.path)
        let other = WineProcess(pid: 102, bottleID: "shared", windowsImage: "C:\\YuanShen.exe", startTime: 1, auditIdentity: nil, prefixPath: foreign)
        let unknown = WineProcess(pid: 103, bottleID: "shared", windowsImage: "C:\\app.exe", startTime: 1)
        let wrongID = WineProcess(pid: 104, bottleID: "other", windowsImage: "C:\\app.exe", startTime: 1, auditIdentity: nil, prefixPath: root.path)
        #expect(ProcessScanner.scoped([owned, other, unknown, wrongID], prefixes: ["shared": root]) == [owned])
        #expect(ProcessScanner.scoped([owned], prefixes: [:]).isEmpty)
    }

    @Test func exactPrefixAndPathAliases() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent("cider-scope-" + UUID().uuidString)
        try fm.createDirectory(at: root.appendingPathComponent("prefix"), withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let alias = root.appendingPathComponent("alias")
        try fm.createSymbolicLink(at: alias, withDestinationURL: root.appendingPathComponent("prefix"))
        let hit = WineProcess(pid: 101, bottleID: "b", windowsImage: "C:\\app.exe", startTime: 1, auditIdentity: nil, prefixPath: alias.path)
        let descendant = WineProcess(pid: 102, bottleID: "b", windowsImage: "C:\\app.exe", startTime: 1, auditIdentity: nil, prefixPath: alias.appendingPathComponent("child").path)
        #expect(ProcessScanner.scoped([hit, descendant], prefixes: ["b": root.appendingPathComponent("prefix")]) == [hit])
    }

    @Test func currentBottleGateRemainsEffective() {
        let prefix = URL(fileURLWithPath: "/tmp/cider-scope-owned/prefix")
        let gated = WineProcess(pid: 101, bottleID: "b", windowsImage: "C:\\YuanShen.exe", startTime: 1, auditIdentity: nil, prefixPath: prefix.path)
        let current = ProcessScanner.scoped([gated], prefixes: ["b": prefix])
        #expect(current.count == 1)
        #expect(current.first.flatMap { Preflight.check(program: $0.windowsImage, db: nil) } != nil)
    }
    @Test func configuredSymlinkCannotAdoptForeignPrefix() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent("cider-scope-symlink-" + UUID().uuidString)
        try fm.createDirectory(at: root.appendingPathComponent("foreign"), withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let configured = root.appendingPathComponent("prefix")
        try fm.createSymbolicLink(at: configured, withDestinationURL: root.appendingPathComponent("foreign"))
        let foreign = WineProcess(pid: 101, bottleID: "b", windowsImage: "C:\\app.exe", startTime: 1, auditIdentity: nil,
                                  prefixPath: root.appendingPathComponent("foreign").path)
        #expect(ProcessScanner.scoped([foreign], prefixes: ["b": configured]).isEmpty)
    }

    @Test func serverControlClientsAreNotOrphanTargets() {
        for option in ["-w", "--wait", "-k", "-k9", "--kill=9"] {
            #expect(ProcessScanner.isServerControlClient(.init(executable: "/engine/wineserver", argv: ["/engine/wineserver", option], environment: [:])))
            #expect(!ProcessScanner.isServerControlClient(.init(executable: "/engine/wine", argv: ["C:\\app.exe", option], environment: [:])))
        }
        #expect(!ProcessScanner.isServerControlClient(.init(executable: "/engine/wineserver", argv: ["wineserver", "-f", "-p0"], environment: [:])))
    }

    @Test func finalExitObservationDoesNotRetainStalePendingButKeepsUnknown() {
        let delayed = WineProcess(pid: 101, bottleID: "b", windowsImage: "C:\\app.exe", startTime: 1)
        let unknown = WineProcess(pid: 102, bottleID: "b", windowsImage: "C:\\app.exe", startTime: 1)
        let pending = [delayed, unknown]
        #expect(ProcessScanner.unresolved(pending, state: { $0.pid == 101 ? .exited : .unknown }) == [unknown])
        #expect(ProcessScanner.unresolved([delayed], state: { _ in .exited }).isEmpty)
        #expect(ProcessScanner.unresolved([delayed], state: { _ in .running }) == [delayed])
    }

    @Test func refreshedKernelIdentityAllowsSameBirthExecButRejectsOwnershipChanges() {
        let process = WineProcess(pid: 101, bottleID: "b", windowsImage: "C:\\app.exe", startTime: 1,
                                  auditIdentity: [0, 0, 0, 0, 0, 101, 0, 1], prefixPath: "/tmp/owned-prefix")
        let arguments = ProcessScanner.Arguments(executable: "/engine/wine", argv: ["C:\\app.exe"],
                                                 environment: ["CIDER_BOTTLE": "b", "WINEPREFIX": "/tmp/owned-prefix"])
        let current: [UInt32] = [0, 0, 0, 0, 0, 101, 0, 2]
        #expect(ProcessScanner.refreshedAuditIdentity(process, state: { _ in .running }, arguments: { _ in arguments }, audit: { _ in current }) == current)
        #expect(ProcessScanner.refreshedAuditIdentity(process, state: { _ in .exited }, arguments: { _ in arguments }, audit: { _ in current }) == nil)
        #expect(ProcessScanner.refreshedAuditIdentity(process, state: { _ in .unknown }, arguments: { _ in arguments }, audit: { _ in current }) == nil)
        let foreign = ProcessScanner.Arguments(executable: "/engine/wine", argv: [], environment: ["CIDER_BOTTLE": "b", "WINEPREFIX": "/tmp/foreign"])
        #expect(ProcessScanner.refreshedAuditIdentity(process, state: { _ in .running }, arguments: { _ in foreign }, audit: { _ in current }) == nil)
        #expect(ProcessScanner.refreshedAuditIdentity(process, state: { _ in .running }, arguments: { _ in arguments }, audit: { _ in nil }) == nil)
        let wrongPID: [UInt32] = [0, 0, 0, 0, 0, 102, 0, 2]
        #expect(ProcessScanner.refreshedAuditIdentity(process, state: { _ in .running }, arguments: { _ in arguments }, audit: { _ in wrongPID }) == nil)
    }

    @Test func kernelTokenPrecedesOwnershipReadAcrossExec() {
        let process = WineProcess(pid: 101, bottleID: "b", windowsImage: "loader", startTime: 1, auditIdentity: nil,
                                  prefixPath: "/tmp/prefix")
        var order: [String] = []
        var generation: UInt32 = 1
        let token = ProcessScanner.refreshedAuditIdentity(process, state: { _ in .running }, arguments: { _ in
            order.append("arguments"); generation = 2
            return .init(executable: "/engine/wine", argv: [], environment: ["CIDER_BOTTLE": "b", "WINEPREFIX": "/tmp/prefix"])
        }, audit: { _ in
            order.append("token"); return [0, 0, 0, 0, 0, 101, 0, generation]
        })
        #expect(order == ["token", "arguments"])
        #expect(token?[7] == 1)
        #expect(token?[7] != generation) // Kernel rejection follows an intervening exec.
    }

}
