#!/usr/bin/env python3
"""Derive and validate every design milestone and release criterion.

The governing design remains the sole copy of criterion text. This module
derives stable IDs from its immutable SHA-256-bound structure, then applies the
small status/evidence overlay in requirements/milestone-state.json.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

from target_contract import ACTIVE_TARGET, AMENDMENT, EFFECTIVE_CRITERION_TEXT

ROOT = Path(__file__).resolve().parents[1]
DESIGN_PATH = ROOT / "docs/design/DM-D5102Q-RS485-WIFI-BRIDGE-SPEC-v1.0.md"
STATE_PATH = ROOT / "requirements/milestone-state.json"
EXPECTED_DESIGN_SHA256 = (
    "d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114"
)
MILESTONE_IDS = tuple(f"M{index}" for index in range(10))
GROUP_SPECS = {
    "host-tests": ("16.1", "HT", 8),
    "hardware-tests": ("16.2", "HI", 12),
    "field-tests": ("16.3", "FT", 6),
    "quantitative-acceptance": ("16.4", "QA", 11),
    "release-definition": ("20", "RD", 11),
    "implementation-checklist": ("21", "IC", 10),
}
EXPECTED_MILESTONE_COUNTS = {
    "M0": (7, 5, 4, 1),
    "M1": (6, 4, 4, 1),
    "M2": (6, 4, 4, 1),
    "M3": (6, 2, 4, 1),
    "M4": (6, 3, 4, 1),
    "M5": (6, 3, 5, 1),
    "M6": (6, 3, 4, 1),
    "M7": (5, 3, 5, 1),
    "M8": (7, 3, 5, 1),
    "M9": (6, 4, 5, 1),
}
EXPECTED_TOTAL = 207
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
PHYSICAL_PROOF_STAGES = {
    "host-system",
    "host-then-hardware",
    "host-then-target",
    "hardware",
    "system",
    "target-deployment",
    "deployment",
}
CRITERION_STATUSES = {
    "satisfied",
    "blocked",
    "registered",
    "not-started",
    "not-evaluated",
    "open",
}
STOP_STATUSES = {"clear", "triggered", "not-evaluated"}
MILESTONE_STATUSES = {"blocked", "not-started", "in-progress", "satisfied"}
ALLOWED_OVERRIDE_FIELDS = {
    "status",
    "proofStage",
    "evidenceIds",
    "artifactPaths",
    "blocker",
    "note",
}


class MilestoneValidationError(AssertionError):
    """A fail-closed milestone catalog or state violation."""


@dataclass(frozen=True)
class Criterion:
    id: str
    group: str
    kind: str
    text: str
    sourceLine: int
    textSha256: str


def fail(message: str) -> None:
    raise MilestoneValidationError(message)


def load_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        fail(f"missing required file: {path.relative_to(ROOT)}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid JSON in {path.relative_to(ROOT)}: {exc}")
    if not isinstance(value, dict):
        fail(f"JSON root must be an object: {path.relative_to(ROOT)}")
    return value


def criterion(
    criterion_id: str,
    group: str,
    kind: str,
    text: str,
    line_number: int,
) -> Criterion:
    normalized = text.strip()
    if not normalized:
        fail(f"empty criterion derived for {criterion_id}")
    return Criterion(
        id=criterion_id,
        group=group,
        kind=kind,
        text=normalized,
        sourceLine=line_number,
        textSha256=hashlib.sha256(normalized.encode("utf-8")).hexdigest(),
    )


def find_line(lines: list[str], pattern: str, start: int = 0) -> int:
    compiled = re.compile(pattern)
    for index in range(start, len(lines)):
        if compiled.match(lines[index]):
            return index
    fail(f"governing design section is missing: {pattern}")
    raise AssertionError("unreachable")


def bullet_rows(lines: list[str], start: int, end: int, checklist: bool = False) -> list[tuple[int, str]]:
    prefix = "- [ ] " if checklist else "- "
    return [
        (index + 1, line[len(prefix) :].strip())
        for index, line in enumerate(lines[start:end], start)
        if line.startswith(prefix)
    ]


def derive_catalog(design_text: str) -> list[Criterion]:
    lines = design_text.splitlines()
    catalog: list[Criterion] = []

    milestone_starts = {
        milestone_id: find_line(lines, rf"^### {milestone_id} \u2014 ")
        for milestone_id in MILESTONE_IDS
    }
    milestone_end = find_line(lines, r"^## 18\. ")
    for milestone_index, milestone_id in enumerate(MILESTONE_IDS):
        start = milestone_starts[milestone_id]
        end = (
            milestone_starts[MILESTONE_IDS[milestone_index + 1]]
            if milestone_index + 1 < len(MILESTONE_IDS)
            else milestone_end
        )
        task_marker = find_line(lines, r"^작업:$", start)
        deliverable_line = find_line(lines, r"^산출물: ", task_marker)
        exit_marker = find_line(lines, r"^종료 조건:$", deliverable_line)
        stop_line = find_line(lines, r"^중단 조건: ", exit_marker)
        if not (start < task_marker < deliverable_line < exit_marker < stop_line < end):
            fail(f"unexpected governing design structure for {milestone_id}")

        for ordinal, (line_number, text) in enumerate(
            bullet_rows(lines, task_marker + 1, deliverable_line), 1
        ):
            catalog.append(
                criterion(f"{milestone_id}-T{ordinal:02d}", milestone_id, "task", text, line_number)
            )

        deliverable_text = lines[deliverable_line].split(":", 1)[1].strip()
        deliverables = [item.strip().rstrip(".") for item in deliverable_text.split(",")]
        for ordinal, text in enumerate(deliverables, 1):
            catalog.append(
                criterion(
                    f"{milestone_id}-D{ordinal:02d}",
                    milestone_id,
                    "deliverable",
                    text,
                    deliverable_line + 1,
                )
            )

        for ordinal, (line_number, text) in enumerate(
            bullet_rows(lines, exit_marker + 1, stop_line), 1
        ):
            catalog.append(
                criterion(f"{milestone_id}-E{ordinal:02d}", milestone_id, "exit", text, line_number)
            )

        stop_text = lines[stop_line].split(":", 1)[1].strip()
        catalog.append(
            criterion(f"{milestone_id}-S01", milestone_id, "stop", stop_text, stop_line + 1)
        )

    for group, (section, prefix, _) in GROUP_SPECS.items():
        if section in {"20", "21"}:
            start = find_line(lines, rf"^## {re.escape(section)}\. ")
            end = next(
                (
                    index
                    for index in range(start + 1, len(lines))
                    if lines[index].startswith("## ")
                ),
                len(lines),
            )
        else:
            start = find_line(lines, rf"^### {re.escape(section)} ")
            end = next(
                (
                    index
                    for index in range(start + 1, len(lines))
                    if lines[index].startswith("### ") or lines[index].startswith("## ")
                ),
                len(lines),
            )
        rows = bullet_rows(lines, start + 1, end, checklist=section == "21")
        for ordinal, (line_number, text) in enumerate(rows, 1):
            catalog.append(criterion(f"{prefix}-{ordinal:02d}", group, "criterion", text, line_number))

    ids = [item.id for item in catalog]
    if len(ids) != len(set(ids)):
        fail("derived milestone catalog contains duplicate IDs")
    return catalog


def catalog_sha256(catalog: list[Criterion]) -> str:
    payload = [asdict(item) for item in catalog]
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def validate_catalog(catalog: list[Criterion], state: dict[str, Any]) -> None:
    if len(catalog) != EXPECTED_TOTAL:
        fail(f"derived catalog must contain {EXPECTED_TOTAL} criteria, found {len(catalog)}")

    for milestone_id, expected in EXPECTED_MILESTONE_COUNTS.items():
        counts = Counter(item.kind for item in catalog if item.group == milestone_id)
        actual = (
            counts["task"],
            counts["deliverable"],
            counts["exit"],
            counts["stop"],
        )
        if actual != expected:
            fail(f"{milestone_id} derived counts changed: expected={expected}, actual={actual}")

    for group, (_, _, expected) in GROUP_SPECS.items():
        actual = sum(item.group == group for item in catalog)
        if actual != expected:
            fail(f"{group} derived count changed: expected={expected}, actual={actual}")

    expectations = state.get("catalogExpectations")
    if not isinstance(expectations, dict) or expectations.get("total") != EXPECTED_TOTAL:
        fail("state catalogExpectations.total is missing or incorrect")
    expected_milestones = expectations.get("milestones")
    expected_groups = expectations.get("groups")
    if set(expected_milestones or {}) != set(MILESTONE_IDS):
        fail("state catalog milestone expectations must cover exactly M0 through M9")
    if set(expected_groups or {}) != set(GROUP_SPECS):
        fail("state catalog group expectations are incomplete")
    labels = ("tasks", "deliverables", "exitConditions", "stopConditions")
    for milestone_id, expected in EXPECTED_MILESTONE_COUNTS.items():
        stated = tuple(expected_milestones[milestone_id].get(label) for label in labels)
        if stated != expected:
            fail(f"state expectation changed for {milestone_id}: {stated}")
    for group, (_, _, expected) in GROUP_SPECS.items():
        if expected_groups[group] != expected:
            fail(f"state expectation changed for {group}: {expected_groups[group]}")

    digest = catalog_sha256(catalog)
    if state.get("catalogSha256") != digest:
        fail(f"derived catalog hash mismatch: expected {state.get('catalogSha256')}, found {digest}")


def validate_artifact_path(relative: Any, criterion_id: str) -> str:
    if not isinstance(relative, str) or not relative:
        fail(f"{criterion_id} has an invalid artifact path")
    path = Path(relative)
    if path.is_absolute() or ".." in path.parts:
        fail(f"{criterion_id} artifact path escapes the repository: {relative}")
    if not (ROOT / path).exists():
        fail(f"{criterion_id} artifact is missing: {relative}")
    return relative


def validate_requirement_ids(value: Any, known: set[str], label: str) -> list[str]:
    if not isinstance(value, list) or not value:
        fail(f"{label} must map to at least one requirement")
    if any(not isinstance(item, str) for item in value):
        fail(f"{label} has a non-string requirement ID")
    unknown = set(value) - known
    if unknown:
        fail(f"{label} maps to unknown requirements: {sorted(unknown)}")
    if len(value) != len(set(value)):
        fail(f"{label} repeats a requirement ID")
    return value


def validate_resolved_criterion(
    item: Criterion,
    resolved: dict[str, Any],
    evidence: dict[str, dict[str, Any]],
) -> None:
    status = resolved["status"]
    allowed_statuses = STOP_STATUSES if item.kind == "stop" else CRITERION_STATUSES
    if status not in allowed_statuses:
        fail(f"{item.id} has invalid status: {status}")
    proof_stage = resolved["proofStage"]
    if proof_stage not in ALLOWED_PROOF_STAGES:
        fail(f"{item.id} has invalid proofStage: {proof_stage}")

    evidence_ids = resolved.get("evidenceIds", [])
    artifact_paths = resolved.get("artifactPaths", [])
    if not isinstance(evidence_ids, list) or any(not isinstance(value, str) for value in evidence_ids):
        fail(f"{item.id} has invalid evidenceIds")
    if not isinstance(artifact_paths, list):
        fail(f"{item.id} has invalid artifactPaths")
    if len(evidence_ids) != len(set(evidence_ids)):
        fail(f"{item.id} repeats an evidence ID")
    unknown_evidence = set(evidence_ids) - set(evidence)
    if unknown_evidence:
        fail(f"{item.id} references unknown evidence: {sorted(unknown_evidence)}")
    for path in artifact_paths:
        validate_artifact_path(path, item.id)

    if status == "satisfied" and not (evidence_ids or artifact_paths):
        fail(f"{item.id} is satisfied without evidence or an artifact")
    if status == "satisfied" and proof_stage in PHYSICAL_PROOF_STAGES:
        observed = [evidence[evidence_id] for evidence_id in evidence_ids]
        if not any(
            entry.get("classification") == "OBS"
            and entry.get("status") not in {"missing", "unresolved"}
            for entry in observed
        ):
            fail(f"{item.id} claims {proof_stage} satisfaction without observed evidence")
    if status in {"blocked", "triggered"} and not resolved.get("blocker"):
        fail(f"{item.id} is {status} without a blocker")


def resolve_group(
    catalog: list[Criterion],
    group_id: str,
    group_state: dict[str, Any],
    evidence: dict[str, dict[str, Any]],
    known_requirements: set[str],
    stop_default: str | None = None,
) -> list[dict[str, Any]]:
    items = [item for item in catalog if item.group == group_id]
    item_by_id = {item.id: item for item in items}
    overrides = group_state.get("overrides")
    if not isinstance(overrides, dict):
        fail(f"{group_id} overrides must be an object")
    unknown = set(overrides) - set(item_by_id)
    if unknown:
        fail(f"{group_id} contains unknown criterion overrides: {sorted(unknown)}")

    proof_stage = group_state.get("defaultProofStage")
    if proof_stage not in ALLOWED_PROOF_STAGES:
        fail(f"{group_id} has invalid defaultProofStage: {proof_stage}")
    default_status = group_state.get("defaultStatus")
    if default_status not in CRITERION_STATUSES:
        fail(f"{group_id} has invalid defaultStatus: {default_status}")
    requirement_ids = validate_requirement_ids(
        group_state.get("requirementIds"), known_requirements, group_id
    )

    resolved_items: list[dict[str, Any]] = []
    for item in items:
        override = overrides.get(item.id, {})
        if not isinstance(override, dict):
            fail(f"{item.id} override must be an object")
        unexpected_fields = set(override) - ALLOWED_OVERRIDE_FIELDS
        if unexpected_fields:
            fail(
                f"{item.id} override contains unsupported fields: "
                f"{sorted(unexpected_fields)}"
            )
        resolved = {
            "status": stop_default if item.kind == "stop" else default_status,
            "proofStage": proof_stage,
            "requirementIds": requirement_ids,
            **override,
        }
        validate_resolved_criterion(item, resolved, evidence)
        resolved_items.append({
            **asdict(item),
            **resolved,
            "effectiveText": EFFECTIVE_CRITERION_TEXT.get(item.id, item.text),
        })
    return resolved_items


def validate_repository_milestones(
    state: dict[str, Any] | None = None,
    design_text: str | None = None,
) -> dict[str, Any]:
    design_bytes = DESIGN_PATH.read_bytes() if design_text is None else design_text.encode("utf-8")
    design_digest = hashlib.sha256(design_bytes).hexdigest()
    if design_digest != EXPECTED_DESIGN_SHA256:
        fail(f"governing design hash changed: {design_digest}")
    design_text = design_bytes.decode("utf-8")
    state = load_json(STATE_PATH) if state is None else state
    if state.get("schemaVersion") != 1:
        fail("milestone state schemaVersion must be 1")
    if state.get("designVersion") != "1.0":
        fail("milestone state designVersion must be 1.0")
    if state.get("designSha256") != EXPECTED_DESIGN_SHA256:
        fail("milestone state points to the wrong governing design hash")
    if state.get("designAmendment") != AMENDMENT:
        fail("milestone state target amendment changed or is missing")
    if state.get("effectiveTarget") != ACTIVE_TARGET:
        fail("milestone state effective target must be C008 plus T002")
    amendment_bytes = (ROOT / AMENDMENT["path"]).read_bytes()
    if hashlib.sha256(amendment_bytes).hexdigest() != AMENDMENT["sha256"]:
        fail("target amendment SHA-256 mismatch")

    catalog = derive_catalog(design_text)
    validate_catalog(catalog, state)

    requirements_doc = load_json(ROOT / "requirements/requirements.json")
    vectors_doc = load_json(ROOT / "tests/vectors/initial.json")
    for label, document in (("requirements", requirements_doc), ("vectors", vectors_doc)):
        if document.get("designAmendment") != AMENDMENT:
            fail(f"{label} target amendment changed or is missing")
    requirements = requirements_doc.get("requirements", [])
    vectors = vectors_doc.get("vectors", [])
    known_requirements = {item.get("id") for item in requirements}
    if None in known_requirements or len(known_requirements) != 11:
        fail("top-level requirement baseline is malformed")
    test_ids_by_requirement: dict[str, list[str]] = {
        requirement_id: sorted(
            vector["id"]
            for vector in vectors
            if requirement_id in vector.get("requirementIds", [])
        )
        for requirement_id in known_requirements
    }

    evidence_doc = load_json(ROOT / "docs/cleanroom/evidence-register.json")
    evidence_entries = evidence_doc.get("entries", [])
    evidence = {entry.get("id"): entry for entry in evidence_entries}
    if None in evidence or len(evidence) != len(evidence_entries):
        fail("evidence register contains missing or duplicate IDs")

    milestone_states = state.get("milestones")
    group_states = state.get("groups")
    if not isinstance(milestone_states, dict) or set(milestone_states) != set(MILESTONE_IDS):
        fail("milestone state must cover exactly M0 through M9")
    if not isinstance(group_states, dict) or set(group_states) != set(GROUP_SPECS):
        fail("global criterion state groups are incomplete")

    milestone_reports: dict[str, Any] = {}
    prior_satisfied = True
    for milestone_id in MILESTONE_IDS:
        milestone_state = milestone_states[milestone_id]
        if not isinstance(milestone_state, dict):
            fail(f"{milestone_id} state must be an object")
        milestone_status = milestone_state.get("status")
        if milestone_status not in MILESTONE_STATUSES:
            fail(f"{milestone_id} has invalid milestone status: {milestone_status}")
        stop_default = milestone_state.get("defaultStopStatus")
        if stop_default not in STOP_STATUSES:
            fail(f"{milestone_id} has invalid defaultStopStatus: {stop_default}")

        resolved = resolve_group(
            catalog,
            milestone_id,
            milestone_state,
            evidence,
            known_requirements,
            stop_default=stop_default,
        )
        non_stop = [item for item in resolved if item["kind"] != "stop"]
        stops = [item for item in resolved if item["kind"] == "stop"]
        blockers = [
            item["id"]
            for item in resolved
            if item["status"] in {"blocked", "triggered"}
        ]
        blocked_by = milestone_state.get("blockedBy", [])
        if not isinstance(blocked_by, list) or any(not isinstance(value, str) for value in blocked_by):
            fail(f"{milestone_id} has invalid blockedBy")
        known_block_refs = set(MILESTONE_IDS) | {item["id"] for item in resolved}
        unknown_block_refs = set(blocked_by) - known_block_refs
        if unknown_block_refs:
            fail(f"{milestone_id} has unknown blockedBy references: {sorted(unknown_block_refs)}")

        if milestone_status == "satisfied":
            if not prior_satisfied:
                fail(f"{milestone_id} cannot be satisfied before every prior milestone")
            if any(item["status"] != "satisfied" for item in non_stop):
                fail(f"{milestone_id} is satisfied with open task/deliverable/exit criteria")
            if any(item["status"] != "clear" for item in stops):
                fail(f"{milestone_id} is satisfied without a clear stop condition")
        elif milestone_status == "blocked":
            if not blockers or not blocked_by:
                fail(f"{milestone_id} is blocked without explicit blocking criteria")
            if not set(blocked_by) <= set(blockers):
                fail(f"{milestone_id} blockedBy must name currently blocked or triggered criteria")
        elif milestone_status == "not-started":
            if any(item["status"] != "not-started" for item in non_stop):
                fail(f"{milestone_id} is not-started but has advanced criteria")
            if any(item["status"] != "not-evaluated" for item in stops):
                fail(f"{milestone_id} is not-started but its stop condition was evaluated")
        elif milestone_status == "in-progress" and not prior_satisfied:
            fail(f"{milestone_id} cannot start before every prior milestone is satisfied")

        counts = Counter(item["status"] for item in resolved)
        requirement_ids = milestone_state["requirementIds"]
        test_ids = sorted(
            {
                test_id
                for requirement_id in requirement_ids
                for test_id in test_ids_by_requirement[requirement_id]
            }
        )
        milestone_reports[milestone_id] = {
            "status": milestone_status,
            "criteria": len(resolved),
            "statusCounts": dict(sorted(counts.items())),
            "blockedBy": blocked_by,
            "requirementIds": requirement_ids,
            "testIds": test_ids,
            "items": resolved,
        }
        prior_satisfied = prior_satisfied and milestone_status == "satisfied"

    first_unsatisfied = next(
        (milestone_id for milestone_id in MILESTONE_IDS if milestone_reports[milestone_id]["status"] != "satisfied"),
        None,
    )
    if state.get("currentMilestone") != first_unsatisfied:
        fail(
            "currentMilestone must be the first unsatisfied milestone: "
            f"expected {first_unsatisfied}, found {state.get('currentMilestone')}"
        )

    group_reports: dict[str, Any] = {}
    for group_id in GROUP_SPECS:
        resolved = resolve_group(
            catalog,
            group_id,
            group_states[group_id],
            evidence,
            known_requirements,
        )
        counts = Counter(item["status"] for item in resolved)
        group_reports[group_id] = {
            "criteria": len(resolved),
            "statusCounts": dict(sorted(counts.items())),
            "items": resolved,
        }

    release_ready = (
        all(report["status"] == "satisfied" for report in milestone_reports.values())
        and all(
            item["status"] == "satisfied"
            for group_id in ("quantitative-acceptance", "release-definition", "implementation-checklist")
            for item in group_reports[group_id]["items"]
        )
    )
    return {
        "designSha256": design_digest,
        "designAmendment": dict(AMENDMENT),
        "effectiveTarget": dict(ACTIVE_TARGET),
        "catalogSha256": catalog_sha256(catalog),
        "criteria": len(catalog),
        "currentMilestone": first_unsatisfied,
        "releaseReady": release_ready,
        "milestones": milestone_reports,
        "groups": group_reports,
    }


def print_report(report: dict[str, Any]) -> None:
    print(
        f"Detailed gate catalog: {report['criteria']} criteria; "
        f"current={report['currentMilestone']}; releaseReady={str(report['releaseReady']).lower()}"
    )
    for milestone_id in MILESTONE_IDS:
        item = report["milestones"][milestone_id]
        counts = ", ".join(f"{key}={value}" for key, value in item["statusCounts"].items())
        print(f"  {milestone_id}: {item['status']} ({item['criteria']}; {counts})")
        if item["blockedBy"]:
            print(f"    blockedBy: {', '.join(item['blockedBy'])}")
    for group_id in GROUP_SPECS:
        item = report["groups"][group_id]
        counts = ", ".join(f"{key}={value}" for key, value in item["statusCounts"].items())
        print(f"  {group_id}: {item['criteria']} ({counts})")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", action="store_true", help="print per-milestone and group status")
    parser.add_argument("--json", action="store_true", help="print the complete derived report as JSON")
    args = parser.parse_args(argv)
    try:
        report = validate_repository_milestones()
    except (MilestoneValidationError, OSError, UnicodeDecodeError) as exc:
        print(f"Milestone gate validation: FAIL: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    elif args.report:
        print_report(report)
    else:
        print(
            "Milestone gate validation: PASS "
            f"({report['criteria']} criteria, current={report['currentMilestone']}, "
            f"releaseReady={str(report['releaseReady']).lower()})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
