import Darwin
import Foundation
import Testing
import CiderStore
@testable import CiderIntegration
@testable import CiderRuntime

@Suite struct AppLifecycleTests {
    let process = WineProcess(pid: 101, bottleID: "test", windowsImage: #"C:\App\app.exe"#, startTime: 100)

    @Test func existingInstanceIsNotSpawnedAgain() throws {
        var launches = 0
        let result = try AppLifecycle.start(matching: { [process] }) { () throws -> Int in launches += 1; return 1 }
        #expect(result == nil)
        #expect(launches == 0)
    }

    @Test func stoppedInstanceIsStartedOnce() throws {
        var launches = 0
        let result = try AppLifecycle.start(matching: { [] }) { () throws -> Int in launches += 1; return 7 }
        #expect(result == 7)
        #expect(launches == 1)
    }

    @Test func failureToStopPreventsRestart() {
        var launches = 0
        #expect(throws: AppLifecycle.Failure.self) {
            try AppLifecycle.drain(matching: { [process] }, terminate: { $0 }, pause: {})
            _ = AppLifecycle.start(matching: { [] }) { launches += 1 }
        }
        #expect(launches == 0)
    }

    @Test func helperReplacementIsRescanned() throws {
        let helper = WineProcess(pid: 102, bottleID: "test", windowsImage: #"C:\App\helper.exe"#, startTime: 200)
        var active = [process]
        var stopped: [pid_t] = []
        try AppLifecycle.drain(matching: { active }, terminate: { targets in
            stopped += targets.map(\.pid)
            active = targets.contains(process) ? [helper] : []
            return []
        }, pause: {})
        #expect(stopped == [101, 102])
    }

    @Test func constantRespawnHasBoundedFailure() {
        var stops = 0
        #expect(throws: AppLifecycle.Failure.self) {
            try AppLifecycle.drain(matching: { [process] }, terminate: { _ in stops += 1; return [] }, pause: {})
        }
        #expect(stops == 3)
    }
}

@Suite struct RecipeCommandCancellationTests {
    @Test func cancellationStopsOwnedDownloadProcess() async throws {
        let start = ProcessInfo.processInfo.systemUptime
        let worker = Task.detached { try Command.runCancellable("/bin/sleep", ["30"]) }
        try await Task.sleep(for: .milliseconds(200))
        worker.cancel()
        do {
            _ = try await worker.value
            Issue.record("Cancelled command unexpectedly succeeded")
        } catch is CancellationError {
            #expect(ProcessInfo.processInfo.systemUptime - start < 5)
        }
    }

    @Test func normalCommandReturnsOutput() throws {
        #expect(try Command.runCancellable("/usr/bin/printf", ["recipe-output"]) == "recipe-output")
    }

    @Test func cancellationForcesExitWhenTermIsIgnored() async throws {
        let marker = FileManager.default.temporaryDirectory.appendingPathComponent("cider-cancel-ready-\(UUID().uuidString)")
        // exec preserves SIG_IGN. This owned sleep process has no subprocesses and no bottle access.
        let worker = Task.detached {
            try Command.runCancellable("/bin/sh", ["-c", "trap '' TERM; printf ready > \"$1\"; exec /bin/sleep 30",
                                                    "cider-cancel-check", marker.path])
        }
        defer { worker.cancel(); try? FileManager.default.removeItem(at: marker) }
        let readyDeadline = ProcessInfo.processInfo.systemUptime + 3
        while !FileManager.default.fileExists(atPath: marker.path), ProcessInfo.processInfo.systemUptime < readyDeadline {
            try await Task.sleep(for: .milliseconds(25))
        }
        try #require(FileManager.default.fileExists(atPath: marker.path))
        let start = ProcessInfo.processInfo.systemUptime
        worker.cancel()
        do {
            _ = try await worker.value
            Issue.record("Signal-ignoring command unexpectedly succeeded")
        } catch is CancellationError {
            let elapsed = ProcessInfo.processInfo.systemUptime - start
            #expect(elapsed >= 0.9)
            #expect(elapsed < 5)
        }
    }
}

@Suite struct ProcessTerminationTests {
    let process = WineProcess(pid: 101, bottleID: "test", windowsImage: #"C:\App\app.exe"#, startTime: 100)

    @Test func reusedPIDIsNotSignalledAfterGracePeriod() {
        var current: ProcessScanner.State = .running
        var signals: [Int32] = []
        var time = 0.0
        let remaining = ProcessScanner.terminate([process], grace: 1, forceWait: 1,
            state: { _ in current }, signal: { _, signal in signals.append(signal) },
            now: { time }, pause: { time += 0.1; current = .exited })
        #expect(signals == [SIGTERM])
        #expect(remaining.isEmpty)
    }

    @Test func unknownIdentityIsNotSignalledAndNotDeclaredStopped() {
        var signals: [Int32] = []
        let remaining = ProcessScanner.terminate([process], grace: 0, forceWait: 0,
            state: { _ in .unknown }, signal: { _, signal in signals.append(signal) }, now: { 0 }, pause: {})
        #expect(signals.isEmpty)
        #expect(remaining == [process])
    }

    @Test func forcedExitIsObservedBeforeReturning() {
        var current: ProcessScanner.State = .running
        var signals: [Int32] = []
        var time = 0.0
        var forceSent = false
        let remaining = ProcessScanner.terminate([process], grace: 0, forceWait: 1,
            state: { _ in current }, signal: { _, signal in signals.append(signal); forceSent = signal == SIGKILL },
            now: { time }, pause: { time += 0.1; if forceSent { current = .exited } })
        #expect(signals == [SIGTERM, SIGKILL])
        #expect(remaining.isEmpty)
        #expect(time > 0)
    }

    @Test func unsuccessfulForcedExitIsReturned() {
        let remaining = ProcessScanner.terminate([process], grace: 0, forceWait: 0,
            state: { _ in .running }, signal: { _, _ in }, now: { 0 }, pause: {})
        #expect(remaining == [process])
    }

    @Test func kernelRejectsStaleAuditIdentity() throws {
        // An owned, ordinary host child; this check never touches a Wine bottle or user program.
        guard let handle = dlopen(nil, RTLD_LAZY) else { return }
        defer { dlclose(handle) }
        guard dlsym(handle, "proc_signal_with_audittoken") != nil else { return }
        let child = Process()
        child.executableURL = URL(fileURLWithPath: "/bin/sleep")
        child.arguments = ["30"]
        try child.run()
        defer { if child.isRunning { child.terminate(); child.waitUntilExit() } }
        let pid = child.processIdentifier
        var info = proc_bsdinfo()
        #expect(proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size)) == MemoryLayout<proc_bsdinfo>.size)
        var identity = try #require(ProcessScanner.auditIdentity(for: pid))
        let actual = WineProcess(pid: pid, bottleID: "test", windowsImage: "owned-host-child", startTime: ProcessScanner.startTime(info), auditIdentity: identity)
        #expect(ProcessScanner.signalCaptured(actual, SIGCONT) == 0)
        identity[7] &+= 1
        let stale = WineProcess(pid: pid, bottleID: "test", windowsImage: "owned-host-child", startTime: actual.startTime, auditIdentity: identity)
        #expect(ProcessScanner.signalCaptured(stale, SIGCONT) == ESRCH)
        #expect(child.isRunning)
    }

    @Test func realOwnedChildExitIsConfirmed() throws {
        let child = Process()
        child.executableURL = URL(fileURLWithPath: "/bin/sleep")
        child.arguments = ["30"]
        try child.run()
        defer { if child.isRunning { child.terminate(); child.waitUntilExit() } }
        let pid = child.processIdentifier
        var info = proc_bsdinfo()
        _ = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size))
        let target = WineProcess(pid: pid, bottleID: "test", windowsImage: "owned-host-child", startTime: ProcessScanner.startTime(info),
                                 auditIdentity: ProcessScanner.auditIdentity(for: pid))
        #expect(ProcessScanner.terminate([target], grace: 1).isEmpty)
        child.waitUntilExit()
        #expect(!child.isRunning)
    }
}
