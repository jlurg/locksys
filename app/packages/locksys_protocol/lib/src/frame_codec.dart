// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:typed_data';

import 'package:locksys_protocol/src/close_codes.dart';
import 'package:locksys_protocol/src/gen/locksys_app.pb.dart';
import 'package:locksys_protocol/src/gen/ls_params.dart';
import 'package:locksys_protocol/src/session_crypto.dart';
import 'package:protobuf/protobuf.dart' show InvalidProtocolBufferException;

/// Receive-path violations of LS-SAIC-001 section 8.2.
enum ViolationKind {
  /// Message larger than `ws_frame_max_bytes` (close 1002 on the APP).
  oversize,

  /// The payload is not a valid `Frame`.
  malformedFrame,

  /// `Frame.body` is not a valid `Body`.
  malformedBody,

  /// A handshake frame carries a tag or a non-zero counter.
  badHandshakeFrame,

  /// Missing or wrong tag.
  badTag,

  /// Counter 0 inside a session.
  zeroCounter,

  /// Counter not strictly increasing.
  replayedCounter,

  /// A known message type in the wrong direction.
  wrongDirection,
}

/// A receive-path violation; the session must close with [closeCode].
final class ProtocolViolation implements Exception {
  /// Creates a violation of [kind].
  const new(this.kind);

  /// The violated rule.
  final ViolationKind kind;

  /// WebSocket close code the APP uses for this violation.
  int get closeCode => kind == ViolationKind.oversize
      ? LsCloseCode.protocolError
      : LsCloseCode.policyViolation;

  @override
  String toString() => 'ProtocolViolation(${kind.name}, close $closeCode)';
}

/// A received frame before tag and body validation.
final class RawFrame {
  /// Creates a raw frame.
  const new({required this.counter, required this.body, required this.tag});

  /// Frame counter.
  final int counter;

  /// Raw `Body` bytes as received; tags are computed over these bytes.
  final Uint8List body;

  /// Tag bytes (empty for handshake frames).
  final Uint8List tag;
}

/// Encoding and decoding of `Frame` messages (LS-SAIC-001 section 8.2).
// @satisfies SWR-APP-021
abstract final class FrameCodec {
  /// Largest accepted WebSocket message in bytes.
  static const int maxFrameBytes = LsParams.wsFrameMaxBytes;

  /// Body members sent by the APP.
  static const Set<Body_Msg> appToCgwMembers = {
    Body_Msg.clientAuth,
    Body_Msg.ping,
    Body_Msg.pong,
    Body_Msg.doorCommand,
    Body_Msg.windowMove,
    Body_Msg.windowStop,
    Body_Msg.statusRequest,
  };

  /// Body members sent by the CGW.
  static const Set<Body_Msg> cgwToAppMembers = {
    Body_Msg.serverHello,
    Body_Msg.authResult,
    Body_Msg.ping,
    Body_Msg.pong,
    Body_Msg.commandAck,
    Body_Msg.doorCommandResult,
    Body_Msg.statusUpdate,
    Body_Msg.notice,
    Body_Msg.sessionClose,
  };

  /// Encodes a handshake frame (counter 0, empty tag).
  static Uint8List encodeHandshake(Body body) =>
      _checked(Frame(body: body.writeToBuffer()).writeToBuffer());

  /// Encodes a session frame tagged with [kSess] for [direction].
  static Uint8List encodeSession({
    required List<int> kSess,
    required LinkDirection direction,
    required int counter,
    required Body body,
  }) {
    if (counter < 1) {
      throw RangeError.value(counter, 'counter', 'session counters start at 1');
    }
    final raw = body.writeToBuffer();
    final tag = SessionCrypto.tag(
      kSess: kSess,
      direction: direction,
      counter: counter,
      body: raw,
    );
    return _checked(
      Frame(counter: counter, body: raw, tag: tag).writeToBuffer(),
    );
  }

  /// Parses the outer `Frame` of a received message.
  ///
  /// Throws [ProtocolViolation] for oversize or malformed input.
  static RawFrame decodeFrame(List<int> message) {
    if (message.length > maxFrameBytes) {
      throw const ProtocolViolation(ViolationKind.oversize);
    }
    final Frame frame;
    try {
      frame = Frame.fromBuffer(message);
    } on InvalidProtocolBufferException {
      throw const ProtocolViolation(ViolationKind.malformedFrame);
    }
    return RawFrame(
      counter: frame.counter,
      body: Uint8List.fromList(frame.body),
      tag: Uint8List.fromList(frame.tag),
    );
  }

  /// Parses raw `Body` bytes.
  ///
  /// An unknown member yields a body with `whichMsg() == Body_Msg.notSet`,
  /// which the caller ignores and counts.
  static Body decodeBody(List<int> raw) {
    try {
      return Body.fromBuffer(raw);
    } on InvalidProtocolBufferException {
      throw const ProtocolViolation(ViolationKind.malformedBody);
    }
  }

  /// Decodes a handshake frame received by the APP (ServerHello or a failed
  /// AuthResult): counter 0, empty tag, CGW -> APP member.
  static Body openHandshake(List<int> message) {
    final frame = decodeFrame(message);
    if (frame.counter != 0 || frame.tag.isNotEmpty) {
      throw const ProtocolViolation(ViolationKind.badHandshakeFrame);
    }
    final body = decodeBody(frame.body);
    final member = body.whichMsg();
    if (member != Body_Msg.notSet && !cgwToAppMembers.contains(member)) {
      throw const ProtocolViolation(ViolationKind.wrongDirection);
    }
    return body;
  }

  static Uint8List _checked(Uint8List encoded) {
    if (encoded.length > maxFrameBytes) {
      throw ArgumentError.value(
        encoded.length,
        'frame',
        'exceeds $maxFrameBytes bytes',
      );
    }
    return encoded;
  }
}

/// Counter and tag state of one authenticated session on the APP side.
///
/// Outbound counters start at 1; inbound counters must be non-zero and
/// strictly increasing. Receive order: frame, tag over the raw body, counter,
/// body, direction (LS-SAIC-001 section 8.2).
// @satisfies SWR-APP-021
// @satisfies SWR-APP-022
final class SessionChannel {
  /// Creates the channel for K_sess [kSess]; the key is copied.
  new({required List<int> kSess, this.local = LinkDirection.appToCgw})
    : _key = Uint8List.fromList(kSess) {
    if (_key.length != SessionCrypto.keyLength) {
      throw ArgumentError.value(kSess.length, 'kSess', 'expected 32 bytes');
    }
  }

  /// Direction of frames sent by this side.
  final LinkDirection local;

  final Uint8List _key;
  int _txCounter = 0;
  int _rxCounter = 0;
  bool _closed = false;

  /// Counter of the last frame sent.
  int get txCounter => _txCounter;

  /// Counter of the last frame accepted.
  int get rxCounter => _rxCounter;

  /// Whether [close] was called.
  bool get isClosed => _closed;

  LinkDirection get _remote => local == LinkDirection.appToCgw
      ? LinkDirection.cgwToApp
      : LinkDirection.appToCgw;

  Set<Body_Msg> get _remoteMembers => local == LinkDirection.appToCgw
      ? FrameCodec.cgwToAppMembers
      : FrameCodec.appToCgwMembers;

  /// Encodes [body] with the next outbound counter.
  Uint8List seal(Body body) {
    _checkOpen();
    final next = _txCounter + 1;
    final encoded = FrameCodec.encodeSession(
      kSess: _key,
      direction: local,
      counter: next,
      body: body,
    );
    _txCounter = next;
    return encoded;
  }

  /// Validates a received message and returns its body.
  ///
  /// Throws [ProtocolViolation]; the counter state is unchanged on failure.
  Body open(List<int> message) {
    _checkOpen();
    final frame = FrameCodec.decodeFrame(message);
    if (frame.tag.length != SessionCrypto.tagLength ||
        !SessionCrypto.verifyTag(
          kSess: _key,
          direction: _remote,
          counter: frame.counter,
          body: frame.body,
          tag: frame.tag,
        )) {
      throw const ProtocolViolation(ViolationKind.badTag);
    }
    if (frame.counter == 0) {
      throw const ProtocolViolation(ViolationKind.zeroCounter);
    }
    if (frame.counter <= _rxCounter) {
      throw const ProtocolViolation(ViolationKind.replayedCounter);
    }
    final body = FrameCodec.decodeBody(frame.body);
    final member = body.whichMsg();
    if (member != Body_Msg.notSet && !_remoteMembers.contains(member)) {
      throw const ProtocolViolation(ViolationKind.wrongDirection);
    }
    _rxCounter = frame.counter;
    return body;
  }

  /// Zeroises K_sess; further use throws [StateError].
  void close() {
    _key.fillRange(0, _key.length, 0);
    _closed = true;
  }

  void _checkOpen() {
    if (_closed) {
      throw StateError('session channel is closed');
    }
  }
}
