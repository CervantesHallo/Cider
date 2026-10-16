import CiderCore
import CiderRuntime
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
        var info = proc_bsdinfo()
        while ProcessInfo.processInfo.systemUptime < deadline {
            if proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size)) == 0,
               errno == ESRCH { break }
            usleep(50_000)
        }
        // Only read waitpid after the reaper has removed the child from the process table.
        #expect(proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size)) == 0)
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
}
