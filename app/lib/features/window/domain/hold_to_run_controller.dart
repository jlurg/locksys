// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:equatable/equatable.dart';
import 'package:locksys_app/core/time/monotonic_clock.dart';
import 'package:locksys_app/features/window/domain/window_command_port.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

/// Stop triggers T1-T12 of LS-APP-SAD-001 (T5 is ignored, not a stop).
enum StopTrigger {
  /// T1: the owning pointer was lifted.
  release,

  /// T2: the system took the pointer.
  pointerCancel,

  /// T3: the pointer left the zone captured at pointer-down.
  slideOff,

  /// T4: a pointer went down on the other zone.
  interlock,

  /// T6: the app left the resumed state.
  lifecycle,

  /// T7: link loss.
  linkLoss,

  /// T8: the CGW rejected or latched the press.
  cgwReject,

  /// T9: the controls became unavailable or the end position was reached.
  unavailable,

  /// T10: the control was disposed or its owner closed.
  disposed,

  /// T11: the session is ending.
  sessionEnd,

  /// T12: the DCU stopped the motion.
  dcuStop,
}

/// Phase of the hold-to-run state machine.
enum HoldPhase { idle, holding, latched }

/// Why a pointer-down did not start a press.
enum PressRejection { disabled, pressGap, latched, pointersDown, interlock }

/// Result of [HoldToRunController.pointerDown].
final class PressOutcome extends Equatable {
  const new accepted() : rejection = null, ignored = false;

  const new ignored() : rejection = null, ignored = true;

  const new rejected(PressRejection this.rejection) : ignored = false;

  final PressRejection? rejection;

  /// T5: an extra pointer on the zone that owns the press.
  final bool ignored;

  bool get isAccepted => rejection == null && !ignored;

  @override
  List<Object?> get props => [rejection, ignored];
}

/// Snapshot of the controller.
final class HoldToRunState extends Equatable {
  const new({
    this.phase = HoldPhase.idle,
    this.direction,
    this.pressId = 0,
    this.lastTrigger,
  });

  final HoldPhase phase;

  /// Direction of the active press; null when idle.
  final WindowDirection? direction;

  /// Last press_id used in this session (0 before the first press).
  final int pressId;

  /// Trigger that ended the last press.
  final StopTrigger? lastTrigger;

  @override
  List<Object?> get props => [phase, direction, pressId, lastTrigger];
}

/// Owner of every window press: press admission, keep-alive, stop triggers,
/// latch and interlock (LS-APP-SAD-001 hold-to-run, SAF).
///
/// One press exists system-wide. It lasts as long as its first pointer; any
/// stop ends it with `WindowStop`, and no new press starts until all window
/// pointers are up and the press gap has elapsed.
// @satisfies SWR-APP-030
// @satisfies SWR-APP-031
// @satisfies SWR-APP-032
// @satisfies SWR-APP-072
final class HoldToRunController {
  new({
    required this._port,
    required this._clock,
    required this._keepAlivePeriod,
    required this._pressGapMin,
  });

  final WindowCommandPort _port;
  final MonotonicClock _clock;
  final Duration _keepAlivePeriod;
  final Duration _pressGapMin;

  final Set<int> _pointers = <int>{};
  final StreamController<HoldToRunState> _states =
      StreamController<HoldToRunState>.broadcast(sync: true);

  HoldToRunState _state = const HoldToRunState();
  int? _owner;
  Duration _pressStart = Duration.zero;
  Duration? _lastPressEnd;
  Timer? _keepAlive;
  bool _sawMotion = false;
  bool _disposed = false;

  HoldToRunState get state => _state;

  /// Every state change, delivered synchronously.
  Stream<HoldToRunState> get states => _states.stream;

  /// Pointer down on the [direction] zone; [enabled] is the zone enablement
  /// at the time of the event.
  PressOutcome pointerDown(
    int pointer,
    WindowDirection direction, {
    required bool enabled,
  }) {
    final othersDown = _pointers.isNotEmpty;
    _pointers.add(pointer);
    switch (_state.phase) {
      case HoldPhase.holding:
        if (direction == _state.direction) {
          return const PressOutcome.ignored();
        }
        _stop(StopTrigger.interlock);
        return const PressOutcome.rejected(PressRejection.interlock);
      case HoldPhase.latched:
        return const PressOutcome.rejected(PressRejection.latched);
      case HoldPhase.idle:
        break;
    }
    if (othersDown) {
      return const PressOutcome.rejected(PressRejection.pointersDown);
    }
    if (!enabled || _disposed) {
      return const PressOutcome.rejected(PressRejection.disabled);
    }
    final now = _clock.elapsed;
    final lastEnd = _lastPressEnd;
    if (lastEnd != null && now - lastEnd < _pressGapMin) {
      return const PressOutcome.rejected(PressRejection.pressGap);
    }
    _startPress(pointer, direction, now);
    return const PressOutcome.accepted();
  }

  /// Pointer up (T1 for the owning pointer).
  void pointerUp(int pointer) => _release(pointer, StopTrigger.release);

  /// Pointer cancelled by the system (T2 for the owning pointer).
  void pointerCancel(int pointer) =>
      _release(pointer, StopTrigger.pointerCancel);

  /// The owning pointer left the zone bounds (T3); re-entry never resumes.
  void pointerLeft(int pointer) {
    if (_state.phase == HoldPhase.holding && pointer == _owner) {
      _stop(StopTrigger.slideOff);
    }
  }

  /// Ends the active press for [trigger] (T6-T12). No effect when idle.
  void abort(StopTrigger trigger) {
    if (_state.phase == HoldPhase.holding) {
      _stop(trigger);
    }
  }

  /// Applies a status update: end positions (T9) and DCU stops (T12).
  void onWindowStatus(WindowStatus status) {
    if (_state.phase != HoldPhase.holding) {
      return;
    }
    final direction = _state.direction;
    if ((direction == WindowDirection.up &&
            status.state == WindowState.fullyClosed) ||
        (direction == WindowDirection.down &&
            status.state == WindowState.fullyOpen) ||
        status.state == WindowState.fault) {
      _stop(StopTrigger.unavailable);
      return;
    }
    if (status.state.isMoving) {
      _sawMotion = true;
    } else if (_sawMotion && status.stopReason.isForced) {
      _stop(StopTrigger.dcuStop);
    }
  }

  /// Starts a new session scope: press ids restart at 1 (LS-SAIC-001 8.4).
  void resetSession() {
    abort(StopTrigger.sessionEnd);
    _emit(
      HoldToRunState(
        phase: _state.phase,
        direction: _state.direction,
        lastTrigger: _state.lastTrigger,
      ),
    );
  }

  /// Stops any press (T10) and releases resources.
  void dispose() {
    abort(StopTrigger.disposed);
    _disposed = true;
    unawaited(_states.close());
  }

  void _startPress(int pointer, WindowDirection direction, Duration now) {
    final pressId = _state.pressId >= 0xFFFFFFFF ? 1 : _state.pressId + 1;
    _owner = pointer;
    _pressStart = now;
    _sawMotion = false;
    _port.sendMove(pressId: pressId, direction: direction, holdMs: 0);
    _keepAlive = Timer.periodic(_keepAlivePeriod, (_) => _tick());
    _emit(
      HoldToRunState(
        phase: HoldPhase.holding,
        direction: direction,
        pressId: pressId,
      ),
    );
  }

  void _tick() {
    final direction = _state.direction;
    if (_state.phase != HoldPhase.holding || direction == null) {
      return;
    }
    _port.sendMove(
      pressId: _state.pressId,
      direction: direction,
      holdMs: (_clock.elapsed - _pressStart).inMilliseconds,
    );
  }

  void _release(int pointer, StopTrigger trigger) {
    _pointers.remove(pointer);
    if (_state.phase == HoldPhase.holding && pointer == _owner) {
      _stop(trigger);
    } else if (_state.phase == HoldPhase.latched && _pointers.isEmpty) {
      _emit(
        HoldToRunState(
          pressId: _state.pressId,
          lastTrigger: _state.lastTrigger,
        ),
      );
    }
  }

  void _stop(StopTrigger trigger) {
    _keepAlive?.cancel();
    _keepAlive = null;
    _owner = null;
    _port.sendStop(pressId: _state.pressId);
    _lastPressEnd = _clock.elapsed;
    _emit(
      HoldToRunState(
        phase: _pointers.isEmpty ? HoldPhase.idle : HoldPhase.latched,
        pressId: _state.pressId,
        lastTrigger: trigger,
      ),
    );
  }

  void _emit(HoldToRunState next) {
    _state = next;
    if (!_states.isClosed) {
      _states.add(next);
    }
  }
}
