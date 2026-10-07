import CiderCore
import CiderRuntime
import CiderStore
import Darwin
import Foundation

/// Support bundle (docs/plan/05 §15 row 48, `.ciderlog`): a zip with the bottle's recent session logs, its
/// config, the engine manifest, the Mac's OS/hardware and the running Windows processes. Reads are bounded;
/// personal paths and common credential fields are redacted before writing any diagnostic content.
public enum Diagnostics {
    @discardableResult
    public static func bundle(for bottle: Bottle, store: BottleStore, recentSessions: Int = 5, to directory: URL) throws -> URL {
        try Task.checkCancellation()
        let fm = FileManager.default
        let stamp = Identifiers.compactTimestamp()
        let name = "Cider-\(bottle.config.id)-\(stamp)"
        let staging = fm.temporaryDirectory.appendingPathComponent("cider-diagnostics-\(UUID().uuidString)", isDirectory: true)
        try fm.createDirectory(at: staging, withIntermediateDirectories: false, attributes: [.posixPermissions: 0o700])
        defer { try? fm.removeItem(at: staging) }

        // Sessions, newest first.
        let sessionsRoot = store.paths.logs.appendingPathComponent("sessions/\(bottle.config.id)", isDirectory: true)
        let sessions = ((try? fm.contentsOfDirectory(atPath: sessionsRoot.path)) ?? []).sorted(by: >).prefix(max(0, recentSessions))
        for (index, session) in sessions.enumerated() {
            try Task.checkCancellation()
            let src = sessionsRoot.appendingPathComponent(session, isDirectory: true)
            // Session labels can contain personal information too; archive only their chronological index.
            let dst = staging.appendingPathComponent("sessions/session-\(index + 1)", isDirectory: true)
            try fm.ensureDirectory(dst)
            for file in ["wine.log", "session.json"] {
                let from = src.appendingPathComponent(file)
                try Task.checkCancellation()
                let data = try readBounded(from: from, keepBothEnds: file == "wine.log")
                try redact(data, structuredJSON: file != "wine.log").write(to: dst.appendingPathComponent(file))
            }
        }

        try Task.checkCancellation()
        let config = try readBounded(from: bottle.configURL, keepBothEnds: false)
        try redact(config, structuredJSON: true).write(to: staging.appendingPathComponent("cider-bottle.json"))
        // Installed engines live at Engines/<id>. Avoid EngineStore.list()/JSONFile.read here: those
        // would load every engine's entire manifest before this reader gets a chance to bound it.
        let engineID = bottle.config.engine.id
        if !engineID.isEmpty, engineID != ".", engineID != "..",
           !engineID.contains("/"), !engineID.contains("\\"), !engineID.contains(":"),
           !engineID.unicodeScalars.contains(where: { CharacterSet.controlCharacters.contains($0) }) {
            let manifest = store.paths.engines.appendingPathComponent(engineID, isDirectory: true).appendingPathComponent("manifest.json")
            let data = try readBounded(from: manifest, keepBothEnds: false)
            try redact(data, structuredJSON: true).write(to: staging.appendingPathComponent("engine-manifest.json"))
        }

        var system: [String] = []
        let os = ProcessInfo.processInfo.operatingSystemVersion
        system.append("macOS \(os.majorVersion).\(os.minorVersion).\(os.patchVersion)")
        system.append("chip: \((try? Command.run("/usr/sbin/sysctl", ["-n", "machdep.cpu.brand_string"]))?.trimmingCharacters(in: .whitespacesAndNewlines) ?? "?")")
        system.append("memory: \(ProcessInfo.processInfo.physicalMemory / 1_073_741_824) GB")
        system.append("rosetta: \((try? Command.run("/usr/bin/arch", ["-x86_64", "/usr/bin/true"])) != nil ? "yes" : "no")")
        system.append("gstreamer: \(fm.fileExists(atPath: WineRunner.gstreamerFramework) ? "yes" : "no")")
        system.append("")
        system.append("windows processes:")
        for p in ProcessScanner.scan() where p.bottleID == bottle.config.id {
            system.append("  \(p.pid)  \(p.windowsImage)")
        }
        try redact(Data(system.joined(separator: "\n").utf8)).write(to: staging.appendingPathComponent("system.txt"))

        try Task.checkCancellation()
        try fm.ensureDirectory(directory)
        let zip = directory.appendingPathComponent("\(name).zip")
        try Command.run("/usr/bin/ditto", ["-c", "-k", "--norsrc", "--noextattr", "--keepParent", staging.path, zip.path])
        return zip
    }

    /// At most `limit` source bytes are read, regardless of file size. Large JSON documents are omitted
    /// entirely: fragments can lose the key that tells us their value is a credential.
    static func readBounded(from url: URL, limit: Int = 4 << 20, keepBothEnds: Bool = true) throws -> Data {
        try Task.checkCancellation()
        guard limit > 0 else { throw CiderError.invalid("诊断读取上限必须大于零") }
        let fd = open(url.path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)
        guard fd >= 0 else {
            if errno == ENOENT { return Data("[diagnostic file unavailable]\n".utf8) }
            throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
        }
        let handle = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        defer { try? handle.close() }
        var info = stat()
        guard fstat(fd, &info) == 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        guard info.st_mode & mode_t(S_IFMT) == mode_t(S_IFREG) else {
            throw CiderError.invalid("诊断输入必须是普通文件")
        }
        let size = try handle.seekToEnd()
        func read(_ count: Int) throws -> Data {
            var result = Data()
            while result.count < count {
                try Task.checkCancellation()
                guard let chunk = try handle.read(upToCount: min(count - result.count, 64 << 10)), !chunk.isEmpty else { break }
                result.append(chunk)
            }
            return result
        }
        if size <= UInt64(limit) {
            try handle.seek(toOffset: 0)
            return try read(Int(size))
        }
        guard keepBothEnds else { return Data("[oversized diagnostic JSON omitted]\n".utf8) }
        let headCount = limit / 2
        let tailCount = limit - headCount
        try handle.seek(toOffset: 0)
        let head = try read(headCount)
        // Joining UTF-16/binary fragments with a UTF-8 marker loses their decoding boundaries.
        guard !head.contains(0), !head.starts(with: [0xFF, 0xFE]), !head.starts(with: [0xFE, 0xFF]) else {
            return Data("[large non-UTF-8 diagnostic log omitted]\n".utf8)
        }
        try handle.seek(toOffset: size - UInt64(tailCount))
        let tail = try read(tailCount)
        // Cut on complete lines. A tail beginning inside a secret value has lost its field/argument label.
        let safeHead = head.lastIndex(of: 10).map { Data(head.prefix(through: $0)) } ?? Data()
        let safeTail = tail.firstIndex(of: 10).map { Data(tail.suffix(from: $0 + 1)) } ?? Data()
        let marker = Data("\n… [\(size - UInt64(safeHead.count + safeTail.count)) bytes omitted] …\n".utf8)
        return safeHead + marker + safeTail
    }

    static func truncated(_ data: Data, limit: Int = 4 << 20) -> Data {
        guard data.count > limit else { return data }
        let half = limit / 2
        return data.prefix(half) + Data("\n\n… [\(data.count - limit) bytes omitted] …\n\n".utf8) + data.suffix(half)
    }

    /// Never returns undecoded input. JSON credentials and separate argv flag/value pairs are redacted
    /// structurally; a text fallback handles malformed JSON, log lines, URLs and common authorization forms.
    static func redact(_ data: Data, home: String = NSHomeDirectory(), user: String = NSUserName(), structuredJSON: Bool = false) -> Data {
        let text: String
        if data.starts(with: [0xFF, 0xFE]) {
            guard let decoded = String(data: data.dropFirst(2), encoding: .utf16LittleEndian) else { return omittedText }
            text = decoded
        } else if data.starts(with: [0xFE, 0xFF]) {
            guard let decoded = String(data: data.dropFirst(2), encoding: .utf16BigEndian) else { return omittedText }
            text = decoded
        } else {
            // Lossy UTF-8 decoding replaces bad/truncated code units without bypassing the scrubber.
            // NUL-bearing input may be an unknown binary/UTF-16 format; omit it rather than guessing.
            guard !data.contains(0) else { return omittedText }
            text = String(decoding: data, as: UTF8.self)
        }
        guard !text.contains("\0") else { return omittedText }
        if let object = try? JSONSerialization.jsonObject(with: Data(text.utf8), options: [.fragmentsAllowed]),
           let encoded = try? JSONSerialization.data(withJSONObject: redactJSON(object, home: home, user: user, depth: 0),
                                                      options: [.fragmentsAllowed, .prettyPrinted, .sortedKeys, .withoutEscapingSlashes]) {
            return encoded
        }
        if structuredJSON { return Data("[unavailable, oversized or malformed diagnostic JSON omitted]\n".utf8) }
        return Data(redactText(text, home: home, user: user).utf8)
    }

    private static var omittedText: Data { Data("[undecodable diagnostic content omitted]\n".utf8) }

    private static func sensitiveKey(_ key: String) -> Bool {
        let normalized = key.lowercased().filter { $0.isASCII && ($0.isLetter || $0.isNumber) }
        return ["authorization", "proxyauthorization", "auth", "authentication", "credential", "credentials", "cookie",
                "setcookie", "session", "sessionid", "sessionkey", "csrftoken", "csrf", "jwt", "bearer", "pwd", "passwd",
                "authcode", "authorizationcode", "nonce", "ticket", "user", "username", "account", "accountid",
                "userid", "uid", "email", "login", "logname"].contains(normalized)
            || normalized.contains("token") || normalized.contains("password") || normalized.contains("secret")
            || normalized.hasSuffix("apikey") || normalized.hasSuffix("accesskey") || normalized.hasSuffix("privatekey")
            || normalized.hasSuffix("cookie") || normalized.hasSuffix("credentials")
    }

    private static func sensitiveArgument(_ value: String) -> Bool {
        let flag = value.trimmingCharacters(in: .whitespacesAndNewlines)
        guard flag.hasPrefix("-") || flag.hasPrefix("/"), !flag.contains("="), !flag.contains(":") else { return false }
        return sensitiveKey(String(flag.drop(while: { $0 == "-" || $0 == "/" })))
    }

    private static func redactJSON(_ object: Any, home: String, user: String, depth: Int) -> Any {
        guard depth < 32 else { return "<nested diagnostic content omitted>" }
        if let fields = object as? [String: Any] {
            return fields.reduce(into: [String: Any]()) { result, entry in
                result[entry.key] = sensitiveKey(entry.key) ? "<redacted>" : redactJSON(entry.value, home: home, user: user, depth: depth + 1)
            }
        }
        if let values = object as? [Any] {
            var nextIsSecret = false
            return values.map { value -> Any in
                if nextIsSecret { nextIsSecret = false; return "<redacted>" }
                if let string = value as? String { nextIsSecret = sensitiveArgument(string) }
                return redactJSON(value, home: home, user: user, depth: depth + 1)
            }
        }
        if let string = object as? String { return redactText(string, home: home, user: user) }
        return object
    }

    private static let urlExpression = try? NSRegularExpression(pattern: #"(?i)\b[a-z][a-z0-9+.-]*://[^\s<>\"']+"#)
    private static let secretKeyPattern = #"(?:[a-z0-9_-]*token[a-z0-9_-]*|[a-z0-9_-]*password[a-z0-9_-]*|[a-z0-9_-]*secret[a-z0-9_-]*|api[_-]?key|access[_-]?key|private[_-]?key|authorization|proxy[_-]?authorization|authentication|auth(?:[_-]?code)?|authorization[_-]?code|credentials?|cookie|set[_-]?cookie|session(?:[_-]?(?:id|key))?|csrf|jwt|pwd|passwd|nonce|ticket|username|account(?:[_-]?id)?|user[_-]?id|email|login|logname)"#
    private static let textRules: [(expression: NSRegularExpression?, replacement: String)] = [
        // Header values can contain spaces and separators: omit the entire remainder of their line.
        (try? NSRegularExpression(pattern: #"(?im)((?:authorization|proxy-authorization|cookie|set-cookie)[\"']?\s*[:=]\s*)[^\r\n]*"#), "$1<redacted>"),
        (try? NSRegularExpression(pattern: #"(?i)\b(?:bearer|basic)[ \t]+[a-z0-9._~+/=-]+"#), "<redacted authorization>"),
        (try? NSRegularExpression(pattern: #"(?i)\b[a-z][a-z0-9+.-]*%3a(?:%2f){2}[^\s<>\"']+"#), "<redacted encoded URL>"),
        (try? NSRegularExpression(pattern: #"(?i)([?&;][a-z0-9_.%~-]+=)[^&;\s#\"'<>]*"#), "$1<redacted>"),
        (try? NSRegularExpression(pattern: #"(?i)("# + secretKeyPattern + #"(?:%3d|%3a))[^\s\"'<>]+"#), "$1<redacted>"),
        (try? NSRegularExpression(pattern: #"(?i)([\"']?"# + secretKeyPattern + #"[\"']?\s*[:=]\s*)(?:\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'|[^\s,;&}\]\"']+)"#), "$1<redacted>"),
        (try? NSRegularExpression(pattern: #"(?i)((?:--?|/)"# + secretKeyPattern + #"[ \t]+)(?:\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'|\S+)"#), "$1<redacted>"),
        (try? NSRegularExpression(pattern: #"\beyJ[a-zA-Z0-9_-]+\.[a-zA-Z0-9_-]+\.[a-zA-Z0-9_-]+\b"#), "<redacted JWT>"),
        (try? NSRegularExpression(pattern: #"(?i)(/users/|\\{1,2}users\\{1,2})[^/\\\s\"'<>]+"#), "$1<user>"),
    ]

    private static func redactText(_ input: String, home: String, user: String) -> String {
        // Keep percent escapes intact: decoding %0A before recognizing an URL could expose the rest of a
        // credential as a new line. JSON escapes are decoded structurally; normalize escaped URL slashes.
        var text = input.replacingOccurrences(of: #"\/"#, with: "/")
        guard let urlExpression else { return "[diagnostic redaction unavailable]" }
        let original = text as NSString
        let mutable = NSMutableString(string: text)
        for match in urlExpression.matches(in: text, range: NSRange(location: 0, length: original.length)).reversed() {
            let raw = original.substring(with: match.range)
            var replacement = "<redacted URL>"
            if var url = URLComponents(string: raw) {
                url.user = nil
                url.password = nil
                url.query = nil
                url.fragment = nil
                replacement = url.string ?? replacement
            }
            mutable.replaceCharacters(in: match.range, with: replacement)
        }
        text = mutable as String
        if !home.isEmpty { text = text.replacingOccurrences(of: home, with: "~") }
        if user.count >= 3 { text = text.replacingOccurrences(of: user, with: "<user>") }
        for rule in textRules {
            guard let expression = rule.expression else { return "[diagnostic redaction unavailable]" }
            text = expression.stringByReplacingMatches(in: text, range: NSRange(text.startIndex..., in: text), withTemplate: rule.replacement)
        }
        return text
    }
}
