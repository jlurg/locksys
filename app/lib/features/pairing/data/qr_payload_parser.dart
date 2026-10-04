// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:convert';
import 'dart:typed_data';

import 'package:locksys_app/core/logging/secret.dart';
import 'package:locksys_app/features/pairing/domain/pairing_record.dart';

/// Why a pairing payload was rejected.
enum PairingPayloadError {
  tooLong,
  notLockSysPairing,
  unsupportedVersion,
  duplicateParameter,
  missingParameter,
  invalidDeviceId,
  invalidSsid,
  invalidPassphrase,
  invalidKey,
  invalidBssid,
  invalidSecurity,
  ssidBssidMismatch,
  simHostNotAllowed,
}

final class PairingPayloadException implements Exception {
  const new(this.error);

  final PairingPayloadError error;

  @override
  String toString() => 'PairingPayloadException(${error.name})';
}

/// Parser of `locksys://pair?v=1&id=..&s=..&p=..&k=..&b=..&sec=..`
/// (LS-SAIC-001 section 8.5). Unknown parameters are ignored; duplicates are
/// rejected. The exception never carries secret values.
// @satisfies SWR-APP-002
final class QrPayloadParser {
  const new({this.allowSimHost = false});

  /// Accept the simulator-only `h` parameter.
  final bool allowSimHost;

  static const int maxLength = 256;

  static final RegExp _deviceId = RegExp(r'^[0-9A-F]{16}$');
  static final RegExp _ssid = RegExp(r'^LockSys-([0-9A-F]{4})$');
  static final RegExp _passphrase = RegExp(r'^[A-Z2-7]{20}$');
  static final RegExp _key = RegExp(r'^[A-Za-z0-9_-]{43}$');
  static final RegExp _bssid = RegExp(
    r'^[0-9A-F]{2}(:[0-9A-F]{2}){3}:([0-9A-F]{2}):([0-9A-F]{2})$',
  );

  PairingRecord parse(String payload) {
    if (payload.length > maxLength) {
      throw const PairingPayloadException(PairingPayloadError.tooLong);
    }
    final uri = Uri.tryParse(payload);
    if (uri == null || uri.scheme != 'locksys' || uri.host != 'pair') {
      throw const PairingPayloadException(
        PairingPayloadError.notLockSysPairing,
      );
    }
    final all = uri.queryParametersAll;
    if (all.values.any((v) => v.length > 1)) {
      throw const PairingPayloadException(
        PairingPayloadError.duplicateParameter,
      );
    }
    String need(String name) {
      final value = all[name]?.single;
      if (value == null) {
        throw const PairingPayloadException(
          PairingPayloadError.missingParameter,
        );
      }
      return value;
    }

    if (need('v') != '1') {
      throw const PairingPayloadException(
        PairingPayloadError.unsupportedVersion,
      );
    }
    final id = _match(
      _deviceId,
      need('id'),
      PairingPayloadError.invalidDeviceId,
    );
    final ssid = _ssid.firstMatch(need('s'));
    if (ssid == null) {
      throw const PairingPayloadException(PairingPayloadError.invalidSsid);
    }
    final passphrase = _match(
      _passphrase,
      need('p'),
      PairingPayloadError.invalidPassphrase,
    );
    final key = _match(_key, need('k'), PairingPayloadError.invalidKey);
    final bssid = _bssid.firstMatch(need('b'));
    if (bssid == null) {
      throw const PairingPayloadException(PairingPayloadError.invalidBssid);
    }
    if (ssid.group(1) != '${bssid.group(2)}${bssid.group(3)}') {
      throw const PairingPayloadException(
        PairingPayloadError.ssidBssidMismatch,
      );
    }
    final security = switch (need('sec')) {
      'wpa3' => SoftApSecurity.wpa3,
      'wpa2wpa3' => SoftApSecurity.wpa2wpa3,
      _ => throw const PairingPayloadException(
        PairingPayloadError.invalidSecurity,
      ),
    };
    final simHost = all['h']?.single;
    if (simHost != null && !allowSimHost) {
      throw const PairingPayloadException(
        PairingPayloadError.simHostNotAllowed,
      );
    }
    return PairingRecord(
      deviceId: _hex(id),
      ssid: ssid.group(0)!,
      passphrase: Secret(passphrase),
      kPair: Secret(Uint8List.fromList(base64Url.decode('$key='))),
      bssid: bssid.group(0)!,
      security: security,
      simHost: simHost,
    );
  }

  static String _match(RegExp re, String value, PairingPayloadError error) {
    if (!re.hasMatch(value)) {
      throw PairingPayloadException(error);
    }
    return value;
  }

  static Uint8List _hex(String s) => Uint8List.fromList([
    for (var i = 0; i < s.length; i += 2)
      int.parse(s.substring(i, i + 2), radix: 16),
  ]);
}
