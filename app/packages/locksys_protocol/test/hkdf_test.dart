// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:crypto/crypto.dart';
import 'package:locksys_protocol/locksys_protocol.dart';
import 'package:test/test.dart';

import 'support/vectors.dart';

void main() {
  final kat = loadVectors('crypto_kat.json');

  group('HKDF-SHA256 (RFC 5869 appendix A)', () {
    for (final v
        in (kat['hkdf_sha256']! as List).cast<Map<String, Object?>>()) {
      test(v['name'], () {
        final ikm = unhex(v['ikm']! as String);
        final salt = unhex(v['salt']! as String);
        final info = unhex(v['info']! as String);
        final length = v['length']! as int;
        expect(hex(hkdfExtract(salt: salt, ikm: ikm)), v['prk']);
        expect(
          hex(hkdfSha256(ikm: ikm, salt: salt, info: info, length: length)),
          v['okm'],
        );
      });
    }
  });

  group('HMAC-SHA256 (RFC 4231 section 4)', () {
    for (final v
        in (kat['hmac_sha256']! as List).cast<Map<String, Object?>>()) {
      test(v['name'], () {
        final mac = Hmac(
          sha256,
          unhex(v['key']! as String),
        ).convert(unhex(v['data']! as String)).bytes;
        expect(hex(mac.sublist(0, v['truncate_bytes']! as int)), v['mac']);
      });
    }
  });

  group('HKDF argument checks', () {
    final prk = List<int>.filled(32, 1);

    test('rejects a PRK shorter than HashLen', () {
      expect(
        () => hkdfExpand(prk: prk.sublist(1), info: const [], length: 32),
        throwsArgumentError,
      );
    });

    test('rejects lengths outside 1..8160', () {
      expect(
        () => hkdfExpand(prk: prk, info: const [], length: 0),
        throwsRangeError,
      );
      expect(
        () => hkdfExpand(prk: prk, info: const [], length: 255 * 32 + 1),
        throwsRangeError,
      );
    });

    test('accepts the maximum length', () {
      expect(
        hkdfExpand(prk: prk, info: const [], length: 255 * 32),
        hasLength(8160),
      );
    });
  });
}
