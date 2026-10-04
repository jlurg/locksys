// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/core/domain/link_state.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';
import 'package:locksys_app/features/temperature/domain/temperature.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

import '../helpers/pump_app.dart';

final Finder _up = find.byKey(const Key('windowZoneUp'));
final Finder _down = find.byKey(const Key('windowZoneDown'));

Future<void> _teardown(WidgetTester tester) async {
  await tester.pumpWidget(const SizedBox());
  await tester.pump(const Duration(seconds: 1));
}

// @verifies SWR-APP-030
// @verifies SWR-APP-031
// @verifies SWR-APP-033
// @verifies SWR-APP-034
// @verifies SWR-APP-041
void main() {
  group('window hold-to-run', () {
    testWidgets('hold sends moves with keep-alives; release stops', (
      tester,
    ) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      final gesture = await tester.startGesture(tester.getCenter(_up));
      expect(g.window.log, ['move 1 up 0']);
      await tester.pump(const Duration(milliseconds: 250));
      expect(g.window.log, ['move 1 up 0', 'move 1 up 100', 'move 1 up 200']);
      expect(g.window.current.state, WindowState.movingUp);
      await gesture.up();
      expect(g.window.log.last, 'stop 1');
      await tester.pump();
      expect(find.byKey(const Key('windowState')), findsOneWidget);
      await _teardown(tester);
    });

    testWidgets('sliding off the zone stops and latches', (tester) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      final gesture = await tester.startGesture(tester.getCenter(_down));
      await tester.pump(const Duration(milliseconds: 120));
      await gesture.moveBy(const Offset(0, -400));
      expect(g.window.log.last, 'stop 1');
      expect(g.deps.holdToRun.state.phase, HoldPhase.latched);
      await gesture.moveBy(const Offset(0, 400));
      await tester.pump(const Duration(milliseconds: 300));
      expect(g.window.log.where((c) => c.startsWith('move')), hasLength(2));
      await gesture.up();
      expect(g.deps.holdToRun.state.phase, HoldPhase.idle);
      await _teardown(tester);
    });

    testWidgets('the app leaving the resumed state stops the press', (
      tester,
    ) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      final gesture = await tester.startGesture(tester.getCenter(_up));
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
      expect(g.window.log.last, 'stop 1');
      expect(g.deps.holdToRun.state.lastTrigger, StopTrigger.lifecycle);
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
      await gesture.up();
      await _teardown(tester);
    });

    testWidgets('link loss disables the zones and stops the press', (
      tester,
    ) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      final gesture = await tester.startGesture(tester.getCenter(_up));
      g.linkRepo.set(LinkState.reconnecting);
      await tester.pump();
      expect(g.window.log.last, 'stop 1');
      expect(g.deps.holdToRun.state.lastTrigger, StopTrigger.unavailable);
      await gesture.up();
      await tester.pump(const Duration(milliseconds: 300));
      await tester.startGesture(tester.getCenter(_down));
      expect(g.window.log.where((c) => c.startsWith('move')), hasLength(1));
      expect(find.text('Connection lost. Reconnecting…'), findsOneWidget);
      await _teardown(tester);
    });

    testWidgets('semantic taps never move the window', (tester) async {
      final handle = tester.ensureSemantics();
      final g = TestGraph();
      await tester.pumpLockSys(g);
      expect(
        tester.getSemantics(find.bySemanticsLabel('Close')),
        matchesSemantics(
          label: 'Close',
          hint: 'Press and hold to move the window. Release to stop.',
          isButton: true,
          hasEnabledState: true,
          isEnabled: true,
        ),
      );
      // The zone exposes no tap or long-press action, so assistive
      // technology cannot start motion.
      expect(g.window.log, isEmpty);
      handle.dispose();
      await _teardown(tester);
    });

    testWidgets('hold zones meet the tap target guidelines', (tester) async {
      final handle = tester.ensureSemantics();
      final g = TestGraph();
      await tester.pumpLockSys(g);
      await expectLater(tester, meetsGuideline(androidTapTargetGuideline));
      await expectLater(tester, meetsGuideline(iOSTapTargetGuideline));
      await expectLater(tester, meetsGuideline(labeledTapTargetGuideline));
      expect(tester.getSize(_up).height, greaterThanOrEqualTo(96));
      handle.dispose();
      await _teardown(tester);
    });

    testWidgets('disposing the page while held stops the press', (
      tester,
    ) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      await tester.startGesture(tester.getCenter(_up));
      await tester.pumpWidget(const SizedBox());
      expect(g.window.log.last, 'stop 1');
      expect(g.deps.holdToRun.state.lastTrigger, StopTrigger.disposed);
      await tester.pump(const Duration(seconds: 1));
    });
  });

  group('door lock', () {
    testWidgets('unlock requires the hold-to-confirm; lock is a tap', (
      tester,
    ) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      final action = find.byKey(const Key('doorLockActionButton'));
      expect(find.text('Hold to unlock'), findsOneWidget);
      await tester.tap(action);
      await tester.pump(const Duration(seconds: 1));
      expect(g.door.requestIds, isEmpty);

      final gesture = await tester.startGesture(tester.getCenter(action));
      await tester.pump(const Duration(milliseconds: 500));
      await gesture.up();
      await tester.pump(const Duration(seconds: 1));
      expect(g.door.requestIds, isEmpty);

      final hold = await tester.startGesture(tester.getCenter(action));
      await tester.pump(const Duration(milliseconds: 850));
      expect(g.door.requestIds, [1]);
      await tester.pump();
      expect(find.text('Waiting for the door…'), findsOneWidget);
      await hold.up();
      await tester.pump(const Duration(milliseconds: 400));
      expect(g.door.current, DoorLockState.unlocked);
      expect(find.text('Unlocked'), findsOneWidget);

      await tester.tap(find.byKey(const Key('doorLockActionButton')));
      await tester.pump(const Duration(milliseconds: 400));
      expect(g.door.requestIds, [1, 2]);
      expect(g.door.current, DoorLockState.locked);
      await tester.tap(find.byKey(const Key('doorLockStatusButton')));
      await _teardown(tester);
    });
  });

  group('status area', () {
    testWidgets('temperature with 0.1 °C resolution or the status name', (
      tester,
    ) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      expect(find.text('21.5 °C'), findsOneWidget);
      g.temperature.emit(const Temperature(status: TempStatus.sensorFault));
      await tester.pump();
      await tester.pump();
      expect(find.text('Unavailable (sensorFault)'), findsOneWidget);
      await _teardown(tester);
    });

    testWidgets('banner states and connect action', (tester) async {
      final g = TestGraph(link: LinkState.disconnected);
      await tester.pumpLockSys(g);
      expect(find.text('Paused'), findsOneWidget);
      await tester.tap(find.text('Connect'));
      await tester.pump();
      expect(find.text('Connecting…'), findsOneWidget);
      await tester.pump(const Duration(milliseconds: 300));
      expect(find.text('Connected'), findsOneWidget);
      for (final s in LinkState.values) {
        g.linkRepo.set(s);
        await tester.pump();
      }
      await _teardown(tester);
    });

    testWidgets('window position follows the reported status', (tester) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      expect(find.text('50 % open'), findsOneWidget);
      final gesture = await tester.startGesture(tester.getCenter(_down));
      await tester.pump(const Duration(milliseconds: 1000));
      expect(g.window.current.state, WindowState.movingDown);
      await gesture.up();
      await tester.pump();
      expect(find.text('60 % open'), findsOneWidget);
      await _teardown(tester);
    });

    testWidgets('diagnostics page shows build values', (tester) async {
      final g = TestGraph();
      await tester.pumpLockSys(g);
      await tester.tap(find.byTooltip('Diagnostics'));
      await tester.pumpAndSettle();
      expect(find.text('0.1.0 (abc1234)'), findsOneWidget);
      expect(find.text('mock'), findsOneWidget);
      await _teardown(tester);
    });
  });

  testWidgets('haptics follow the press outcome', (tester) async {
    final calls = <String>[];
    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform,
      (call) async {
        if (call.method == 'HapticFeedback.vibrate') {
          calls.add(call.arguments as String);
        }
        return null;
      },
    );
    final g = TestGraph();
    await tester.pumpLockSys(g);
    final first = await tester.startGesture(tester.getCenter(_up));
    await first.up();
    final early = await tester.startGesture(tester.getCenter(_up));
    await early.up();
    expect(calls, [
      'HapticFeedbackType.mediumImpact',
      'HapticFeedbackType.selectionClick',
      'HapticFeedbackType.heavyImpact',
    ]);
    await _teardown(tester);
  });
}
