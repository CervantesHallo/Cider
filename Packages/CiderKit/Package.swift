// swift-tools-version:6.0
import PackageDescription

let package = Package(
    name: "CiderKit",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "CiderKit", targets: ["CiderCore", "CiderSchema", "CiderStore", "CiderRuntime", "CiderBottle", "CiderIntegration", "CiderPE", "CiderData"]),
    ],
    targets: [
        .target(name: "CiderCore"),
        .target(name: "CiderSchema", dependencies: ["CiderCore"]),
        .target(name: "CiderStore", dependencies: ["CiderCore", "CiderSchema"]),
        .target(name: "CiderRuntime", dependencies: ["CiderCore", "CiderSchema", "CiderStore", "CiderData"]),
        .target(name: "CiderBottle", dependencies: ["CiderCore", "CiderSchema", "CiderStore", "CiderRuntime"]),
        .target(name: "CiderPE"),
        .target(name: "CiderData"),
        .target(name: "CiderIntegration", dependencies: ["CiderCore", "CiderBottle", "CiderRuntime", "CiderStore", "CiderPE", "CiderData"]),
        .testTarget(
            name: "CiderKitTests",
            dependencies: ["CiderCore", "CiderSchema", "CiderStore", "CiderRuntime", "CiderBottle", "CiderIntegration", "CiderPE", "CiderData"]
        ),
    ]
)
