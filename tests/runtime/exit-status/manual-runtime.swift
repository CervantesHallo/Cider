import CiderBottle
import CiderCore
import CiderIntegration
import CiderRuntime
import Foundation

private final class Pending: @unchecked Sendable {
    private let lock = NSLock()
    private var value: Error?
    let done = DispatchSemaphore(value: 0)
    func complete(_ error: Error?) { lock.lock(); value = error; lock.unlock(); done.signal() }
    var error: Error? { lock.lock(); defer { lock.unlock() }; return value }
}

@main struct OwnedManualAcceptance {
    static func require(_ condition: Bool, _ message: String) throws {
        if !condition { throw CiderError.invalid(message) }
    }

    static func main() throws {
        let base = URL(fileURLWithPath: CommandLine.arguments[1]).standardizedFileURL
        let owner = try JSONSerialization.jsonObject(with: Data(contentsOf: base.appendingPathComponent("owner.json"))) as! [String: Any]
        try require(base.lastPathComponent.hasPrefix("runtime-acceptance-") && owner["root"] as? String == base.path, "wrong owned root")
        let home = base.appendingPathComponent("private-home")
        try require(owner["home"] as? String == home.path, "foreign home")
        let paths = CiderPaths.standard(environment: ["CIDER_HOME": home.path])
        let store = BottleStore(paths: paths), bottle = try store.bottle("Runtime Acceptance")
        let tools = try WindowsExitCollector.bundled(resources: base.appendingPathComponent("Cider.app/Contents/Resources"))
        let installer = ManualInstaller(store: store, collector: tools)
        let fixtures = base.appendingPathComponent("exit-status/manual 中文 with spaces")
        var observations: [[String: Any]] = []
        for (name, expected): (String, UInt32) in [("zero64.exe", 0), ("word256.exe", 256), ("high32.exe", 0x80000100)] {
            let activity = try RecipeInstaller.beginActivity(in: bottle)
            let running = try installer.start(fixtures.appendingPathComponent(name), in: bottle, activity: activity)
            var succeeded = false, detail = ""
            do { let result = try running.wait(); try activity.finish(); succeeded = result.windowsExitCode == 0 }
            catch { detail = String(describing: error); try? activity.finish(error: error) }
            let report = try JSONSerialization.jsonObject(with: Data(contentsOf: running.session.directory.appendingPathComponent("exit-capture-result.json"))) as! [String: Any]
            try require((report["windows_exit_code"] as? NSNumber)?.uint32Value == expected && succeeded == (expected == 0), "manual exit status alias")
            observations.append(["case":name,"captured":report,"succeeded":succeeded,"error":detail])
        }

        // A new self-authored MSI has no interactive UI sequence. Exercise the
        // actual manual /i path; recipe /qn results alone do not cover this path.
        let package = fixtures.appendingPathComponent("manual-" + UUID().uuidString + ".msi")
        let runner = try store.runner(for: bottle)
        let made = try runner.runToCompletion(runner.plan(program: base.appendingPathComponent("exit-status/make-msi.exe").path,
            arguments: [WinePathMapping.windowsPath(forHostPath: package.path, prefix: bottle.prefix), "success",
                "Software\\CiderExitFixture\\" + UUID().uuidString], label: "owned-manual-msi-build"))
        try require(made.code == 0, "owned MSI build failed")
        let msiActivity = try RecipeInstaller.beginActivity(in: bottle)
        let msi = try installer.start(package, in: bottle, activity: msiActivity)
        let msiResult = try msi.wait()
        try msiActivity.finish()
        try require(msiResult.windowsExitCode == 0 && !msiResult.requiresRestart, "manual MSI outcome incorrect")
        observations.append(["case":"manual-msi-default-i","windowsExitCode":msiResult.windowsExitCode,"requiresRestart":msiResult.requiresRestart])

        let activity = try RecipeInstaller.beginActivity(in: bottle)
        let running = try installer.start(fixtures.appendingPathComponent("sleep.exe"), in: bottle, activity: activity)
        let pending = Pending()
        DispatchQueue.global().async {
            do { _ = try running.wait(); try activity.finish(); pending.complete(nil) }
            catch { try? activity.finish(error: error); pending.complete(error) }
        }
        let until = Date().addingTimeInterval(10)
        while Date() < until {
            if (try? String(contentsOf: running.session.log, encoding: .utf8))?.contains("owned-manual-installer-started") == true { break }
            Thread.sleep(forTimeInterval: 0.05)
        }
        try require((try String(contentsOf: running.session.log, encoding: .utf8)).contains("owned-manual-installer-started"), "manual child did not start")
        try RecipeInstaller.stoppingInstallations(in: bottle) { try store.runner(for: bottle).killAll() }
        try require(pending.done.wait(timeout: .now() + 2) == .success && pending.error is CancellationError, "manual Stop did not drain cancelled worker")
        try require(!RecipeInstaller.hasActiveActivity(in: bottle), "manual activity remains")
        observations.append(["case":"stop-running-manual","cancelled":true,"workerDrained":true])
        let report: [String: Any] = ["schema":"cider.manual-installer-acceptance/v1","passed":true,"cases":observations,
            "scope":"actual CiderKit service and Wine; not GUI/TCC, native Windows or game evidence"]
        let data = try JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted,.sortedKeys])
        try data.write(to: base.appendingPathComponent("exit-status/manual-result.json"))
    }
}
