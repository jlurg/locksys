// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/core/domain/control_availability.dart';
import 'package:locksys_app/core/domain/link_state.dart';
import 'package:locksys_app/core/logging/secret.dart';
import 'package:locksys_app/features/diagnostics/domain/diagnostics_snapshot.dart';
import 'package:locksys_app/features/pairing/domain/pairing_record.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';

void main() {
  test('CommandResult.isSuccess', () {
    expect(CommandResult.values.where((r) => r.isSuccess), [
      CommandResult.ok,
      CommandResult.accepted,
    ]);
  });

  test('LinkState.hasSession', () {
    expect(LinkState.values.where((s) => s.hasSession), [
      LinkState.connected,
      LinkState.degraded,
    ]);
  });

  test('ControlAvailability', () {
    const off = ControlAvailability.unavailable(UnavailableReason.fault);
    expect(const ControlAvailability.available().isAvailable, isTrue);
    expect(off.isAvailable, isFalse);
    expect(off, const ControlAvailability.unavailable(UnavailableReason.fault));
  });

  test('PressOutcome equality', () {
    expect(const PressOutcome.accepted(), const PressOutcome.accepted());
    expect(
      const PressOutcome.rejected(PressRejection.pressGap),
      isNot(const PressOutcome.rejected(PressRejection.disabled)),
    );
    expect(const PressOutcome.ignored().isAccepted, isFalse);
  });

  test('DiagnosticsSnapshot equality', () {
    DiagnosticsSnapshot make(int? code) => DiagnosticsSnapshot(
      appVersion: '0.1.0',
      gitSha: 'abc',
      protocolVersion: '1.0',
      environment: 'mock',
      lastCloseCode: code,
    );
    expect(make(1000), make(1000));
    expect(make(1000), isNot(make(4004)));
  });

  test('PairingRecord equality ignores secrets', () {
    PairingRecord make(String passphrase) => PairingRecord(
      deviceId: Uint8List(8),
      ssid: 'LockSys-0000',
      passphrase: Secret(passphrase),
      kPair: Secret(Uint8List(32)),
      bssid: '00:00:00:00:00:00',
      security: SoftApSecurity.wpa3,
    );
    expect(make('A').props, make('B').props);
  });
}
