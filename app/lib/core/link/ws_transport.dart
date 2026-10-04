// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:typed_data';

/// WebSocket subprotocol of APP protocol 1.0, compared exactly.
const String lsSubprotocol = 'locksys.v1';

/// Binary message transport to the CGW (LS-SAIC-001 section 8.2).
abstract interface class WsTransport {
  /// Inbound binary messages; completes when the socket closes.
  Stream<Uint8List> get messages;

  /// Sends one binary message.
  void send(List<int> message);

  /// Closes the socket with [code].
  Future<void> close([int code]);

  /// Close code received or sent, once closed.
  int? get closeCode;
}

/// The server did not select `locksys.v1` (close 4001, update required).
final class SubprotocolMismatch implements Exception {
  const new(this.selected);

  final String? selected;

  @override
  String toString() => 'SubprotocolMismatch(selected: $selected)';
}
