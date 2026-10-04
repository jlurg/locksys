// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/core/domain/control_availability.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';
import 'package:locksys_app/features/window/domain/window_command_port.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';

final class WindowHoldViewState extends Equatable {
  const new({
    required this.hold,
    required this.status,
    required this.availability,
  });

  final HoldToRunState hold;
  final WindowStatus status;
  final ControlAvailability availability;

  /// Zone enablement: a UX aid only; the DCU stays authoritative.
  bool isEnabled(WindowDirection direction) {
    if (!availability.isAvailable || status.state == WindowState.fault) {
      return false;
    }
    return switch (direction) {
      WindowDirection.up => status.state != WindowState.fullyClosed,
      WindowDirection.down => status.state != WindowState.fullyOpen,
    };
  }

  bool isPressed(WindowDirection direction) =>
      hold.phase == HoldPhase.holding && hold.direction == direction;

  WindowHoldViewState copyWith({
    HoldToRunState? hold,
    WindowStatus? status,
    ControlAvailability? availability,
  }) => WindowHoldViewState(
    hold: hold ?? this.hold,
    status: status ?? this.status,
    availability: availability ?? this.availability,
  );

  @override
  List<Object?> get props => [hold, status, availability];
}

/// Forwards pointer events to the [HoldToRunController] synchronously, so the
/// path from a pointer event to `WindowMove`/`WindowStop` has no queueing
/// (SWR-APP-072).
// @satisfies SWR-APP-033
class WindowHoldCubit extends Cubit<WindowHoldViewState> {
  new({
    required HoldToRunController controller,
    required WindowStatusRepository statusRepository,
    required Stream<ControlAvailability> availability,
    required ControlAvailability initialAvailability,
  }) : _controller = controller,
       super(
         WindowHoldViewState(
           hold: controller.state,
           status: statusRepository.current,
           availability: initialAvailability,
         ),
       ) {
    _subscriptions
      ..add(controller.states.listen((h) => emit(state.copyWith(hold: h))))
      ..add(
        statusRepository.status.listen((s) {
          _controller.onWindowStatus(s);
          emit(state.copyWith(status: s));
        }),
      )
      ..add(
        availability.listen((a) {
          if (!a.isAvailable) {
            _controller.abort(StopTrigger.unavailable);
          }
          emit(state.copyWith(availability: a));
        }),
      );
  }

  final HoldToRunController _controller;
  final List<StreamSubscription<Object?>> _subscriptions = [];

  PressOutcome pointerDown(int pointer, WindowDirection direction) =>
      _controller.pointerDown(
        pointer,
        direction,
        enabled: state.isEnabled(direction),
      );

  void pointerUp(int pointer) => _controller.pointerUp(pointer);

  void pointerCancel(int pointer) => _controller.pointerCancel(pointer);

  void pointerLeft(int pointer) => _controller.pointerLeft(pointer);

  /// A hold zone was removed from the tree (T10).
  void zoneDisposed() => _controller.abort(StopTrigger.disposed);

  @override
  Future<void> close() async {
    _controller.abort(StopTrigger.disposed);
    for (final s in _subscriptions) {
      await s.cancel();
    }
    await super.close();
  }
}
