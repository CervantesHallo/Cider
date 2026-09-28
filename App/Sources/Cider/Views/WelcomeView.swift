import AppKit
import CiderData
import CiderStore
import SwiftUI
import UniformTypeIdentifiers

/// First run (design canvas "首次运行"): Rosetta, an engine, then a first thing to install.
struct WelcomeView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var importingEngine = false

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            HStack(spacing: 12) {
                Image(systemName: "sparkles").font(.system(size: 26)).foregroundStyle(Theme.accent)
                VStack(alignment: .leading, spacing: 2) {
                    Text("欢迎使用 Cider").font(.system(size: 22, weight: .semibold))
                    Text("在 Mac 上运行 Windows 软件和游戏。先完成下面几步。").font(.system(size: 13)).foregroundStyle(Theme.textSecondary)
                }
            }
            step(1, "Rosetta", done: model.environment.rosetta,
                 detail: model.environment.rosetta ? "已安装" : "Windows 程序经 Rosetta 在 Apple 芯片上运行。安装时系统会请你输入密码。") {
                if !model.environment.rosetta { Button("安装 Rosetta") { model.installRosetta() }.buttonStyle(AccentButtonStyle(height: 32)) }
            }
            step(2, "运行引擎", done: !model.environment.engines.isEmpty,
                 detail: model.environment.engines.first.map { "已安装：\($0)" } ?? "Cider 的 Wine 引擎，下载后会校验完整性再安装。") {
                if model.environment.engines.isEmpty {
                    EngineDownloadButton()
                    Button("从文件导入…") { importingEngine = true }.buttonStyle(OutlineButtonStyle(height: 32))
                        .disabled(model.busy.contains("engine"))
                }
            }
            step(3, "装点什么", done: !model.bottles.isEmpty,
                 detail: model.bottles.isEmpty ? "推荐先装 Steam：之后在 Steam 里下载的游戏会自动出现在资料库。" : "已经有瓶子了，可以随时在「安装软件」里装更多。") {
                if model.bottles.isEmpty, let steam = model.compat.recipes["launcher.steam"] {
                    Button("安装 Steam") { model.installRecipe(steam, bottleID: nil); finish() }
                        .buttonStyle(AccentButtonStyle(height: 32))
                        .disabled(!model.environment.ready)
                    Button("其他软件") { model.section = .install; finish() }.buttonStyle(OutlineButtonStyle(height: 32))
                }
            }
            HStack {
                Spacer()
                Button(model.environment.ready ? "开始使用" : "稍后再说") { finish() }.buttonStyle(OutlineButtonStyle(height: 34))
            }
        }
        .padding(28)
        .frame(width: 560)
        .background(Theme.card)
        .foregroundStyle(Theme.text)
        .fileImporter(isPresented: $importingEngine, allowedContentTypes: [.folder, UTType(filenameExtension: "xz") ?? .data, .gzip]) { result in
            if case .success(let url) = result { model.importEngine(url) }
        }
    }

    private func finish() {
        UserDefaults.standard.set(true, forKey: WelcomeView.doneKey)
        dismiss()
    }

    static let doneKey = "welcomeDone"

    @ViewBuilder
    private func step<Actions: View>(_ n: Int, _ title: String, done: Bool, detail: String,
                                     @ViewBuilder actions: () -> Actions) -> some View {
        HStack(alignment: .top, spacing: 14) {
            ZStack {
                Circle().fill(done ? Theme.good.opacity(0.18) : Theme.raised).frame(width: 30, height: 30)
                if done { Image(systemName: "checkmark").font(.system(size: 12, weight: .heavy)).foregroundStyle(Theme.good) }
                else { Text("\(n)").font(.system(size: 13, weight: .semibold)).foregroundStyle(Theme.textSecondary) }
            }
            VStack(alignment: .leading, spacing: 6) {
                Text(title).font(.system(size: 15, weight: .semibold))
                Text(detail).font(.system(size: 12.5)).foregroundStyle(Theme.textSecondary)
                HStack(spacing: 8) { actions() }
            }
            Spacer()
        }
    }
}

/// Downloads the recommended engine from the bundled index, with progress.
struct EngineDownloadButton: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        if let entry = model.engineIndex?.recommended {
            if let progress = model.engineDownloadProgress {
                HStack(spacing: 8) {
                    ProgressView(value: progress).frame(width: 160)
                    Text(progress >= 1 ? "正在校验并安装…" : "\(Int(progress * 100))%").font(.system(size: 12)).foregroundStyle(Theme.textSecondary)
                }
            } else {
                Button("下载引擎（\(ByteCountFormatter.string(fromByteCount: entry.size, countStyle: .file))）") { model.downloadEngine(entry) }
                    .buttonStyle(AccentButtonStyle(height: 32))
            }
        }
    }
}
