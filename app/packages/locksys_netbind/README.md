# locksys_netbind

Flutter plugin of the LockSys APP that joins the gateway SoftAP and gives the APP a usable route
to it (LS-APP-SAD-001, NetBinder; LS-SAIC-001 §8.1 and §8.12).

| Platform | Join | Routing | Local-network access |
|---|---|---|---|
| Android 10+ | `WifiNetworkSpecifier` with exact SSID and BSSID | `bindProcessToNetwork` | TCP probe on the bound network |
| iOS 15+ | `NEHotspotConfiguration` (`joinOnce`) | not required | `NWConnection` probe |

Status (M0): the pigeon API, the Dart facade and the native registration are in place.
`capabilities` and `setSecureWindow` (Android `FLAG_SECURE`) are implemented; `join` and `probe`
return `error` and no network events are emitted. The platform logic is scheduled for M4.

## Regenerating the platform channels

```sh
cd app/packages/locksys_netbind
fvm dart run pigeon --input pigeons/netbind_api.dart
```

Outputs (committed, never edited by hand, under `gen/` so that formatters and whitespace hooks
skip them): `lib/src/gen/netbind_api.g.dart`,
`android/src/main/kotlin/io/github/jlurg/locksys/netbind/gen/NetbindApi.g.kt` and
`ios/locksys_netbind/Sources/locksys_netbind/gen/NetbindApi.g.swift`.

## Tests

```sh
fvm flutter test
```
