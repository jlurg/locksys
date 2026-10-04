// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:developer' as developer;

import 'package:locksys_app/core/logging/secret.dart';
import 'package:logging/logging.dart';

/// Configures the root logger with a redacting sink.
void setupLogging(String levelName) {
  Logger.root.level = Level.LEVELS.firstWhere(
    (l) => l.name == levelName,
    orElse: () => Level.INFO,
  );
  Logger.root.onRecord.listen((record) {
    developer.log(
      redact(record.message),
      name: record.loggerName,
      level: record.level.value,
      time: record.time,
    );
  });
}
