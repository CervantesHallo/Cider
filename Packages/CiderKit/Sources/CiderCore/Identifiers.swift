import Foundation

public enum Identifiers {
    /// A filesystem-safe id: ASCII slug of `name` plus a UUID suffix.
    /// Names with no ASCII letters (e.g. 日文 galgame) fall back to `bottle-<uuid>`.
    public static func make(from name: String, fallback: String = "bottle") -> String {
        let lowered = name.lowercased()
        var slug = ""
        var lastWasDash = false
        for scalar in lowered.unicodeScalars {
            if scalar.isASCII, CharacterSet.alphanumerics.contains(scalar) {
                slug.unicodeScalars.append(scalar)
                lastWasDash = false
            } else if !lastWasDash, !slug.isEmpty {
                slug.append("-")
                lastWasDash = true
            }
        }
        while slug.hasSuffix("-") { slug.removeLast() }
        if slug.count > 24 { slug = String(slug.prefix(24)) }
        if slug.isEmpty { slug = fallback }
        return "\(slug)-\(UUID().uuidString.lowercased())"
    }

    /// RFC 3339 timestamp in UTC.
    public static func timestamp(_ date: Date = Date()) -> String {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime]
        return f.string(from: date)
    }

    /// Compact timestamp for directory names: 20261003T120000.
    public static func compactTimestamp(_ date: Date = Date()) -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.timeZone = TimeZone(identifier: "UTC")
        f.dateFormat = "yyyyMMdd'T'HHmmss"
        return f.string(from: date)
    }
}
