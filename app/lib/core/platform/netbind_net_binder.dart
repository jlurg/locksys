// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/core/platform/net_binder.dart';
import 'package:locksys_netbind/locksys_netbind.dart';

/// [NetBinder] backed by the `locksys_netbind` plugin (real environment).
final class NetbindNetBinder implements NetBinder {
  new({Netbind? netbind}) : _netbind = netbind ?? Netbind();

  final Netbind _netbind;

  @override
  Future<NetJoinOutcome> join({
    required String ssid,
    required String? bssid,
    required String passphrase,
    required bool wpa3Only,
    required Duration timeout,
  }) async {
    final result = await _netbind.join(
      JoinRequest(
        ssid: ssid,
        bssid: bssid,
        passphrase: passphrase,
        security: wpa3Only ? JoinSecurity.wpa3 : JoinSecurity.wpa2wpa3,
        timeoutMs: timeout.inMilliseconds,
      ),
    );
    return mapJoinStatus(result.status);
  }

  @override
  Future<void> release() => _netbind.release();

  /// Maps the plugin status to the port outcome.
  static NetJoinOutcome mapJoinStatus(JoinStatus status) => switch (status) {
    JoinStatus.joined || JoinStatus.alreadyJoined => NetJoinOutcome.joined,
    JoinStatus.userDenied => NetJoinOutcome.userDenied,
    JoinStatus.notFound => NetJoinOutcome.notFound,
    JoinStatus.timeout => NetJoinOutcome.timeout,
    JoinStatus.wpa3Unsupported => NetJoinOutcome.wpa3Unsupported,
    JoinStatus.invalidCredentials => NetJoinOutcome.invalidCredentials,
    JoinStatus.notForeground => NetJoinOutcome.notForeground,
    JoinStatus.error => NetJoinOutcome.failed,
  };
}
