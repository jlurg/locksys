// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/features/pairing/domain/pairing_record.dart';

/// Pairing store for the mock environment and tests (no persistence).
final class InMemoryPairingRepository implements PairingRepository {
  PairingRecord? _record;

  @override
  Future<PairingRecord?> load() async => _record;

  @override
  Future<void> save(PairingRecord record) async => _record = record;

  @override
  Future<void> erase() async => _record = null;
}
