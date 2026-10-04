// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:equatable/equatable.dart';

/// Values shown on the diagnostics screen (SCR-07).
final class DiagnosticsSnapshot extends Equatable {
  const new({
    required this.appVersion,
    required this.gitSha,
    required this.protocolVersion,
    required this.environment,
    this.cgwFirmware,
    this.lastCloseCode,
  });

  final String appVersion;
  final String gitSha;
  final String protocolVersion;
  final String environment;
  final String? cgwFirmware;
  final int? lastCloseCode;

  @override
  List<Object?> get props => [
    appVersion,
    gitSha,
    protocolVersion,
    environment,
    cgwFirmware,
    lastCloseCode,
  ];
}

/// Source of diagnostics values.
abstract interface class DiagnosticsRepository {
  DiagnosticsSnapshot snapshot();
}
