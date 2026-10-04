# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""``hil`` command-line interface.

Exit status: 0 = success, 1 = check failed, 2 = usage or configuration error,
10 = bench infrastructure error.
"""

import argparse
import signal
import sys
import threading
from collections.abc import Sequence
from pathlib import Path

from locksys_hil.bench.bench import Bench
from locksys_hil.bench.lock import BenchLock
from locksys_hil.config.loader import bench_schema, load_bench_config, resolve_bench_path
from locksys_hil.config.models import BenchConfig
from locksys_hil.errors import INFRASTRUCTURE_EXIT_CODE, BenchInfrastructureError, ConfigError
from locksys_hil.protocols.telemetry import TelemetryMonitor
from locksys_hil.security.redact import scan_for_secrets


def _config(args: argparse.Namespace) -> BenchConfig:
    path = resolve_bench_path(args.bench)
    if path is None:
        raise ConfigError("no bench configuration (--bench or LOCKSYS_HIL_BENCH)")
    return load_bench_config(path)


def _cmd_config_validate(args: argparse.Namespace) -> int:
    for name in args.files:
        load_bench_config(Path(name))
        print(f"{name}: valid")
    return 0


def _cmd_config_schema(args: argparse.Namespace) -> int:
    schema = bench_schema()
    if args.check:
        current = Path(args.check).read_text(encoding="utf-8")
        if current != schema:
            print(f"{args.check} is out of date; regenerate with 'hil config schema --write'")
            return 1
        return 0
    if args.write:
        Path(args.write).write_text(schema, encoding="utf-8")
        return 0
    sys.stdout.write(schema)
    return 0


def _cmd_safe_state(args: argparse.Namespace) -> int:
    bench = Bench.open(_config(args))
    bench.close()
    print("bench in safe state")
    return 0


def _cmd_bench_status(args: argparse.Namespace) -> int:
    lock = BenchLock(Path(_config(args).lock_file).expanduser())
    owner = lock.owner()
    print(f"locked by {owner}" if owner else "free")
    return 0


def _cmd_bench_hold(args: argparse.Namespace) -> int:
    stop = threading.Event()
    with BenchLock(Path(_config(args).lock_file).expanduser()):
        signal.signal(signal.SIGINT, lambda *_: stop.set())
        print("bench lock held; press Ctrl+C to release")
        stop.wait()
    return 0


def _cmd_scan_secrets(args: argparse.Namespace) -> int:
    import keyring

    config = _config(args)
    names = config.secrets
    secrets: list[bytes] = []
    for entry in (names.k_pair, names.wifi_passphrase):
        value = keyring.get_password(names.keyring_service, entry)
        if value:
            secrets.append(bytes.fromhex(value) if entry == names.k_pair else value.encode())
    files = [p for p in Path(args.directory).rglob("*") if p.is_file()]
    hits = scan_for_secrets(files, secrets)
    for path in hits:
        print(f"secret material in {path}")
    return 1 if hits else 0


def _cmd_telemetry_check(args: argparse.Namespace) -> int:
    monitor = TelemetryMonitor()
    text = Path(args.file).read_text(encoding="ascii", errors="replace")
    issues = monitor.feed_all(line for line in text.splitlines() if line.strip())
    for issue in issues:
        print(issue)
    print(f"{len(monitor.sentences)} sentences, {len(issues)} issues")
    return 1 if issues else 0


def build_parser() -> argparse.ArgumentParser:
    """Return the argument parser."""
    parser = argparse.ArgumentParser(prog="hil", description="LockSys HIL bench tool")
    parser.add_argument("--bench", help="bench configuration YAML (default: $LOCKSYS_HIL_BENCH)")
    sub = parser.add_subparsers(dest="command", required=True)

    config = sub.add_parser("config", help="bench configuration").add_subparsers(
        dest="config_command", required=True
    )
    validate = config.add_parser("validate", help="validate bench YAML files")
    validate.add_argument("files", nargs="+")
    validate.set_defaults(func=_cmd_config_validate)
    schema = config.add_parser("schema", help="print, write or check the JSON schema")
    group = schema.add_mutually_exclusive_group()
    group.add_argument("--write", metavar="PATH")
    group.add_argument("--check", metavar="PATH")
    schema.set_defaults(func=_cmd_config_schema)

    sub.add_parser("safe-state", help="bring the bench to its safe state").set_defaults(
        func=_cmd_safe_state
    )

    bench = sub.add_parser("bench", help="bench lock").add_subparsers(
        dest="bench_command", required=True
    )
    bench.add_parser("status", help="show the lock holder").set_defaults(func=_cmd_bench_status)
    bench.add_parser("hold", help="hold the lock until interrupted").set_defaults(
        func=_cmd_bench_hold
    )

    evidence = sub.add_parser("evidence", help="evidence handling").add_subparsers(
        dest="evidence_command", required=True
    )
    scan = evidence.add_parser("scan-secrets", help="search evidence for key material")
    scan.add_argument("directory")
    scan.set_defaults(func=_cmd_scan_secrets)

    telemetry = sub.add_parser("telemetry", help="UART telemetry").add_subparsers(
        dest="telemetry_command", required=True
    )
    check = telemetry.add_parser("check", help="check a captured telemetry log")
    check.add_argument("file")
    check.set_defaults(func=_cmd_telemetry_check)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Run the CLI and return the exit status."""
    args = build_parser().parse_args(argv)
    try:
        return int(args.func(args))
    except BenchInfrastructureError as exc:
        print(f"bench infrastructure error: {exc}", file=sys.stderr)
        return INFRASTRUCTURE_EXIT_CODE
    except (ConfigError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
