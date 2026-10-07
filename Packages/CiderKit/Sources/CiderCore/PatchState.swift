import Foundation

/// Shared by launch and copy: a partly patched prefix must not escape by losing its journal.
public enum PatchState {
    private struct State: Decodable { let state: String }
    public static func requireRecovered(in bottleDirectory: URL) throws {
        let history = try FileSafety.child(".cider/patches", in: bottleDirectory, rejectSymlinks: true)
        guard FileSafety.exists(history) else { return }
        let fm = FileManager.default
        for app in try fm.contentsOfDirectory(at: history, includingPropertiesForKeys: nil) where !app.lastPathComponent.hasPrefix(".") {
            _ = try FileSafety.child(".cider/patches/" + app.lastPathComponent, in: bottleDirectory, rejectSymlinks: true)
            guard (try fm.attributesOfItem(atPath: app.path)[.type] as? FileAttributeType) == .typeDirectory else {
                throw CiderError.invalid("补丁历史目录损坏。")
            }
            for record in try fm.contentsOfDirectory(at: app, includingPropertiesForKeys: nil)
                where !record.lastPathComponent.hasPrefix(".") && record.lastPathComponent != "sequence.json" {
                let journal = try FileSafety.child("journal.json", in: record, rejectSymlinks: true)
                guard FileSafety.exists(journal) else { continue }
                let value = try JSONFile.readMetadata(State.self, from: journal)
                guard ["installed", "undone", "rolled_back"].contains(value.state) else {
                    throw CiderError.invalid("补丁操作尚未恢复或记录损坏。请先在游戏详情恢复补丁，再启动或复制瓶子。")
                }
            }
        }
    }
}
