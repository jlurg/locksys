// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Outcome of joining the CGW SoftAP.
enum NetJoinOutcome {
  joined,
  userDenied,
  notFound,
  timeout,
  wpa3Unsupported,
  invalidCredentials,
  notForeground,
  failed,
}

/// Port to the platform Wi-Fi join and process binding (SWR-APP-010/011).
abstract interface class NetBinder {
  /// Joins [ssid] (and [bssid] when known) with [passphrase].
  Future<NetJoinOutcome> join({
    required String ssid,
    required String? bssid,
    required String passphrase,
    required bool wpa3Only,
    required Duration timeout,
  });

  /// Releases the network request and the process binding.
  Future<void> release();
}

/// Binder for environments without a SoftAP (mock and simulator).
final class NoopNetBinder implements NetBinder {
  const new();

  @override
  Future<NetJoinOutcome> join({
    required String ssid,
    required String? bssid,
    required String passphrase,
    required bool wpa3Only,
    required Duration timeout,
  }) async => NetJoinOutcome.joined;

  @override
  Future<void> release() async {}
}
