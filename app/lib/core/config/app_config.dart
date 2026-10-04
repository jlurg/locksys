// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:equatable/equatable.dart';

/// Runtime environment selected with `--dart-define-from-file=config/<f>.json`.
enum AppEnv {
  /// In-process fake gateway; no network.
  mock,

  /// `tools/simulators/cgw_sim` over WebSocket.
  sim,

  /// The CGW SoftAP.
  real;

  /// Parses `LS_ENV`; throws [FormatException] for unknown values.
  static AppEnv parse(String value) => AppEnv.values.firstWhere(
    (e) => e.name == value,
    orElse: () => throw FormatException('unknown LS_ENV', value),
  );
}

/// Build-time configuration (LS-APP-SAD-001, environments).
final class AppConfig extends Equatable {
  const new({
    required this.env,
    required this.cgwHost,
    required this.cgwPort,
    required this.logLevel,
    required this.gitSha,
  });

  /// Reads the `--dart-define` values. Constant evaluation lets the compiler
  /// drop unused environments from release builds.
  factory fromEnvironment() => AppConfig(
    env: AppEnv.parse(
      const String.fromEnvironment('LS_ENV', defaultValue: 'mock'),
    ),
    cgwHost: const String.fromEnvironment(
      'LS_CGW_HOST',
      defaultValue: '192.168.4.1',
    ),
    cgwPort: int.parse(
      const String.fromEnvironment('LS_CGW_PORT', defaultValue: '80'),
    ),
    logLevel: const String.fromEnvironment(
      'LS_LOG_LEVEL',
      defaultValue: 'INFO',
    ),
    gitSha: const String.fromEnvironment('LS_GIT_SHA'),
  );

  final AppEnv env;
  final String cgwHost;
  final int cgwPort;
  final String logLevel;
  final String gitSha;

  /// WebSocket endpoint of the gateway (LS-SAIC-001 section 8.2).
  Uri get endpoint =>
      Uri(scheme: 'ws', host: cgwHost, port: cgwPort, path: '/ws/v1');

  @override
  List<Object?> get props => [env, cgwHost, cgwPort, logLevel, gitSha];
}
