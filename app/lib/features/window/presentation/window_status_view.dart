// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/features/window/domain/window_status.dart';
import 'package:locksys_app/features/window/presentation/window_hold_cubit.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Window state, position and stop reason from the latest status
/// (SWR-APP-035). Motion is shown only when the DCU reports it.
class WindowStatusView extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final status = context.select<WindowHoldCubit, WindowStatus>(
      (c) => c.state.status,
    );
    final position = status.positionPct;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              l10n.windowTitle,
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            LinearProgressIndicator(
              value: position == null ? null : position / 100,
            ),
            const SizedBox(height: 8),
            Text(
              position == null
                  ? l10n.windowPositionUnknown
                  : l10n.windowPositionOpen(position),
              key: const Key('windowPosition'),
            ),
            Text(
              l10n.windowStateName(status.state.name),
              key: const Key('windowState'),
            ),
            if (status.stopReason.isForced)
              Chip(label: Text(l10n.windowStopReason(status.stopReason.name))),
          ],
        ),
      ),
    );
  }
}
