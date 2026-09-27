import Foundation

public enum Identifiers {
    /// A filesystem-safe id: ASCII slug of `name` plus a short random suffix, e.g. `steam-7f3a`.
    /// Names with no ASCII letters (e.g. 日文 galgame) fall back to `bottle-xxxx`.
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
        let suffix = String(UInt16.random(in: 0...UInt16.max), radix: 16)
        return "\(slug)-\(String(repeating: "0", count: 4 - suffix.count))\(suffix)"
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
