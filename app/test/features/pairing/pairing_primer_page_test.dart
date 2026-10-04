// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/features/pairing/data/in_memory_pairing_repository.dart';
import 'package:locksys_app/features/pairing/data/qr_payload_parser.dart';
import 'package:locksys_app/features/pairing/presentation/pairing_primer_page.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

void main() {
  testWidgets('primer page renders the instructions', (tester) async {
    await tester.pumpWidget(
      const MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: PairingPrimerPage(),
      ),
    );
    expect(find.text('Pair with the vehicle'), findsOneWidget);
  });

  test('in-memory store saves, loads and erases', () async {
    final store = InMemoryPairingRepository();
    expect(await store.load(), isNull);
    final record = const QrPayloadParser().parse(
      'locksys://pair?v=1&id=0102030405060708&s=LockSys-3F2A'
      '&p=ABCDEFGHIJKLMNOPQRST&k=AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8'
      '&b=AA:BB:CC:DD:3F:2A&sec=wpa3',
    );
    await store.save(record);
    expect(await store.load(), record);
    await store.erase();
    expect(await store.load(), isNull);
  });
}
