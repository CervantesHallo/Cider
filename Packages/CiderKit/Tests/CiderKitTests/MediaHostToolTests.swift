import CiderCore
import CiderStore
import Darwin
import Foundation
import Testing

private final class MediaHostToolBox<Value>: @unchecked Sendable {
    private let lock = NSLock()
    private var value: Value
    init(_ value: Value) { self.value = value }
    func access<T>(_ body: (inout Value) -> T) -> T {
        lock.lock(); defer { lock.unlock() }
        return body(&value)
    }
}

private enum MediaHostStop: Sendable, Equatable { case timeout, cancellation, outputLimit }

@Suite(.serialized) struct MediaHostToolTests {
    private func temporaryDirectory() throws -> URL {
        try FileSafety.reserveDirectory(in: FileManager.default.temporaryDirectory,
                                        name: { "media-host-" + UUID().uuidString })
    }

    private func eventually(_ seconds: TimeInterval, _ predicate: () -> Bool) -> Bool {
        let deadline = ProcessInfo.processInfo.systemUptime + seconds
        repeat {
            if predicate() { return true }
            Thread.sleep(forTimeInterval: 0.01)
        } while ProcessInfo.processInfo.systemUptime < deadline
        return predicate()
    }

    @Test func explicitEnvironmentDoesNotInheritHOMEOrUnlistedVariables() throws {
        let output = try Command.runCancellable("/usr/bin/env", [],
            environment: ["CIDER_MEDIA_TEST_ONLY": "isolated"], timeout: 2,
            outputLimit: 4096, retainUntilReaped: true)
        // Report keys only: inherited values could contain credentials on a failing implementation.
        let keys = Set(output.split(separator: "\n").map { String($0.prefix { $0 != "=" }) })
        let sentinelPresent = output.split(separator: "\n").contains("CIDER_MEDIA_TEST_ONLY=isolated")
        #expect(sentinelPresent)
        #expect(!keys.contains("HOME"))
        #expect(keys == Set(["CIDER_MEDIA_TEST_ONLY"]))
    }

    @Test func timeoutKillsAndReapsTERMIgnoringExecSleep() throws {
        try checkStoppedTool(.timeout)
    }

    @Test func cancellationKillsAndReapsTERMIgnoringExecSleep() throws {
        try checkStoppedTool(.cancellation)
    }

    @Test func outputLimitStopsTinyPrintfAndReapsExecSleep() throws {
        try checkStoppedTool(.outputLimit)
    }

    private func checkStoppedTool(_ reason: MediaHostStop) throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let pidFile = root.appendingPathComponent("owned pid 'file'")
        let cancelled = MediaHostToolBox(false)
        let result = MediaHostToolBox<Result<Void, Error>?>(nil)
        let done = DispatchSemaphore(value: 0)
        // Builtins followed by exec create exactly one child; even a broken stop has a finite lifetime.
        let script = #"""
        trap '' TERM
        printf '%s\n' "$$" > "$CIDER_TEST_PID_FILE"
        if [ "$CIDER_TEST_OUTPUT" = 1 ]; then printf '0123456789abcdef'; fi
        exec /bin/sleep 12
        """#
        let started = ProcessInfo.processInfo.systemUptime
        DispatchQueue.global().async {
            let outcome = Result<Void, Error> {
                _ = try Command.runCancellable("/bin/sh", ["-c", script],
                    environment: ["CIDER_TEST_PID_FILE": pidFile.path,
                                  "CIDER_TEST_OUTPUT": reason == .outputLimit ? "1" : "0"],
                    timeout: reason == .timeout ? 2 : 6,
                    outputLimit: reason == .outputLimit ? 8 : 1024,
                    retainUntilReaped: true, cancellationRequested: { cancelled.access { $0 } })
            }
            result.access { $0 = outcome }
            done.signal()
        }
        var joined = false
        defer {
            cancelled.access { $0 = true }
            if !joined { #expect(done.wait(timeout: .now() + 15) == .success) }
        }
        var pid: pid_t = 0
        let observedChild = eventually(3) {
            guard let text = try? String(contentsOf: pidFile, encoding: .utf8),
                  let candidate = pid_t(text.trimmingCharacters(in: .whitespacesAndNewlines)),
                  candidate > 0 else { return false }
            // Output may already be reaped when this thread is scheduled; the PID file survives it.
            if reason == .outputLimit { pid = candidate; return true }
            var path = [CChar](repeating: 0, count: 4096)
            guard proc_pidpath(candidate, &path, UInt32(path.count)) > 0 else { return false }
            let executable = String(decoding: path.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            guard executable == "/bin/sleep" else { return false }
            pid = candidate
            return true
        }
        try #require(observedChild)
        if reason == .cancellation { cancelled.access { $0 = true } }
        try #require(done.wait(timeout: .now() + 6) == .success)
        joined = true
        let elapsed = ProcessInfo.processInfo.systemUptime - started
        // TERM is inherited as ignored across exec: returning before sleep ends requires escalation.
        #expect(elapsed >= 1)
        #expect(elapsed < 6)
        var status: Int32 = 0
        let waited = waitpid(pid, &status, WNOHANG), waitError = errno
        #expect(waited == -1)
        #expect(waitError == ECHILD)
        let alive = kill(pid, 0), probeError = errno
        #expect(alive == -1)
        #expect(probeError == ESRCH)
        if waited != -1 || waitError != ECHILD {
            // Failure cleanup only waits/reaps this finite child; it never signals a potentially reused PID.
            #expect(eventually(13) {
                let rc = waitpid(pid, &status, WNOHANG)
                return rc == pid || (rc == -1 && errno == ECHILD)
            })
        }
        let error = try #require(result.access { outcome -> Error? in
            if case .failure(let error)? = outcome { return error }
            return nil
        })
        if reason == .cancellation {
            #expect(error is CancellationError)
        } else if case CiderError.invalid(let message) = error {
            #expect(message == (reason == .timeout ? "本机组件准备超时。" : "本机组件输出超过限制。"))
        } else {
            Issue.record("Expected a host-tool limit failure")
        }
    }

    @Test func localContendedFileOperationLockCancelsBeforeBodyAndJoinsOwner() throws {
        let root = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: root) }
        let url = root.appendingPathComponent("operation.lock")
        let entered = DispatchSemaphore(value: 0), release = DispatchSemaphore(value: 0)
        let waiting = DispatchSemaphore(value: 0), ownerDone = DispatchSemaphore(value: 0)
        let waiterDone = DispatchSemaphore(value: 0)
        let state = MediaHostToolBox((cancelled: false, checks: 0, bodyEntered: false))
        let owner = MediaHostToolBox<Result<Void, Error>?>(nil)
        let waiter = MediaHostToolBox<Result<Void, Error>?>(nil)
        DispatchQueue.global().async {
            let outcome = Result<Void, Error> {
                try FileOperationLock.withLock(at: url, timeout: 3) {
                    entered.signal()
                    guard release.wait(timeout: .now() + 8) == .success else {
                        throw CiderError.invalid("test owner release timed out")
                    }
                }
            }
            owner.access { $0 = outcome }
            ownerDone.signal()
        }
        var ownerJoined = false, waiterStarted = false, waiterJoined = false
        defer {
            state.access { $0.cancelled = true }
            release.signal()
            if waiterStarted && !waiterJoined { #expect(waiterDone.wait(timeout: .now() + 6) == .success) }
            if !ownerJoined { #expect(ownerDone.wait(timeout: .now() + 10) == .success) }
        }
        try #require(entered.wait(timeout: .now() + 2) == .success)
        waiterStarted = true
        DispatchQueue.global().async {
            let outcome = Result<Void, Error> {
                try FileOperationLock.withLock(at: url, timeout: 5, checking: {
                    let snapshot = state.access { value in
                        value.checks += 1
                        return (value.checks, value.cancelled)
                    }
                    // Check 1 precedes local.try(); check 2 proves contention while the owner holds it.
                    if snapshot.0 == 2 { waiting.signal() }
                    if snapshot.1 { throw CancellationError() }
                }) {
                    state.access { $0.bodyEntered = true }
                }
            }
            waiter.access { $0 = outcome }
            waiterDone.signal()
        }
        try #require(waiting.wait(timeout: .now() + 2) == .success)
        state.access { $0.cancelled = true }
        try #require(waiterDone.wait(timeout: .now() + 2) == .success)
        waiterJoined = true
        let wasCancelled = waiter.access { outcome in
            if case .failure(let error)? = outcome { return error is CancellationError }
            return false
        }
        #expect(wasCancelled)
        #expect(!state.access { $0.bodyEntered })
        release.signal()
        try #require(ownerDone.wait(timeout: .now() + 2) == .success)
        ownerJoined = true
        let ownerSucceeded = owner.access { outcome in
            if case .success? = outcome { return true }
            return false
        }
        #expect(ownerSucceeded)
        #expect(!state.access { $0.bodyEntered })
    }
}
