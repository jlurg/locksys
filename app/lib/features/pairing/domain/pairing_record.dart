// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:typed_data';

import 'package:equatable/equatable.dart';
import 'package:locksys_app/core/logging/secret.dart';

/// SoftAP security named in the pairing payload.
enum SoftApSecurity { wpa3, wpa2wpa3 }

/// Pairing data from the QR payload of LS-SAIC-001 section 8.5.
final class PairingRecord extends Equatable {
  const new({
    required this.deviceId,
    required this.ssid,
    required this.passphrase,
    required this.kPair,
    required this.bssid,
    required this.security,
    this.simHost,
  });

  /// 8-byte device identifier.
  final Uint8List deviceId;

  final String ssid;
  final Secret<String> passphrase;

  /// 32-byte pairing key.
  final Secret<Uint8List> kPair;

  /// AP BSSID, `AA:BB:CC:DD:EE:FF`.
  final String bssid;

  final SoftApSecurity security;

  /// `host:port` of the simulator (simulator builds only).
  final String? simHost;

  @override
  List<Object?> get props => [deviceId, ssid, bssid, security, simHost];
}

/// Persistent storage of the pairing record (OS secure storage in
/// production, SWR-APP-004).
abstract interface class PairingRepository {
  Future<PairingRecord?> load();

  Future<void> save(PairingRecord record);

  Future<void> erase();
}
