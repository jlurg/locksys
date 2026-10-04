// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/features/window/domain/window_status.dart';

/// Outbound window commands. Calls are synchronous: the frame is handed to
/// the socket before the call returns (SWR-APP-030/031).
abstract interface class WindowCommandPort {
  /// Sends `WindowMove(press_id, direction, hold_ms)`.
  void sendMove({
    required int pressId,
    required WindowDirection direction,
    required int holdMs,
  });

  /// Sends `WindowStop(press_id)`; dropped silently without a session.
  void sendStop({required int pressId});
}

/// Source of the window status.
abstract interface class WindowStatusRepository {
  WindowStatus get current;

  Stream<WindowStatus> get status;
}
