// swift-tools-version:6.0
import PackageDescription

let package = Package(
    name: "CiderCommands",
    platforms: [.macOS(.v14)],
    products: [.library(name: "CiderCommands", targets: ["CiderCommands"])],
    dependencies: [
        .package(path: "../../Packages/CiderKit"),
        .package(url: "https://github.com/apple/swift-argument-parser", from: "1.5.0"),
    ],
    targets: [
        .target(name: "CiderCommands", dependencies: [
            .product(name: "CiderKit", package: "CiderKit"),
            .product(name: "ArgumentParser", package: "swift-argument-parser"),
        ]),
        .testTarget(name: "CiderCommandsTests", dependencies: [
            "CiderCommands",
            .product(name: "CiderKit", package: "CiderKit"),
            .product(name: "ArgumentParser", package: "swift-argument-parser"),
        ]),
    ]
)
