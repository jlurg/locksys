// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:typed_data';

import 'package:crypto/crypto.dart';

/// Output length of SHA-256 in bytes.
const int sha256Length = 32;

/// HKDF-Extract (RFC 5869 section 2.2) with HMAC-SHA256.
///
/// An empty [salt] is replaced by [sha256Length] zero bytes.
Uint8List hkdfExtract({required List<int> salt, required List<int> ikm}) {
  final key = salt.isEmpty ? Uint8List(sha256Length) : salt;
  return Uint8List.fromList(Hmac(sha256, key).convert(ikm).bytes);
}

/// HKDF-Expand (RFC 5869 section 2.3) with HMAC-SHA256.
///
/// Throws [ArgumentError] when [prk] is shorter than [sha256Length] or
/// [length] is outside 1..255 * [sha256Length].
Uint8List hkdfExpand({
  required List<int> prk,
  required List<int> info,
  required int length,
}) {
  if (prk.length < sha256Length) {
    throw ArgumentError.value(prk.length, 'prk', 'shorter than HashLen');
  }
  if (length < 1 || length > 255 * sha256Length) {
    throw RangeError.range(length, 1, 255 * sha256Length, 'length');
  }
  final hmac = Hmac(sha256, prk);
  final okm = Uint8List(length);
  var previous = const <int>[];
  var offset = 0;
  for (var counter = 1; offset < length; counter++) {
    previous = hmac.convert([...previous, ...info, counter]).bytes;
    final take = (length - offset) < previous.length
        ? length - offset
        : previous.length;
    okm.setRange(offset, offset + take, previous);
    offset += take;
  }
  return okm;
}

/// HKDF-SHA256 (RFC 5869): extract followed by expand.
Uint8List hkdfSha256({
  required List<int> ikm,
  required List<int> salt,
  required List<int> info,
  required int length,
}) => hkdfExpand(
  prk: hkdfExtract(salt: salt, ikm: ikm),
  info: info,
  length: length,
);
