import Foundation

/// R3 preflight (docs/plan/00 ADR-010, 09): games whose kernel anti-cheat Cider cannot yet satisfy honestly are
/// not started at all — the user gets a route card (official cloud) instead of an anti-cheat error. There is no
/// switch to skip this, in the app or in ciderctl.
public enum Preflight {
    /// Built into the code on purpose: missing or tampered data must never turn a launch back on.
    /// Keys are lowercase image names.
    public static let gatedExecutables: [String: String] = [
        "yuanshen.exe": "cider:com.mihoyo.ys",
        "genshinimpact.exe": "cider:com.mihoyo.ys",
        "starrail.exe": "cider:com.mihoyo.sr",
        "zenlesszonezero.exe": "cider:com.mihoyo.zzz",
    ]

    /// Why a launch was refused, with where the user can play instead.
    public struct Block: Error, Sendable, Equatable, CustomStringConvertible {
        public var gameID: String
        public var image: String
        public var title: String
        public var reason: String
        public var routes: [GameEntry.Route]
        public var description: String { "\(title)：\(reason)" }
    }

    /// Image name of a program given as a Windows path, a Mac path or a bare name.
    public static func imageName(of program: String) -> String {
        let normalized = program.replacingOccurrences(of: "\\", with: "/")
        return (normalized.split(separator: "/").last.map(String.init) ?? normalized).lowercased()
    }

    /// The block for `program`, or nil when it may start. A gated game only starts once the data carries a
    /// playable lab verdict for this environment (`CompatDB.verdict` applies the gated-entry rules).
    public static func check(program: String, db: CompatDB?, key: (String) -> VerdictKey? = { _ in nil }) -> Block? {
        let image = imageName(of: program)
        guard let gameID = gatedExecutables[image] else { return nil }
        if let db, let k = key(gameID), [.playable, .playableCaveats].contains(db.verdict(for: k).result) { return nil }
        let game = db?.games[gameID]
        return Block(
            gameID: gameID, image: image,
            title: game?.name(for: ["zh-Hans"]) ?? image,
            reason: "Cider 尚未完成这款游戏所需的 Windows 内核兼容验证，目前主动拦截本地启动。下载完成不会解除这个限制；安装状态与本地运行支持分别判断。",
            routes: game?.routes ?? [])
    }
}

extension GameEntry {
    /// Where else the game can be played (docs/plan/00 ADR-010: only official clouds; never an iOS form).
    public struct Route: Codable, Sendable, Equatable {
        public enum Form: String, Codable, Sendable { case web, windowsCloudClient = "windows-cloud-client", nativeMacOS = "native-macos" }
        public var kind: String            // official-cloud
        public var form: Form
        public var name: [String: String]
        public var url: String
    }
}
