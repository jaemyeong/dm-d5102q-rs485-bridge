#!/usr/bin/env python3
"""Validate the M0 documentation and traceability contract.

This is a host-only consistency check. It deliberately makes no hardware,
system, or deployment claim.
"""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

from validate_evidence_packet import validate_repository_evidence_contract
from validate_memory_budget import validate_repository_budget
from validate_milestone_gates import validate_repository_milestones
from validate_project_loop import validate_repository_project_loop
from validate_public_sources import validate_repository_public_sources

ROOT = Path(__file__).resolve().parents[1]
DESIGN_PATH = ROOT / "docs/design/DM-D5102Q-RS485-WIFI-BRIDGE-SPEC-v1.0.md"
EXPECTED_DESIGN_SHA256 = (
    "d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114"
)
EXPECTED_REQUIREMENTS = {
    "HW-01",
    "RX-01",
    "FR-01",
    "NET-01",
    "CFG-01",
    "WEB-01",
    "TX-01",
    "SCN-01",
    "SEC-01",
    "OTA-01",
    "OPS-01",
}
ALLOWED_CLASSIFICATIONS = {"PUB", "OBS", "DEC", "UNV"}
ALLOWED_PROOF_STAGES = {
    "host",
    "host-browser",
    "host-system",
    "host-then-hardware",
    "host-then-target",
    "hardware",
    "system",
    "target-deployment",
    "deployment",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def fail(message: str) -> None:
    raise AssertionError(message)


def load_json(relative: str) -> dict:
    path = ROOT / relative
    if not path.is_file():
        fail(f"missing required file: {relative}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid JSON in {relative}: {exc}")


def unique_ids(items: list[dict], label: str) -> set[str]:
    ids = [item.get("id") for item in items]
    if any(not isinstance(item_id, str) or not item_id for item_id in ids):
        fail(f"{label} contains a missing or invalid id")
    if len(ids) != len(set(ids)):
        fail(f"{label} contains duplicate ids")
    return set(ids)


def validate_design_hash() -> None:
    if not DESIGN_PATH.is_file():
        fail(f"governing design is unavailable: {DESIGN_PATH}")
    digest = hashlib.sha256(DESIGN_PATH.read_bytes()).hexdigest()
    if digest != EXPECTED_DESIGN_SHA256:
        fail(f"governing design hash changed: {digest}")


def validate_traceability() -> tuple[int, int]:
    requirements_doc = load_json("requirements/requirements.json")
    vectors_doc = load_json("tests/vectors/initial.json")
    requirements = requirements_doc.get("requirements", [])
    vectors = vectors_doc.get("vectors", [])
    requirement_ids = unique_ids(requirements, "requirements")
    vector_ids = unique_ids(vectors, "test vectors")

    if requirements_doc.get("designSha256") != EXPECTED_DESIGN_SHA256:
        fail("requirements baseline points to the wrong design hash")
    if requirement_ids != EXPECTED_REQUIREMENTS:
        missing = sorted(EXPECTED_REQUIREMENTS - requirement_ids)
        extra = sorted(requirement_ids - EXPECTED_REQUIREMENTS)
        fail(f"requirement id mismatch; missing={missing}, extra={extra}")

    referenced_vectors: set[str] = set()
    for requirement in requirements:
        sources = set(requirement.get("sources", []))
        if not sources or not sources <= ALLOWED_CLASSIFICATIONS:
            fail(f"{requirement['id']} has invalid source classifications")
        tests = requirement.get("tests", [])
        if not tests:
            fail(f"{requirement['id']} has no test mapping")
        unknown = set(tests) - vector_ids
        if unknown:
            fail(f"{requirement['id']} references unknown tests: {sorted(unknown)}")
        referenced_vectors.update(tests)

    if referenced_vectors != vector_ids:
        fail(f"unreferenced test vectors: {sorted(vector_ids - referenced_vectors)}")

    mapped_requirements: set[str] = set()
    for vector in vectors:
        proof_stage = vector.get("proofStage")
        if proof_stage not in ALLOWED_PROOF_STAGES:
            fail(f"{vector['id']} has invalid proofStage: {proof_stage}")
        mapped = set(vector.get("requirementIds", []))
        unknown = mapped - requirement_ids
        if not mapped or unknown:
            fail(f"{vector['id']} has invalid requirement mapping: {sorted(unknown)}")
        mapped_requirements.update(mapped)

    if mapped_requirements != requirement_ids:
        fail("not every requirement is covered by a vector")
    return len(requirements), len(vectors)


def validate_evidence() -> int:
    evidence_doc = load_json("docs/cleanroom/evidence-register.json")
    entries = evidence_doc.get("entries", [])
    unique_ids(entries, "evidence register")
    for entry in entries:
        classification = entry.get("classification")
        if classification not in ALLOWED_CLASSIFICATIONS:
            fail(f"{entry['id']} has invalid classification: {classification}")
        digest = entry.get("sha256")
        if digest is not None and not SHA256_RE.fullmatch(digest):
            fail(f"{entry['id']} has an invalid SHA-256")
        if not entry.get("facts"):
            fail(f"{entry['id']} has no recorded facts")
        if classification == "UNV" and not entry.get("verification"):
            fail(f"{entry['id']} is UNV without a verification method")
    return len(entries)


def validate_loop_state() -> int:
    metrics = validate_repository_project_loop()
    return int(metrics["ledger_rows"])


def main() -> int:
    try:
        validate_design_hash()
        requirement_count, vector_count = validate_traceability()
        evidence_count = validate_evidence()
        ledger_count = validate_loop_state()
        evidence_contract = validate_repository_evidence_contract()
        public_sources = validate_repository_public_sources()
        _, layout = validate_repository_budget()
        milestone_gates = validate_repository_milestones()
    except AssertionError as exc:
        print(f"M0 validation: FAIL: {exc}", file=sys.stderr)
        return 1

    print(
        "M0 validation: PASS "
        f"({requirement_count} requirements, {vector_count} vectors, "
        f"{evidence_count} evidence entries, {ledger_count} ledger rows, "
        f"{evidence_contract['packet_types']} physical packet types, "
        f"{evidence_contract['templates']} non-evidence templates, "
        f"{public_sources['sources']} locked public sources, "
        f"{public_sources['conflicts']} historical A131 conflicts, "
        f"{public_sources['hardware_gates']} unverified T002 hardware gates, "
        f"{milestone_gates['criteria']} detailed design criteria, "
        f"current milestone {milestone_gates['currentMilestone']}, "
        f"{layout.unallocated_spare_bytes} partition-spare bytes)"
    )
    print(
        "Proof boundary: host/document/flash-layout consistency only; "
        "runtime and physical gates remain UNV"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
