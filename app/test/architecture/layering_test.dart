// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

final _import = RegExp(
  r'''^\s*(?:import|export)\s+['"]([^'"]+)['"]''',
  multiLine: true,
);

const _app = 'package:locksys_app/';

/// Packages a domain file may import besides app domain code.
const _domainPackages = [
  'dart:async',
  'dart:core',
  'dart:math',
  'dart:typed_data',
  'package:equatable/',
  'package:meta/',
];

/// Returns the rule violations of one file (path relative to `lib/`).
List<String> violations(String path, Iterable<String> imports) {
  final out = <String>[];
  final feature = RegExp('^features/([^/]+)/(data|domain|presentation)/')
      .firstMatch(path);
  final isDomain =
      path.startsWith('core/domain/') || feature?.group(2) == 'domain';
  for (final uri in imports) {
    void fail(String rule) => out.add('$path -> $uri: $rule');
    if (uri.startsWith('package:locksys_protocol/') &&
        !(feature?.group(2) == 'data' ||
            path.startsWith('core/link/') ||
            path.startsWith('app/'))) {
      fail('protocol types only in features/*/data, core/link and app');
    }
    if (uri.startsWith('package:locksys_netbind/') &&
        !path.startsWith('core/platform/')) {
      fail('locksys_netbind only in core/platform');
    }
    if (isDomain) {
      final allowed =
          _domainPackages.any(uri.startsWith) ||
          uri.startsWith('${_app}core/domain/') ||
          uri.startsWith('${_app}core/time/') ||
          uri.startsWith('${_app}core/logging/secret.dart') ||
          (feature != null &&
              uri.startsWith('${_app}features/${feature.group(1)}/domain/'));
      if (!allowed) {
        fail(
          'domain depends only on dart:core/async, equatable, meta and domain code',
        );
      }
    }
    if (feature != null && uri.startsWith('${_app}features/')) {
      final target = RegExp('features/([^/]+)/([^/]+)/').firstMatch(uri);
      if (target != null && target.group(1) != feature.group(1)) {
        fail('a feature never imports another feature');
      }
      if (feature.group(2) == 'presentation' && target?.group(2) == 'data') {
        fail('presentation never imports data');
      }
    }
    if (path.startsWith('core/') && uri.startsWith('${_app}features/')) {
      fail('core never imports features');
    }
  }
  return out;
}

// @verifies SWR-APP-071
void main() {
  test('lib/ follows the dependency rules', () {
    final lib = Directory('lib');
    final found = <String>[];
    var files = 0;
    for (final entity in lib.listSync(recursive: true)) {
      if (entity is! File ||
          !entity.path.endsWith('.dart') ||
          entity.path.contains('/l10n/gen/')) {
        continue;
      }
      files++;
      final rel = entity.path.substring(lib.path.length + 1);
      final imports = _import
          .allMatches(entity.readAsStringSync())
          .map((m) => m[1]!);
      found.addAll(violations(rel, imports));
    }
    expect(files, greaterThan(30));
    expect(found, isEmpty, reason: found.join('\n'));
  });

  test('the rules detect violations', () {
    expect(
      violations('features/window/domain/x.dart', [
        'package:flutter/material.dart',
      ]),
      hasLength(1),
    );
    expect(
      violations('features/window/domain/x.dart', [
        'package:locksys_protocol/locksys_protocol.dart',
      ]),
      hasLength(2),
    );
    expect(
      violations('features/window/presentation/x.dart', [
        '${_app}features/door_lock/domain/a.dart',
      ]),
      hasLength(1),
    );
    expect(
      violations('features/window/presentation/x.dart', [
        '${_app}features/window/data/a.dart',
      ]),
      hasLength(1),
    );
    expect(
      violations('features/window/data/x.dart', [
        'package:locksys_netbind/locksys_netbind.dart',
      ]),
      hasLength(1),
    );
    expect(
      violations('core/link/x.dart', ['${_app}features/window/domain/a.dart']),
      hasLength(1),
    );
    expect(
      violations('features/window/data/x.dart', [
        'package:locksys_protocol/locksys_protocol.dart',
      ]),
      isEmpty,
    );
  });
}
