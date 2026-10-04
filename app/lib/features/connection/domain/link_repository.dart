// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:locksys_app/core/domain/control_availability.dart';
import 'package:locksys_app/core/domain/link_state.dart';

/// Connection state and the derived control availability.
abstract interface class LinkRepository {
  LinkState get current;

  Stream<LinkState> get state;

  ControlAvailability get currentAvailability;

  Stream<ControlAvailability> get availability;

  /// Starts connecting (or reconnecting) to the paired gateway.
  Future<void> connect();

  /// Closes the session with code 1000 and stays disconnected.
  Future<void> disconnect();
}
