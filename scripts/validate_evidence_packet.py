#!/usr/bin/env python3
"""Validate M0 physical evidence packets and paired RX-only captures.

The validator proves format, provenance fields, artifact integrity, and gate
criteria. It cannot prove that a photograph or measurement is truthful; that
requires independent review and remains a separate evidence stage.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

from target_contract import AMENDMENT

ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = ROOT / "requirements/evidence-packet-contract.json"
IDENTITY_TEMPLATE_PATH = ROOT / "docs/cleanroom/identity-photo-packet.template.json"
CAPTURE_TEMPLATE_PATH = ROOT / "docs/cleanroom/passive-capture.template.ndjson"
DESIGN_SHA256 = "d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114"
PRIVATE_ROOT = Path(".arduino/evidence/hardware/private")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
PACKET_ID_RE = re.compile(r"^EVP-[A-Z0-9][A-Z0-9._-]{7,63}$")
CAPTURE_ID_RE = re.compile(r"^CAP-[A-Z0-9][A-Z0-9._-]{7,63}$")
HEX_BYTES_RE = re.compile(r"^[0-9A-F]{2}(?: [0-9A-F]{2})*$")
PACKET_TYPES = {"identity-photo", "continuity", "approval", "passive-capture"}
IDENTITY_ROLES = {
    "atom-front",
    "atom-back",
    "base-front",
    "base-back",
    "base-terminal",
    "base-connector",
}
IDENTITY_CLAIM_FIELDS = {
    "target.atom.sku",
    "target.atom.pcbRevision",
    "target.atom.connectorOrientation",
    "target.base.sku",
    "target.base.pcbRevision",
    "target.base.connectorOrientation",
    "target.base.transceiverMarking",
    "target.base.terminationPopulation",
    "target.base.terminalLabels",
}
CONTINUITY_MATRIX = {("RX", 32), ("TX", 26), ("RX", 26), ("TX", 32)}
CAPTURE_SOURCES = {"mcu-uart", "logic-analyzer"}
CAPTURE_MINIMUM_MS = {
    "field-passive": 1_800_000,
    "bench-loss-115200": 3_600_000,
}
MAX_CAPTURE_RECORD_BYTES = 256
MEDIA_TYPES = {
    "image/jpeg",
    "image/png",
    "image/heic",
    "image/webp",
    "application/x-ndjson",
    "text/plain",
}


class EvidencePacketValidationError(AssertionError):
    """Raised when an evidence contract, packet, or capture fails closed."""


def fail(message: str) -> None:
    raise EvidencePacketValidationError(message)


def load_json_object(path: Path, label: str) -> dict[str, Any]:
    if not path.is_file():
        fail(f"missing {label}: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid {label}: {exc}")
    if not isinstance(value, dict):
        fail(f"{label} must be a JSON object")
    return value


def load_contract(path: Path = CONTRACT_PATH) -> dict[str, Any]:
    return load_json_object(path, "evidence packet contract")


def load_packet(path: Path = IDENTITY_TEMPLATE_PATH) -> dict[str, Any]:
    return load_json_object(path, "evidence packet")


def require_object(parent: dict[str, Any], key: str, label: str) -> dict[str, Any]:
    value = parent.get(key)
    if not isinstance(value, dict):
        fail(f"{label}.{key} must be an object")
    return value


def require_list(parent: dict[str, Any], key: str, label: str) -> list[Any]:
    value = parent.get(key)
    if not isinstance(value, list):
        fail(f"{label}.{key} must be a list")
    return value


def require_nonempty_string(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        fail(f"{label} must be a non-empty string")
    return value


def require_nonnegative_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        fail(f"{label} must be a non-negative integer")
    return value


def require_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        fail(f"{label} must be a lowercase SHA-256")
    return value


def parse_timestamp(value: Any, label: str) -> datetime:
    if not isinstance(value, str) or not value:
        fail(f"{label} must be an offset-aware ISO-8601 timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        fail(f"{label} is not ISO-8601: {exc}")
    if parsed.utcoffset() is None:
        fail(f"{label} must include a UTC offset")
    return parsed


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_contract(contract: dict[str, Any]) -> dict[str, int]:
    if contract.get("schemaVersion") != 1:
        fail("evidence contract schemaVersion must be 1")
    if contract.get("designSha256") != DESIGN_SHA256:
        fail("evidence contract points to the wrong governing design")
    if contract.get("designAmendment") != AMENDMENT:
        fail("evidence contract target amendment changed or is missing")
    if contract.get("packetSchemaVersion") != 1 or contract.get("captureSchemaVersion") != 1:
        fail("packet and capture schema versions must both be 1")

    path_policy = require_object(contract, "pathPolicy", "contract")
    if path_policy.get("privateRoot") != PRIVATE_ROOT.as_posix():
        fail("evidence privateRoot changed")
    if path_policy.get("rawEvidenceCommitted") is not False:
        fail("raw evidence must remain uncommitted")

    packet_types = require_object(contract, "packetTypes", "contract")
    if set(packet_types) != PACKET_TYPES:
        fail("evidence contract packet types changed")

    identity = require_object(packet_types, "identity-photo", "packetTypes")
    if identity.get("proofStage") != "hardware":
        fail("identity-photo proof stage must be hardware")
    if set(identity.get("requiredArtifactRoles", [])) != IDENTITY_ROLES:
        fail("identity-photo artifact roles changed")
    if identity.get("requiredPower") != {
        "usb": "disconnected",
        "field": "disconnected",
        "bench": "disconnected",
    }:
        fail("identity-photo must require all power disconnected")
    if identity.get("requiredWiring") != {
        "atomBaseMated": False,
        "fieldBusConnected": False,
    }:
        fail("identity-photo must require separated hardware and field bus")
    if set(identity.get("allowedClaimFields", [])) != IDENTITY_CLAIM_FIELDS:
        fail("identity-photo claim field boundary changed")

    continuity = require_object(packet_types, "continuity", "packetTypes")
    matrix = {
        (item.get("baseNet"), item.get("gpio"))
        for item in continuity.get("candidateMatrix", [])
        if isinstance(item, dict)
    }
    if matrix != CONTINUITY_MATRIX or len(continuity.get("candidateMatrix", [])) != 4:
        fail("continuity candidate matrix changed")
    if continuity.get("autoResolutionPermitted") is not False:
        fail("continuity evidence must not auto-resolve the pin map")
    if continuity.get("requiredPower") != identity.get("requiredPower"):
        fail("continuity must require every power source disconnected")

    capture = require_object(contract, "capture", "contract")
    if set(capture.get("sources", [])) != CAPTURE_SOURCES:
        fail("capture sources must be MCU UART and independent logic analyzer")
    kinds = require_object(capture, "kinds", "capture")
    actual_minimums = {
        name: value.get("minimumDurationMs")
        for name, value in kinds.items()
        if isinstance(value, dict)
    }
    if actual_minimums != CAPTURE_MINIMUM_MS:
        fail("capture minimum durations changed")
    if capture.get("txPolicy") != "prohibited":
        fail("capture TX policy must remain prohibited")
    if capture.get("maximumRecordBytes") != MAX_CAPTURE_RECORD_BYTES:
        fail("capture maximumRecordBytes must remain 256")

    invariants = contract.get("invariants")
    if not isinstance(invariants, list) or len(invariants) < 7:
        fail("evidence contract invariants are incomplete")
    return {"packet_types": len(packet_types), "capture_kinds": len(kinds)}


def validate_private_path(relative: Any, *, root: Path) -> tuple[Path, Path]:
    if not isinstance(relative, str) or not relative:
        fail("artifact path must be a non-empty repository-relative path")
    path = Path(relative)
    if path.is_absolute() or ".." in path.parts:
        fail(f"unsafe artifact path: {relative}")
    try:
        path.relative_to(PRIVATE_ROOT)
    except ValueError:
        fail(f"artifact path must stay under {PRIVATE_ROOT}: {relative}")

    private_absolute = (root / PRIVATE_ROOT).resolve()
    artifact = root / path
    resolved = artifact.resolve()
    try:
        resolved.relative_to(private_absolute)
    except ValueError:
        fail(f"artifact resolves outside the private root: {relative}")
    return artifact, path


def validate_artifacts(
    packet: dict[str, Any],
    rule: dict[str, Any],
    *,
    contract: dict[str, Any],
    root: Path,
    template: bool,
    require_artifacts: bool,
) -> tuple[set[str], set[str], dict[str, dict[str, Any]]]:
    del contract  # Contract validation already pins the path and role rules.
    artifacts = require_list(packet, "artifacts", "packet")
    by_id: dict[str, dict[str, Any]] = {}
    roles: set[str] = set()
    for index, artifact in enumerate(artifacts):
        if not isinstance(artifact, dict):
            fail(f"artifact {index} must be an object")
        artifact_id = require_nonempty_string(artifact.get("id"), f"artifact {index}.id")
        if artifact_id in by_id:
            fail(f"duplicate artifact id: {artifact_id}")
        artifact_roles = artifact.get("roles")
        if not isinstance(artifact_roles, list) or not artifact_roles:
            fail(f"artifact {artifact_id} must have at least one role")
        if any(not isinstance(role, str) or not role for role in artifact_roles):
            fail(f"artifact {artifact_id} has an invalid role")
        if len(set(artifact_roles)) != len(artifact_roles):
            fail(f"artifact {artifact_id} has duplicate roles")
        roles.update(artifact_roles)
        by_id[artifact_id] = artifact

        if template:
            for field in ("path", "sha256", "bytes", "mediaType", "capturedAt"):
                if artifact.get(field) is not None:
                    fail(f"template artifact {artifact_id}.{field} must be null")
            continue

        artifact_path, relative = validate_private_path(artifact.get("path"), root=root)
        digest = require_sha256(artifact.get("sha256"), f"artifact {artifact_id}.sha256")
        size = require_nonnegative_int(artifact.get("bytes"), f"artifact {artifact_id}.bytes")
        if size == 0:
            fail(f"artifact {artifact_id} must not be empty")
        if artifact.get("mediaType") not in MEDIA_TYPES:
            fail(f"artifact {artifact_id} has unsupported mediaType")
        parse_timestamp(artifact.get("capturedAt"), f"artifact {artifact_id}.capturedAt")

        if require_artifacts:
            if artifact_path.is_symlink():
                fail(f"artifact {artifact_id} must not be a symlink")
            if not artifact_path.is_file():
                fail(f"artifact {artifact_id} is missing: {relative}")
            actual_size = artifact_path.stat().st_size
            if actual_size != size:
                fail(f"artifact {artifact_id} size mismatch: {actual_size}")
            actual_digest = hash_file(artifact_path)
            if actual_digest != digest:
                fail(f"artifact {artifact_id} SHA-256 mismatch: {actual_digest}")

    required_roles = set(rule.get("requiredArtifactRoles", []))
    missing_roles = sorted(required_roles - roles)
    if missing_roles:
        fail(f"packet is missing required artifact roles: {missing_roles}")
    return set(by_id), roles, by_id


def validate_claims(
    packet: dict[str, Any],
    *,
    allowed_fields: set[str],
    artifact_ids: set[str],
    template: bool,
) -> dict[str, Any]:
    claims = require_list(packet, "claims", "packet")
    if template and claims:
        fail("template packet must not contain claims")
    by_field: dict[str, Any] = {}
    claim_ids: set[str] = set()
    for index, claim in enumerate(claims):
        if not isinstance(claim, dict):
            fail(f"claim {index} must be an object")
        claim_id = require_nonempty_string(claim.get("id"), f"claim {index}.id")
        if claim_id in claim_ids:
            fail(f"duplicate claim id: {claim_id}")
        claim_ids.add(claim_id)
        field = claim.get("field")
        if field not in allowed_fields:
            fail(f"claim {claim_id} is not allowed to assert {field}")
        if field in by_field:
            fail(f"multiple claims assert the same field: {field}")
        if claim.get("classification") != "OBS":
            fail(f"claim {claim_id} classification must be OBS")
        require_nonempty_string(claim.get("method"), f"claim {claim_id}.method")
        if claim.get("value") is None:
            fail(f"claim {claim_id} value must not be null")
        references = claim.get("artifactIds")
        if not isinstance(references, list) or not references:
            fail(f"claim {claim_id} must reference artifacts")
        unknown = set(references) - artifact_ids
        if unknown:
            fail(f"claim {claim_id} references unknown artifacts: {sorted(unknown)}")
        by_field[field] = claim.get("value")
    return by_field


def validate_conditions(
    packet: dict[str, Any],
    rule: dict[str, Any],
    *,
    template: bool,
) -> None:
    conditions = require_object(packet, "conditions", "packet")
    power = require_object(conditions, "power", "conditions")
    wiring = require_object(conditions, "wiring", "conditions")
    if template:
        for key in ("usb", "field", "bench", "verificationMethod"):
            if power.get(key) is not None:
                fail(f"template power.{key} must be null")
        for key in ("atomBaseMated", "fieldBusConnected"):
            if wiring.get(key) is not None:
                fail(f"template wiring.{key} must be null")
        return

    for key, expected in rule.get("requiredPower", {}).items():
        if power.get(key) != expected:
            fail(f"power.{key} must be {expected}")
    require_nonempty_string(power.get("verificationMethod"), "power.verificationMethod")
    for key, expected in rule.get("requiredWiring", {}).items():
        if wiring.get(key) is not expected:
            fail(f"wiring.{key} must be {expected}")
    for key in ("atomBaseMated", "fieldBusConnected"):
        if not isinstance(wiring.get(key), bool):
            fail(f"wiring.{key} must be boolean")


def observation_present(value: Any) -> bool:
    if value is None:
        return False
    if isinstance(value, str) and value.strip().lower() in {
        "unverified",
        "not-visible",
        "not visible",
        "unknown",
    }:
        return False
    return not (isinstance(value, (list, dict, str)) and not value)


def validate_identity_packet(
    packet: dict[str, Any],
    rule: dict[str, Any],
    *,
    artifact_ids: set[str],
    template: bool,
) -> bool:
    validate_conditions(packet, rule, template=template)
    content = require_object(packet, "content", "packet")
    atom = require_object(content, "atom", "content")
    base = require_object(content, "base", "content")
    unverified_raw = require_list(packet, "unverified", "packet")
    if any(not isinstance(field, str) or not field for field in unverified_raw):
        fail("packet.unverified contains an invalid field")
    if len(set(unverified_raw)) != len(unverified_raw):
        fail("packet.unverified contains duplicates")
    unverified = set(unverified_raw)
    if not unverified <= IDENTITY_CLAIM_FIELDS:
        fail(f"identity packet has unknown UNV fields: {sorted(unverified - IDENTITY_CLAIM_FIELDS)}")

    if template:
        null_fields = (
            atom.get("observedSku"),
            atom.get("pcbRevision"),
            atom.get("connectorOrientation"),
            base.get("observedSku"),
            base.get("pcbRevision"),
            base.get("connectorOrientation"),
            base.get("transceiverMarking"),
        )
        if any(value is not None for value in null_fields):
            fail("identity template observations must remain null")
        if atom.get("markings") != [] or base.get("markings") != []:
            fail("identity template markings must remain empty")
        if base.get("terminalLabels") != [] or base.get("terminationPopulation") != "unverified":
            fail("identity template base observations must remain unverified")
        if unverified != IDENTITY_CLAIM_FIELDS:
            fail("identity template must enumerate every claim field as UNV")
        return False

    for name, value in (
        ("content.atom.observedSku", atom.get("observedSku")),
        ("content.atom.pcbRevision", atom.get("pcbRevision")),
        ("content.atom.connectorOrientation", atom.get("connectorOrientation")),
        ("content.base.observedSku", base.get("observedSku")),
        ("content.base.pcbRevision", base.get("pcbRevision")),
        ("content.base.connectorOrientation", base.get("connectorOrientation")),
        ("content.base.transceiverMarking", base.get("transceiverMarking")),
    ):
        if value is not None and (not isinstance(value, str) or not value.strip()):
            fail(f"{name} must be null or a non-empty string")
    if base.get("terminationPopulation") not in {
        "populated",
        "unpopulated",
        "not-visible",
        "unverified",
    }:
        fail("content.base.terminationPopulation has an invalid value")
    for name, values in (
        ("content.atom.markings", atom.get("markings")),
        ("content.base.markings", base.get("markings")),
        ("content.base.terminalLabels", base.get("terminalLabels")),
    ):
        if not isinstance(values, list) or any(not isinstance(value, str) or not value for value in values):
            fail(f"{name} must be a list of non-empty strings")

    claims = validate_claims(
        packet,
        allowed_fields=IDENTITY_CLAIM_FIELDS,
        artifact_ids=artifact_ids,
        template=False,
    )
    observations = {
        "target.atom.sku": atom.get("observedSku"),
        "target.atom.pcbRevision": atom.get("pcbRevision"),
        "target.atom.connectorOrientation": atom.get("connectorOrientation"),
        "target.base.sku": base.get("observedSku"),
        "target.base.pcbRevision": base.get("pcbRevision"),
        "target.base.connectorOrientation": base.get("connectorOrientation"),
        "target.base.transceiverMarking": base.get("transceiverMarking"),
        "target.base.terminationPopulation": base.get("terminationPopulation"),
        "target.base.terminalLabels": base.get("terminalLabels"),
    }
    for field, value in observations.items():
        present = observation_present(value)
        if present and claims.get(field) != value:
            fail(f"observed {field} must have an equal OBS claim")
        if present and field in unverified:
            fail(f"{field} cannot be both observed and UNV")
        if not present and field not in unverified:
            fail(f"missing observation {field} must remain UNV")
        if not present and field in claims:
            fail(f"unobserved {field} must not have an OBS claim")

    labels = {label.strip().upper() for label in base.get("terminalLabels", [])}
    return all(
        (
            atom.get("observedSku") == "C008",
            base.get("observedSku") == "T002",
            observation_present(atom.get("pcbRevision")),
            observation_present(base.get("pcbRevision")),
            observation_present(atom.get("connectorOrientation")),
            observation_present(base.get("connectorOrientation")),
            {"A", "B"} <= labels,
        )
    )


def validate_continuity_packet(
    packet: dict[str, Any],
    rule: dict[str, Any],
    *,
    artifact_ids: set[str],
) -> bool:
    validate_conditions(packet, rule, template=False)
    if require_list(packet, "claims", "packet"):
        fail("raw continuity packets must not assert interpreted claims")
    content = require_object(packet, "content", "packet")
    identity_ref = require_object(content, "identityPacketRef", "content")
    require_nonempty_string(identity_ref.get("packetId"), "identityPacketRef.packetId")
    require_sha256(identity_ref.get("sha256"), "identityPacketRef.sha256")
    meter = require_object(content, "meter", "content")
    for field in ("manufacturerModel", "serial", "mode"):
        require_nonempty_string(meter.get(field), f"meter.{field}")
    lead_resistance = meter.get("leadResistanceOhms")
    if isinstance(lead_resistance, bool) or not isinstance(lead_resistance, (int, float)) or lead_resistance < 0:
        fail("meter.leadResistanceOhms must be a non-negative number")

    measurements = require_list(content, "measurements", "content")
    found: set[tuple[str, int]] = set()
    review_ready = True
    for index, measurement in enumerate(measurements):
        if not isinstance(measurement, dict):
            fail(f"continuity measurement {index} must be an object")
        pair = (measurement.get("baseNet"), measurement.get("gpio"))
        if pair not in CONTINUITY_MATRIX:
            fail(f"continuity measurement {index} is outside the candidate matrix")
        if pair in found:
            fail(f"duplicate continuity candidate: {pair}")
        found.add(pair)
        result = measurement.get("result")
        if result not in {"continuity", "open", "ambiguous"}:
            fail(f"continuity measurement {pair} has an invalid result")
        if result == "ambiguous":
            review_ready = False
        resistance = measurement.get("resistanceOhms")
        if resistance is not None and (
            isinstance(resistance, bool)
            or not isinstance(resistance, (int, float))
            or resistance < 0
        ):
            fail(f"continuity measurement {pair} has invalid resistanceOhms")
        if result == "continuity" and resistance is None:
            fail(f"continuity measurement {pair} requires resistanceOhms")
        require_nonempty_string(measurement.get("meterMode"), f"measurement {pair}.meterMode")
        references = measurement.get("artifactIds")
        if not isinstance(references, list) or not references:
            fail(f"continuity measurement {pair} must reference artifacts")
        unknown = set(references) - artifact_ids
        if unknown:
            fail(f"continuity measurement {pair} references unknown artifacts: {sorted(unknown)}")

    if found != CONTINUITY_MATRIX:
        fail(f"continuity packet does not cover the exact candidate matrix: {sorted(CONTINUITY_MATRIX - found)}")
    if content.get("confirmedPinMap") is not None:
        fail("raw continuity evidence cannot confirm a UART pin map")
    if content.get("reviewStatus") != "pending-independent-review":
        fail("continuity reviewStatus must remain pending-independent-review")
    unverified = set(require_list(packet, "unverified", "packet"))
    if "selectedUartPinMap" not in unverified:
        fail("selectedUartPinMap must remain UNV in a raw continuity packet")
    return review_ready


def validate_approval_packet(packet: dict[str, Any], rule: dict[str, Any]) -> bool:
    if require_list(packet, "claims", "packet"):
        fail("approval packet must not contain OBS claims")
    content = require_object(packet, "content", "packet")
    role = content.get("role")
    if role not in set(rule.get("roles", [])):
        fail("approval role is invalid")
    decision = content.get("decision")
    if decision not in set(rule.get("decisions", [])):
        fail("approval decision is invalid")
    scopes = content.get("scopes")
    if not isinstance(scopes, list) or not scopes or len(set(scopes)) != len(scopes):
        fail("approval scopes must be a non-empty unique list")
    unknown_scopes = set(scopes) - set(rule.get("scopes", []))
    if unknown_scopes:
        fail(f"approval contains unknown scopes: {sorted(unknown_scopes)}")
    signer_id = require_nonempty_string(content.get("signerId"), "approval.signerId")
    parse_timestamp(content.get("signedAt"), "approval.signedAt")
    operator = require_object(packet, "operator", "packet")
    if operator.get("id") != signer_id:
        fail("approval signerId must equal packet operator.id")
    if operator.get("role") != role:
        fail("approval role must equal packet operator.role")
    conditions = content.get("conditions")
    if not isinstance(conditions, list) or any(not isinstance(item, str) or not item for item in conditions):
        fail("approval conditions must be a list of strings")
    evidence_refs = content.get("evidenceRefs")
    if not isinstance(evidence_refs, list):
        fail("approval evidenceRefs must be a list")
    for index, reference in enumerate(evidence_refs):
        if not isinstance(reference, dict):
            fail(f"approval evidenceRef {index} must be an object")
        require_nonempty_string(reference.get("packetId"), f"evidenceRef {index}.packetId")
        require_sha256(reference.get("sha256"), f"evidenceRef {index}.sha256")
    if decision == "approved" and not evidence_refs:
        fail("an approval must reference evidence packets")
    return decision == "approved"


def validate_passive_capture_packet(
    packet: dict[str, Any],
    rule: dict[str, Any],
    *,
    contract: dict[str, Any],
    artifacts: dict[str, dict[str, Any]],
    root: Path,
    require_artifacts: bool,
    require_gate_ready: bool,
) -> bool:
    if require_list(packet, "claims", "packet"):
        fail("passive-capture packet must not contain interpreted claims")
    conditions = require_object(packet, "conditions", "packet")
    for key, expected in rule.get("requiredConditions", {}).items():
        if conditions.get(key) != expected:
            fail(f"passive-capture condition {key} must be {expected}")
    content = require_object(packet, "content", "packet")
    for field in ("identityPacketSha256", "wiringPacketSha256", "firmwareSha256"):
        require_sha256(content.get(field), f"content.{field}")
    capture_path = content.get("capturePath")
    _, relative = validate_private_path(capture_path, root=root)
    matching = [
        artifact
        for artifact in artifacts.values()
        if artifact.get("path") == relative.as_posix()
        and "paired-capture-ndjson" in artifact.get("roles", [])
    ]
    if len(matching) != 1:
        fail("passive-capture packet must bind capturePath to one paired-capture artifact")
    if not require_artifacts:
        return False
    metrics = validate_capture_file(
        root / relative,
        contract,
        allow_template=False,
        require_complete=require_gate_ready,
    )
    return bool(metrics["ready"])


def validate_packet(
    packet: dict[str, Any],
    contract: dict[str, Any],
    *,
    root: Path = ROOT,
    allow_template: bool = False,
    require_artifacts: bool = False,
    require_gate_ready: bool = False,
) -> dict[str, Any]:
    validate_contract(contract)
    if packet.get("schemaVersion") != 1:
        fail("packet schemaVersion must be 1")
    if packet.get("designSha256") != DESIGN_SHA256:
        fail("packet points to the wrong governing design")
    if packet.get("designAmendmentId") != AMENDMENT["id"]:
        fail("packet target amendment is missing or incorrect")
    packet_type = packet.get("packetType")
    if packet_type not in PACKET_TYPES:
        fail(f"unknown packetType: {packet_type}")
    rule = require_object(require_object(contract, "packetTypes", "contract"), packet_type, "packetTypes")
    if packet.get("proofStage") != rule.get("proofStage"):
        fail(f"{packet_type} proofStage must be {rule.get('proofStage')}")

    status = packet.get("status")
    template = status == "template"
    if template:
        if not allow_template:
            fail("template packet is not evidence")
        if packet_type != "identity-photo":
            fail("only the repository identity-photo template is supported")
        if packet.get("packetId") != "TEMPLATE-NOT-EVIDENCE":
            fail("template packetId must be TEMPLATE-NOT-EVIDENCE")
        if packet.get("capturedAt") is not None:
            fail("template capturedAt must be null")
    else:
        if status != "captured":
            fail("actual packet status must be captured")
        packet_id = packet.get("packetId")
        if not isinstance(packet_id, str) or not PACKET_ID_RE.fullmatch(packet_id):
            fail("actual packetId has an invalid format")
        parse_timestamp(packet.get("capturedAt"), "packet.capturedAt")

    operator = require_object(packet, "operator", "packet")
    if operator.get("role") != "evidence" and packet_type != "approval":
        fail("physical evidence packet operator.role must be evidence")
    if template:
        if operator.get("id") is not None:
            fail("template operator.id must be null")
    else:
        require_nonempty_string(operator.get("id"), "operator.id")
        require_nonempty_string(operator.get("role"), "operator.role")

    target = require_object(packet, "target", "packet")
    if target.get("declaredAtomSku") != "C008" or target.get("declaredBaseSku") != "T002":
        fail("packet target declaration must be C008 plus T002")

    artifact_ids, _, artifacts = validate_artifacts(
        packet,
        rule,
        contract=contract,
        root=root,
        template=template,
        require_artifacts=require_artifacts,
    )

    if packet_type == "identity-photo":
        ready = validate_identity_packet(
            packet,
            rule,
            artifact_ids=artifact_ids,
            template=template,
        )
    elif template:
        fail("non-identity packet cannot be a template")
    elif packet_type == "continuity":
        ready = validate_continuity_packet(
            packet,
            rule,
            artifact_ids=artifact_ids,
        )
    elif packet_type == "approval":
        ready = validate_approval_packet(packet, rule)
    else:
        ready = validate_passive_capture_packet(
            packet,
            rule,
            contract=contract,
            artifacts=artifacts,
            root=root,
            require_artifacts=require_artifacts,
            require_gate_ready=require_gate_ready,
        )

    if require_gate_ready and not ready:
        fail(f"{packet_type} packet is valid evidence but does not satisfy its gate")
    return {
        "packet_type": packet_type,
        "template": template,
        "artifacts": len(artifacts),
        "claims": len(packet.get("claims", [])),
        "ready": ready,
    }


def validate_capture_header(header: dict[str, Any], contract: dict[str, Any]) -> bool:
    if header.get("recordType") != "header" or header.get("schemaVersion") != 1:
        fail("capture first record must be a schemaVersion 1 header")
    if header.get("designSha256") != DESIGN_SHA256:
        fail("capture points to the wrong governing design")
    if header.get("designAmendmentId") != AMENDMENT["id"]:
        fail("capture target amendment is missing or incorrect")
    if header.get("captureKind") not in CAPTURE_MINIMUM_MS:
        fail("captureKind is unsupported")
    if header.get("timebase") != "monotonic-us" or header.get("txPolicy") != "prohibited":
        fail("capture timebase or TX policy changed")
    sources = header.get("sources")
    if not isinstance(sources, list) or set(sources) != CAPTURE_SOURCES or len(sources) != 2:
        fail("capture must contain exactly MCU UART and logic analyzer sources")

    template = header.get("status") == "template"
    if template:
        if header.get("captureId") != "TEMPLATE-NOT-EVIDENCE":
            fail("capture template id must be TEMPLATE-NOT-EVIDENCE")
        for field in (
            "startedAt",
            "firmwareSha256",
            "identityPacketSha256",
            "wiringPacketSha256",
        ):
            if header.get(field) is not None:
                fail(f"capture template {field} must be null")
        uart = require_object(header, "uart", "capture header")
        if any(uart.get(field) is not None for field in ("baud", "dataBits", "parity", "stopBits")):
            fail("capture template UART values must be null")
        if uart.get("framing") != "raw":
            fail("capture framing must be raw")
        return True

    if header.get("status") != "captured":
        fail("actual capture status must be captured")
    capture_id = header.get("captureId")
    if not isinstance(capture_id, str) or not CAPTURE_ID_RE.fullmatch(capture_id):
        fail("actual captureId has an invalid format")
    parse_timestamp(header.get("startedAt"), "capture.startedAt")
    for field in ("firmwareSha256", "identityPacketSha256", "wiringPacketSha256"):
        require_sha256(header.get(field), f"capture.{field}")
    uart = require_object(header, "uart", "capture header")
    baud = uart.get("baud")
    if isinstance(baud, bool) or not isinstance(baud, int) or not 300 <= baud <= 921_600:
        fail("capture UART baud is outside 300..921600")
    if uart.get("dataBits") not in {5, 6, 7, 8}:
        fail("capture UART dataBits is invalid")
    if uart.get("parity") not in {"none", "even", "odd"}:
        fail("capture UART parity is invalid")
    if uart.get("stopBits") not in {1, 2}:
        fail("capture UART stopBits is invalid")
    if uart.get("framing") != "raw":
        fail("capture framing must be raw")
    return False


def validate_capture_file(
    path: Path,
    contract: dict[str, Any],
    *,
    allow_template: bool = False,
    require_complete: bool = False,
) -> dict[str, Any]:
    validate_contract(contract)
    if not path.is_file():
        fail(f"capture file is missing: {path}")

    header: dict[str, Any] | None = None
    trailer: dict[str, Any] | None = None
    template = False
    line_count = 0
    record_counts = {source: 0 for source in CAPTURE_SOURCES}
    byte_counts = {source: 0 for source in CAPTURE_SOURCES}
    hashers = {source: hashlib.sha256() for source in CAPTURE_SOURCES}
    expected_sequence = {source: 0 for source in CAPTURE_SOURCES}
    last_timestamp = {source: -1 for source in CAPTURE_SOURCES}
    last_errors = {
        source: {"frame": 0, "parity": 0, "overflow": 0}
        for source in CAPTURE_SOURCES
    }

    with path.open("r", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            line_count += 1
            if not line.strip():
                fail(f"blank capture record at line {line_number}")
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                fail(f"invalid capture JSON at line {line_number}: {exc}")
            if not isinstance(record, dict):
                fail(f"capture line {line_number} must be an object")

            if line_number == 1:
                header = record
                template = validate_capture_header(header, contract)
                if template and not allow_template:
                    fail("capture template is not evidence")
                continue
            if trailer is not None:
                fail(f"capture has a record after its trailer at line {line_number}")
            record_type = record.get("recordType")
            if record_type == "trailer":
                trailer = record
                continue
            if record_type != "rx":
                fail(f"capture line {line_number} must be rx or trailer")
            if template:
                fail("capture template must not contain RX records")

            source = record.get("source")
            if source not in CAPTURE_SOURCES:
                fail(f"capture line {line_number} has an invalid source")
            sequence = require_nonnegative_int(record.get("seq"), f"line {line_number}.seq")
            if sequence != expected_sequence[source]:
                fail(f"capture {source} sequence discontinuity at line {line_number}")
            expected_sequence[source] += 1
            timestamp = require_nonnegative_int(
                record.get("tMonotonicUs"), f"line {line_number}.tMonotonicUs"
            )
            if timestamp < last_timestamp[source]:
                fail(f"capture {source} timestamp moved backwards at line {line_number}")
            last_timestamp[source] = timestamp
            bytes_hex = record.get("bytesHex")
            if not isinstance(bytes_hex, str) or not HEX_BYTES_RE.fullmatch(bytes_hex):
                fail(f"capture line {line_number} bytesHex is not canonical uppercase hex")
            payload = bytes.fromhex(bytes_hex)
            if not payload or payload.hex(" ").upper() != bytes_hex:
                fail(f"capture line {line_number} contains invalid or empty bytes")
            if len(payload) > MAX_CAPTURE_RECORD_BYTES:
                fail(
                    f"capture line {line_number} exceeds "
                    f"{MAX_CAPTURE_RECORD_BYTES} bytes"
                )

            errors = require_object(record, "uartErrors", f"capture line {line_number}")
            if set(errors) != {"frame", "parity", "overflow"}:
                fail(f"capture line {line_number} uartErrors fields changed")
            for field in ("frame", "parity", "overflow"):
                value = require_nonnegative_int(errors.get(field), f"line {line_number}.uartErrors.{field}")
                if value < last_errors[source][field]:
                    fail(f"capture {source} cumulative {field} counter decreased")
                last_errors[source][field] = value

            record_counts[source] += 1
            byte_counts[source] += len(payload)
            hashers[source].update(payload)

    if header is None or trailer is None or line_count < 2:
        fail("capture must contain one header and one final trailer")
    if trailer.get("recordType") != "trailer":
        fail("capture final record must be a trailer")

    if template:
        if line_count != 2:
            fail("capture template must contain exactly header and trailer")
        if trailer.get("endedAt") is not None or trailer.get("durationMs") is not None:
            fail("capture template end time and duration must be null")
        if trailer.get("recordsBySource") != record_counts or trailer.get("bytesBySource") != byte_counts:
            fail("capture template counts must be zero")
        if trailer.get("sha256BySource") != {
            "mcu-uart": None,
            "logic-analyzer": None,
        }:
            fail("capture template source hashes must be null")
        if trailer.get("txFrames") != 0 or trailer.get("busFaults") != 0:
            fail("capture template must declare zero TX and bus faults")
        return {
            "template": True,
            "records": 0,
            "bytes": 0,
            "duration_ms": 0,
            "ready": False,
            "reasons": ["template is not evidence"],
        }

    started_at = parse_timestamp(header.get("startedAt"), "capture.startedAt")
    ended_at = parse_timestamp(trailer.get("endedAt"), "capture.endedAt")
    if ended_at < started_at:
        fail("capture endedAt precedes startedAt")
    duration_ms = require_nonnegative_int(trailer.get("durationMs"), "trailer.durationMs")
    wall_duration_ms = round((ended_at - started_at).total_seconds() * 1000)
    if abs(wall_duration_ms - duration_ms) > 5000:
        fail("capture wall-clock and monotonic durations differ by more than 5 seconds")
    if any(value > duration_ms * 1000 + 1_000_000 for value in last_timestamp.values()):
        fail("capture monotonic RX timestamp exceeds declared duration")

    computed_hashes = {source: hashers[source].hexdigest() for source in CAPTURE_SOURCES}
    if trailer.get("recordsBySource") != record_counts:
        fail("capture trailer record counts do not match the stream")
    if trailer.get("bytesBySource") != byte_counts:
        fail("capture trailer byte counts do not match the stream")
    if trailer.get("sha256BySource") != computed_hashes:
        fail("capture trailer source hashes do not match the stream")
    tx_frames = require_nonnegative_int(trailer.get("txFrames"), "trailer.txFrames")
    bus_faults = require_nonnegative_int(trailer.get("busFaults"), "trailer.busFaults")

    reasons: list[str] = []
    minimum_duration = CAPTURE_MINIMUM_MS[header["captureKind"]]
    if duration_ms < minimum_duration:
        reasons.append(f"duration {duration_ms} is below {minimum_duration} ms")
    if any(byte_counts[source] == 0 for source in CAPTURE_SOURCES):
        reasons.append("both sources must contain observed bytes")
    if byte_counts["mcu-uart"] != byte_counts["logic-analyzer"]:
        reasons.append("MCU and analyzer byte counts differ")
    if computed_hashes["mcu-uart"] != computed_hashes["logic-analyzer"]:
        reasons.append("MCU and analyzer byte streams differ")
    if tx_frames != 0:
        reasons.append("capture reports TX frames")
    if bus_faults != 0:
        reasons.append("capture reports field-bus faults")
    ready = not reasons
    if require_complete and not ready:
        fail("capture is valid evidence but incomplete: " + "; ".join(reasons))
    return {
        "template": False,
        "records": sum(record_counts.values()),
        "bytes": sum(byte_counts.values()),
        "duration_ms": duration_ms,
        "ready": ready,
        "reasons": reasons,
    }


def validate_repository_evidence_contract() -> dict[str, int]:
    contract = load_contract()
    contract_metrics = validate_contract(contract)
    validate_packet(
        load_packet(),
        contract,
        allow_template=True,
        require_artifacts=False,
    )
    validate_capture_file(
        CAPTURE_TEMPLATE_PATH,
        contract,
        allow_template=True,
        require_complete=False,
    )
    return {
        "packet_types": contract_metrics["packet_types"],
        "capture_kinds": contract_metrics["capture_kinds"],
        "templates": 2,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--packet", type=Path, help="validate one private JSON packet")
    group.add_argument("--capture", type=Path, help="validate one private NDJSON capture")
    parser.add_argument(
        "--require-artifacts",
        action="store_true",
        help="for a packet, require every artifact and verify size plus SHA-256",
    )
    parser.add_argument(
        "--require-gate-ready",
        action="store_true",
        help="for a packet, fail when valid evidence is insufficient to close its gate",
    )
    parser.add_argument(
        "--require-complete",
        action="store_true",
        help="for a capture, require duration, paired byte equality, zero TX, and zero faults",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        contract = load_contract()
        if args.packet is not None:
            if args.require_complete:
                fail("--require-complete applies only to --capture")
            metrics = validate_packet(
                load_packet(args.packet),
                contract,
                require_artifacts=args.require_artifacts,
                require_gate_ready=args.require_gate_ready,
            )
            print(
                "evidence packet validation: PASS "
                f"(type={metrics['packet_type']}, artifacts={metrics['artifacts']}, "
                f"claims={metrics['claims']}, gateReady={str(metrics['ready']).lower()})"
            )
        elif args.capture is not None:
            if args.require_artifacts or args.require_gate_ready:
                fail("--require-artifacts and --require-gate-ready apply only to --packet")
            metrics = validate_capture_file(
                args.capture,
                contract,
                require_complete=args.require_complete,
            )
            print(
                "passive capture validation: PASS "
                f"(records={metrics['records']}, bytes={metrics['bytes']}, "
                f"durationMs={metrics['duration_ms']}, gateReady={str(metrics['ready']).lower()})"
            )
        else:
            if args.require_artifacts or args.require_gate_ready or args.require_complete:
                fail("gate flags require --packet or --capture")
            metrics = validate_repository_evidence_contract()
            print(
                "evidence contract validation: PASS "
                f"({metrics['packet_types']} packet types, {metrics['capture_kinds']} capture kinds, "
                f"{metrics['templates']} non-evidence templates)"
            )
    except EvidencePacketValidationError as exc:
        print(f"evidence validation: FAIL: {exc}", file=sys.stderr)
        return 1

    print(
        "Proof boundary: structure and artifact integrity only; truthfulness, "
        "independent approval, and absent physical observations remain UNV"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
