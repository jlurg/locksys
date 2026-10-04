// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';
import 'package:locksys_protocol/locksys_protocol.dart' as pb;

/// Maps door protobuf types to the domain.
// @satisfies SWR-APP-070
abstract final class DoorLockProtoMapping {
  static DoorLockState state(pb.DoorLockState v) => switch (v) {
    pb.DoorLockState.DOOR_LOCK_STATE_LOCKED => DoorLockState.locked,
    pb.DoorLockState.DOOR_LOCK_STATE_UNLOCKED => DoorLockState.unlocked,
    pb.DoorLockState.DOOR_LOCK_STATE_LOCKING => DoorLockState.locking,
    pb.DoorLockState.DOOR_LOCK_STATE_UNLOCKING => DoorLockState.unlocking,
    pb.DoorLockState.DOOR_LOCK_STATE_FAULT => DoorLockState.fault,
    _ => DoorLockState.unknown,
  };

  static pb.DoorAction action(DoorAction v) => switch (v) {
    DoorAction.lock => pb.DoorAction.DOOR_ACTION_LOCK,
    DoorAction.unlock => pb.DoorAction.DOOR_ACTION_UNLOCK,
  };
}
