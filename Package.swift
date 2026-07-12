// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "9top",
    platforms: [.macOS(.v14)],
    products: [.executable(name: "9top", targets: ["NineTop"])],
    targets: [
        .target(
            name: "NativeCore",
            linkerSettings: [.linkedFramework("IOKit"), .linkedFramework("CoreFoundation")]
        ),
        .executableTarget(name: "NineTop", dependencies: ["NativeCore"]),
        .testTarget(name: "NineTopTests", dependencies: ["NineTop"]),
    ]
)
