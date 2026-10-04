# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Shared paths and model access of the Visual State checks (LS-DCU-GDE-001).

The model files are XML. The element and attribute names used to find events, actions,
variables, constants and transitions are collected in ``MODEL_SCHEMA``; they follow Visual State
11.x and are confirmed against the first committed model (Lab Host check D9).
"""

from __future__ import annotations

import hashlib
import xml.etree.ElementTree as ET
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DCU_DIR = Path("firmware/dcu")
MODEL_DIR = DCU_DIR / "model/visualstate"
GEN_DIR = DCU_DIR / "gen_vs"
MANIFEST = GEN_DIR / "VS_MANIFEST.json"
VARIANTS = ("release", "debug")
PARAMS_HEADER = Path("libs/ls_common/gen/ls_params_gen.h")
ENUMS_HEADER = Path("libs/ls_common/gen/ls_enums_gen.h")

MODEL_SUFFIXES = (".vnw", ".vsp", ".vsr", ".vssm", ".vste", ".stereotypes")
XML_SUFFIXES = (".vsp", ".vsr", ".vssm")
GEN_SUFFIXES = (".c", ".h")
GEN_IGNORED = ("README.md",)

MODEL_SCHEMA: dict[str, tuple[str, ...]] = {
    "event": ("event",),
    "action": ("actionfunction", "action_function", "actionfunc"),
    "variable": ("variable", "externalvariable", "internalvariable"),
    "constant": ("constant",),
    "transition": ("transition",),
    "parameter": ("parameter", "argument"),
    "enumeration": ("enumeration", "enumtype", "enum"),
    "literal": ("literal", "enumerator", "enumvalue"),
}
NAME_ATTRIBUTES = ("name", "Name")
VALUE_ATTRIBUTES = ("value", "Value", "initialvalue", "InitialValue")
ACTION_ATTRIBUTES = ("action", "Action", "actions", "Actions")
BUILTIN_EVENTS = frozenset({"SE_RESET", "SE_NOP"})


def normalised_sha256(path: Path) -> str:
    """Return the SHA-256 of a file with CRLF line endings converted to LF."""
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def aggregate_sha256(files: dict[str, str]) -> str:
    """Return one SHA-256 over sorted ``path`` and ``hash`` pairs."""
    digest = hashlib.sha256()
    for rel in sorted(files):
        digest.update(f"{rel}\0{files[rel]}\n".encode())
    return digest.hexdigest()


def model_files(root: Path) -> list[Path]:
    """Return the model files and coder option files, sorted."""
    base = root / MODEL_DIR
    if not base.is_dir():
        return []
    found = [
        p
        for p in base.rglob("*")
        if p.is_file()
        and (p.suffix in MODEL_SUFFIXES or (p.suffix == ".opt" and p.parent.name == "options"))
        and "reports" not in p.relative_to(base).parts
    ]
    return sorted(found)


def project_files(root: Path) -> list[Path]:
    """Return the Visual State project files (.vsp)."""
    base = root / MODEL_DIR
    return sorted(base.glob("*.vsp")) if base.is_dir() else []


def generated_files(root: Path, variant: str) -> list[Path]:
    """Return the generated sources of one variant, sorted."""
    base = root / GEN_DIR / variant
    if not base.is_dir():
        return []
    return sorted(p for p in base.rglob("*") if p.is_file() and p.name not in GEN_IGNORED)


def local_tag(element: ET.Element) -> str:
    """Return the lower-case tag of an element without its namespace."""
    return element.tag.rsplit("}", 1)[-1].lower()


def attribute(element: ET.Element, names: tuple[str, ...]) -> str | None:
    """Return the first present attribute of ``names``."""
    for name in names:
        value = element.get(name)
        if value is not None:
            return value
    return None


@dataclass(frozen=True)
class ModelElement:
    """A named model element."""

    kind: str
    name: str
    element: ET.Element
    source: Path


def iter_model_elements(paths: list[Path]) -> Iterator[ModelElement]:
    """Yield the named elements of the model files whose tag maps to a schema kind."""
    kinds = {tag: kind for kind, tags in MODEL_SCHEMA.items() for tag in tags}
    for path in paths:
        if path.suffix not in XML_SUFFIXES:
            continue
        tree = ET.parse(path)
        for element in tree.iter():
            kind = kinds.get(local_tag(element))
            name = attribute(element, NAME_ATTRIBUTES)
            if kind is not None and name is not None:
                yield ModelElement(kind, name, element, path)
