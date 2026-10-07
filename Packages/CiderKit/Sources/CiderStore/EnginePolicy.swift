import CiderCore
import Foundation

/// Exact native modules from Cider's recorded r1/r2 builds containing the child-process policy.
/// An imported manifest's claims are not capability evidence. New builds need a new recorded binding.
public enum EnginePolicy {
    private static let childPolicyModules: Set<String> = [
        "d41738dba3315d992206f280fa5b0087f6db6077149c17473dbf1ad2a8c48a9a",
        "a9e5d97fd8596865df93829e93bef843da0bafd137bf0999bc26e7254c5aa4fd",
    ]

    public static func supportsChildPreflight(_ engine: InstalledEngine) throws -> Bool {
        guard engine.manifest.cpuBackend == "rosetta-x86_64" else { return false }
        let module = try FileSafety.child("lib/wine/x86_64-unix/ntdll.so", in: engine.wineRoot)
        guard FileSafety.exists(module) else { return false }
        guard childPolicyModules.contains(try EngineStore.sha256(of: module)) else { return false }
        let loadingChain = [
            "lib/wine/x86_64-unix/wine": "12cb5248e71b738f75b85e5a15831c4ae98958f07d356e318a4962ee43c54941",
            "bin/wine": "f7b6346aa0804aed3a6da2438dd2038b5e96a00ae8b2644bb1a15ca078f0740c",
            "bin/wineserver": "4286fb870ef5ea4025b32cfa6f36aae9a228a5efb8b1b044b23566459a7dc2c9",
        ]
        for (path, hash) in loadingChain {
            guard try EngineStore.sha256(of: FileSafety.child(path, in: engine.wineRoot)) == hash else { return false }
        }
        return true
    }

    public static func requireChildPreflight(_ engine: InstalledEngine) throws {
        guard try supportsChildPreflight(engine) else {
            throw CiderError.invalid("这个引擎的子进程启动策略尚未验证，无法保证阻止不支持的游戏启动。请使用已验证的 Cider 引擎。")
        }
    }
}
