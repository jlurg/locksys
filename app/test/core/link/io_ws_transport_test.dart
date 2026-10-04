// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/core/link/io_ws_transport.dart';
import 'package:locksys_app/core/link/ws_transport.dart';

/// Loopback WebSocket server; [select] chooses the subprotocol.
Future<(HttpServer, Uri)> serve(
  String? Function(List<String>)? select,
  void Function(WebSocket) onSocket,
) async {
  final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
  server.listen((request) async {
    final socket = await WebSocketTransformer.upgrade(
      request,
      protocolSelector: select,
    );
    onSocket(socket);
  });
  return (server, Uri.parse('ws://127.0.0.1:${server.port}/ws/v1'));
}

// @verifies SWR-APP-012
void main() {
  test('binary messages round-trip with subprotocol locksys.v1', () async {
    final (server, uri) = await serve(
      (_) => lsSubprotocol,
      (s) => s.listen(s.add),
    );
    addTearDown(server.close);
    final transport = await IoWsTransport.connect(uri);
    final first = transport.messages.first;
    transport.send([1, 2, 3]);
    expect(await first, [1, 2, 3]);
    await transport.close();
    expect(transport.closeCode, 1000);
  });

  test('a missing subprotocol is refused', () async {
    final (server, uri) = await serve(null, (s) => s.listen((_) {}));
    addTearDown(server.close);
    await expectLater(
      IoWsTransport.connect(uri),
      throwsA(
        isA<SubprotocolMismatch>().having(
          (e) => e.toString(),
          'text',
          contains('null'),
        ),
      ),
    );
  });

  test('a text frame closes the socket with 1003', () async {
    final (server, uri) = await serve((_) => lsSubprotocol, (s) {
      s
        ..add('text')
        ..listen((_) {});
    });
    addTearDown(server.close);
    final transport = await IoWsTransport.connect(uri);
    await expectLater(transport.messages, emitsDone);
    expect(transport.closeCode, 1003);
  });

  test('a server close is reported', () async {
    final (server, uri) = await serve(
      (_) => lsSubprotocol,
      (s) => unawaited(s.close(4004)),
    );
    addTearDown(server.close);
    final transport = await IoWsTransport.connect(uri);
    await expectLater(transport.messages, emitsDone);
    expect(transport.closeCode, 4004);
  });
}
