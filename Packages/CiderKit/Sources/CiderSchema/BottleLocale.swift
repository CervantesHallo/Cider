import Foundation

/// Locale of the Windows side of a bottle. Independent of the Cider UI language (docs/plan/05 §12).
/// Wine derives the ANSI code page from LC_ALL/LANG, so a full `xx_YY.UTF-8` is always set;
/// a bare `LC_CTYPE=UTF-8` would silently fall back to en-US / 1252 (research 09 §4.3).
public struct BottleLocale: Codable, Equatable, Sendable {
    public var ui: String
    public var lcAll: String
    public var acp: Int

    enum CodingKeys: String, CodingKey {
        case ui
        case lcAll = "LC_ALL"
        case acp
    }

    public init(ui: String, lcAll: String, acp: Int) {
        self.ui = ui
        self.lcAll = lcAll
        self.acp = acp
    }

    public static let japanese = BottleLocale(ui: "ja", lcAll: "ja_JP.UTF-8", acp: 932)
    public static let simplifiedChinese = BottleLocale(ui: "zh-Hans", lcAll: "zh_CN.UTF-8", acp: 936)
    public static let traditionalChinese = BottleLocale(ui: "zh-Hant", lcAll: "zh_TW.UTF-8", acp: 950)
    public static let korean = BottleLocale(ui: "ko", lcAll: "ko_KR.UTF-8", acp: 949)
    public static let english = BottleLocale(ui: "en", lcAll: "en_US.UTF-8", acp: 1252)
    public static let all: [BottleLocale] = [.simplifiedChinese, .japanese, .traditionalChinese, .korean, .english]

    /// Name shown in the UI, in the locale's own language.
    public var displayName: String {
        switch ui {
        case "zh-Hans": return "简体中文"
        case "zh-Hant": return "繁體中文"
        case "ja": return "日本語"
        case "ko": return "한국어"
        case "en": return "English"
        default: return lcAll
        }
    }

    /// Accepts the short names used by ciderctl and the UI.
    public static func named(_ name: String) -> BottleLocale? {
        switch name.lowercased() {
        case "ja", "ja_jp", "japanese", "932": return .japanese
        case "zh", "zh-hans", "zh_cn", "chs", "936": return .simplifiedChinese
        case "zh-hant", "zh_tw", "cht", "950": return .traditionalChinese
        case "ko", "ko_kr", "949": return .korean
        case "en", "en_us", "1252": return .english
        default: return nil
        }
    }

    /// Environment that makes Wine pick this locale. Applied last so nothing inherited can override it.
    public var environment: [String: String] {
        ["LANG": lcAll, "LC_ALL": lcAll]
    }
}
