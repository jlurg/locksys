// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/features/temperature/domain/temperature.dart';

class TemperatureCubit extends Cubit<Temperature> {
  new(TemperatureRepository repository) : super(repository.current) {
    _subscription = repository.temperature.listen(emit);
  }

  late final StreamSubscription<Temperature> _subscription;

  @override
  Future<void> close() async {
    await _subscription.cancel();
    await super.close();
  }
}
