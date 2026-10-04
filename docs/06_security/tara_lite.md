# LockSys TARA-lite

| Field | Value |
|---|---|
| Document ID | LS-SEC-002 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |

## 1. Purpose and scope

This threat analysis and risk assessment (lite) derives the cybersecurity goals and requirements of the [security concept](security_concept.md) (LS-SEC-001). It covers the MVP product (APP, CGW, DCU, CAN) and the development infrastructure (public repository, CI, Lab Host, HIL bench).

The method follows the steps of ISO/SAE 21434 clause 15 in reduced form: asset identification, damage scenarios with impact rating, threat scenarios, attack paths, attack feasibility, risk determination and treatment decision. It is not a certified TARA.

## 2. Method

### 2.1 Impact rating

Impact is rated per damage scenario in the safety (S), financial (F), operational (O) and privacy (P) categories; the highest category sets the rating. Safety impact uses the vehicle-intent HARA of LS-SAF-001; the bench impact is noted where it is lower.

| Rating | Meaning for LockSys |
|---|---|
| Severe | Possible life-threatening injury (S3 hazard), or firmware integrity lost in a way that defeats safety mechanisms |
| Major | Theft enabled, compromise of keys or infrastructure, tampered releases reaching users |
| Moderate | Nuisance actuation, loss of availability with the safe state maintained, disclosure of presence information |
| Negligible | No noticeable effect |

### 2.2 Attack feasibility

The baseline is the attack vector of the attack path; it is adjusted by one step when the path additionally requires secret knowledge (passphrase, K_pair), physical presence or specialist equipment.

| Attack vector | Baseline feasibility | Example |
|---|---|---|
| Network | High | Public repository, package registries |
| Adjacent | Medium | Wi-Fi within RF range |
| Local | Low | Console or UART access, app storage on the phone |
| Physical | Very Low | CAN wiring, debug ports, BOOT button |

### 2.3 Risk determination

| Impact \ Feasibility | Very Low | Low | Medium | High |
|---|---|---|---|---|
| Severe | 2 | 3 | 4 | 5 |
| Major | 1 | 2 | 3 | 4 |
| Moderate | 1 | 2 | 2 | 3 |
| Negligible | 1 | 1 | 1 | 1 |

### 2.4 Treatment rule

| Risk value | Decision |
|---|---|
| 4–5 | Reduce: cybersecurity requirements are mandatory |
| 3 | Reduce; a residual value of 3 may be retained only with a documented rationale and monitoring |
| 2 | Reduce where the cost is low; otherwise accept with a rationale |
| 1 | Accept |

The initial risk is assessed without the controls listed in the threat row; the residual risk with them.

## 3. Damage scenarios

| ID | Damage scenario | Assets | S | F | O | P | Impact |
|---|---|---|---|---|---|---|---|
| DS-01 | The window closes without the owner's intent | AS-02 | Severe (vehicle); Moderate (bench) | — | Moderate | — | Severe |
| DS-02 | The door is unlocked by an unauthorised party | AS-01 | — | Major | Moderate | Moderate | Major |
| DS-03 | Nuisance actuation (lock, unlock or window opening) | AS-01, AS-02 | — | — | Moderate | — | Moderate |
| DS-04 | The owner cannot operate the lock or the window | AS-01, AS-02 | — | — | Moderate | — | Moderate |
| DS-05 | Lock state or presence is disclosed | AS-07 | — | — | — | Moderate | Moderate |
| DS-06 | Pairing secrets (K_pair, passphrase) are disclosed, enabling DS-01…DS-03 | AS-03, AS-05 | — | Major | Major | — | Major |
| DS-07 | Firmware or configuration is tampered with | AS-06 | Severe | Major | Major | — | Severe |
| DS-08 | The Lab Host or the bench is compromised (code execution, uncontrolled actuation, credential theft) | AS-08 | Moderate (bench) | Major | Major | — | Major |
| DS-09 | Repository history, release artifacts or verification evidence are tampered with | AS-09 | — | Major | Major | — | Major |

## 4. Threat scenarios

Feasibility and risk are given as initial → residual.

| ID | Threat scenario (STRIDE) | Damage | Attack path | Vector | Feasibility | Impact | Risk | Treatment | Controls |
|---|---|---|---|---|---|---|---|---|---|
| TS-01 | Spoofed APP commands (S) | DS-01, DS-02 | Join the SoftAP, open a WebSocket session, send `WindowMove` or `DoorCommand` | Adjacent | Medium → Very Low (needs passphrase and K_pair) | Severe | 4 → 2 | Reduce | CSR-001, CSR-004, CSR-005, CSR-006, CSR-008 |
| TS-02 | Replay of captured frames (T) | DS-01, DS-02 | Capture frames of a session, re-send them in the same or a later session | Adjacent | Low (requires the passphrase to join the network) → Very Low | Severe | 3 → 2 | Reduce | CSR-005, CSR-006 |
| TS-03 | Guessing the passphrase or K_pair (S) | DS-06 | Offline dictionary attack on the Wi-Fi handshake; online guessing of the session proof | Adjacent | Medium → Very Low (SAE resists offline guessing; ≈ 100-bit passphrase; 256-bit key; throttling) | Major | 3 → 1 | Reduce | CSR-001, CSR-002, CSR-007, CSR-010 |
| TS-04 | Wi-Fi denial of service: deauthentication, jamming, flooding (D) | DS-04 | Forged management frames, RF jamming, frame flooding | Adjacent | Medium → Medium (jamming remains possible) | Moderate | 2 → 2 | Reduce where cheap; accept: availability only, safe stop (CSG-04) | CSR-001 (PMF), CSR-008 (rate limit, client limit) |
| TS-05 | Rogue access point or evil twin (S) | DS-05, DS-06 | Fake access point with the same SSID; phone connects and reveals data or accepts forged status | Adjacent | Medium → Very Low (SAE requires the passphrase; `server_proof`; device ID and BSSID pinned from the QR) | Major | 3 → 1 | Reduce | CSR-001, CSR-004 |
| TS-06 | Sniffing of application traffic (I) | DS-05 | Passive capture of Wi-Fi traffic | Adjacent | Medium → Low (WPA3 link encryption with forward secrecy) | Moderate | 2 → 2 | Accept; AEAD LATER | CSR-001 |
| TS-07 | CAN injection with physical access (S, T) | DS-01, DS-02 | Connect to the CAN wiring and send `WinCmd` / `DoorCmd` with valid E2E | Physical | Very Low → Very Low | Severe | 2 → 2 | Accept for the MVP; authenticated CAN LATER | — (E2E is not a security control) |
| TS-08 | Firmware extraction or tampering through SWD / USB-JTAG (T, I) | DS-06, DS-07 | Attach a debugger, read flash, write a modified image | Physical | Very Low → Very Low | Severe | 2 → 2 | Accept in the MVP; release hardening deferred | CSR-016; LS-SEC-001 §13 |
| TS-09 | Key extraction from the phone (I) | DS-06 | Read app storage on an unlocked or compromised phone; backup extraction | Local | Low → Very Low | Major | 2 → 1 | Reduce | CSR-014 |
| TS-10 | Supply-chain compromise (T) | DS-07, DS-09 | Malicious package, action, container image or vendored source update | Network | High → Low | Severe | 5 → 3 | Reduce; residual retained with monitoring (Dependabot, advisories, review of updates) | CSR-018, CSR-019 |
| TS-11 | Secret disclosure through console output, logs, test evidence or public artifacts (I) | DS-06 | Read the pairing QR or key material from CI logs, HIL evidence or release assets | Network | High → Very Low | Major | 4 → 1 | Reduce | CSR-012, CSR-013 |
| TS-12 | Fork pull request executes code on the self-hosted runner (E, T) | DS-08 | Open a pull request whose workflow targets the self-hosted labels | Network | High → Very Low | Major | 4 → 1 | Reduce | CSR-017, CSR-019 |
| TS-13 | Workflow injection through untrusted event data (T, E) | DS-08, DS-09 | Crafted branch name, title or body interpolated into a `run:` script | Network | Medium → Very Low | Major | 3 → 1 | Reduce | CSR-018 |
| TS-14 | Debug or fault-injection features present in release images (E) | DS-01, DS-07 | Send fault-injection commands over the DCU UART or the CGW console of a release image | Local | Low → Very Low | Severe | 3 → 2 | Reduce | CSR-015 |
| TS-15 | Hijack of an open pairing window (S) | DS-06, DS-01 | Press BOOT, read the console QR and pair a foreign phone | Physical | Very Low → Very Low | Major | 1 → 1 | Accept | CSR-010 |
| TS-16 | Malformed input exploiting protocol decoder defects (T, E) | DS-07, DS-04 | Oversized, deeply nested or malformed frames before or after authentication | Adjacent (needs the passphrase) | Low → Very Low | Severe | 3 → 2 | Reduce | CSR-003, CSR-006, CSR-008, CSR-009 |
| TS-17 | Compromise of the maintainer account or commit-signing key (S, E) | DS-09 | Phishing or credential theft; pushes or tags with a stolen identity | Network | Medium → Low | Major | 3 → 2 | Reduce | CSR-019 |
| TS-18 | Remote access to the Lab Host from outside the lab network (E) | DS-08 | Exposed SSH or RDP port; weak authentication | Network | High → Low | Major | 4 → 2 | Reduce | CSR-020 |

## 5. Residual risk summary

| Residual risk | Threats | Rationale |
|---|---|---|
| 3 | TS-10 | Supply-chain risk cannot be eliminated; pinning, review of updates and advisory monitoring keep it bounded. |
| 2 | TS-01, TS-02, TS-04, TS-06, TS-07, TS-08, TS-14, TS-16, TS-17, TS-18 | Accepted for the bench demonstrator. TS-04: RF jamming cannot be prevented and ends in the safe stop of SG-01. TS-06, TS-07 and TS-08 have planned LATER controls (application-layer AEAD, authenticated CAN, release hardening). |
| 1 | TS-03, TS-05, TS-09, TS-11, TS-12, TS-13, TS-15 | Accepted |

## 6. Review triggers

The TARA is reviewed when any of the following occurs:

- a new external interface or a new node is added;
- a major version change of the APP protocol or the CAN matrix;
- release hardening (LS-SEC-001 §13) is scheduled;
- a window stage (B–E) changes the safety impact of DS-01;
- a new class of third-party dependency or a new CI trigger is introduced;
- a vulnerability advisory affects a pinned component (ESP-IDF, mbedTLS, nanopb, Flutter packages, Python packages, GitHub Actions).

## 7. References

| Reference | Title |
|---|---|
| LS-SEC-001 | [Cybersecurity concept](security_concept.md) |
| LS-SAF-001 | [Functional safety concept](../05_safety/safety_concept.md) |
| LS-SAIC-001 | System architecture and interface contract (`docs/02_system/LS-SAIC.md`) |
| ISO/SAE 21434:2021 | Road vehicles — Cybersecurity engineering, clause 15 and annexes (informative use) |
