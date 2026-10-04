// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_protocol/locksys_protocol.dart';
import 'package:test/test.dart';

// @verifies SWR-APP-020
void main() {
  test('equal sequences compare equal', () {
    expect(constantTimeEquals([1, 2, 3], [1, 2, 3]), isTrue);
    expect(constantTimeEquals(const [], const []), isTrue);
  });

  test('a difference in any position is detected', () {
    for (var i = 0; i < 3; i++) {
      final b = [1, 2, 3]..[i] ^= 0x80;
      expect(constantTimeEquals([1, 2, 3], b), isFalse);
    }
  });

  test('different lengths compare unequal', () {
    expect(constantTimeEquals([1, 2], [1, 2, 3]), isFalse);
  });
}
