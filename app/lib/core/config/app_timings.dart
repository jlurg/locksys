// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:equatable/equatable.dart';

/// APP timing parameters, filled from the generated `LsParams` by the
/// composition root (LS-SAIC-001 section 5.0).
final class AppTimings extends Equatable {
  const new({
    required this.keepAlivePeriod,
    required this.pressGapMin,
    required this.unlockConfirm,
    required this.statusStale,
  });

  /// `t_app_ka_ms`.
  final Duration keepAlivePeriod;

  /// `t_app_press_gap_min_ms`.
  final Duration pressGapMin;

  /// `t_app_unlock_confirm_ms`.
  final Duration unlockConfirm;

  /// `t_app_status_stale_ms`.
  final Duration statusStale;

  @override
  List<Object?> get props => [
    keepAlivePeriod,
    pressGapMin,
    unlockConfirm,
    statusStale,
  ];
}
