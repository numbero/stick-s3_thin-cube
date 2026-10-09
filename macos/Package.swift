// swift-tools-version: 5.9
import PackageDescription
let package = Package(
    name: "ThinCube", platforms: [.macOS(.v13)],
    products: [.executable(name: "ThinCube", targets: ["ThinCube"])],
    targets: [
        .target(name: "ThinCubeCore"),
        .executableTarget(name: "ThinCube", dependencies: ["ThinCubeCore"]),
        .testTarget(name: "ThinCubeCoreTests", dependencies: ["ThinCubeCore"])
    ]
)
