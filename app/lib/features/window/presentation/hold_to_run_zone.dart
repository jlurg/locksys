// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/features/window/domain/hold_to_run_controller.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';
import 'package:locksys_app/features/window/presentation/window_hold_cubit.dart';

/// One hold-to-run zone of the window switch.
///
/// Raw [Listener] events (no gesture arena): bounds are frozen at
/// pointer-down, a slide beyond [slideOffTolerance] stops the press, and the
/// listener stays mounted across enable and state changes. Semantic actions
/// never move the window (SWR-APP-033).
// @satisfies SWR-APP-030
// @satisfies SWR-APP-033
// @satisfies SWR-APP-034
class HoldToRunZone extends StatefulWidget {
  const new({
    required this.direction,
    required this.label,
    required this.hint,
    required this.icon,
    super.key,
  });

  /// Tolerance beyond the zone rectangle before a slide-off stop (T3).
  static const double slideOffTolerance = 8;

  /// Minimum zone size in logical pixels (SWR-APP-061).
  static const double minSize = 96;

  final WindowDirection direction;
  final String label;
  final String hint;
  final IconData icon;

  @override
  State<HoldToRunZone> createState() => _HoldToRunZoneState();
}

class _HoldToRunZoneState extends State<HoldToRunZone> {
  late WindowHoldCubit _cubit;
  Rect? _bounds;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _cubit = context.read<WindowHoldCubit>();
  }

  @override
  void dispose() {
    _cubit.zoneDisposed();
    super.dispose();
  }

  void _onDown(PointerDownEvent e) {
    final size = context.size ?? Size.zero;
    _bounds = Offset.zero & size;
    final outcome = _cubit.pointerDown(e.pointer, widget.direction);
    if (outcome.isAccepted) {
      HapticFeedback.mediumImpact().ignore();
    } else if (!outcome.ignored) {
      HapticFeedback.heavyImpact().ignore();
    }
  }

  void _onMove(PointerMoveEvent e) {
    final bounds = _bounds;
    if (bounds != null &&
        !bounds
            .inflate(HoldToRunZone.slideOffTolerance)
            .contains(e.localPosition)) {
      _cubit.pointerLeft(e.pointer);
    }
  }

  void _onUp(PointerUpEvent e) {
    final wasHolding = _cubit.state.isPressed(widget.direction);
    _cubit.pointerUp(e.pointer);
    if (wasHolding) {
      HapticFeedback.selectionClick().ignore();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Listener(
      behavior: HitTestBehavior.opaque,
      onPointerDown: _onDown,
      onPointerMove: _onMove,
      onPointerUp: _onUp,
      onPointerCancel: (e) => _cubit.pointerCancel(e.pointer),
      child: BlocBuilder<WindowHoldCubit, WindowHoldViewState>(
        buildWhen: (a, b) =>
            a.isEnabled(widget.direction) != b.isEnabled(widget.direction) ||
            a.isPressed(widget.direction) != b.isPressed(widget.direction) ||
            a.hold.phase != b.hold.phase,
        builder: (context, state) {
          final enabled = state.isEnabled(widget.direction);
          final pressed = state.isPressed(widget.direction);
          final scheme = Theme.of(context).colorScheme;
          return Semantics(
            button: true,
            enabled: enabled,
            label: widget.label,
            hint: widget.hint,
            excludeSemantics: true,
            child: AnimatedContainer(
              duration: const Duration(milliseconds: 80),
              constraints: const BoxConstraints(
                minWidth: HoldToRunZone.minSize,
                minHeight: HoldToRunZone.minSize,
              ),
              decoration: BoxDecoration(
                color: !enabled
                    ? scheme.surfaceContainerHighest
                    : pressed
                    ? scheme.primary
                    : scheme.primaryContainer,
                borderRadius: BorderRadius.circular(16),
                border: Border.all(
                  color: state.hold.phase == HoldPhase.latched
                      ? scheme.error
                      : scheme.outline,
                  width: 2,
                ),
              ),
              alignment: Alignment.center,
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    widget.icon,
                    size: 40,
                    color: pressed ? scheme.onPrimary : null,
                  ),
                  Text(
                    widget.label,
                    style: TextStyle(color: pressed ? scheme.onPrimary : null),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}
