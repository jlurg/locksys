// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Explains pairing before the camera opens (SCR-03).
class PairingPrimerPage extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(l10n.pairingTitle)),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Text(l10n.pairingInstructions),
      ),
    );
  }
}
