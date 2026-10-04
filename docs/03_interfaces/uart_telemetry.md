# LockSys UART Telemetry, Version 1

| Field | Value |
|---|---|
| Document ID | LS-IF-003 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Interface version | UART telemetry 1 |
| Source of truth | `interfaces/uart/telemetry_v1.md` |
| Normative text | LS-SAIC-001 §9 ([LS-SAIC](../02_system/LS-SAIC.md)) |

## 1. Purpose and scope

This document renders the DCU UART telemetry interface, version 1: the physical layer, the sentence grammar, every sentence with its fields, the implementation constraints, the checks applied by the HIL parser, and the fault-injection input channel of DEV and HIL builds.

- `interfaces/uart/telemetry_v1.md` is the source of truth; LS-SAIC-001 §9 holds the same content as normative text. This document is informative.
- Producer: DCU module `Tlm` ([LS-DCU-SAD-001](../04_software/dcu/architecture.md)). Consumers: the HIL telemetry parser and developers on the ST-LINK virtual COM port.
- The telemetry version is reported in `$LSVER`. Version 1 belongs to system release 1.0.0 together with CAN matrix 1.0 and APP protocol 1.0.

## 2. Physical layer

| Item | Value |
|---|---|
| Peripheral | USART2: TX on PA2, RX on PA3, routed to the ST-LINK/V2-1 virtual COM port |
| Format | 115200 baud (`uart_baud`), 8 data bits, no parity, 1 stop bit, no flow control |
| Character set | ASCII; every line ends with CR LF |
| Direction | DCU → host in all builds; host → DCU only in DEV and HIL builds (§7) |

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

### 3.1 Field types

| Type | Format | Example |
|---|---|---|
| seq | 5 decimal digits, zero-padded; one counter per sentence type, 00000–65535, wrapping | `00042` |
| t_ms | 10 decimal digits, zero-padded; uptime in ms modulo 2^32 | `0000043012` |
| temp | `[+-]D.DD` with 1–3 integer digits (cdeg / 100); empty when no valid value | `+23.45` |
| enum | Canonical enumeration name (LS-SAIC-001 §6.4) | `MOVING_UP` |
| pos | 0–100, or `UNK` | `UNK` |
| kl30 | `D.D` volts with 1–2 integer digits; empty when invalid or not fitted | `12.6` |
| speed | `[+-]D.D` rpm with 1–4 integer digits; empty when invalid | `+169.8` |
| counts | `[+-]D` with 1–10 digits (32-bit signed) | `+12345` |
| hex | Uppercase hexadecimal, width given per field | `9A1171` |

## 4. Sentences

### 4.1 Overview

| Sentence | Content | Rate |
|---|---|---|
| `$LSTMP` | Temperature sample | Each sample (1 Hz), ≤ 10 ms after it completes |
| `$LSSTA` | Node status summary | Every `t_tlm_sta_ms` (1000 ms) |
| `$LSMOT` | Window motion | Every `t_tlm_mot_ms` (100 ms) while the window bridge drives or brakes, and once after it switches off |
| `$LSVER` | Software and interface versions | At boot and every `t_tlm_ver_ms` (60 s) |
| `$LSRST` | Reset reason | At boot |
| `$LSDTC` | DTC status change | On every testFailed change; once per stored entry after boot |
| `#LOG` | Event log | On events, ≤ `n_log_max_per_s` (20) lines per second |

### 4.2 Fields

`$LSTMP`

| # | Field | Type |
|---|---|---|
| 1 | Sequence number; modulo 256 equals `TempSts_SampleSeq` of the matching DCU_TempSts frame | seq |
| 2 | Uptime | t_ms |
| 3 | Temperature | temp |
| 4 | TempStatus (never STALE) | enum |

`$LSSTA`

| # | Field | Type |
|---|---|---|
| 1 | Sequence number | seq |
| 2 | Uptime | t_ms |
| 3 | NodeMode | enum |
| 4 | DoorLockState | enum |
| 5 | WindowState | enum |
| 6 | Window position | pos |
| 7 | KL30 voltage | kl30 |
| 8 | Confirmed DTC count | decimal |
| 9 | Window output-shaft speed | speed |

`$LSMOT`

| # | Field | Type |
|---|---|---|
| 1 | Sequence number | seq |
| 2 | Uptime | t_ms |
| 3 | WindowState | enum |
| 4 | Commanded duty, 0–100 % | decimal |
| 5 | Output-shaft speed | speed |
| 6 | Relative position since reset, positive = UP | counts |
| 7 | EncoderStatus | enum |

`$LSVER`

| # | Field | Type |
|---|---|---|
| 1 | Software version, string from `VERSION` | text |
| 2 | First 7 hexadecimal digits of the git commit (lower case) | text |
| 3 | CAN matrix version, `major.minor` | text |
| 4 | Telemetry version (`1`) | decimal |
| 5 | BuildType | enum |

`$LSRST`

| # | Field | Type |
|---|---|---|
| 1 | ResetReason | enum |
| 2 | Resets since power-on | decimal |

`$LSDTC`

| # | Field | Type |
|---|---|---|
| 1 | Sequence number | seq |
| 2 | Uptime | t_ms |
| 3 | DTC, 3 bytes | hex, 6 digits |
| 4 | DTC status byte | hex, 2 digits |
| 5 | `SET` (testFailed rose) or `CLR` (testFailed fell) | text |

`#LOG`

| # | Field | Type |
|---|---|---|
| 1 | Sequence number | seq |
| 2 | Uptime | t_ms |
| 3 | Level: `E`, `W`, `I` or `D` | text |
| 4 | Module code, 2–4 uppercase letters | text |
| 5 | Event code | hex, 4 digits |
| 6 | Text, ≤ 40 printable characters | text |

The module codes and event codes of the DCU are listed in [LS-DCU-SAD-001](../04_software/dcu/architecture.md) §10.4.

### 4.3 Examples

Checksums are computed; the longest example is 81 characters including CR LF.

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

Worst-case lengths including CR LF: `$LSSTA` 81, `$LSMOT` 78, `$LSVER` 56, `$LSTMP` 49, `$LSDTC` 42 characters.

## 5. Implementation constraints

- A ring buffer of `tlm_ring_bytes` (1024 bytes) feeds USART2 TX through DMA and never blocks the caller.
- On overflow whole lines are dropped, never partial lines, and a line `#LOG,…,W,TLM,0001,DROPPED n` follows when space is available.
- D-level log lines are compiled out of RELEASE builds.
- Telemetry load stays ≤ 20 % of the line capacity (11.5 kB/s). UART activity adds less than 50 µs of jitter to the 1 ms scheduler tick (SYS-090).

## 6. HIL parser checks

| Check | Rule |
|---|---|
| Line syntax | After stripping CR LF: `^(\$LS[A-Z]{3}\|#LOG\|!LSFI),(.*)\*([0-9A-F]{2})$` |
| Checksum and length | Checksum matches; length within the grammar limit |
| Sequence | Per-type sequence continuity; a gap fails the test unless a DROPPED line accounts for it |
| Time | The t_ms field is monotonic, modulo the 32-bit wrap |
| Temperature consistency | `$LSTMP` sequence modulo 256 = `TempSts_SampleSeq`; the UART value equals the CAN value (both in cdeg) |
| Position consistency | `$LSMOT` position modulo 2^24 equals `WinMot_PosCounts` of the DCU_WinMotion frame sent closest in time, within one frame period of motion |

## 7. Fault-injection channel (DEV and HIL builds only)

- Inbound sentence `!LSFI,<cmd>[,<arg>]*CS`. RC and RELEASE builds disable USART2 RX and contain no `Fi_` symbols; CI checks the ELF (and the absence of `cgw_fi_` symbols in the CGW ELF).
- The DCU acknowledges each accepted command with `#LOG,…,I,FI,<code>,ACK <cmd>` and answers a malformed or unknown command with `#LOG,…,W,FI,<code>,NAK`.

| Command | Argument | Effect | Example |
|---|---|---|---|
| HANG | ms (optional; default: forever) | The 10 ms task blocks → hang-monitor path | `!LSFI,HANG,500*25` |
| HANG_IRQOFF | ms (optional) | Interrupts disabled while spinning → IWDG path | `!LSFI,HANG_IRQOFF,50*4F` |
| SKIP_CHECKPOINT | — | One supervised entity misses its checkpoint → alive supervision → IWDG | `!LSFI,SKIP_CHECKPOINT*68` |
| HARDFAULT | — | Executes a faulting access → fault-handler path | `!LSFI,HARDFAULT*69` |
| STACK | — | Provokes a stack overflow → canary or fault path | `!LSFI,STACK*72` |
| ROMCRC | — | The next ROM CRC comparison fails → B1A50 → SAFE | `!LSFI,ROMCRC*3E` |
| E2E_TX | n | Corrupts the CRC of the next n transmitted E2E frames | `!LSFI,E2E_TX,3*42` |
| CLRRST | — | Clears the watchdog-reset counter | `!LSFI,CLRRST*34` |
| CSS | — | Runs the CSS/NMI handler path (clock-failure reaction) | `!LSFI,CSS*7F` |
| SEED | n | Arms negative-control defect n (catalogue in LS-HIL-001) | `!LSFI,SEED,2*35` |

The CGW has its own fault-injection console commands in DEV builds (`fi core_hang <ms>`, `fi canio_hang`, `fi can_silent`, `fi e2e_crc <n>`, `fi e2e_ctr <n>`, `fi drop_stop`, `fi wifi_off`); they are not part of this interface.

## 8. Change rules

- Any change of the grammar, of a sentence or of a field changes the telemetry version reported in `$LSVER` (LS-SAIC-001 §14.3); the HIL parser selects its rules by that version.
- Enumeration values are printed by their canonical names, so additions to an enumeration do not change the grammar.

## 9. Rationale

- **NMEA-style lines with a checksum.** Readable on any terminal, robust against partial lines, and parsed with one regular expression.
- **Whole-line drop on overflow.** Telemetry never blocks a real-time task; the DROPPED marker keeps the sequence check meaningful.
- **Separate start character for inbound commands.** `!` cannot be confused with DCU output, and the channel does not exist in RC and RELEASE builds.

## 10. References

- [LS-SAIC-001](../02_system/LS-SAIC.md) §6.4, §9
- [LS-SRS-001](../02_system/system_requirements.md): SYS-009, SYS-090
- [LS-IF-001 CAN matrix](can_matrix.md) (DCU_TempSts, DCU_WinMotion)
- [HIL architecture](../07_verification/hil_architecture.md) (LS-HIL-001)
