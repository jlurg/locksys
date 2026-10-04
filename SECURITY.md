# Security policy

LockSys is a bench demonstrator. It is not intended for installation in a vehicle. Never test
LockSys, or any finding about it, on a real vehicle or on public roads.

## Supported versions

Only the latest minor release line receives security fixes. Fixes are not backported to
older lines.

| Version | Supported |
|---|---|
| Latest minor release (`X.Y.z`) | Yes |
| Older releases | No |
| `develop` (integration branch) | Fixed through the normal development flow |

## Reporting a vulnerability

Report vulnerabilities privately through GitHub private vulnerability reporting:
<https://github.com/jlurg/locksys/security/advisories/new>.

Do not open a public issue, discussion or pull request for a suspected vulnerability, and do
not include secrets or real pairing data in the report.

A useful report contains:

- the affected component (DCU, CGW, APP, HIL tooling, CI workflows) and version or commit;
- the steps to reproduce, with the bench configuration if relevant;
- the impact you expect, for example unintended actuation, session takeover or secret
  disclosure.

## Response targets

| Step | Target |
|---|---|
| Acknowledgement | Within 7 days |
| Triage (validity, severity, plan) | Within 14 days |
| Fix | In the next patch release of the supported line |
| Disclosure | Coordinated through a GitHub security advisory once a fix is available |

## Scope

In scope:

- the CGW Wi-Fi access point, WebSocket interface, session authentication and pairing;
- the APP session handling and secret storage;
- CAN messages, end-to-end protection and the DCU command interlocks;
- secrets in logs, console output and CI artifacts;
- the GitHub Actions workflows and the self-hosted runner trust model.

Out of scope:

- attacks that require physical access to the bench internals or debug ports;
- radio jamming and other denial of service on the physical layer;
- known vulnerabilities in third-party dependencies without a LockSys-specific impact
  (report them upstream);
- findings that require a vehicle.

The threat model and security goals are described in the
[security concept](docs/06_security/security_concept.md).
