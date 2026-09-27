import SwiftUI

/// Colors and metrics from the approved design (https://claude.ai/artifact/CX8EF8Q8pG6EXzbHWxtdJk).
enum Theme {
    static let window = Color(hex: 0x1A1816)
    static let sidebar = Color(hex: 0x201D1A)
    static let card = Color(hex: 0x26231F)
    static let raised = Color(hex: 0x2A2622)
    static let selected = Color(hex: 0x3A332B)
    static let selectedBorder = Color(hex: 0x5A4A36)
    static let line = Color(hex: 0x2E2A26)
    static let cardLine = Color(hex: 0x33302B)
    static let control = Color(hex: 0x45403A)

    static let text = Color(hex: 0xF3EEE7)
    static let textSecondary = Color(hex: 0xB9B0A4)
    static let textTertiary = Color(hex: 0x9A9185)
    static let textSidebar = Color(hex: 0xCFC6BA)

    static let accent = Color(hex: 0xE8A33D)
    static let onAccent = Color(hex: 0x1B140A)

    static let good = Color(hex: 0x7FD8C3)
    static let ok = Color(hex: 0xA9C4FF)
    static let tune = Color(hex: 0xF2BE6E)
    static let bad = Color(hex: 0xF09A92)

    /// Placeholder poster colors, picked deterministically per title until real artwork loads.
    static let posterPalette: [(Color, Color)] = [
        (Color(hex: 0x46304C), Color(hex: 0xEBD9EE)), (Color(hex: 0x2E3D52), Color(hex: 0xD6E2F2)),
        (Color(hex: 0x4A3A2A), Color(hex: 0xF1E0C9)), (Color(hex: 0x2F4640), Color(hex: 0xD3EAE2)),
        (Color(hex: 0x23303A), Color(hex: 0xCFE0EC)), (Color(hex: 0x3A2F2A), Color(hex: 0xEEDCCF)),
    ]

    static func poster(for key: String) -> (Color, Color) {
        posterPalette[abs(key.unicodeScalars.reduce(0) { $0 &* 31 &+ Int($1.value) }) % posterPalette.count]
    }

    /// Display serif for titles (Fraunces in the mockup; New York is the closest system face).
    static func display(_ size: CGFloat, weight: Font.Weight = .semibold) -> Font {
        .system(size: size, weight: weight, design: .serif)
    }
}

extension Color {
    init(hex: UInt32) {
        self.init(.sRGB,
                  red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255,
                  opacity: 1)
    }
}

/// Compatibility verdict shown as a chip (docs/plan/06; statuses match the design).
enum Verdict: String, Sendable {
    case good, ok, tune, unsupported, unknown

    var label: String {
        switch self {
        case .good: return "运行良好"
        case .ok: return "可运行"
        case .tune: return "需调整"
        case .unsupported: return "不支持"
        case .unknown: return "未验证"
        }
    }

    var color: Color {
        switch self {
        case .good: return Theme.good
        case .ok: return Theme.ok
        case .tune: return Theme.tune
        case .unsupported: return Theme.bad
        case .unknown: return Theme.textTertiary
        }
    }

    var symbol: String {
        switch self {
        case .good: return "checkmark"
        case .ok: return "info"
        case .tune: return "exclamationmark"
        case .unsupported: return "xmark"
        case .unknown: return "questionmark"
        }
    }
}
