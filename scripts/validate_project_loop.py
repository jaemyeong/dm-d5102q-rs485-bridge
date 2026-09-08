#!/usr/bin/env python3
"""Validate the durable embedded-project loop and its evidence boundaries."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections.abc import Mapping
from datetime import datetime
from pathlib import Path
from typing import Any

from target_contract import ACTIVE_TARGET, AMENDMENT

ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = ROOT / "requirements/project-loop-contract.json"
MILESTONE_STATE_PATH = "requirements/milestone-state.json"
EXPECTED_CONTRACT_CANONICAL_SHA256 = (
    "864cc5695835563742c4cbb0d47a96e3e410f99320444c571180f4c0f143a51e"
)
EXPECTED_DESIGN_SHA256 = (
    "d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114"
)
EXPECTED_TARGET = {
    "boardId": "m5stack-atom",
    "atomSku": "C008",
    "baseSku": "T002",
    "excludedBoard": "Atom S3 Lite",
    "excludedPort": "/dev/cu.usbmodem212201",
    "excludedUsbSerial": "34:B7:DA:57:38:94",
}
EXPECTED_STATE_FILES = {
    ".arduino/goal.md",
    ".arduino/next-todo.md",
    ".arduino/board-profile.md",
    ".arduino/resume-token.md",
    ".arduino/experiment-log.jsonl",
}
EXPECTED_EVIDENCE_STAGES = {
    ".arduino/evidence/build",
    ".arduino/evidence/upload",
    ".arduino/evidence/hardware",
    ".arduino/evidence/system",
    ".arduino/evidence/deployment",
}
EXPECTED_LEDGER_PATH = ".arduino/experiment-log.jsonl"
EXPECTED_SEALED_PREFIX_ROWS = 24
EXPECTED_SEALED_PREFIX_SHA256 = (
    "33563364927cfb9840348a7fd412326c9e02ef661ef9dff0c594678019a74ba4"
)
EXPECTED_ROLLBACK = {
    "path": ".arduino/evidence/deployment/atom-lite-pre-smoke-4mb.bin",
    "bytes": 4194304,
    "sha256": "3f2a06f6ecd7d8bf358201923d00b4f10bf5ad847cb66a1473d220b787e13c8c",
}
EXPECTED_IGNORE_RULES = {
    ".arduino/evidence/deployment/*.bin",
    ".arduino/evidence/hardware/private/",
    ".arduino/evidence/public-sources/raw/",
}
CORE_LEDGER_FIELDS = {"timestamp", "stage", "result"}
STRUCTURED_LEDGER_FIELDS = {
    "schemaVersion",
    "previousRowSha256",
    "timestamp",
    "stage",
    "hypothesis",
    "changed_files",
    "command",
    "result",
    "metric",
    "gates",
    "acceptance",
    "rollback",
    "lesson",
    "next_candidate",
}
TEXT_LEDGER_FIELDS = {
    "timestamp",
    "stage",
    "hypothesis",
    "command",
    "result",
    "acceptance",
    "rollback",
    "lesson",
    "next_candidate",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


class ProjectLoopValidationError(AssertionError):
    """A fail-closed durable-loop contract violation."""


def fail(message: str) -> None:
    raise ProjectLoopValidationError(message)


def canonical_sha256(value: dict[str, Any]) -> str:
    encoded = json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def load_contract(path: Path = CONTRACT_PATH) -> dict[str, Any]:
    if not path.is_file():
        fail(f"missing project-loop contract: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid project-loop contract: {exc}")
    if not isinstance(value, dict):
        fail("project-loop contract root must be an object")
    return value


def validate_contract(contract: dict[str, Any]) -> None:
    if contract.get("schemaVersion") != 1:
        fail("project-loop contract schemaVersion must be 1")
    if contract.get("designSha256") != EXPECTED_DESIGN_SHA256:
        fail("project-loop contract designSha256 changed")
    if contract.get("designAmendment") != AMENDMENT:
        fail("project-loop target amendment changed or is missing")
    if contract.get("currentMilestone") != "M0":
        fail("project-loop contract currentMilestone must remain M0")

    target = contract.get("target")
    if not isinstance(target, dict):
        fail("project-loop contract target must be an object")
    for field, expected in EXPECTED_TARGET.items():
        if target.get(field) != expected:
            fail(f"project-loop contract {field} must be {expected!r}")

    if set(contract.get("stateFiles", [])) != EXPECTED_STATE_FILES:
        fail("project-loop contract stateFiles changed")
    if set(contract.get("evidenceStageDirectories", [])) != EXPECTED_EVIDENCE_STAGES:
        fail("project-loop contract evidenceStageDirectories changed")

    next_todo = contract.get("nextTodo")
    if not isinstance(next_todo, dict):
        fail("project-loop contract nextTodo must be an object")
    if next_todo.get("openTasks") != 1 or next_todo.get("owner") != "user":
        fail("project-loop contract must retain one user-owned next action")

    ledger = contract.get("ledger")
    if not isinstance(ledger, dict):
        fail("project-loop contract ledger must be an object")
    if ledger.get("path") != EXPECTED_LEDGER_PATH:
        fail(f"project-loop ledger path must be {EXPECTED_LEDGER_PATH}")
    if ledger.get("sealedPrefixRows") != EXPECTED_SEALED_PREFIX_ROWS:
        fail(f"project-loop sealedPrefixRows must be {EXPECTED_SEALED_PREFIX_ROWS}")
    if ledger.get("sealedPrefixSha256") != EXPECTED_SEALED_PREFIX_SHA256:
        fail("project-loop sealedPrefixSha256 changed")
    if ledger.get("rowSchemaVersion") != 1:
        fail("project-loop rowSchemaVersion must be 1")
    if set(ledger.get("requiredFieldsAfterPrefix", [])) != STRUCTURED_LEDGER_FIELDS:
        fail("project-loop requiredFieldsAfterPrefix changed")

    if contract.get("rollbackArtifact") != EXPECTED_ROLLBACK:
        fail("project-loop rollback artifact contract changed")
    if set(contract.get("requiredIgnoreRules", [])) != EXPECTED_IGNORE_RULES:
        fail("project-loop sensitive ignore rules changed")

    digest = canonical_sha256(contract)
    if digest != EXPECTED_CONTRACT_CANONICAL_SHA256:
        fail(f"project-loop contract digest changed: {digest}")


def normalize_text(value: str) -> str:
    return " ".join(value.split())


def require_markers(text: str, markers: Any, label: str) -> None:
    if not isinstance(markers, list) or any(not isinstance(marker, str) for marker in markers):
        fail(f"{label} contract markers are invalid")
    normalized = normalize_text(text)
    for marker in markers:
        if normalize_text(marker) not in normalized:
            fail(f"{label} is missing required marker: {marker}")


def validate_relative_path(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        fail(f"{label} must be a non-empty relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        fail(f"{label} escapes the repository: {value}")
    return value


def parse_timestamp(value: Any, label: str) -> datetime:
    if not isinstance(value, str) or not value:
        fail(f"{label} must be a non-empty timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        fail(f"{label} is not ISO-8601: {value}")
    if parsed.utcoffset() is None:
        fail(f"{label} must include a timezone offset")
    return parsed


def read_relative(
    relative: str,
    *,
    root: Path,
    file_overrides: Mapping[str, bytes | None],
    label: str,
) -> bytes:
    if relative in file_overrides:
        value = file_overrides[relative]
        if value is None:
            fail(f"missing {label}: {relative}")
        if not isinstance(value, bytes):
            fail(f"override for {relative} must be bytes or None")
        return value
    path = root / validate_relative_path(relative, label)
    if not path.is_file():
        fail(f"missing {label}: {relative}")
    try:
        return path.read_bytes()
    except OSError as exc:
        fail(f"cannot read {label} {relative}: {exc}")
    raise AssertionError("unreachable")


def decode_text(raw: bytes, label: str) -> str:
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        fail(f"{label} is not UTF-8: {exc}")
    raise AssertionError("unreachable")


def validate_ledger(raw: bytes, ledger_contract: dict[str, Any]) -> dict[str, int]:
    if not raw.endswith(b"\n"):
        fail("experiment ledger must end with a newline")
    lines_with_endings = raw.splitlines(keepends=True)
    if len(lines_with_endings) < EXPECTED_SEALED_PREFIX_ROWS:
        fail(
            "experiment ledger has fewer rows than its sealed prefix: "
            f"{len(lines_with_endings)}"
        )
    prefix = b"".join(lines_with_endings[:EXPECTED_SEALED_PREFIX_ROWS])
    if hashlib.sha256(prefix).hexdigest() != ledger_contract["sealedPrefixSha256"]:
        fail("experiment ledger sealed prefix SHA-256 mismatch")

    previous_timestamp: datetime | None = None
    previous_content: bytes | None = None
    for line_number, line in enumerate(lines_with_endings, 1):
        content = line.rstrip(b"\r\n")
        if not content:
            fail(f"blank experiment ledger row at line {line_number}")
        try:
            row = json.loads(content)
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            fail(f"invalid experiment ledger JSON at line {line_number}: {exc}")
        if not isinstance(row, dict):
            fail(f"experiment ledger row {line_number} must be an object")
        missing_core = CORE_LEDGER_FIELDS - set(row)
        if missing_core:
            fail(f"experiment ledger row {line_number} lacks core fields: {sorted(missing_core)}")
        for field in CORE_LEDGER_FIELDS:
            if not isinstance(row[field], str) or not row[field]:
                fail(f"experiment ledger row {line_number} has invalid {field}")

        timestamp = parse_timestamp(row["timestamp"], f"ledger row {line_number} timestamp")
        if previous_timestamp is not None and timestamp < previous_timestamp:
            fail(f"experiment ledger row {line_number} timestamp moved backwards")

        if line_number > EXPECTED_SEALED_PREFIX_ROWS:
            missing = STRUCTURED_LEDGER_FIELDS - set(row)
            if missing:
                fail(
                    f"experiment ledger row {line_number} is missing required fields: "
                    f"{sorted(missing)}"
                )
            if row.get("schemaVersion") != ledger_contract["rowSchemaVersion"]:
                fail(f"experiment ledger row {line_number} has wrong schemaVersion")
            previous_hash = row.get("previousRowSha256")
            if not isinstance(previous_hash, str) or not SHA256_RE.fullmatch(previous_hash):
                fail(f"experiment ledger row {line_number} has invalid previousRowSha256")
            expected_previous_hash = hashlib.sha256(previous_content or b"").hexdigest()
            if previous_hash != expected_previous_hash:
                fail(f"experiment ledger row {line_number} previous-row hash mismatch")
            for field in TEXT_LEDGER_FIELDS:
                if not isinstance(row.get(field), str) or not row[field]:
                    fail(f"experiment ledger row {line_number} has invalid {field}")
            changed_files = row.get("changed_files")
            if not isinstance(changed_files, list) or any(
                not isinstance(value, str) for value in changed_files
            ):
                fail(f"experiment ledger row {line_number} has invalid changed_files")
            if len(changed_files) != len(set(changed_files)):
                fail(f"experiment ledger row {line_number} repeats changed_files")
            for changed_file in changed_files:
                validate_relative_path(changed_file, f"ledger row {line_number} changed_file")
            for field in ("metric", "gates"):
                if not isinstance(row.get(field), dict):
                    fail(f"experiment ledger row {line_number} has invalid {field}")

        previous_timestamp = timestamp
        previous_content = content

    return {
        "rows": len(lines_with_endings),
        "sealed_rows": EXPECTED_SEALED_PREFIX_ROWS,
        "linked_rows": len(lines_with_endings) - EXPECTED_SEALED_PREFIX_ROWS,
    }


def validate_repository_project_loop(
    contract: dict[str, Any] | None = None,
    *,
    root: Path = ROOT,
    file_overrides: Mapping[str, bytes | None] | None = None,
) -> dict[str, int | str]:
    contract = load_contract() if contract is None else contract
    validate_contract(contract)
    file_overrides = {} if file_overrides is None else file_overrides

    state_text: dict[str, str] = {}
    for relative in contract["stateFiles"]:
        raw = read_relative(
            relative,
            root=root,
            file_overrides=file_overrides,
            label="loop state",
        )
        if relative != EXPECTED_LEDGER_PATH:
            state_text[relative] = decode_text(raw, relative)

    design_raw = read_relative(
        contract["designPath"],
        root=root,
        file_overrides=file_overrides,
        label="governing design",
    )
    if hashlib.sha256(design_raw).hexdigest() != contract["designSha256"]:
        fail("governing design SHA-256 does not match the project-loop contract")

    amendment_raw = read_relative(
        AMENDMENT["path"],
        root=root,
        file_overrides=file_overrides,
        label="target amendment",
    )
    if hashlib.sha256(amendment_raw).hexdigest() != AMENDMENT["sha256"]:
        fail("target amendment SHA-256 mismatch")

    require_markers(
        state_text[".arduino/goal.md"],
        contract["goalRequiredMarkers"],
        "goal",
    )
    require_markers(
        state_text[".arduino/board-profile.md"],
        contract["boardProfileRequiredMarkers"],
        "board profile",
    )
    require_markers(
        state_text[".arduino/resume-token.md"],
        contract["resumeRequiredMarkers"],
        "resume token",
    )

    todo_text = state_text[".arduino/next-todo.md"]
    open_tasks = len(re.findall(r"^- \[ \] ", todo_text, flags=re.MULTILINE))
    if open_tasks != contract["nextTodo"]["openTasks"]:
        fail(
            f"next-todo must contain exactly {contract['nextTodo']['openTasks']} open task, "
            f"found {open_tasks}"
        )
    require_markers(todo_text, contract["nextTodo"]["requiredMarkers"], "next-todo")

    milestone_raw = read_relative(
        MILESTONE_STATE_PATH,
        root=root,
        file_overrides=file_overrides,
        label="milestone state",
    )
    try:
        milestone_state = json.loads(milestone_raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"invalid milestone state JSON: {exc}")
    if not isinstance(milestone_state, dict):
        fail("milestone state root must be an object")
    if milestone_state.get("currentMilestone") != contract["currentMilestone"]:
        fail(
            "milestone state currentMilestone does not match the durable loop: "
            f"{milestone_state.get('currentMilestone')}"
        )
    if milestone_state.get("designSha256") != contract["designSha256"]:
        fail("milestone state designSha256 does not match the durable loop")
    if milestone_state.get("designAmendment") != AMENDMENT:
        fail("milestone state target amendment does not match the durable loop")
    if milestone_state.get("effectiveTarget") != ACTIVE_TARGET:
        fail("milestone state effective target does not match the durable loop")

    for relative in contract["evidenceStageDirectories"]:
        directory = root / validate_relative_path(relative, "evidence stage")
        if not directory.is_dir():
            fail(f"missing evidence stage directory: {relative}")
        if not (directory / "README.md").is_file():
            fail(f"evidence stage lacks README.md boundary: {relative}")

    ledger_raw = read_relative(
        contract["ledger"]["path"],
        root=root,
        file_overrides=file_overrides,
        label="experiment ledger",
    )
    ledger_metrics = validate_ledger(ledger_raw, contract["ledger"])

    rollback = contract["rollbackArtifact"]
    rollback_raw = read_relative(
        rollback["path"],
        root=root,
        file_overrides=file_overrides,
        label="rollback artifact",
    )
    rollback_digest = hashlib.sha256(rollback_raw).hexdigest()
    if len(rollback_raw) != rollback["bytes"] or rollback_digest != rollback["sha256"]:
        fail(
            "rollback artifact size or SHA-256 mismatch: "
            f"bytes={len(rollback_raw)}, sha256={rollback_digest}"
        )

    gitignore = decode_text(
        read_relative(
            ".gitignore",
            root=root,
            file_overrides=file_overrides,
            label="Git ignore policy",
        ),
        ".gitignore",
    )
    ignore_rules = {line.strip() for line in gitignore.splitlines() if line.strip()}
    missing_ignore_rules = set(contract["requiredIgnoreRules"]) - ignore_rules
    if missing_ignore_rules:
        fail(f"missing sensitive evidence ignore rule: {sorted(missing_ignore_rules)}")

    private_root = root / ".arduino/evidence/hardware/private"
    private_files = sum(path.is_file() for path in private_root.rglob("*")) if private_root.is_dir() else 0
    return {
        "current_milestone": contract["currentMilestone"],
        "open_tasks": open_tasks,
        "state_files": len(contract["stateFiles"]),
        "evidence_stages": len(contract["evidenceStageDirectories"]),
        "ledger_rows": ledger_metrics["rows"],
        "sealed_ledger_rows": ledger_metrics["sealed_rows"],
        "linked_ledger_rows": ledger_metrics["linked_rows"],
        "rollback_bytes": len(rollback_raw),
        "private_evidence_files": private_files,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args(argv)
    try:
        metrics = validate_repository_project_loop()
    except ProjectLoopValidationError as exc:
        print(f"project-loop validation: FAIL: {exc}", file=sys.stderr)
        return 1
    print(
        "project-loop validation: PASS "
        f"({metrics['state_files']} state files, {metrics['open_tasks']} open task, "
        f"{metrics['evidence_stages']} evidence stages, {metrics['ledger_rows']} ledger rows, "
        f"{metrics['linked_ledger_rows']} hash-linked rows, "
        f"{metrics['private_evidence_files']} private evidence files)"
    )
    print(
        "Proof boundary: durable host state and artifact integrity only; "
        "no physical action or field observation is inferred"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
