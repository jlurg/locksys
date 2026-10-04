# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""pytest hooks and fixtures of the HIL framework (LS-HIL-001 section 9).

* Registers the marker catalogue (the repository runs pytest with ``--strict-markers``).
* Writes ``test_id`` and ``verifies`` as JUnit properties and, with ``--hil-trace-json``, as
  ``trace.json`` for ``tools/trace/trace.py``.
* Collection check: catalogue tests need exactly one ``test_id`` and at least one ``verifies``;
  every identifier must have a valid format and, when the registry is available, exist in it.
* ``bench`` fixture: skipped without a bench configuration; otherwise opens the bench (lock,
  drivers, self-test) and brings it to the safe state at the end of the session.
* Infrastructure errors end the session with exit code 10.
"""

from collections.abc import Generator, Iterator
from pathlib import Path

import pytest

from locksys_hil.bench.bench import Bench
from locksys_hil.config.loader import BENCH_ENV, load_bench_config, resolve_bench_path
from locksys_hil.errors import INFRASTRUCTURE_EXIT_CODE, BenchInfrastructureError, ConfigError
from locksys_hil.pytest_plugin.markers import (
    CATALOGUE_MARKERS,
    ini_lines,
    marker_args,
)
from locksys_hil.reporting.traceability import (
    TEST_ID_RE,
    TraceRecord,
    is_valid_id,
    load_registry,
    write_trace_json,
)

TOPOLOGY_CAPABILITIES: dict[str, frozenset[str]] = {
    "T1": frozenset({"dcu", "can"}),
    "T2": frozenset({"cgw", "can", "wifi"}),
    "T3": frozenset({"dcu", "cgw", "can", "wifi"}),
}
"""Capabilities each integration topology needs (LS-HIL-001 section 4)."""

_INFRA_ERRORS = pytest.StashKey[list[str]]()
_RECORDS = pytest.StashKey[dict[str, TraceRecord]]()


def pytest_addoption(parser: pytest.Parser) -> None:
    """Add the HIL options."""
    group = parser.getgroup("locksys-hil", "LockSys HIL framework")
    group.addoption(
        "--hil-bench",
        default=None,
        help=f"bench configuration YAML (default: ${BENCH_ENV}); without it bench tests skip",
    )
    group.addoption(
        "--hil-registry",
        default="auto",
        help="docs directory for the identifier registry; 'auto' = repository docs/, 'none' = off",
    )
    group.addoption(
        "--hil-trace-json", default=None, help="write test identifiers and outcomes to this file"
    )


def pytest_configure(config: pytest.Config) -> None:
    """Register markers and initialise the session state."""
    for line in ini_lines():
        config.addinivalue_line("markers", line)
    config.stash[_INFRA_ERRORS] = []
    config.stash[_RECORDS] = {}


def _registry(config: pytest.Config) -> frozenset[str] | None:
    option = str(config.getoption("--hil-registry"))
    if option == "none":
        return None
    if option != "auto":
        return load_registry(Path(option))
    from locksys_hil.paths import repo_root

    try:
        docs = repo_root() / "docs"
    except ConfigError:
        return None
    return load_registry(docs) if docs.is_dir() else None


def _check_item(item: pytest.Item, registry: frozenset[str] | None) -> list[str]:
    problems: list[str] = []
    test_ids = marker_args(item, "test_id")
    verifies = marker_args(item, "verifies")
    if any(item.get_closest_marker(m) is not None for m in CATALOGUE_MARKERS):
        if len(test_ids) != 1:
            problems.append(f"needs exactly one test_id, has {len(test_ids)}")
        if not verifies:
            problems.append("needs at least one verifies identifier")
    for identifier in test_ids:
        if not TEST_ID_RE.match(identifier):
            problems.append(f"invalid test_id {identifier!r}")
    for identifier in verifies:
        if not is_valid_id(identifier):
            problems.append(f"invalid identifier {identifier!r}")
        elif registry is not None and identifier not in registry:
            problems.append(f"unknown identifier {identifier}")
    return problems


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    """Attach JUnit properties and reject tests with missing or unknown identifiers.

    Raises:
        pytest.UsageError: at least one test violates the identifier rules.
    """
    registry: frozenset[str] | None = None
    registry_loaded = False
    errors: list[str] = []
    for item in items:
        test_ids = marker_args(item, "test_id")
        verifies = marker_args(item, "verifies")
        if test_ids:
            item.user_properties.append(("test_id", ",".join(test_ids)))
        if verifies:
            item.user_properties.append(("verifies", ",".join(verifies)))
        if (
            not test_ids
            and not verifies
            and not any(item.get_closest_marker(m) is not None for m in CATALOGUE_MARKERS)
        ):
            continue
        if not registry_loaded:
            registry = _registry(config)
            registry_loaded = True
        errors.extend(f"{item.nodeid}: {p}" for p in _check_item(item, registry))
    if errors:
        raise pytest.UsageError("traceability check failed:\n  " + "\n  ".join(errors))


@pytest.hookimpl(wrapper=True)
def pytest_runtest_makereport(
    item: pytest.Item, call: pytest.CallInfo[None]
) -> Generator[None, pytest.TestReport, pytest.TestReport]:
    """Classify infrastructure errors and record trace outcomes."""
    report = yield
    if call.excinfo is not None and call.excinfo.errisinstance(BenchInfrastructureError):
        item.config.stash[_INFRA_ERRORS].append(f"{item.nodeid}: {call.excinfo.value}")
        report.user_properties.append(("outcome_class", "infrastructure"))
    if report.when == "call" or report.outcome != "passed":
        test_ids = marker_args(item, "test_id")
        item.config.stash[_RECORDS][item.nodeid] = TraceRecord(
            nodeid=item.nodeid,
            test_id=test_ids[0] if test_ids else None,
            verifies=marker_args(item, "verifies"),
            outcome=report.outcome if report.when == "call" else f"{report.outcome}:{report.when}",
        )
    return report


def pytest_sessionfinish(session: pytest.Session, exitstatus: int) -> None:
    """Write ``trace.json`` and map infrastructure errors to exit code 10."""
    config = session.config
    trace_path = config.getoption("--hil-trace-json")
    if trace_path:
        write_trace_json(Path(str(trace_path)), config.stash[_RECORDS].values())
    if config.stash[_INFRA_ERRORS]:
        session.exitstatus = INFRASTRUCTURE_EXIT_CODE


def pytest_terminal_summary(terminalreporter: pytest.TerminalReporter) -> None:
    """List infrastructure errors at the end of the run."""
    errors = terminalreporter.config.stash.get(_INFRA_ERRORS, [])
    if errors:
        terminalreporter.section("bench infrastructure errors (exit code 10)")
        for line in errors:
            terminalreporter.line(line)


@pytest.fixture(scope="session")
def bench(request: pytest.FixtureRequest) -> Iterator[Bench]:
    """Opened bench; the test is skipped when no bench configuration is given."""
    path = resolve_bench_path(request.config.getoption("--hil-bench"))
    if path is None:
        pytest.skip(f"no bench configuration (--hil-bench or {BENCH_ENV})")
    try:
        config = load_bench_config(path)
    except ConfigError as exc:
        raise BenchInfrastructureError(str(exc)) from exc
    opened = Bench.open(config)
    try:
        yield opened
    finally:
        opened.close()


@pytest.fixture(autouse=True)
def _hil_requirements(request: pytest.FixtureRequest) -> None:
    """Skip tests whose ``requires`` capabilities are missing on the bench."""
    required = (
        marker_args(request.node, "requires") if isinstance(request.node, pytest.Item) else ()
    )
    if not required:
        return
    opened: Bench = request.getfixturevalue("bench")
    missing = opened.missing(required)
    if missing:
        pytest.skip(f"bench lacks {', '.join(missing)}")


@pytest.fixture
def topology(request: pytest.FixtureRequest, bench: Bench) -> str:
    """Integration topology from the ``topology`` marker; skipped when the bench cannot provide it.

    Raises:
        pytest.UsageError: the test has no or an unknown topology marker.
    """
    names = marker_args(request.node, "topology") if isinstance(request.node, pytest.Item) else ()
    if len(names) != 1 or names[0] not in TOPOLOGY_CAPABILITIES:
        raise pytest.UsageError(f"{request.node.nodeid}: needs one topology marker T1, T2 or T3")
    missing = bench.missing(TOPOLOGY_CAPABILITIES[names[0]])
    if missing:
        pytest.skip(f"topology {names[0]} needs {', '.join(missing)}")
    return names[0]


@pytest.fixture
def safe_bench(bench: Bench) -> Iterator[Bench]:
    """Bench that is brought to the safe state after the test, whatever its outcome."""
    try:
        yield bench
    finally:
        bench.safe_state()
