// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/core/domain/node_mode.dart';
import 'package:locksys_app/core/link/proto_mapping.dart';
import 'package:locksys_app/features/door_lock/data/door_lock_proto_mapping.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';
import 'package:locksys_app/features/temperature/data/temperature_proto_mapping.dart';
import 'package:locksys_app/features/temperature/domain/temperature.dart';
import 'package:locksys_app/features/window/data/window_proto_mapping.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';
import 'package:locksys_protocol/locksys_protocol.dart' as pb;
import 'package:protobuf/protobuf.dart';

/// `WINDOW_STATE_MOVING_UP` with prefix `WindowState` -> `movingUp`.
String domainName(String protoName, String typeName) {
  final snake = typeName.replaceAllMapped(
    RegExp('(?<=[a-z0-9])([A-Z])'),
    (m) => '_${m[1]}',
  );
  final prefix = '${snake.toUpperCase()}_';
  expect(protoName, startsWith(prefix));
  final parts = protoName.substring(prefix.length).toLowerCase().split('_');
  return parts.first +
      parts.skip(1).map((p) => p[0].toUpperCase() + p.substring(1)).join();
}

void expectBijection<P extends ProtobufEnum, D extends Enum>(
  String typeName,
  List<P> protoValues,
  List<D> domainValues,
  D Function(P) map,
) {
  final mapped = <D>{};
  for (final v in protoValues) {
    final d = map(v);
    expect(d.name, domainName(v.name, typeName), reason: v.name);
    mapped.add(d);
  }
  expect(
    mapped,
    domainValues.toSet(),
    reason: '$typeName is not covered one to one',
  );
}

// @verifies SWR-APP-070
void main() {
  test('NodeMode', () {
    expectBijection(
      'NodeMode',
      pb.NodeMode.values,
      NodeMode.values,
      CoreProtoMapping.nodeMode,
    );
  });

  test('CommandResult', () {
    expectBijection(
      'CommandResult',
      pb.CommandResult.values,
      CommandResult.values,
      CoreProtoMapping.commandResult,
    );
  });

  test('DoorLockState', () {
    expectBijection(
      'DoorLockState',
      pb.DoorLockState.values,
      DoorLockState.values,
      DoorLockProtoMapping.state,
    );
  });

  test('WindowState and WindowStopReason', () {
    expectBijection(
      'WindowState',
      pb.WindowState.values,
      WindowState.values,
      WindowProtoMapping.state,
    );
    expectBijection(
      'WindowStopReason',
      pb.WindowStopReason.values,
      WindowStopReason.values,
      WindowProtoMapping.stopReason,
    );
  });

  test('TempStatus', () {
    expectBijection(
      'TempStatus',
      pb.TempStatus.values,
      TempStatus.values,
      TemperatureProtoMapping.status,
    );
  });

  test('outbound enumerations', () {
    expect(
      WindowProtoMapping.direction(WindowDirection.up),
      pb.WindowDirection.WINDOW_DIRECTION_UP,
    );
    expect(
      WindowProtoMapping.direction(WindowDirection.down),
      pb.WindowDirection.WINDOW_DIRECTION_DOWN,
    );
    expect(
      DoorLockProtoMapping.action(DoorAction.lock),
      pb.DoorAction.DOOR_ACTION_LOCK,
    );
    expect(
      DoorLockProtoMapping.action(DoorAction.unlock),
      pb.DoorAction.DOOR_ACTION_UNLOCK,
    );
  });

  test('StatusUpdate projections', () {
    final update = pb.StatusUpdate(
      windowState: pb.WindowState.WINDOW_STATE_MOVING_UP,
      windowPositionPct: WindowProtoMapping.positionUnknown,
      windowStopReason: pb.WindowStopReason.WINDOW_STOP_REASON_STALL,
      temperatureCdeg: -1234,
      tempStatus: pb.TempStatus.TEMP_STATUS_VALID,
    );
    expect(
      WindowProtoMapping.status(update),
      const WindowStatus(
        state: WindowState.movingUp,
        stopReason: WindowStopReason.stall,
      ),
    );
    expect(
      WindowProtoMapping.status(update..windowPositionPct = 42).positionPct,
      42,
    );
    expect(
      TemperatureProtoMapping.temperature(update).formattedCelsius,
      '-12.3',
    );
  });
}
