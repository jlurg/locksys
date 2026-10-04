// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/features/window/domain/window_status.dart';
import 'package:locksys_protocol/locksys_protocol.dart' as pb;

/// Maps window protobuf types to the domain.
// @satisfies SWR-APP-070
abstract final class WindowProtoMapping {
  /// Position value that means "unknown".
  static const int positionUnknown = 255;

  static WindowState state(pb.WindowState v) => switch (v) {
    pb.WindowState.WINDOW_STATE_STOPPED => WindowState.stopped,
    pb.WindowState.WINDOW_STATE_MOVING_UP => WindowState.movingUp,
    pb.WindowState.WINDOW_STATE_MOVING_DOWN => WindowState.movingDown,
    pb.WindowState.WINDOW_STATE_FULLY_CLOSED => WindowState.fullyClosed,
    pb.WindowState.WINDOW_STATE_FULLY_OPEN => WindowState.fullyOpen,
    pb.WindowState.WINDOW_STATE_BLOCKED => WindowState.blocked,
    pb.WindowState.WINDOW_STATE_FAULT => WindowState.fault,
    _ => WindowState.unknown,
  };

  static WindowStopReason stopReason(pb.WindowStopReason v) => switch (v) {
    pb.WindowStopReason.WINDOW_STOP_REASON_RELEASED =>
      WindowStopReason.released,
    pb.WindowStopReason.WINDOW_STOP_REASON_UPPER_LIMIT =>
      WindowStopReason.upperLimit,
    pb.WindowStopReason.WINDOW_STOP_REASON_LOWER_LIMIT =>
      WindowStopReason.lowerLimit,
    pb.WindowStopReason.WINDOW_STOP_REASON_HOLD_TIMEOUT =>
      WindowStopReason.holdTimeout,
    pb.WindowStopReason.WINDOW_STOP_REASON_CAN_TIMEOUT =>
      WindowStopReason.canTimeout,
    pb.WindowStopReason.WINDOW_STOP_REASON_E2E_ERROR =>
      WindowStopReason.e2eError,
    pb.WindowStopReason.WINDOW_STOP_REASON_STALL => WindowStopReason.stall,
    pb.WindowStopReason.WINDOW_STOP_REASON_OVERCURRENT =>
      WindowStopReason.overcurrent,
    pb.WindowStopReason.WINDOW_STOP_REASON_MAX_RUNTIME =>
      WindowStopReason.maxRuntime,
    pb.WindowStopReason.WINDOW_STOP_REASON_OBSTACLE =>
      WindowStopReason.obstacle,
    pb.WindowStopReason.WINDOW_STOP_REASON_UNDERVOLTAGE =>
      WindowStopReason.undervoltage,
    pb.WindowStopReason.WINDOW_STOP_REASON_OVERVOLTAGE =>
      WindowStopReason.overvoltage,
    pb.WindowStopReason.WINDOW_STOP_REASON_OVERTEMP =>
      WindowStopReason.overtemp,
    pb.WindowStopReason.WINDOW_STOP_REASON_DRIVER_FAULT =>
      WindowStopReason.driverFault,
    pb.WindowStopReason.WINDOW_STOP_REASON_MODE_INHIBIT =>
      WindowStopReason.modeInhibit,
    pb.WindowStopReason.WINDOW_STOP_REASON_DIR_MISMATCH =>
      WindowStopReason.dirMismatch,
    _ => WindowStopReason.none,
  };

  static pb.WindowDirection direction(WindowDirection v) => switch (v) {
    WindowDirection.up => pb.WindowDirection.WINDOW_DIRECTION_UP,
    WindowDirection.down => pb.WindowDirection.WINDOW_DIRECTION_DOWN,
  };

  /// Builds the domain status from a `StatusUpdate`.
  static WindowStatus status(pb.StatusUpdate update) => WindowStatus(
    state: state(update.windowState),
    positionPct: update.windowPositionPct <= 100
        ? update.windowPositionPct
        : null,
    stopReason: stopReason(update.windowStopReason),
  );
}
