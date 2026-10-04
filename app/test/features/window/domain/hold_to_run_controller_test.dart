// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:fake_async/fake_async.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

import '../../../helpers/fakes.dart';

const Duration _ka = Duration(milliseconds: 100);
const Duration _gap = Duration(milliseconds: 200);
const WindowDirection _up = WindowDirection.up;
const WindowDirection _down = WindowDirection.down;

void run(
  void Function(FakeAsync async, HoldToRunController c, List<String> tx) body,
) {
  fakeAsync((async) {
    final port = RecordingWindowPort();
    final controller = HoldToRunController(
      port: port,
      clock: TestClock(),
      keepAlivePeriod: _ka,
      pressGapMin: _gap,
    );
    body(async, controller, port.calls);
    controller.dispose();
    async.flushTimers();
  });
}

// @verifies SWR-APP-030
// @verifies SWR-APP-031
// @verifies SWR-APP-032
// @verifies SWR-APP-072
void main() {
  group('press and keep-alive', () {
    test('first WindowMove is sent synchronously with hold_ms 0', () {
      run((async, c, tx) {
        final outcome = c.pointerDown(1, _up, enabled: true);
        expect(outcome.isAccepted, isTrue);
        expect(tx, ['move 1 up 0']);
        expect(c.state.phase, HoldPhase.holding);
        expect(c.state.direction, _up);
      });
    });

    test('keep-alives every period carry the monotonic hold time', () {
      run((async, c, tx) {
        c.pointerDown(1, _up, enabled: true);
        async.elapse(const Duration(milliseconds: 350));
        expect(tx, [
          'move 1 up 0',
          'move 1 up 100',
          'move 1 up 200',
          'move 1 up 300',
        ]);
      });
    });

    test('release sends WindowStop in the same turn and ends keep-alives', () {
      run((async, c, tx) {
        c.pointerDown(1, _down, enabled: true);
        async.elapse(const Duration(milliseconds: 150));
        c.pointerUp(1);
        expect(tx.last, 'stop 1');
        final count = tx.length;
        async.elapse(const Duration(seconds: 1));
        expect(tx, hasLength(count));
        expect(c.state.phase, HoldPhase.idle);
        expect(c.state.lastTrigger, StopTrigger.release);
      });
    });

    test('state changes are delivered synchronously', () {
      run((async, c, tx) {
        final seen = <HoldPhase>[];
        c.states.listen((s) => seen.add(s.phase));
        c
          ..pointerDown(1, _up, enabled: true)
          ..pointerUp(1);
        expect(seen, [HoldPhase.holding, HoldPhase.idle]);
      });
    });
  });

  group('admission', () {
    test('a disabled zone starts no press', () {
      run((async, c, tx) {
        final outcome = c.pointerDown(1, _up, enabled: false);
        expect(outcome.rejection, PressRejection.disabled);
        expect(tx, isEmpty);
      });
    });

    test('the press gap is enforced and press ids increase', () {
      run((async, c, tx) {
        c
          ..pointerDown(1, _up, enabled: true)
          ..pointerUp(1);
        async.elapse(const Duration(milliseconds: 150));
        expect(
          c.pointerDown(2, _up, enabled: true).rejection,
          PressRejection.pressGap,
        );
        c.pointerUp(2);
        async.elapse(const Duration(milliseconds: 50));
        expect(c.pointerDown(3, _up, enabled: true).isAccepted, isTrue);
        expect(tx.last, 'move 2 up 0');
      });
    });

    test('no press starts while another window pointer is down', () {
      run((async, c, tx) {
        c.pointerDown(1, _up, enabled: false);
        async.elapse(_gap);
        expect(
          c.pointerDown(2, _down, enabled: true).rejection,
          PressRejection.pointersDown,
        );
        expect(tx, isEmpty);
      });
    });
  });

  group('stop triggers', () {
    test('T2 pointer cancel stops the press', () {
      run((async, c, tx) {
        c
          ..pointerDown(1, _up, enabled: true)
          ..pointerCancel(1);
        expect(tx.last, 'stop 1');
        expect(c.state.lastTrigger, StopTrigger.pointerCancel);
        expect(c.state.phase, HoldPhase.idle);
      });
    });

    test(
      'T3 slide-off latches until the pointer is up; re-entry never resumes',
      () {
        run((async, c, tx) {
          c
            ..pointerDown(1, _up, enabled: true)
            ..pointerLeft(1);
          expect(tx.last, 'stop 1');
          expect(c.state.phase, HoldPhase.latched);
          async.elapse(const Duration(seconds: 1));
          expect(tx, ['move 1 up 0', 'stop 1']);
          expect(
            c.pointerDown(2, _up, enabled: true).rejection,
            PressRejection.latched,
          );
          c
            ..pointerUp(2)
            ..pointerUp(1);
          expect(c.state.phase, HoldPhase.idle);
        });
      },
    );

    test('T4 interlock stops and latches until all pointers are up', () {
      run((async, c, tx) {
        c.pointerDown(1, _up, enabled: true);
        final outcome = c.pointerDown(2, _down, enabled: true);
        expect(outcome.rejection, PressRejection.interlock);
        expect(tx.last, 'stop 1');
        expect(c.state.lastTrigger, StopTrigger.interlock);
        c.pointerUp(1);
        expect(c.state.phase, HoldPhase.latched);
        c.pointerUp(2);
        expect(c.state.phase, HoldPhase.idle);
      });
    });

    test('T5 an extra pointer on the owning zone is ignored but tracked', () {
      run((async, c, tx) {
        c.pointerDown(1, _up, enabled: true);
        expect(c.pointerDown(2, _up, enabled: true).ignored, isTrue);
        expect(tx, ['move 1 up 0']);
        c.pointerUp(2);
        expect(c.state.phase, HoldPhase.holding);
        c
          ..pointerDown(3, _up, enabled: true)
          ..pointerUp(1);
        expect(tx.last, 'stop 1');
        expect(c.state.phase, HoldPhase.latched);
        c.pointerUp(3);
        expect(c.state.phase, HoldPhase.idle);
      });
    });

    for (final trigger in [
      StopTrigger.lifecycle,
      StopTrigger.linkLoss,
      StopTrigger.cgwReject,
      StopTrigger.unavailable,
      StopTrigger.disposed,
    ]) {
      test('${trigger.name} aborts the press and latches', () {
        run((async, c, tx) {
          c
            ..pointerDown(1, _down, enabled: true)
            ..abort(trigger);
          expect(tx.last, 'stop 1');
          expect(c.state.lastTrigger, trigger);
          expect(c.state.phase, HoldPhase.latched);
        });
      });
    }

    test('abort while idle sends nothing', () {
      run((async, c, tx) {
        c.abort(StopTrigger.lifecycle);
        expect(tx, isEmpty);
      });
    });

    test('T9 end position in the press direction stops the press', () {
      run((async, c, tx) {
        c
          ..pointerDown(1, _up, enabled: true)
          ..onWindowStatus(const WindowStatus(state: WindowState.fullyOpen));
        expect(c.state.phase, HoldPhase.holding);
        c.onWindowStatus(const WindowStatus(state: WindowState.fullyClosed));
        expect(tx.last, 'stop 1');
        expect(c.state.lastTrigger, StopTrigger.unavailable);
      });
    });

    test('T9 a fault stops a DOWN press', () {
      run((async, c, tx) {
        c
          ..pointerDown(1, _down, enabled: true)
          ..onWindowStatus(const WindowStatus(state: WindowState.fault));
        expect(c.state.lastTrigger, StopTrigger.unavailable);
      });
    });

    test('T12 a DCU stop after motion stops the press', () {
      run((async, c, tx) {
        c
          ..pointerDown(1, _up, enabled: true)
          // A forced reason before any motion belongs to an earlier press.
          ..onWindowStatus(
            const WindowStatus(
              state: WindowState.stopped,
              stopReason: WindowStopReason.stall,
            ),
          );
        expect(c.state.phase, HoldPhase.holding);
        c
          ..onWindowStatus(const WindowStatus(state: WindowState.movingUp))
          ..onWindowStatus(
            const WindowStatus(
              state: WindowState.stopped,
              stopReason: WindowStopReason.released,
            ),
          );
        expect(c.state.phase, HoldPhase.holding);
        c.onWindowStatus(
          const WindowStatus(
            state: WindowState.stopped,
            stopReason: WindowStopReason.maxRuntime,
          ),
        );
        expect(tx.last, 'stop 1');
        expect(c.state.lastTrigger, StopTrigger.dcuStop);
      });
    });

    test('status updates while idle are ignored', () {
      run((async, c, tx) {
        c.onWindowStatus(const WindowStatus(state: WindowState.fullyClosed));
        expect(tx, isEmpty);
      });
    });
  });

  group('session scope and disposal', () {
    test('resetSession stops the press and restarts press ids at 1', () {
      run((async, c, tx) {
        c
          ..pointerDown(1, _up, enabled: true)
          ..resetSession();
        expect(tx.last, 'stop 1');
        expect(c.state.pressId, 0);
        c.pointerUp(1);
        async.elapse(_gap);
        c.pointerDown(2, _up, enabled: true);
        expect(tx.last, 'move 1 up 0');
      });
    });

    test('dispose stops the press and refuses new presses', () {
      fakeAsync((async) {
        final port = RecordingWindowPort();
        final c =
            HoldToRunController(
                port: port,
                clock: TestClock(),
                keepAlivePeriod: _ka,
                pressGapMin: _gap,
              )
              ..pointerDown(1, _up, enabled: true)
              ..dispose()
              ..pointerUp(1);
        expect(port.calls.last, 'stop 1');
        async.elapse(_gap);
        expect(
          c.pointerDown(2, _up, enabled: true).rejection,
          PressRejection.disabled,
        );
      });
    });
  });
}
