// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:clock/clock.dart';
import 'package:locksys_app/core/time/monotonic_clock.dart';
import 'package:locksys_app/features/window/domain/window_command_port.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

/// Records window commands as `move <id> <dir> <hold_ms>` / `stop <id>`.
final class RecordingWindowPort implements WindowCommandPort {
  final List<String> calls = <String>[];

  @override
  void sendMove({
    required int pressId,
    required WindowDirection direction,
    required int holdMs,
  }) => calls.add('move $pressId ${direction.name} $holdMs');

  @override
  void sendStop({required int pressId}) => calls.add('stop $pressId');
}

/// Monotonic clock on `package:clock`, which `fakeAsync` and `testWidgets`
/// control. Test use only: the production clock is [StopwatchClock].
final class TestClock implements MonotonicClock {
  new() : _origin = clock.now();

  final DateTime _origin;

  @override
  Duration get elapsed => clock.now().difference(_origin);
}
