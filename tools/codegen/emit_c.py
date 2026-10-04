# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""C emitters: enumerations, parameters, DTC codes, CAN matrix, DCU Dem table, E2E test vectors.

Every constant is a macro with an explicit unsigned suffix or a cast to its 8-bit type, so the
headers stay usable under MISRA C:2012 essential-type rules. Comments are ASCII.
"""

from __future__ import annotations

import re
from typing import Any

from tools.codegen.common import banner, snake_upper
from tools.codegen.model import Catalogue, Dtc, EnumSet, Param, ParamGroup

SPDX = "/* SPDX-License-Identifier: Apache-2.0 */\n/* Copyright (c) 2026 jlurg */\n"

_ASCII = {
    "→": "->",
    "←": "<-",
    "≤": "<=",
    "≥": ">=",
    "×": "x",
    "µ": "u",
    "°": "deg",
    "–": "-",
    "—": "-",
    "\u2212": "-",
    "…": "...",
    "±": "+/-",
    "Ω": "Ohm",
    "≠": "!=",
    "§": "section ",
}


def ascii_text(text: str) -> str:
    """Transliterate contract text for C comments (ASCII only, no comment terminators)."""
    for src, dst in _ASCII.items():
        text = text.replace(src, dst)
    text = text.replace("*/", "* /").replace("`", "")
    return text.encode("ascii", "replace").decode("ascii")


def _guard(name: str) -> str:
    return re.sub(r"[^A-Z0-9]", "_", name.upper())


def _header(file_name: str, source: str, brief: str, includes: list[str], body: list[str]) -> str:
    guard = _guard(file_name)
    lines = [
        banner(source, "c").rstrip("\n"),
        SPDX.rstrip("\n"),
        "",
        "/**",
        f" * @file {file_name}",
        f" * @brief {brief}",
        " */",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]
    lines += [f"#include {inc}" for inc in includes]
    if includes:
        lines.append("")
    lines += body
    lines += ["", f"#endif /* {guard} */", ""]
    return "\n".join(lines)


def _define(name: str, value: str, comment: str | None = None, width: int = 48) -> str:
    text = f"#define {name.ljust(width)} {value}"
    return f"{text} /**< {comment} */" if comment else text


def _int_literal(value: int, hex_digits: int = 0) -> str:
    if value < 0:
        return f"({value})"
    if hex_digits:
        return f"(0x{value:0{hex_digits}X}u)"
    return f"({value}u)"


# --------------------------------------------------------------------------- enumerations


def enum_constant(enum_name: str, value_name: str) -> str:
    """C macro name of an enumeration value (``LS_WINDOW_STATE_FULLY_OPEN``)."""
    return f"LS_{snake_upper(enum_name)}_{value_name}"


def enum_type(enum_name: str) -> str:
    """C type name of an enumeration (``Ls_WindowStateType``)."""
    return f"Ls_{enum_name}Type"


def enums_header(enum_set: EnumSet, source: str) -> str:
    """libs/ls_common/gen/ls_enums_gen.h."""
    body = [
        "/*",
        " * Canonical enumerations of LS-SAIC-001 section 6.4. Every enumeration is an 8-bit",
        " * unsigned type with one constant per value; value 0 is the safe or unknown default and",
        " * the values are 0 .. <ENUM>_COUNT - 1. A received value >= <ENUM>_COUNT is invalid input.",
        " */",
    ]
    for enum in enum_set.enums.values():
        prefix = f"LS_{enum.snake}"
        width = "proto only" if enum.bits is None else f"CAN {enum.bits} bits"
        body += [
            "",
            f"/** @brief {enum.name} ({width}; used by {ascii_text(enum.used_by)}). */",
            f"typedef uint8_t {enum_type(enum.name)};",
        ]
        for value in enum.values:
            comment = ascii_text(value.note) if value.note else None
            body.append(
                _define(
                    f"{prefix}_{value.name}", f"(({enum_type(enum.name)}){value.value}u)", comment
                )
            )
        body.append(
            _define(f"{prefix}_COUNT", f"({len(enum.values)}u)", f"number of {enum.name} values")
        )
        if enum.bits is not None:
            body.append(_define(f"{prefix}_BITS", f"({enum.bits}u)", "CAN signal width"))
    return _header(
        "ls_enums_gen.h",
        source,
        "Canonical enumerations (LS-SAIC-001 section 6.4).",
        ["<stdint.h>"],
        body,
    )


# --------------------------------------------------------------------------- parameters


def param_macro(key: str) -> str:
    """C macro of a parameter key (``LS_T_APP_KA_MS``)."""
    return f"LS_{key.upper()}"


#: Units of signed quantities: their parameters use signed literals so that comparisons with
#: signed variables never convert a negative value to unsigned.
SIGNED_UNITS = ("cdeg", "cdeg/s", "counts", "counts/s")


def is_signed_param(param: Param) -> bool:
    """True when the parameter is emitted as a signed constant."""
    negative = int(param.value) < 0 or (param.minimum is not None and param.minimum < 0)
    return param.unit in SIGNED_UNITS or negative


def _param_literal(param: Param, value: int | bool) -> str:
    if isinstance(value, bool):
        return "(true)" if value else "(false)"
    return f"({int(value)})" if is_signed_param(param) else _int_literal(int(value))


def params_header(groups: list[ParamGroup], source: str) -> str:
    """libs/ls_common/gen/ls_params_gen.h."""
    body = [
        "/*",
        " * Parameter registry of LS-SAIC-001 section 5.0: build-time constants (no calibration",
        " * block in the MVP). <KEY>_MIN and <KEY>_MAX give the calibration range or tolerance",
        " * where the contract defines one. LS_LIM_* are verification limits for tests only.",
        " * Signed quantities (temperatures, encoder counts and count rates, negative ranges) are",
        " * signed constants; every other value carries the u suffix.",
        " */",
    ]
    for group in groups:
        body += ["", f"/** @name {ascii_text(group.title)} (LS-SAIC-001 {group.section}) @{{ */"]
        for p in group.params:
            details = [f"[{p.unit}]", f"owner {'/'.join(p.owner)}", f"stage {p.stage}"]
            if p.cal:
                details.append("calibratable")
            if p.refs:
                details.append(", ".join(p.refs))
            body.append(f"/** {ascii_text(p.desc)}; {', '.join(details)}. */")
            body.append(_define(param_macro(p.key), _param_literal(p, p.value)))
            if p.minimum is not None and p.maximum is not None:
                body.append(_define(f"{param_macro(p.key)}_MIN", _param_literal(p, p.minimum)))
                body.append(_define(f"{param_macro(p.key)}_MAX", _param_literal(p, p.maximum)))
        body.append("/** @} */")
    return _header(
        "ls_params_gen.h",
        source,
        "Parameter registry (LS-SAIC-001 section 5.0).",
        ["<stdbool.h>"],
        body,
    )


# --------------------------------------------------------------------------- DTC codes


def dtc_macro(dtc: Dtc) -> str:
    """C macro of a DTC value (``LS_DTC_B1A11_71``)."""
    return f"LS_DTC_{dtc.symbol}"


def dtc_header(catalogue: Catalogue, source: str) -> str:
    """libs/ls_common/gen/ls_dtc_gen.h: DTC values, notice codes and severities."""
    body = [
        "/*",
        " * DTC values are 24 bits: SAE J2012 two-byte code followed by the failure type byte",
        " * (LS-SAIC-001 section 13.1). Notice codes below 0x010000 are non-DTC notices (8.9).",
        " */",
        "",
        "/** @name DTC status byte (ISO 14229-1) @{ */",
        _define("LS_DTC_STATUS_TEST_FAILED", "(0x01u)"),
        _define("LS_DTC_STATUS_PENDING", "(0x04u)"),
        _define("LS_DTC_STATUS_CONFIRMED", "(0x08u)"),
        _define("LS_DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR", "(0x20u)"),
        _define("LS_DTC_STATUS_AVAILABILITY_MASK", f"(0x{catalogue.availability_mask:02X}u)"),
        "/** @} */",
    ]
    for node in ("DCU", "CGW"):
        body += ["", f"/** @name {node} DTCs: value and severity (FaultSeverity) @{{ */"]
        for dtc in (d for d in catalogue.dtcs if d.node == node):
            body.append(
                _define(
                    dtc_macro(dtc), _int_literal(dtc.value, 6), f"{dtc.code} {ascii_text(dtc.name)}"
                )
            )
            body.append(
                _define(f"{dtc_macro(dtc)}_SEVERITY", enum_constant("FaultSeverity", dtc.severity))
            )
        body.append("/** @} */")
    body += ["", "/** @name Notices: code and severity (FaultSeverity) @{ */"]
    for notice in catalogue.notices:
        name = f"LS_NOTICE_{notice.name}"
        body.append(_define(name, _int_literal(notice.code, 4), ascii_text(notice.meaning)))
        body.append(_define(f"{name}_SEVERITY", enum_constant("FaultSeverity", notice.severity)))
    body.append("/** @} */")
    return _header(
        "ls_dtc_gen.h",
        source,
        "DTC and notice codes (LS-SAIC-001 sections 8.9 and 13).",
        ['"ls_enums_gen.h"'],
        body,
    )


_STAGE_INDEX = {"A": 0, "B": 1, "C": 2, "D": 3, "E": 4, "LATER": 5}
_INHIBIT = {
    "none": "DEM_INHIBIT_NONE",
    "window": "DEM_INHIBIT_WINDOW",
    "window_dir": "DEM_INHIBIT_WINDOW_DIR",
    "lock": "DEM_INHIBIT_LOCK",
    "window_lock": "DEM_INHIBIT_WINDOW_LOCK",
    "safe": "DEM_INHIBIT_SAFE",
}
_RELEASE = {
    None: "DEM_RELEASE_NONE",
    "heal": "DEM_RELEASE_HEAL",
    "timeout": "DEM_RELEASE_TIMEOUT",
    "test_after": "DEM_RELEASE_TEST_AFTER",
    "clear": "DEM_RELEASE_CLEAR",
}


def dem_table(catalogue: Catalogue, source: str, base_name: str) -> tuple[str, str]:
    """firmware/dcu/gen/dem_dtc_gen.{h,c}: DCU DTC configuration table (stage A values)."""
    dcu = [d for d in catalogue.dtcs if d.node == "DCU"]
    body = [
        "/*",
        " * DCU DTC configuration (LS-SAIC-001 sections 6.3 and 13.2). Severity and inhibit are",
        " * the stage A values; entries of later stages keep their IDs stable.",
        " */",
        "",
        "/** @brief Index of a DTC in Dem_DtcCfg. */",
        "typedef uint8_t Dem_DtcIdType;",
    ]
    for index, dtc in enumerate(dcu):
        body.append(
            _define(f"DEM_DTC_ID_{dtc.symbol}", f"((Dem_DtcIdType){index}u)", ascii_text(dtc.name))
        )
    body += [
        _define("DEM_DTC_ID_COUNT", f"({len(dcu)}u)", "number of DCU DTCs"),
        "",
        "/** @brief Function inhibited while the DTC is active (LS-SAIC-001 section 6.3). */",
        "typedef uint8_t Dem_InhibitTargetType;",
    ]
    for index, (target, macro) in enumerate(_INHIBIT.items()):
        body.append(_define(macro, f"((Dem_InhibitTargetType){index}u)", target))
    body += [
        "",
        "/** @brief Release condition of the inhibit. */",
        "typedef uint8_t Dem_InhibitReleaseType;",
    ]
    for index, macro in enumerate(_RELEASE.values()):
        body.append(_define(macro, f"((Dem_InhibitReleaseType){index}u)"))
    body += ["", "/** @brief Window stage that introduces the DTC (LS-SAIC-001 section 0.4). */"]
    for stage, index in _STAGE_INDEX.items():
        body.append(_define(f"DEM_STAGE_{stage}", f"({index}u)"))
    body += [
        "",
        "/** @brief Configuration of one DCU DTC. */",
        "typedef struct",
        "{",
        "    uint32_t dtc;                         /**< 24-bit DTC value (LS_DTC_*) */",
        "    uint32_t releaseMs;                   /**< timeout or test_after time, 0 otherwise */",
        "    Ls_FaultSeverityType severity;        /**< severity (stage A) */",
        "    Dem_InhibitTargetType inhibit;        /**< inhibited function (stage A) */",
        "    Dem_InhibitReleaseType release;       /**< inhibit release condition */",
        "    uint8_t stage;                        /**< DEM_STAGE_* that introduces the DTC */",
        "    bool criticalAtResetLimit;            /**< CRITICAL once the reset limit is reached */",
        "} Dem_DtcCfgType;",
        "",
        "/** @brief DTC configuration table, indexed by Dem_DtcIdType. */",
        "extern const Dem_DtcCfgType Dem_DtcCfg[DEM_DTC_ID_COUNT];",
    ]
    header_name = f"{base_name}.h"
    header = _header(
        header_name,
        source,
        "DCU DTC configuration table (LS-SAIC-001 section 13.2).",
        ["<stdbool.h>", "<stdint.h>", '"ls_enums_gen.h"', '"ls_dtc_gen.h"', '"ls_params_gen.h"'],
        body,
    )
    rows = []
    for dtc in dcu:
        inhibit = dtc.inhibit
        assert inhibit is not None
        if isinstance(inhibit.time, str):
            release_ms = param_macro(inhibit.time)
        else:
            release_ms = f"{inhibit.time or 0}u"
        rows += [
            f"    /* {dtc.code} {ascii_text(dtc.name)} */",
            "    {",
            f"        .dtc = {dtc_macro(dtc)},",
            f"        .releaseMs = {release_ms},",
            f"        .severity = {enum_constant('FaultSeverity', dtc.severity)},",
            f"        .inhibit = {_INHIBIT[inhibit.target]},",
            f"        .release = {_RELEASE[inhibit.release]},",
            f"        .stage = DEM_STAGE_{dtc.stage},",
            f"        .criticalAtResetLimit = {'true' if dtc.critical_at_reset_limit else 'false'},",
            "    },",
        ]
    source_text = "\n".join(
        [
            banner(source, "c").rstrip("\n"),
            SPDX.rstrip("\n"),
            "",
            "/**",
            f" * @file {base_name}.c",
            " * @brief DCU DTC configuration table (LS-SAIC-001 section 13.2).",
            " */",
            "",
            f'#include "{header_name}"',
            "",
            "const Dem_DtcCfgType Dem_DtcCfg[DEM_DTC_ID_COUNT] =",
            "{",
            *rows,
            "};",
            "",
        ]
    )
    return header, source_text


# --------------------------------------------------------------------------- CAN matrix


def _message_macro(name: str) -> str:
    return "LS_CAN_" + re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name).upper()


def _signal_macro(name: str) -> str:
    return "LS_CAN_SIG_" + re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name).upper()


def can_matrix_header(db: Any, source: str) -> str:
    """libs/ls_common/gen/ls_can_matrix_gen.h: frame attributes of the CAN matrix."""
    version = str(db.version or "")
    major, _, minor = version.partition(".")
    send_types = ["LS_CAN_SEND_CYCLIC", "LS_CAN_SEND_EVENT", "LS_CAN_SEND_CYCLIC_AND_EVENT"]
    e2e_modes = ["LS_CAN_E2E_MODE_NONE", "LS_CAN_E2E_MODE_CYCLIC", "LS_CAN_E2E_MODE_EVENT"]
    db_attributes = db.dbc.attributes if db.dbc else {}
    baud = int(db_attributes["Baudrate"].value) if "Baudrate" in db_attributes else 500000
    body = [
        "/*",
        " * CAN matrix attributes (interfaces/can/locksys.dbc, LS-SAIC-001 section 7.3): identifiers,",
        " * DLC, send type, cycle time, minimum gap, repetitions, E2E profile and RX timeout per",
        " * frame. The DBC attributes are the only source of these values.",
        " */",
        "",
        _define("LS_CAN_MATRIX_VERSION_MAJOR", f"({int(major)}u)"),
        _define("LS_CAN_MATRIX_VERSION_MINOR", f"({int(minor or 0)}u)"),
        _define("LS_CAN_BAUDRATE", f"({baud}u)", "bit/s"),
        "",
        "/** @name Send types (GenMsgSendType) @{ */",
    ]
    body += [_define(name, f"({index}u)") for index, name in enumerate(send_types)]
    body += ["/** @} */", "", "/** @name E2E modes (LsE2eMode) @{ */"]
    body += [_define(name, f"({index}u)") for index, name in enumerate(e2e_modes)]
    body.append("/** @} */")
    for message in sorted(db.messages, key=lambda m: m.frame_id):
        attrs = message.dbc.attributes

        def attr(name: str, attrs: Any = attrs) -> int:
            return int(attrs[name].value) if name in attrs else 0

        prefix = _message_macro(message.name)
        senders = ", ".join(message.senders)
        receivers = sorted({r for s in message.signals for r in s.receivers})
        route = f"{senders} -> {', '.join(receivers) if receivers else 'tester'}"
        body += [
            "",
            f"/** @name {message.name} (0x{message.frame_id:03X}), {route} @{{ */",
            _define(f"{prefix}_ID", f"(0x{message.frame_id:03X}u)"),
            _define(f"{prefix}_DLC", f"({message.length}u)"),
            _define(f"{prefix}_SEND_TYPE", send_types[attr("GenMsgSendType")]),
            _define(f"{prefix}_CYCLE_MS", f"({attr('GenMsgCycleTime')}u)"),
            _define(f"{prefix}_MIN_GAP_MS", f"({attr('GenMsgDelayTime')}u)"),
            _define(f"{prefix}_REPETITIONS", f"({attr('GenMsgNrOfRepetition')}u)"),
            _define(f"{prefix}_E2E_MODE", e2e_modes[attr("LsE2eMode")]),
            _define(f"{prefix}_E2E_DATA_ID", f"(0x{attr('LsE2eDataId'):04X}u)"),
            _define(f"{prefix}_E2E_MAX_DELTA", f"({attr('LsE2eMaxDelta')}u)"),
            _define(f"{prefix}_RX_TIMEOUT_MS", f"({attr('LsRxTimeoutMs')}u)"),
            "/** @} */",
        ]
    starts = []
    for message in sorted(db.messages, key=lambda m: m.frame_id):
        for signal in message.signals:
            raw = signal.dbc.attributes.get("GenSigStartValue") if signal.dbc else None
            if raw is not None and int(raw.value) != 0:
                starts.append(
                    _define(f"{_signal_macro(signal.name)}_START_RAW", _int_literal(int(raw.value)))
                )
    body += [
        "",
        "/** @name Non-zero signal start values (GenSigStartValue, raw) @{ */",
        *starts,
        "/** @} */",
    ]
    return _header(
        "ls_can_matrix_gen.h",
        source,
        "CAN matrix frame attributes (LS-SAIC-001 section 7).",
        [],
        body,
    )


# --------------------------------------------------------------------------- E2E test vectors


def _bytes_init(data: bytes, size: int = 8) -> str:
    padded = data + bytes(size - len(data))
    return "{" + ", ".join(f"0x{b:02X}u" for b in padded) + "}"


def e2e_vectors_header(vectors: dict[str, Any], source: str) -> str:
    """libs/ls_e2e/test/gen/ls_e2e_vectors_gen.h: C form of interfaces/vectors/e2e_v1.json."""
    profile = vectors["profile"]
    messages = vectors["messages"]
    modes = {"Cyclic": "LS_E2E_MODE_CYCLIC", "Event": "LS_E2E_MODE_EVENT"}
    body = [
        f'#define LS_E2E_VEC_CRC_CHECK_INPUT "{bytes.fromhex(vectors["crc"]["check"]["input"]).decode("ascii")}"',
        _define("LS_E2E_VEC_CRC_CHECK_VALUE", f"(0x{vectors['crc']['check']['crc']:02X}u)"),
        _define("LS_E2E_VEC_N_OK_VALID", f"({profile['n_ok_valid']}u)"),
        _define("LS_E2E_VEC_N_ERR_INVALID", f"({profile['n_err_invalid']}u)"),
        "",
        "/** @brief One protected frame of the contract vector table (LS-SAIC-001 section 7.10). */",
        "typedef struct",
        "{",
        "    const char *name;",
        "    uint16_t dataId;",
        "    uint8_t dlc;",
        "    uint8_t counter;",
        "    uint8_t payload[8];  /**< frame with CRC byte and counter bits zero */",
        "    uint8_t expected[8]; /**< frame after protection */",
        "} LsE2eVec_FrameType;",
        "",
        "static const LsE2eVec_FrameType LsE2eVec_Frames[] =",
        "{",
    ]
    for frame in vectors["frames"]:
        dlc = messages[frame["message"]]["dlc"]
        body.append(
            f'    {{"{frame["name"]}", 0x{frame["data_id"]:04X}u, {dlc}u, {frame["counter"]}u, '
            f"{_bytes_init(bytes.fromhex(frame['payload']))}, {_bytes_init(bytes.fromhex(frame['bytes']))}}},"
        )
    body += [
        "};",
        _define("LS_E2E_VEC_NUM_FRAMES", f"({len(vectors['frames'])}u)"),
        "",
        "/** @brief One receiver step: a received frame or an RX timeout. */",
        "typedef struct",
        "{",
        "    bool isTimeout;",
        "    uint8_t len;",
        "    uint8_t bytes[8];",
        "    LsE2e_CheckStatusType status;",
        "    LsE2e_StateType state;",
        "    bool delivered;",
        "} LsE2eVec_StepType;",
        "",
    ]
    statuses = {s: f"LS_E2E_STATUS_{s}" for s in ("OK", "CRC_ERROR", "REPEATED", "WRONG_SEQUENCE")}
    names = []
    for sequence in vectors["rx_sequences"]:
        array = f"LsE2eVec_Steps_{sequence['name']}"
        names.append(array)
        body.append(f"static const LsE2eVec_StepType {array}[] =")
        body.append("{")
        for step in sequence["steps"]:
            state = f"LS_E2E_STATE_{step['state']}"
            if step["event"] == "timeout":
                body.append(
                    f"    {{true, 0u, {_bytes_init(b'')}, LS_E2E_STATUS_OK, {state}, false}},"
                )
            else:
                data = bytes.fromhex(step["bytes"])
                delivered = "true" if step["delivered"] else "false"
                body.append(
                    f"    {{false, {len(data)}u, {_bytes_init(data)}, {statuses[step['status']]}, {state}, {delivered}}},"
                )
        body += ["};", ""]
    body += [
        "/** @brief Receiver scenario with its message configuration and final counters. */",
        "typedef struct",
        "{",
        "    const char *name;",
        "    LsE2e_ConfigType config;",
        "    const LsE2eVec_StepType *steps;",
        "    uint8_t numSteps;",
        "    uint16_t crcErrors;",
        "    uint16_t seqErrors;",
        "    uint16_t repeated;",
        "    uint16_t timeouts;",
        "} LsE2eVec_SequenceType;",
        "",
        "static const LsE2eVec_SequenceType LsE2eVec_Sequences[] =",
        "{",
    ]
    for sequence, array in zip(vectors["rx_sequences"], names, strict=True):
        msg = messages[sequence["message"]]
        counters = sequence["counters"]
        body += [
            "    {",
            f'        "{sequence["name"]}",',
            f"        {{0x{msg['data_id']:04X}u, {msg['dlc']}u, {modes[msg['mode']]}, {msg['max_delta']}u, "
            f"LS_E2E_VEC_N_OK_VALID, LS_E2E_VEC_N_ERR_INVALID}},",
            f"        {array},",
            f"        (uint8_t)(sizeof({array}) / sizeof({array}[0])),",
            f"        {counters['crc']}u, {counters['sequence']}u, {counters['repeated']}u, {counters['timeout']}u,",
            "    },",
        ]
    body += ["};", _define("LS_E2E_VEC_NUM_SEQUENCES", f"({len(names)}u)")]
    return _header(
        "ls_e2e_vectors_gen.h",
        source,
        "E2E test vectors for the ls_e2e unit tests (interfaces/vectors/e2e_v1.json).",
        ["<stdbool.h>", "<stdint.h>", '"ls_e2e/ls_e2e.h"'],
        body,
    )
