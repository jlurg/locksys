# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Fetch the pinned protoc release for the host into tools/codegen/.protoc/.

The archive is downloaded from the protocolbuffers/protobuf GitHub release and verified
against the SHA-256 pin below before it is unpacked. protoc 33.x matches the Python protobuf
6.33.x runtime of the uv workspace (generated code must not be newer than the runtime).

Usage:
    uv run tools/codegen/fetch_protoc.py            # fetch if missing, print the protoc path
    uv run tools/codegen/fetch_protoc.py --check    # verify the pins against tools/versions.env

The environment variable LS_PROTOC may name an existing protoc binary instead; its reported
version must equal the pinned version.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

PROTOC_VERSION = "33.6"

#: Release asset suffix -> SHA-256 of protoc-<version>-<suffix>.zip (GitHub release digests).
PROTOC_SHA256 = {
    "linux-x86_64": "9c49962391ff8b5754342509efc3885db91dfe11a358e1e98c9a71518f0ad199",
    "linux-aarch_64": "4016cd2ad24c6a2405332a2ef7e04b719240e40d811587c6a1c4bbe514cd668c",
    "osx-aarch_64": "606aca137976af9090847025dcc8fc0daceb5cbbc5a46819296118fc53f0b870",
    "osx-x86_64": "13a2c245613ed7eca4a8c1a779545f0f2762f2735b2f7d7f7d6fe2618440ae2d",
    "win64": "96a83b8ee942f1046f079c43657cc09fd41f37a4e349ddb0e8ba5064c48bb5ff",
}

#: tools/versions.env key of each asset pin.
VERSIONS_ENV_KEYS = {
    "linux-x86_64": "PROTOC_SHA256_LINUX_X86_64",
    "linux-aarch_64": "PROTOC_SHA256_LINUX_AARCH_64",
    "osx-aarch_64": "PROTOC_SHA256_OSX_AARCH_64",
    "osx-x86_64": "PROTOC_SHA256_OSX_X86_64",
    "win64": "PROTOC_SHA256_WIN64",
}

URL = "https://github.com/protocolbuffers/protobuf/releases/download/v{version}/protoc-{version}-{asset}.zip"
CACHE_DIR = Path(__file__).resolve().parent / ".protoc"
REPO_ROOT = Path(__file__).resolve().parents[2]


class FetchError(Exception):
    """protoc cannot be provided."""


def host_asset() -> str:
    """Release asset suffix for the running host."""
    system = platform.system()
    machine = platform.machine().lower()
    arm = machine in ("arm64", "aarch64")
    x86 = machine in ("x86_64", "amd64")
    if system == "Linux" and (arm or x86):
        return "linux-aarch_64" if arm else "linux-x86_64"
    if system == "Darwin" and (arm or x86):
        return "osx-aarch_64" if arm else "osx-x86_64"
    if system == "Windows" and x86:
        return "win64"
    raise FetchError(f"no pinned protoc build for {system}/{machine}")


def read_versions_env(path: Path | None = None) -> dict[str, str]:
    """Parse tools/versions.env (KEY=VALUE lines)."""
    path = path or REPO_ROOT / "tools" / "versions.env"
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def check_pins(env: dict[str, str]) -> list[str]:
    """Return mismatches between the pin table and tools/versions.env."""
    problems = []
    if env.get("PROTOC") != PROTOC_VERSION:
        problems.append(f"PROTOC={env.get('PROTOC')} but fetch_protoc.py pins {PROTOC_VERSION}")
    for asset, key in VERSIONS_ENV_KEYS.items():
        if env.get(key) != PROTOC_SHA256[asset]:
            problems.append(f"{key}={env.get(key)} but fetch_protoc.py pins {PROTOC_SHA256[asset]}")
    return problems


def _binary_name() -> str:
    return "protoc.exe" if platform.system() == "Windows" else "protoc"


def protoc_version(binary: Path) -> str:
    """Return the version reported by ``protoc --version`` (``libprotoc 33.6`` -> ``33.6``)."""
    try:
        result = subprocess.run(
            [str(binary), "--version"], capture_output=True, text=True, check=True, timeout=60
        )
    except (OSError, subprocess.SubprocessError) as exc:
        raise FetchError(f"{binary}: cannot run: {exc}") from exc
    return result.stdout.strip().rsplit(" ", 1)[-1]


def _download(url: str, target: Path) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": "locksys-fetch-protoc"})
    try:
        with urllib.request.urlopen(request, timeout=120) as response, target.open("wb") as out:
            shutil.copyfileobj(response, out)
    except (urllib.error.URLError, OSError) as exc:
        raise FetchError(f"download failed: {url}: {exc}") from exc


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_protoc(cache_dir: Path = CACHE_DIR) -> Path:
    """Return the path of the pinned protoc binary, downloading it when needed."""
    override = os.environ.get("LS_PROTOC")
    if override:
        binary = Path(override)
        version = protoc_version(binary)
        if version != PROTOC_VERSION:
            raise FetchError(f"LS_PROTOC={binary} reports {version}, pinned {PROTOC_VERSION}")
        return binary
    asset = host_asset()
    install = cache_dir / f"{PROTOC_VERSION}-{asset}"
    binary = install / "bin" / _binary_name()
    if binary.is_file() and protoc_version(binary) == PROTOC_VERSION:
        return binary
    cache_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=cache_dir) as tmp:
        archive = Path(tmp) / "protoc.zip"
        _download(URL.format(version=PROTOC_VERSION, asset=asset), archive)
        actual = _sha256(archive)
        if actual != PROTOC_SHA256[asset]:
            raise FetchError(
                f"protoc-{PROTOC_VERSION}-{asset}.zip: SHA-256 {actual} != pin {PROTOC_SHA256[asset]}"
            )
        staging = Path(tmp) / "unpacked"
        with zipfile.ZipFile(archive) as bundle:
            for member in bundle.namelist():
                destination = (staging / member).resolve()
                if not destination.is_relative_to(staging.resolve()):
                    raise FetchError(f"unsafe path in archive: {member}")
            bundle.extractall(staging)
        (staging / "bin" / _binary_name()).chmod(0o755)
        if install.exists():
            shutil.rmtree(install)
        staging.rename(install)
    version = protoc_version(binary)
    if version != PROTOC_VERSION:
        raise FetchError(f"{binary} reports {version}, pinned {PROTOC_VERSION}")
    return binary


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0] if __doc__ else None)
    parser.add_argument(
        "--check", action="store_true", help="only compare the pins with tools/versions.env"
    )
    args = parser.parse_args(argv)
    problems = check_pins(read_versions_env())
    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if problems or args.check:
        return 1 if problems else 0
    try:
        print(ensure_protoc())
    except FetchError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
