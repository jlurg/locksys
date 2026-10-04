// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:locksys_app/features/temperature/domain/temperature.dart';

/// Constant cabin temperature for the mock environment.
final class FakeTemperatureRepository implements TemperatureRepository {
  new({
    this.current = const Temperature(
      status: TempStatus.valid,
      centiDegrees: 2150,
    ),
  });

  final StreamController<Temperature> _controller =
      StreamController<Temperature>.broadcast();

  @override
  Temperature current;

  @override
  Stream<Temperature> get temperature => _controller.stream;

  /// Publishes [value].
  void emit(Temperature value) {
    current = value;
    _controller.add(value);
  }

  Future<void> dispose() => _controller.close();
}
