// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:locksys_app/core/domain/link_state.dart';
import 'package:locksys_app/features/connection/domain/link_repository.dart';

/// Projects the link state for the banner (SWR-APP-024).
class ConnectionCubit extends Cubit<LinkState> {
  new(LinkRepository repository)
    : _repository = repository,
      super(repository.current) {
    _subscription = repository.state.listen(emit);
  }

  final LinkRepository _repository;
  late final StreamSubscription<LinkState> _subscription;

  Future<void> connect() => _repository.connect();

  @override
  Future<void> close() async {
    await _subscription.cancel();
    await super.close();
  }
}
