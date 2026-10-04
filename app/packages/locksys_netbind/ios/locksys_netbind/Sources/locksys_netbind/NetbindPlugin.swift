// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import Flutter
import UIKit

/// iOS implementation of `NetbindHostApi`.
///
/// M0 skeleton: capabilities are reported; join and probe return `.error` until the
/// NEHotspotConfiguration join and the NWConnection local-network probe land (M4).
public class NetbindPlugin: NSObject, FlutterPlugin, NetbindHostApi {
  private let events = NetEventsHandler()

  public static func register(with registrar: FlutterPluginRegistrar) {
    let instance = NetbindPlugin()
    NetbindHostApiSetup.setUp(binaryMessenger: registrar.messenger(), api: instance)
    NetEventsStreamHandler.register(with: registrar.messenger(), streamHandler: instance.events)
  }

  func capabilities() throws -> NetCapabilities {
    // Apple platforms support WPA3 Personal; programmatic SAE joins are gated at WP0 (TST-MAN-APP-003).
    return NetCapabilities(
      osVersion: UIDevice.current.systemVersion,
      wpa3Sae: true,
      staConcurrencyLocalOnly: false)
  }

  func join(request: JoinRequest) async throws -> JoinResult {
    return JoinResult(status: .error, detail: "join is not implemented in this build")
  }

  func probe(host: String, port: Int64, timeoutMs: Int64) async throws -> LocalNetResult {
    return LocalNetResult(status: .error, elapsedMs: nil)
  }

  func release() throws {
    // No hotspot configuration is held by this build.
  }

  func setSecureWindow(enabled: Bool) throws {
    // Screen-capture protection on iOS is handled by the privacy cover in Dart.
  }
}

/// Event stream of network changes; this build emits no events.
final class NetEventsHandler: NetEventsStreamHandler {}
