// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:convert';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:locksys_protocol/src/ct_equal.dart';
import 'package:locksys_protocol/src/hkdf.dart';

/// Direction byte that prefixes the tag input (LS-SAIC-001 section 8.2).
enum LinkDirection {
  /// APP -> CGW, `0x41`.
  appToCgw(0x41),

  /// CGW -> APP, `0x43`.
  cgwToApp(0x43);

  new(this.code);

  /// Byte value placed before the counter.
  final int code;
}

/// Key derivation, proofs and frame tags of LS-SAIC-001 section 8.3.
// @satisfies SWR-APP-020
// @satisfies SWR-APP-021
abstract final class SessionCrypto {
  /// Length of K_pair and K_sess in bytes.
  static const int keyLength = 32;

  /// Length of `device_id` in bytes.
  static const int deviceIdLength = 8;

  /// Length of the nonces and of `client_id` in bytes.
  static const int nonceLength = 16;

  /// Length of a frame tag in bytes (truncated HMAC-SHA256).
  static const int tagLength = 16;

  /// Label of the client proof, ASCII `LSv1|cli`.
  static final Uint8List clientProofLabel = _ascii('LSv1|cli');

  /// Label of the server proof, ASCII `LSv1|srv`.
  static final Uint8List serverProofLabel = _ascii('LSv1|srv');

  /// HKDF info label of the session key, ASCII `LSv1|session`.
  static final Uint8List sessionLabel = _ascii('LSv1|session');

  /// HMAC-SHA256(K_pair, "LSv1|cli" ‖ device_id ‖ server_nonce ‖
  /// client_nonce ‖ client_id).
  static Uint8List clientProof({
    required List<int> kPair,
    required List<int> deviceId,
    required List<int> serverNonce,
    required List<int> clientNonce,
    required List<int> clientId,
  }) {
    _checkHandshake(kPair, deviceId, serverNonce, clientNonce, clientId);
    return _hmac(kPair, [
      ...clientProofLabel,
      ...deviceId,
      ...serverNonce,
      ...clientNonce,
      ...clientId,
    ]);
  }

  /// HMAC-SHA256(K_pair, "LSv1|srv" ‖ device_id ‖ client_nonce ‖
  /// server_nonce ‖ client_id).
  static Uint8List serverProof({
    required List<int> kPair,
    required List<int> deviceId,
    required List<int> serverNonce,
    required List<int> clientNonce,
    required List<int> clientId,
  }) {
    _checkHandshake(kPair, deviceId, serverNonce, clientNonce, clientId);
    return _hmac(kPair, [
      ...serverProofLabel,
      ...deviceId,
      ...clientNonce,
      ...serverNonce,
      ...clientId,
    ]);
  }

  /// Verifies a received server proof in constant time.
  static bool verifyServerProof({
    required List<int> kPair,
    required List<int> deviceId,
    required List<int> serverNonce,
    required List<int> clientNonce,
    required List<int> clientId,
    required List<int> proof,
  }) => constantTimeEquals(
    serverProof(
      kPair: kPair,
      deviceId: deviceId,
      serverNonce: serverNonce,
      clientNonce: clientNonce,
      clientId: clientId,
    ),
    proof,
  );

  /// K_sess = HKDF-SHA256(IKM = K_pair, salt = server_nonce ‖ client_nonce,
  /// info = "LSv1|session" ‖ device_id ‖ client_id, L = 32).
  static Uint8List deriveSessionKey({
    required List<int> kPair,
    required List<int> deviceId,
    required List<int> serverNonce,
    required List<int> clientNonce,
    required List<int> clientId,
  }) {
    _checkHandshake(kPair, deviceId, serverNonce, clientNonce, clientId);
    return hkdfSha256(
      ikm: kPair,
      salt: [...serverNonce, ...clientNonce],
      info: [...sessionLabel, ...deviceId, ...clientId],
      length: keyLength,
    );
  }

  /// First [tagLength] bytes of HMAC-SHA256(K_sess, dir ‖ counter_BE32 ‖
  /// body), computed over the raw body bytes.
  static Uint8List tag({
    required List<int> kSess,
    required LinkDirection direction,
    required int counter,
    required List<int> body,
  }) {
    _checkLength(kSess, keyLength, 'kSess');
    if (counter < 0 || counter > 0xFFFFFFFF) {
      throw RangeError.range(counter, 0, 0xFFFFFFFF, 'counter');
    }
    final header = ByteData(5)
      ..setUint8(0, direction.code)
      ..setUint32(1, counter);
    final mac = _hmac(kSess, [...header.buffer.asUint8List(), ...body]);
    return Uint8List.sublistView(mac, 0, tagLength);
  }

  /// Verifies a received tag in constant time.
  static bool verifyTag({
    required List<int> kSess,
    required LinkDirection direction,
    required int counter,
    required List<int> body,
    required List<int> tag,
  }) => constantTimeEquals(
    SessionCrypto.tag(
      kSess: kSess,
      direction: direction,
      counter: counter,
      body: body,
    ),
    tag,
  );

  static Uint8List _hmac(List<int> key, List<int> data) =>
      Uint8List.fromList(Hmac(sha256, key).convert(data).bytes);

  static Uint8List _ascii(String s) => Uint8List.fromList(ascii.encode(s));

  static void _checkHandshake(
    List<int> kPair,
    List<int> deviceId,
    List<int> serverNonce,
    List<int> clientNonce,
    List<int> clientId,
  ) {
    _checkLength(kPair, keyLength, 'kPair');
    _checkLength(deviceId, deviceIdLength, 'deviceId');
    _checkLength(serverNonce, nonceLength, 'serverNonce');
    _checkLength(clientNonce, nonceLength, 'clientNonce');
    _checkLength(clientId, nonceLength, 'clientId');
  }

  static void _checkLength(List<int> value, int expected, String name) {
    if (value.length != expected) {
      throw ArgumentError.value(value.length, name, 'expected $expected bytes');
    }
  }
}
