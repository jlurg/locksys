// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/features/temperature/presentation/temperature_cubit.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Cabin temperature; greyed when the value is not valid (SWR-APP-052).
class TemperatureTile extends StatelessWidget {
  const new({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final temperature = context.watch<TemperatureCubit>().state;
    final value = temperature.formattedCelsius;
    final text = value != null
        ? l10n.temperatureValue(value)
        : l10n.temperatureStatus(temperature.status.name);
    return Card(
      child: ListTile(
        leading: const Icon(Icons.thermostat),
        title: Text(l10n.temperatureTitle),
        trailing: Text(
          text,
          key: const Key('temperatureValue'),
          style: value == null
              ? TextStyle(color: Theme.of(context).disabledColor)
              : Theme.of(context).textTheme.titleMedium,
        ),
      ),
    );
  }
}
