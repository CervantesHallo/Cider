import CiderCore
import CiderSchema
import CiderStore
import CryptoKit
import Darwin
import Foundation

/// One-time preparation of Cider's managed media runtime, at its final installed
/// paths. Call only as part of an authorized execution workflow, outside the
/// bottle lock, and retain that workflow's cancellation lease through Wine spawn.
/// Importing an engine remains a byte-only operation.
public enum MediaRuntimePreparation {
    public enum Outcome: Sendable, Equatable { case unmanaged, cached, prepared }

    private struct Description: Decodable {
        let schemaVersion: Int
        let cpuBackend: String
        let helper: String
        let probeElements: [String]
        let files: [String: String]
    }

    private struct Receipt: Codable, Equatable {
        let schemaVersion: Int
        let enginePath: String
        let directoryIdentity: String
        let systemVersion: String
        let descriptionSHA256: String
        let registrySHA256: String
        let files: [String: String]
    }

    /// The checker is polled while waiting for the engine lock and helper.
    /// A failed or cancelled helper is reaped before this method returns.
    @discardableResult
    public static func prepare(engine: InstalledEngine, paths: CiderPaths,
                               checking check: @escaping @Sendable () throws -> Void = { try Task.checkCancellation() },
                               progress: @Sendable (String) -> Void = { _ in }) throws -> Outcome {
        try check()
        guard engine.bundledGStreamer != nil else { return .unmanaged }
        let runtime = try FileSafety.child("frameworks/gstreamer", in: engine.directory, rejectSymlinks: true)
        let descriptionURL = try FileSafety.child("cider-media-runtime.json", in: runtime, rejectSymlinks: true)
        guard FileSafety.exists(descriptionURL) else { return .unmanaged } // Older packages retain their behavior.
        let engineURL = try FileSafety.child(engine.manifest.id, in: paths.engines, rejectSymlinks: true)
        guard engineURL.path == engine.directory.standardizedFileURL.resolvingSymlinksInPath().path else {
            throw CiderError.invalid("媒体组件不在当前引擎目录内。")
        }
        let store = EngineStore(paths: paths)
        return try FileOperationLock.withLock(at: store.operationLock(for: engine.manifest.id), timeout: 120,
                                              checking: check) {
            try check()
            let current = try JSONFile.readMetadata(EngineManifest.self, from: engineURL.appendingPathComponent("manifest.json"), limit: 1 << 20)
            guard current == engine.manifest else { throw CiderError.invalid("引擎已改变，请重新启动操作。") }
            let runtime = try FileSafety.child("frameworks/gstreamer", in: engineURL, rejectSymlinks: true)
            try EnginePolicy.requireChildPreflight(engine)
            var identity = stat()
            guard lstat(engineURL.path, &identity) == 0, identity.st_mode & mode_t(S_IFMT) == mode_t(S_IFDIR) else {
                throw CiderError.invalid("无法确认媒体运行库目录。")
            }
            let directoryIdentity = "\(identity.st_dev):\(identity.st_ino)"
            let rawDescription = try boundedData(descriptionURL, maximum: 1 << 20)
            let description = try JSONDecoder().decode(Description.self, from: rawDescription)
            guard description.schemaVersion == 1, description.cpuBackend == "rosetta-x86_64",
                  engine.manifest.cpuBackend == description.cpuBackend,
                  description.helper == "bin/gst-inspect-1.0",
                  description.probeElements == ["decodebin", "avdec_wmav2"],
                  !description.files.isEmpty, description.files.count <= 128,
                  description.files[description.helper] != nil,
                  description.files["lib/libgstreamer-1.0.0.dylib"] != nil else {
                throw CiderError.invalid("媒体运行库准备说明不受支持。")
            }
            let cacheDirectory = try FileSafety.child("gstreamer/" + engine.manifest.id, in: paths.caches,
                                                      rejectSymlinks: true)
            try FileManager.default.ensureDirectory(cacheDirectory)
            let registry = try FileSafety.child("registry.bin", in: cacheDirectory, rejectSymlinks: true)
            let receiptURL = try FileSafety.child("preparation.json", in: cacheDirectory, rejectSymlinks: true)
            let system = ProcessInfo.processInfo.operatingSystemVersionString
            let descriptionHash = digest(rawDescription)


            // A package descriptor is data, not an authority to select another
            // executable. Only the fixed bundled helper and checked files run.
            var checkedBytes = 0
            var fileIdentities: [String: String] = [:]
            for (relative, expected) in description.files {
                try check()
                guard expected.count == 64, expected.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }) else {
                    throw CiderError.invalid("媒体运行库校验值无效。")
                }
                let file = try FileSafety.child(relative, in: runtime, rejectSymlinks: true)
                let before = try fileIdentity(file)
                let data = try boundedData(file, maximum: 64 << 20, checking: check)
                let after = try fileIdentity(file)
                checkedBytes += data.count
                guard checkedBytes <= 512 << 20, before == after, digest(data) == expected else {
                    throw CiderError.invalid("媒体组件文件已改变，请重新安装引擎。")
                }
                fileIdentities[relative] = after
            }
            let helper = try FileSafety.child(description.helper, in: runtime, rejectSymlinks: true)
            guard FileManager.default.isExecutableFile(atPath: helper.path) else {
                throw CiderError.invalid("媒体准备组件无法执行。")
            }
            let headerFile = try FileHandle(forReadingFrom: helper)
            defer { try? headerFile.close() }
            let header = try headerFile.read(upToCount: 16) ?? Data()
            guard header.count == 16, Array(header.prefix(4)) == [0xcf, 0xfa, 0xed, 0xfe],
                  Array(header[4..<8]) == [0x07, 0x00, 0x00, 0x01],
                  Array(header[12..<16]) == [0x02, 0x00, 0x00, 0x00] else {
                throw CiderError.invalid("媒体准备组件的架构不正确。")
            }
            if let old = try? JSONDecoder().decode(Receipt.self, from: boundedData(receiptURL, maximum: 1 << 20)),
               old.schemaVersion == 1, old.enginePath == engineURL.path,
               old.directoryIdentity == directoryIdentity, old.systemVersion == system,
               old.descriptionSHA256 == descriptionHash, old.files == fileIdentities,
               let registryData = try? boundedData(registry, maximum: 16 << 20), !registryData.isEmpty,
               digest(registryData) == old.registrySHA256 {
                try check()
                return .cached
            }
            let pluginDirectory = try FileSafety.child("lib/gstreamer-1.0", in: runtime, rejectSymlinks: true)
            let scanner = try FileSafety.child("libexec/gstreamer-1.0/gst-plugin-scanner", in: runtime, rejectSymlinks: true)
            let transaction = try FileSafety.reserveDirectory(in: cacheDirectory) { "preparing-" + UUID().uuidString }
            let temporaryRegistry = transaction.appendingPathComponent("registry.bin")
            var preserveTransaction = false
            defer { if !preserveTransaction { try? FileManager.default.removeItem(at: transaction) } }
            let environment = [
                "HOME": FileManager.default.homeDirectoryForCurrentUser.path,
                "PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "LANG": "en_US.UTF-8",
                "GST_PLUGIN_SYSTEM_PATH": pluginDirectory.path, "GST_PLUGIN_PATH": "",
                "GST_PLUGIN_SCANNER": scanner.path, "GST_REGISTRY": temporaryRegistry.path,
                "GST_DEBUG": "1", "GST_DEBUG_NO_COLOR": "1",
            ]
            progress("正在准备媒体组件…")
            let clock = ContinuousClock()
            let deadline = clock.now.advanced(by: .seconds(90))
            for (index, element) in description.probeElements.enumerated() {
                try check()
                let remaining = clock.now.duration(to: deadline).components
                let seconds = Double(remaining.seconds) + Double(remaining.attoseconds) / 1e18
                guard seconds > 0 else { throw CiderError.invalid("媒体组件准备超时，请重试。") }
                var arguments = ["--gst-disable-registry-fork"]
                // The second process must read the written registry; a failed
                // writer cannot be hidden by another successful in-memory scan.
                if index > 0 { arguments.append("--gst-disable-registry-update") }
                arguments += ["--exists", element]
                do {
                    try Command.runCancellable(helper.path, arguments,
                                               environment: environment, timeout: seconds, outputLimit: 1 << 20,
                                               retainUntilReaped: true,
                                               cancellationRequested: {
                        do { try check(); return false } catch { return true }
                    })
                } catch is CancellationError {
                    try check() // Retain a more specific ownership/lease error when available.
                    throw CancellationError()
                }
            }
            try check()
            let data = try boundedData(temporaryRegistry, maximum: 16 << 20)
            guard !data.isEmpty else { throw CiderError.invalid("媒体组件未生成有效缓存。") }
            let oldReceipt = transaction.appendingPathComponent("prior-preparation.json")
            if FileSafety.exists(receiptURL) {
                try boundedData(receiptURL, maximum: 1 << 20).write(to: oldReceipt, options: .atomic)
            }
            var oldRegistry: URL?
            var registryPublished = false
            do {
                try check()
                oldRegistry = try FileSafety.commit(temporaryRegistry, to: registry)
                registryPublished = true
                var finalEnvironment = environment
                finalEnvironment["GST_REGISTRY"] = registry.path
                let remaining = clock.now.duration(to: deadline).components
                let seconds = Double(remaining.seconds) + Double(remaining.attoseconds) / 1e18
                guard seconds > 0 else { throw CiderError.invalid("媒体组件准备超时，请重试。") }
                try Command.runCancellable(helper.path,
                                           ["--gst-disable-registry-fork", "--gst-disable-registry-update", "--exists", "avdec_wmav2"],
                                           environment: finalEnvironment, timeout: seconds, outputLimit: 1 << 20,
                                           retainUntilReaped: true, cancellationRequested: {
                    do { try check(); return false } catch { return true }
                })
                try check()
                let committed = try boundedData(registry, maximum: 16 << 20)
                guard !committed.isEmpty else { throw CiderError.invalid("媒体组件未生成有效缓存。") }
                for (relative, expectedIdentity) in fileIdentities {
                    let file = try FileSafety.child(relative, in: runtime, rejectSymlinks: true)
                    guard try fileIdentity(file) == expectedIdentity else {
                        throw CiderError.invalid("媒体组件在准备期间已改变，请重试。")
                    }
                }
                let receipt = Receipt(schemaVersion: 1, enginePath: engineURL.path, directoryIdentity: directoryIdentity,
                                      systemVersion: system, descriptionSHA256: descriptionHash, registrySHA256: digest(committed),
                                      files: fileIdentities)
                try JSONFile.write(receipt, to: receiptURL)
                try check()
                return .prepared
            } catch {
                if registryPublished {
                    // Existing Wine processes may also update registry.bin.
                    // Keep the prior pair instead of clobbering such a writer.
                    preserveTransaction = true
                    try? JSONFile.writeMetadata([
                        "engine": engine.manifest.id,
                        "cause": String(describing: error),
                        "priorRegistry": oldRegistry?.path ?? "",
                        "priorReceipt": FileSafety.exists(oldReceipt) ? oldReceipt.path : "",
                        "liveRegistry": registry.path,
                        "liveReceipt": receiptURL.path,
                        "recovery": "Retained prior bytes; retry preparation from verified runtime files. No destination overwritten during failure recovery.",
                    ], to: transaction.appendingPathComponent("recovery.json"))
                    CiderDiagnostics.warning("媒体准备未完成，已有缓存备份保留在 \(transaction.path)。")
                }
                throw error
            }
        }
    }

    /// Read-only revalidation in the short bottle->engine launch transaction.
    /// Never repairs or executes a cold scan while that transaction is held.
    public static func requirePrepared(engine: InstalledEngine, paths: CiderPaths) throws {
        let engineURL = try FileSafety.child(engine.manifest.id, in: paths.engines, rejectSymlinks: true)
        guard engineURL.path == engine.directory.standardizedFileURL.resolvingSymlinksInPath().path,
              try JSONFile.readMetadata(EngineManifest.self, from: engineURL.appendingPathComponent("manifest.json"), limit: 1 << 20) == engine.manifest else {
            throw CiderError.invalid("引擎已改变，请重新启动操作。")
        }
        let runtime = try FileSafety.child("frameworks/gstreamer", in: engineURL, rejectSymlinks: true)
        let description = try FileSafety.child("cider-media-runtime.json", in: runtime, rejectSymlinks: true)
        let raw = try boundedData(description, maximum: 1 << 20)
        let layout = try JSONDecoder().decode(Description.self, from: raw)
        guard layout.schemaVersion == 1, layout.cpuBackend == engine.manifest.cpuBackend,
              layout.helper == "bin/gst-inspect-1.0", !layout.files.isEmpty, layout.files.count <= 128 else {
            throw CiderError.invalid("媒体组件准备说明已改变。")
        }
        var currentFiles: [String: String] = [:]
        for relative in layout.files.keys {
            currentFiles[relative] = try fileIdentity(FileSafety.child(relative, in: runtime, rejectSymlinks: true))
        }
        let directory = try FileSafety.child("gstreamer/" + engine.manifest.id, in: paths.caches, rejectSymlinks: true)
        let registry = try FileSafety.child("registry.bin", in: directory, rejectSymlinks: true)
        let receiptURL = try FileSafety.child("preparation.json", in: directory, rejectSymlinks: true)
        let old = try JSONDecoder().decode(Receipt.self, from: boundedData(receiptURL, maximum: 1 << 20))
        var identity = stat()
        guard lstat(engineURL.path, &identity) == 0, identity.st_mode & mode_t(S_IFMT) == mode_t(S_IFDIR),
              old.schemaVersion == 1, old.enginePath == engineURL.path,
              old.directoryIdentity == "\(identity.st_dev):\(identity.st_ino)",
              old.systemVersion == ProcessInfo.processInfo.operatingSystemVersionString,
              old.descriptionSHA256 == digest(raw), old.files == currentFiles else {
            throw CiderError.invalid("媒体组件准备状态已改变，请重新启动操作。")
        }
        let data = try boundedData(registry, maximum: 16 << 20)
        guard !data.isEmpty, old.registrySHA256 == digest(data) else {
            throw CiderError.invalid("媒体缓存已改变，请重新准备后启动。")
        }
    }

    private static func fileIdentity(_ url: URL) throws -> String {
        var info = stat()
        guard lstat(url.path, &info) == 0, info.st_mode & mode_t(S_IFMT) == mode_t(S_IFREG) else {
            throw CiderError.invalid("媒体组件必须是普通文件。")
        }
        return "\(info.st_dev):\(info.st_ino):\(info.st_size):\(info.st_mode):" +
            "\(info.st_mtimespec.tv_sec):\(info.st_mtimespec.tv_nsec):" +
            "\(info.st_ctimespec.tv_sec):\(info.st_ctimespec.tv_nsec)"
    }

    private static func boundedData(_ url: URL, maximum: Int,
                                    checking check: () throws -> Void = {}) throws -> Data {
        var info = stat()
        guard lstat(url.path, &info) == 0, info.st_mode & mode_t(S_IFMT) == mode_t(S_IFREG),
              info.st_size >= 0, info.st_size <= maximum else {
            throw CiderError.invalid("媒体运行库记录类型或大小无效。")
        }
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        var data = Data()
        while let chunk = try handle.read(upToCount: min(64 << 10, maximum + 1 - data.count)), !chunk.isEmpty {
            try check()
            data.append(chunk)
            guard data.count <= maximum else { throw CiderError.invalid("媒体运行库记录过大。") }
        }
        return data
    }

    private static func digest(_ data: Data) -> String {
        SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
    }
}
