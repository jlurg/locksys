// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

import 'dart:async';

import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:locksys_app/core/domain/command_result.dart';
import 'package:locksys_app/core/domain/control_availability.dart';
import 'package:locksys_app/features/door_lock/data/fake_door_lock_repository.dart';
import 'package:locksys_app/features/door_lock/domain/door_lock_status.dart';
import 'package:locksys_app/features/door_lock/presentation/door_lock_cubit.dart';

const _available = ControlAvailability.available();
const _offline = ControlAvailability.unavailable(
  UnavailableReason.notConnected,
);

// @verifies SWR-APP-042
void main() {
  late FakeDoorLockRepository repo;
  late StreamController<ControlAvailability> availability;

  setUp(() {
    repo = FakeDoorLockRepository(actuation: Duration.zero);
    availability = StreamController<ControlAvailability>.broadcast();
  });
  tearDown(() async {
    await availability.close();
    await repo.dispose();
  });

  DoorLockCubit build(ControlAvailability initial) => DoorLockCubit(
    repository: repo,
    availability: availability.stream,
    initialAvailability: initial,
  );

  blocTest<DoorLockCubit, DoorLockViewState>(
    'unlock: pending, transient state, final state and result',
    build: () => build(_available),
    act: (c) => c.unlock(),
    expect: () => const [
      DoorLockViewState(
        lockState: DoorLockState.locked,
        availability: _available,
        pending: true,
      ),
      DoorLockViewState(
        lockState: DoorLockState.unlocking,
        availability: _available,
        pending: true,
      ),
      DoorLockViewState(
        lockState: DoorLockState.unlocked,
        availability: _available,
        lastResult: CommandResult.ok,
      ),
    ],
    verify: (_) => expect(repo.requestIds, [1]),
  );

  blocTest<DoorLockCubit, DoorLockViewState>(
    'one command in flight; request ids increase',
    build: () => build(_available),
    act: (c) async {
      final first = c.unlock();
      await c.lock();
      await first;
      await c.lock();
    },
    verify: (c) {
      expect(repo.requestIds, [1, 2]);
      expect(c.state.lockState, DoorLockState.locked);
    },
  );

  blocTest<DoorLockCubit, DoorLockViewState>(
    'no command without availability; availability changes are projected',
    build: () => build(_offline),
    act: (c) async {
      await c.lock();
      availability.add(_available);
    },
    expect: () => const [
      DoorLockViewState(
        lockState: DoorLockState.locked,
        availability: _available,
      ),
    ],
    verify: (_) => expect(repo.requestIds, isEmpty),
  );

  blocTest<DoorLockCubit, DoorLockViewState>(
    'refresh republishes the current state',
    build: () => build(_available),
    act: (c) => c.refresh(),
    expect: () => const [
      DoorLockViewState(
        lockState: DoorLockState.locked,
        availability: _available,
      ),
    ],
  );

  test('a busy actuator rejects a second command', () async {
    final slow = FakeDoorLockRepository(
      actuation: const Duration(milliseconds: 20),
    );
    final first = slow.send(DoorAction.lock, requestId: 1);
    expect(
      await slow.send(DoorAction.unlock, requestId: 2),
      CommandResult.rejectedBusy,
    );
    expect(await first, CommandResult.ok);
    await slow.dispose();
  });
}
