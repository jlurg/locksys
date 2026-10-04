// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/core/domain/node_mode.dart';
import 'package:locksys_protocol/locksys_protocol.dart' as pb;

/// Maps the shared protobuf enumerations to domain enumerations.
// @satisfies SWR-APP-070
abstract final class CoreProtoMapping {
  static NodeMode nodeMode(pb.NodeMode v) => switch (v) {
    pb.NodeMode.NODE_MODE_INIT => NodeMode.init,
    pb.NodeMode.NODE_MODE_NORMAL => NodeMode.normal,
    pb.NodeMode.NODE_MODE_DEGRADED => NodeMode.degraded,
    pb.NodeMode.NODE_MODE_SAFE => NodeMode.safe,
    pb.NodeMode.NODE_MODE_SERVICE => NodeMode.service,
    _ => NodeMode.unknown,
  };

  static CommandResult commandResult(pb.CommandResult v) => switch (v) {
    pb.CommandResult.COMMAND_RESULT_OK => CommandResult.ok,
    pb.CommandResult.COMMAND_RESULT_ACCEPTED => CommandResult.accepted,
    pb.CommandResult.COMMAND_RESULT_REJECTED_BUSY => CommandResult.rejectedBusy,
    pb.CommandResult.COMMAND_RESULT_REJECTED_MODE => CommandResult.rejectedMode,
    pb.CommandResult.COMMAND_RESULT_REJECTED_INTERLOCK =>
      CommandResult.rejectedInterlock,
    pb.CommandResult.COMMAND_RESULT_REJECTED_RATE_LIMIT =>
      CommandResult.rejectedRateLimit,
    pb.CommandResult.COMMAND_RESULT_REJECTED_INVALID =>
      CommandResult.rejectedInvalid,
    pb.CommandResult.COMMAND_RESULT_FAILED_ACTUATOR =>
      CommandResult.failedActuator,
    pb.CommandResult.COMMAND_RESULT_FAILED_TIMEOUT =>
      CommandResult.failedTimeout,
    pb.CommandResult.COMMAND_RESULT_FAILED_COMM => CommandResult.failedComm,
    pb.CommandResult.COMMAND_RESULT_REJECTED_AUTH => CommandResult.rejectedAuth,
    pb.CommandResult.COMMAND_RESULT_REJECTED_VERSION =>
      CommandResult.rejectedVersion,
    pb.CommandResult.COMMAND_RESULT_REJECTED_LINK_QUALITY =>
      CommandResult.rejectedLinkQuality,
    _ => CommandResult.unspecified,
  };
}
