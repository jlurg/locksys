// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';

/// Door lock commands and state.
abstract interface class DoorLockRepository {
  /// Latest state from a fresh `StatusUpdate` or `DoorCommandResult`.
  DoorLockState get current;

  Stream<DoorLockState> get state;

  /// Sends `DoorCommand(request_id, action)` and completes with the final
  /// `DoorCommandResult` (or a timeout result).
  Future<CommandResult> send(DoorAction action, {required int requestId});

  /// Sends `StatusRequest`.
  Future<void> refresh();
}
