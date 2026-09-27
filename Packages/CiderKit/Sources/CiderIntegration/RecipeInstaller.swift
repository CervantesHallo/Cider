import CiderBottle
import CiderCore
import CiderData
import CiderRuntime
import CiderStore
import Foundation

/// Runs recipes (docs/plan/06 §4, §8): downloads into a content-addressed cache, checks hashes, installs
/// dependencies first, executes the typed steps in the bottle and verifies the result with `detect`.
public struct RecipeInstaller: Sendable {
    public let store: BottleStore
    public let db: CompatDB
    public var downloads: URL { store.paths.caches.appendingPathComponent("downloads/cas", isDirectory: true) }

    public init(store: BottleStore, db: CompatDB) {
        self.store = store
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
        guard let recipe = db.recipes[recipeID] else { throw InstallError.unknownRecipe(recipeID) }
        guard !visiting.contains(recipeID) else { return }                     // dependency cycle: ignore
        for dependency in recipe.dependencies ?? [] {
            if let dep = db.recipes[dependency], isInstalled(dep, in: bottle) { continue }
            try install(dependency, in: bottle, progress: progress, visiting: visiting.union([recipeID]))
        }
        if isInstalled(recipe, in: bottle) { return }
        let runner = try store.runner(for: bottle)
        var files: [String: URL] = [:]
        for step in recipe.steps {
            if let run = step.runInstaller {
                let file: URL
                if let cached = files[run.source] { file = cached } else {
                    progress("正在下载 \(recipe.title())…")
                    file = try fetch(recipe.sources[run.source]!, name: run.source)
                    files[run.source] = file
                }
                progress("正在安装 \(recipe.title())…")
                let plan = run.kind == "msi"
                    ? runner.plan(program: "msiexec", arguments: ["/i", file.path] + (run.args ?? []), label: "install-\(recipe.id)")
                    : runner.plan(program: file.path, arguments: run.args ?? [], label: "install-\(recipe.id)",
                                  cwd: file.deletingLastPathComponent())
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
        while !isInstalled(recipe, in: bottle), Date() < deadline { Thread.sleep(forTimeInterval: 2) }
        guard isInstalled(recipe, in: bottle) else { throw InstallError.notDetected(recipe.title()) }
    }

    /// Downloads a source into `cas/<sha256>/<file>`; pinned sources must match one of their hashes.
    func fetch(_ source: Recipe.Source, name: String) throws -> URL {
        let fm = FileManager.default
        if let known = source.sha256.first {
            let dir = downloads.appendingPathComponent(known.lowercased(), isDirectory: true)
            if let cached = try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil).first { return cached }
        }
        let tmp = downloads.appendingPathComponent(".partial-\(UUID().uuidString)")
        try fm.ensureDirectory(downloads)
        var lastError: Error?
        for url in source.urls {
            do {
                try Command.run("/usr/bin/curl", ["-fsSL", "--retry", "3", "--connect-timeout", "20", "-o", tmp.path, url])
                lastError = nil
                break
            } catch { lastError = error }
        }
        if let lastError { try? fm.removeItem(at: tmp); throw lastError }
        let sha = try EngineStore.sha256(of: tmp)
        guard source.sha256.isEmpty || source.sha256.contains(where: { $0.lowercased() == sha }) else {
            try? fm.removeItem(at: tmp)
            throw InstallError.checksum(source.filename ?? name)
        }
        let filename = source.filename ?? URL(string: source.urls[0])?.lastPathComponent ?? name
        let dir = downloads.appendingPathComponent(sha, isDirectory: true)
        try fm.ensureDirectory(dir)
        let dest = dir.appendingPathComponent(filename)
        try? fm.removeItem(at: dest)
        try fm.moveItem(at: tmp, to: dest)
        return dest
    }
}
