// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';
import 'package:locksys_app/features/door_lock/presentation/door_lock_cubit.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Lock is a single tap; unlock requires a hold of [unlockConfirm].
// @satisfies SWR-APP-041
class DoorLockActionButton extends StatefulWidget {
  const new({required this.unlockConfirm, super.key});

  final Duration unlockConfirm;

  @override
  State<DoorLockActionButton> createState() => _DoorLockActionButtonState();
}

class _DoorLockActionButtonState extends State<DoorLockActionButton> {
  Timer? _confirm;
  bool _holding = false;

  @override
  void dispose() {
    _confirm?.cancel();
    super.dispose();
  }

  void _startHold(DoorLockCubit cubit) {
    _confirm?.cancel();
    setState(() => _holding = true);
    _confirm = Timer(widget.unlockConfirm, () {
      setState(() => _holding = false);
      unawaited(cubit.unlock());
    });
  }

  void _endHold() {
    _confirm?.cancel();
    _confirm = null;
    if (_holding) {
      setState(() => _holding = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final cubit = context.watch<DoorLockCubit>();
    final state = cubit.state;
    final enabled = state.canCommand;
    if (state.lockState != DoorLockState.locked) {
      return FilledButton.icon(
        key: const Key('doorLockActionButton'),
        onPressed: enabled ? cubit.lock : null,
        icon: const Icon(Icons.lock),
        label: Text(state.pending ? l10n.doorPending : l10n.doorLockAction),
      );
    }
    return Listener(
      onPointerDown: enabled ? (_) => _startHold(cubit) : null,
      onPointerUp: (_) => _endHold(),
      onPointerCancel: (_) => _endHold(),
      child: FilledButton.tonalIcon(
        key: const Key('doorLockActionButton'),
        // Activation happens through the hold timer only.
        onPressed: enabled ? () {} : null,
        icon: _holding
            ? const SizedBox.square(
                dimension: 18,
                child: CircularProgressIndicator(strokeWidth: 2),
              )
            : const Icon(Icons.lock_open),
        label: Text(state.pending ? l10n.doorPending : l10n.doorUnlockAction),
      ),
    );
  }
}
