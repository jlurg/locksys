// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';

/// Material 3 themes from one seed colour.
abstract final class AppTheme {
  static const Color _seed = Color(0xFF1B5E8C);

  static ThemeData light() =>
      ThemeData(colorScheme: ColorScheme.fromSeed(seedColor: _seed));

  static ThemeData dark() => ThemeData(
    colorScheme: ColorScheme.fromSeed(
      seedColor: _seed,
      brightness: Brightness.dark,
    ),
  );
}
