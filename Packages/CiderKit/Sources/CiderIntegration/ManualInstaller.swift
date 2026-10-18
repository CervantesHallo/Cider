import CiderBottle
import CiderCore
import CiderRuntime
import Foundation

/// A local EXE/MSI has the same exit-status and cancellation requirements as a
/// recipe. The caller retains ownership through refresh and the next UI action.
public struct ManualInstaller: Sendable {
    public let store: BottleStore
    private let collector: WindowsExitCollector?
    public init(store: BottleStore, collector: WindowsExitCollector? = nil) {
        self.store = store; self.collector = collector
    }

    public struct Outcome: Sendable {
        public let windowsExitCode: UInt32
        public let requiresRestart: Bool
    }

    public struct Running: Sendable {
        public let session: SessionHandle
        private let capture: PreparedWindowsExitCapture
        private let activity: RecipeInstaller.Activity
        private let isMSI: Bool
        private let title: String

        fileprivate init(session: SessionHandle, capture: PreparedWindowsExitCapture,
                         activity: RecipeInstaller.Activity, isMSI: Bool, title: String) {
            self.session = session; self.capture = capture; self.activity = activity
            self.isMSI = isMSI; self.title = title
        }

        public func wait() throws -> Outcome {
            // Always reap the collector, including Stop between launch and wait.
            let code = Spawn.wait(session.pid)
            do { try capture.recordEvidence(in: session) }
            catch { CiderDiagnostics.warning("安装状态记录未能加入会话诊断，原始记录保留在 \(capture.directory.path)：\(error)") }
            try activity.checkCancellation()
            let status = try capture.readStatus(afterCollectorUnixStatus: code)
            let restart = try RecipeInstaller.validateInstallerStatus(status, kind: isMSI ? "msi" : "exe",
                convention: nil, title: title, report: capture.resultURL)
            try activity.checkCancellation()
            guard let exit = status.capturedWindowsExitCode else { throw CiderError.invalid("安装退出状态未完整取得。") }
            return Outcome(windowsExitCode: exit, requiresRestart: restart)
        }
    }

    public func start(_ file: URL, in bottle: Bottle, activity: RecipeInstaller.Activity) throws -> Running {
        try activity.checkCancellation()
        let runner = try store.runner(for: bottle).checkingLaunches { try activity.checkCancellation() }
        let isMSI = file.pathExtension.lowercased() == "msi"
        let plan = isMSI
            ? runner.plan(program: "C:\\windows\\system32\\msiexec.exe", arguments: ["/i",
                WinePathMapping.windowsPath(forHostPath: file.path, prefix: bottle.prefix)],
                label: file.deletingPathExtension().lastPathComponent, cwd: file.deletingLastPathComponent())
            : runner.plan(program: file.path, label: file.deletingPathExtension().lastPathComponent, cwd: file.deletingLastPathComponent())
        let tool = try collector ?? WindowsExitCollector.bundled(cpuBackend: runner.engine.manifest.cpuBackend)
        let capture = try runner.prepareWindowsExitCapture(plan, collector: tool.executable,
            expectedSHA256: tool.sha256, timeoutMilliseconds: 3_600_000)
        let session = try runner.launch(capture.plan)
        return Running(session: session, capture: capture, activity: activity, isMSI: isMSI, title: file.lastPathComponent)
    }
}
