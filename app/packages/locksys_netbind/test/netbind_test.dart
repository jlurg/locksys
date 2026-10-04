// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_netbind/locksys_netbind.dart';
import 'package:locksys_netbind/src/gen/netbind_api.g.dart' show NetbindHostApi;

const _prefix = 'dev.flutter.pigeon.locksys_netbind.NetbindHostApi';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
  const codec = NetbindHostApi.pigeonChannelCodec;

  void reply(String method, Object? Function(List<Object?> args) handler) {
    messenger.setMockMessageHandler('$_prefix.$method', (message) async {
      final args = (codec.decodeMessage(message) as List<Object?>?) ?? const [];
      return codec.encodeMessage(<Object?>[handler(args)]);
    });
  }

  tearDown(() {
    for (final m in [
      'capabilities',
      'join',
      'probe',
      'release',
      'setSecureWindow',
    ]) {
      messenger.setMockMessageHandler('$_prefix.$m', null);
    }
  });

  test('capabilities are decoded from the host reply', () async {
    reply(
      'capabilities',
      (_) => NetCapabilities(
        osVersion: '36',
        wpa3Sae: true,
        staConcurrencyLocalOnly: false,
      ),
    );
    final caps = await Netbind().capabilities();
    expect(caps.osVersion, '36');
    expect(caps.wpa3Sae, isTrue);
    expect(caps.staConcurrencyLocalOnly, isFalse);
  });

  test('join forwards the request and returns the status', () async {
    JoinRequest? seen;
    reply('join', (args) {
      seen = args.first as JoinRequest?;
      return JoinResult(status: JoinStatus.joined);
    });
    final result = await Netbind().join(
      JoinRequest(
        ssid: 'LockSys-3F2A',
        bssid: 'AA:BB:CC:DD:3F:2A',
        passphrase: 'ABCDEFGHIJKLMNOPQRST',
        security: JoinSecurity.wpa3,
        timeoutMs: 30000,
      ),
    );
    expect(result.status, JoinStatus.joined);
    expect(seen?.ssid, 'LockSys-3F2A');
    expect(seen?.security, JoinSecurity.wpa3);
  });

  test('probe passes the timeout in milliseconds', () async {
    List<Object?>? seen;
    reply('probe', (args) {
      seen = args;
      return LocalNetResult(status: LocalNetStatus.reachable, elapsedMs: 12);
    });
    final result = await Netbind().probe(
      '192.168.4.1',
      80,
      const Duration(seconds: 2),
    );
    expect(result.status, LocalNetStatus.reachable);
    expect(seen, <Object?>['192.168.4.1', 80, 2000]);
  });

  test('release and setSecureWindow reach the host', () async {
    final calls = <String>[];
    reply('release', (_) {
      calls.add('release');
      return null;
    });
    reply('setSecureWindow', (args) {
      calls.add('secure:${args.first}');
      return null;
    });
    final netbind = Netbind();
    await netbind.release();
    await netbind.setSecureWindow(enabled: true);
    expect(calls, ['release', 'secure:true']);
  });

  test('a missing host implementation surfaces as PlatformException', () {
    expect(Netbind().capabilities(), throwsA(isA<PlatformException>()));
  });
}
