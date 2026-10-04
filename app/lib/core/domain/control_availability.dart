// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:equatable/equatable.dart';

/// Why the controls are disabled (SWR-APP-050).
enum UnavailableReason {
  notConnected,
  statusStale,
  dcuOffline,
  modeInhibit,
  fault,
}

/// Whether door and window controls may be used.
// @satisfies SWR-APP-050
final class ControlAvailability extends Equatable {
  const new unavailable(UnavailableReason this.reason);

  const new available() : reason = null;

  /// Null when the controls are available.
  final UnavailableReason? reason;

  bool get isAvailable => reason == null;

  @override
  List<Object?> get props => [reason];
}
