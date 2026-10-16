// swift-tools-version:6.0
import PackageDescription

let package = Package(
    name: "ciderctl",
    platforms: [.macOS(.v14)],
    dependencies: [
        .package(path: "../../Packages/CiderKit"),
        .package(path: "../CiderCommands"),
    ],
    targets: [
        .executableTarget(
            name: "ciderctl",
            dependencies: [
                .product(name: "CiderIPC", package: "CiderKit"),
                .product(name: "CiderCommands", package: "CiderCommands"),
            ]
        ),
    ]
)
