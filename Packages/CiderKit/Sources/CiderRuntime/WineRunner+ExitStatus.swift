import CiderCore
import CiderData
import CiderPE
import CryptoKit
import Darwin
import Foundation

/// An explicitly owned collector invocation. Preparing/reading does not execute Wine
/// or prove that installer descendants have stopped.
public struct PreparedWindowsExitCapture: Sendable {
    public let plan: LaunchPlan
    public let directory: URL
    public let resultURL: URL
    fileprivate let sessionRoot: URL

    /// Attach only bounded JSON to the real Wine session; the PE/scratch directory
    /// is separate and never competes with timestamped sessions in support bundles.
    public func recordEvidence(in session: SessionHandle) throws {
        let destination = try Self.evidenceDestination(sessionDirectory: session.directory, root: sessionRoot)
        for (name, target, limit) in [("request.json", "exit-capture-request.json", 16 << 10),
                                      ("result.json", "exit-capture-result.json", 4096)] {
            let from = try FileSafety.child(name, in: directory, rejectSymlinks: true)
            let to = try FileSafety.child(target, in: destination, rejectSymlinks: true)
            try CaptureFiles.writeNew(CaptureFiles.read(from, limit: limit), to: to)
        }
    }

    static func evidenceDestination(sessionDirectory: URL, root: URL) throws -> URL {
        let destination = try FileSafety.child(sessionDirectory.lastPathComponent, in: root, rejectSymlinks: true)
        guard destination.resolvingSymlinksInPath().path == sessionDirectory.standardizedFileURL.resolvingSymlinksInPath().path else {
            throw CiderError.invalid("退出状态记录与会话目录不属于同一瓶子。")
        }
        return destination
    }

    /// A zero Unix status is necessary for the collector, never sufficient for its child.
    public func readStatus(afterCollectorUnixStatus code: Int32) throws -> WindowsExitStatus {
        let decoded = Result { () throws -> WindowsExitStatus in
            let file = try FileSafety.child("result.json", in: directory, rejectSymlinks: true)
            return try WindowsExitStatus(data: CaptureFiles.read(file, limit: 4096))
        }
        guard code == 0 else {
            let detail: String
            if case .success(let status) = decoded {
                detail = "\(status.phase.rawValue)，Win32 \(status.win32Error)；记录：\(resultURL.path)"
            } else { detail = "没有完整状态记录。" }
            throw CiderError.commandFailed(command: "Windows exit-status collector", status: code,
                                           output: "采集工具未成功结束，不能据此判断目标程序退出码。\(detail)")
        }
        let report = try decoded.get()
        guard report.capturedWindowsExitCode != nil else {
            throw CiderError.invalid("没有取得完整 Windows 退出码（\(report.phase.rawValue)，Win32 \(report.win32Error)）。")
        }
        return report
    }
}

extension WineRunner {
    /// The caller supplies a known collector digest, performs
    /// a normal runner.launch(plan), observe completion, and handle installer detection.
    /// Scratch is deliberately retained; descendants' cleanup must not be inferred.
    public func prepareWindowsExitCapture(_ target: LaunchPlan, collector: URL,
                                          expectedSHA256: String, timeoutMilliseconds: UInt32) throws -> PreparedWindowsExitCapture {
        try Task.checkCancellation()
        guard target.bottleID == bottleID, target.engineID == engine.manifest.id,
              [engine.wine.path, engine.bin.appendingPathComponent("wine").path].contains(target.loader),
              let program = target.argv.first, !program.isEmpty,
              target.argv.allSatisfy({ !$0.contains("\0") }),
              captureEnvironmentIsValid(target.env),
              timeoutMilliseconds > 0, timeoutMilliseconds <= 3_600_000 else {
            throw CiderError.invalid("退出码采集目标、参数或超时无效。")
        }
        if let block = target.argv.lazy.compactMap({ Preflight.check(program: $0, db: nil) }).first { throw block }
        let digest = expectedSHA256.lowercased()
        guard digest.utf8.count == 64,
              digest.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }), collector.isFileURL else {
            throw CiderError.invalid("退出码采集工具的来源摘要无效。")
        }
        let bytes = try CaptureFiles.read(collector, limit: 2 << 20)
        let actual = SHA256.hash(data: bytes).map { String(format: "%02x", $0) }.joined()
        guard actual == digest, let header = PEHeader(data: bytes), header.isExecutableImage,
              !header.isDLL, header.subsystem == 2 || header.subsystem == 3 else {
            throw CiderError.invalid("退出码采集工具校验失败，或不是 Windows 用户程序 PE。")
        }

        let targetWindowsPath = try captureWindowsProgram(program)
        let cwd: URL?
        let windowsCwd: String
        if let path = target.cwd {
            guard path.hasPrefix("/"), !path.contains("\0") else { throw CiderError.invalid("采集工作目录必须是绝对宿主路径。") }
            cwd = URL(fileURLWithPath: path, isDirectory: true)
            windowsCwd = WinePathMapping.windowsPath(forHostPath: path, prefix: prefix)
        } else { cwd = nil; windowsCwd = "-" }
        let root = try FileSafety.child(try FileSafety.component(bottleID),
                                        in: paths.logs.appendingPathComponent("exit-captures"), rejectSymlinks: true)
        let directory = try FileSafety.reserveDirectory(in: root, name: { "capture-" + UUID().uuidString.lowercased() })
        let copiedCollector = try FileSafety.child("collector.exe", in: directory, rejectSymlinks: true)
        try CaptureFiles.writeNew(bytes, to: copiedCollector)
        let result = try FileSafety.child("result.json", in: directory, rejectSymlinks: true)
        guard !FileSafety.exists(result) else { throw CiderError.alreadyExists("退出码采集结果") }
        let resultWindowsPath = WinePathMapping.windowsPath(forHostPath: result.path, prefix: prefix)
        let plannedTarget = self.plan(program: program, arguments: Array(target.argv.dropFirst()), label: target.label,
                                      cwd: cwd, extraEnv: target.env)
        let plan = self.plan(program: copiedCollector.path,
                             arguments: [resultWindowsPath, String(timeoutMilliseconds), windowsCwd, targetWindowsPath]
                                + Array(target.argv.dropFirst()),
                             label: target.label + "-exit-status", cwd: cwd, extraEnv: plannedTarget.env)
        guard captureEnvironmentIsValid(plan.env) else {
            throw CiderError.invalid("采集环境包含无效的变量名或 NUL，不能忠实传递给目标程序。")
        }
        let metadata = CaptureMetadata(schema: "cider.exit-capture-request/v1", collectorSHA256: digest,
                                       collectorMachine: header.machine, targetProgram: targetWindowsPath,
                                       timeoutMilliseconds: timeoutMilliseconds,
                                       plannedEnvironmentProfile: plannedTarget.matchingEnvironmentProfile)
        try JSONFile.writeMetadata(metadata, to: directory.appendingPathComponent("request.json"), limit: 16 << 10)
        try Task.checkCancellation()
        return PreparedWindowsExitCapture(plan: plan, directory: directory, resultURL: result,
                                          sessionRoot: paths.sessions.appendingPathComponent(bottleID))
    }

    private func captureWindowsProgram(_ program: String) throws -> String {
        let mapped = program.hasPrefix("/") ? WinePathMapping.windowsPath(forHostPath: program, prefix: prefix) : program
        let characters = Array(mapped.unicodeScalars.prefix(3))
        guard characters.count == 3, characters[0].isASCII,
              (65...90).contains(characters[0].value) || (97...122).contains(characters[0].value),
              characters[1].value == 58, characters[2].value == 47 || characters[2].value == 92 else {
            throw CiderError.invalid("采集目标必须是绝对 Windows 路径，不查找 PATH 或解释命令。")
        }
        return mapped.replacingOccurrences(of: "/", with: "\\")
    }

    private func captureEnvironmentIsValid(_ environment: [String: String]) -> Bool {
        environment.allSatisfy { key, value in
            !key.isEmpty && !key.contains("=") && !key.contains("\0") && !value.contains("\0")
        }
    }
}

/// Helpers belong to this App's signed resources, independently of compatibility
/// data overlays. The runner rechecks the selected digest before making a private copy.
public struct WindowsExitCollector: Sendable {
    public let executable: URL
    public let sha256: String
    public init(executable: URL, sha256: String) { self.executable = executable; self.sha256 = sha256 }

    public static func bundled(cpuBackend: String = "rosetta-x86_64", resources: URL? = CiderResources.mainBundleResources) throws -> WindowsExitCollector {
        guard let resources else { throw CiderError.notFound("安装状态采集工具；请使用完整的 Cider 应用包。") }
        let root = try FileSafety.child("windows-tools", in: resources, rejectSymlinks: true)
        let file = try FileSafety.child("manifest.json", in: root, rejectSymlinks: true)
        let manifest = try JSONDecoder().decode(Manifest.self, from: CaptureFiles.read(file, limit: 16 << 10))
        let architecture: String
        switch cpuBackend {
        case "rosetta-x86_64": architecture = "x86_64"
        case "fex-arm64": architecture = "arm64"
        default: throw CiderError.invalid("安装状态采集工具不支持当前 CPU 后端。")
        }
        let expectedMachine: UInt16 = architecture == "x86_64" ? 0x8664 : 0xaa64
        guard manifest.schema == "cider.windows-tools/v1", let entry = manifest.collectors[architecture],
              entry.file == "cider-exit-status-\(architecture).exe", entry.machine == expectedMachine else {
            throw CiderError.invalid("安装状态采集工具的架构或清单不受支持。")
        }
        let executable = try FileSafety.child(entry.file, in: root, rejectSymlinks: true)
        let bytes = try CaptureFiles.read(executable, limit: 2 << 20)
        let sha = SHA256.hash(data: bytes).map { String(format: "%02x", $0) }.joined()
        guard sha == entry.sha256, let header = PEHeader(data: bytes), header.machine == expectedMachine,
              header.isExecutableImage, !header.isDLL, header.subsystem == 2 || header.subsystem == 3 else {
            throw CiderError.invalid("应用内的安装状态采集工具已损坏。")
        }
        return WindowsExitCollector(executable: executable, sha256: sha)
    }

    private struct Manifest: Decodable {
        let schema: String
        let collectors: [String: Entry]
        struct Entry: Decodable { let file: String; let machine: UInt16; let sha256: String }
    }
}

private struct CaptureMetadata: Encodable {
    let schema: String
    let collectorSHA256: String
    let collectorMachine: UInt16
    let targetProgram: String
    let timeoutMilliseconds: UInt32
    let plannedEnvironmentProfile: EnvironmentProfileTrace?
}

private enum CaptureFiles {
    static func read(_ url: URL, limit: Int) throws -> Data {
        let fd = open(url.path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)
        guard fd >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        let handle = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        defer { try? handle.close() }
        var before = stat()
        guard fstat(fd, &before) == 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        guard before.st_mode & mode_t(S_IFMT) == mode_t(S_IFREG), before.st_nlink == 1,
              before.st_size >= 0, before.st_size <= limit else { throw CiderError.invalid("采集文件必须是大小受限的普通单链接文件。") }
        var data = Data()
        while true {
            try Task.checkCancellation()
            let chunk = try handle.read(upToCount: min(64 << 10, limit - data.count + 1)) ?? Data()
            if chunk.isEmpty { break }
            data.append(chunk)
            guard data.count <= limit else { throw CiderError.invalid("采集文件超过读取预算。") }
        }
        var after = stat()
        guard fstat(fd, &after) == 0, after.st_nlink == 1, before.st_size == after.st_size,
              after.st_size == data.count,
              before.st_mtimespec.tv_sec == after.st_mtimespec.tv_sec,
              before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec,
              before.st_ctimespec.tv_sec == after.st_ctimespec.tv_sec,
              before.st_ctimespec.tv_nsec == after.st_ctimespec.tv_nsec else {
            throw CiderError.invalid("读取期间采集文件发生变化。")
        }
        return data
    }

    static func writeNew(_ data: Data, to url: URL) throws {
        let fd = open(url.path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0o400)
        guard fd >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        defer { close(fd) }
        try data.withUnsafeBytes { bytes in
            var written = 0
            while written < bytes.count {
                try Task.checkCancellation()
                let count = write(fd, bytes.baseAddress!.advanced(by: written), bytes.count - written)
                if count < 0, errno == EINTR { continue }
                guard count > 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
                written += count
            }
        }
        guard fsync(fd) == 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
    }
}
