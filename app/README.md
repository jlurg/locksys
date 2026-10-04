# LockSys APP

Flutter application that controls the LockSys door lock and the hold-to-run window through the
CGW SoftAP (LS-APP-SAD-001, `docs/04_software/app/`). Protocol: LS-SAIC-001 §8.

| Item | Value |
|---|---|
| Flutter / Dart | 3.47.6 / 3.13.5, pinned by `.fvmrc` (FVM) |
| Application ID | `io.github.jlurg.locksys` |
| Platforms | Android 10+ (minSdk 29, targetSdk 36), iOS 15+ |
| State management | `bloc` / `flutter_bloc`; manual composition root in `lib/app/bootstrap.dart` |

## Layout

| Path | Content |
|---|---|
| `lib/app/` | Composition root, root widget, lifecycle observer, theme, control page |
| `lib/core/` | `config`, `domain`, `link` (WebSocket transport, protobuf mapping), `platform` (NetBinder), `time`, `logging` |
| `lib/features/<f>/{data,domain,presentation}` | `connection`, `pairing`, `door_lock`, `window`, `temperature`, `diagnostics` |
| `lib/l10n/` | `arb/app_en.arb`; `gen/` is produced by `fvm flutter gen-l10n` and committed |
| `packages/locksys_protocol/` | Pure Dart: generated protobuf, `LsParams`, notice/DTC constants, frame codec, HMAC/HKDF session crypto |
| `packages/locksys_netbind/` | Plugin: SoftAP join, process binding, local-network probe (pigeon) |
| `config/` | `--dart-define-from-file` inputs: `mock`, `sim_android_emulator`, `sim_ios_simulator`, `real` |

Dependency rules (checked by `test/architecture/layering_test.dart`): domain code depends only on
`dart:` libraries, `equatable`, `meta` and other domain code; features never import each other;
presentation never imports data; `locksys_protocol` is used only in `features/*/data`,
`core/link` and the composition root; `locksys_netbind` only in `core/platform`.

`lib/features/window/domain/hold_to_run_controller.dart` holds all [SAF] window logic (press
admission, keep-alive, stop triggers T1-T12, latch, interlock, press gap).

## Status (M0)

Only the `mock` environment is wired: in-process fakes of the link, door actuator, window plant and
temperature. `sim` and `real` stop at start-up until the CGW session (handshake, `SessionChannel`,
reconnect policy) is integrated. Pairing has the payload parser and an in-memory store; QR
scanning and secure storage are not part of this build.

## Commands

```sh
cd app
fvm flutter pub get --enforce-lockfile
fvm flutter gen-l10n
fvm flutter analyze --fatal-infos
fvm flutter test --coverage
fvm flutter run --dart-define-from-file=config/mock.json

cd packages/locksys_protocol && fvm dart test
cd packages/locksys_netbind && fvm flutter test
```

Generated protocol code: `uv run tools/codegen/regen.py` from the repository root (requires `dart`
and `protoc-gen-dart` from `protoc_plugin` 25.1.0 on `PATH`).
