// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_protocol/locksys_protocol.dart';
import 'package:test/test.dart';

// @verifies SWR-APP-023
// @verifies SWR-APP-024
void main() {
  group('ClosePolicy.forCode', () {
    test('reconnect codes', () {
      for (final code in [
        null,
        LsCloseCode.normal,
        LsCloseCode.protocolError,
        LsCloseCode.unsupportedData,
        LsCloseCode.messageTooBig,
        LsCloseCode.sessionTimeout,
        4999,
      ]) {
        final p = ClosePolicy.forCode(code);
        expect(p.reaction, CloseReaction.reconnect, reason: '$code');
        expect(p.minDelay, Duration.zero, reason: '$code');
      }
    });

    test('delayed reconnects', () {
      expect(
        ClosePolicy.forCode(LsCloseCode.policyViolation).minDelay,
        const Duration(seconds: 2),
      );
      expect(
        ClosePolicy.forCode(LsCloseCode.busy).minDelay,
        const Duration(milliseconds: LsParams.tAppBusyBackoffMs),
      );
      expect(
        ClosePolicy.forCode(LsCloseCode.handshakeThrottled).minDelay,
        const Duration(milliseconds: LsParams.tAuthThrottleMs),
      );
    });

    test('blocking codes', () {
      expect(
        ClosePolicy.forCode(LsCloseCode.versionMismatch).reaction,
        CloseReaction.blockedUpdateRequired,
      );
      expect(
        ClosePolicy.forCode(LsCloseCode.authFailed).reaction,
        CloseReaction.blockedRepairRequired,
      );
      expect(
        ClosePolicy.forCode(LsCloseCode.pairingRequired).reaction,
        CloseReaction.unpaired,
      );
    });
  });

  group('reconnectBackoff', () {
    test('doubles from the minimum up to the maximum', () {
      const min = LsParams.tAppReconnectMinMs;
      const max = LsParams.tAppReconnectMaxMs;
      expect(reconnectBackoff(1, jitter: 1).inMilliseconds, min);
      expect(reconnectBackoff(2, jitter: 1).inMilliseconds, 2 * min);
      expect(reconnectBackoff(3, jitter: 1).inMilliseconds, 4 * min);
      expect(reconnectBackoff(40, jitter: 1).inMilliseconds, max);
    });

    test('applies the jitter factor', () {
      const min = LsParams.tAppReconnectMinMs;
      expect(
        reconnectBackoff(1, jitter: 0.8).inMilliseconds,
        (min * 0.8).round(),
      );
      expect(
        reconnectBackoff(1, jitter: 1.2).inMilliseconds,
        (min * 1.2).round(),
      );
    });

    test('rejects invalid arguments', () {
      expect(() => reconnectBackoff(0, jitter: 1), throwsRangeError);
      expect(() => reconnectBackoff(1, jitter: 0.79), throwsRangeError);
      expect(() => reconnectBackoff(1, jitter: 1.21), throwsRangeError);
    });
  });
}
