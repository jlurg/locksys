// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

plugins {
    id("com.android.application")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

android {
    namespace = "io.github.jlurg.locksys"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        applicationId = "io.github.jlurg.locksys"
        // Android 10 (API 29) is the minimum: WifiNetworkSpecifier and WPA3-SAE (LS-APP-SAD-001).
        minSdk = 29
        // Stay on API 36 until dart:io supports the API 37 local-network permission.
        targetSdk = 36
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    buildTypes {
        release {
            // Release signing is defined at M6; debug keys keep `flutter run --release` usable.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}
