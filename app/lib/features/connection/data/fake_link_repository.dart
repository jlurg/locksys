// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:locksys_app/core/domain/control_availability.dart';
import 'package:locksys_app/core/domain/link_state.dart';
import 'package:locksys_app/features/connection/domain/link_repository.dart';

/// Link model for the mock environment: connects after [connectDelay].
final class FakeLinkRepository implements LinkRepository {
  new({
    this.connectDelay = const Duration(milliseconds: 300),
    LinkState initial = LinkState.disconnected,
  }) : _current = initial;

  final Duration connectDelay;

  final StreamController<LinkState> _state =
      StreamController<LinkState>.broadcast();
  final StreamController<ControlAvailability> _availability =
      StreamController<ControlAvailability>.broadcast();
  LinkState _current;
  Timer? _timer;

  @override
  LinkState get current => _current;

  @override
  Stream<LinkState> get state => _state.stream;

  @override
  ControlAvailability get currentAvailability => _availabilityOf(_current);

  @override
  Stream<ControlAvailability> get availability => _availability.stream;

  @override
  Future<void> connect() async {
    _timer?.cancel();
    set(LinkState.connecting);
    _timer = Timer(connectDelay, () => set(LinkState.connected));
  }

  @override
  Future<void> disconnect() async {
    _timer?.cancel();
    set(LinkState.disconnected);
  }

  /// Forces [next], for tests and demonstrations.
  void set(LinkState next) {
    final before = currentAvailability;
    _current = next;
    if (_state.isClosed) {
      return;
    }
    _state.add(next);
    final after = currentAvailability;
    if (after != before) {
      _availability.add(after);
    }
  }

  Future<void> dispose() async {
    _timer?.cancel();
    await _state.close();
    await _availability.close();
  }

  static ControlAvailability _availabilityOf(LinkState s) => switch (s) {
    LinkState.connected => const ControlAvailability.available(),
    LinkState.degraded => const ControlAvailability.unavailable(
      UnavailableReason.dcuOffline,
    ),
    _ => const ControlAvailability.unavailable(UnavailableReason.notConnected),
  };
}
