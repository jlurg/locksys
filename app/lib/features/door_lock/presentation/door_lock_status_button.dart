// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';
import 'package:locksys_app/features/door_lock/presentation/door_lock_cubit.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Shows the door lock state; a tap requests a status refresh.
// @satisfies SWR-APP-040
class DoorLockStatusButton extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final lockState = context.select<DoorLockCubit, DoorLockState>(
      (c) => c.state.lockState,
    );
    final (icon, text) = switch (lockState) {
      DoorLockState.locked => (Icons.lock, l10n.doorLocked),
      DoorLockState.unlocked => (Icons.lock_open, l10n.doorUnlocked),
      DoorLockState.locking => (Icons.lock_clock, l10n.doorLocking),
      DoorLockState.unlocking => (Icons.lock_clock, l10n.doorUnlocking),
      DoorLockState.fault => (Icons.error_outline, l10n.doorFault),
      DoorLockState.unknown => (Icons.help_outline, l10n.doorUnknown),
    };
    return OutlinedButton.icon(
      key: const Key('doorLockStatusButton'),
      onPressed: () => context.read<DoorLockCubit>().refresh(),
      icon: Icon(icon),
      label: Text(text),
    );
  }
}
