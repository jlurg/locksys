// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/core/domain/control_availability.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_repository.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';

final class DoorLockViewState extends Equatable {
  const new({
    required this.lockState,
    required this.availability,
    this.pending = false,
    this.lastResult,
  });

  final DoorLockState lockState;
  final ControlAvailability availability;

  /// A door command is in flight (one at a time, SWR-APP-042).
  final bool pending;

  final CommandResult? lastResult;

  bool get canCommand => availability.isAvailable && !pending;

  DoorLockViewState copyWith({
    DoorLockState? lockState,
    ControlAvailability? availability,
    bool? pending,
    CommandResult? lastResult,
  }) => DoorLockViewState(
    lockState: lockState ?? this.lockState,
    availability: availability ?? this.availability,
    pending: pending ?? this.pending,
    lastResult: lastResult ?? this.lastResult,
  );

  @override
  List<Object?> get props => [lockState, availability, pending, lastResult];
}

/// Door lock status and the single in-flight door command.
// @satisfies SWR-APP-040
// @satisfies SWR-APP-042
class DoorLockCubit extends Cubit<DoorLockViewState> {
  new({
    required DoorLockRepository repository,
    required Stream<ControlAvailability> availability,
    required ControlAvailability initialAvailability,
  }) : _repository = repository,
       super(
         DoorLockViewState(
           lockState: repository.current,
           availability: initialAvailability,
         ),
       ) {
    _subscriptions
      ..add(repository.state.listen((s) => emit(state.copyWith(lockState: s))))
      ..add(availability.listen((a) => emit(state.copyWith(availability: a))));
  }

  final DoorLockRepository _repository;
  final List<StreamSubscription<Object?>> _subscriptions = [];
  int _requestId = 0;

  Future<void> lock() => _request(DoorAction.lock);

  Future<void> unlock() => _request(DoorAction.unlock);

  Future<void> refresh() => _repository.refresh();

  Future<void> _request(DoorAction action) async {
    if (!state.canCommand) {
      return;
    }
    _requestId++;
    emit(state.copyWith(pending: true));
    final result = await _repository.send(action, requestId: _requestId);
    if (!isClosed) {
      // The result carries the lock state; do not wait for the next status.
      emit(
        state.copyWith(
          lockState: _repository.current,
          pending: false,
          lastResult: result,
        ),
      );
    }
  }

  @override
  Future<void> close() async {
    for (final s in _subscriptions) {
      await s.cancel();
    }
    await super.close();
  }
}
