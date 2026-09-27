import SwiftUI

@main
struct CiderApp: App {
    @State private var model = AppModel()

    var body: some Scene {
        WindowGroup("Cider") {
            RootView()
                .environment(model)
                .frame(minWidth: 1180, minHeight: 760)
                .preferredColorScheme(.dark)
                .task { await model.refresh(); model.startMonitoring() }
        }
        .windowStyle(.hiddenTitleBar)
        .commands {
            CommandGroup(after: .newItem) {
                Button("刷新资料库") { Task { await model.refresh() } }
                    .keyboardShortcut("r")
            }
        }
    }
}
