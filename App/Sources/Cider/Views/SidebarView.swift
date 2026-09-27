import SwiftUI

struct SidebarView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        VStack(alignment: .leading, spacing: 22) {
            HStack(spacing: 10) {
                Image(systemName: "apple.meditate")
                    .hidden()
                    .overlay { CiderMark().stroke(Theme.accent, style: StrokeStyle(lineWidth: 1.8, lineCap: .round, lineJoin: .round)) }
                    .frame(width: 26, height: 26)
                Text("Cider").font(Theme.display(25))
            }
            .padding(.horizontal, 8)
            .padding(.top, 44)  // leaves room for the traffic lights of the hidden title bar

            VStack(spacing: 2) {
                ForEach(SidebarSection.allCases) { section in
                    let selected = model.section == section
                    Button {
                        model.section = section
                        if section == .library { model.detail = nil }
                    } label: {
                        HStack(spacing: 10) {
                            Image(systemName: section.symbol).frame(width: 18)
                            Text(section.title)
                            Spacer()
                            if section == .library, !model.games.isEmpty {
                                Text("\(model.games.count)").font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                            }
                        }
                        .font(.system(size: 14))
                        .foregroundStyle(selected ? .white : Theme.textSidebar)
                        .padding(.horizontal, 10)
                        .frame(height: 36)
                        .background(selected ? Theme.selected : .clear, in: RoundedRectangle(cornerRadius: 9))
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityAddTraits(selected ? .isSelected : [])
                }
            }

            Spacer()

            VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 8) {
                    Circle().fill(model.environment.ready ? Theme.good : Theme.tune).frame(width: 8, height: 8)
                    Text(model.environment.ready ? "运行环境已就绪" : "运行环境未就绪").font(.system(size: 12, weight: .semibold))
                }
                Text(environmentSummary).font(.system(size: 12)).foregroundStyle(Theme.textSecondary).lineSpacing(3)
                Button("查看环境检查") { model.section = .environment }
                    .buttonStyle(.plain)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(Theme.accent)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 12)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Theme.raised, in: RoundedRectangle(cornerRadius: 12))
        }
        .padding(.horizontal, 12)
        .padding(.bottom, 16)
        .frame(width: 240)
        .frame(maxHeight: .infinity)
        .background(Theme.sidebar)
        .overlay(alignment: .trailing) { Rectangle().fill(Theme.line).frame(width: 1) }
    }

    private var environmentSummary: String {
        let engineID = model.bottles.first?.config.engine.id ?? model.environment.engines.last
        let engine = engineID.map { $0.replacingOccurrences(of: "-x86_64", with: "") } ?? "未安装引擎"
        return "\(engine) · Rosetta \(model.environment.rosetta ? "已安装" : "未安装")"
    }
}

/// The apple-and-leaf mark from the design.
struct CiderMark: Shape {
    func path(in rect: CGRect) -> Path {
        let s = min(rect.width, rect.height) / 28
        func p(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: rect.minX + x * s, y: rect.minY + y * s) }
        var path = Path()
        path.move(to: p(14, 9))
        path.addCurve(to: p(6, 14.2), control1: p(11, 6.8), control2: p(6, 7.8))
        path.addCurve(to: p(12, 24), control1: p(6, 19.8), control2: p(9.6, 24))
        path.addCurve(to: p(14, 23.5), control1: p(13, 24), control2: p(13.4, 23.5))
        path.addCurve(to: p(16, 24), control1: p(14.6, 23.5), control2: p(15, 24))
        path.addCurve(to: p(22, 14.2), control1: p(18.4, 24), control2: p(22, 19.8))
        path.addCurve(to: p(14, 9), control1: p(22, 7.8), control2: p(17, 6.8))
        path.move(to: p(14, 9))
        path.addQuadCurve(to: p(18.4, 4), control: p(14.2, 5.2))
        path.move(to: p(10, 15.5))
        path.addQuadCurve(to: p(13, 19.3), control: p(10.6, 18.3))
        return path
    }
}
