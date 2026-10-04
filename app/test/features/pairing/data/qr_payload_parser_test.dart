// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/features/pairing/data/qr_payload_parser.dart';
import 'package:locksys_app/features/pairing/domain/pairing_record.dart';

// K_pair test vector: bytes 0x00..0x1F, base64url without padding.
final String _key = base64Url
    .encode(List<int>.generate(32, (i) => i))
    .replaceAll('=', '');
final String _valid =
    'locksys://pair?v=1&id=0102030405060708&s=LockSys-3F2A'
    '&p=ABCDEFGHIJKLMNOPQRST&k=$_key&b=AA:BB:CC:DD:3F:2A&sec=wpa3';

Matcher rejects(PairingPayloadError error) => throwsA(
  isA<PairingPayloadException>().having((e) => e.error, 'error', error),
);

// @verifies SWR-APP-002
void main() {
  const parser = QrPayloadParser();

  test('parses a valid payload', () {
    final r = parser.parse(_valid);
    expect(r.deviceId, [1, 2, 3, 4, 5, 6, 7, 8]);
    expect(r.ssid, 'LockSys-3F2A');
    expect(r.bssid, 'AA:BB:CC:DD:3F:2A');
    expect(r.security, SoftApSecurity.wpa3);
    expect(r.kPair.value, List<int>.generate(32, (i) => i));
    expect(r.passphrase.value, 'ABCDEFGHIJKLMNOPQRST');
    expect(r.simHost, isNull);
  });

  test('secrets never appear in toString', () {
    final r = parser.parse(_valid);
    expect('${r.passphrase} ${r.kPair}', '<redacted> <redacted>');
  });

  test('ignores unknown parameters and accepts wpa2wpa3', () {
    final r = parser.parse(_valid.replaceFirst('sec=wpa3', 'sec=wpa2wpa3&x=1'));
    expect(r.security, SoftApSecurity.wpa2wpa3);
  });

  final cases = <String, (String, PairingPayloadError)>{
    'too long': ('$_valid&pad=${'x' * 256}', PairingPayloadError.tooLong),
    'scheme': (
      _valid.replaceFirst('locksys://', 'https://'),
      PairingPayloadError.notLockSysPairing,
    ),
    'version': (
      _valid.replaceFirst('v=1', 'v=2'),
      PairingPayloadError.unsupportedVersion,
    ),
    'duplicate': ('$_valid&v=1', PairingPayloadError.duplicateParameter),
    'missing': (
      _valid.replaceFirst('&sec=wpa3', ''),
      PairingPayloadError.missingParameter,
    ),
    'device id': (
      _valid.replaceFirst('id=0102030405060708', 'id=01020304050607'),
      PairingPayloadError.invalidDeviceId,
    ),
    'ssid': (
      _valid.replaceFirst('s=LockSys-3F2A', 's=Other'),
      PairingPayloadError.invalidSsid,
    ),
    'passphrase': (
      _valid.replaceFirst('p=ABCDEFGHIJKLMNOPQRST', 'p=abc'),
      PairingPayloadError.invalidPassphrase,
    ),
    'key': (
      _valid.replaceFirst('k=$_key', 'k=short'),
      PairingPayloadError.invalidKey,
    ),
    'bssid': (
      _valid.replaceFirst('b=AA:BB:CC:DD:3F:2A', 'b=AABBCCDD3F2A'),
      PairingPayloadError.invalidBssid,
    ),
    'security': (
      _valid.replaceFirst('sec=wpa3', 'sec=open'),
      PairingPayloadError.invalidSecurity,
    ),
    'ssid/bssid': (
      _valid.replaceFirst('3F:2A', '3F:2B'),
      PairingPayloadError.ssidBssidMismatch,
    ),
    'sim host': (
      '$_valid&h=10.0.2.2:8765',
      PairingPayloadError.simHostNotAllowed,
    ),
  };
  for (final MapEntry(key: name, value: (payload, error)) in cases.entries) {
    test(
      'rejects: $name',
      () => expect(() => parser.parse(payload), rejects(error)),
    );
  }

  test('simulator builds accept the host parameter', () {
    final r = const QrPayloadParser(allowSimHost: true)
        .parse('$_valid&h=10.0.2.2:8765');
    expect(r.simHost, '10.0.2.2:8765');
  });

  test('exception text names the rule only', () {
    expect(
      const PairingPayloadException(PairingPayloadError.invalidKey).toString(),
      'PairingPayloadException(invalidKey)',
    );
  });
}
