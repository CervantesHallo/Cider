import CiderBottle
import CiderCore
import CiderRuntime
import CiderSchema
import Foundation

/// Owns slow dependency preparation and the following short spawn as one
/// cancellable product operation. Call before the outermost bottle lock.
public enum LaunchPreparation {
    public static func perform<T>(plan: LaunchPlan, using runner: WineRunner,
                                  checking check: @escaping @Sendable () throws -> Void = {},
                                  progress: @Sendable (String) -> Void = { _ in },
                                  _ work: (WineRunner) throws -> T) throws -> T {
        try Task.checkCancellation()
        try check()
        let bottle = try BottleStore(paths: runner.paths).bottle(runner.bottleID)
        guard bottle.prefix.standardizedFileURL.path == runner.prefix.standardizedFileURL.path,
              bottle.config.engine.id == runner.engine.manifest.id else {
            throw CiderError.invalid("瓶子或引擎已改变，请重新发起启动。")
        }
        return try InstallationActivity.perform(in: bottle, onStop: {}) {
            let guarded = runner.checkingLaunches {
                try InstallationActivity.checkCancellation()
                try check()
            }
            let prepared = try guarded.preparingMedia(for: plan, progress: progress)
            try InstallationActivity.checkCancellation()
            return try work(prepared)
        }
    }

    public static func launch(_ plan: LaunchPlan, using runner: WineRunner,
                              checking check: @escaping @Sendable () throws -> Void = {},
                              progress: @Sendable (String) -> Void = { _ in }) throws -> SessionHandle {
        try perform(plan: plan, using: runner, checking: check, progress: progress) {
            try $0.launch(plan)
        }
    }
}
