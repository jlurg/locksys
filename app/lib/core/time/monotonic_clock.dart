// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Monotonic time source for ages and timeouts; never the wall clock.
abstract interface class MonotonicClock {
  /// Time since an arbitrary fixed origin.
  Duration get elapsed;
}

/// Production clock backed by [Stopwatch].
final class StopwatchClock implements MonotonicClock {
  new() {
    _watch.start();
  }

  final Stopwatch _watch = Stopwatch();

  @override
  Duration get elapsed => _watch.elapsed;
}
