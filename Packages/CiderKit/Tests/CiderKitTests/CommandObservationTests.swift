import CiderCore
@testable import CiderRuntime
import Darwin
import Foundation
import Testing

@Suite(.serialized) struct CommandObservationTests {
    @Test func attachedWaitKeepsRealExitCode() throws {
        let log = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: log) }
        let pid = try Spawn.launch(executable: "/bin/sh", arguments: ["-c", "exit 37"],
                                   environment: ["PATH": "/usr/bin:/bin"], workingDirectory: nil, logPath: log.path)
        #expect(try Spawn.wait(pid, observation: CommandObservation()) == 37)
    }

    @Test func detachedWaitLeavesChildAliveAndReapsItsLaterExit() throws {
        let log = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: log) }
        let pid = try Spawn.launch(executable: "/bin/sleep", arguments: ["0.8"],
                                   environment: ["PATH": "/usr/bin:/bin"], workingDirectory: nil, logPath: log.path)
        let observation = CommandObservation()
        observation.detach()
        let start = ProcessInfo.processInfo.systemUptime
        #expect(throws: CommandObserverDetached.self) { try Spawn.wait(pid, observation: observation) }
        #expect(ProcessInfo.processInfo.systemUptime - start < 0.4)
        #expect(kill(pid, 0) == 0)
        let deadline = ProcessInfo.processInfo.systemUptime + 3
        while Spawn.isAwaitingDetachedReap(pid), ProcessInfo.processInfo.systemUptime < deadline {
            usleep(50_000)
        }
        #expect(!Spawn.isAwaitingDetachedReap(pid))
        // Wait for actual reaper ownership to finish before observing ECHILD;
        // proc_pidinfo can already hide a zombie that still belongs to waitpid.
        var status: Int32 = 0
        #expect(waitpid(pid, &status, WNOHANG) == -1)
        #expect(errno == ECHILD)
    }

    @Test func observationIsMonotonic() throws {
        let observation = CommandObservation()
        try observation.check()
        observation.detach(); observation.detach()
        #expect(observation.isDetached)
        #expect(throws: CommandObserverDetached.self) { try observation.check() }
    }

    @Test func detachListenersWakeOnceAndLateListenersObserveTheEnd() {
        let observation = CommandObservation(), notified = DispatchSemaphore(value: 0)
        let removed = observation.onDetach { Issue.record("removed listener ran") }
        observation.removeDetachListener(removed)
        observation.onDetach { notified.signal() }
        observation.detach(); observation.detach()
        #expect(notified.wait(timeout: .now()) == .success)
        #expect(notified.wait(timeout: .now()) == .timedOut)
        observation.onDetach { notified.signal() }
        #expect(notified.wait(timeout: .now()) == .success)
    }
}
