import CiderCore
import CiderRuntime
import CiderSchema
import Foundation

/// A registry value written through a `.reg` import.
public enum RegValue: Equatable, Sendable {
    case string(String)
    case multiString([String])
    case dword(UInt32)
    /// Removes the value (`"name"=-` in .reg syntax).
    case delete
}

/// Prefix adjustments applied at bottle creation. Each revision's steps are idempotent, and existing bottles
/// are brought up to `revision` by `BottleStore.upgradePrefix` (recorded in settings["prefixRevision"]).
public enum PrefixSetup {
    /// 1: shell folders + font replacements. 2: dialog fonts + CJK SystemLink fallbacks.
    public static let revision = 2
    public static let revisionKey = "prefixRevision"

    /// Font substitutions so games asking for Windows CJK fonts get the macOS system equivalents
    /// (research 09 §4.2). Both the English and the localized family names are mapped, because Japanese
    /// games (KiriKiri titles included) usually request e.g. "ＭＳ ゴシック" rather than "MS Gothic".
    public static let fontReplacements: [(key: String, values: [(String, String)])] = [
        (#"HKEY_CURRENT_USER\Software\Wine\Fonts\Replacements"#, [
            // Japanese
            ("MS Gothic", "Hiragino Sans"), ("MS PGothic", "Hiragino Sans"), ("MS UI Gothic", "Hiragino Sans"),
            ("ＭＳ ゴシック", "Hiragino Sans"), ("ＭＳ Ｐゴシック", "Hiragino Sans"),
            ("MS Mincho", "Hiragino Mincho ProN"), ("MS PMincho", "Hiragino Mincho ProN"),
            ("ＭＳ 明朝", "Hiragino Mincho ProN"), ("ＭＳ Ｐ明朝", "Hiragino Mincho ProN"),
            ("Meiryo", "Hiragino Sans"), ("メイリオ", "Hiragino Sans"), ("Meiryo UI", "Hiragino Sans"),
            ("Yu Gothic", "Hiragino Sans"), ("游ゴシック", "Hiragino Sans"),
            ("Yu Mincho", "Hiragino Mincho ProN"), ("游明朝", "Hiragino Mincho ProN"),
            // Simplified Chinese
            ("SimSun", "Songti SC"), ("宋体", "Songti SC"), ("NSimSun", "Songti SC"), ("新宋体", "Songti SC"),
            ("SimHei", "PingFang SC"), ("黑体", "PingFang SC"),
            ("Microsoft YaHei", "PingFang SC"), ("微软雅黑", "PingFang SC"), ("Microsoft YaHei UI", "PingFang SC"),
            ("DengXian", "PingFang SC"), ("等线", "PingFang SC"),
            // Traditional Chinese
            ("MingLiU", "PingFang TC"), ("細明體", "PingFang TC"), ("PMingLiU", "PingFang TC"), ("新細明體", "PingFang TC"),
            ("Microsoft JhengHei", "PingFang TC"), ("微軟正黑體", "PingFang TC"), ("Microsoft JhengHei UI", "PingFang TC"),
            // Korean
            ("Gulim", "Apple SD Gothic Neo"), ("굴림", "Apple SD Gothic Neo"),
            ("Malgun Gothic", "Apple SD Gothic Neo"), ("맑은 고딕", "Apple SD Gothic Neo"),
        ]),
    ]

    /// Dialog fonts and fallbacks (revision 2). Windows programs draw most UI text in "MS Shell Dlg" or a Latin
    /// face such as Tahoma or Segoe UI; on Windows those fall back to the locale's CJK font through FontLink.
    /// Without SystemLink entries Wine has nothing to fall back to, and text in e.g. Steam's bootstrapper
    /// dialog simply disappears. The dialog font becomes the locale's sans UI font (→ PingFang / Hiragino via
    /// the replacements above) instead of SimSun's serif Songti.
    public static func uiFonts(for locale: BottleLocale) -> [(key: String, values: [(String, RegValue)])] {
        let sc = "Hiragino Sans GB.ttc,Hiragino Sans GB"
        let tc = "STHeiti Medium.ttc,Heiti TC"
        let jp = "ヒラギノ角ゴシック W3.ttc,Hiragino Sans"
        let kr = "AppleSDGothicNeo.ttc,Apple SD Gothic Neo"
        let (dialog, fallbacks): (String?, [String]) = switch locale.ui {
        case "zh-Hans": ("Microsoft YaHei UI", [sc, jp, tc, kr])
        case "zh-Hant": ("Microsoft JhengHei UI", [tc, sc, jp, kr])
        case "ja": ("MS UI Gothic", [jp, sc, tc, kr])
        case "ko": ("Malgun Gothic", [kr, jp, sc, tc])
        default: (nil, [jp, sc, tc, kr])
        }
        let latinFaces = ["Tahoma", "Segoe UI", "Microsoft Sans Serif", "MS Sans Serif", "Arial", "Verdana",
                          "Lucida Sans Unicode", "Trebuchet MS", "Times New Roman", "Courier New"]
        var entries: [(key: String, values: [(String, RegValue)])] = [
            (#"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontLink\SystemLink"#,
             latinFaces.map { ($0, .multiString(fallbacks)) }),
        ]
        if let dialog {
            entries.append((#"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes"#,
                            [("MS Shell Dlg", .string(dialog)), ("MS Shell Dlg 2", .string(dialog))]))
        }
        return entries
    }

    /// CrossOver's "High Resolution Mode": Wine renders at the display's full pixel density (RetinaMode) and
    /// reports 192 DPI so DPI-aware programs scale their UI; off restores 1x rendering at 96 DPI.
    public static func highResolution(_ on: Bool) -> [(key: String, values: [(String, RegValue)])] {
        let dpi: UInt32 = on ? 192 : 96
        return [
            (#"HKEY_CURRENT_USER\Software\Wine\Mac Driver"#, [("RetinaMode", .string(on ? "y" : "n"))]),
            (#"HKEY_CURRENT_USER\Control Panel\Desktop"#, [("LogPixels", .dword(dpi))]),
            (#"HKEY_LOCAL_MACHINE\System\CurrentControlSet\Hardware Profiles\Current\Software\Fonts"#, [("LogPixels", .dword(dpi))]),
        ]
    }

    /// Brings a prefix from revision `from` to `revision`. Every step can be rerun safely.
    public static func upgrade(bottle: Bottle, runner: WineRunner, from: Int) throws {
        if from < 2 {
            try importRegistry(fontReplacements, named: "fonts", bottle: bottle, runner: runner)
            try importRegistry(uiFonts(for: bottle.config.locale), named: "ui-fonts", bottle: bottle, runner: runner)
        }
    }

    /// Replaces Wine's links from the Windows profile folders (Desktop, Documents, Downloads, Music, Pictures,
    /// Videos) to the Mac home folders with plain directories, so Windows programs stay out of ~/Documents etc.
    /// and don't trigger macOS privacy prompts (docs/plan/01 §9).
    public static func isolateShellFolders(in driveC: URL) throws {
        let fm = FileManager.default
        let users = driveC.appendingPathComponent("users", isDirectory: true)
        guard let profiles = try? fm.contentsOfDirectory(at: users, includingPropertiesForKeys: nil) else { return }
        for profile in profiles {
            guard let items = try? fm.contentsOfDirectory(at: profile, includingPropertiesForKeys: [.isSymbolicLinkKey]) else { continue }
            for item in items where (try? item.resourceValues(forKeys: [.isSymbolicLinkKey]).isSymbolicLink) == true {
                try fm.removeItem(at: item)
                try fm.createDirectory(at: item, withIntermediateDirectories: false)
            }
        }
    }

    public static func importRegistry(_ entries: [(key: String, values: [(String, String)])], named name: String,
                                      bottle: Bottle, runner: WineRunner) throws {
        try importRegistry(entries.map { ($0.key, $0.values.map { ($0.0, RegValue.string($0.1)) }) }, named: name,
                           bottle: bottle, runner: runner)
    }

    /// Writes a UTF-16LE `.reg` file under C:\cider\ and imports it with `regedit /S`.
    public static func importRegistry(_ entries: [(key: String, values: [(String, RegValue)])], named name: String,
                                      bottle: Bottle, runner: WineRunner) throws {
        let dir = bottle.driveC.appendingPathComponent("cider", isDirectory: true)
        try FileManager.default.ensureDirectory(dir)
        let file = dir.appendingPathComponent("\(name).reg")
        try regFile(entries).write(to: file)
        let result = try runner.runToCompletion(runner.plan(program: "regedit", arguments: ["/S", #"C:\cider\\#(name).reg"#], label: "regedit-\(name)"))
        guard result.code == 0 else {
            throw CiderError.commandFailed(command: "regedit /S \(name).reg", status: result.code, output: "")
        }
    }

    static func regFile(_ entries: [(key: String, values: [(String, RegValue)])]) -> Data {
        func quote(_ s: String) -> String {
            "\"" + s.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"") + "\""
        }
        func encode(_ value: RegValue) -> String {
            switch value {
            case .string(let s): return quote(s)
            case .multiString(let list):
                // hex(7): UTF-16LE strings, each NUL-terminated, plus a final NUL.
                var bytes: [UInt8] = []
                for s in list { bytes += Array(s.data(using: .utf16LittleEndian)!) + [0, 0] }
                bytes += [0, 0]
                return "hex(7):" + bytes.map { String(format: "%02x", $0) }.joined(separator: ",")
            case .dword(let v): return String(format: "dword:%08x", v)
            case .delete: return "-"
            }
        }
        var lines = ["Windows Registry Editor Version 5.00", ""]
        for entry in entries {
            lines.append("[\(entry.key)]")
            for (name, value) in entry.values { lines.append("\(quote(name))=\(encode(value))") }
            lines.append("")
        }
        var data = Data([0xFF, 0xFE])
        data.append(lines.joined(separator: "\r\n").data(using: .utf16LittleEndian)!)
        return data
    }
}
