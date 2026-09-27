import AppKit
import CiderData
import SwiftUI

/// Shown instead of launching a game Cider cannot run honestly yet (docs/plan/00 ADR-010): why, and where to play
/// it officially. Only official-cloud routes exist in the data; iOS forms are rejected by the schema.
struct RouteCardView: View {
    @Environment(AppModel.self) private var model
    let block: Preflight.Block

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(spacing: 12) {
                Image(systemName: "shield.lefthalf.filled").font(.system(size: 26)).foregroundStyle(Theme.tune)
                Text("《\(block.title)》暂时不能在本机运行").font(.system(size: 20, weight: .semibold))
            }
            Text(block.reason).font(.system(size: 13.5)).foregroundStyle(Theme.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
            if !block.routes.isEmpty {
                Text("现在可以这样玩").font(.system(size: 14, weight: .semibold)).padding(.top, 4)
                ForEach(block.routes, id: \.url) { route in
                    Button { open(route) } label: {
                        HStack {
                            Image(systemName: "cloud")
                            Text(route.name["zh-Hans"] ?? route.name["en"] ?? route.url)
                            Spacer()
                            Image(systemName: "arrow.up.right.square")
                        }
                        .padding(.horizontal, 14).frame(height: 44)
                        .background(Theme.raised, in: RoundedRectangle(cornerRadius: 10))
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
                Text("官方云游戏，用米哈游通行证登录；存档与本地客户端相同。")
                    .font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
            }
            HStack {
                Spacer()
                Button("好") { model.routeCard = nil }.buttonStyle(AccentButtonStyle(height: 34))
            }
        }
        .padding(26)
        .frame(width: 480)
        .background(Theme.card)
        .foregroundStyle(Theme.text)
    }

    /// Chrome's app window when Chrome is installed (closest to a native window), otherwise the default browser.
    private func open(_ route: GameEntry.Route) {
        guard let url = URL(string: route.url) else { return }
        if let chrome = NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.google.Chrome") {
            let config = NSWorkspace.OpenConfiguration()
            config.arguments = ["--app=\(route.url)"]
            config.createsNewApplicationInstance = false
            NSWorkspace.shared.openApplication(at: chrome, configuration: config)
        } else {
            NSWorkspace.shared.open(url)
        }
        model.routeCard = nil
    }
}
