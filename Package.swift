// swift-tools-version: 6.0
import PackageDescription
import Foundation
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().path

let package = Package(
    name: "EWAF",
    platforms: [.macOS(.v14)],
    products: [.executable(name: "EWAF", targets: ["EWAF"]), .library(name: "EWAFCore", targets: ["EWAFCore"])],
    targets: [
        .target(name: "CEWAF", path: "bindings", exclude: ["module.modulemap"], publicHeadersPath: "include"),
        .target(name: "EWAFCore", dependencies: ["CEWAF"], linkerSettings: [.unsafeFlags(["-L", root + "/target/swift"]), .linkedLibrary("ewaf_ffi")]),
        .executableTarget(name: "EWAF", dependencies: ["EWAFCore"]),
        .testTarget(name: "EWAFAppTests", dependencies: ["EWAF", "EWAFCore"], path: "tests/EWAFAppTests"),
        .testTarget(name: "EWAFCoreTests", dependencies: ["EWAFCore"], path: "tests/EWAFCoreTests", resources: [.copy("Fixtures")])
    ]
)
