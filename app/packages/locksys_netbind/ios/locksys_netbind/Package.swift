// swift-tools-version: 5.9
// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import PackageDescription

let package = Package(
    name: "locksys_netbind",
    platforms: [
        .iOS("15.0")
    ],
    products: [
        .library(name: "locksys-netbind", targets: ["locksys_netbind"])
    ],
    dependencies: [
        .package(name: "FlutterFramework", path: "../FlutterFramework")
    ],
    targets: [
        .target(
            name: "locksys_netbind",
            dependencies: [
                .product(name: "FlutterFramework", package: "FlutterFramework")
            ],
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
