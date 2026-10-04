// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/app/bootstrap.dart';
import 'package:locksys_app/app/lifecycle_observer.dart';
import 'package:locksys_app/app/theme/app_theme.dart';
import 'package:locksys_app/app/view/control_page.dart';
import 'package:locksys_app/features/connection/presentation/connection_cubit.dart';
import 'package:locksys_app/l10n/gen/app_localizations.dart';

/// Root widget: providers, theme, localisation and the control page.
class LockSysApp extends StatefulWidget {
  const new({required this.dependencies, super.key});

  final AppDependencies dependencies;

  @override
  State<LockSysApp> createState() => _LockSysAppState();
}

class _LockSysAppState extends State<LockSysApp> {
  late final LifecycleObserver _lifecycle;

  @override
  void initState() {
    super.initState();
    _lifecycle = LifecycleObserver(controller: widget.dependencies.holdToRun);
  }

  @override
  void dispose() {
    _lifecycle.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final deps = widget.dependencies;
    return RepositoryProvider<AppDependencies>.value(
      value: deps,
      child: BlocProvider(
        create: (_) => ConnectionCubit(deps.link),
        child: MaterialApp(
          onGenerateTitle: (context) => AppLocalizations.of(context).appTitle,
          theme: AppTheme.light(),
          darkTheme: AppTheme.dark(),
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          supportedLocales: AppLocalizations.supportedLocales,
          home: const ControlPage(),
        ),
      ),
    );
  }
}
