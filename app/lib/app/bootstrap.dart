// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:flutter/widgets.dart';
import 'package:locksys_app/app/app.dart';
import 'package:locksys_app/core/config/app_config.dart';
import 'package:locksys_app/core/config/app_timings.dart';
import 'package:locksys_app/core/logging/log_setup.dart';
import 'package:locksys_app/core/time/monotonic_clock.dart';
import 'package:locksys_app/features/connection/data/fake_link_repository.dart';
import 'package:locksys_app/features/connection/domain/link_repository.dart';
import 'package:locksys_app/features/diagnostics/data/static_diagnostics_repository.dart';
import 'package:locksys_app/features/diagnostics/domain/diagnostics_snapshot.dart';
import 'package:locksys_app/features/door_lock/data/fake_door_lock_repository.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_repository.dart';
import 'package:locksys_app/features/temperature/data/fake_temperature_repository.dart';
import 'package:locksys_app/features/temperature/domain/temperature.dart';
import 'package:locksys_app/features/window/data/fake_window_repository.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';
import 'package:locksys_app/features/window/domain/window_command_port.dart';
import 'package:locksys_protocol/locksys_protocol.dart';

/// Version reported in ClientAuth and Diagnostics; kept equal to pubspec.
const String appVersion = '0.1.0';

/// Timing parameters from the generated parameter registry.
const AppTimings lsTimings = AppTimings(
  keepAlivePeriod: Duration(milliseconds: LsParams.tAppKaMs),
  pressGapMin: Duration(milliseconds: LsParams.tAppPressGapMinMs),
  unlockConfirm: Duration(milliseconds: LsParams.tAppUnlockConfirmMs),
  statusStale: Duration(milliseconds: LsParams.tAppStatusStaleMs),
);

/// Long-lived objects of the composition root (LS-APP-SAD-001).
final class AppDependencies {
  new({
    required this.config,
    required this.timings,
    required this.link,
    required this.doorLock,
    required this.windowCommands,
    required this.windowStatus,
    required this.temperature,
    required this.diagnostics,
    required this.holdToRun,
  });

  /// Wires the in-process fakes of the mock environment.
  factory mock(AppConfig config, {AppTimings timings = lsTimings}) {
    final window = FakeWindowRepository();
    return AppDependencies(
      config: config,
      timings: timings,
      link: FakeLinkRepository(),
      doorLock: FakeDoorLockRepository(),
      windowCommands: window,
      windowStatus: window,
      temperature: FakeTemperatureRepository(),
      diagnostics: StaticDiagnosticsRepository(
        DiagnosticsSnapshot(
          appVersion: appVersion,
          gitSha: config.gitSha,
          protocolVersion: '1.0',
          environment: config.env.name,
        ),
      ),
      holdToRun: HoldToRunController(
        port: window,
        clock: StopwatchClock(),
        keepAlivePeriod: timings.keepAlivePeriod,
        pressGapMin: timings.pressGapMin,
      ),
    );
  }

  /// Selects the adapters for the configured environment.
  ///
  /// Throws [UnsupportedError] for environments whose transport is not part
  /// of this build.
  factory forConfig(AppConfig config) => switch (config.env) {
    AppEnv.mock => AppDependencies.mock(config),
    AppEnv.sim || AppEnv.real => throw UnsupportedError(
      'LS_ENV=${config.env.name}: the CGW session transport is not part of '
      'this build',
    ),
  };

  final AppConfig config;
  final AppTimings timings;
  final LinkRepository link;
  final DoorLockRepository doorLock;
  final WindowCommandPort windowCommands;
  final WindowStatusRepository windowStatus;
  final TemperatureRepository temperature;
  final DiagnosticsRepository diagnostics;
  final HoldToRunController holdToRun;
}

/// Builds the object graph once per process and runs the app.
Future<void> bootstrap() async {
  WidgetsFlutterBinding.ensureInitialized();
  final config = AppConfig.fromEnvironment();
  setupLogging(config.logLevel);
  final deps = AppDependencies.forConfig(config);
  runApp(LockSysApp(dependencies: deps));
  unawaited(deps.link.connect());
}
