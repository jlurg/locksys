// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/core/logging/log_setup.dart';
import 'package:locksys_app/core/logging/secret.dart';
import 'package:logging/logging.dart';

// @verifies SWR-APP-005
void main() {
  test('Secret.toString is redacted', () {
    expect(const Secret('passphrase').toString(), '<redacted>');
  });

  test('redact masks pairing URIs and secret parameters', () {
    expect(
      redact('scanned locksys://pair?v=1&k=SECRET&p=PASS done'),
      'scanned locksys://pair<redacted> done',
    );
    expect(
      redact('join?s=LockSys-3F2A&p=PASS&k=KEY'),
      'join?s=LockSys-3F2A&p=<redacted>&k=<redacted>',
    );
    expect(redact('nothing to hide'), 'nothing to hide');
  });

  test('setupLogging applies the level name and falls back to INFO', () {
    setupLogging('FINE');
    expect(Logger.root.level, Level.FINE);
    setupLogging('BOGUS');
    expect(Logger.root.level, Level.INFO);
    Logger('test').info('locksys://pair?k=1');
  });
}
