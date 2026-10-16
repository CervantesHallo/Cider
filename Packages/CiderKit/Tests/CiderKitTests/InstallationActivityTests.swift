import CiderCore
import CiderRuntime
import CiderSchema
import CiderStore
import Foundation
import Testing
@testable import CiderBottle
@testable import CiderIntegration

private final class ActivityResult: @unchecked Sendable {
    private let lock = NSLock()
    private var value: Swift.Result<Void, Error>?
    private let ready = DispatchSemaphore(value: 0)
    func set(_ result: Swift.Result<Void, Error>) { lock.lock(); value = result; lock.unlock(); ready.signal() }
    func wait() -> Bool { ready.wait(timeout: .now() + 2) == .success }
    var finished: Bool { lock.lock(); defer { lock.unlock() }; return value != nil }
    var cancelled: Bool {
        lock.lock(); defer { lock.unlock() }
        if case .failure(let error) = value { return error is CancellationError }
        return false
    }
    var succeeded: Bool { lock.lock(); defer { lock.unlock() }; if case .success = value { return true }; return false }
}

private final class CompletionError: @unchecked Sendable {
    private let lock = NSLock()
    private var error: Error?
    func set(_ error: Error?) { lock.lock(); self.error = error; lock.unlock() }
    var cancelled: Bool { lock.lock(); defer { lock.unlock() }; return error is CancellationError }
}

@Suite struct InstallationActivityTests {
    private func bottle(_ root: URL, _ subdirectory: String) throws -> Bottle {
        let config = BottleConfig(id: "same-id", name: "owned", createdBy: "tests", createdAt: "owned",
                                  template: .win10_64, engine: .init(id: "none"), locale: .japanese)
        let directory = root.appendingPathComponent(subdirectory)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return Bottle(config: config, directory: directory)
    }
    private func start(_ bottle: Bottle, failure: Bool = false) throws -> ActivityResult {
        let entered = DispatchSemaphore(value: 0), result = ActivityResult()
        DispatchQueue.global().async {
            result.set(Swift.Result {
                try InstallationActivity.perform(in: bottle, onStop: {}) { () throws -> Void in
                    entered.signal()
                    while !InstallationActivity.isCancelled { Thread.sleep(forTimeInterval: 0.01) }
                    if failure { throw CiderError.invalid("owned cleanup failure") }
                    try InstallationActivity.checkCancellation()
                }
            })
        }
        guard entered.wait(timeout: .now() + 2) == .success else { throw CiderError.invalid("test work did not start") }
        return result
    }

    @Test func stopDrainsAllMatchingWorkAndPreservesOtherRoots() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let a = try bottle(root, "a"), b = try bottle(root, "b")
        let first = try start(a), second = try start(a), foreign = try start(b)
        #expect(try RecipeInstaller.stoppingInstallations(in: a, timeout: 2) { 17 } == 17)
        #expect(first.wait() && second.wait())
        #expect(first.finished && second.finished && first.cancelled && second.cancelled)
        #expect(!foreign.finished)
        try RecipeInstaller.stoppingInstallations(in: b, timeout: 2) {}
        #expect(foreign.wait())
        #expect(foreign.cancelled)
        #expect(try InstallationActivity.perform(in: a, onStop: {}) { 7 } == 7)
    }

    @Test func cleanupErrorIsNotConvertedIntoStoppedSuccess() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let target = try bottle(root, "a")
        let work = try start(target, failure: true)
        #expect(throws: CiderError.self) { try RecipeInstaller.stoppingInstallations(in: target, timeout: 2) {} }
        #expect(work.wait())
        #expect(work.finished && !work.cancelled)
    }

    @Test func stopBarrierAndUnfinishedCancellationRejectNewWork() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let target = try bottle(root, "a"), entered = DispatchSemaphore(value: 0), release = DispatchSemaphore(value: 0)
        let done = DispatchSemaphore(value: 0)
        DispatchQueue.global().async {
            _ = try? InstallationActivity.perform(in: target, onStop: {}) {
                entered.signal(); _ = release.wait(timeout: .now() + 5)
            }
            done.signal()
        }
        #expect(entered.wait(timeout: .now() + 2) == .success)
        var rejectedDuringStop = false
        #expect(throws: CiderError.self) {
            try RecipeInstaller.stoppingInstallations(in: target, timeout: 0.05) {
                do { try InstallationActivity.perform(in: target, onStop: {}) {} }
                catch { rejectedDuringStop = true }
            }
        }
        #expect(rejectedDuringStop)
        #expect(throws: CiderError.self) { try InstallationActivity.perform(in: target, onStop: {}) {} }
        release.signal()
        #expect(done.wait(timeout: .now() + 2) == .success)
        #expect(try InstallationActivity.perform(in: target, onStop: {}) { true })
    }

    @Test func selfJoinIsRejectedAndScopeAliasesMatch() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let target = try bottle(root, "a")
        let alias = root.appendingPathComponent("alias")
        try FileManager.default.createSymbolicLink(at: alias, withDestinationURL: target.directory)
        let same = Bottle(config: target.config, directory: alias)
        #expect(InstallationActivity.scope(target) == InstallationActivity.scope(same))
        #expect(throws: CiderError.self) {
            try InstallationActivity.perform(in: target, onStop: {}) { try RecipeInstaller.stoppingInstallations(in: target) {} }
        }
    }

    @Test func hostToolChecksCooperativeCancellation() throws {
        let observation = CommandObservation()
        DispatchQueue.global().asyncAfter(deadline: .now() + 0.05) { observation.detach() }
        let start = ProcessInfo.processInfo.systemUptime
        #expect(throws: CancellationError.self) {
            try Command.runCancellable("/bin/sleep", ["5"], cancellationRequested: { observation.isDetached })
        }
        #expect(ProcessInfo.processInfo.systemUptime - start < 2)
    }

    @Test func cancellationAndSuccessHaveOneAtomicWinner() {
        for index in 0..<100 {
            let ticket = InstallationActivity.Ticket(scope: "owned-\(index)", onStop: {})
            let completion = CompletionError()
            DispatchQueue.concurrentPerform(iterations: 2) { operation in
                if operation == 0 { ticket.cancel() }
                else { completion.set(ticket.finish(error: nil)) }
            }
            #expect(completion.cancelled == ticket.isCancelled)
        }
    }

    @Test func ownershipSurvivesInnerInstallAndRefreshHandoff() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let target = try bottle(root, "a"), activity = try RecipeInstaller.beginActivity(in: target)
        let refreshEntered = DispatchSemaphore(value: 0), resumeRefresh = DispatchSemaphore(value: 0)
        let nativeStopEntered = DispatchSemaphore(value: 0), lateLaunch = CommandObservation()
        let workflow = ActivityResult(), control = ActivityResult()
        DispatchQueue.global().async {
            let result = Swift.Result {
                try InstallationActivity.perform(in: target, onStop: {}) {}
                refreshEntered.signal(); _ = resumeRefresh.wait(timeout: .now() + 5)
                try activity.checkCancellation()
                lateLaunch.detach()
                try activity.finish()
            }
            if case .failure(let error) = result { try? activity.finish(error: error) }
            workflow.set(result)
        }
        #expect(refreshEntered.wait(timeout: .now() + 2) == .success)
        DispatchQueue.global().async {
            control.set(Swift.Result {
                try RecipeInstaller.stoppingInstallations(in: target, timeout: 2) { _ = nativeStopEntered.signal() }
            })
        }
        #expect(nativeStopEntered.wait(timeout: .now() + 2) == .success)
        resumeRefresh.signal()
        #expect(workflow.wait() && control.wait())
        #expect(workflow.cancelled && control.succeeded && !lateLaunch.isDetached)
    }

    @Test func stopBeforeInnerRegistrationIsCancellationRatherThanCleanupFailure() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let target = try bottle(root, "a"), activity = try RecipeInstaller.beginActivity(in: target)
        let stopEntered = DispatchSemaphore(value: 0), control = ActivityResult()
        DispatchQueue.global().async {
            control.set(Swift.Result {
                try RecipeInstaller.stoppingInstallations(in: target, timeout: 2) { _ = stopEntered.signal() }
            })
        }
        #expect(stopEntered.wait(timeout: .now() + 2) == .success)
        do {
            _ = try activity.performing {
                try InstallationActivity.perform(in: target, onStop: {}) { Issue.record("cancelled inner work started") }
            }
            Issue.record("cancelled inner registration succeeded")
        } catch {
            #expect(error is CancellationError)
            try? activity.finish(error: error)
        }
        #expect(control.wait() && control.succeeded)
    }

    @Test func completedOwnershipCannotBeReusedForAnotherLaunch() throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        let target = try bottle(root, "a"), activity = try RecipeInstaller.beginActivity(in: target)
        try activity.finish()
        #expect(throws: CiderError.self) { try activity.checkCancellation() }
        #expect(throws: CiderError.self) { try activity.performing {} }
    }
}
