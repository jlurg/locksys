# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Regenerate every output derived from interfaces/ (LS-SAIC-001 section 1.5).

Single entry point of the LockSys code generators. The output manifest is
interfaces/can/codegen.yaml. Before generating, the sources are validated: DBC ASCII check,
enumeration cross-check (YAML, DBC VAL_ tables, proto), contract comparison with
docs/02_system/LS-SAIC.md and recomputation of the shared test vectors.

Outputs: C enumerations, parameters, DTC codes and CAN matrix attributes (libs/ls_common/gen),
cantools C per node (firmware/dcu/gen, firmware/cgw/components/cgw_com/gen), the DCU DTC table
(firmware/dcu/gen), nanopb C (firmware/cgw/components/cgw_proto/gen, vendored generator),
Python protobuf stubs and constants (hil/src/locksys_hil/gen), the E2E vector header of the
ls_e2e unit tests and, when protoc-gen-dart is available, Dart protobuf code and constants
(app/packages/locksys_protocol/lib/src/gen).

Usage:
    uv run tools/codegen/regen.py            # regenerate and write
    uv run tools/codegen/regen.py --check    # fail (exit 1) when committed outputs drift
"""

from __future__ import annotations

import argparse
import difflib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Callable
from pathlib import Path
from types import ModuleType
from typing import Any

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.codegen import (
    check_ascii,
    check_contract,
    check_enums,
    emit_c,
    emit_dart,
    emit_py,
    saic,
    vectors,
)
from tools.codegen.common import (
    MANIFEST,
    REPO_ROOT,
    CodegenError,
    banner,
    is_generated,
    load_yaml,
    write_text,
)
from tools.codegen.fetch_protoc import FetchError, ensure_protoc
from tools.codegen.model import (
    all_params,
    load_dtcs,
    load_enums,
    load_manifest,
    load_params,
)

Outputs = dict[str, str]

_CTIME = re.compile(
    r" [A-Z][a-z]{2} [A-Z][a-z]{2} [ 0-9][0-9] [0-9]{2}:[0-9]{2}:[0-9]{2} [0-9]{4}\."
)


class Context:
    """Validated sources and tool locations shared by the generators."""

    def __init__(self, manifest: dict[str, Any], repo_root: Path) -> None:
        self.manifest = manifest
        self.root = repo_root
        self.params = load_params(repo_root / manifest["params"]["source"])
        self.enums = load_enums(repo_root / manifest["enums"]["source"])
        self.catalogue = load_dtcs(repo_root / manifest["dtc"]["source"], all_params(self.params))
        self.protoc: Path | None = None
        self.dart_plugin: Path | None = None

    def src(self, section: str, key: str = "source") -> str:
        """Repository-relative source path of a manifest section."""
        return str(self.manifest[section][key])


# --------------------------------------------------------------------------- helpers


def _run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise CodegenError(
            f"{' '.join(command)} failed ({result.returncode}):\n{result.stdout}{result.stderr}"
        )


def _with_banner(text: str, source: str, style: str, tool: str) -> str:
    return banner(source, style, tool) + text


#: Inserted before the first #include of a cantools source whose node does not use every
#: static inline pack/unpack helper (clang reports those under -Wall as unused functions).
_UNUSED_FUNCTION_PRAGMA = (
    "#if defined(__GNUC__)\n"
    "/* cantools emits every shift helper; the pruned node matrix does not use all of them. */\n"
    '#pragma GCC diagnostic ignored "-Wunused-function"\n'
    "#endif\n\n"
)


def _suppress_unused_function(source: str) -> str:
    index = source.find("#include")
    if index < 0:
        raise CodegenError("cantools source without #include: cannot insert the diagnostic pragma")
    return source[:index] + _UNUSED_FUNCTION_PRAGMA + source[index:]


def _proto_paths(ctx: Context) -> tuple[Path, Path]:
    proto = ctx.root / ctx.src("proto")
    options = ctx.root / ctx.src("proto", "options")
    if options.parent != proto.parent or options.stem != proto.stem:
        raise CodegenError(
            f"{ctx.src('proto', 'options')}: must sit beside the proto with the same stem"
        )
    return proto, proto.parent


# --------------------------------------------------------------------------- generators


def gen_constants(ctx: Context) -> Outputs:
    """C headers of libs/ls_common/gen, the DCU Dem table and the HIL Python constants."""
    import cantools

    m = ctx.manifest
    out: Outputs = {
        m["enums"]["c_header"]: emit_c.enums_header(ctx.enums, ctx.src("enums")),
        m["params"]["c_header"]: emit_c.params_header(ctx.params, ctx.src("params")),
        m["dtc"]["c_header"]: emit_c.dtc_header(ctx.catalogue, ctx.src("dtc")),
        m["enums"]["python"]: emit_py.enums_module(ctx.enums, ctx.src("enums")),
        m["params"]["python"]: emit_py.params_module(ctx.params, ctx.src("params")),
        m["dtc"]["python"]: emit_py.dtc_module(ctx.catalogue, ctx.src("dtc")),
    }
    base = m["dtc"]["dcu_table"]
    header, source = emit_c.dem_table(ctx.catalogue, ctx.src("dtc"), Path(base).name)
    out[f"{base}.h"] = header
    out[f"{base}.c"] = source
    db = cantools.database.load_file(str(ctx.root / m["can"]["dbc"]), strict=True)
    out[m["can"]["matrix_header"]] = emit_c.can_matrix_header(db, m["can"]["dbc"])
    vectors_doc = json.loads((ctx.root / m["vectors"]["e2e"]).read_text(encoding="utf-8"))
    out[m["vectors"]["e2e_c_header"]] = emit_c.e2e_vectors_header(vectors_doc, m["vectors"]["e2e"])
    init = str(Path(m["proto"]["python"]["output_dir"]) / "__init__.py")
    out[init] = emit_py.package_init(m["proto"]["python"]["output_dir"])
    return out


def gen_can(ctx: Context) -> Outputs:
    """cantools C pack/unpack code per node."""
    import cantools
    from cantools.database.can.c_source import generate

    can = ctx.manifest["can"]
    if cantools.__version__ != can["cantools_version"]:
        raise CodegenError(
            f"cantools {cantools.__version__} installed, {can['cantools_version']} pinned"
        )
    out: Outputs = {}
    tool = f"cantools {cantools.__version__}"
    for target in can["nodes"]:
        db = cantools.database.load_file(
            str(ctx.root / can["dbc"]), prune_choices=bool(can["prune_choices"]), strict=True
        )
        assert isinstance(db, cantools.database.can.Database)
        excluded = set(can.get("exclude_messages", []))
        db.messages[:] = [msg for msg in db.messages if msg.name not in excluded]
        name = target["database_name"]
        header, source, _, _ = generate(
            db,
            name,
            f"{name}.h",
            f"{name}.c",
            f"{name}_fuzzer.c",
            bool(can["floating_point_numbers"]),
            bool(can["bit_fields"]),
            False,
            target["node"],
            False,
        )
        if target.get("suppress_unused_function", False):
            source = _suppress_unused_function(source)
        for suffix, text in ((".h", header), (".c", source)):
            text = _CTIME.sub(".", text, count=1)
            path = f"{target['output_dir']}/{name}{suffix}"
            out[path] = _with_banner(text, can["dbc"], "c", tool)
    return out


def gen_nanopb(ctx: Context) -> Outputs:
    """nanopb C code with the vendored generator and the pinned protoc."""
    assert ctx.protoc is not None
    proto, proto_dir = _proto_paths(ctx)
    nanopb = ctx.manifest["proto"]["nanopb"]
    generator = ctx.root / nanopb["generator"]
    version = re.search(r"nanopb-(\d+\.\d+\.\d+(?:\.\d+)?)", nanopb["generator"])
    tool = f"nanopb {version.group(1) if version else '?'}, protoc {ctx.manifest['proto']['protoc_version']}"
    out: Outputs = {}
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        descriptor = tmp / f"{proto.stem}.pb"
        _run(
            [
                str(ctx.protoc),
                f"-I{proto_dir}",
                "--include_imports",
                "--include_source_info",
                f"-o{descriptor}",
                proto.name,
            ],
            cwd=proto_dir,
        )
        (tmp / "pb2").mkdir()
        (tmp / "out").mkdir()
        env = dict(os.environ)
        env["PATH"] = f"{ctx.protoc.parent}{os.pathsep}{env.get('PATH', '')}"
        env["NANOPB_PB2_TEMP_DIR"] = str(tmp / "pb2")
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        _run(
            [
                sys.executable,
                str(generator),
                "-q",
                "--error-on-unmatched",
                "-D",
                str(tmp / "out"),
                "-I",
                str(proto_dir),
                str(descriptor),
            ],
            cwd=tmp,
            env=env,
        )
        for produced in sorted((tmp / "out").iterdir()):
            text = produced.read_text(encoding="utf-8")
            out[f"{nanopb['output_dir']}/{produced.name}"] = _with_banner(
                text, ctx.src("proto"), "c", tool
            )
    return out


def gen_python_proto(ctx: Context) -> Outputs:
    """Python protobuf module and type stub for the HIL."""
    assert ctx.protoc is not None
    proto, proto_dir = _proto_paths(ctx)
    target = ctx.manifest["proto"]["python"]["output_dir"]
    tool = f"protoc {ctx.manifest['proto']['protoc_version']}"
    out: Outputs = {}
    with tempfile.TemporaryDirectory() as tmp:
        _run(
            [
                str(ctx.protoc),
                f"-I{proto_dir}",
                f"--python_out={tmp}",
                f"--pyi_out={tmp}",
                proto.name,
            ],
            cwd=proto_dir,
        )
        for produced in sorted(Path(tmp).iterdir()):
            text = produced.read_text(encoding="utf-8")
            out[f"{target}/{produced.name}"] = _with_banner(text, ctx.src("proto"), "hash", tool)
    return out


def gen_dart(ctx: Context) -> Outputs:
    """Dart protobuf code and constants (only when protoc-gen-dart is available)."""
    assert ctx.protoc is not None
    assert ctx.dart_plugin is not None
    proto, proto_dir = _proto_paths(ctx)
    m = ctx.manifest
    target = m["proto"]["dart"]["output_dir"]
    tool = f"protoc_plugin {m['proto']['dart']['protoc_plugin_version']}, protoc {m['proto']['protoc_version']}"
    out: Outputs = {
        m["params"]["dart"]: emit_dart.params_library(ctx.params, ctx.src("params")),
        m["dtc"]["dart"]: emit_dart.dtc_library(ctx.catalogue, ctx.src("dtc")),
    }
    with tempfile.TemporaryDirectory() as tmp:
        _run(
            [
                str(ctx.protoc),
                f"-I{proto_dir}",
                f"--plugin=protoc-gen-dart={ctx.dart_plugin}",
                f"--dart_out={tmp}",
                proto.name,
            ],
            cwd=proto_dir,
        )
        for produced in sorted(Path(tmp).iterdir()):
            text = produced.read_text(encoding="utf-8")
            out[f"{target}/{produced.name}"] = _with_banner(text, ctx.src("proto"), "dart", tool)
    return out


# --------------------------------------------------------------------------- checks


def source_problems(ctx: Context, use_contract: bool) -> list[str]:
    """Validate the interface sources before anything is generated."""
    problems = [
        p
        for path in [ctx.root / ctx.manifest["can"]["dbc"]]
        for p in check_ascii.problems(path, strict=True)
    ]
    problems += check_enums.check(ctx.root)
    doc = None
    contract = ctx.root / ctx.manifest["contract"]["document"]
    if use_contract and contract.is_file():
        problems += check_contract.check(ctx.root)
        doc = saic.load(ctx.root)
    elif use_contract:
        print(
            f"warning: {ctx.manifest['contract']['document']} not found; contract comparison skipped"
        )
    problems += vectors.check_files(ctx.root, doc)
    return problems


def _load_module(path: Path, name: str) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise CodegenError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def runtime_problems(ctx: Context, outputs: Outputs) -> list[str]:
    """Encode the APP session vectors with the generated Python stubs and the protobuf runtime."""
    target = ctx.manifest["proto"]["python"]["output_dir"]
    proto_name = Path(ctx.src("proto")).stem
    module_rel = f"{target}/{proto_name}_pb2.py"
    module_name = f"_locksys_regen_{proto_name}_pb2"
    if module_name in sys.modules:
        pb2 = sys.modules[module_name]
    else:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / f"{proto_name}_pb2.py"
            path.write_text(outputs[module_rel], encoding="utf-8")
            pb2 = _load_module(path, module_name)
            sys.modules[module_name] = pb2
    session = json.loads(
        (ctx.root / ctx.manifest["vectors"]["app_session"]).read_text(encoding="utf-8")
    )
    return [
        f"{ctx.manifest['vectors']['app_session']}: {p}"
        for p in vectors.protobuf_problems(session, pb2)
    ]


# --------------------------------------------------------------------------- Dart plugin


def find_dart_plugin(explicit: str | None) -> Path | None:
    """Locate protoc-gen-dart: --dart-plugin, LS_PROTOC_GEN_DART, PATH, then the pub cache.

    A plugin found on PATH or in the pub cache is a ``dart pub global activate`` launcher that
    needs the Dart SDK, so it is used only when ``dart`` is on PATH as well.
    """
    for candidate in (explicit, os.environ.get("LS_PROTOC_GEN_DART")):
        if candidate:
            path = Path(candidate)
            if not path.is_file():
                raise CodegenError(f"protoc-gen-dart not found at {path}")
            return path
    if shutil.which("dart") is None:
        return None
    found = shutil.which("protoc-gen-dart")
    if found:
        return Path(found)
    pub_cache = Path(os.environ.get("PUB_CACHE", Path.home() / ".pub-cache"))
    for name in ("protoc-gen-dart", "protoc-gen-dart.bat"):
        path = pub_cache / "bin" / name
        if path.is_file():
            return path
    return None


def dart_plugin_version(plugin: Path) -> str | None:
    """protoc_plugin version recorded by ``dart pub global activate`` (None when unknown)."""
    lock = plugin.parent.parent / "global_packages" / "protoc_plugin" / "pubspec.lock"
    if not lock.is_file():
        return None
    data = load_yaml(lock) or {}
    entry = (data.get("packages") or {}).get("protoc_plugin") or {}
    version = entry.get("version")
    return str(version) if version else None


# --------------------------------------------------------------------------- compare and write


def managed_dirs(ctx: Context, with_dart: bool) -> list[str]:
    """Directories whose regen.py-generated files are owned by this tool."""
    m = ctx.manifest
    dirs = {
        str(Path(m["enums"]["c_header"]).parent),
        str(Path(m["dtc"]["dcu_table"]).parent),
        str(Path(m["vectors"]["e2e_c_header"]).parent),
        m["proto"]["nanopb"]["output_dir"],
        m["proto"]["python"]["output_dir"],
        *(target["output_dir"] for target in m["can"]["nodes"]),
    }
    if with_dart:
        dirs.add(m["proto"]["dart"]["output_dir"])
    return sorted(dirs)


def compare(
    root: Path, outputs: Outputs, dirs: list[str]
) -> tuple[list[str], list[str], list[str]]:
    """Return (changed, missing, stale) repository-relative paths."""
    changed, missing = [], []
    for rel_path, text in sorted(outputs.items()):
        path = root / rel_path
        if not path.is_file():
            missing.append(rel_path)
        elif path.read_bytes() != text.encode("utf-8"):
            changed.append(rel_path)
    stale = []
    for directory in dirs:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            relative = path.relative_to(root).as_posix()
            if path.is_file() and relative not in outputs and is_generated(path):
                stale.append(relative)
    return changed, missing, stale


def _diff(root: Path, rel_path: str, text: str, limit: int = 40) -> str:
    old = (root / rel_path).read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
    lines = list(
        difflib.unified_diff(old, text.splitlines(keepends=True), f"a/{rel_path}", f"b/{rel_path}")
    )
    if len(lines) > limit:
        lines = [*lines[:limit], f"... ({len(lines) - limit} more diff lines)\n"]
    return "".join(lines)


def macro_collisions(outputs: Outputs) -> list[str]:
    """Macros defined by more than one generated C header."""
    owner: dict[str, str] = {}
    problems = []
    for path, text in sorted(outputs.items()):
        if not path.endswith(".h"):
            continue
        for name in re.findall(r"^#define\s+([A-Za-z_]\w*)", text, re.M):
            if owner.setdefault(name, path) != path:
                problems.append(f"macro {name} defined in {owner[name]} and {path}")
    return problems


def generate_all(ctx: Context, with_dart: bool) -> Outputs:
    """Run every generator and return path -> content."""
    steps: list[Callable[[Context], Outputs]] = [
        gen_constants,
        gen_can,
        gen_nanopb,
        gen_python_proto,
    ]
    if with_dart:
        steps.append(gen_dart)
    outputs: Outputs = {}
    for step in steps:
        produced = step(ctx)
        overlap = outputs.keys() & produced.keys()
        if overlap:
            raise CodegenError(f"two generators write {sorted(overlap)}")
        outputs.update(produced)
    collisions = macro_collisions(outputs)
    if collisions:
        raise CodegenError("; ".join(collisions))
    return outputs


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(
        description="Regenerate every output derived from interfaces/."
    )
    parser.add_argument("--check", action="store_true", help="fail when committed outputs differ")
    parser.add_argument("--no-dart", action="store_true", help="skip the Dart outputs")
    parser.add_argument("--dart-plugin", help="path of protoc-gen-dart (protoc_plugin)")
    parser.add_argument(
        "--no-contract",
        action="store_true",
        help="skip the comparison with LS-SAIC-001 (local iteration only; CI never skips it)",
    )
    args = parser.parse_args(argv)
    if args.check and args.no_contract:
        parser.error("--no-contract cannot be combined with --check")
    try:
        ctx = Context(load_manifest(REPO_ROOT / MANIFEST), REPO_ROOT)
        problems = source_problems(ctx, use_contract=not args.no_contract)
        if problems:
            for problem in problems:
                print(f"error: {problem}", file=sys.stderr)
            print(f"regen: {len(problems)} source problem(s); nothing generated", file=sys.stderr)
            return 1
        print("regen: sources consistent (DBC ASCII, enumerations, contract, vectors)")
        ctx.protoc = ensure_protoc()
        with_dart = False
        if not args.no_dart:
            ctx.dart_plugin = find_dart_plugin(args.dart_plugin)
            with_dart = ctx.dart_plugin is not None
        if with_dart:
            assert ctx.dart_plugin is not None
            pinned = ctx.manifest["proto"]["dart"]["protoc_plugin_version"]
            found = dart_plugin_version(ctx.dart_plugin)
            if found is not None and found != pinned:
                raise CodegenError(
                    f"protoc_plugin {found} found at {ctx.dart_plugin}, {pinned} pinned"
                )
        else:
            print(
                "regen: Dart outputs skipped (dart or protoc-gen-dart from protoc_plugin "
                f"{ctx.manifest['proto']['dart']['protoc_plugin_version']} not found); "
                f"{ctx.manifest['proto']['dart']['output_dir']} is not generated or checked"
            )
        outputs = generate_all(ctx, with_dart)
        problems = runtime_problems(ctx, outputs)
        if problems:
            for problem in problems:
                print(f"error: {problem}", file=sys.stderr)
            return 1
    except (CodegenError, FetchError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    changed, missing, stale = compare(REPO_ROOT, outputs, managed_dirs(ctx, with_dart))
    if args.check:
        for rel_path in changed:
            print(f"drift: {rel_path} differs from the regenerated output", file=sys.stderr)
            print(_diff(REPO_ROOT, rel_path, outputs[rel_path]), file=sys.stderr)
        for rel_path in missing:
            print(f"drift: {rel_path} is missing", file=sys.stderr)
        for rel_path in stale:
            print(f"drift: {rel_path} is no longer generated", file=sys.stderr)
        if changed or missing or stale:
            print(
                "regen --check: outputs drift; run uv run tools/codegen/regen.py", file=sys.stderr
            )
            return 1
        print(f"regen --check: {len(outputs)} generated files up to date")
        return 0
    for rel_path in changed + missing:
        write_text(REPO_ROOT / rel_path, outputs[rel_path])
        print(f"wrote {rel_path}")
    for rel_path in stale:
        (REPO_ROOT / rel_path).unlink()
        print(f"removed {rel_path}")
    print(
        f"regen: {len(outputs)} files, {len(changed) + len(missing)} written, {len(stale)} removed"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
