// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/widgets.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';

/// App-scoped lifecycle hook: leaving the resumed state stops any window
/// press (T6). `inactive` never tears down the link.
// @satisfies SWR-APP-031
final class LifecycleObserver {
  new({required this._controller}) {
    _listener = AppLifecycleListener(
      onInactive: _stop,
      onHide: _stop,
      onPause: _stop,
      onDetach: _stop,
    );
  }

  final HoldToRunController _controller;
  late final AppLifecycleListener _listener;

  void _stop() => _controller.abort(StopTrigger.lifecycle);

  void dispose() => _listener.dispose();
}
