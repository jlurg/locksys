# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/romcrc/verify_rom_crc.py."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from tools.romcrc import verify_rom_crc as rc

START = 0x08000000
END = 0x0800000F
CRC_ADDR = 0x08000010


def _image(words: list[int]) -> dict[int, int]:
    data = b"".join(w.to_bytes(4, "little") for w in words)
    return {START + i: b for i, b in enumerate(data)}


def test_crc32_mpeg2_check_value() -> None:
    assert rc.crc32_mpeg2(b"123456789") == 0x0376E6E7


@pytest.mark.verifies("SWR-DCU-074")
def test_stm32_word_crc_matches_crc_unit_reference() -> None:
    # Reference: CRC unit after reset fed with the single word 0x12345678.
    assert rc.crc32_stm32_words((0x12345678).to_bytes(4, "little")) == 0xDF8A8A2B


def test_stm32_word_crc_rejects_partial_words() -> None:
    with pytest.raises(ValueError, match="multiple of 4"):
        rc.crc32_stm32_words(b"\x00\x01\x02")


def test_region_fills_gaps_with_ff() -> None:
    assert rc.region({START: 0x11, START + 2: 0x33}, START, START + 3) == b"\x11\xff\x33\xff"


def test_ihex_round_trip_across_64k_boundary() -> None:
    memory = {0x0800FFFE + i: i for i in range(4)} | {0x08020000: 0xAA}
    assert rc.parse_ihex(rc.to_ihex(memory)) == memory


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("00000001FF\n", "missing ':'"),
        (":0Z\n", "line 1"),
        (":0100000001FF\n", "checksum"),
        (":02000000\n", "bad record length"),
        (":00000006FA\n", "unknown record type"),
    ],
)
def test_parse_ihex_rejects_malformed_input(text: str, message: str) -> None:
    with pytest.raises(rc.ImageError, match=message):
        rc.parse_ihex(text)


def test_parse_ihex_segment_address_and_start_records() -> None:
    text = ":020000021000EC\n:0100000042BD\n:0400000300000000F9\n:00000001FF\n"
    assert rc.parse_ihex(text) == {0x10000: 0x42}


def _elf(segments: list[tuple[int, int, bytes]]) -> bytes:
    """Build a minimal little-endian ELF32 with (type, paddr, data) program headers."""
    phoff = 52
    header = bytearray(52)
    header[0:4] = rc.ELF_MAGIC
    header[4] = 1
    header[5] = 1
    struct.pack_into("<I", header, 28, phoff)
    struct.pack_into("<HH", header, 42, 32, len(segments))
    body = bytearray()
    phdrs = bytearray()
    data_offset = phoff + 32 * len(segments)
    for p_type, paddr, data in segments:
        phdrs += struct.pack(
            "<IIIIIIII", p_type, data_offset + len(body), paddr, paddr, len(data), len(data), 5, 4
        )
        body += data
    return bytes(header + phdrs + body)


def test_parse_elf_reads_load_segments_at_physical_address() -> None:
    elf = _elf([(rc.PT_LOAD, START, b"\x01\x02"), (2, 0x1000, b"\x09"), (rc.PT_LOAD, 0, b"")])
    assert rc.parse_elf(elf) == {START: 1, START + 1: 2}


@pytest.mark.parametrize(
    "data",
    [b"nope", rc.ELF_MAGIC + b"\x02\x01" + bytes(46)],
)
def test_parse_elf_rejects_unsupported_files(data: bytes) -> None:
    with pytest.raises(rc.ImageError):
        rc.parse_elf(data)


def test_main_verifies_written_image_and_detects_bad_crc(tmp_path: Path) -> None:
    source = tmp_path / "image.bin"
    source.write_bytes(b"".join(w.to_bytes(4, "little") for w in (1, 2, 3, 4)))
    good = tmp_path / "good.hex"
    bad = tmp_path / "bad.hex"
    common = ["--start", hex(START), "--end", hex(END), "--crc-addr", hex(CRC_ADDR)]
    assert rc.main([str(source), *common, "--write", str(good)]) == 0
    assert rc.main([str(source), *common, "--write", str(bad), "--bad"]) == 0
    assert rc.main([str(good), *common]) == 0
    assert rc.main([str(bad), *common]) == 1
    assert rc.main([str(source), *common]) == 1  # no CRC word in the raw image


def test_main_compute_prints_crc(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = tmp_path / "image.bin"
    source.write_bytes(b"\xff" * 16)
    expected = rc.crc32_stm32_words(b"\xff" * 16)
    assert rc.main([str(source), "--end", hex(END), "--crc-addr", hex(CRC_ADDR), "--compute"]) == 0
    assert capsys.readouterr().out.strip() == f"0x{expected:08X}"


@pytest.mark.parametrize(
    "extra",
    [
        ["--end", hex(START + 2)],
        ["--end", hex(END), "--crc-addr", hex(START + 4)],
    ],
)
def test_main_rejects_invalid_ranges(tmp_path: Path, extra: list[str]) -> None:
    source = tmp_path / "image.bin"
    source.write_bytes(b"\x00" * 4)
    assert rc.main([str(source), *extra]) == 2


def test_main_rejects_unreadable_or_unknown_images(tmp_path: Path) -> None:
    assert rc.main([str(tmp_path / "missing.hex")]) == 2
    unknown = tmp_path / "image.txt"
    unknown.write_text("x", encoding="ascii")
    assert rc.main([str(unknown)]) == 2


def test_with_crc_writes_little_endian_word() -> None:
    patched = rc.with_crc(_image([0, 0, 0, 0]), START, END, CRC_ADDR, 0x11223344)
    assert rc.stored_crc(patched, CRC_ADDR) == 0x11223344
    assert rc.stored_crc({}, CRC_ADDR) is None
