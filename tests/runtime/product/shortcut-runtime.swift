import CiderBottle
import CiderCore
import CiderIntegration
import CiderPE
import CiderRuntime
import CoreFoundation
import Foundation

@main struct OwnedShortcutAcceptance {
    static func require(_ value: Bool, _ message: String) throws { if !value { throw CiderError.invalid(message) } }
    static func word(_ n: Int) -> Data { withUnsafeBytes(of: UInt16(n).littleEndian) { Data($0) } }
    static func dword(_ n: Int) -> Data { withUnsafeBytes(of: UInt32(n).littleEndian) { Data($0) } }
    static func wide(_ s: String) -> Data { s.utf16.reduce(into: Data()) { $0.append(word(Int($1))) } }
    static let acp = String.Encoding(rawValue: CFStringConvertEncodingToNSStringEncoding(CFStringConvertWindowsCodepageToEncoding(936)))

    static func link(target: String, cwd: String, arguments: String, unicode: Bool) throws -> Data {
        let encode: (String) throws -> Data = { s in
            if unicode { return wide(s) }
            guard let bytes = s.data(using: acp) else { throw CiderError.invalid("fixture CP936 encoding failed") }
            return bytes
        }
        let headerSize = unicode ? 36 : 28
        let volume = dword(17) + dword(3) + dword(0) + dword(16) + Data([0])
        let base = unicode ? Data("C:\\fallback.exe".utf8) : try encode(target)
        let baseOffset = headerSize + volume.count, suffixOffset = baseOffset + base.count + 1
        var body = volume + base + Data([0,0])
        var baseU = 0, suffixU = 0
        if unicode {
            baseU = headerSize + body.count; body.append(wide(target) + word(0))
            suffixU = headerSize + body.count; body.append(word(0))
        }
        var info = dword(headerSize + body.count) + dword(headerSize) + dword(1) + dword(headerSize)
            + dword(baseOffset) + dword(0) + dword(suffixOffset)
        if unicode { info.append(dword(baseU) + dword(suffixU)) }
        info.append(body)
        var result = dword(0x4c)
        result.append(contentsOf: [1,0x14,2,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46])
        result.append(dword(2 | 4 | 0x10 | 0x20 | 0x40 | (unicode ? 0x80 : 0)))
        result.append(Data(repeating: 0, count: 0x4c - result.count)); result.append(info)
        for text in ["合法丂显示元数据", cwd, arguments, "丂不存在.ico"] {
            let bytes = try encode(text)
            result.append(word(unicode ? bytes.count / 2 : bytes.count)); result.append(bytes)
        }
        return result + dword(0)
    }

    static func main() throws {
        let base = URL(fileURLWithPath: CommandLine.arguments[1]).standardizedFileURL
        let owner = try JSONSerialization.jsonObject(with: Data(contentsOf: base.appendingPathComponent("owner.json"))) as! [String: Any]
        try require(base.lastPathComponent.hasPrefix("runtime-acceptance-") && owner["root"] as? String == base.path, "foreign root")
        let home = base.appendingPathComponent("private-home")
        try require(owner["home"] as? String == home.path, "foreign data home")
        let paths = CiderPaths.standard(environment: ["CIDER_HOME": home.path]), store = BottleStore(paths: paths)
        let bottle = try store.bottle("Runtime Acceptance")
        try require(bottle.config.locale.acp == 936, "requires known CP936 test bottle")
        let nonce = UUID().uuidString, fixture = bottle.driveC.appendingPathComponent("Owned Links/" + nonce)
        let cwd = fixture.appendingPathComponent("工作 dir"), links = bottle.driveC.appendingPathComponent("ProgramData/Microsoft/Windows/Start Menu/Programs/Owned-" + nonce)
        let archive = base.appendingPathComponent("shortcut-artifacts/" + nonce)
        for directory in [cwd,links,archive] { try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true) }
        defer { try? FileManager.default.removeItem(at: fixture); try? FileManager.default.removeItem(at: links) }
        let expected = ["", "alpha beta", "中文", "quote\"tail", #"backslash\\"#, "tab\tvalue", "--exit", "37"]
        let tail = ["\"\"", "\"alpha beta\"", "中文", #"quote\"tail"#, #"backslash\\"#, "\"tab\tvalue\"", "--exit", "37"].joined(separator: " ")
        let winCwd = WindowsPath.windowsPath(forHostPath: cwd.path, in: bottle)
        let catalog = AppCatalog(iconCache: IconCache(directory: archive.appendingPathComponent("icons")))
        let runner = try store.runner(for: bottle)
        var observations: [[String: Any]] = []

        func execute(_ app: CatalogApp, label: String, environment: String = "") throws {
            try require(app.launchArguments == expected, "catalog argument mismatch")
            let hostCwd = try WindowsPath.workingDirectory(app.workingDirectory, in: bottle)
            try require(hostCwd?.standardizedFileURL.path == cwd.standardizedFileURL.path, "guest cwd mapped incorrectly")
            for round in 0..<2 {
                guard let session = try AppLifecycle.start(app, using: runner, cwd: hostCwd) else { throw CiderError.invalid("old app falsely still running") }
                let code = Spawn.wait(session.pid)
                let log = try String(contentsOf: session.log, encoding: .utf8)
                guard let line = log.split(separator: "\n").first(where: { $0.hasPrefix("{\"kind\":\"cider-owned-win32-probe\"") }) else { throw CiderError.invalid("missing owned guest observation") }
                let value = try JSONSerialization.jsonObject(with: Data(line.utf8)) as! [String: Any]
                try require(code == 37 && value["arguments"] as? [String] == expected, "guest arguments or exit changed")
                try require((value["cwd"] as? String)?.lowercased() == winCwd.lowercased(), "actual guest cwd differs")
                try require(value["fixtureEnvironment"] as? String == environment, "saved environment differs")
                observations.append(["entry":label,"round":round,"unixExitCode":code,"observed":value,"session":session.directory.path])
            }
        }
        for unicode in [true,false] {
            let name = unicode ? "Unicode 游戏" : "CP936 游戏"
            let file = fixture.appendingPathComponent(name + ".exe")
            try FileManager.default.copyItem(at: base.appendingPathComponent("probe.exe"), to: file)
            let bytes = try link(target: WindowsPath.windowsPath(forHostPath: file.path, in: bottle), cwd: winCwd, arguments: tail, unicode: unicode)
            try bytes.write(to: links.appendingPathComponent(name + ".lnk")); try bytes.write(to: archive.appendingPathComponent(name + ".lnk"))
            guard let app = catalog.apps(in: bottle).first(where: { $0.title == name }) else { throw CiderError.invalid("valid shortcut disappeared from catalog") }
            try execute(app, label: name)
        }

        let id = "owned-" + nonce
        defer { _ = try? store.removeLauncher(id: id, from: bottle) }
        let saved = try store.saveLauncher(.init(id: id, name: "Owned Saved " + nonce,
            program: fixture.appendingPathComponent("Unicode 游戏.exe").path, arguments: expected,
            workingDirectory: winCwd, environment: ["ACCEPTANCE_VALUE":"saved-env-中文"]), in: bottle)
        guard let app = catalog.apps(in: saved).first(where: { $0.id == bottle.config.id + "/launcher/" + id }) else { throw CiderError.invalid("saved entry missing") }
        try execute(app, label: "saved-host-program-guest-cwd", environment: "saved-env-中文")
        let result: [String:Any] = ["schema":"cider.shortcut-runtime/v1","passed":true,"cases":observations,
            "scope":"real Catalog/ShellLink/WindowsPath/AppLifecycle/Wine, not GUI clicks or native Windows Shell",
            "fixtureArchive":archive.path]
        try JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted,.sortedKeys]).write(to: base.appendingPathComponent("shortcut-result.json"))
    }
}
