// swift-tools-version:6.0
import PackageDescription

let package = Package(
    name: "ciderctl",
    platforms: [.macOS(.v14)],
    dependencies: [
        .package(path: "../../Packages/CiderKit"),
        .package(url: "https://github.com/apple/swift-argument-parser", from: "1.5.0"),
    ],
    targets: [
        .executableTarget(
            name: "ciderctl",
            dependencies: [
                .product(name: "CiderKit", package: "CiderKit"),
                .product(name: "ArgumentParser", package: "swift-argument-parser"),
            ]
        ),
    ]
)
