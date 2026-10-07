import CiderCore
import Darwin
import Foundation

/// Whether a prefix currently has a running wineserver.
///
/// wineserver keeps its socket and a `lock` file in `/tmp/.wine-<uid>/server-<dev>-<ino>/`, where dev/ino identify
/// the prefix directory, and holds an fcntl write lock on `lock` for as long as it runs. Asking for that lock is a
/// cheap, side-effect-free liveness check (a stale directory left by a crash has no lock holder).
public enum BottleActivity {
    public static func serverDirectory(forPrefix prefix: URL) -> URL? {
        PrefixServer.directory(for: prefix)
    }

    public static func isRunning(prefix: URL) -> Bool {
        PrefixServer.isRunning(prefix: prefix)
    }
}
