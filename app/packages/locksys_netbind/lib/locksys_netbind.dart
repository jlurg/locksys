// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 jlurg

/// Wi-Fi join, process binding and local-network probe for the LockSys SoftAP.
///
/// Only `core/platform/` of the LockSys APP may import this library
/// (LS-APP-SAD-001 dependency rules).
library;

export 'src/gen/netbind_api.g.dart'
    show
        JoinRequest,
        JoinResult,
        JoinSecurity,
        JoinStatus,
        LocalNetResult,
        LocalNetStatus,
        NetCapabilities,
        NetEvent,
        NetEventKind;
export 'src/netbind.dart';
