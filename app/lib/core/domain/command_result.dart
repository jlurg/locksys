// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Result of a door or window command (LS-SAIC-001 `CommandResult`).
enum CommandResult {
  unspecified,
  ok,
  accepted,
  rejectedBusy,
  rejectedMode,
  rejectedInterlock,
  rejectedRateLimit,
  rejectedInvalid,
  failedActuator,
  failedTimeout,
  failedComm,
  rejectedAuth,
  rejectedVersion,
  rejectedLinkQuality;

  /// Whether the command was carried out or is being carried out.
  bool get isSuccess => this == ok || this == accepted;
}
