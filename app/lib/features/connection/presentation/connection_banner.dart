// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/core/domain/link_state.dart';
import 'package:locksys_app/features/connection/presentation/connection_cubit.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Connection state with icon and text, never colour alone.
class ConnectionBanner extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    final link = context.watch<ConnectionCubit>().state;
    final (icon, text, color) = switch (link) {
      LinkState.unpaired => (
        Icons.link_off,
        l10n.bannerUnpaired,
        scheme.outline,
      ),
      LinkState.connecting => (
        Icons.sync,
        l10n.bannerConnecting,
        scheme.primary,
      ),
      LinkState.authenticating => (
        Icons.key,
        l10n.bannerAuthenticating,
        scheme.primary,
      ),
      LinkState.connected => (
        Icons.check_circle,
        l10n.bannerConnected,
        scheme.primary,
      ),
      LinkState.degraded => (
        Icons.warning_amber,
        l10n.bannerDegraded,
        scheme.tertiary,
      ),
      LinkState.reconnecting => (
        Icons.sync_problem,
        l10n.bannerReconnecting,
        scheme.tertiary,
      ),
      LinkState.blocked => (Icons.block, l10n.bannerBlocked, scheme.error),
      LinkState.disconnected => (
        Icons.pause_circle,
        l10n.bannerDisconnected,
        scheme.outline,
      ),
    };
    return Semantics(
      liveRegion: true,
      child: ListTile(
        key: const Key('connectionBanner'),
        leading: Icon(icon, color: color),
        title: Text(text),
        trailing: link == LinkState.disconnected
            ? TextButton(
                onPressed: () => context.read<ConnectionCubit>().connect(),
                child: Text(l10n.bannerConnect),
              )
            : null,
      ),
    );
  }
}
