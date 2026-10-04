// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/app/bootstrap.dart';
import 'package:locksys_app/core/config/app_config.dart';
import 'package:locksys_app/core/time/monotonic_clock.dart';

void main() {
  test('defaults without --dart-define select the mock environment', () {
    final config = AppConfig.fromEnvironment();
    expect(config.env, AppEnv.mock);
    expect(config.endpoint.toString(), 'ws://192.168.4.1:80/ws/v1');
  });

  test('parse rejects unknown environments', () {
    expect(AppEnv.parse('sim'), AppEnv.sim);
    expect(() => AppEnv.parse('prod'), throwsFormatException);
  });

  test('only the mock environment is wired in this build', () {
    const base = AppConfig(
      env: AppEnv.mock,
      cgwHost: 'h',
      cgwPort: 1,
      logLevel: 'INFO',
      gitSha: '',
    );
    expect(AppDependencies.forConfig(base).config, base);
    for (final env in [AppEnv.sim, AppEnv.real]) {
      final config = AppConfig(
        env: env,
        cgwHost: 'h',
        cgwPort: 1,
        logLevel: 'INFO',
        gitSha: '',
      );
      expect(() => AppDependencies.forConfig(config), throwsUnsupportedError);
    }
  });

  test('timings come from the generated parameters', () {
    expect(lsTimings.keepAlivePeriod, const Duration(milliseconds: 100));
    expect(lsTimings.pressGapMin, const Duration(milliseconds: 200));
    expect(lsTimings.unlockConfirm, const Duration(milliseconds: 800));
  });

  test('the stopwatch clock is monotonic', () {
    final clock = StopwatchClock();
    final a = clock.elapsed;
    expect(clock.elapsed >= a, isTrue);
  });
}
