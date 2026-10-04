// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:locksys_app/features/diagnostics/domain/diagnostics_snapshot.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Diagnostics screen (SCR-07); canonical names are shown verbatim.
class DiagnosticsPage extends StatelessWidget {
  const new({required this.repository, super.key});

  final DiagnosticsRepository repository;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final s = repository.snapshot();
    final rows = <(String, String)>[
      (
        'APP',
        s.gitSha.isEmpty ? s.appVersion : '${s.appVersion} (${s.gitSha})',
      ),
      ('Protocol', s.protocolVersion),
      ('Environment', s.environment),
      ('CGW firmware', s.cgwFirmware ?? '-'),
      ('Last close code', s.lastCloseCode?.toString() ?? '-'),
    ];
    return Scaffold(
      appBar: AppBar(title: Text(l10n.diagnosticsTitle)),
      body: ListView(
        children: [
          for (final (k, v) in rows)
            ListTile(title: Text(k), trailing: Text(v)),
        ],
      ),
    );
  }
}
