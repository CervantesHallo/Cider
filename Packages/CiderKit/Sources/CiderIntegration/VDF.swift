import Foundation

/// Valve KeyValues ("VDF") text format, as used by libraryfolders.vdf and appmanifest_*.acf.
/// Keys may repeat in the wild; the last value wins for lookups, but `entries` keeps them all.
public indirect enum VDF: Equatable, Sendable {
    case string(String)
    case object([(key: String, value: VDF)])

    public static func == (lhs: VDF, rhs: VDF) -> Bool {
        switch (lhs, rhs) {
        case let (.string(a), .string(b)): return a == b
        case let (.object(a), .object(b)):
            return a.count == b.count && zip(a, b).allSatisfy { $0.key == $1.key && $0.value == $1.value }
        default: return false
        }
    }

    public var string: String? { if case .string(let s) = self { return s }; return nil }

    public var entries: [(key: String, value: VDF)] { if case .object(let e) = self { return e }; return [] }

    /// Case-insensitive lookup (Valve files are inconsistent about key case).
    public subscript(key: String) -> VDF? {
        entries.last { $0.key.caseInsensitiveCompare(key) == .orderedSame }?.value
    }

    public static func parse(_ text: String) throws -> VDF {
        var parser = Parser(scalars: Array(text.unicodeScalars))
        return .object(try parser.parseEntries(topLevel: true))
    }

    public struct ParseError: Error, CustomStringConvertible {
        public let message: String
        public let offset: Int
        public var description: String { "VDF parse error at \(offset): \(message)" }
    }

    private struct Parser {
        let scalars: [Unicode.Scalar]
        var i = 0

        init(scalars: [Unicode.Scalar]) { self.scalars = scalars }

        mutating func parseEntries(topLevel: Bool) throws -> [(key: String, value: VDF)] {
            var result: [(key: String, value: VDF)] = []
            while true {
                skipTrivia()
                guard i < scalars.count else {
                    if topLevel { return result }
                    throw ParseError(message: "unexpected end of input, missing }", offset: i)
                }
                if scalars[i] == "}" {
                    if topLevel { throw ParseError(message: "unbalanced }", offset: i) }
                    i += 1
                    return result
                }
                let key = try token()
                skipTrivia()
                guard i < scalars.count else { throw ParseError(message: "missing value for \(key)", offset: i) }
                if scalars[i] == "{" {
                    i += 1
                    result.append((key, .object(try parseEntries(topLevel: false))))
                } else {
                    result.append((key, .string(try token())))
                }
            }
        }

        mutating func token() throws -> String {
            if scalars[i] == "\"" {
                i += 1
                var out = String.UnicodeScalarView()
                while i < scalars.count {
                    let c = scalars[i]
                    if c == "\"" { i += 1; return String(out) }
                    if c == "\\", i + 1 < scalars.count {
                        i += 1
                        switch scalars[i] {
                        case "n": out.append("\n")
                        case "t": out.append("\t")
                        default: out.append(scalars[i])
                        }
                    } else {
                        out.append(c)
                    }
                    i += 1
                }
                throw ParseError(message: "unterminated string", offset: i)
            }
            // Unquoted token: runs until whitespace or a brace.
            let start = i
            while i < scalars.count, !CharacterSet.whitespacesAndNewlines.contains(scalars[i]), scalars[i] != "{", scalars[i] != "}" {
                i += 1
            }
            guard i > start else { throw ParseError(message: "expected a token", offset: i) }
            return String(String.UnicodeScalarView(scalars[start..<i]))
        }

        mutating func skipTrivia() {
            while i < scalars.count {
                if CharacterSet.whitespacesAndNewlines.contains(scalars[i]) {
                    i += 1
                } else if scalars[i] == "/", i + 1 < scalars.count, scalars[i + 1] == "/" {
                    while i < scalars.count, scalars[i] != "\n" { i += 1 }
                } else {
                    return
                }
            }
        }
    }
}
