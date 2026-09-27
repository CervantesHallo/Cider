import Darwin
import Foundation

/// Whether a prefix currently has a running wineserver.
///
/// wineserver keeps its socket and a `lock` file in `/tmp/.wine-<uid>/server-<dev>-<ino>/`, where dev/ino identify
/// the prefix directory, and holds an fcntl write lock on `lock` for as long as it runs. Asking for that lock is a
/// cheap, side-effect-free liveness check (a stale directory left by a crash has no lock holder).
public enum BottleActivity {
    public static func serverDirectory(forPrefix prefix: URL) -> URL? {
        var st = stat()
        guard stat(prefix.path, &st) == 0 else { return nil }
        let dir = "/tmp/.wine-\(getuid())/server-\(String(UInt64(bitPattern: Int64(st.st_dev)), radix: 16))-\(String(st.st_ino, radix: 16))"
        return URL(fileURLWithPath: dir, isDirectory: true)
    }

    public static func isRunning(prefix: URL) -> Bool {
        guard let dir = serverDirectory(forPrefix: prefix) else { return false }
        let fd = open(dir.appendingPathComponent("lock").path, O_RDWR)
        guard fd >= 0 else { return false }
        defer { close(fd) }
        var lock = flock()
        lock.l_type = Int16(F_WRLCK)
        lock.l_whence = Int16(SEEK_SET)
        guard fcntl(fd, F_GETLK, &lock) == 0 else { return false }
        return lock.l_type != Int16(F_UNLCK)
    }
}
