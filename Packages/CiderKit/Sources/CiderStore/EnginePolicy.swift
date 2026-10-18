import CiderCore
import Foundation

/// Exact native module pairs from recorded builds containing the child-process policy.
/// An imported manifest's claims are not capability evidence. New builds need a new recorded binding.
public enum EnginePolicy {
    private static let childPolicyModulePairs: Set<String> = [
        "d41738dba3315d992206f280fa5b0087f6db6077149c17473dbf1ad2a8c48a9a:4286fb870ef5ea4025b32cfa6f36aae9a228a5efb8b1b044b23566459a7dc2c9",
        "a9e5d97fd8596865df93829e93bef843da0bafd137bf0999bc26e7254c5aa4fd:4286fb870ef5ea4025b32cfa6f36aae9a228a5efb8b1b044b23566459a7dc2c9",
        // Isolated protocol-1810 build; exact source/ABI/policy evidence in research71.
        "146c27ac1932ff413fc986991fcf02ef9f1da264897e1fc393a1a764e3654a64:a3d006b5b8a70fc3a833f6876ce8feacdd3d25e43c49533d67b3cc29c27ae4c2",
    ]

    public static func supportsChildPreflight(_ engine: InstalledEngine) throws -> Bool {
        guard engine.manifest.cpuBackend == "rosetta-x86_64" else { return false }
        let module = try FileSafety.child("lib/wine/x86_64-unix/ntdll.so", in: engine.wineRoot)
        guard FileSafety.exists(module) else { return false }
        let server = try FileSafety.child("bin/wineserver", in: engine.wineRoot)
        guard FileSafety.exists(server) else { return false }
        let pair = try EngineStore.sha256(of: module) + ":" + EngineStore.sha256(of: server)
        guard childPolicyModulePairs.contains(pair) else { return false }
        let loadingChain = [
            "lib/wine/x86_64-unix/wine": "12cb5248e71b738f75b85e5a15831c4ae98958f07d356e318a4962ee43c54941",
            "bin/wine": "f7b6346aa0804aed3a6da2438dd2038b5e96a00ae8b2644bb1a15ca078f0740c",
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
