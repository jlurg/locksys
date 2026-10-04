# LockSys UART telemetry, version 1

| Field | Value |
|---|---|
| Document ID | LS-SAIC-001-TLM |
| Version | 1 (UART telemetry version 1) |
| Status | Pre-baseline |
| Owner | jlurg |
| Source | LS-SAIC-001 v0.2, section 9 (normative) |

## 1. Purpose and scope

This document is the interface artefact of the DCU UART telemetry (interface IF-03 of LS-SAIC-001). It is consumed by the DCU telemetry service (Tlm) and by the HIL telemetry parser. Its tables and listings are identical to LS-SAIC-001 section 9; `uv run tools/codegen/regen.py --check` fails when they differ or when an example checksum is wrong. A change is made in LS-SAIC-001 first and in this document in the same pull request.

## 2. Physical layer

USART2 → ST-LINK virtual COM port; 115200 baud, 8N1, no flow control; ASCII; every line ends with CRLF. The DCU receive direction is used only in DEV and HIL builds (section 8).

## 3. Grammar

```text
data-line = "$LS" TYP "," fields "*" CS CRLF      ; DCU -> host, <= 82 characters including CRLF
log-line  = "#LOG," fields "*" CS CRLF            ; DCU -> host, <= 120 characters including CRLF
fi-line   = "!LSFI," cmd *("," arg) "*" CS CRLF   ; host -> DCU, DEV and HIL builds only
TYP       = 3 uppercase letters
fields    = field *("," field)
CS        = 2 uppercase hexadecimal digits: XOR of all bytes between the start
            character ("$", "#" or "!") and "*", both excluded
```

Fields never contain `,`, `*`, `$`, `#`, `!`, CR or LF.

| Field type | Format |
|---|---|
| seq | 5 decimal digits, zero-padded; one counter per sentence type, 00000–65535 wrapping |
| t_ms | 10 decimal digits, zero-padded; uptime in ms modulo 2^32 |
| temp | `[+-]D.DD` with 1–3 integer digits (cdeg / 100); empty when no valid value |
| enum | Canonical name (§6.4) |
| pos | 0–100, or `UNK` |
| kl30 | `D.D` volts with 1–2 integer digits; empty when invalid or not fitted |
| speed | `[+-]D.D` rpm with 1–4 integer digits; empty when invalid |
| counts | `[+-]D` with 1–10 digits (32-bit signed) |
| hex | Uppercase hexadecimal, width given per field |

"Canonical name (§6.4)" refers to the enumerations of LS-SAIC-001 section 6.4, generated from `interfaces/enums/locksys_enums.yaml`.

## 4. Sentences

| Sentence | Fields | Rate |
|---|---|---|
| `$LSTMP` | seq, t_ms, temp, TempStatus | Each sample (1 Hz), ≤ 10 ms after it completes |
| `$LSSTA` | seq, t_ms, NodeMode, DoorLockState, WindowState, pos, kl30, confirmed DTC count, speed | Every `t_tlm_sta_ms` |
| `$LSMOT` | seq, t_ms, WindowState, duty % (0–100), speed, position counts, EncoderStatus | Every `t_tlm_mot_ms` while the window bridge drives or brakes, and once after it switches Off |
| `$LSVER` | software version (string from `VERSION`), git7, CAN matrix version (major.minor), telemetry version (1), BuildType | At boot and every `t_tlm_ver_ms` |
| `$LSRST` | ResetReason, resets since power-on | At boot |
| `$LSDTC` | seq, t_ms, DTC (6 hex), status byte (2 hex), `SET` or `CLR` | On every testFailed change; once per stored entry after boot |
| `#LOG` | seq, t_ms, level (`E`, `W`, `I`, `D`), module (2–4 uppercase letters), code (4 hex), text (≤ 40 printable characters) | Events, ≤ `n_log_max_per_s` lines per second |

The `t_*` and `n_*` keys are parameters of `interfaces/params/timing.yaml`.

## 5. Examples

Checksums are computed; the codegen gate recomputes them.

```text
$LSTMP,00042,0000043012,+23.45,VALID*37
$LSTMP,00043,0000044012,,SENSOR_FAULT*61
$LSSTA,00042,0000043020,NORMAL,LOCKED,STOPPED,UNK,12.6,0,+0.0*68
$LSSTA,00043,0000044020,NORMAL,UNLOCKED,MOVING_UP,UNK,12.4,0,+169.8*76
$LSMOT,00007,0000051230,MOVING_UP,100,+169.8,+12345,OK*35
$LSMOT,00008,0000051330,BLOCKED,0,+0.0,+12401,NO_MOTION*79
$LSVER,0.1.0-dev,a1b2c3d,1.0,1,DEV*64
$LSRST,POWER_ON,1*7A
$LSDTC,00001,0000051000,9A1171,2D,SET*1F
$LSDTC,00002,0000062000,9A1171,2C,CLR*04
#LOG,00107,0000051002,W,WIN,0207,NO_MOTION N=3 EXP=90*0F
#LOG,00108,0000051010,W,TLM,0001,DROPPED 4*37
!LSFI,HANG,500*25
```

Worst-case lengths including CRLF: `$LSSTA` 81, `$LSMOT` 78, `$LSVER` 56, `$LSTMP` 49, `$LSDTC` 42 characters.

## 6. Implementation constraints

- A `tlm_ring_bytes` ring buffer feeds USART2 TX through DMA and never blocks. On overflow whole lines are dropped, never partial lines, and a line `#LOG,…,W,TLM,0001,DROPPED n` follows.
- D-level logs are compiled out of RELEASE builds.
- Telemetry load stays ≤ 20 % of the line capacity (11.5 kB/s); UART activity adds < 50 µs jitter to the 1 ms tick (SYS-090).

## 7. HIL parser checks

- Line regex after stripping CRLF: `^(\$LS[A-Z]{3}|#LOG|!LSFI),(.*)\*([0-9A-F]{2})$`.
- Checksum and maximum length.
- Per-type sequence continuity; a gap fails the test unless a DROPPED line accounts for it.
- The t_ms field is monotonic (modulo the 32-bit wrap).
- `$LSTMP` seq modulo 256 = `TempSts_SampleSeq`, and the UART value equals the CAN value (both in cdeg).
- `$LSMOT` position modulo 2^24 equals `WinMot_PosCounts` of the DCU_WinMotion frame sent closest in time, within one frame period of motion.

## 8. Fault injection channel (DEV and HIL builds only)

- Inbound sentence `!LSFI,<cmd>[,<arg>]*CS`. RC and RELEASE builds disable USART2 RX and contain no `Fi_` symbols; CI checks the ELF (and `cgw_fi_` symbols in the CGW ELF).

| Command | Argument | Effect |
|---|---|---|
| HANG | ms (optional; default: forever) | The 10 ms task blocks → hang-monitor path |
| HANG_IRQOFF | ms (optional) | Interrupts disabled while spinning → IWDG path |
| SKIP_CHECKPOINT | — | One supervised entity misses its checkpoint → alive supervision → IWDG |
| HARDFAULT | — | Executes a faulting access → fault-handler path |
| STACK | — | Provokes a stack overflow → canary or fault path |
| ROMCRC | — | The next ROM CRC comparison fails → B1A50 → SAFE |
| E2E_TX | n | Corrupts the CRC of the next n transmitted E2E frames |
| CLRRST | — | Clears the watchdog-reset counter |
| CSS | — | Runs the CSS/NMI handler path (clock-failure reaction) |
| SEED | n | Arms negative-control defect n (catalogue in LS-HIL-001) |

- The DCU acknowledges each accepted command with `#LOG,…,I,FI,<code>,ACK <cmd>` and answers a malformed or unknown command with `#LOG,…,W,FI,<code>,NAK`.
- CGW fault injection (DEV builds, console commands): `fi core_hang <ms>`, `fi canio_hang`, `fi can_silent`, `fi e2e_crc <n>`, `fi e2e_ctr <n>`, `fi drop_stop`, `fi wifi_off`.

## 9. Rationale

- A line-oriented ASCII format with an XOR checksum can be read on any terminal and parsed without a schema compiler; the checksum and the per-type sequence numbers detect corrupted and dropped lines.
- Values use the canonical enumeration names and the CAN units (cdeg, counts), so the HIL compares UART and CAN data without conversion tables.

## 10. References

- LS-SAIC-001 System Architecture & Interface Contract, sections 6.4 and 9 (`docs/02_system/LS-SAIC.md`).
- `interfaces/params/timing.yaml` (telemetry parameters), `interfaces/enums/locksys_enums.yaml`, `interfaces/dtc/dtc_catalog.yaml`.
