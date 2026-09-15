// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "EWAF",
    platforms: [.macOS(.v14)],
    products: [.executable(name: "EWAF", targets: ["EWAF"]), .library(name: "EWAFCore", targets: ["EWAFCore"])],
    targets: [
        .target(name: "EWAFCore"),
        .executableTarget(name: "EWAF", dependencies: ["EWAFCore"]),
        .testTarget(name: "EWAFAppTests", dependencies: ["EWAF", "EWAFCore"], path: "tests/EWAFAppTests"),
        .testTarget(name: "EWAFCoreTests", dependencies: ["EWAFCore"], path: "tests/EWAFCoreTests", resources: [.copy("Fixtures")])
    ]
)
