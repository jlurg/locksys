// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_protocol/locksys_protocol.dart';
import 'package:protobuf/protobuf.dart';
import 'package:test/test.dart';

import 'support/vectors.dart';

/// Checks every field listed in the vector against the decoded message.
void expectFields(GeneratedMessage msg, Map<String, Object?> fields) {
  final byProtoName = {
    for (final f in msg.info_.fieldInfo.values) f.protoName: f,
  };
  for (final entry in fields.entries) {
    final info = byProtoName[entry.key];
    expect(info, isNotNull, reason: 'unknown field ${entry.key}');
    final Object? value = msg.getField(info!.tagNumber);
    final actual = switch (value) {
      final ProtobufEnum e => e.name,
      final ProtoVersion v => [v.major, v.minor],
      final List<int> bytes => hex(bytes),
      _ => value,
    };
    expect(actual, entry.value, reason: entry.key);
  }
}

LinkDirection direction(String name) =>
    name == 'app_to_cgw' ? LinkDirection.appToCgw : LinkDirection.cgwToApp;

// @verifies SWR-APP-020
// @verifies SWR-APP-021
// @verifies SWR-APP-070
void main() {
  final v = loadVectors('app_session_v1.json');
  final inputs = (v['inputs']! as Map).cast<String, String>();
  final kPair = unhex(inputs['k_pair']!);
  final deviceId = unhex(inputs['device_id']!);
  final serverNonce = unhex(inputs['server_nonce']!);
  final clientNonce = unhex(inputs['client_nonce']!);
  final clientId = unhex(inputs['client_id']!);
  final kSess = unhex(v['k_sess']! as String);

  group('handshake values', () {
    test('labels', () {
      final labels = (v['labels']! as Map).cast<String, String>();
      expect(hex(SessionCrypto.clientProofLabel), labels['client_proof']);
      expect(hex(SessionCrypto.serverProofLabel), labels['server_proof']);
      expect(hex(SessionCrypto.sessionLabel), labels['session']);
    });

    test('direction bytes and tag length', () {
      final dirs = (v['directions']! as Map).cast<String, int>();
      expect(LinkDirection.appToCgw.code, dirs['app_to_cgw']);
      expect(LinkDirection.cgwToApp.code, dirs['cgw_to_app']);
      expect(SessionCrypto.tagLength, v['tag_length']);
    });

    test('client proof', () {
      final proof = SessionCrypto.clientProof(
        kPair: kPair,
        deviceId: deviceId,
        serverNonce: serverNonce,
        clientNonce: clientNonce,
        clientId: clientId,
      );
      expect(hex(proof), v['client_proof']);
    });

    test('server proof verifies and a modified proof does not', () {
      final proof = unhex(v['server_proof']! as String);
      bool verify(List<int> p) => SessionCrypto.verifyServerProof(
        kPair: kPair,
        deviceId: deviceId,
        serverNonce: serverNonce,
        clientNonce: clientNonce,
        clientId: clientId,
        proof: p,
      );
      expect(verify(proof), isTrue);
      expect(verify(List.of(proof)..[31] ^= 1), isFalse);
      expect(verify(proof.sublist(0, 16)), isFalse);
    });

    test('session key (HKDF intermediate values and K_sess)', () {
      final h = (v['hkdf']! as Map).cast<String, Object?>();
      expect(hex(serverNonce + clientNonce), h['salt']);
      expect(
        hex([...SessionCrypto.sessionLabel, ...deviceId, ...clientId]),
        h['info'],
      );
      final derived = SessionCrypto.deriveSessionKey(
        kPair: kPair,
        deviceId: deviceId,
        serverNonce: serverNonce,
        clientNonce: clientNonce,
        clientId: clientId,
      );
      expect(hex(derived), v['k_sess']);
      expect(hex(derived), h['okm']);
    });

    test('rejects inputs of the wrong length', () {
      expect(
        () => SessionCrypto.clientProof(
          kPair: kPair.sublist(1),
          deviceId: deviceId,
          serverNonce: serverNonce,
          clientNonce: clientNonce,
          clientId: clientId,
        ),
        throwsArgumentError,
      );
      expect(
        () => SessionCrypto.deriveSessionKey(
          kPair: kPair,
          deviceId: deviceId,
          serverNonce: serverNonce,
          clientNonce: clientNonce,
          clientId: clientId.sublist(2),
        ),
        throwsArgumentError,
      );
      expect(
        () => SessionCrypto.tag(
          kSess: kSess,
          direction: LinkDirection.appToCgw,
          counter: -1,
          body: const [],
        ),
        throwsRangeError,
      );
    });
  });

  group('frames', () {
    for (final f in (v['frames']! as List).cast<Map<String, Object?>>()) {
      final name = f['name']! as String;
      final counter = f['counter']! as int;
      final rawBody = unhex(f['body']! as String);

      test('$name: body decodes to the listed fields and re-encodes', () {
        final body = FrameCodec.decodeBody(rawBody);
        final member = body.info_.fieldInfo.values.singleWhere(
          (i) => i.protoName == f['body_member'],
        );
        expect(body.hasField(member.tagNumber), isTrue);
        expectFields(
          body.getField(member.tagNumber) as GeneratedMessage,
          (f['fields']! as Map).cast<String, Object?>(),
        );
        expect(hex(body.writeToBuffer()), f['body']);
      });

      test('$name: frame encoding and tag', () {
        final body = FrameCodec.decodeBody(rawBody);
        if (counter == 0) {
          expect(f['tag'], isEmpty);
          expect(hex(FrameCodec.encodeHandshake(body)), f['frame']);
        } else {
          final dir = direction(f['direction']! as String);
          final tag = SessionCrypto.tag(
            kSess: kSess,
            direction: dir,
            counter: counter,
            body: rawBody,
          );
          expect(hex(tag), f['tag']);
          final frame = FrameCodec.encodeSession(
            kSess: kSess,
            direction: dir,
            counter: counter,
            body: body,
          );
          expect(hex(frame), f['frame']);
        }
        final decoded = FrameCodec.decodeFrame(unhex(f['frame']! as String));
        expect(decoded.counter, counter);
        expect(hex(decoded.body), f['body']);
        expect(hex(decoded.tag), f['tag']);
      });
    }
  });

  group('negative vectors', () {
    for (final n in (v['negative']! as List).cast<Map<String, Object?>>()) {
      test(n['name'], () {
        expect(n['expect'], 'reject');
        final ok = SessionCrypto.verifyTag(
          kSess: kSess,
          direction: LinkDirection.appToCgw,
          counter: n['counter']! as int,
          body: unhex(n['body']! as String),
          tag: unhex(n['tag']! as String),
        );
        expect(ok, isFalse);
      });
    }
  });
}
