// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:typed_data';

import 'package:locksys_protocol/locksys_protocol.dart';
import 'package:test/test.dart';

import 'support/vectors.dart';

Map<String, Map<String, Object?>> framesByName(Map<String, Object?> v) => {
  for (final f in (v['frames']! as List).cast<Map<String, Object?>>())
    f['name']! as String: f,
};

Matcher violation(ViolationKind kind, int closeCode) => throwsA(
  isA<ProtocolViolation>()
      .having((e) => e.kind, 'kind', kind)
      .having((e) => e.closeCode, 'closeCode', closeCode),
);

// @verifies SWR-APP-021
// @verifies SWR-APP-022
void main() {
  final v = loadVectors('app_session_v1.json');
  final frames = framesByName(v);
  final kSess = unhex(v['k_sess']! as String);
  Uint8List frameBytes(String name) => unhex(frames[name]!['frame']! as String);

  group('FrameCodec', () {
    test('opens the ServerHello and a failed AuthResult', () {
      final hello = FrameCodec.openHandshake(frameBytes('server_hello'));
      expect(hello.whichMsg(), Body_Msg.serverHello);
      expect(hello.serverHello.fwVersion, '0.1.0-dev+a1b2c3d');
      final rejected = FrameCodec.openHandshake(
        frameBytes('auth_result_rejected'),
      );
      expect(
        rejected.authResult.result,
        CommandResult.COMMAND_RESULT_REJECTED_AUTH,
      );
    });

    test('a handshake frame with a counter or tag is rejected', () {
      expect(
        () => FrameCodec.openHandshake(frameBytes('auth_result_ok')),
        violation(ViolationKind.badHandshakeFrame, LsCloseCode.policyViolation),
      );
    });

    test('an APP -> CGW handshake member is rejected by the APP', () {
      expect(
        () => FrameCodec.openHandshake(frameBytes('client_auth')),
        violation(ViolationKind.wrongDirection, LsCloseCode.policyViolation),
      );
    });

    test('an unknown body member is returned as notSet', () {
      // Field 30, length-delimited, empty: not a member of Body in 1.0.
      final raw = Frame(body: [0xF2, 0x01, 0x00]).writeToBuffer();
      expect(FrameCodec.openHandshake(raw).whichMsg(), Body_Msg.notSet);
    });

    test('oversize messages close with 1002', () {
      expect(
        () => FrameCodec.decodeFrame(Uint8List(FrameCodec.maxFrameBytes + 1)),
        violation(ViolationKind.oversize, LsCloseCode.protocolError),
      );
    });

    test('malformed frames and bodies close with 1008', () {
      expect(
        () => FrameCodec.decodeFrame([0xFF]),
        violation(ViolationKind.malformedFrame, LsCloseCode.policyViolation),
      );
      expect(
        () => FrameCodec.decodeBody([0x0A, 0x05, 0x01]),
        violation(ViolationKind.malformedBody, LsCloseCode.policyViolation),
      );
    });

    test('refuses to encode an oversize frame or counter 0', () {
      final big = Body(
        serverHello: ServerHello(fwVersion: 'x' * FrameCodec.maxFrameBytes),
      );
      expect(() => FrameCodec.encodeHandshake(big), throwsArgumentError);
      expect(
        () => FrameCodec.encodeSession(
          kSess: kSess,
          direction: LinkDirection.appToCgw,
          counter: 0,
          body: Body(statusRequest: StatusRequest()),
        ),
        throwsRangeError,
      );
    });

    test('violation text names the rule and close code', () {
      expect(
        const ProtocolViolation(ViolationKind.badTag).toString(),
        'ProtocolViolation(badTag, close 1008)',
      );
    });
  });

  group('SessionChannel (APP side)', () {
    late SessionChannel channel;
    setUp(() => channel = SessionChannel(kSess: kSess));

    test('seals outbound frames with counters from 1', () {
      for (final name in [
        'window_move_first',
        'window_move_keepalive',
        'window_stop',
        'door_command_unlock',
        'status_request',
      ]) {
        final body = FrameCodec.decodeBody(
          unhex(frames[name]!['body']! as String),
        );
        expect(hex(channel.seal(body)), frames[name]!['frame'], reason: name);
      }
      expect(channel.txCounter, 5);
    });

    test('opens inbound frames with strictly increasing counters', () {
      for (final name in [
        'auth_result_ok',
        'ping_after_auth',
        'command_ack_accepted',
        'door_command_result_ok',
        'status_update_negative_values',
        'notice_ka_timeout',
        'session_close_timeout',
      ]) {
        channel.open(frameBytes(name));
      }
      expect(channel.rxCounter, 7);
    });

    test('a replayed or non-increasing counter is rejected', () {
      channel
        ..open(frameBytes('auth_result_ok'))
        ..open(frameBytes('ping_after_auth'));
      expect(
        () => channel.open(frameBytes('pong')),
        violation(ViolationKind.replayedCounter, LsCloseCode.policyViolation),
      );
      expect(channel.rxCounter, 2);
    });

    test('a modified or missing tag is rejected', () {
      final tampered = frameBytes('auth_result_ok');
      tampered[tampered.length - 1] ^= 0x01;
      expect(
        () => channel.open(tampered),
        violation(ViolationKind.badTag, LsCloseCode.policyViolation),
      );
      expect(
        () => channel.open(frameBytes('server_hello')),
        violation(ViolationKind.badTag, LsCloseCode.policyViolation),
      );
    });

    test('counter 0 inside a session is rejected', () {
      final body = Body(ping: Ping(timestampMs: 1)).writeToBuffer();
      final tag = SessionCrypto.tag(
        kSess: kSess,
        direction: LinkDirection.cgwToApp,
        counter: 0,
        body: body,
      );
      expect(
        () => channel.open(Frame(body: body, tag: tag).writeToBuffer()),
        violation(ViolationKind.zeroCounter, LsCloseCode.policyViolation),
      );
    });

    test('an APP -> CGW member received by the APP is rejected', () {
      final frame = FrameCodec.encodeSession(
        kSess: kSess,
        direction: LinkDirection.cgwToApp,
        counter: 1,
        body: Body(windowStop: WindowStop(pressId: 1)),
      );
      expect(
        () => channel.open(frame),
        violation(ViolationKind.wrongDirection, LsCloseCode.policyViolation),
      );
      expect(channel.rxCounter, 0);
    });

    test('a CGW-side channel accepts APP frames', () {
      final cgw = SessionChannel(kSess: kSess, local: LinkDirection.cgwToApp);
      final body = cgw.open(frameBytes('window_move_first'));
      expect(body.windowMove.pressId, 1);
    });

    test('close zeroises the key and blocks further use', () {
      channel.close();
      expect(channel.isClosed, isTrue);
      expect(
        () => channel.seal(Body(statusRequest: StatusRequest())),
        throwsStateError,
      );
      expect(
        () => channel.open(frameBytes('ping_after_auth')),
        throwsStateError,
      );
    });

    test('rejects a key of the wrong length', () {
      expect(
        () => SessionChannel(kSess: kSess.sublist(1)),
        throwsArgumentError,
      );
    });
  });
}
