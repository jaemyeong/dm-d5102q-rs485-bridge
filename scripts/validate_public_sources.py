#!/usr/bin/env python3
"""Validate the immutable public-source lock and optional local archives.

This is a host-only integrity check. It deliberately keeps selected-hardware
identity, pin mapping, component revision, and electrical behavior unverified.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

from target_contract import ACTIVE_TARGET, AMENDMENT


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "docs/cleanroom/public-source-lock.json"
ARCHIVE_ROOT = Path(".arduino/evidence/public-sources/raw")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
EXPECTED_SOURCE_HASHES = {
    "PUB-T002-PAGE-001": "b1c587d397abf4565a8e327cef29f85abaaa6a2d4ba957961439f7cbc06ced50",
    "PUB-T002-BLOCK-DIAGRAM-001": "394dab7fcba7668172b929a0b5da6754bfc87bdb9e389d78660ad788815c1f48",
    "PUB-ATOM-PDF-001": "95d1dd837d4c47f49e29ee13ceb8de41c436131eb7fb86a743b9a2d87197a59c",
    "PUB-A131-PDF-001": "1707cded160830ee9b0653cf2fa349bc7655e1f95307cb8bc657a46b237508b1",
    "PUB-ATOM-SCHEMATIC-001": "807418b50ff5630349cee07e1c8f751a59015112082d8a32073b0f77feadd6c9",
    "PUB-A131-SCHEMATIC-001": "72fa3f8f2f70f0097edc759aa4941778e213c6dfa1130aabdcf80b0b8019cefa",
    "PUB-M5UNIT-README-001": "47410384539cdac981fa7520c50271f30830b2d512c0302cf16b73ed0ef4ca95",
    "PUB-M5UNIT-WIRING-001": "5be77135b2120364bc539d2387676a37bbc48ff430b073895df441dfe58342a5",
    "PUB-A131-LINKED-SP485-DATASHEET-001": "07881f7fed9d4974d4e22df35eb1d5b4f279e78bd44bd3e2627c39a76428b22d",
    "PUB-A131-LINKED-AOZ1282-DATASHEET-001": "61032eaad2d8951adb7a2801598255df83fae32eeab5863b7cc6f1f116fa2cba",
}
REQUIRED_CONFLICT_IDS = {"PIN-CONFLICT-001", "A131-REVISION-CONFLICT-001"}
REQUIRED_INFERENCE_IDS = {"DEC-A131-BOOT-DIRECTION-001"}
ACTIVE_SOURCE_IDS = {
    "PUB-ATOM-PDF-001", "PUB-ATOM-SCHEMATIC-001",
    "PUB-T002-PAGE-001", "PUB-T002-BLOCK-DIAGRAM-001",
}
REQUIRED_T002_GATES = {
    "T002-IDENTITY-CONTINUITY-001", "T002-ELECTRICAL-001",
    "T002-BOOT-PASSIVITY-001",
}


class PublicSourceValidationError(AssertionError):
    """Raised when the source lock weakens or an archive changes."""


def fail(message: str) -> None:
    raise PublicSourceValidationError(message)


def load_manifest(path: Path = MANIFEST_PATH) -> dict[str, Any]:
    if not path.is_file():
        fail(f"missing public-source manifest: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid public-source manifest: {exc}")
    if not isinstance(value, dict):
        fail("public-source manifest must be a JSON object")
    return value


def indexed(items: Any, label: str) -> dict[str, dict[str, Any]]:
    if not isinstance(items, list) or not items:
        fail(f"{label} must be a non-empty list")
    result: dict[str, dict[str, Any]] = {}
    for item in items:
        if not isinstance(item, dict):
            fail(f"{label} contains a non-object entry")
        item_id = item.get("id")
        if not isinstance(item_id, str) or not item_id:
            fail(f"{label} contains a missing or invalid id")
        if item_id in result:
            fail(f"{label} contains duplicate id {item_id}")
        result[item_id] = item
    return result


def validate_archive_path(relative: Any) -> Path:
    if not isinstance(relative, str) or not relative:
        fail("every source must define archivePath")
    path = Path(relative)
    if path.is_absolute() or ".." in path.parts:
        fail(f"unsafe archivePath: {relative}")
    try:
        path.relative_to(ARCHIVE_ROOT)
    except ValueError:
        fail(f"archivePath must stay under {ARCHIVE_ROOT}: {relative}")
    return path


def validate_manifest(
    manifest: dict[str, Any],
    *,
    root: Path = ROOT,
    require_archives: bool = False,
) -> dict[str, int]:
    if manifest.get("schemaVersion") != 1:
        fail("public-source schemaVersion must be 1")
    if manifest.get("hashAlgorithm") != "SHA-256":
        fail("public-source hashAlgorithm must be SHA-256")
    if not manifest.get("capturedAt"):
        fail("public-source manifest lacks capturedAt")
    if manifest.get("designAmendment") != AMENDMENT:
        fail("public-source target amendment changed or is missing")
    if manifest.get("activeTarget") != ACTIVE_TARGET:
        fail("public-source active target must be C008 plus T002")
    active_sources = manifest.get("activeSourceIds")
    if not isinstance(active_sources, list) or set(active_sources) != ACTIVE_SOURCE_IDS or len(active_sources) != 4:
        fail("active T002 source set changed or includes historical A131 sources")

    claims = manifest.get("selectedHardwareClaims")
    if not isinstance(claims, dict):
        fail("selectedHardwareClaims must be an object")
    for field in (
        "confirmedUartPinMap",
        "confirmedTransceiverPart",
        "confirmedInputVoltageRange",
    ):
        if claims.get(field) is not None:
            fail(f"{field} must remain null until exact-hardware evidence resolves it")
    if claims.get("m1Gate") != "blocked":
        fail("M1 must remain blocked by unverified T002 hardware gates")

    sources = indexed(manifest.get("sources"), "sources")
    missing_sources = sorted(set(EXPECTED_SOURCE_HASHES) - set(sources))
    if missing_sources:
        fail(f"missing pinned public sources: {missing_sources}")

    archive_count = 0
    for source_id, source in sources.items():
        expected_scope = (
            "shared-C008" if source_id.startswith("PUB-ATOM-") else
            "active-T002" if source_id in ACTIVE_SOURCE_IDS else "historical-A131"
        )
        if source.get("applicability") != expected_scope:
            fail(f"{source_id} source applicability changed")
        if source.get("classification") != "PUB":
            fail(f"{source_id} classification must be PUB")
        required_metadata = ("title", "publisher", "sourceDate", "retrievedAt", "revision")
        if not all(source.get(field) for field in required_metadata):
            fail(f"{source_id} lacks title/publisher/date/revision metadata")
        uri = source.get("uri")
        if not isinstance(uri, str) or not uri.startswith("https://"):
            fail(f"{source_id} must use an HTTPS source URI")
        digest = source.get("sha256")
        if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
            fail(f"{source_id} has an invalid SHA-256")
        expected_digest = EXPECTED_SOURCE_HASHES.get(source_id)
        if expected_digest is not None and digest != expected_digest:
            fail(f"{source_id} does not match its pinned digest")
        if not isinstance(source.get("facts"), list) or not source["facts"]:
            fail(f"{source_id} has no bounded facts")
        if not isinstance(source.get("limitations"), list) or not source["limitations"]:
            fail(f"{source_id} has no limitations")
        if source.get("mutableUri") is False:
            revision = str(source.get("revision"))
            revisions = [token for token in revision.split() if len(token) == 40]
            if not revisions or not any(token in uri for token in revisions):
                fail(f"{source_id} immutable URI does not contain its revision")

        archive_relative = validate_archive_path(source.get("archivePath"))
        archive = root / archive_relative
        if archive.is_file():
            archive_count += 1
            actual_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            if actual_digest != digest:
                fail(f"{source_id} local archive hash mismatch: {actual_digest}")
        elif require_archives:
            fail(f"{source_id} local archive is required but missing: {archive_relative}")

    conflicts = indexed(manifest.get("conflicts"), "conflicts")
    missing_conflicts = sorted(REQUIRED_CONFLICT_IDS - set(conflicts))
    if missing_conflicts:
        fail(f"missing required conflicts: {missing_conflicts}")
    for conflict_id, conflict in conflicts.items():
        if conflict.get("appliesToBaseSku") != "A131" or conflict.get("applicability") != "historical-non-target":
            fail(f"{conflict_id} must remain historical A131 evidence, not a T002 claim")
        if conflict.get("classification") != "UNV" or conflict.get("status") != "unresolved":
            fail(f"{conflict_id} must remain UNV and unresolved")
        if not isinstance(conflict.get("claims"), list) or len(conflict["claims"]) < 2:
            fail(f"{conflict_id} must retain at least two conflicting claims")
        if not isinstance(conflict.get("verification"), list) or not conflict["verification"]:
            fail(f"{conflict_id} lacks a physical verification method")
        if "M1" not in conflict.get("blockingMilestones", []):
            fail(f"{conflict_id} must block M1")
        if not conflict.get("safetyImpact"):
            fail(f"{conflict_id} lacks safety impact")

    inferences = indexed(manifest.get("inferences"), "inferences")
    missing_inferences = sorted(REQUIRED_INFERENCE_IDS - set(inferences))
    if missing_inferences:
        fail(f"missing required inferences: {missing_inferences}")
    for inference_id, inference in inferences.items():
        if inference.get("appliesToBaseSku") != "A131" or inference.get("applicability") != "historical-non-target":
            fail(f"{inference_id} must remain historical A131 evidence, not a T002 claim")
        if inference.get("classification") != "DEC":
            fail(f"{inference_id} classification must be DEC")
        if inference.get("status") != "requires-hardware-verification":
            fail(f"{inference_id} must require hardware verification")
        if not inference.get("statement") or not inference.get("verification"):
            fail(f"{inference_id} lacks statement or verification")
        if "M1" not in inference.get("blockingMilestones", []):
            fail(f"{inference_id} must block M1")
        unknown_sources = set(inference.get("sourceIds", [])) - set(sources)
        if unknown_sources:
            fail(f"{inference_id} references unknown sources: {sorted(unknown_sources)}")

    hardware_gates = indexed(manifest.get("unverifiedGates"), "T002 hardware gates")
    if set(hardware_gates) != REQUIRED_T002_GATES:
        fail("required T002 hardware gates changed or are missing")
    for gate_id, gate in hardware_gates.items():
        if gate.get("classification") != "UNV" or gate.get("status") != "unverified":
            fail(f"{gate_id} must remain UNV and unverified")
        if gate.get("baseSku") != "T002" or "M1" not in gate.get("blockingMilestones", []):
            fail(f"{gate_id} must block M1 for T002")
        if not gate.get("statement") or not isinstance(gate.get("verification"), list) or not gate["verification"]:
            fail(f"{gate_id} lacks a physical verification method")
        if not isinstance(gate.get("sourceIds"), list) or not gate["sourceIds"] or not set(gate["sourceIds"]) <= ACTIVE_SOURCE_IDS:
            fail(f"{gate_id} must reference current C008/T002 sources only")

    return {
        "sources": len(sources),
        "active_sources": len(ACTIVE_SOURCE_IDS),
        "hardware_gates": len(hardware_gates),
        "conflicts": len(conflicts),
        "inferences": len(inferences),
        "archives": archive_count,
    }


def validate_repository_public_sources(*, require_archives: bool = False) -> dict[str, int]:
    return validate_manifest(load_manifest(), require_archives=require_archives)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--require-archives",
        action="store_true",
        help="fail unless every locally ignored raw source snapshot is present",
    )
    args = parser.parse_args()
    try:
        metrics = validate_repository_public_sources(require_archives=args.require_archives)
    except PublicSourceValidationError as exc:
        print(f"public-source validation: FAIL: {exc}", file=sys.stderr)
        return 1

    print(
        "public-source validation: PASS "
        f"({metrics['sources']} sources, {metrics['active_sources']} active C008/T002 sources, "
        f"{metrics['conflicts']} historical A131 conflicts, {metrics['hardware_gates']} T002 hardware gates, "
        f"{metrics['archives']} local archives)"
    )
    print(
        "Proof boundary: source identity and integrity only; selected hardware, "
        "pin continuity, power limits, and boot-time bus passivity remain UNV"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
