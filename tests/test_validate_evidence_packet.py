from __future__ import annotations

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from validate_evidence_packet import (
    EvidencePacketValidationError,
    load_contract,
    load_packet,
    validate_capture_file,
    validate_packet,
    validate_repository_evidence_contract,
)

CAPTURE_TEMPLATE = ROOT / "docs/cleanroom/passive-capture.template.ndjson"
CAPTURE_SOURCES = ("mcu-uart", "logic-analyzer")
STAMP = "2026-09-01T00:00:00+00:00"


class EvidencePacketValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = load_contract()
        self.template = load_packet()

    def make_identity_packet(self, root: Path) -> dict:
        packet = copy.deepcopy(self.template)
        packet.update(
            {
                "packetId": "EVP-20260901T000000Z-IDENTITY",
                "status": "captured",
                "capturedAt": STAMP,
            }
        )
        packet["operator"] = {"id": "operator-01", "role": "evidence"}
        packet["conditions"] = {
            "power": {
                "usb": "disconnected",
                "field": "disconnected",
                "bench": "disconnected",
                "verificationMethod": "visual cable removal plus meter zero-voltage check",
            },
            "wiring": {"atomBaseMated": False, "fieldBusConnected": False},
        }
        packet["content"] = {
            "atom": {
                "observedSku": "C008",
                "pcbRevision": "ATOM-LITE-REV-A",
                "connectorOrientation": "USB-C label up; bottom pads facing camera",
                "markings": ["M5Stack", "ATOM Lite"],
            },
            "base": {
                "observedSku": "T002",
                "pcbRevision": "TEST-T002-REV-A",
                "connectorOrientation": "terminal away from Atom connector",
                "terminalLabels": ["B", "A", "12V+", "12V-"],
                "transceiverMarking": None,
                "terminationPopulation": "unverified",
                "markings": ["Tail RS485 T002"],
            },
            "notes": "synthetic unit-test packet; never field evidence",
        }
        packet["unverified"] = [
            "target.base.transceiverMarking",
            "target.base.terminationPopulation",
        ]

        artifact_by_role: dict[str, str] = {}
        for artifact in packet["artifacts"]:
            artifact_id = artifact["id"]
            relative = Path(
                f".arduino/evidence/hardware/private/test-identity/{artifact_id}.jpg"
            )
            absolute = root / relative
            absolute.parent.mkdir(parents=True, exist_ok=True)
            content = f"test-only-{artifact_id}".encode()
            absolute.write_bytes(content)
            artifact.update(
                {
                    "path": relative.as_posix(),
                    "sha256": hashlib.sha256(content).hexdigest(),
                    "bytes": len(content),
                    "mediaType": "image/jpeg",
                    "capturedAt": STAMP,
                }
            )
            artifact_by_role[artifact["roles"][0]] = artifact_id

        observations = {
            "target.atom.sku": ("C008", [artifact_by_role["atom-front"]]),
            "target.atom.pcbRevision": (
                "ATOM-LITE-REV-A",
                [artifact_by_role["atom-back"]],
            ),
            "target.atom.connectorOrientation": (
                "USB-C label up; bottom pads facing camera",
                [artifact_by_role["atom-front"], artifact_by_role["atom-back"]],
            ),
            "target.base.sku": ("T002", [artifact_by_role["base-front"]]),
            "target.base.pcbRevision": (
                "TEST-T002-REV-A",
                [artifact_by_role["base-back"]],
            ),
            "target.base.connectorOrientation": (
                "terminal away from Atom connector",
                [artifact_by_role["base-connector"]],
            ),
            "target.base.terminalLabels": (
                ["B", "A", "12V+", "12V-"],
                [artifact_by_role["base-terminal"]],
            ),
        }
        packet["claims"] = [
            {
                "id": f"OBS-{index:02d}",
                "field": field,
                "value": value,
                "classification": "OBS",
                "method": "labeled photograph",
                "artifactIds": artifact_ids,
            }
            for index, (field, (value, artifact_ids)) in enumerate(
                observations.items(), 1
            )
        ]
        return packet

    def make_continuity_packet(self, root: Path) -> dict:
        artifacts = []
        for role in ("meter-overview", "probe-orientation"):
            relative = Path(f".arduino/evidence/hardware/private/test-continuity/{role}.jpg")
            absolute = root / relative
            absolute.parent.mkdir(parents=True, exist_ok=True)
            content = f"test-only-{role}".encode()
            absolute.write_bytes(content)
            artifacts.append(
                {
                    "id": role,
                    "path": relative.as_posix(),
                    "sha256": hashlib.sha256(content).hexdigest(),
                    "bytes": len(content),
                    "mediaType": "image/jpeg",
                    "capturedAt": STAMP,
                    "roles": [role],
                }
            )
        measurements = []
        for base_net, gpio, result in (
            ("RX", 32, "continuity"),
            ("TX", 26, "continuity"),
            ("RX", 26, "open"),
            ("TX", 32, "open"),
        ):
            measurements.append(
                {
                    "baseNet": base_net,
                    "gpio": gpio,
                    "result": result,
                    "resistanceOhms": 0.4 if result == "continuity" else None,
                    "meterMode": "resistance",
                    "artifactIds": ["probe-orientation"],
                }
            )
        return {
            "schemaVersion": 1,
            "designSha256": self.contract["designSha256"],
            "designAmendmentId": self.contract["designAmendment"]["id"],
            "packetId": "EVP-20260901T010000Z-CONTINUITY",
            "packetType": "continuity",
            "status": "captured",
            "proofStage": "hardware",
            "capturedAt": STAMP,
            "operator": {"id": "operator-01", "role": "evidence"},
            "target": {"declaredAtomSku": "C008", "declaredBaseSku": "T002"},
            "conditions": {
                "power": {
                    "usb": "disconnected",
                    "field": "disconnected",
                    "bench": "disconnected",
                    "verificationMethod": "cables removed and zero-voltage check",
                },
                "wiring": {"atomBaseMated": True, "fieldBusConnected": False},
            },
            "artifacts": artifacts,
            "content": {
                "identityPacketRef": {
                    "packetId": "EVP-20260901T000000Z-IDENTITY",
                    "sha256": "1" * 64,
                },
                "meter": {
                    "manufacturerModel": "TEST-DMM",
                    "serial": "TEST-001",
                    "mode": "resistance",
                    "leadResistanceOhms": 0.2,
                },
                "measurements": measurements,
                "confirmedPinMap": None,
                "reviewStatus": "pending-independent-review",
            },
            "claims": [],
            "unverified": ["selectedUartPinMap"],
        }

    def test_repository_contract_and_non_evidence_templates_pass(self) -> None:
        metrics = validate_repository_evidence_contract()

        self.assertEqual(metrics, {"packet_types": 4, "capture_kinds": 2, "templates": 2})

    def test_template_is_rejected_as_actual_evidence(self) -> None:
        with self.assertRaisesRegex(EvidencePacketValidationError, "not evidence"):
            validate_packet(self.template, self.contract)

    def test_complete_identity_packet_and_artifacts_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)

            metrics = validate_packet(
                packet,
                self.contract,
                root=root,
                require_artifacts=True,
                require_gate_ready=True,
            )

        self.assertTrue(metrics["ready"])
        self.assertEqual(metrics["artifacts"], 6)

    def test_identity_packet_rejects_connected_power(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["conditions"]["power"]["usb"] = "connected"

            with self.assertRaisesRegex(EvidencePacketValidationError, "power.usb"):
                validate_packet(packet, self.contract, root=root)

    def test_identity_packet_cannot_claim_pin_mapping(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["claims"].append(
                {
                    "id": "OBS-PIN",
                    "field": "selectedUartPinMap",
                    "value": {"rx": 22, "tx": 19},
                    "classification": "OBS",
                    "method": "photo",
                    "artifactIds": ["base-connector"],
                }
            )

            with self.assertRaisesRegex(EvidencePacketValidationError, "not allowed"):
                validate_packet(packet, self.contract, root=root)

    def test_identity_packet_rejects_artifact_hash_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            artifact = packet["artifacts"][0]
            (root / artifact["path"]).write_bytes(b"changed-but-same-kind")
            artifact["bytes"] = len(b"changed-but-same-kind")

            with self.assertRaisesRegex(EvidencePacketValidationError, "SHA-256 mismatch"):
                validate_packet(
                    packet,
                    self.contract,
                    root=root,
                    require_artifacts=True,
                )

    def test_wrong_observed_sku_is_valid_evidence_but_not_gate_ready(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["content"]["atom"]["observedSku"] = "NOT-C008"
            claim = next(
                item for item in packet["claims"] if item["field"] == "target.atom.sku"
            )
            claim["value"] = "NOT-C008"

            metrics = validate_packet(packet, self.contract, root=root)
            self.assertFalse(metrics["ready"])
            with self.assertRaisesRegex(EvidencePacketValidationError, "does not satisfy"):
                validate_packet(
                    packet,
                    self.contract,
                    root=root,
                    require_gate_ready=True,
                )

    def test_not_visible_revision_remains_unverified(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["content"]["base"]["pcbRevision"] = "not-visible"
            packet["claims"] = [
                item
                for item in packet["claims"]
                if item["field"] != "target.base.pcbRevision"
            ]
            packet["unverified"].append("target.base.pcbRevision")

            metrics = validate_packet(packet, self.contract, root=root)

        self.assertFalse(metrics["ready"])

    def test_artifact_path_cannot_escape_private_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["artifacts"][0]["path"] = "../outside.jpg"

            with self.assertRaisesRegex(EvidencePacketValidationError, "unsafe artifact"):
                validate_packet(packet, self.contract, root=root)

    def test_continuity_packet_covers_t002_straight_and_crossed_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_continuity_packet(root)

            metrics = validate_packet(
                packet,
                self.contract,
                root=root,
                require_artifacts=True,
                require_gate_ready=True,
            )

        self.assertTrue(metrics["ready"])

    def test_continuity_packet_cannot_auto_confirm_pin_map(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_continuity_packet(root)
            packet["content"]["confirmedPinMap"] = {"rx": 22, "tx": 19}

            with self.assertRaisesRegex(EvidencePacketValidationError, "cannot confirm"):
                validate_packet(packet, self.contract, root=root)

    def test_continuity_packet_requires_exact_candidate_matrix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_continuity_packet(root)
            packet["content"]["measurements"].pop()

            with self.assertRaisesRegex(EvidencePacketValidationError, "exact candidate matrix"):
                validate_packet(packet, self.contract, root=root)

    def test_approval_requires_matching_role_and_evidence_reference(self) -> None:
        packet = {
            "schemaVersion": 1,
            "designSha256": self.contract["designSha256"],
            "designAmendmentId": self.contract["designAmendment"]["id"],
            "packetId": "EVP-20260901T020000Z-APPROVAL",
            "packetType": "approval",
            "status": "captured",
            "proofStage": "host-then-hardware",
            "capturedAt": STAMP,
            "operator": {"id": "bus-owner-01", "role": "bus-owner"},
            "target": {"declaredAtomSku": "C008", "declaredBaseSku": "T002"},
            "artifacts": [],
            "content": {
                "role": "bus-owner",
                "decision": "approved",
                "scopes": ["rx-only-observation"],
                "signerId": "bus-owner-01",
                "signedAt": STAMP,
                "conditions": ["TX remains prohibited"],
                "evidenceRefs": [
                    {
                        "packetId": "EVP-20260901T000000Z-IDENTITY",
                        "sha256": "1" * 64,
                    }
                ],
            },
            "claims": [],
            "unverified": [],
        }

        self.assertTrue(validate_packet(packet, self.contract)["ready"])
        packet["operator"]["role"] = "safety-approver"
        with self.assertRaisesRegex(EvidencePacketValidationError, "role must equal"):
            validate_packet(packet, self.contract)


    def test_a131_declaration_cannot_pass_t002_packet_validation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["target"]["declaredBaseSku"] = "A131"
            with self.assertRaisesRegex(EvidencePacketValidationError, "C008 plus T002"):
                validate_packet(packet, self.contract, root=root)

    def test_observed_a131_is_failed_evidence_not_t002_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            packet["content"]["base"]["observedSku"] = "A131"
            claim = next(c for c in packet["claims"] if c["field"] == "target.base.sku")
            claim["value"] = "A131"
            self.assertFalse(validate_packet(packet, self.contract, root=root)["ready"])
            with self.assertRaisesRegex(EvidencePacketValidationError, "does not satisfy"):
                validate_packet(packet, self.contract, root=root, require_gate_ready=True)

    def test_packet_requires_current_amendment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_identity_packet(root)
            del packet["designAmendmentId"]
            with self.assertRaisesRegex(EvidencePacketValidationError, "target amendment"):
                validate_packet(packet, self.contract, root=root)

    def test_capture_header_requires_current_amendment(self) -> None:
        from validate_evidence_packet import validate_capture_header

        header = json.loads(CAPTURE_TEMPLATE.read_text().splitlines()[0])
        del header["designAmendmentId"]
        with self.assertRaisesRegex(EvidencePacketValidationError, "target amendment"):
            validate_capture_header(header, self.contract)

    def test_a131_continuity_matrix_is_rejected_for_t002(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            packet = self.make_continuity_packet(root)
            for row, (net, gpio) in zip(
                packet["content"]["measurements"],
                [("RX", 22), ("TX", 19), ("RX", 32), ("TX", 26)],
            ):
                row["baseNet"], row["gpio"] = net, gpio
            with self.assertRaisesRegex(EvidencePacketValidationError, "outside the candidate matrix"):
                validate_packet(packet, self.contract, root=root)



class PassiveCaptureValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = load_contract()

    def write_capture(
        self,
        path: Path,
        *,
        duration_ms: int = 1_800_000,
        analyzer_payload: bytes = b"\x00\x7e\x80\xff",
        tx_frames: int = 0,
        corrupt_trailer_hash: bool = False,
    ) -> None:
        started = datetime(2026, 9, 1, tzinfo=timezone.utc)
        ended = started + timedelta(milliseconds=duration_ms)
        mcu_payload = b"\x00\x7e\x80\xff"
        payloads = {
            "mcu-uart": mcu_payload,
            "logic-analyzer": analyzer_payload,
        }
        header = {
            "recordType": "header",
            "schemaVersion": 1,
            "status": "captured",
            "captureId": "CAP-20260901T000000Z-FIELD",
            "captureKind": "field-passive",
            "designSha256": self.contract["designSha256"],
            "designAmendmentId": self.contract["designAmendment"]["id"],
            "startedAt": started.isoformat(),
            "timebase": "monotonic-us",
            "txPolicy": "prohibited",
            "firmwareSha256": "1" * 64,
            "identityPacketSha256": "2" * 64,
            "wiringPacketSha256": "3" * 64,
            "uart": {
                "baud": 3840,
                "dataBits": 8,
                "parity": "none",
                "stopBits": 1,
                "framing": "raw",
            },
            "sources": list(CAPTURE_SOURCES),
        }
        records = []
        for source in CAPTURE_SOURCES:
            records.append(
                {
                    "recordType": "rx",
                    "source": source,
                    "seq": 0,
                    "tMonotonicUs": 1000,
                    "bytesHex": payloads[source].hex(" ").upper(),
                    "uartErrors": {"frame": 0, "parity": 0, "overflow": 0},
                }
            )
        hashes = {
            source: hashlib.sha256(payloads[source]).hexdigest()
            for source in CAPTURE_SOURCES
        }
        if corrupt_trailer_hash:
            hashes["mcu-uart"] = "0" * 64
        trailer = {
            "recordType": "trailer",
            "endedAt": ended.isoformat(),
            "durationMs": duration_ms,
            "recordsBySource": {source: 1 for source in CAPTURE_SOURCES},
            "bytesBySource": {
                source: len(payloads[source]) for source in CAPTURE_SOURCES
            },
            "sha256BySource": hashes,
            "txFrames": tx_frames,
            "busFaults": 0,
        }
        lines = [header, *records, trailer]
        path.write_text(
            "\n".join(json.dumps(item, separators=(",", ":")) for item in lines)
            + "\n",
            encoding="utf-8",
        )

    def test_capture_template_is_not_actual_evidence(self) -> None:
        with self.assertRaisesRegex(EvidencePacketValidationError, "not evidence"):
            validate_capture_file(CAPTURE_TEMPLATE, self.contract)

    def test_complete_paired_capture_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "capture.ndjson"
            self.write_capture(path)

            metrics = validate_capture_file(
                path,
                self.contract,
                require_complete=True,
            )

        self.assertTrue(metrics["ready"])
        self.assertEqual(metrics["bytes"], 8)

    def test_mismatched_stream_is_valid_but_not_complete(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "capture.ndjson"
            self.write_capture(path, analyzer_payload=b"\x00\x7e\x80\x00")

            metrics = validate_capture_file(path, self.contract)
            self.assertFalse(metrics["ready"])
            with self.assertRaisesRegex(EvidencePacketValidationError, "streams differ"):
                validate_capture_file(path, self.contract, require_complete=True)

    def test_short_capture_is_valid_but_not_complete(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "capture.ndjson"
            self.write_capture(path, duration_ms=1_000)

            with self.assertRaisesRegex(EvidencePacketValidationError, "below 1800000"):
                validate_capture_file(path, self.contract, require_complete=True)

    def test_capture_with_tx_is_not_complete(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "capture.ndjson"
            self.write_capture(path, tx_frames=1)

            with self.assertRaisesRegex(EvidencePacketValidationError, "reports TX"):
                validate_capture_file(path, self.contract, require_complete=True)

    def test_capture_rejects_false_trailer_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "capture.ndjson"
            self.write_capture(path, corrupt_trailer_hash=True)

            with self.assertRaisesRegex(EvidencePacketValidationError, "source hashes"):
                validate_capture_file(path, self.contract)

    def test_passive_capture_packet_binds_and_validates_private_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            relative = Path(
                ".arduino/evidence/hardware/private/passive-capture/capture.ndjson"
            )
            capture = root / relative
            capture.parent.mkdir(parents=True, exist_ok=True)
            self.write_capture(capture)
            content = capture.read_bytes()
            packet = {
                "schemaVersion": 1,
                "designSha256": self.contract["designSha256"],
                "designAmendmentId": self.contract["designAmendment"]["id"],
                "packetId": "EVP-20260901T030000Z-PASSIVE",
                "packetType": "passive-capture",
                "status": "captured",
                "proofStage": "hardware",
                "capturedAt": STAMP,
                "operator": {"id": "operator-01", "role": "evidence"},
                "target": {
                    "declaredAtomSku": "C008",
                    "declaredBaseSku": "T002",
                },
                "conditions": {
                    "txPolicy": "compile-time-disabled",
                    "fieldBusConnected": True,
                    "independentAnalyzerConnected": True,
                },
                "artifacts": [
                    {
                        "id": "paired-capture",
                        "path": relative.as_posix(),
                        "sha256": hashlib.sha256(content).hexdigest(),
                        "bytes": len(content),
                        "mediaType": "application/x-ndjson",
                        "capturedAt": STAMP,
                        "roles": ["paired-capture-ndjson"],
                    }
                ],
                "content": {
                    "capturePath": relative.as_posix(),
                    "identityPacketSha256": "2" * 64,
                    "wiringPacketSha256": "3" * 64,
                    "firmwareSha256": "1" * 64,
                },
                "claims": [],
                "unverified": [],
            }

            metrics = validate_packet(
                packet,
                self.contract,
                root=root,
                require_artifacts=True,
                require_gate_ready=True,
            )

        self.assertTrue(metrics["ready"])



if __name__ == "__main__":
    unittest.main()
