// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:math' as math;

import 'package:locksys_protocol/src/gen/ls_params.dart';

/// WebSocket close codes of LS-SAIC-001 section 8.4.
abstract final class LsCloseCode {
  /// Normal closure.
  static const int normal = 1000;

  /// The APP received a frame larger than `ws_frame_max_bytes`.
  static const int protocolError = 1002;

  /// Text or fragmented frame.
  static const int unsupportedData = 1003;

  /// Integrity violation or rate limit.
  static const int policyViolation = 1008;

  /// The CGW received a frame larger than `ws_frame_max_bytes`.
  static const int messageTooBig = 1009;

  /// Subprotocol or protocol major mismatch.
  static const int versionMismatch = 4001;

  /// Authentication failed.
  static const int authFailed = 4002;

  /// Another controller session is alive.
  static const int busy = 4003;

  /// Session timeout or pre-emption.
  static const int sessionTimeout = 4004;

  /// No K_pair, or the pairing was replaced.
  static const int pairingRequired = 4005;

  /// Handshake timeout or authentication throttled.
  static const int handshakeThrottled = 4006;
}

/// What the APP does after the link closed.
enum CloseReaction {
  /// Reconnect with backoff, not earlier than [ClosePolicy.minDelay].
  reconnect,

  /// Stop reconnecting: an APP update is required.
  blockedUpdateRequired,

  /// Stop reconnecting: the user must pair again.
  blockedRepairRequired,

  /// The pairing is no longer valid.
  unpaired,
}

/// Reaction to a close code (LS-SAIC-001 section 8.4 table).
final class ClosePolicy {
  /// Creates a policy.
  const new(this.reaction, {this.minDelay = Duration.zero});

  /// Maps a close [code] (null when the socket closed without one).
  factory forCode(int? code) => switch (code) {
    LsCloseCode.policyViolation => const ClosePolicy(
      CloseReaction.reconnect,
      minDelay: Duration(seconds: 2),
    ),
    LsCloseCode.versionMismatch => const ClosePolicy(
      CloseReaction.blockedUpdateRequired,
    ),
    LsCloseCode.authFailed => const ClosePolicy(
      CloseReaction.blockedRepairRequired,
    ),
    LsCloseCode.busy => const ClosePolicy(
      CloseReaction.reconnect,
      minDelay: Duration(milliseconds: LsParams.tAppBusyBackoffMs),
    ),
    LsCloseCode.pairingRequired => const ClosePolicy(CloseReaction.unpaired),
    LsCloseCode.handshakeThrottled => const ClosePolicy(
      CloseReaction.reconnect,
      minDelay: Duration(milliseconds: LsParams.tAuthThrottleMs),
    ),
    _ => const ClosePolicy(CloseReaction.reconnect),
  };

  /// Reaction of the connection state machine.
  final CloseReaction reaction;

  /// Lower bound of the reconnect delay.
  final Duration minDelay;
}

/// Reconnect delay d_n = min(max, min × 2^(n−1)) × [jitter] for attempt
/// [attempt] ≥ 1, with [jitter] drawn from U(0.8, 1.2) by the caller.
// @satisfies SWR-APP-024
Duration reconnectBackoff(int attempt, {required double jitter}) {
  if (attempt < 1) {
    throw RangeError.value(attempt, 'attempt', 'must be >= 1');
  }
  if (jitter < 0.8 || jitter > 1.2) {
    throw RangeError.value(jitter, 'jitter', 'must be within [0.8, 1.2]');
  }
  const minMs = LsParams.tAppReconnectMinMs;
  const maxMs = LsParams.tAppReconnectMaxMs;
  // Cap the exponent before shifting so that large attempt numbers cannot
  // overflow.
  final exponent = math.min(attempt - 1, 30);
  final base = math.min(maxMs, minMs * (1 << exponent));
  return Duration(milliseconds: (base * jitter).round());
}
