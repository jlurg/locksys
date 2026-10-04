// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/features/diagnostics/domain/diagnostics_snapshot.dart';

/// Diagnostics from build-time values only (no live session yet).
final class StaticDiagnosticsRepository implements DiagnosticsRepository {
  const new(this._snapshot);

  final DiagnosticsSnapshot _snapshot;

  @override
  DiagnosticsSnapshot snapshot() => _snapshot;
}
