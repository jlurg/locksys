// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:equatable/equatable.dart';

/// Validity of the cabin temperature (LS-SAIC-001 `TempStatus`).
enum TempStatus { unknown, valid, outOfRange, implausible, sensorFault, stale }

/// Cabin temperature from the DCU (TMP117).
// @satisfies SWR-APP-052
final class Temperature extends Equatable {
  const new({required this.status, this.centiDegrees});

  static const Temperature unknown = Temperature(status: TempStatus.unknown);

  final TempStatus status;

  /// Hundredths of a degree Celsius; meaningful only when [status] is valid.
  final int? centiDegrees;

  /// Value with 0.1 °C resolution, or null when not valid.
  String? get formattedCelsius {
    final c = centiDegrees;
    if (status != TempStatus.valid || c == null) {
      return null;
    }
    return (c / 100).toStringAsFixed(1);
  }

  @override
  List<Object?> get props => [status, centiDegrees];
}

/// Source of the cabin temperature.
abstract interface class TemperatureRepository {
  Temperature get current;

  Stream<Temperature> get temperature;
}
