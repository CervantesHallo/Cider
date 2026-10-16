import Foundation
import Testing
@testable import CiderRuntime

@Suite struct StopDrainTests {
    @Test func recapturesChildrenWhichStartAfterFirstServerStop() throws {
        let first = WineProcess(pid: 101, bottleID: "b", windowsImage: "loader", startTime: 1)
        let late = WineProcess(pid: 102, bottleID: "b", windowsImage: "C:\\child.exe", startTime: 2)
        var round = 0, terminatedRound = 0
        var clock = 0.0
        var stopped = Set<Int32>()
        let complete = try WineRunner.drainStop(budget: 2, requestStop: { round += 1 }, scan: {
            if round == 1 { return terminatedRound == 1 ? [late] : [first] }
            if round == 2 && terminatedRound < 2 { return [late] }
            return []
        }, terminate: { processes, _, _ in
            stopped.formUnion(processes.map(\.pid)); terminatedRound = round; return []
        }, unresolved: { $0 }, serverRunning: { false }, now: { clock }, pause: { clock += 0.1 })
        #expect(complete)
        #expect(stopped == [101, 102])
        #expect(round >= 2)
    }

    @Test func unresolvedCapturedIdentityNeverBecomesSuccessFromEmptyScan() throws {
        let unknown = WineProcess(pid: 101, bottleID: "b", windowsImage: "loader", startTime: 1)
        var round = 0
        var clock = 0.0
        let complete = try WineRunner.drainStop(budget: 0.3, requestStop: { round += 1 },
            scan: { round == 1 ? [unknown] : [] }, terminate: { processes, _, _ in processes },
            unresolved: { $0 }, serverRunning: { false }, now: { clock }, pause: { clock += 0.1 })
        #expect(!complete)
    }

    @Test func activeServerAndLateExitRequireFinalQuiescence() throws {
        let pending = WineProcess(pid: 101, bottleID: "b", windowsImage: "loader", startTime: 1)
        var round = 0
        var clock = 0.0
        let complete = try WineRunner.drainStop(budget: 1, requestStop: { round += 1 },
            scan: { round == 1 ? [pending] : [] }, terminate: { processes, _, _ in processes },
            unresolved: { clock >= 0.2 ? [] : $0 }, serverRunning: { round < 3 },
            now: { clock }, pause: { clock += 0.1 })
        #expect(complete)
        #expect(round >= 3)
    }
    @Test func lateSuccessfulStopGetsReadOnlyConfirmationPastSoftDeadline() throws {
        var clock = 0.0
        var requests = 0
        var scans = 0
        let complete = try WineRunner.drainStop(budget: 12, requestStop: { requests += 1; clock += 13 },
            scan: { scans += 1; return [] }, terminate: { processes, _, _ in processes }, unresolved: { $0 },
            serverRunning: { false }, now: { clock }, pause: { clock += 0.1 })
        #expect(complete)
        #expect(requests == 1)
        #expect(scans >= 3)
    }

}
