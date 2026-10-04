// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/features/temperature/domain/temperature.dart';
import 'package:locksys_protocol/locksys_protocol.dart' as pb;

/// Maps temperature protobuf types to the domain.
// @satisfies SWR-APP-070
abstract final class TemperatureProtoMapping {
  static TempStatus status(pb.TempStatus v) => switch (v) {
    pb.TempStatus.TEMP_STATUS_VALID => TempStatus.valid,
    pb.TempStatus.TEMP_STATUS_OUT_OF_RANGE => TempStatus.outOfRange,
    pb.TempStatus.TEMP_STATUS_IMPLAUSIBLE => TempStatus.implausible,
    pb.TempStatus.TEMP_STATUS_SENSOR_FAULT => TempStatus.sensorFault,
    pb.TempStatus.TEMP_STATUS_STALE => TempStatus.stale,
    _ => TempStatus.unknown,
  };

  static Temperature temperature(pb.StatusUpdate update) => Temperature(
    status: status(update.tempStatus),
    centiDegrees: update.temperatureCdeg,
  );
}
