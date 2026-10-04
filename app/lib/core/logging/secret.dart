// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Wraps key material and passphrases so that they never reach a log
/// through `toString` (SWR-APP-005).
// @satisfies SWR-APP-005
final class Secret<T> {
  const new(this.value);

  final T value;

  @override
  String toString() => '<redacted>';
}

final _pairingUri = RegExp(r'locksys://pair\S*');
final _secretParam = RegExp(r'([?&](?:p|k)=)[^&\s]*');

/// Masks pairing URIs and `p=`/`k=` parameters in a log line.
String redact(String line) => line
    .replaceAll(_pairingUri, 'locksys://pair<redacted>')
    .replaceAllMapped(_secretParam, (m) => '${m[1]}<redacted>');
