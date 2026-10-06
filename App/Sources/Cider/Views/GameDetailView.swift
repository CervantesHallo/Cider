import CiderBottle
import CiderData
import SwiftUI

struct GameDetailView: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem
    @State private var showSettings = false

    private var bottle: Bottle? { model.bottles.first { $0.config.id == item.bottleID } }

    var body: some View {
        ScrollView {
            VStack(spacing: 0) {
                ZStack(alignment: .bottomLeading) {
                    Artwork(item: item, url: item.heroURL, titleSize: 1)
                    LinearGradient(colors: [.clear, Theme.window.opacity(0.85)], startPoint: .center, endPoint: .bottom)
                    VStack(alignment: .leading, spacing: 8) {
                        AppIcon(item: item, size: 56)
                        Text(item.title).font(.system(size: 44, weight: .semibold)).lineLimit(2)
                        Text("\(item.source) · 瓶子「\(item.bottleName)」").font(.system(size: 15)).foregroundStyle(Theme.textSecondary)
                    }
                    .padding(40)
                    Button { model.detail = nil } label: { Label("资料库", systemImage: "chevron.left") }
                        .buttonStyle(.plain)
                        .font(.system(size: 13))
                        .padding(.horizontal, 12).frame(height: 32)
                        .background(.black.opacity(0.45), in: RoundedRectangle(cornerRadius: 9))
                        .padding(.leading, 28).padding(.top, 40)
                        .frame(maxHeight: .infinity, alignment: .top)
                }
                .frame(height: 330)
                .clipped()
    
                HStack(spacing: 12) {
                    if model.isRunning(item) {
                        RunningChip()
                        StopButton(item: item, large: true)
                        Button { model.restart(item) } label: { Label("重启", systemImage: "arrow.clockwise") }
                            .buttonStyle(OutlineButtonStyle(height: 50))
                            .disabled(model.busy.contains(item.id) || model.busy.contains("activity:\(item.bottleID)"))
                    } else if let progress = item.downloadProgress {
                        VStack(alignment: .leading, spacing: 6) {
                            Text("Steam 正在下载 \(Int(progress * 100))%").font(.system(size: 14, weight: .semibold))
                            DownloadBar(progress: progress).frame(width: 260)
                        }
                    } else {
                        Button { model.launch(item) } label: {
                            Label(model.busy.contains(item.id) ? "正在启动…" : (item.isGame ? "开始游戏" : "启动应用"), systemImage: "play.fill")
                        }
                        .buttonStyle(AccentButtonStyle(height: 50))
                        .disabled(model.busy.contains(item.id))
                    }
                    Button { showSettings = true } label: { Label("设置", systemImage: "slider.horizontal.3") }
                        .buttonStyle(OutlineButtonStyle(height: 50))
                    Spacer()
                    Stat(title: "兼容性", value: model.verdict(for: item).label, color: model.verdict(for: item).color)
                    Stat(title: "上次游玩", value: item.lastPlayed.map { $0.formatted(.relative(presentation: .named)) } ?? "—")
                    Stat(title: "占用空间", value: item.sizeOnDisk > 0 ? ByteCountFormatter.string(fromByteCount: item.sizeOnDisk, countStyle: .file) : "—")
                }
                .padding(.horizontal, 40)
                .frame(height: 92)
                .overlay(alignment: .bottom) { Rectangle().fill(Theme.line).frame(height: 1) }
    
                HStack(alignment: .top, spacing: 24) {
                    Card {
                        VStack(alignment: .leading, spacing: 14) {
                            Text("运行方式").font(.system(size: 15, weight: .semibold))
                            Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 11) {
                                row("瓶子", "\(item.bottleName)（Windows 10 · 64 位）")
                                row("引擎", "\(bottle?.config.engine.id ?? "—") · x86_64，经 Rosetta 运行")
                                row("图形", "自动：DX9 → wined3d（OpenGL），DX10/11 → DXMT（Metal）")
                                row("区域", bottle.map { "\($0.config.locale.lcAll)（代码页 \($0.config.locale.acp)）" } ?? "—")
                                row("视频", model.environment.gstreamerBundled ? "引擎自带 GStreamer，开场动画可以播放" : model.environment.gstreamer ? "GStreamer 已就绪，开场动画可以播放" : "未安装 GStreamer，开场动画可能无法播放")
                            }
                            .font(.system(size: 13.5))
                        }
                    }
                    VStack(spacing: 20) {
                        CompatCard(item: item)
                        if case .steamClient = item.kind { SteamTipsCard() } else { PatchCard(item: item) }
                        Card {
                            VStack(alignment: .leading, spacing: 4) {
                                Text("快捷操作").font(.system(size: 15, weight: .semibold)).padding(.bottom, 6)
                                if let bottle, case .steamGame(let appID, let installDir) = item.kind {
                                    QuickAction(title: "在访达中显示游戏文件", symbol: "folder") {
                                        let dir = bottle.driveC.appendingPathComponent("Program Files (x86)/Steam/steamapps/common/\(installDir)")
                                        model.revealInFinder(dir)
                                    }
                                    QuickAction(title: "打开 Steam 商店页（App \(appID)）", symbol: "cart") {
                                        NSWorkspace.shared.open(URL(string: "https://store.steampowered.com/app/\(appID)/")!)
                                    }
                                }
                                QuickAction(title: "打开运行日志文件夹", symbol: "doc.text.magnifyingglass") {
                                    model.revealInFinder(FileManager.default.homeDirectoryForCurrentUser
                                        .appendingPathComponent("Library/Logs/Cider/sessions/\(item.bottleID)"))
                                }
                                if let bottle {
                                QuickAction(title: "生成诊断包（求助时附上）", symbol: "stethoscope") { model.makeDiagnostics(for: bottle) }
                            }
                            QuickAction(title: "结束瓶子内所有程序", symbol: "stop.circle") { model.stopBottle(item.bottleID) }
                            }
                        }
                    }
                    .frame(width: 420)
                }
                .padding(.horizontal, 40)
                .padding(.vertical, 26)
            }
        }
        .sheet(isPresented: $showSettings) { GameSettingsSheet(item: item) }
    }

    @ViewBuilder private func row(_ label: String, _ value: String) -> some View {
        GridRow {
            Text(label).foregroundStyle(Theme.textTertiary)
            Text(value)
        }
    }
}

struct Stat: View {
    let title: String
    let value: String
    var color: Color = Theme.text
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
            Text(value).font(.system(size: 14, weight: .semibold)).foregroundStyle(color)
        }
        .padding(.leading, 24)
    }
}

struct QuickAction: View {
    let title: String
    let symbol: String
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: symbol).foregroundStyle(Theme.textSecondary).frame(width: 18)
                Text(title)
                Spacer()
            }
            .font(.system(size: 13.5))
            .frame(height: 36)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// Per-game settings (design: 游戏设置). Graphics and locale options are shown; wiring them to the
/// config layers (docs/plan/01 §7) comes with the profile system.
struct GameSettingsSheet: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let item: LibraryItem
    @State private var winver: String = ""

    private var bottle: Bottle? { model.bottles.first { $0.config.id == item.bottleID } }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("\(item.title) · 设置").font(.system(size: 17, weight: .semibold))
                Spacer()
                Button { dismiss() } label: { Image(systemName: "xmark").font(.system(size: 12, weight: .bold)) }
                    .buttonStyle(.plain).accessibilityLabel("关闭")
            }
            .padding(.horizontal, 26).frame(height: 64)
            Divider().overlay(Theme.line)
            VStack(alignment: .leading, spacing: 22) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("这个程序").font(.system(size: 14, weight: .semibold))
                    HStack {
                        Text("Windows 版本").foregroundStyle(Theme.textTertiary).frame(width: 100, alignment: .leading)
                        Picker("Windows 版本", selection: Binding(get: { winver }, set: { v in
                            winver = v
                            model.setProgramWindowsVersion(v.isEmpty ? nil : v, for: item)
                        })) {
                            Text("跟随瓶子").tag("")
                            ForEach(["win11", "win10", "win81", "win8", "win7", "winxp"], id: \.self) { Text("Windows \($0.dropFirst(3))").tag($0) }
                        }
                        .labelsHidden().frame(width: 200)
                        .disabled(item.executableName == nil)
                    }
                    Text(item.executableName.map { "只影响 \($0)，也适用于经 Steam 等启动器启动的情况。" } ?? "没有找到这个程序的主程序文件。")
                        .font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                }
                VStack(alignment: .leading, spacing: 8) {
                    Text("图形").font(.system(size: 14, weight: .semibold))
                    Text("自动选择：DirectX 9 及更早 → wined3d（OpenGL）；DirectX 10/11 → DXMT（Metal）；DirectX 12 → D3DMetal（需要从 Apple 的 GPTK 导入）。")
                        .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
                }
                if let bottle {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("整个瓶子「\(bottle.config.name)」").font(.system(size: 14, weight: .semibold))
                        Toggle("高分辨率模式（文字更清晰，部分老游戏窗口会变小）", isOn: Binding(
                            get: { bottle.config.settings[BottleStore.highResolutionKey] == "1" },
                            set: { model.setHighResolution($0, for: bottle) }))
                        Toggle("显示帧率与性能信息（Metal HUD）", isOn: Binding(
                            get: { bottle.config.settings[BottleStore.metalHUDKey] == "1" },
                            set: { model.setMetalHUD($0, for: bottle) }))
                        Toggle("报告 AVX 支持（要求 AVX 的新游戏需要，macOS 15 起可用）", isOn: Binding(
                            get: { bottle.config.settings[BottleStore.advertiseAVXKey] == "1" },
                            set: { model.setAdvertiseAVX($0, for: bottle) }))
                        Text("经 Steam 启动的游戏，要在 Steam 重启后才会带上瓶子级的改动。")
                            .font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                    }
                }
            }
            .font(.system(size: 13.5))
            .toggleStyle(.switch)
            .tint(Theme.accent)
            .padding(26)
            Spacer()
            HStack {
                Text("更改在下次启动时生效").font(.system(size: 12.5)).foregroundStyle(Theme.textTertiary)
                Spacer()
                Button("完成") { dismiss() }.buttonStyle(AccentButtonStyle(height: 38))
            }
            .padding(.horizontal, 26).frame(height: 68)
        }
        .frame(width: 720, height: 560)
        .background(Theme.card)
        .foregroundStyle(Theme.text)
        .onAppear { winver = model.programWindowsVersion(item) ?? "" }
    }
}

/// Drop zone that copies files into the game's root folder (e.g. a galgame's restoration patch), with undo.
struct PatchCard: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem
    @State private var targeted = false
    @State private var showImporter = false

    var body: some View {
        let last = model.lastPatch(for: item)
        let working = model.busy.contains("patch:\(item.id)")
        Card {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("游戏目录与补丁").font(.system(size: 15, weight: .semibold))
                    Spacer()
                    if let dir = item.installDirectory {
                        Button("打开目录") { model.revealInFinder(dir) }.buttonStyle(.plain)
                            .font(.system(size: 12.5, weight: .semibold)).foregroundStyle(Theme.accent)
                    }
                }
                Button { showImporter = true } label: {
                    VStack(spacing: 6) {
                        Image(systemName: working ? "hourglass" : "arrow.down.doc").font(.system(size: 20, weight: .light))
                        Text(working ? "正在安装补丁…" : "把补丁文件、.zip 或补丁安装程序拖到这里").font(.system(size: 13))
                        Text("文件复制到游戏根目录，被覆盖的原文件自动备份；.exe 会在瓶子里运行").font(.system(size: 11.5)).foregroundStyle(Theme.textTertiary)
                    }
                    .foregroundStyle(Theme.textSecondary)
                    .frame(maxWidth: .infinity).frame(height: 104)
                    .background(targeted ? Theme.raised : .clear, in: RoundedRectangle(cornerRadius: 12))
                    .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(style: StrokeStyle(lineWidth: 1.5, dash: [6, 5])).foregroundStyle(targeted ? Theme.accent : Theme.control))
                }
                .buttonStyle(.plain)
                .disabled(working || item.installDirectory == nil)
                .dropDestination(for: URL.self) { urls, _ in
                    guard !urls.isEmpty else { return false }
                    model.installPatch(urls, for: item)
                    return true
                } isTargeted: { targeted = $0 }
                .fileImporter(isPresented: $showImporter, allowedContentTypes: [.item], allowsMultipleSelection: true) { result in
                    if case .success(let urls) = result, !urls.isEmpty { model.installPatch(urls, for: item) }
                }
                if let last {
                    HStack(spacing: 8) {
                        Text("上次补丁：\(last.sources.joined(separator: "、"))").font(.system(size: 12)).foregroundStyle(Theme.textTertiary).lineLimit(1)
                        Spacer()
                        Button("撤销") { model.undoPatch(for: item) }.buttonStyle(.plain)
                            .font(.system(size: 12, weight: .semibold)).foregroundStyle(Theme.bad)
                    }
                }
            }
        }
    }
}

/// What the compatibility database knows: the verdict for this environment, where it was verified, known issues.
struct CompatCard: View {
    @Environment(AppModel.self) private var model
    let item: LibraryItem

    var body: some View {
        let verdict = model.verdict(for: item)
        let decision = model.compatDecisions[item.id]
        let profile = item.steamAppID.flatMap { model.compat.profile(steamAppID: $0) }
            ?? model.compat.profile(exe: item.launchProgram)
        Card {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text("兼容性").font(.system(size: 15, weight: .semibold))
                    Spacer()
                    VerdictChip(verdict: verdict)
                }
                if let v = decision?.verdict {
                    if let note = v.notes?["zh-Hans"] ?? v.notes?["en"] {
                        Text(note).font(.system(size: 13.5)).foregroundStyle(Theme.textSidebar)
                    }
                    Text("\(v.lastVerified) 验证 · \(v.provenance.hw ?? "—") · \(v.provenance.engine ?? "引擎 \(v.engineMajor).x")")
                        .font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                } else if case .steamClient = item.kind {
                    Text("Cider 已验证：登录、商店、下载、云存档正常。").font(.system(size: 13.5)).foregroundStyle(Theme.textSidebar)
                } else {
                    Text("这个环境下还没有验证记录。运行正常或遇到问题，都可以在诊断里生成报告。")
                        .font(.system(size: 13.5)).foregroundStyle(Theme.textSidebar)
                }
                ForEach(Array((profile?.knownIssues ?? []).enumerated()), id: \.offset) { _, issue in
                    VStack(alignment: .leading, spacing: 3) {
                        Label(issue.symptom["zh-Hans"] ?? issue.symptom["en"] ?? "", systemImage: "info.circle")
                            .font(.system(size: 13, weight: .medium))
                        if let fix = issue.fix?["zh-Hans"] ?? issue.fix?["en"] {
                            Text(fix).font(.system(size: 12.5)).foregroundStyle(Theme.textSecondary).padding(.leading, 22)
                        }
                    }
                    .padding(.top, 4)
                }
            }
        }
    }
}

/// Steam settings that make the client lighter on Macs with little memory (docs/plan/02 P-4). They live in
/// Steam's own settings store, so Cider explains them instead of editing them.
struct SteamTipsCard: View {
    private let tips: [(String, String)] = [
        ("设置 → 界面 → 启动时的首选窗口：选「库」", "不再每次启动都加载带动画的商店页。"),
        ("设置 → 库 → 勾选「低性能模式」", "关闭库里的动画和模糊效果。"),
        ("设置 → 界面 → 取消「通知我有关…新发行和即将发行的游戏」", "去掉启动时弹出的「特惠」窗口，少开一个网页进程。"),
        ("不要关闭「网页视图 GPU 加速」", "在 Mac 上关掉后 Steam 会黑屏。"),
    ]

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 10) {
                Text("让 Steam 更流畅").font(.system(size: 15, weight: .semibold))
                ForEach(tips, id: \.0) { tip in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(tip.0).font(.system(size: 13, weight: .medium))
                        Text(tip.1).font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                    }
                }
                Text("从 Cider 启动游戏时，如果 Steam 没开，它会在后台安静启动，不弹主窗口。")
                    .font(.system(size: 12)).foregroundStyle(Theme.textSecondary).padding(.top, 2)
            }
        }
    }
}
