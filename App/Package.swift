// swift-tools-version:6.0
import PackageDescription

// The macOS app. Built with `scripts/build-app.sh`, which wraps the executable into Cider.app.
// Open this Package.swift in Xcode for SwiftUI previews.
let package = Package(
    name: "CiderApp",
    platforms: [.macOS(.v14)],
    dependencies: [
        .package(path: "../Packages/CiderKit"),
    ],
    targets: [
        .executableTarget(
            name: "Cider",
            dependencies: [.product(name: "CiderKit", package: "CiderKit")],
            path: "Sources/Cider"
        ),
    ]
)
