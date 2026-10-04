// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Door lock state reported by the DCU (LS-SAIC-001 `DoorLockState`).
enum DoorLockState {
  unknown,
  locked,
  unlocked,
  locking,
  unlocking,
  fault;

  bool get isTransient => this == locking || this == unlocking;
}

/// Requested door action (LS-SAIC-001 `DoorAction`, NONE excluded).
enum DoorAction { lock, unlock }
