// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:fake_async/fake_async.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/features/window/data/fake_window_repository.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

void main() {
  test('moves while keep-alives arrive and stops on WindowStop', () {
    fakeAsync((async) {
      final repo = FakeWindowRepository(initialPositionPct: 10)
        ..sendMove(pressId: 1, direction: WindowDirection.up, holdMs: 0);
      expect(repo.current.state, WindowState.movingUp);
      for (var t = 100; t <= 300; t += 100) {
        async.elapse(const Duration(milliseconds: 100));
        repo.sendMove(pressId: 1, direction: WindowDirection.up, holdMs: t);
      }
      expect(repo.current.positionPct, 7);
      repo.sendStop(pressId: 1);
      expect(repo.current.state, WindowState.stopped);
      expect(repo.current.stopReason, WindowStopReason.released);
      expect(repo.log.first, 'move 1 up 0');
      async.flushTimers();
    });
  });

  test('stops with HOLD_TIMEOUT when keep-alives cease', () {
    fakeAsync((async) {
      final repo = FakeWindowRepository()
        ..sendMove(pressId: 1, direction: WindowDirection.down, holdMs: 0);
      async.elapse(const Duration(milliseconds: 400));
      expect(repo.current.state, WindowState.stopped);
      expect(repo.current.stopReason, WindowStopReason.holdTimeout);
    });
  });

  test('reaches the end positions and refuses to move further', () {
    fakeAsync((async) {
      final repo = FakeWindowRepository(
        initialPositionPct: 1,
        keepAliveTimeout: const Duration(seconds: 10),
      )..sendMove(pressId: 1, direction: WindowDirection.up, holdMs: 0);
      async.elapse(const Duration(milliseconds: 250));
      expect(repo.current.state, WindowState.fullyClosed);
      expect(repo.current.stopReason, WindowStopReason.upperLimit);
      repo.sendMove(pressId: 2, direction: WindowDirection.up, holdMs: 0);
      expect(repo.current.state, WindowState.fullyClosed);
      repo.sendMove(pressId: 3, direction: WindowDirection.down, holdMs: 0);
      async.elapse(const Duration(seconds: 11));
      expect(repo.current.stopReason, WindowStopReason.holdTimeout);
      final down = FakeWindowRepository(
        initialPositionPct: 99,
        keepAliveTimeout: const Duration(seconds: 10),
      )..sendMove(pressId: 1, direction: WindowDirection.down, holdMs: 0);
      async.elapse(const Duration(milliseconds: 250));
      expect(down.current.state, WindowState.fullyOpen);
      expect(down.current.stopReason, WindowStopReason.lowerLimit);
      async.flushTimers();
      repo.dispose().ignore();
      down.dispose().ignore();
    });
  });
}
