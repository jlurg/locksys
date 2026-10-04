// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

/// Loads `interfaces/vectors/<name>` from the repository root, found by
/// walking up from the working directory.
Map<String, Object?> loadVectors(String name) {
  var dir = Directory.current.absolute;
  while (true) {
    final file = File('${dir.path}/interfaces/vectors/$name');
    if (file.existsSync()) {
      return jsonDecode(file.readAsStringSync()) as Map<String, Object?>;
    }
    final parent = dir.parent;
    if (parent.path == dir.path) {
      throw StateError('interfaces/vectors/$name not found');
    }
    dir = parent;
  }
}

/// Decodes a lower-case hex string.
Uint8List unhex(String hex) {
  final out = Uint8List(hex.length ~/ 2);
  for (var i = 0; i < out.length; i++) {
    out[i] = int.parse(hex.substring(2 * i, 2 * i + 2), radix: 16);
  }
  return out;
}

/// Encodes bytes as a lower-case hex string.
String hex(List<int> bytes) =>
    bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
