import CiderCore
import Foundation

/// Imports Apple's D3DMetal from a Game Porting Toolkit download the user provides (docs/plan/04 §7.1, ADR-006).
/// D3DMetal is never committed or bundled with Cider: files are copied unmodified into
/// `Components/d3dmetal/<version>/`, with their signature checked and a per-file hash list written.
public struct D3DMetalImporter: Sendable {
    public let paths: CiderPaths
    public init(paths: CiderPaths) { self.paths = paths }

    public var root: URL { paths.appSupport.appendingPathComponent("Components/d3dmetal", isDirectory: true) }

    public struct Imported: Codable, Sendable, Equatable {
        public var schemaVersion: Int
        public var kind: String                    // "d3dmetal"
        public var version: String
        public var importedAt: String
        public var source: String                  // file name of the DMG or folder
        public var codesignValid: Bool
        public var codesignAuthority: [String]
        public var archs: [String]
        public var license: String?                // relative path of License.rtf, if the package has one
        public var modified: Bool
    }

    public enum ImportError: Error, CustomStringConvertible {
        case notGPTK
        case alreadyImported(String)
        public var description: String {
            switch self {
            case .notGPTK: return "这里没有找到 D3DMetal（需要 Apple Game Porting Toolkit 的 DMG 或其中的 redist 文件夹）"
            case .alreadyImported(let v): return "D3DMetal \(v) 已经导入过了"
            }
        }
    }

    /// Imported versions, newest first.
    public func installed() -> [(version: String, directory: URL, info: Imported)] {
        let fm = FileManager.default
        return ((try? fm.contentsOfDirectory(atPath: root.path)) ?? []).compactMap { name in
            let dir = root.appendingPathComponent(name, isDirectory: true)
            guard let info = try? JSONFile.read(Imported.self, from: dir.appendingPathComponent("import.json")) else { return nil }
            return (info.version, dir, info)
        }
        .sorted { $0.version.compare($1.version, options: .numeric) == .orderedDescending }
    }

    /// Imports from a GPTK `.dmg` (nested evaluation-environment DMGs are followed) or a folder containing
    /// `redist/lib` (or `lib`) with `external/libd3dshared.dylib`.
    @discardableResult
    public func importPackage(at url: URL) throws -> Imported {
        var mounts: [URL] = []
        defer { for m in mounts.reversed() { _ = try? Command.run("/usr/bin/hdiutil", ["detach", m.path, "-force"]) } }
        var searchRoots = [url]
        if url.pathExtension.lowercased() == "dmg" {
            let m = try attach(url); mounts.append(m); searchRoots = [m]
            // GPTK wraps the evaluation environment in a second DMG.
            if let inner = findFiles(in: m, maxDepth: 3, where: { $0.pathExtension.lowercased() == "dmg" }).first {
                let m2 = try attach(inner); mounts.append(m2); searchRoots.append(m2)
            }
        }
        guard let lib = searchRoots.lazy.compactMap({ self.findLib(in: $0) }).first else { throw ImportError.notGPTK }
        return try importLib(lib, sourceName: url.lastPathComponent,
                             license: searchRoots.lazy.compactMap { self.findFiles(in: $0, maxDepth: 4, where: { $0.lastPathComponent == "License.rtf" }).first }.first)
    }

    /// `lib` is the directory holding `external/` and `wine/`.
    func importLib(_ lib: URL, sourceName: String, license: URL?) throws -> Imported {
        let fm = FileManager.default
        let framework = lib.appendingPathComponent("external/D3DMetal.framework")
        let plist = NSDictionary(contentsOf: framework.appendingPathComponent("Resources/Info.plist"))
            ?? NSDictionary(contentsOf: framework.appendingPathComponent("Versions/A/Resources/Info.plist"))
        let version = (plist?["CFBundleShortVersionString"] as? String) ?? (plist?["CFBundleVersion"] as? String) ?? "unknown"
        let destination = root.appendingPathComponent(version, isDirectory: true)
        if fm.fileExists(atPath: destination.path) { throw ImportError.alreadyImported(version) }

        // Signature and architecture are recorded, not enforced, until the first real import fixes the
        // requirement (docs/plan/04 open question: `anchor apple` vs `anchor apple generic` + Team ID).
        let verify = (try? Command.run("/usr/bin/codesign", ["--verify", "--strict", framework.path])) != nil
        let details = (try? Command.run("/usr/bin/codesign", ["-dvv", framework.path])) ?? ""
        let authority = details.split(separator: "\n").filter { $0.hasPrefix("Authority=") }.map { String($0.dropFirst(10)) }
        let dylib = lib.appendingPathComponent("external/libd3dshared.dylib")
        let archs = ((try? Command.run("/usr/bin/lipo", ["-archs", dylib.path])) ?? "")
            .split(separator: " ").map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }.filter { !$0.isEmpty }

        let staging = root.appendingPathComponent(".import-\(UUID().uuidString)", isDirectory: true)
        try fm.ensureDirectory(staging)
        do {
            try fm.ensureDirectory(staging.appendingPathComponent("lib"))
            for sub in ["external", "wine"] where fm.fileExists(atPath: lib.appendingPathComponent(sub).path) {
                try Command.run("/usr/bin/ditto", [lib.appendingPathComponent(sub).path, staging.appendingPathComponent("lib/\(sub)").path])
            }
            if let license { try fm.copyItem(at: license, to: staging.appendingPathComponent("License.rtf")) }
            let info = Imported(schemaVersion: 1, kind: "d3dmetal", version: version, importedAt: Identifiers.timestamp(),
                                source: sourceName, codesignValid: verify, codesignAuthority: authority, archs: archs,
                                license: license == nil ? nil : "License.rtf", modified: false)
            try JSONFile.write(info, to: staging.appendingPathComponent("import.json"))
            try hashList(of: staging).write(to: staging.appendingPathComponent("files.sha256"), atomically: true, encoding: .utf8)
            try fm.moveItem(at: staging, to: destination)
            _ = try? Command.run("/bin/chmod", ["-R", "a-w", destination.path])
            return info
        } catch {
            try? fm.removeItem(at: staging)
            throw error
        }
    }

    func attach(_ dmg: URL) throws -> URL {
        let out = try Command.run("/usr/bin/hdiutil", ["attach", "-readonly", "-nobrowse", "-mountrandom", "/tmp", dmg.path])
        guard let mount = out.split(separator: "\n").compactMap({ line -> String? in
            guard let r = line.range(of: "/tmp/") else { return nil }
            return String(line[r.lowerBound...]).trimmingCharacters(in: .whitespaces)
        }).last else { throw ImportError.notGPTK }
        return URL(fileURLWithPath: mount)
    }

    func findLib(in root: URL) -> URL? {
        findFiles(in: root, maxDepth: 6, where: { $0.lastPathComponent == "libd3dshared.dylib" && $0.deletingLastPathComponent().lastPathComponent == "external" })
            .first?.deletingLastPathComponent().deletingLastPathComponent()
    }

    func findFiles(in root: URL, maxDepth: Int, where match: (URL) -> Bool) -> [URL] {
        guard let walker = FileManager.default.enumerator(at: root, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) else { return [] }
        var found: [URL] = []
        for case let url as URL in walker {
            if walker.level > maxDepth { walker.skipDescendants(); continue }
            if url.pathExtension == "framework" { walker.skipDescendants() }
            if match(url) { found.append(url) }
        }
        return found
    }

    func hashList(of dir: URL) throws -> String {
        var entries: [(path: String, hash: String)] = []
        guard let walker = FileManager.default.enumerator(at: dir, includingPropertiesForKeys: [.isRegularFileKey, .isSymbolicLinkKey]) else { return "" }
        for case let url as URL in walker {
            let values = try url.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey])
            guard values.isRegularFile == true, values.isSymbolicLink != true, url.lastPathComponent != "files.sha256" else { continue }
            entries.append((String(url.path.dropFirst(dir.path.count + 1)), try EngineStore.sha256(of: url)))
        }
        return entries.sorted { $0.path < $1.path }.map { "\($0.hash)  \($0.path)" }.joined(separator: "\n") + "\n"
    }
}
