# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Check the include rules of the DCU layered architecture.

Rules (LS-DCU-SAD-001 section 3.2), configured in ``tools/arch/layers.yaml``:

* LAY001 every source file belongs to a configured group;
* LAY002 every quoted include resolves to a file;
* LAY003 a group includes only the groups and modules it is allowed to;
* LAY004 register headers (CMSIS) only in the register-header groups;
* LAY005 ``*_priv.h`` only from its own directory;
* LAY006 ``extern`` object declarations only in the allowed groups and files;
* LAY007 a generated Visual State system header only in its own SWC, at most one per file.

Exit status: 0 = no violation, 1 = violations, 2 = configuration error.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = Path(__file__).resolve().parent / "layers.yaml"

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
EXTERN_OBJECT_RE = re.compile(r'^\s*extern\s+(?!"C")(?!const\b)[^;(]*;')
SOURCE_SUFFIXES = (".c", ".h")


class ConfigError(Exception):
    """Invalid rule configuration."""


@dataclass(frozen=True)
class Group:
    """A named set of files selected by a path pattern."""

    name: str
    pattern: re.Pattern[str]


@dataclass(frozen=True)
class Location:
    """Group and module of a file."""

    group: str
    module: str | None


@dataclass(frozen=True)
class Violation:
    """One rule violation."""

    path: str
    line: int
    rule: str
    message: str

    def __str__(self) -> str:
        return f"{self.path}:{self.line}: {self.rule}: {self.message}"


def glob_to_regex(glob: str) -> re.Pattern[str]:
    """Translate a group glob (``**``, ``*`` and ``{module}``) into a regular expression."""
    out: list[str] = []
    i = 0
    while i < len(glob):
        if glob.startswith("{module}", i):
            out.append("(?P<module>[^/]+)")
            i += len("{module}")
        elif glob.startswith("**", i):
            out.append(".*")
            i += 2
        elif glob[i] == "*":
            out.append("[^/]*")
            i += 1
        else:
            out.append(re.escape(glob[i]))
            i += 1
    return re.compile("^" + "".join(out) + "$")


@dataclass(frozen=True)
class Rules:
    """Parsed rule configuration."""

    sources: tuple[str, ...]
    include_roots: tuple[str, ...]
    std_headers: frozenset[str]
    groups: tuple[Group, ...]
    allow: Mapping[str, tuple[str, ...]]
    register_groups: frozenset[str]
    private_suffix: str
    extern_groups: frozenset[str]
    extern_files: frozenset[str]
    vs_systems: Mapping[str, str]

    def locate(self, rel: str) -> Location | None:
        """Return the group and module of a repository-relative path."""
        for group in self.groups:
            match = group.pattern.match(rel)
            if match:
                module = match.groupdict().get("module")
                return Location(group.name, module)
        return None

    def allows(self, source: Location, target: Location) -> bool:
        """Return whether ``source`` may include ``target``."""
        for spec in self.allow.get(source.group, ()):
            name, _, modules = spec.partition(":")
            if name != target.group:
                continue
            if not modules:
                return True
            wanted = {m.strip() for m in modules.split(",")}
            if "own" in wanted and target.module == source.module:
                return True
            if target.module in wanted:
                return True
        return False


def _str_list(value: Any, key: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
        raise ConfigError(f"'{key}' must be a list of strings")
    return tuple(value)


def load_rules(path: Path) -> Rules:
    """Load and validate the rule configuration."""
    try:
        data = yaml.safe_load(path.read_text(encoding="utf-8"))
    except (OSError, yaml.YAMLError) as exc:
        raise ConfigError(f"{path}: {exc}") from exc
    if not isinstance(data, dict) or data.get("version") != 1:
        raise ConfigError(f"{path}: expected a mapping with 'version: 1'")
    groups: list[Group] = []
    for entry in data.get("groups", []):
        if not isinstance(entry, dict) or "name" not in entry or "glob" not in entry:
            raise ConfigError("each group needs 'name' and 'glob'")
        groups.append(Group(str(entry["name"]), glob_to_regex(str(entry["glob"]))))
    names = {g.name for g in groups}
    allow_raw = data.get("allow", {})
    if not isinstance(allow_raw, dict):
        raise ConfigError("'allow' must be a mapping")
    allow: dict[str, tuple[str, ...]] = {}
    for group_name, specs in allow_raw.items():
        if group_name not in names:
            raise ConfigError(f"'allow' names unknown group '{group_name}'")
        allow[group_name] = _str_list(specs, f"allow.{group_name}")
        for spec in allow[group_name]:
            if spec.partition(":")[0] not in names:
                raise ConfigError(f"'allow.{group_name}' names unknown group in '{spec}'")
    vs_systems = data.get("vs_systems", {})
    if not isinstance(vs_systems, dict):
        raise ConfigError("'vs_systems' must be a mapping")
    return Rules(
        sources=_str_list(data.get("sources", []), "sources"),
        include_roots=_str_list(data.get("include_roots", []), "include_roots"),
        std_headers=frozenset(_str_list(data.get("std_headers", []), "std_headers")),
        groups=tuple(groups),
        allow=allow,
        register_groups=frozenset(
            _str_list(data.get("register_header_groups", []), "register_header_groups")
        ),
        private_suffix=str(data.get("private_header_suffix", "_priv.h")),
        extern_groups=frozenset(
            _str_list(data.get("extern_object_groups", []), "extern_object_groups")
        ),
        extern_files=frozenset(
            _str_list(data.get("extern_object_files", []), "extern_object_files")
        ),
        vs_systems={str(k): str(v) for k, v in vs_systems.items()},
    )


def iter_sources(root: Path, rules: Rules) -> Iterable[Path]:
    """Yield the C sources and headers below the configured source directories."""
    for source in rules.sources:
        base = root / source
        if not base.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(d for d in dirnames if d != "build")
            for name in sorted(filenames):
                if name.endswith(SOURCE_SUFFIXES):
                    yield Path(dirpath) / name


def resolve(root: Path, including: Path, name: str, rules: Rules) -> Path | None:
    """Resolve an include like the compiler: the including directory, then the roots."""
    candidates = [including.parent / name] + [root / r / name for r in rules.include_roots]
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    return None


def _rel(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def check_file(root: Path, path: Path, rules: Rules) -> list[Violation]:
    """Check one file against every rule."""
    rel = _rel(root, path)
    source = rules.locate(rel)
    if source is None:
        return [Violation(rel, 1, "LAY001", "file belongs to no configured group")]
    violations: list[Violation] = []
    vs_headers: list[str] = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for number, line in enumerate(lines, start=1):
        if (
            EXTERN_OBJECT_RE.match(line)
            and source.group not in rules.extern_groups
            and rel not in rules.extern_files
        ):
            violations.append(
                Violation(rel, number, "LAY006", "extern object declaration outside the RTE")
            )
        match = INCLUDE_RE.match(line)
        if not match:
            continue
        bracket, name = match.groups()
        if bracket == "<" and name in rules.std_headers:
            continue
        target_path = resolve(root, path, name, rules)
        if target_path is None:
            if bracket == '"':
                violations.append(Violation(rel, number, "LAY002", f'unresolved include "{name}"'))
            continue
        target_rel = _rel(root, target_path)
        target = rules.locate(target_rel)
        if target is None:
            violations.append(
                Violation(rel, number, "LAY003", f"{target_rel} belongs to no configured group")
            )
            continue
        if target.group == "cmsis" and source.group not in rules.register_groups:
            violations.append(
                Violation(rel, number, "LAY004", f"register header {name} outside platform/MCAL")
            )
            continue
        if name.endswith(rules.private_suffix) and target_path.parent != path.resolve().parent:
            violations.append(
                Violation(rel, number, "LAY005", f"{name} included outside its module")
            )
            continue
        if target.group == "gen_vs":
            system = Path(name).stem
            owner = rules.vs_systems.get(system)
            if owner is not None:
                vs_headers.append(system)
                if source.group != "swc" or source.module != owner:
                    violations.append(
                        Violation(rel, number, "LAY007", f"{name} included outside SWC {owner}")
                    )
                    continue
        if not rules.allows(source, target):
            where = f"{target.group}:{target.module}" if target.module else target.group
            what = f"{source.group}:{source.module}" if source.module else source.group
            violations.append(
                Violation(rel, number, "LAY003", f"{what} may not include {where} ({name})")
            )
    if len(vs_headers) > 1:
        violations.append(
            Violation(
                rel,
                1,
                "LAY007",
                "more than one Visual State system header: " + ", ".join(vs_headers),
            )
        )
    return violations


def check_tree(root: Path, rules: Rules) -> list[Violation]:
    """Check every source file of the configured directories."""
    violations: list[Violation] = []
    for path in iter_sources(root, rules):
        violations.extend(check_file(root, path, rules))
    return violations


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG, help="rule file")
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    args = parser.parse_args(argv)
    try:
        rules = load_rules(args.config)
    except ConfigError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    violations = check_tree(args.root, rules)
    annotate = os.environ.get("GITHUB_ACTIONS") == "true"
    for violation in violations:
        print(violation)
        if annotate:
            print(
                f"::error file={violation.path},line={violation.line},"
                f"title={violation.rule}::{violation.message}"
            )
    count = sum(1 for _ in iter_sources(args.root, rules))
    print(f"check_layers: {count} file(s), {len(violations)} violation(s)")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
