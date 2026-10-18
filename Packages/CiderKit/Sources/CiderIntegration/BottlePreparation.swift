import CiderBottle
import CiderCore
import CiderSchema
import Foundation

/// Product-level ownership from exclusive directory reservation through prefix
/// initialization, then transferred to the caller's install/launch workflow.
public enum BottlePreparation {
    public struct Prepared: Sendable {
        public let bottle: Bottle
        public let activity: RecipeInstaller.Activity
    }

    public static func create(store: BottleStore, name: String, template: BottleTemplate = .win10_64,
                              locale: BottleLocale, engineID: String? = nil, createdBy: String,
                              observation: CommandObservation? = nil,
                              progress: @Sendable (String) -> Void = { _ in }) throws -> Prepared {
        var activity: RecipeInstaller.Activity?
        do {
            let bottle = try store.create(name: name, template: template, locale: locale, engineID: engineID,
                createdBy: createdBy, onReserved: { reserved in
                    let owner = try RecipeInstaller.beginActivity(in: reserved, onStop: { observation?.detach() })
                    activity = owner
                    return { try owner.checkCancellation() }
                }, progress: progress)
            guard let activity else { throw CiderError.invalid("新瓶子初始化未建立活动归属。") }
            try activity.checkCancellation()
            return Prepared(bottle: bottle, activity: activity)
        } catch {
            try? activity?.finish(error: error)
            throw error
        }
    }
}
