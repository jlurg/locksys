// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:locksys_app/app/app.dart';
import 'package:locksys_app/app/bootstrap.dart';
import 'package:locksys_app/core/config/app_config.dart';

/// On-device smoke test of the mock environment:
/// `fvm flutter test integration_test --dart-define-from-file=config/mock.json`.
void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('the control page starts in the mock environment', (
    tester,
  ) async {
    final deps = AppDependencies.mock(AppConfig.fromEnvironment());
    await tester.pumpWidget(LockSysApp(dependencies: deps));
    await deps.link.connect();
    await tester.pumpAndSettle(const Duration(milliseconds: 500));
    expect(find.text('Connected'), findsOneWidget);
    expect(find.text('Close'), findsOneWidget);
  });
}
