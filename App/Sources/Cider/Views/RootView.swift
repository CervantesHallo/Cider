import CiderData
import SwiftUI

struct RootView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        HStack(spacing: 0) {
            SidebarView()
            Group {
                switch model.section {
                case .library:
                    if let item = model.detail { GameDetailView(item: item) } else { LibraryView() }
                case .install: InstallView()
                case .bottles: BottlesView()
                case .environment: EnvironmentView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Theme.window)
        }
        .foregroundStyle(Theme.text)
        .overlay(alignment: .bottom) { MessageBar() }
        .ignoresSafeArea()
        .sheet(item: Binding(get: { model.routeCard.map(IdentifiedBlock.init) }, set: { if $0 == nil { model.routeCard = nil } })) {
            RouteCardView(block: $0.block)
        }
        .sheet(isPresented: Binding(get: { model.showWelcome }, set: { model.showWelcome = $0 })) { WelcomeView() }
    }
}

struct MessageBar: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        if let message = model.message {
            HStack(spacing: 10) {
                Text(message).font(.system(size: 13))
                Button { model.message = nil } label: { Image(systemName: "xmark").font(.system(size: 11, weight: .bold)) }
                    .buttonStyle(.plain)
                    .accessibilityLabel("关闭提示")
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
            .background(Theme.raised, in: Capsule())
            .overlay(Capsule().stroke(Theme.cardLine))
            .padding(.bottom, 20)
            .padding(.leading, 240)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }
}

/// Primary amber button from the design.
struct AccentButtonStyle: ButtonStyle {
    var height: CGFloat = 40
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 14, weight: .semibold))
            .foregroundStyle(Theme.onAccent)
            .padding(.horizontal, 18)
            .frame(height: height)
            .background(Theme.accent.opacity(configuration.isPressed ? 0.8 : 1), in: RoundedRectangle(cornerRadius: 11))
    }
}

/// Outlined secondary button.
struct OutlineButtonStyle: ButtonStyle {
    var height: CGFloat = 40
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 14, weight: .semibold))
            .foregroundStyle(Theme.text)
            .padding(.horizontal, 16)
            .frame(height: height)
            .background(configuration.isPressed ? Theme.raised : .clear, in: RoundedRectangle(cornerRadius: 11))
            .overlay(RoundedRectangle(cornerRadius: 11).stroke(Theme.control))
    }
}

struct VerdictChip: View {
    let verdict: Verdict
    var body: some View {
        Label {
            Text(verdict.label)
        } icon: {
            Image(systemName: verdict.symbol).font(.system(size: 9, weight: .heavy))
        }
        .labelStyle(.titleAndIcon)
        .font(.system(size: 11, weight: .semibold))
        .foregroundStyle(verdict.color)
        .padding(.horizontal, 8)
        .frame(height: 22)
        .background(verdict.color.opacity(0.14), in: Capsule())
    }
}

struct Card<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        content
            .padding(22)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Theme.card, in: RoundedRectangle(cornerRadius: 16))
            .overlay(RoundedRectangle(cornerRadius: 16).stroke(Theme.cardLine))
    }
}

/// Section title bar used at the top of each page.
struct PageHeader<Trailing: View>: View {
    let title: String
    @ViewBuilder var trailing: Trailing
    var body: some View {
        HStack(spacing: 16) {
            Text(title).font(.system(size: 20, weight: .semibold))
            Spacer()
            trailing
        }
        .padding(.horizontal, 32)
        .frame(height: 64)
        .padding(.top, 12)
        .overlay(alignment: .bottom) { Rectangle().fill(Theme.line).frame(height: 1) }
    }
}

struct IdentifiedBlock: Identifiable {
    let block: Preflight.Block
    var id: String { block.gameID }
}
