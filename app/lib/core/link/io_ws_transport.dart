// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:locksys_app/core/link/ws_transport.dart';
import 'package:locksys_protocol/locksys_protocol.dart';

/// dart:io WebSocket transport: no permessage-deflate, 256-byte payload cap,
/// exact subprotocol check (LS-APP-SAD-001 transport decision).
// @satisfies SWR-APP-012
final class IoWsTransport implements WsTransport {
  new _(this._socket) {
    _socket.listen(
      (data) {
        if (data is List<int>) {
          _messages.add(Uint8List.fromList(data));
        } else {
          // Text frames are not part of the protocol.
          unawaited(close(LsCloseCode.unsupportedData));
        }
      },
      onDone: () {
        _closeCode ??= _socket.closeCode;
        unawaited(_messages.close());
      },
      onError: (Object _) => unawaited(_messages.close()),
      cancelOnError: true,
    );
  }

  /// Connects to [endpoint] within [timeout].
  ///
  /// Throws [SubprotocolMismatch] when the server selects another
  /// subprotocol, and [TimeoutException] or [SocketException] when the
  /// connection fails.
  static Future<IoWsTransport> connect(
    Uri endpoint, {
    Duration timeout = const Duration(milliseconds: LsParams.tAppWsConnectToMs),
  }) async {
    final socket = await WebSocket.connect(
      endpoint.toString(),
      protocols: const [lsSubprotocol],
      compression: CompressionOptions.compressionOff,
      maxPayloadLength: LsParams.wsFrameMaxBytes,
    ).timeout(timeout);
    if (socket.protocol != lsSubprotocol) {
      await socket.close(LsCloseCode.versionMismatch);
      throw SubprotocolMismatch(socket.protocol);
    }
    return IoWsTransport._(socket);
  }

  final WebSocket _socket;
  final StreamController<Uint8List> _messages = StreamController<Uint8List>();
  int? _closeCode;

  @override
  Stream<Uint8List> get messages => _messages.stream;

  @override
  int? get closeCode => _closeCode;

  @override
  void send(List<int> message) => _socket.add(message);

  @override
  Future<void> close([int code = LsCloseCode.normal]) async {
    _closeCode ??= code;
    await _socket.close(code);
  }
}
