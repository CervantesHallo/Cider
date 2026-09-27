import CiderBottle
import CiderRuntime
import CiderSchema
import SwiftUI
import UniformTypeIdentifiers

/// Bottle management (docs/plan/05 §15 parity rows 13–19, 21, 27, 32, 34): a list of bottles and, for the selected
/// one, running programs and commands, Wine's Windows tools, settings, snapshots and maintenance.
struct BottlesView: View {
    @Environment(AppModel.self) private var model
    @State private var selectedID: String?
    @State private var showCreate = false
    @State private var showForeign = false
    @State private var importingArchive = false

    private var selected: Bottle? {
        model.bottles.first { $0.config.id == selectedID } ?? model.bottles.first
    }

    var body: some View {
        VStack(spacing: 0) {
            PageHeader(title: "瓶子") {
                Menu {
                    Button("导入 .ciderbottle 文件…") { importingArchive = true }
                    Button("从 CrossOver / Whisky 导入…") { showForeign = true }
                } label: { Label("导入", systemImage: "square.and.arrow.down") }
                    .menuStyle(.borderlessButton).fixedSize()
                    .disabled(model.busy.contains("import"))
                Button { showCreate = true } label: { Label("新建瓶子", systemImage: "plus") }
                    .buttonStyle(AccentButtonStyle(height: 36))
                    .disabled(model.busy.contains("create-bottle"))
            }
            if model.bottles.isEmpty {
                VStack(spacing: 10) {
                    Image(systemName: "flask").font(.system(size: 34, weight: .light)).foregroundStyle(Theme.textTertiary)
                    Text("还没有瓶子").font(.system(size: 16, weight: .semibold))
                    Text("瓶子是一个独立的 Windows 环境，每个瓶子有自己的 C: 盘、注册表和设置。")
                        .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
                    Button("新建瓶子") { showCreate = true }.buttonStyle(AccentButtonStyle()).padding(.top, 6)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                HStack(alignment: .top, spacing: 0) {
                    ScrollView {
                        VStack(spacing: 6) {
                            ForEach(model.bottles, id: \.config.id) { bottle in
                                BottleListRow(bottle: bottle, selected: bottle.config.id == selected?.config.id)
                                    .onTapGesture { selectedID = bottle.config.id }
                            }
                        }
                        .padding(16)
                    }
                    .frame(width: 270)
                    .overlay(alignment: .trailing) { Rectangle().fill(Theme.line).frame(width: 1) }
                    if let bottle = selected {
                        BottleDetailView(bottle: bottle).id(bottle.config.id)
                    }
                }
            }
        }
        .sheet(isPresented: $showCreate) { CreateBottleSheet() }
        .sheet(isPresented: $showForeign) { ForeignImportSheet() }
        .fileImporter(isPresented: $importingArchive, allowedContentTypes: [UTType(filenameExtension: "ciderbottle") ?? .zip]) { result in
            if case .success(let url) = result { model.importArchive(url) }
        }
    }
}

struct BottleListRow: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle
    let selected: Bool

    var body: some View {
        HStack(spacing: 12) {
            BottleBadge(name: bottle.config.name, size: 36)
            VStack(alignment: .leading, spacing: 2) {
                Text(bottle.config.name).font(.system(size: 14, weight: .semibold)).lineLimit(1)
                Text("\(bottle.config.locale.displayName) · \(bottle.config.template.winver.uppercased())")
                    .font(.system(size: 12)).foregroundStyle(Theme.textTertiary).lineLimit(1)
            }
            Spacer(minLength: 4)
            if model.runningBottles.contains(bottle.config.id) {
                Circle().fill(Theme.good).frame(width: 7, height: 7).help("有程序在运行")
            }
        }
        .padding(10)
        .background(selected ? Theme.selected : .clear, in: RoundedRectangle(cornerRadius: 11))
        .overlay(RoundedRectangle(cornerRadius: 11).stroke(selected ? Theme.selectedBorder : .clear))
        .contentShape(Rectangle())
    }
}

struct BottleBadge: View {
    let name: String
    var size: CGFloat = 40
    var body: some View {
        let (bg, fg) = Theme.poster(for: name)
        Image(systemName: "flask.fill").font(.system(size: size * 0.42)).foregroundStyle(fg)
            .frame(width: size, height: size)
            .background(bg, in: RoundedRectangle(cornerRadius: size * 0.28))
    }
}

// MARK: - Detail

struct BottleDetailView: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle
    @State private var renaming = false
    @State private var newName = ""
    @State private var confirmDelete = false

    private var running: Bool { model.runningBottles.contains(bottle.config.id) }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                header
                // Two columns when the window is wide, one otherwise.
                ViewThatFits(in: .horizontal) {
                    HStack(alignment: .top, spacing: 20) {
                        primary.frame(minWidth: 480)
                        secondary.frame(width: 340)
                    }
                    VStack(spacing: 20) {
                        primary
                        secondary
                    }
                }
            }
            .padding(28)
        }
        .alert("把“\(bottle.config.name)”移到废纸篓？", isPresented: $confirmDelete) {
            Button("移到废纸篓", role: .destructive) { model.deleteBottle(bottle) }
            Button("取消", role: .cancel) {}
        } message: {
            Text("瓶子里安装的程序和存档都在里面。移到废纸篓后，清倒废纸篓前还能找回。")
        }
    }

    private var header: some View {
        HStack(spacing: 16) {
            BottleBadge(name: bottle.config.name, size: 56)
            VStack(alignment: .leading, spacing: 5) {
                if renaming {
                    HStack {
                        TextField("名称", text: $newName).textFieldStyle(.roundedBorder).frame(width: 240)
                            .onSubmit(commitRename)
                        Button("完成", action: commitRename).buttonStyle(OutlineButtonStyle(height: 28))
                    }
                } else {
                    HStack(spacing: 8) {
                        Text(bottle.config.name).font(.system(size: 26, weight: .semibold))
                        Button { newName = bottle.config.name; renaming = true } label: { Image(systemName: "pencil") }
                            .buttonStyle(.plain).foregroundStyle(Theme.textTertiary).help("重命名")
                    }
                }
                Text("Windows \(bottle.config.template.winver.dropFirst(3)) · 64 位 · \(bottle.config.locale.lcAll) · \(bottle.config.engine.id)")
                    .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
            }
            Spacer()
            if running {
                RunningChip()
                Button { model.stopBottle(bottle.config.id) } label: { Label("结束所有程序", systemImage: "stop.fill") }
                    .buttonStyle(OutlineButtonStyle(height: 34))
            }
        }
    }

    private var primary: some View {
        VStack(spacing: 20) {
            RunCommandCard(bottle: bottle)
            ToolsCard(bottle: bottle)
            if let launchers = bottle.config.launchers, !launchers.isEmpty {
                LaunchersCard(bottle: bottle, launchers: launchers)
            }
        }
    }

    private var secondary: some View {
        VStack(spacing: 20) {
            BottleSettingsCard(bottle: bottle)
            SnapshotsCard(bottle: bottle)
            maintenance
        }
    }

    private func commitRename() {
        renaming = false
        let name = newName.trimmingCharacters(in: .whitespaces)
        if !name.isEmpty, name != bottle.config.name { model.renameBottle(bottle, to: name) }
    }

    private var maintenance: some View {
        Card {
            VStack(alignment: .leading, spacing: 4) {
                Text("维护").font(.system(size: 15, weight: .semibold)).padding(.bottom, 6)
                QuickAction(title: "在访达中打开 C: 盘", symbol: "externaldrive") { model.revealInFinder(bottle.driveC) }
                QuickAction(title: "模拟重启", symbol: "arrow.clockwise") { model.simulateReboot(bottle) }
                QuickAction(title: "修复瓶子", symbol: "wrench.and.screwdriver") { model.repairBottle(bottle) }
                QuickAction(title: "打开运行日志文件夹", symbol: "doc.text.magnifyingglass") {
                    model.revealInFinder(FileManager.default.homeDirectoryForCurrentUser
                        .appendingPathComponent("Library/Logs/Cider/sessions/\(bottle.config.id)"))
                }
                QuickAction(title: "生成诊断包", symbol: "stethoscope") { model.makeDiagnostics(for: bottle) }
                QuickAction(title: "复制瓶子", symbol: "plus.square.on.square") { model.duplicateBottle(bottle) }
                QuickAction(title: "导出为 .ciderbottle", symbol: "square.and.arrow.up") { model.exportBottle(bottle) }
                QuickAction(title: "移到废纸篓…", symbol: "trash") { confirmDelete = true }
                    .foregroundStyle(Theme.bad)
            }
        }
    }
}

/// "运行程序" and "运行命令" (CrossOver's Run Command, with "Save as launcher" and a verbose-log option).
struct RunCommandCard: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle
    @State private var program = ""
    @State private var arguments = ""
    @State private var verbose = false
    @State private var picking = false
    @State private var launcherName = ""
    @State private var naming = false

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("运行").font(.system(size: 15, weight: .semibold))
                    Spacer()
                    Button { picking = true } label: { Label("选择程序…", systemImage: "doc.badge.gearshape") }
                        .buttonStyle(OutlineButtonStyle(height: 30))
                }
                VStack(alignment: .leading, spacing: 8) {
                    TextField(#"程序，例如 C:\Program Files\App\app.exe"#, text: $program).textFieldStyle(.roundedBorder)
                    TextField("参数（可选）", text: $arguments).textFieldStyle(.roundedBorder)
                    Toggle("记录详细日志（排查问题时用）", isOn: $verbose).toggleStyle(.checkbox).font(.system(size: 12.5))
                }
                HStack(spacing: 10) {
                    Button { model.runCommand(program: trimmedProgram, arguments: argumentList, in: bottle, verbose: verbose) } label: {
                        Label("运行", systemImage: "play.fill")
                    }
                    .buttonStyle(AccentButtonStyle(height: 34))
                    .disabled(trimmedProgram.isEmpty)
                    if naming {
                        TextField("启动器名称", text: $launcherName).textFieldStyle(.roundedBorder).frame(width: 160)
                            .onSubmit(saveLauncher)
                        Button("保存", action: saveLauncher).buttonStyle(OutlineButtonStyle(height: 34))
                            .disabled(launcherName.trimmingCharacters(in: .whitespaces).isEmpty)
                    } else {
                        Button("另存为启动器…") {
                            launcherName = URL(fileURLWithPath: trimmedProgram.replacingOccurrences(of: "\\", with: "/"))
                                .deletingPathExtension().lastPathComponent
                            naming = true
                        }
                        .buttonStyle(OutlineButtonStyle(height: 34))
                        .disabled(trimmedProgram.isEmpty)
                    }
                    Spacer()
                    if let log = model.lastCommandLog {
                        Button("显示日志") { model.revealInFinder(log) }.buttonStyle(.plain)
                            .font(.system(size: 12.5, weight: .semibold)).foregroundStyle(Theme.accent)
                    }
                }
            }
        }
        .fileImporter(isPresented: $picking, allowedContentTypes: [.exe, .msi, .item], allowsMultipleSelection: false) { result in
            if case .success(let urls) = result, let url = urls.first { program = url.path }
        }
    }

    private var trimmedProgram: String { program.trimmingCharacters(in: .whitespaces) }
    private var argumentList: [String] { ArgumentSplitter.split(arguments) }

    private func saveLauncher() {
        let name = launcherName.trimmingCharacters(in: .whitespaces)
        guard !name.isEmpty else { return }
        model.saveLauncher(name: name, program: trimmedProgram, arguments: argumentList, in: bottle)
        naming = false
    }
}

enum ArgumentSplitter {
    /// Splits an argument string like a shell: spaces separate, double quotes group.
    static func split(_ s: String) -> [String] {
        var args: [String] = []
        var current = ""
        var quoted = false
        var any = false
        for ch in s {
            if ch == "\"" { quoted.toggle(); any = true; continue }
            if ch == " " && !quoted {
                if any || !current.isEmpty { args.append(current) }
                current = ""; any = false
                continue
            }
            current.append(ch)
        }
        if any || !current.isEmpty { args.append(current) }
        return args
    }
}

struct ToolsCard: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 12) {
                Text("Windows 工具").font(.system(size: 15, weight: .semibold))
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 150), spacing: 10)], spacing: 10) {
                    ForEach(WindowsTool.allCases) { tool in
                        Button { model.runTool(tool, in: bottle) } label: {
                            HStack(spacing: 10) {
                                Image(systemName: tool.symbol).frame(width: 18).foregroundStyle(Theme.textSecondary)
                                Text(tool.title).font(.system(size: 13)).lineLimit(1).fixedSize()
                                Spacer(minLength: 0)
                            }
                            .padding(.horizontal, 12).frame(height: 38)
                            .background(Theme.raised, in: RoundedRectangle(cornerRadius: 9))
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
        }
    }
}

struct LaunchersCard: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle
    let launchers: [BottleConfig.Launcher]

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 8) {
                Text("我的启动器").font(.system(size: 15, weight: .semibold)).padding(.bottom, 4)
                ForEach(launchers) { launcher in
                    HStack(spacing: 10) {
                        Image(systemName: "app.badge").foregroundStyle(Theme.textSecondary).frame(width: 18)
                        VStack(alignment: .leading, spacing: 1) {
                            Text(launcher.name).font(.system(size: 13.5, weight: .medium))
                            Text(([launcher.program] + launcher.arguments).joined(separator: " "))
                                .font(.system(size: 11.5)).foregroundStyle(Theme.textTertiary).lineLimit(1).truncationMode(.middle)
                        }
                        Spacer()
                        Button { model.runCommand(program: launcher.program, arguments: launcher.arguments, in: bottle, verbose: false) } label: {
                            Image(systemName: "play.fill")
                        }
                        .buttonStyle(.plain).foregroundStyle(Theme.accent).help("运行")
                        Button { model.removeLauncher(id: launcher.id, in: bottle) } label: { Image(systemName: "xmark") }
                            .buttonStyle(.plain).foregroundStyle(Theme.textTertiary).help("删除启动器")
                    }
                    .frame(height: 36)
                }
            }
        }
    }
}

struct BottleSettingsCard: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle
    @State private var pendingEngine: String?

    private var sync: SyncMode { SyncMode(setting: bottle.config.settings[SyncMode.settingKey]) }

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 12) {
                Text("设置").font(.system(size: 15, weight: .semibold))
                Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 10) {
                    GridRow {
                        label("区域")
                        Picker("区域", selection: Binding(get: { bottle.config.locale.ui }, set: { ui in
                            if let locale = BottleLocale.named(ui), locale != bottle.config.locale { model.setLocale(locale, for: bottle) }
                        })) {
                            ForEach(BottleLocale.all, id: \.ui) { Text($0.displayName).tag($0.ui) }
                        }
                        .labelsHidden()
                    }
                    GridRow {
                        label("Windows")
                        Picker("Windows", selection: Binding(get: { bottle.config.template }, set: { t in
                            if t != bottle.config.template { model.setWindowsVersion(t, for: bottle) }
                        })) {
                            ForEach(BottleTemplate.allCases, id: \.self) { Text("Windows \($0.winver.dropFirst(3))").tag($0) }
                        }
                        .labelsHidden()
                    }
                    GridRow {
                        label("引擎")
                        Picker("引擎", selection: Binding(get: { bottle.config.engine.id }, set: { id in
                            if id != bottle.config.engine.id { pendingEngine = id }
                        })) {
                            ForEach(model.environment.engines, id: \.self) { Text($0).tag($0) }
                        }
                        .labelsHidden()
                    }
                    GridRow {
                        label("同步")
                        Picker("同步", selection: Binding(get: { sync }, set: { m in
                            if m != sync { model.setSync(m, for: bottle) }
                        })) {
                            Text("msync（推荐）").tag(SyncMode.msync)
                            Text("wineserver（兼容）").tag(SyncMode.server)
                        }
                        .labelsHidden()
                    }
                }
                .font(.system(size: 13))
                Toggle(isOn: Binding(get: { bottle.config.settings[BottleStore.highResolutionKey] == "1" },
                                     set: { model.setHighResolution($0, for: bottle) })) {
                    VStack(alignment: .leading, spacing: 1) {
                        Text("高分辨率模式").font(.system(size: 13))
                        Text("按 Retina 像素渲染、DPI 192：文字更清晰，不支持高 DPI 的老程序窗口会变小")
                            .font(.system(size: 11)).foregroundStyle(Theme.textTertiary)
                    }
                }
                .toggleStyle(.switch).controlSize(.small)
                Toggle(isOn: Binding(get: { bottle.config.settings[BottleStore.advertiseAVXKey] == "1" },
                                     set: { model.setAdvertiseAVX($0, for: bottle) })) {
                    VStack(alignment: .leading, spacing: 1) {
                        Text("报告 AVX 支持").font(.system(size: 13))
                        Text("要求 AVX 才能启动的新游戏需要（macOS 15 起）").font(.system(size: 11)).foregroundStyle(Theme.textTertiary)
                    }
                }
                .toggleStyle(.switch).controlSize(.small)
                Text("换引擎前会自动做快照，出问题可以一键回滚。改同步方式会先结束瓶子里的程序。")
                    .font(.system(size: 11.5)).foregroundStyle(Theme.textTertiary)
            }
        }
        .alert("切换到 \(pendingEngine ?? "")？", isPresented: Binding(get: { pendingEngine != nil }, set: { if !$0 { pendingEngine = nil } })) {
            Button("切换") { if let id = pendingEngine { model.switchEngine(id, for: bottle) }; pendingEngine = nil }
            Button("取消", role: .cancel) { pendingEngine = nil }
        } message: {
            Text("会先结束瓶子里的程序并自动快照，然后用新引擎更新瓶子。")
        }
    }

    private func label(_ s: String) -> some View {
        Text(s).foregroundStyle(Theme.textTertiary).frame(width: 64, alignment: .leading)
    }
}

struct SnapshotsCard: View {
    @Environment(AppModel.self) private var model
    let bottle: Bottle
    @State private var confirmRestore: BottleSnapshot?

    var body: some View {
        let snapshots = model.snapshots(of: bottle)
        Card {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text("快照").font(.system(size: 15, weight: .semibold))
                    Spacer()
                    Button("立即快照") { model.takeSnapshot(bottle) }.buttonStyle(.plain)
                        .font(.system(size: 12.5, weight: .semibold)).foregroundStyle(Theme.accent)
                        .disabled(model.busy.contains("snapshot:\(bottle.config.id)"))
                }
                if snapshots.isEmpty {
                    Text("还没有快照。快照几乎不占空间（APFS 克隆），换引擎前会自动创建。")
                        .font(.system(size: 12.5)).foregroundStyle(Theme.textTertiary)
                } else {
                    ForEach(snapshots.prefix(8)) { snapshot in
                        HStack(spacing: 8) {
                            VStack(alignment: .leading, spacing: 1) {
                                Text(snapshot.date?.formatted(date: .abbreviated, time: .shortened) ?? snapshot.id)
                                    .font(.system(size: 13, weight: .medium))
                                Text(snapshot.reason).font(.system(size: 11.5)).foregroundStyle(Theme.textTertiary).lineLimit(1)
                            }
                            Spacer()
                            Button("恢复") { confirmRestore = snapshot }.buttonStyle(.plain)
                                .font(.system(size: 12, weight: .semibold)).foregroundStyle(Theme.accent)
                            Button { model.deleteSnapshot(snapshot, of: bottle) } label: { Image(systemName: "xmark") }
                                .buttonStyle(.plain).foregroundStyle(Theme.textTertiary).help("删除快照")
                        }
                    }
                }
            }
        }
        .alert("恢复到这个快照？", isPresented: Binding(get: { confirmRestore != nil }, set: { if !$0 { confirmRestore = nil } })) {
            Button("恢复") { if let s = confirmRestore { model.restore(bottle, to: s) }; confirmRestore = nil }
            Button("取消", role: .cancel) { confirmRestore = nil }
        } message: {
            Text("瓶子会回到快照时的样子（包括当时的引擎）。现在的状态会先存成一个新快照，随时可以再恢复回来。")
        }
    }
}

struct CreateBottleSheet: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var locale = "zh-Hans"
    @State private var template: BottleTemplate = .win10_64

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            Text("新建瓶子").font(.system(size: 20, weight: .semibold))
            Text("瓶子是独立的 64 位 Windows 环境。日文游戏建议用「日本語」区域，中文软件用简体或繁体中文。")
                .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
            Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 12) {
                GridRow {
                    Text("名称").foregroundStyle(Theme.textTertiary)
                    TextField("例如：Galgame", text: $name).textFieldStyle(.roundedBorder)
                }
                GridRow {
                    Text("区域").foregroundStyle(Theme.textTertiary)
                    Picker("区域", selection: $locale) {
                        ForEach(BottleLocale.all, id: \.ui) { Text($0.displayName).tag($0.ui) }
                    }
                    .labelsHidden()
                }
                GridRow {
                    Text("Windows").foregroundStyle(Theme.textTertiary)
                    Picker("Windows", selection: $template) {
                        ForEach(BottleTemplate.allCases, id: \.self) { Text("Windows \($0.winver.dropFirst(3))").tag($0) }
                    }
                    .labelsHidden()
                }
            }
            .font(.system(size: 13))
            HStack {
                Spacer()
                Button("取消") { dismiss() }.buttonStyle(OutlineButtonStyle(height: 34))
                Button("创建") {
                    if let l = BottleLocale.named(locale) {
                        model.createBottle(name: name.trimmingCharacters(in: .whitespaces), locale: l, template: template)
                    }
                    dismiss()
                }
                .buttonStyle(AccentButtonStyle(height: 34))
                .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
        .padding(26)
        .frame(width: 460)
        .background(Theme.card)
        .foregroundStyle(Theme.text)
    }
}

/// Copies CrossOver/Whisky bottles into Cider (docs/plan/05 §15 row 18 ★). The originals are never modified.
struct ForeignImportSheet: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var found: [ForeignBottle] = []
    @State private var locale = "zh-Hans"

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("从 CrossOver / Whisky 导入").font(.system(size: 20, weight: .semibold))
            Text("会把瓶子完整复制一份（APFS 克隆，几乎不占额外空间），原来的瓶子不受影响。只支持 64 位瓶子。")
                .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
            HStack {
                Text("区域").foregroundStyle(Theme.textTertiary)
                Picker("区域", selection: $locale) {
                    ForEach(BottleLocale.all, id: \.ui) { Text($0.displayName).tag($0.ui) }
                }
                .labelsHidden().frame(width: 160)
            }
            .font(.system(size: 13))
            if found.isEmpty {
                Text("这台 Mac 上没有找到 CrossOver 或 Whisky 的瓶子。").font(.system(size: 13)).foregroundStyle(Theme.textTertiary)
            } else {
                ForEach(found) { foreign in
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(foreign.name).font(.system(size: 14, weight: .medium))
                            Text(foreign.source.rawValue).font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                        }
                        Spacer()
                        Button("导入") {
                            if let l = BottleLocale.named(locale) { model.importForeign(foreign, locale: l) }
                            dismiss()
                        }
                        .buttonStyle(OutlineButtonStyle(height: 30))
                    }
                    .padding(10)
                    .background(Theme.raised, in: RoundedRectangle(cornerRadius: 10))
                }
            }
            HStack { Spacer(); Button("关闭") { dismiss() }.buttonStyle(OutlineButtonStyle(height: 34)) }
        }
        .padding(26)
        .frame(width: 480)
        .background(Theme.card)
        .foregroundStyle(Theme.text)
        .onAppear { found = model.foreignBottles() }
    }
}
