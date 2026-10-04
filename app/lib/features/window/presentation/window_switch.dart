// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';
import 'package:locksys_app/features/window/presentation/hold_to_run_zone.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Rocker-style window switch: CLOSE (up) and OPEN (down) hold zones.
// @satisfies SWR-APP-034
class WindowSwitch extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    return Row(
      children: [
        Expanded(
          child: HoldToRunZone(
            key: const Key('windowZoneUp'),
            direction: WindowDirection.up,
            label: l10n.windowCloseLabel,
            hint: l10n.holdToMoveHint,
            icon: Icons.arrow_upward,
          ),
        ),
        const SizedBox(width: 16),
        Expanded(
          child: HoldToRunZone(
            key: const Key('windowZoneDown'),
            direction: WindowDirection.down,
            label: l10n.windowOpenLabel,
            hint: l10n.holdToMoveHint,
            icon: Icons.arrow_downward,
          ),
        ),
      ],
    );
  }
}
