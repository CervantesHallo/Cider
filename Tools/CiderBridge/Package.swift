// swift-tools-version:6.0
import PackageDescription
let package = Package(name: "CiderBridge", platforms: [.macOS(.v14)],
    dependencies: [.package(path: "../../Packages/CiderKit")],
    targets: [.executableTarget(name: "CiderXPCBridge", dependencies: [.product(name: "CiderIPC", package: "CiderKit")])])
