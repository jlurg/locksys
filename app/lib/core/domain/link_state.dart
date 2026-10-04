// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Connection state shown by the banner (LS-APP-SAD-001 connection model).
enum LinkState {
  unpaired,
  connecting,
  authenticating,
  connected,
  degraded,
  reconnecting,
  blocked,
  disconnected;

  /// Whether an authenticated session exists.
  bool get hasSession => this == connected || this == degraded;
}
