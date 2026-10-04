// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/services.dart';
import 'package:locksys_netbind/src/gen/netbind_api.g.dart';

/// Facade over the pigeon-generated host API and event channel.
class Netbind {
  /// Creates the facade; [binaryMessenger] is injectable for tests.
  new({BinaryMessenger? binaryMessenger})
    : _api = NetbindHostApi(binaryMessenger: binaryMessenger);

  final NetbindHostApi _api;

  /// Platform facts that select the join strategy.
  Future<NetCapabilities> capabilities() => _api.capabilities();

  /// Joins the SoftAP described by [request].
  Future<JoinResult> join(JoinRequest request) => _api.join(request);

  /// Opens a TCP connection to [host]:[port] to trigger and test local-network
  /// access within [timeout].
  Future<LocalNetResult> probe(String host, int port, Duration timeout) =>
      _api.probe(host, port, timeout.inMilliseconds);

  /// Releases the network request and the process binding.
  Future<void> release() => _api.release();

  /// Enables or disables screen-capture protection for pairing screens.
  Future<void> setSecureWindow({required bool enabled}) =>
      _api.setSecureWindow(enabled);

  /// Network availability changes of the joined SoftAP.
  Stream<NetEvent> events() => netEvents();
}
