// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/app/app.dart';
import 'package:locksys_app/app/bootstrap.dart';
import 'package:locksys_app/core/config/app_config.dart';
import 'package:locksys_app/core/domain/link_state.dart';
import 'package:locksys_app/features/connection/data/fake_link_repository.dart';
import 'package:locksys_app/features/diagnostics/data/static_diagnostics_repository.dart';
import 'package:locksys_app/features/diagnostics/domain/diagnostics_snapshot.dart';
import 'package:locksys_app/features/door_lock/data/fake_door_lock_repository.dart';
import 'package:locksys_app/features/temperature/data/fake_temperature_repository.dart';
import 'package:locksys_app/features/window/data/fake_window_repository.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';

import 'fakes.dart';

const testConfig = AppConfig(
  env: AppEnv.mock,
  cgwHost: '127.0.0.1',
  cgwPort: 8765,
  logLevel: 'INFO',
  gitSha: 'abc1234',
);

/// Mock-environment graph with handles on the fakes.
final class TestGraph {
  new({LinkState link = LinkState.connected}) {
    linkRepo = FakeLinkRepository(initial: link);
    deps = AppDependencies(
      config: testConfig,
      timings: lsTimings,
      link: linkRepo,
      doorLock: door,
      windowCommands: window,
      windowStatus: window,
      temperature: temperature,
      diagnostics: const StaticDiagnosticsRepository(
        DiagnosticsSnapshot(
          appVersion: appVersion,
          gitSha: 'abc1234',
          protocolVersion: '1.0',
          environment: 'mock',
        ),
      ),
      holdToRun: HoldToRunController(
        port: window,
        clock: TestClock(),
        keepAlivePeriod: lsTimings.keepAlivePeriod,
        pressGapMin: lsTimings.pressGapMin,
      ),
    );
  }

  late final FakeLinkRepository linkRepo;
  final FakeDoorLockRepository door = FakeDoorLockRepository();
  final FakeWindowRepository window = FakeWindowRepository();
  final FakeTemperatureRepository temperature = FakeTemperatureRepository();
  late final AppDependencies deps;
}

extension PumpApp on WidgetTester {
  /// Pumps the app on [graph] and settles the first frame.
  Future<void> pumpLockSys(TestGraph graph) async {
    await pumpWidget(LockSysApp(dependencies: graph.deps));
    await pump();
  }
}
