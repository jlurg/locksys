# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Verify the ROM CRC-32 of a DCU image as the STM32F1 CRC unit computes it.

The CRC unit computes CRC-32/MPEG-2 (polynomial 0x04C11DB7, initial value 0xFFFFFFFF, no
reflection, no final XOR) over 32-bit words fed most significant bit first. Flash words are
little-endian, so the byte stream is processed in the order byte 3, 2, 1, 0 of each word. This
equals ``ielftool --fill "0xFF;<range>" --checksum "ls_rom_crc:4,crc32:Li,0xFFFFFFFF;<range>"``.

Inputs: Intel HEX (.hex), ELF (.elf, .out; PT_LOAD segments at their physical addresses) or a
raw binary (.bin, placed at --base). Bytes of the range that the image does not define count
as 0xFF.

Exit status: 0 = the stored CRC matches (or --compute/--write succeeded), 1 = mismatch,
2 = input error.
"""

from __future__ import annotations

import argparse
import struct
import sys
from collections.abc import Iterable, Mapping, Sequence
from pathlib import Path

APP_START = 0x08000000
APP_END = 0x0801EFFB
CRC_ADDR = 0x0801EFFC
POLY = 0x04C11DB7
INIT = 0xFFFFFFFF
FILL = 0xFF

ELF_MAGIC = b"\x7fELF"
PT_LOAD = 1


class ImageError(Exception):
    """The image cannot be read."""


def _crc_table() -> tuple[int, ...]:
    table = []
    for byte in range(256):
        crc = byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ POLY) if crc & 0x80000000 else (crc << 1)
        table.append(crc & 0xFFFFFFFF)
    return tuple(table)


_TABLE = _crc_table()


def crc32_mpeg2(data: bytes, crc: int = INIT) -> int:
    """Return CRC-32/MPEG-2 of a byte stream, continuing from ``crc``."""
    for byte in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ _TABLE[((crc >> 24) ^ byte) & 0xFF]
    return crc


def crc32_stm32_words(data: bytes) -> int:
    """Return the CRC of little-endian 32-bit words as the STM32 CRC unit computes it."""
    if len(data) % 4:
        raise ValueError("data length must be a multiple of 4")
    swapped = bytearray()
    for offset in range(0, len(data), 4):
        swapped += data[offset : offset + 4][::-1]
    return crc32_mpeg2(bytes(swapped))


def parse_ihex(text: str) -> dict[int, int]:
    """Parse Intel HEX into a sparse address-to-byte map."""
    memory: dict[int, int] = {}
    base = 0
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ImageError(f"line {number}: missing ':'")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as exc:
            raise ImageError(f"line {number}: {exc}") from exc
        if len(record) < 5 or len(record) != record[0] + 5:
            raise ImageError(f"line {number}: bad record length")
        if sum(record) & 0xFF:
            raise ImageError(f"line {number}: checksum error")
        count, address, kind = record[0], (record[1] << 8) | record[2], record[3]
        payload = record[4 : 4 + count]
        if kind == 0x00:
            for index, value in enumerate(payload):
                memory[base + address + index] = value
        elif kind == 0x01:
            break
        elif kind == 0x02:
            base = int.from_bytes(payload, "big") << 4
        elif kind == 0x04:
            base = int.from_bytes(payload, "big") << 16
        elif kind in (0x03, 0x05):
            continue
        else:
            raise ImageError(f"line {number}: unknown record type {kind:#04x}")
    return memory


def parse_elf(data: bytes) -> dict[int, int]:
    """Read the PT_LOAD segments of a little-endian ELF32 file at their physical addresses."""
    if data[:4] != ELF_MAGIC or len(data) < 52:
        raise ImageError("not an ELF file")
    if data[4] != 1 or data[5] != 1:
        raise ImageError("only little-endian ELF32 is supported")
    phoff = struct.unpack_from("<I", data, 28)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    memory: dict[int, int] = {}
    for index in range(phnum):
        offset = phoff + index * phentsize
        if offset + 32 > len(data):
            raise ImageError("truncated program header table")
        p_type, p_offset, _vaddr, p_paddr, p_filesz = struct.unpack_from("<IIIII", data, offset)
        if p_type != PT_LOAD or p_filesz == 0:
            continue
        segment = data[p_offset : p_offset + p_filesz]
        if len(segment) != p_filesz:
            raise ImageError("truncated segment")
        for pos, value in enumerate(segment):
            memory[p_paddr + pos] = value
    return memory


def load_image(path: Path, base: int = APP_START) -> dict[int, int]:
    """Load an image file into a sparse address-to-byte map."""
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise ImageError(str(exc)) from exc
    suffix = path.suffix.lower()
    if suffix == ".hex":
        return parse_ihex(data.decode("ascii", errors="replace"))
    if suffix in (".elf", ".out", ".axf") or data[:4] == ELF_MAGIC:
        return parse_elf(data)
    if suffix == ".bin":
        return {base + index: value for index, value in enumerate(data)}
    raise ImageError(f"unsupported image type: {path.name}")


def region(memory: Mapping[int, int], start: int, end: int) -> bytes:
    """Return the bytes of [start, end] with undefined bytes as 0xFF."""
    return bytes(memory.get(address, FILL) for address in range(start, end + 1))


def stored_crc(memory: Mapping[int, int], address: int) -> int | None:
    """Return the little-endian CRC word at ``address``, or None when it is not in the image."""
    if not all((address + i) in memory for i in range(4)):
        return None
    return int.from_bytes(bytes(memory[address + i] for i in range(4)), "little")


def to_ihex(memory: Mapping[int, int], record_size: int = 16) -> str:
    """Serialise a sparse map as Intel HEX with extended linear address records."""
    lines: list[str] = []

    def emit(kind: int, address: int, payload: bytes) -> None:
        record = bytes([len(payload), (address >> 8) & 0xFF, address & 0xFF, kind]) + payload
        checksum = (-sum(record)) & 0xFF
        lines.append(":" + (record + bytes([checksum])).hex().upper())

    upper: int | None = None
    addresses = sorted(memory)
    index = 0
    while index < len(addresses):
        start = addresses[index]
        chunk = bytearray([memory[start]])
        index += 1
        while (
            index < len(addresses)
            and addresses[index] == start + len(chunk)
            and len(chunk) < record_size
            and (addresses[index] & 0xFFFF) != 0
        ):
            chunk.append(memory[addresses[index]])
            index += 1
        if upper != start >> 16:
            upper = start >> 16
            emit(0x04, 0, upper.to_bytes(2, "big"))
        emit(0x00, start & 0xFFFF, bytes(chunk))
    emit(0x01, 0, b"")
    return "\n".join(lines) + "\n"


def with_crc(
    memory: Mapping[int, int], start: int, end: int, crc_addr: int, value: int
) -> dict[int, int]:
    """Return a copy of the image with the range filled and ``value`` at ``crc_addr``."""
    patched = dict(memory)
    for address in range(start, end + 1):
        patched.setdefault(address, FILL)
    for offset, byte in enumerate(value.to_bytes(4, "little")):
        patched[crc_addr + offset] = byte
    return patched


def _int(text: str) -> int:
    return int(text, 0)


def _parse_args(argv: Iterable[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("image", type=Path, help="image: .hex, .elf/.out or .bin")
    parser.add_argument("--start", type=_int, default=APP_START, help="first address of the range")
    parser.add_argument("--end", type=_int, default=APP_END, help="last address of the range")
    parser.add_argument("--crc-addr", type=_int, default=CRC_ADDR, help="address of the CRC word")
    parser.add_argument("--base", type=_int, default=APP_START, help="load address of a .bin")
    parser.add_argument("--compute", action="store_true", help="print the CRC only")
    parser.add_argument("--write", type=Path, help="write an Intel HEX copy with the CRC set")
    parser.add_argument(
        "--bad", action="store_true", help="with --write: store the complement of the CRC"
    )
    return parser.parse_args(list(argv) if argv is not None else None)


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    args = _parse_args(argv)
    if args.end < args.start or (args.end - args.start + 1) % 4:
        print("error: the range must hold a whole number of words", file=sys.stderr)
        return 2
    if args.start <= args.crc_addr <= args.end:
        print("error: the CRC word must lie outside the range", file=sys.stderr)
        return 2
    try:
        memory = load_image(args.image, args.base)
    except ImageError as exc:
        print(f"error: {args.image}: {exc}", file=sys.stderr)
        return 2
    computed = crc32_stm32_words(region(memory, args.start, args.end))
    if args.compute:
        print(f"0x{computed:08X}")
        return 0
    if args.write is not None:
        value = computed ^ 0xFFFFFFFF if args.bad else computed
        patched = with_crc(memory, args.start, args.end, args.crc_addr, value)
        args.write.write_text(to_ihex(patched), encoding="ascii")
        print(f"{args.write}: ROM CRC 0x{value:08X} at 0x{args.crc_addr:08X}")
        return 0
    stored = stored_crc(memory, args.crc_addr)
    if stored is None:
        print(f"FAIL {args.image}: no CRC word at 0x{args.crc_addr:08X}")
        return 1
    if stored != computed:
        print(f"FAIL {args.image}: stored 0x{stored:08X}, computed 0x{computed:08X}")
        return 1
    print(f"OK {args.image}: ROM CRC 0x{computed:08X}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
