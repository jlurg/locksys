# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Visual State manifest: hashes of the model and of the generated engines.

``--check`` (default) fails when the model changed without regeneration, when generated code
was edited, or when generated code exists without a model. Before the model exists the
manifest is the placeholder (``model_present: false``) and ``gen_vs/`` holds no sources.
``--update`` rewrites the manifest (run by ``Invoke-VsGenerate.ps1`` after generation).
Hashes are taken over content with CRLF converted to LF.

Exit status: 0 = consistent, 1 = drift, 2 = input error.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Any

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.vs import vs_common as vc

SCHEMA_VERSION = 1


def _rel(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def compute(root: Path) -> dict[str, Any]:
    """Return the manifest content of the current tree (without tool data)."""
    model = {_rel(root, p): vc.normalised_sha256(p) for p in vc.model_files(root)}
    gen: dict[str, dict[str, str]] = {}
    for variant in vc.VARIANTS:
        gen[variant] = {
            _rel(root, p): vc.normalised_sha256(p) for p in vc.generated_files(root, variant)
        }
    present = bool(vc.project_files(root))
    return {
        "schema_version": SCHEMA_VERSION,
        "model_present": present,
        "model_sha256": vc.aggregate_sha256(model) if present else None,
        "gen_sha256": {v: (vc.aggregate_sha256(gen[v]) if gen[v] else None) for v in vc.VARIANTS},
        "files": {
            "model": model if present else {},
            "gen": {k: v for d in gen.values() for k, v in d.items()},
        },
    }


def placeholder() -> dict[str, Any]:
    """Return the manifest of a tree without a model."""
    return {
        "schema_version": SCHEMA_VERSION,
        "model_present": False,
        "visual_state_version": None,
        "coder_options": {
            "release": (vc.MODEL_DIR / "options/coder_release.opt").as_posix(),
            "debug": (vc.MODEL_DIR / "options/coder_debug.opt").as_posix(),
        },
        "model_sha256": None,
        "gen_sha256": dict.fromkeys(vc.VARIANTS),
        "files": {"model": {}, "gen": {}},
        "verificator": {},
    }


def load_manifest(path: Path) -> dict[str, Any]:
    """Read the manifest file."""
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"{path}: {exc}") from exc
    if not isinstance(data, dict) or data.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"{path}: unsupported manifest (schema_version {SCHEMA_VERSION} expected)")
    return data


def _diff(kind: str, expected: dict[str, str], actual: dict[str, str]) -> list[str]:
    problems = []
    for rel in sorted(set(expected) | set(actual)):
        if rel not in actual:
            problems.append(f"{kind} file removed: {rel}")
        elif rel not in expected:
            problems.append(f"{kind} file not in the manifest: {rel}")
        elif expected[rel] != actual[rel]:
            problems.append(f"{kind} file changed: {rel}")
    return problems


def check(root: Path, manifest: dict[str, Any]) -> list[str]:
    """Return the differences between the manifest and the tree."""
    current = compute(root)
    files = manifest.get("files", {})
    if not current["model_present"]:
        problems = []
        if manifest.get("model_present"):
            problems.append("manifest lists a model, but no .vsp file exists")
        for rel in current["files"]["gen"]:
            problems.append(f"generated file without a model: {rel}")
        return problems
    if not manifest.get("model_present"):
        return ["a model exists, but the manifest is the placeholder: run Invoke-VsGenerate.ps1"]
    problems = _diff("model", dict(files.get("model", {})), current["files"]["model"])
    problems += _diff("generated", dict(files.get("gen", {})), current["files"]["gen"])
    if not problems:
        if manifest.get("model_sha256") != current["model_sha256"]:
            problems.append("model_sha256 does not match the model files")
        if manifest.get("gen_sha256") != current["gen_sha256"]:
            problems.append("gen_sha256 does not match the generated files")
    for variant in vc.VARIANTS:
        if current["gen_sha256"][variant] is None:
            problems.append(f"no generated code in gen_vs/{variant}")
    return problems


def update(
    root: Path,
    previous: dict[str, Any] | None,
    vs_version: str | None,
    verificator: dict[str, str],
) -> dict[str, Any]:
    """Return a new manifest for the current tree, keeping tool data not given."""
    if not vc.project_files(root):
        return placeholder()
    data = placeholder()
    data.update(compute(root))
    old = previous or {}
    data["visual_state_version"] = vs_version or old.get("visual_state_version")
    merged = dict(old.get("verificator", {}))
    merged.update(verificator)
    data["verificator"] = merged
    return data


def _write(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def _parse_verificator(items: Sequence[str]) -> dict[str, str]:
    result = {}
    for item in items:
        system, sep, summary = item.partition("=")
        if not sep or not system:
            raise ValueError(f"--verificator expects SYSTEM=SUMMARY, got '{item}'")
        result[system] = summary
    return result


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--check", action="store_true", help="compare the manifest with the tree (default)"
    )
    mode.add_argument("--update", action="store_true", help="rewrite the manifest")
    parser.add_argument("--root", type=Path, default=vc.REPO_ROOT, help="repository root")
    parser.add_argument("--vs-version", help="Visual State version used for generation (--update)")
    parser.add_argument(
        "--verificator",
        action="append",
        default=[],
        metavar="SYSTEM=SUMMARY",
        help="Verificator result per system (--update, repeatable)",
    )
    args = parser.parse_args(argv)
    path = args.root / vc.MANIFEST
    try:
        if args.update:
            previous = load_manifest(path) if path.is_file() else None
            data = update(
                args.root, previous, args.vs_version, _parse_verificator(args.verificator)
            )
            _write(path, data)
            print(
                f"vs_manifest: wrote {vc.MANIFEST.as_posix()} (model_present={data['model_present']})"
            )
            return 0
        if not path.is_file():
            print(f"vs_manifest: {vc.MANIFEST.as_posix()} is missing", file=sys.stderr)
            return 1
        manifest = load_manifest(path)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    problems = check(args.root, manifest)
    for problem in problems:
        print(f"vs_manifest: {problem}")
    if problems:
        return 1
    state = "model and generated code match" if manifest.get("model_present") else "no model yet"
    print(f"vs_manifest: OK ({state})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
