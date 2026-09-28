import AppKit
import CiderBottle
import CiderData
import CiderPE
import CiderSchema
import CiderStore
import SwiftUI
import UniformTypeIdentifiers

struct InstallView: View {
    @Environment(AppModel.self) private var model
    @State private var bottleID: String?
    @State private var dropTargeted = false
    @State private var showImporter = false
    @State private var pending: (url: URL, info: InstallerInfo)?
    @State private var lastInfo: InstallerInfo?

    var body: some View {
        VStack(spacing: 0) {
            PageHeader(title: "安装 Windows 软件") { EmptyView() }
            ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                if model.bottles.isEmpty {
                    RecipeCatalog()
                    Card {
                        VStack(alignment: .leading, spacing: 8) {
                            Text("安装未收录的软件，先创建一个瓶子").font(.system(size: 16, weight: .semibold))
                            Text("瓶子是一个独立的 Windows 环境。到「瓶子」页新建一个；上面目录里的软件会自动建好合适的瓶子。").foregroundStyle(Theme.textSecondary)
                        }
                    }
                } else {
                    RecipeCatalog()
                    Text("安装未收录的软件").font(.system(size: 17, weight: .semibold)).padding(.top, 8)
                    HStack(spacing: 12) {
                        Text("安装到").font(.system(size: 14, weight: .semibold))
                        Picker("瓶子", selection: Binding(get: { bottleID ?? model.bottles.first?.config.id }, set: { bottleID = $0 })) {
                            ForEach(model.bottles, id: \.config.id) { b in Text(b.config.name).tag(Optional(b.config.id)) }
                        }
                        .labelsHidden()
                        .frame(width: 240)
                    }
                    Button { showImporter = true } label: {
                        VStack(spacing: 10) {
                            Image(systemName: "square.and.arrow.down.on.square").font(.system(size: 30, weight: .light))
                            Text("把 .exe 或 .msi 拖到这里，或点按选择文件").font(.system(size: 14))
                            Text("安装程序会在所选瓶子里运行").font(.system(size: 12)).foregroundStyle(Theme.textTertiary)
                        }
                        .foregroundStyle(Theme.textSecondary)
                        .frame(maxWidth: .infinity)
                        .frame(height: 220)
                        .background(dropTargeted ? Theme.raised : .clear, in: RoundedRectangle(cornerRadius: 16))
                        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(style: StrokeStyle(lineWidth: 1.5, dash: [6, 5])).foregroundStyle(Theme.control))
                    }
                    .buttonStyle(.plain)
                    .disabled(model.busy.contains("install"))
                    .dropDestination(for: URL.self) { urls, _ in
                        guard let url = urls.first, ["exe", "msi"].contains(url.pathExtension.lowercased()) else { return false }
                        install(url)
                        return true
                    } isTargeted: { dropTargeted = $0 }
                    .fileImporter(isPresented: $showImporter, allowedContentTypes: [.exe, .msi]) { result in
                        if case .success(let url) = result { install(url) }
                    }
                    if let pending { localeMismatchCard(pending.url, pending.info) }
                    if let info = lastInfo, info.kind != .unknown || info.product != nil {
                        Text("检测到：" + [info.kind == .unknown ? nil : info.kind.rawValue, info.product, info.company]
                                .compactMap { $0 }.joined(separator: " · "))
                            .font(.system(size: 12.5)).foregroundStyle(Theme.textTertiary)
                    }
                    if model.busy.contains("install") {
                        HStack(spacing: 10) { ProgressView().controlSize(.small); Text("安装程序运行中…") }
                    }
                }
            }
            .padding(32)
            }
        }
    }

    private var selectedBottle: Bottle? {
        let id = bottleID ?? model.bottles.first?.config.id
        return model.bottles.first { $0.config.id == id }
    }

    private func install(_ url: URL) {
        guard let bottle = selectedBottle else { return }
        let info = InstallerInfo.inspect(fileAt: url)
        lastInfo = info
        if let wanted = info.locale, wanted != bottle.config.locale.ui {
            pending = (url, info)
            return
        }
        model.runInstaller(url, in: bottle.config.id)
    }

    @ViewBuilder
    private func localeMismatchCard(_ url: URL, _ info: InstallerInfo) -> some View {
        let wanted = info.locale.flatMap(BottleLocale.named) ?? .japanese
        let bottle = selectedBottle
        Card {
            VStack(alignment: .leading, spacing: 10) {
                Label("这是\(wanted.displayName)的安装程序", systemImage: "character.bubble").font(.system(size: 15, weight: .semibold))
                Text("\(info.product ?? url.lastPathComponent) 的界面语言是\(wanted.displayName)，而瓶子「\(bottle?.config.name ?? "")」是\(bottle?.config.locale.displayName ?? "")区域。区域不一致时，安装程序和游戏里经常出现乱码，有的根本装不上。")
                    .font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
                HStack(spacing: 10) {
                    Button("新建「\(wanted.displayName)」瓶子并安装") {
                        model.installIntoNewBottle(url, locale: wanted, name: info.product ?? url.deletingPathExtension().lastPathComponent)
                        pending = nil
                    }
                    .buttonStyle(AccentButtonStyle(height: 34))
                    Button("仍然装到「\(bottle?.config.name ?? "")」") {
                        if let bottle { model.runInstaller(url, in: bottle.config.id) }
                        pending = nil
                    }
                    .buttonStyle(OutlineButtonStyle(height: 34))
                    Button("取消") { pending = nil }.buttonStyle(.plain).foregroundStyle(Theme.textTertiary)
                }
            }
        }
    }
}

extension UTType {
    static let exe = UTType(filenameExtension: "exe") ?? .data
    static let msi = UTType(filenameExtension: "msi") ?? .data
}

struct EnvironmentView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        VStack(spacing: 0) {
            PageHeader(title: "运行环境") {
                Button { Task { await model.refresh() } } label: { Label("重新检查", systemImage: "arrow.clockwise") }
                    .buttonStyle(OutlineButtonStyle(height: 36))
            }
            VStack(spacing: 12) {
                check("Rosetta", ok: model.environment.rosetta, detail: model.environment.rosetta ? "已安装" : "运行 Windows 程序需要 Rosetta。在终端运行：softwareupdate --install-rosetta")
                check("运行引擎", ok: !model.environment.engines.isEmpty, detail: model.environment.engines.joined(separator: "、").isEmpty ? "未安装" : model.environment.engines.joined(separator: "、"))
                if model.environment.engines.isEmpty || model.engineDownloadProgress != nil { EngineDownloadButton() }
                check("GStreamer（视频播放）", ok: model.environment.gstreamer, detail: model.environment.gstreamerBundled ? "引擎自带（MPEG-1/2、WMV、H.264、VP8/9、AV1 等）" : model.environment.gstreamer ? "使用 /Library/Frameworks 里的 GStreamer" : "未安装：游戏开场动画可能无法播放")
                D3DMetalCard()
                Spacer()
            }
            .padding(32)
        }
    }

    private func check(_ title: String, ok: Bool, detail: String) -> some View {
        HStack(spacing: 16) {
            Image(systemName: ok ? "checkmark" : "exclamationmark")
                .font(.system(size: 13, weight: .heavy))
                .foregroundStyle(ok ? Theme.good : Theme.tune)
                .frame(width: 28, height: 28)
                .background((ok ? Theme.good : Theme.tune).opacity(0.16), in: Circle())
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.system(size: 14.5, weight: .semibold))
                Text(detail).font(.system(size: 12.5)).foregroundStyle(Theme.textSecondary)
            }
            Spacer()
        }
        .padding(16)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 14))
        .overlay(RoundedRectangle(cornerRadius: 14).stroke(Theme.cardLine))
    }
}

/// App directory (docs/plan/05 §15 rows 1–3): recipes with one-click install into a chosen or a new bottle.
struct RecipeCatalog: View {
    @Environment(AppModel.self) private var model
    @State private var target: [String: String] = [:]      // recipe id → bottle id ("" = new bottle)

    var body: some View {
        let recipes = model.compat.recipes.values.sorted { ($0.kind == .launcher ? 0 : 1, $0.id) < ($1.kind == .launcher ? 0 : 1, $1.id) }
        VStack(alignment: .leading, spacing: 12) {
            Text("应用目录").font(.system(size: 17, weight: .semibold))
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 300), spacing: 16)], spacing: 16) {
                ForEach(recipes) { recipe in
                    Card {
                        VStack(alignment: .leading, spacing: 8) {
                            HStack {
                                Image(systemName: recipe.kind == .launcher ? "gamecontroller" : "shippingbox")
                                    .foregroundStyle(Theme.accent)
                                Text(recipe.title()).font(.system(size: 15, weight: .semibold))
                                Spacer()
                                Text(recipe.kind == .launcher ? "启动器" : recipe.kind == .component ? "运行库" : "应用")
                                    .font(.system(size: 11)).foregroundStyle(Theme.textTertiary)
                            }
                            Text(recipe.blurb()).font(.system(size: 12.5)).foregroundStyle(Theme.textSecondary)
                                .lineLimit(3).frame(maxWidth: .infinity, minHeight: 48, alignment: .topLeading)
                            HStack {
                                Picker("安装到", selection: Binding(get: { target[recipe.id] ?? defaultTarget(recipe) },
                                                                 set: { target[recipe.id] = $0 })) {
                                    if recipe.kind != .component {
                                        Text("新瓶子“\(recipe.bottle?.name ?? recipe.title())”").tag("")
                                    }
                                    ForEach(model.bottles, id: \.config.id) { Text($0.config.name).tag($0.config.id) }
                                }
                                .labelsHidden()
                                Button(model.busy.contains("recipe:\(recipe.id)") ? "安装中…" : "安装") {
                                    let t = target[recipe.id] ?? defaultTarget(recipe)
                                    model.installRecipe(recipe, bottleID: t.isEmpty ? nil : t)
                                }
                                .buttonStyle(AccentButtonStyle(height: 30))
                                .disabled(model.busy.contains("recipe:\(recipe.id)")
                                          || (recipe.kind == .component && model.bottles.isEmpty))
                            }
                        }
                    }
                }
            }
        }
    }

    /// Launchers get their own bottle by default; components go into the first bottle.
    private func defaultTarget(_ recipe: Recipe) -> String {
        recipe.kind == .component ? (model.bottles.first?.config.id ?? "") : ""
    }
}

/// D3DMetal (DirectX 12) comes only from Apple's Game Porting Toolkit, imported by the user (ADR-006).
struct D3DMetalCard: View {
    @Environment(AppModel.self) private var model
    @State private var picking = false
    @State private var imported: [(version: String, directory: URL, info: D3DMetalImporter.Imported)] = []

    var body: some View {
        HStack(alignment: .top, spacing: 16) {
            Image(systemName: imported.isEmpty ? "cube.transparent" : "checkmark")
                .font(.system(size: 13, weight: .heavy))
                .foregroundStyle(imported.isEmpty ? Theme.textTertiary : Theme.good)
                .frame(width: 28, height: 28)
                .background((imported.isEmpty ? Theme.textTertiary : Theme.good).opacity(0.16), in: Circle())
            VStack(alignment: .leading, spacing: 4) {
                Text("D3DMetal（DirectX 12）").font(.system(size: 14.5, weight: .semibold))
                if imported.isEmpty {
                    Text("DirectX 12 游戏需要 Apple 的 D3DMetal。它不随 Cider 分发：从 Apple 开发者网站下载 Game Porting Toolkit，然后把 DMG 导入这里。")
                        .font(.system(size: 12.5)).foregroundStyle(Theme.textSecondary)
                } else {
                    ForEach(imported, id: \.version) { entry in
                        HStack(spacing: 8) {
                            Text("\(entry.version) · \(entry.info.archs.joined(separator: "/")) · \(entry.info.codesignValid ? "Apple 签名有效" : "签名未通过验证")")
                                .font(.system(size: 12.5)).foregroundStyle(entry.info.codesignValid ? Theme.textSecondary : Theme.tune)
                            if entry.info.license != nil {
                                Button("许可协议") { NSWorkspace.shared.open(entry.directory.appendingPathComponent("License.rtf")) }
                                    .buttonStyle(.plain).font(.system(size: 12, weight: .semibold)).foregroundStyle(Theme.accent)
                            }
                        }
                    }
                }
            }
            Spacer()
            Button("导入 GPTK…") { picking = true }.buttonStyle(OutlineButtonStyle(height: 32))
                .disabled(model.busy.contains("d3dmetal"))
        }
        .padding(16)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 14))
        .overlay(RoundedRectangle(cornerRadius: 14).stroke(Theme.cardLine))
        .onAppear { imported = D3DMetalImporter(paths: model.paths).installed() }
        .fileImporter(isPresented: $picking, allowedContentTypes: [.diskImage, .folder]) { result in
            if case .success(let url) = result { model.importD3DMetal(url) { imported = D3DMetalImporter(paths: model.paths).installed() } }
        }
    }
}
