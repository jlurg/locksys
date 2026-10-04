# CGW target tests

ESP-IDF Unity test application for the adapters, run on the Lab Host at M3 (LS-CGW-SAD-001 section 14). It is not part of the M0 skeleton.

| Test group | Scope | Notes |
|---|---|---|
| TWAI self-test loopback | `cgw_can_esp`: timing, acceptance filter (0x200–0x23F, 0x510/0x590), TX slots, bus-off recovery | Self-test mode on GPIO18; no traffic on the bench bus |
| Key store | `cgw_store_nvs`: pairing record and passphrase round trip, erase, error handling | Dedicated test NVS partition |
| Crypto | `cgw_crypto_psa`: `interfaces/vectors/crypto_kat.json` and `app_session_v1.json` through PSA | Same vectors as the host tests |
| Trace outputs | `cgw_platform_esp`: TRACE0 pulse widths, TRACE1..3 levels | Logic analyser on the HIL bench |

The portable core is tested on the host: see `../host/project.yml`.
