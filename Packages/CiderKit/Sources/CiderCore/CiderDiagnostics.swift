import Foundation

/// Optional per-operation diagnostics. CLI work hosted by the GUI streams warnings
/// through its own sink; normal GUI actions retain their existing stderr behavior.
public enum CiderDiagnostics {
    @TaskLocal public static var warningSink: (@Sendable (String) -> Void)?
    public static func warning(_ text: String) {
        if let sink = warningSink { sink(text) }
        else { FileHandle.standardError.write(Data((text + "\n").utf8)) }
    }
}
