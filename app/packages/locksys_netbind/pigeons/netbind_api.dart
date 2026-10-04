// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

// Pigeon definition of the locksys_netbind platform API (LS-APP-SAD-001).
// Regenerate: fvm dart run pigeon --input pigeons/netbind_api.dart
// Pigeon parses classic constructors and positional host-API parameters only.
// ignore_for_file: unnecessary_type_name_in_constructor
// ignore_for_file: avoid_positional_boolean_parameters
import 'package:pigeon/pigeon.dart';

@ConfigurePigeon(
  PigeonOptions(
    dartOut: 'lib/src/gen/netbind_api.g.dart',
    dartPackageName: 'locksys_netbind',
    kotlinOut: 'android/src/main/kotlin/io/github/jlurg/locksys/netbind/gen/NetbindApi.g.kt',
    kotlinOptions: KotlinOptions(package: 'io.github.jlurg.locksys.netbind'),
    swiftOut:
        'ios/locksys_netbind/Sources/locksys_netbind/gen/NetbindApi.g.swift',
    copyrightHeader: 'pigeons/copyright_header.txt',
  ),
)
/// Security of the SoftAP named in the pairing payload (`sec`).
enum JoinSecurity { wpa3, wpa2wpa3 }

/// Outcome of a join request.
enum JoinStatus {
  joined,
  alreadyJoined,
  userDenied,
  notFound,
  timeout,
  wpa3Unsupported,
  invalidCredentials,
  notForeground,
  error,
}

/// Outcome of a local-network probe.
enum LocalNetStatus { reachable, denied, timeout, error }

/// Change of the joined network.
enum NetEventKind { available, lost, unavailable }

/// Platform facts that gate the join strategy.
class NetCapabilities {
  NetCapabilities({
    required this.osVersion,
    required this.wpa3Sae,
    required this.staConcurrencyLocalOnly,
  });

  /// Android SDK_INT or the iOS system version.
  final String osVersion;

  /// Whether the device supports WPA3-SAE.
  final bool wpa3Sae;

  /// Whether a local-only Wi-Fi connection can coexist with the primary one.
  final bool staConcurrencyLocalOnly;
}

/// Join parameters taken from the pairing record.
class JoinRequest {
  JoinRequest({
    required this.ssid,
    required this.bssid,
    required this.passphrase,
    required this.security,
    required this.timeoutMs,
  });

  final String ssid;
  final String? bssid;
  final String passphrase;
  final JoinSecurity security;
  final int timeoutMs;
}

/// Result of [NetbindHostApi.join].
class JoinResult {
  JoinResult({required this.status, this.detail});

  final JoinStatus status;
  final String? detail;
}

/// Result of [NetbindHostApi.probe].
class LocalNetResult {
  LocalNetResult({required this.status, this.elapsedMs});

  final LocalNetStatus status;
  final int? elapsedMs;
}

/// Network change event.
class NetEvent {
  NetEvent({required this.kind});

  final NetEventKind kind;
}

/// Calls from Dart into the platform.
@HostApi()
abstract class NetbindHostApi {
  NetCapabilities capabilities();

  @async
  JoinResult join(JoinRequest request);

  @async
  LocalNetResult probe(String host, int port, int timeoutMs);

  void release();

  void setSecureWindow(bool enabled);
}

/// Network events from the platform.
@EventChannelApi()
abstract class NetbindEventApi {
  NetEvent netEvents();
}
