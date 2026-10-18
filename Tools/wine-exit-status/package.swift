import CryptoKit
import Foundation

let arguments = CommandLine.arguments
guard arguments.count == 3 else { exit(64) }
let directory = URL(fileURLWithPath: arguments[1], isDirectory: true)
func digest(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
func u16(_ data: Data, _ offset: Int) -> UInt16 { UInt16(data[offset]) | UInt16(data[offset + 1]) << 8 }
func u32(_ data: Data, _ offset: Int) -> UInt32 { UInt32(u16(data, offset)) | UInt32(u16(data, offset + 2)) << 16 }
var collectors: [String: Any] = [:]
for (arch, machine) in [("x86_64", UInt16(0x8664)), ("i686", UInt16(0x14c))] {
    let name = "cider-exit-status-\(arch).exe"
    let bytes = try Data(contentsOf: directory.appendingPathComponent(name))
    guard bytes.count >= 64, u16(bytes, 0) == 0x5a4d else { exit(65) }
    let pe = Int(u32(bytes, 0x3c))
    guard pe <= bytes.count - 24, u32(bytes, pe) == 0x4550, u16(bytes, pe + 4) == machine,
          u16(bytes, pe + 22) & 0x2002 == 0x0002 else { exit(65) }
    collectors[arch] = ["file": name, "machine": machine, "sha256": digest(bytes)]
}
let manifest: [String: Any] = ["schema": "cider.windows-tools/v1", "collectors": collectors,
                             "sourceSHA256": digest(try Data(contentsOf: URL(fileURLWithPath: arguments[2])))]
try JSONSerialization.data(withJSONObject: manifest, options: [.sortedKeys, .prettyPrinted])
    .write(to: directory.appendingPathComponent("manifest.json"), options: .atomic)
