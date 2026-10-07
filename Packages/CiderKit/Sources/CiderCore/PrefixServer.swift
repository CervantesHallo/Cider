import Darwin
import Foundation

/// Read-only liveness used by storage and runtime. Unreadable state is conservatively treated as active.
public enum PrefixServer {
    public static func directory(for prefix: URL) -> URL? {
        var info = stat()
        guard stat(prefix.path, &info) == 0 else { return nil }
        return URL(fileURLWithPath: "/tmp/.wine-\(getuid())/server-\(String(UInt64(bitPattern: Int64(info.st_dev)), radix: 16))-\(String(info.st_ino, radix: 16))")
    }
    public static func isRunning(prefix: URL) -> Bool {
        guard let directory = directory(for: prefix) else { return errno != ENOENT }
        let descriptor = open(directory.appendingPathComponent("lock").path, O_RDWR | O_CLOEXEC)
        guard descriptor >= 0 else { return errno != ENOENT }
        defer { close(descriptor) }
        var query = flock()
        query.l_type = Int16(F_WRLCK)
        query.l_whence = Int16(SEEK_SET)
        guard fcntl(descriptor, F_GETLK, &query) == 0 else { return true }
        return query.l_type != Int16(F_UNLCK)
    }

    /// Include the loader-before-server window and orphaned Wine processes when replacing storage.
    public static func hasProcesses(prefix: URL, bottleID: String) -> Bool {
        let count = proc_listallpids(nil, 0)
        guard count > 0 else { return true }
        var pids = [pid_t](repeating: 0, count: Int(count) + 64)
        let filled = pids.withUnsafeMutableBufferPointer { proc_listallpids($0.baseAddress, Int32($0.count * MemoryLayout<pid_t>.size)) }
        for pid in pids.prefix(Int(max(0, filled))) where pid > 0 {
            var info = proc_bsdinfo()
            guard proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, Int32(MemoryLayout<proc_bsdinfo>.size)) > 0,
                  info.pbi_uid == getuid(), info.pbi_status != SZOMB else { continue }
            var mib: [Int32] = [CTL_KERN, KERN_PROCARGS2, pid]
            var size = 0
            if sysctl(&mib, 3, nil, &size, nil, 0) == 0, size > 4 {
                var bytes = [UInt8](repeating: 0, count: size)
                if sysctl(&mib, 3, &bytes, &size, nil, 0) == 0 {
                    let strings = bytes[4..<size].split(separator: 0).map { String(decoding: $0, as: UTF8.self) }
                    if strings.contains("CIDER_BOTTLE=" + bottleID) && strings.contains("WINEPREFIX=" + prefix.path) { return true }
                    continue
                }
            }
            var path = [CChar](repeating: 0, count: 4096)
            if proc_pidpath(pid, &path, UInt32(path.count)) > 0 {
                let executable = String(decoding: path.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
                if executable.contains("/wine") || executable.contains("/winetemp-") || executable.hasSuffix(".exe") { return true }
            }
        }
        return false
    }
}
