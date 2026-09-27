import AppKit
import SwiftUI

struct LibraryView: View {
    @Environment(AppModel.self) private var model
    @State private var filter: Filter = .all
    @State private var query = ""

    enum Filter: String, CaseIterable, Identifiable {
        case all = "全部", games = "游戏", apps = "应用"
        var id: String { rawValue }
    }

    private var visible: [LibraryItem] {
        model.items
            .filter { filter == .all || (filter == .games) == $0.isGame }
            .filter { query.isEmpty || $0.title.localizedCaseInsensitiveContains(query) }
    }

    var body: some View {
        VStack(spacing: 0) {
            PageHeader(title: "资料库") {
                Picker("筛选", selection: $filter) {
                    ForEach(Filter.allCases) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .labelsHidden()
                .frame(width: 200)
                SearchField(text: $query)
                Button { model.section = .install } label: { Label("安装 Windows 软件", systemImage: "plus") }
                    .buttonStyle(AccentButtonStyle(height: 36))
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 28) {
                    if let item = model.continueItem, query.isEmpty, filter != .apps {
                        ContinueCard(item: item)
                    }
                    if model.items.isEmpty {
                        EmptyLibrary()
                    } else {
                        HStack(spacing: 10) {
                            Text("全部").font(.system(size: 17, weight: .semibold))
                            Text("\(visible.count) 项").font(.system(size: 13)).foregroundStyle(Theme.textTertiary)
                        }
                        LazyVGrid(columns: [GridItem(.adaptive(minimum: 250), spacing: 20)], spacing: 22) {
                            ForEach(visible) { item in LibraryTile(item: item) }
                        }
                    }
                }
                .padding(.horizontal, 32)
                .padding(.vertical, 28)
            }
        }
    }
}

struct SearchField: View {
    @Binding var text: String
    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass").foregroundStyle(Theme.textTertiary)
            TextField("搜索游戏和应用", text: $text).textFieldStyle(.plain).font(.system(size: 13))
        }
        .padding(.horizontal, 12)
        .frame(width: 240, height: 36)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 10))
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Theme.cardLine))
    }
}

/// Loads local artwork synchronously (Steam's cache, extracted icons) and remote artwork asynchronously.
struct ArtImage: View {
    let url: URL?
    var body: some View {
        if let url, url.isFileURL {
            if let image = NSImage(contentsOf: url) {
                Image(nsImage: image).resizable().aspectRatio(contentMode: .fill)
            }
        } else if let url {
            AsyncImage(url: url) { phase in
                if let image = phase.image { image.resizable().aspectRatio(contentMode: .fill) }
            }
        }
    }
}

/// The app's own icon on a rounded tile, or its initial when there is none.
struct AppIcon: View {
    let item: LibraryItem
    var size: CGFloat = 36

    var body: some View {
        let (bg, fg) = Theme.poster(for: item.title)
        ZStack {
            RoundedRectangle(cornerRadius: size * 0.24).fill(item.iconURL == nil ? bg : Color.black.opacity(0.25))
            if let url = item.iconURL, let image = NSImage(contentsOf: url) {
                Image(nsImage: image).resizable().interpolation(.high).aspectRatio(contentMode: .fit).padding(size * 0.08)
            } else {
                Text(String(item.title.prefix(1))).font(Theme.display(size * 0.45)).foregroundStyle(fg)
            }
        }
        .frame(width: size, height: size)
        .clipShape(RoundedRectangle(cornerRadius: size * 0.24))
    }
}

/// Wide artwork with a typeset placeholder; programs without artwork show their icon large instead.
struct Artwork: View {
    let item: LibraryItem
    let url: URL?
    var titleSize: CGFloat = 20

    var body: some View {
        let (bg, fg) = Theme.poster(for: item.title)
        // Everything sits in overlays so a large image can never widen the layout (it only fills and clips).
        bg
            .overlay(alignment: .bottomLeading) {
                if url == nil, item.iconURL != nil {
                    AppIcon(item: item, size: 72).frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    Text(item.title)
                        .font(Theme.display(titleSize, weight: .medium))
                        .foregroundStyle(fg)
                        .lineLimit(2)
                        .padding(14)
                }
            }
            .overlay { ArtImage(url: url) }
            .clipped()
    }
}

struct LibraryTile: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Button { open() } label: {
                Artwork(item: item, url: item.headerURL)
                    .aspectRatio(460.0 / 215.0, contentMode: .fit)
                    .clipShape(RoundedRectangle(cornerRadius: 12))
                    .overlay(alignment: .bottom) {
                        if let progress = item.downloadProgress {
                            DownloadBar(progress: progress).padding(10)
                        }
                    }
            }
            .buttonStyle(.plain)
            .accessibilityLabel("\(item.title) 详情")
            .dropDestination(for: URL.self) { urls, _ in
                // Dropping files on a game tile installs them into the game folder (e.g. a patch).
                guard !urls.isEmpty, item.installDirectory != nil, item.kind != .steamClient else { return false }
                model.installPatch(urls, for: item)
                return true
            }
            HStack(alignment: .center, spacing: 10) {
                AppIcon(item: item, size: 32)
                VStack(alignment: .leading, spacing: 2) {
                    Text(item.title).font(.system(size: 14, weight: .semibold)).lineLimit(1)
                    Text(item.source).font(.system(size: 12)).foregroundStyle(Theme.textTertiary).lineLimit(1)
                }
                Spacer(minLength: 4)
                StatusControl(item: item).fixedSize()
            }
        }
    }

    private func open() {
        if item.isGame { model.detail = item } else if !model.isRunning(item) { model.launch(item) } else { model.launch(item) }
    }
}

/// Running → "运行中" + a stop button; downloading → progress; otherwise the compatibility verdict.
struct StatusControl: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem

    var body: some View {
        if model.isRunning(item) {
            HStack(spacing: 6) {
                RunningChip()
                StopButton(item: item)
            }
        } else if let progress = item.downloadProgress {
            Text("下载中 \(Int(progress * 100))%")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(Theme.ok)
                .padding(.horizontal, 8).frame(height: 22)
                .background(Theme.ok.opacity(0.15), in: Capsule())
        } else {
            VerdictChip(verdict: model.verdict(for: item))
        }
    }
}

struct StopButton: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem
    var large = false

    var body: some View {
        let stopping = model.busy.contains("stop:\(item.id)")
        Group {
            if large {
                Button { model.stop(item) } label: { Label(stopping ? "正在停止…" : "停止", systemImage: "stop.fill") }
                    .buttonStyle(OutlineButtonStyle(height: 50))
            } else {
                Button { model.stop(item) } label: {
                    Image(systemName: stopping ? "hourglass" : "stop.fill").font(.system(size: 10, weight: .bold))
                        .frame(width: 24, height: 22)
                        .background(Theme.bad.opacity(0.16), in: Capsule())
                        .foregroundStyle(Theme.bad)
                }
                .buttonStyle(.plain)
            }
        }
        .disabled(stopping)
        .help("停止 \(item.title)")
        .accessibilityLabel("停止 \(item.title)")
    }
}

struct RunningChip: View {
    var body: some View {
        HStack(spacing: 5) {
            Circle().fill(Theme.good).frame(width: 7, height: 7)
            Text("运行中")
        }
        .font(.system(size: 11, weight: .semibold))
        .foregroundStyle(Theme.good)
        .padding(.horizontal, 8)
        .frame(height: 22)
        .background(Theme.good.opacity(0.14), in: Capsule())
    }
}

struct DownloadBar: View {
    let progress: Double
    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                Capsule().fill(.black.opacity(0.5))
                Capsule().fill(Theme.accent).frame(width: max(6, geo.size.width * progress))
            }
        }
        .frame(height: 6)
    }
}

struct ContinueCard: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem

    var body: some View {
        HStack(spacing: 0) {
            VStack(alignment: .leading, spacing: 0) {
                Text("继续游玩").font(.system(size: 12, weight: .semibold)).tracking(1).foregroundStyle(Theme.textSecondary)
                Text(item.title).font(.system(size: 36, weight: .semibold)).lineLimit(2).padding(.top, 10)
                Spacer()
                HStack(spacing: 12) {
                    if model.isRunning(item) { RunningChip() } else { VerdictChip(verdict: model.verdict(for: item)) }
                    Text(meta).font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
                }
                HStack(spacing: 12) {
                    if model.isRunning(item) {
                        StopButton(item: item, large: true)
                    } else {
                        Button { model.launch(item) } label: {
                            Label(model.busy.contains(item.id) ? "正在启动…" : "开始游戏", systemImage: "play.fill")
                        }
                        .buttonStyle(AccentButtonStyle(height: 48))
                        .disabled(model.busy.contains(item.id))
                    }
                    Button("详情") { model.detail = item }.buttonStyle(OutlineButtonStyle(height: 48))
                }
                .padding(.top, 18)
            }
            .padding(.horizontal, 34)
            .padding(.vertical, 30)
            .frame(width: 480, alignment: .leading)
            Artwork(item: item, url: item.heroURL, titleSize: 44)
        }
        .frame(height: 282)
        .background(Theme.card)
        .clipShape(RoundedRectangle(cornerRadius: 18))
        .overlay(RoundedRectangle(cornerRadius: 18).stroke(Theme.cardLine))
    }

    private var meta: String {
        var parts = [item.source]
        if let last = item.lastPlayed { parts.append("上次游玩：\(last.formatted(.relative(presentation: .named)))") }
        return parts.joined(separator: " · ")
    }
}

struct EmptyLibrary: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 10) {
                Text("资料库还是空的").font(.system(size: 17, weight: .semibold))
                Text("先安装 Steam 或任意 Windows 软件。Steam 里安装的游戏、开始菜单里的程序都会自动出现在这里。")
                    .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
                Button("安装 Windows 软件") { model.section = .install }.buttonStyle(AccentButtonStyle()).padding(.top, 6)
            }
        }
    }
}
