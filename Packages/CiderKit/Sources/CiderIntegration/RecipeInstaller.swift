import CiderBottle
import CiderCore
import CiderData
import CiderRuntime
import CiderStore
import CryptoKit
import Darwin
import Foundation

/// Runs recipes (docs/plan/06 §4, §8): downloads into a content-addressed cache, checks hashes, installs
/// dependencies first, executes the typed steps in the bottle and verifies the result with `detect`.
public struct RecipeInstaller: Sendable {
    public let store: BottleStore
    public let db: CompatDB
    public var downloads: URL { store.paths.caches.appendingPathComponent("downloads/cas", isDirectory: true) }

    public init(store: BottleStore, db: CompatDB) {
        self.store = BottleStore(paths: store.paths, compat: db)
        self.db = db
    }

    public enum InstallError: Error, CustomStringConvertible {
        case unknownRecipe(String)
        case checksum(String)
        case notDetected(String)
        public var description: String {
            switch self {
            case .unknownRecipe(let id): return "没有找到配方 \(id)"
            case .checksum(let file): return "\(file) 的校验和不匹配，已停止安装（文件可能被篡改或下载不完整）"
            case .notDetected(let name): return "\(name) 的安装程序结束了，但没有找到安装结果"
            }
        }
    }

    /// Whether everything `detect` lists exists in the bottle.
    public func isInstalled(_ recipe: Recipe, in bottle: Bottle) -> Bool {
        let files = recipe.detect.files.allSatisfy { path in
            (try? WindowsPath.hostURL(for: path, in: bottle)).map { FileManager.default.fileExists(atPath: $0.path) } ?? false
        }
        let any = recipe.detect.anyFiles.map { list in list.contains { path in
            (try? WindowsPath.hostURL(for: path, in: bottle)).map { FileManager.default.fileExists(atPath: $0.path) } ?? false
        } } ?? true
        return files && any && (recipe.detect.registryKeys ?? []).allSatisfy { registryKeyExists($0, in: bottle) }
            && (recipe.detect.registryValues ?? []).allSatisfy { (registryDWORD($0.key, $0.name, in: bottle) ?? 0) >= $0.atLeast }
    }

    /// A DWORD value from Wine's registry files, or nil.
    func registryDWORD(_ key: String, _ name: String, in bottle: Bottle) -> UInt32? {
        guard let (text, needle) = registryText(for: key, in: bottle),
              let start = text.range(of: needle, options: .caseInsensitive) else { return nil }
        let block = text[start.upperBound...]
        for line in block.split(separator: "\n").dropFirst() {
            if line.hasPrefix("[") { break }
            let prefix = "\"\(name)\"=dword:"
            if line.lowercased().hasPrefix(prefix.lowercased()) { return UInt32(line.dropFirst(prefix.count), radix: 16) }
        }
        return nil
    }

    private func registryText(for key: String, in bottle: Bottle) -> (String, String)? {
        let parts = key.split(separator: "\\", maxSplits: 1).map(String.init)
        guard parts.count == 2 else { return nil }
        let file: String
        switch parts[0].uppercased() {
        case "HKLM", "HKEY_LOCAL_MACHINE": file = "system.reg"
        case "HKCU", "HKEY_CURRENT_USER": file = "user.reg"
        default: return nil
        }
        guard let text = try? String(contentsOf: bottle.prefix.appendingPathComponent(file), encoding: .utf8) else { return nil }
        return (text, "[" + parts[1].replacingOccurrences(of: "\\", with: "\\\\") + "]")
    }

    /// Looks the key up in Wine's registry files (`system.reg` for HKLM, `user.reg` for HKCU), where keys are
    /// written as `[Software\\Microsoft\\…]` with doubled backslashes. Wine flushes these files a few seconds
    /// after a change, which the post-install polling absorbs.
    func registryKeyExists(_ key: String, in bottle: Bottle) -> Bool {
        guard let (text, needle) = registryText(for: key, in: bottle) else { return false }
        return text.range(of: needle, options: .caseInsensitive) != nil
    }

    /// Installs the recipe and its missing dependencies. `progress` gets short user-facing steps.
    public func install(_ recipeID: String, in bottle: Bottle, progress: @Sendable (String) -> Void = { _ in }) throws {
        try install(recipeID, in: bottle, progress: progress, visiting: [])
    }

    private func install(_ recipeID: String, in bottle: Bottle, progress: @Sendable (String) -> Void, visiting: Set<String>) throws {
        try Task.checkCancellation()
        guard let recipe = db.recipes[recipeID] else { throw InstallError.unknownRecipe(recipeID) }
        guard !visiting.contains(recipeID) else { return }                     // dependency cycle: ignore
        for dependency in recipe.dependencies ?? [] {
            try Task.checkCancellation()
            if let dep = db.recipes[dependency], isInstalled(dep, in: bottle) { continue }
            try install(dependency, in: bottle, progress: progress, visiting: visiting.union([recipeID]))
        }
        if isInstalled(recipe, in: bottle) { return }
        let runner = try store.runner(for: bottle)
        var files: [String: URL] = [:]
        for step in recipe.steps {
            try Task.checkCancellation()
            if let run = step.runInstaller {
                var installerEnvironment: [String: String] = [:]
                if let id = step.environmentProfile {
                    guard let profile = db.profiles.first(where: { $0.id == id }), profile.target == recipe.id else {
                        throw CiderError.invalid("安装步骤的环境档案不存在、被拒绝或不属于此配方：\(id)")
                    }
                    installerEnvironment = profile.actions.env ?? [:]
                }
                guard let source = recipe.sources[run.source] else { throw CiderError.invalid("配方引用了不存在的下载来源") }
                let file: URL
                if let cached = files[run.source] {
                    // A previous installer can change the cache: check every consumption, including reuse
                    // within this recipe. Floating downloads are bound to their content-addressed directory.
                    try verifyCachedFile(cached, source: source, name: run.source)
                    file = cached
                } else {
                    progress("正在下载 \(recipe.title())…")
                    file = try fetch(source, name: run.source)
                    files[run.source] = file
                }
                try Task.checkCancellation()
                progress("正在安装 \(recipe.title())…")
                let plan = run.kind == "msi"
                    ? runner.plan(program: "msiexec", arguments: ["/i", file.path] + (run.args ?? []), label: "install-\(recipe.id)",
                                  extraEnv: installerEnvironment)
                    : runner.plan(program: file.path, arguments: run.args ?? [], label: "install-\(recipe.id)",
                                  cwd: file.deletingLastPathComponent(), extraEnv: installerEnvironment)
                _ = try runner.runToCompletion(plan)
            }
            if let sets = step.registry, !sets.isEmpty {
                let grouped = Dictionary(grouping: sets, by: \.key)
                try PrefixSetup.importRegistry(grouped.map { ($0.key, $0.value.map { ($0.name, RegValue.string($0.value)) }) },
                                               named: "recipe-\(recipe.id)", bottle: bottle, runner: runner)
            }
            if let winver = step.winver {
                _ = try runner.runToCompletion(runner.plan(program: "winecfg", arguments: ["/v", winver], label: "winver-\(recipe.id)"))
            }
        }
        // Installers often hand off to a child process and exit early: poll for the result (bounded — other
        // programs such as Steam may keep the bottle busy, so waiting for it to go idle could block forever).
        let deadline = Date().addingTimeInterval(120)
        while !isInstalled(recipe, in: bottle), Date() < deadline {
            try Task.checkCancellation()
            Thread.sleep(forTimeInterval: 0.2)
        }
        try Task.checkCancellation()
        guard isInstalled(recipe, in: bottle) else { throw InstallError.notDetected(recipe.title()) }
    }

    /// Downloads a source into `cas/<sha256>/<file>`; pinned sources must match one of their hashes.
    func fetch(_ source: Recipe.Source, name: String) throws -> URL {
        try Task.checkCancellation()
        let fm = FileManager.default
        let filename = try Self.filename(for: source, fallback: name)
        let hashes = try Self.acceptedHashes(for: source)
        for known in hashes {
            try Task.checkCancellation()
            guard let dir = try cacheDirectory(hash: known, create: false) else { continue }
            let cached = dir.appendingPathComponent(filename)
            if try Self.fileMode(at: cached) != nil {
                try verifyCachedFile(cached, source: source, name: name)
                return cached
            }
        }
        _ = try cacheDirectory(create: true)
        let tmp = downloads.appendingPathComponent(".partial-\(UUID().uuidString)")
        // Reserve a private regular file before curl opens it. Never treat a pre-existing link as a download.
        let fd = open(tmp.path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode_t(0o600))
        guard fd >= 0 else { throw Self.fileError() }
        _ = close(fd)
        defer { try? fm.removeItem(at: tmp) }
        var lastError: Error?
        for url in source.urls {
            try Task.checkCancellation()
            do {
                try Command.runCancellable("/usr/bin/curl", ["-fsSL", "--retry", "3", "--connect-timeout", "20", "--max-time", "300", "-o", tmp.path, url])
                lastError = nil
                break
            } catch is CancellationError { throw CancellationError() } catch {
                // Preserve a failed child termination instead of hiding it behind task cancellation.
                if Task.isCancelled { throw error }
                lastError = error
            }
        }
        try Task.checkCancellation()
        if let lastError { try? fm.removeItem(at: tmp); throw lastError }
        let sha = try Self.regularFileSHA256(of: tmp)
        guard hashes.isEmpty || hashes.contains(sha) else {
            try? fm.removeItem(at: tmp)
            throw InstallError.checksum(filename)
        }
        guard let dir = try cacheDirectory(hash: sha, create: true) else { throw CiderError.invalid("无法创建安装缓存") }
        let dest = dir.appendingPathComponent(filename)
        if try Self.fileMode(at: dest) != nil {
            // Another download may have filled the cache meanwhile. Consume only the verified fixed file;
            // do not recursively remove a directory or overwrite an unexpected filesystem object.
            try verifyCachedFile(dest, source: source, name: name)
            return dest
        }
        try Task.checkCancellation()
        try fm.moveItem(at: tmp, to: dest)
        try verifyCachedFile(dest, source: source, name: name)
        return dest
    }

    private static func filename(for source: Recipe.Source, fallback: String) throws -> String {
        guard let firstURL = source.urls.first else { throw CiderError.invalid("下载来源没有 URL") }
        let last = URL(string: firstURL)?.lastPathComponent
        let filename = source.filename ?? last.flatMap { $0.isEmpty ? nil : $0 } ?? fallback
        guard !filename.isEmpty, filename != ".", filename != "..", filename.utf8.count <= 255,
              filename == filename.trimmingCharacters(in: .whitespacesAndNewlines),
              !filename.contains("/"), !filename.contains("\\"), !filename.contains(":"),
              !filename.unicodeScalars.contains(where: { CharacterSet.controlCharacters.contains($0) }) else {
            throw CiderError.invalid("下载文件名必须是安全的单个文件名")
        }
        return filename
    }

    private static func acceptedHashes(for source: Recipe.Source) throws -> [String] {
        let hashes = source.sha256.map { $0.lowercased() }
        guard hashes.allSatisfy({ hash in
            hash.utf8.count == 64 && hash.utf8.allSatisfy { (48...57).contains($0) || (97...102).contains($0) }
        }) else { throw CiderError.invalid("下载来源的 SHA-256 格式无效") }
        return hashes
    }

    /// Refuse links and non-directory objects in the cache hierarchy, including the CAS hash directory.
    /// The caller chooses the cache root; the descendants are always fixed components or validated hashes.
    private func cacheDirectory(hash: String? = nil, create: Bool) throws -> URL? {
        let fm = FileManager.default
        var dir = store.paths.caches
        let components = ["downloads", "cas"] + (hash.map { [$0] } ?? [])
        for component in [nil] + components.map({ Optional($0) }) {
            if let component { dir.appendPathComponent(component, isDirectory: true) }
            if try Self.fileMode(at: dir) == nil {
                guard create else { return nil }
                try fm.createDirectory(at: dir, withIntermediateDirectories: component == nil, attributes: [.posixPermissions: 0o700])
            }
            guard let mode = try Self.fileMode(at: dir), mode & mode_t(S_IFMT) == mode_t(S_IFDIR) else {
                throw CiderError.invalid("安装缓存目录不能是符号链接或其他文件类型")
            }
        }
        return dir
    }

    private func verifyCachedFile(_ file: URL, source: Recipe.Source, name: String) throws {
        try Task.checkCancellation()
        let filename = try Self.filename(for: source, fallback: name)
        let hashes = try Self.acceptedHashes(for: source)
        let expected = file.deletingLastPathComponent().lastPathComponent
        guard expected.utf8.count == 64,
              expected.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }),
              file.lastPathComponent == filename,
              let dir = try cacheDirectory(hash: expected, create: false),
              dir.standardizedFileURL.path == file.deletingLastPathComponent().standardizedFileURL.path,
              hashes.isEmpty || hashes.contains(expected) else { throw InstallError.checksum(filename) }
        guard try Self.regularFileSHA256(of: file) == expected else { throw InstallError.checksum(filename) }
        try Task.checkCancellation()
    }

    private static func fileMode(at url: URL) throws -> mode_t? {
        var info = stat()
        guard lstat(url.path, &info) == 0 else {
            if errno == ENOENT { return nil }
            throw fileError()
        }
        return info.st_mode
    }

    private static func fileError() -> POSIXError { POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }

    /// Opening without following the final link, then fstat, avoids reading directories, FIFOs or devices.
    /// Check cancellation per chunk rather than hashing an entire installer in an uncancellable call.
    private static func regularFileSHA256(of url: URL) throws -> String {
        try Task.checkCancellation()
        let fd = open(url.path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)
        guard fd >= 0 else { throw fileError() }
        let handle = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        defer { try? handle.close() }
        var info = stat()
        guard fstat(fd, &info) == 0 else { throw fileError() }
        guard info.st_mode & mode_t(S_IFMT) == mode_t(S_IFREG) else {
            throw CiderError.invalid("安装缓存必须是普通文件，不能是目录、链接或设备")
        }
        var hasher = SHA256()
        while true {
            try Task.checkCancellation()
            guard let chunk = try handle.read(upToCount: 1 << 20), !chunk.isEmpty else { break }
            hasher.update(data: chunk)
        }
        try Task.checkCancellation()
        return hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }
}
