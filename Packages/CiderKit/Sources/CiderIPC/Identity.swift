import Foundation
import Security

/// Exact code requirements, enforced by XPC against the connected process (not a PID/path check).
public struct RPCIdentity: Sendable {
    public let gui: String
    public let client: String
    public let broker: String
    public init(bundle: URL) throws {
        gui = try Self.requirement(at: bundle)
        client = try Self.requirement(at: bundle.appendingPathComponent("Contents/MacOS/ciderctl"))
        broker = try Self.requirement(at: bundle.appendingPathComponent("Contents/MacOS/CiderXPCBridge"))
    }
    public static func requirement(at path: URL) throws -> String {
        var code: SecStaticCode?
        let flags = SecCSFlags(rawValue: 0)
        guard SecStaticCodeCreateWithPath(path as CFURL, flags, &code) == errSecSuccess, let code,
              SecStaticCodeCheckValidity(code, SecCSFlags(rawValue: kSecCSStrictValidate | kSecCSCheckAllArchitectures), nil) == errSecSuccess else {
            throw RPCFailure("Cider组件签名无效：\(path.lastPathComponent)")
        }
        var requirement: SecRequirement?
        guard SecCodeCopyDesignatedRequirement(code, flags, &requirement) == errSecSuccess, let requirement else {
            throw RPCFailure("无法核对Cider组件身份。")
        }
        var string: CFString?
        guard SecRequirementCopyString(requirement, flags, &string) == errSecSuccess, let string else {
            throw RPCFailure("无法读取Cider组件身份。")
        }
        return string as String
    }
}
