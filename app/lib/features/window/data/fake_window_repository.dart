// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:locksys_app/features/window/domain/window_command_port.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

/// Window plant model for the mock environment: moves while keep-alives
/// arrive, stops on `WindowStop` or when keep-alives cease.
final class FakeWindowRepository
    implements WindowCommandPort, WindowStatusRepository {
  new({
    int initialPositionPct = 50,
    this.step = const Duration(milliseconds: 100),
    this.keepAliveTimeout = const Duration(milliseconds: 350),
  }) : _current = WindowStatus(
         state: WindowState.stopped,
         positionPct: initialPositionPct,
       );

  /// Plant update period; the position changes by 1 % per step.
  final Duration step;

  /// Motion stops when no WindowMove arrives within this time.
  final Duration keepAliveTimeout;

  final StreamController<WindowStatus> _status =
      StreamController<WindowStatus>.broadcast();
  WindowStatus _current;
  WindowDirection? _moving;
  Timer? _plant;
  Timer? _watchdog;

  /// Commands received, for diagnostics and tests.
  final List<String> log = <String>[];

  @override
  WindowStatus get current => _current;

  @override
  Stream<WindowStatus> get status => _status.stream;

  @override
  void sendMove({
    required int pressId,
    required WindowDirection direction,
    required int holdMs,
  }) {
    log.add('move $pressId ${direction.name} $holdMs');
    _watchdog?.cancel();
    _watchdog = Timer(
      keepAliveTimeout,
      () => _halt(WindowStopReason.holdTimeout),
    );
    if (_moving == direction) {
      return;
    }
    final atEnd = direction == WindowDirection.up
        ? _current.state == WindowState.fullyClosed
        : _current.state == WindowState.fullyOpen;
    if (atEnd) {
      return;
    }
    _moving = direction;
    _set(
      WindowStatus(
        state: direction == WindowDirection.up
            ? WindowState.movingUp
            : WindowState.movingDown,
        positionPct: _current.positionPct,
      ),
    );
    _plant?.cancel();
    _plant = Timer.periodic(step, (_) => _advance());
  }

  @override
  void sendStop({required int pressId}) {
    log.add('stop $pressId');
    _halt(WindowStopReason.released);
  }

  /// Releases timers and closes the stream.
  Future<void> dispose() {
    _plant?.cancel();
    _watchdog?.cancel();
    return _status.close();
  }

  void _advance() {
    final position = _current.positionPct ?? 50;
    if (_moving == WindowDirection.up) {
      if (position <= 0) {
        _halt(WindowStopReason.upperLimit, end: WindowState.fullyClosed);
      } else {
        _set(
          WindowStatus(state: WindowState.movingUp, positionPct: position - 1),
        );
      }
    } else if (_moving == WindowDirection.down) {
      if (position >= 100) {
        _halt(WindowStopReason.lowerLimit, end: WindowState.fullyOpen);
      } else {
        _set(
          WindowStatus(
            state: WindowState.movingDown,
            positionPct: position + 1,
          ),
        );
      }
    }
  }

  void _halt(WindowStopReason reason, {WindowState end = WindowState.stopped}) {
    _plant?.cancel();
    _watchdog?.cancel();
    if (_moving == null) {
      return;
    }
    _moving = null;
    _set(
      WindowStatus(
        state: end,
        positionPct: _current.positionPct,
        stopReason: reason,
      ),
    );
  }

  void _set(WindowStatus next) {
    _current = next;
    if (!_status.isClosed) {
      _status.add(next);
    }
  }
}
