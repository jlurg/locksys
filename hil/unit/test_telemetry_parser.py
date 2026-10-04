# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""UART telemetry parser against ``interfaces/uart/telemetry_v1.md``."""

import re
from pathlib import Path

import pytest

from locksys_hil.errors import ProtocolError
from locksys_hil.protocols.telemetry import (
    Sentence,
    TelemetryMonitor,
    build_line,
    checksum,
    fault_injection_command,
    parse_line,
    parse_speed_x10,
    parse_temp_cdeg,
)


def _examples(root: Path) -> list[str]:
    text = (root / "interfaces" / "uart" / "telemetry_v1.md").read_text(encoding="utf-8")
    section = text.split("## 5. Examples", 1)[1]
    block = re.search(r"```text\n(.*?)```", section, re.DOTALL)
    assert block is not None
    return [line for line in block.group(1).splitlines() if line]


def test_contract_examples_parse(root: Path) -> None:
    lines = _examples(root)
    assert len(lines) == 13
    for line in lines:
        parse_line(line + "\r\n")


def test_decoded_values(root: Path) -> None:
    by_kind: dict[str, list[Sentence]] = {}
    for line in _examples(root):
        sentence = parse_line(line)
        by_kind.setdefault(sentence.kind, []).append(sentence)
    tmp = by_kind["$LSTMP"]
    assert tmp[0].values["temp_cdeg"] == 2345
    assert tmp[0].seq == 42
    assert tmp[0].t_ms == 43012
    assert tmp[1].values["temp_cdeg"] is None
    sta = by_kind["$LSSTA"][1].values
    assert (sta["pos_pct"], sta["kl30_dv"], sta["speed_rpm_x10"]) == (None, 124, 1698)
    mot = by_kind["$LSMOT"][1].values
    assert (mot["pos_counts"], mot["encoder_status"]) == (12401, "NO_MOTION")
    assert by_kind["$LSVER"][0].values["git7"] == "a1b2c3d"
    assert by_kind["$LSDTC"][0].values["dtc"] == 0x9A1171
    assert by_kind["#LOG"][1].values["text"] == "DROPPED 4"
    assert by_kind["!LSFI"][0].values == {"cmd": "HANG", "args": ("500",)}


def test_checksum_and_builder() -> None:
    assert checksum("LSRST,POWER_ON,1") == 0x7A
    assert build_line("$LSRST", ["POWER_ON", "1"]) == "$LSRST,POWER_ON,1*7A"
    assert fault_injection_command("HANG", 500) == "!LSFI,HANG,500*25\r\n"
    with pytest.raises(ValueError, match="reserved character"):
        build_line("!LSFI", ["A,B"])


@pytest.mark.parametrize(
    "line",
    [
        "$LSRST,POWER_ON,1*7B",  # checksum
        "$LSRST,POWER_ON,1*7a",  # lowercase checksum
        "LSRST,POWER_ON,1*7A",  # start character
        "$LSXYZ,1*00",  # unknown type
        "$LSTMP,0042,0000043012,+23.45,VALID",  # no checksum
    ],
)
def test_rejected_lines(line: str) -> None:
    with pytest.raises(ProtocolError):
        parse_line(line)


def test_field_format_errors() -> None:
    for fields in (
        ["0042", "0000043012", "+23.45", "VALID"],
        ["00042", "43012", "+23.45", "VALID"],
        ["00042", "0000043012", "23.45", "VALID"],
        ["00042", "0000043012", "+23.45", "valid"],
    ):
        with pytest.raises(ProtocolError):
            parse_line(build_line("$LSTMP", fields))
    with pytest.raises(ProtocolError, match="fields"):
        parse_line(build_line("$LSTMP", ["00042", "0000043012", "+23.45"]))


def test_length_limit() -> None:
    long_text = "X" * 40
    line = build_line("#LOG", ["00001", "0000000001", "I", "TLM", "0002", long_text])
    parse_line(line)
    with pytest.raises(ProtocolError):
        parse_line(build_line("#LOG", ["00001", "0000000001", "I", "TLM", "0002", long_text + "Y"]))


def test_value_parsers() -> None:
    assert parse_temp_cdeg("-0.05") == -5
    assert parse_temp_cdeg("+125.00") == 12500
    assert parse_speed_x10("-169.8") == -1698
    assert parse_speed_x10("") is None


def _tmp(seq: int, t_ms: int) -> str:
    return build_line("$LSTMP", [f"{seq:05d}", f"{t_ms:010d}", "+23.45", "VALID"])


def _dropped(seq: int, t_ms: int, n: int) -> str:
    return build_line("#LOG", [f"{seq:05d}", f"{t_ms:010d}", "W", "TLM", "0001", f"DROPPED {n}"])


def test_monitor_accepts_continuous_stream() -> None:
    monitor = TelemetryMonitor()
    assert monitor.feed_all([_tmp(65534, 10), _tmp(65535, 20), _tmp(0, 30)]) == []


def test_monitor_gap_needs_dropped_line() -> None:
    lines = [_tmp(1, 10), _tmp(4, 20)]
    assert TelemetryMonitor().feed_all(lines) == ["2 missing lines, only 0 reported as DROPPED"]
    assert TelemetryMonitor().feed_all([*lines, _dropped(7, 21, 2)]) == []


def test_monitor_time_and_repetition() -> None:
    monitor = TelemetryMonitor()
    issues = monitor.feed_all([_tmp(1, 1000), _tmp(1, 900), "garbage"])
    assert any("repeated" in issue for issue in issues)
    assert any("backwards" in issue for issue in issues)
    assert any("grammar" in issue for issue in issues)


def test_monitor_t_ms_wrap() -> None:
    assert TelemetryMonitor().feed_all([_tmp(1, (1 << 32) - 5), _tmp(2, 5)]) == []
