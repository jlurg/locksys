// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:equatable/equatable.dart';

/// Window state reported by the DCU (LS-SAIC-001 `WindowState`).
enum WindowState {
  unknown,
  stopped,
  movingUp,
  movingDown,
  fullyClosed,
  fullyOpen,
  blocked,
  fault;

  bool get isMoving => this == movingUp || this == movingDown;
}

/// Why the window last stopped (LS-SAIC-001 `WindowStopReason`).
enum WindowStopReason {
  none,
  released,
  upperLimit,
  lowerLimit,
  holdTimeout,
  canTimeout,
  e2eError,
  stall,
  overcurrent,
  maxRuntime,
  obstacle,
  undervoltage,
  overvoltage,
  overtemp,
  driverFault,
  modeInhibit,
  dirMismatch;

  /// Whether the DCU, not the user, ended the motion.
  bool get isForced => this != none && this != released;
}

/// Commanded direction of a press; UP closes the window.
enum WindowDirection { up, down }

/// Latest window status from the gateway.
// @satisfies SWR-APP-035
final class WindowStatus extends Equatable {
  const new({
    required this.state,
    this.positionPct,
    this.stopReason = WindowStopReason.none,
  });

  static const WindowStatus unknown = WindowStatus(state: WindowState.unknown);

  final WindowState state;

  /// 0 (closed) to 100 (open); null when the position is unknown (255).
  final int? positionPct;

  final WindowStopReason stopReason;

  @override
  List<Object?> get props => [state, positionPct, stopReason];
}
