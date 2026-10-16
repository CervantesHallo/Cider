// swift-tools-version:6.0
import PackageDescription

let package = Package(
    name: "CiderXPCFixture",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "Cider", targets: ["FixtureHost"]),
        .executable(name: "ciderctl", targets: ["FixtureClient"]),
        .executable(name: "CiderXPCBridge", targets: ["FixtureBroker"]),
        .executable(name: "FixtureAppHost", targets: ["FixtureAppHost"]),
    ],
    dependencies: [
        .package(path: "../../../Packages/CiderKit"),
        .package(path: "../../../Tools/CiderCommands"),
    ],
    targets: [
        .target(name: "FixtureSupport", dependencies: [
            .product(name: "CiderIPC", package: "CiderKit"),
            .product(name: "CiderKit", package: "CiderKit"),
            .product(name: "CiderCommands", package: "CiderCommands"),
        ]),
        .executableTarget(name: "FixtureHost", dependencies: ["FixtureSupport"]),
        .executableTarget(name: "FixtureClient", dependencies: ["FixtureSupport"]),
        .executableTarget(name: "FixtureBroker", dependencies: ["FixtureSupport"]),
        .executableTarget(name: "FixtureAppHost", dependencies: [
            .product(name: "CiderKit", package: "CiderKit"),
            .product(name: "CiderIPC", package: "CiderKit"),
            .product(name: "CiderCommands", package: "CiderCommands"),
        ]),
        .testTarget(name: "FixtureLocalTests", dependencies: [
            .product(name: "CiderKit", package: "CiderKit"),
            .product(name: "CiderIPC", package: "CiderKit"),
        ]),
    ]
)
