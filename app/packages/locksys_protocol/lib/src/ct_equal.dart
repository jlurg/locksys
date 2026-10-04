// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Compares two byte sequences in time that depends only on their lengths.
///
/// Returns false when the lengths differ. Used for proofs and tags
/// (LS-SAIC-001 section 8.3).
// @satisfies SWR-APP-020
bool constantTimeEquals(List<int> a, List<int> b) {
  if (a.length != b.length) {
    return false;
  }
  var diff = 0;
  for (var i = 0; i < a.length; i++) {
    diff |= (a[i] ^ b[i]) & 0xFF;
  }
  return diff == 0;
}
