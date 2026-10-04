// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_repository.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';

/// Door actuator model for the mock environment.
final class FakeDoorLockRepository implements DoorLockRepository {
  new({
    DoorLockState initial = DoorLockState.locked,
    this.actuation = const Duration(milliseconds: 300),
  }) : _current = initial;

  /// Time from command to the final state.
  final Duration actuation;

  final StreamController<DoorLockState> _state =
      StreamController<DoorLockState>.broadcast();
  DoorLockState _current;

  /// Request ids received, for tests.
  final List<int> requestIds = <int>[];

  @override
  DoorLockState get current => _current;

  @override
  Stream<DoorLockState> get state => _state.stream;

  @override
  Future<CommandResult> send(
    DoorAction action, {
    required int requestId,
  }) async {
    requestIds.add(requestId);
    if (_current.isTransient) {
      return CommandResult.rejectedBusy;
    }
    final lock = action == DoorAction.lock;
    _set(lock ? DoorLockState.locking : DoorLockState.unlocking);
    await Future<void>.delayed(actuation);
    _set(lock ? DoorLockState.locked : DoorLockState.unlocked);
    return CommandResult.ok;
  }

  @override
  Future<void> refresh() async => _set(_current);

  Future<void> dispose() => _state.close();

  void _set(DoorLockState next) {
    _current = next;
    if (!_state.isClosed) {
      _state.add(next);
    }
  }
}
