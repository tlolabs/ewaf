// swift-tools-version: 6.0
import PackageDescription
import Foundation
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().path

var appSwiftSettings: [SwiftSetting] = []
var testSwiftSettings: [SwiftSetting] = []
var testLinkerSettings: [LinkerSetting] = []

let xcodeBase = ProcessInfo.processInfo.environment["DEVELOPER_DIR"] ?? (FileManager.default.fileExists(atPath: "/Applications/Xcode.app/Contents/Developer") ? "/Applications/Xcode.app/Contents/Developer" : nil)

if let xcode = xcodeBase {
    let pluginPath = "\(xcode)/Platforms/MacOSX.platform/Developer/usr/lib/swift/host/plugins/libSwiftUIMacros.dylib"
    if FileManager.default.fileExists(atPath: pluginPath) {
        appSwiftSettings.append(.unsafeFlags(["-load-plugin-library", pluginPath]))
        testSwiftSettings.append(.unsafeFlags(["-load-plugin-library", pluginPath]))
    }
    let xctestLib = "\(xcode)/Platforms/MacOSX.platform/Developer/usr/lib"
    let xctestFrameworks = "\(xcode)/Platforms/MacOSX.platform/Developer/Library/Frameworks"
    if FileManager.default.fileExists(atPath: xctestLib) && FileManager.default.fileExists(atPath: xctestFrameworks) {
        testSwiftSettings.append(.unsafeFlags([
            "-I", xctestLib,
            "-F", xctestFrameworks
        ]))
        testLinkerSettings.append(.unsafeFlags([
            "-F", xctestFrameworks,
            "-Xlinker", "-rpath", "-Xlinker", xctestFrameworks,
            "-L", xctestLib,
            "-Xlinker", "-rpath", "-Xlinker", xctestLib,
            "-Xlinker", "-framework", "-Xlinker", "XCTest"
        ]))
    }
}

let package = Package(
    name: "EWAF",
    platforms: [.macOS(.v14)],
    products: [.executable(name: "EWAF", targets: ["EWAF"]), .library(name: "EWAFCore", targets: ["EWAFCore"])],
    dependencies: [.package(url: "https://github.com/sparkle-project/Sparkle", exact: "2.9.6")],
    targets: [
        .target(name: "CEWAF", path: "bindings", exclude: ["module.modulemap"], publicHeadersPath: "include"),
        .target(name: "EWAFCore", dependencies: ["CEWAF"], path: "platform/macos/Sources/EWAFCore", linkerSettings: [.unsafeFlags(["-L", root + "/target/swift"]), .linkedLibrary("ewaf_ffi")]),
        .executableTarget(name: "EWAF", dependencies: ["EWAFCore", .product(name: "Sparkle", package: "Sparkle")], path: "platform/macos/Sources/EWAF", swiftSettings: appSwiftSettings, linkerSettings: [.unsafeFlags(["-Xlinker", "-rpath", "-Xlinker", "@executable_path/../Frameworks"])]),
        .testTarget(name: "EWAFAppTests", dependencies: ["EWAF", "EWAFCore"], path: "tests/macos/EWAFAppTests", swiftSettings: testSwiftSettings, linkerSettings: testLinkerSettings),
        .testTarget(name: "EWAFCoreTests", dependencies: ["EWAFCore"], path: "tests/macos/EWAFCoreTests", swiftSettings: testSwiftSettings, linkerSettings: testLinkerSettings)
    ]
)

