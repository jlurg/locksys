// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/app/bootstrap.dart';
import 'package:locksys_app/features/connection/presentation/connection_banner.dart';
import 'package:locksys_app/features/diagnostics/presentation/diagnostics_page.dart';
import 'package:locksys_app/features/door_lock/presentation/door_lock_action_button.dart';
import 'package:locksys_app/features/door_lock/presentation/door_lock_cubit.dart';
import 'package:locksys_app/features/door_lock/presentation/door_lock_status_button.dart';
import 'package:locksys_app/features/temperature/presentation/temperature_cubit.dart';
import 'package:locksys_app/features/temperature/presentation/temperature_tile.dart';
import 'package:locksys_app/features/window/presentation/window_hold_cubit.dart';
import 'package:locksys_app/features/window/presentation/window_status_view.dart';
import 'package:locksys_app/features/window/presentation/window_switch.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Main control screen (SCR-06): status area scrolls, the window switch sits
/// in a fixed panel outside the system gesture insets.
class ControlPage extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final deps = context.read<AppDependencies>();
    return MultiBlocProvider(
      providers: [
        BlocProvider(
          create: (_) => DoorLockCubit(
            repository: deps.doorLock,
            availability: deps.link.availability,
            initialAvailability: deps.link.currentAvailability,
          ),
        ),
        BlocProvider(create: (_) => TemperatureCubit(deps.temperature)),
        BlocProvider(
          create: (_) => WindowHoldCubit(
            controller: deps.holdToRun,
            statusRepository: deps.windowStatus,
            availability: deps.link.availability,
            initialAvailability: deps.link.currentAvailability,
          ),
        ),
      ],
      child: const _ControlView(),
    );
  }
}

class _ControlView extends StatelessWidget {
  const new();

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final deps = context.read<AppDependencies>();
    final media = MediaQuery.of(context);
    final bottomInset =
        media.systemGestureInsets.bottom > media.viewPadding.bottom
        ? media.systemGestureInsets.bottom
        : media.viewPadding.bottom;
    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.appTitle),
        actions: [
          IconButton(
            tooltip: l10n.diagnosticsTitle,
            icon: const Icon(Icons.info_outline),
            onPressed: () => Navigator.of(context).push(
              MaterialPageRoute<void>(
                builder: (_) => DiagnosticsPage(repository: deps.diagnostics),
              ),
            ),
          ),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                const ConnectionBanner(),
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Wrap(
                      spacing: 16,
                      runSpacing: 8,
                      children: [
                        const DoorLockStatusButton(),
                        DoorLockActionButton(
                          unlockConfirm: deps.timings.unlockConfirm,
                        ),
                      ],
                    ),
                  ),
                ),
                const TemperatureTile(),
                const WindowStatusView(),
              ],
            ),
          ),
          Padding(
            padding: EdgeInsets.fromLTRB(16, 8, 16, 16 + bottomInset),
            child: const WindowSwitch(),
          ),
        ],
      ),
    );
  }
}
