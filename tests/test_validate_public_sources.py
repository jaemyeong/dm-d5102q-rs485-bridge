from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from validate_public_sources import (  # noqa: E402
    PublicSourceValidationError,
    load_manifest,
    validate_manifest,
)


class PublicSourceValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.manifest = load_manifest()

    def test_repository_manifest_preserves_unverified_boundary(self) -> None:
        metrics = validate_manifest(self.manifest)

        self.assertEqual(metrics["sources"], 10)
        self.assertEqual(metrics["active_sources"], 4)
        self.assertEqual(metrics["hardware_gates"], 3)
        self.assertEqual(metrics["conflicts"], 2)
        self.assertEqual(metrics["inferences"], 1)

    def test_pinned_digest_cannot_change_with_manifest(self) -> None:
        weakened = copy.deepcopy(self.manifest)
        weakened["sources"][0]["sha256"] = "0" * 64

        with self.assertRaisesRegex(PublicSourceValidationError, "pinned digest"):
            validate_manifest(weakened)

    def test_pin_conflict_cannot_self_assert_resolution(self) -> None:
        weakened = copy.deepcopy(self.manifest)
        conflict = next(
            item for item in weakened["conflicts"] if item["id"] == "PIN-CONFLICT-001"
        )
        conflict["status"] = "resolved"

        with self.assertRaisesRegex(PublicSourceValidationError, "UNV and unresolved"):
            validate_manifest(weakened)

    def test_selected_pin_map_cannot_be_promoted_without_evidence(self) -> None:
        weakened = copy.deepcopy(self.manifest)
        weakened["selectedHardwareClaims"]["confirmedUartPinMap"] = {
            "rx": 22,
            "tx": 19,
        }

        with self.assertRaisesRegex(PublicSourceValidationError, "must remain null"):
            validate_manifest(weakened)

    def test_conflict_requires_physical_verification_method(self) -> None:
        weakened = copy.deepcopy(self.manifest)
        weakened["conflicts"][0]["verification"] = []

        with self.assertRaisesRegex(PublicSourceValidationError, "verification method"):
            validate_manifest(weakened)

    def test_a131_sources_cannot_become_current_t002_sources(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["activeSourceIds"].append("PUB-A131-SCHEMATIC-001")
        with self.assertRaisesRegex(PublicSourceValidationError, "active T002 source set"):
            validate_manifest(changed)

    def test_t002_hardware_gates_cannot_be_omitted_or_self_approved(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["unverifiedGates"] = []
        with self.assertRaisesRegex(PublicSourceValidationError, "T002 hardware gates"):
            validate_manifest(changed)
        changed = copy.deepcopy(self.manifest)
        changed["unverifiedGates"][0]["status"] = "verified"
        with self.assertRaisesRegex(PublicSourceValidationError, "UNV and unverified"):
            validate_manifest(changed)

    def test_t002_source_digest_is_pinned(self) -> None:
        changed = copy.deepcopy(self.manifest)
        source = next(s for s in changed["sources"] if s["id"] == "PUB-T002-PAGE-001")
        source["sha256"] = "0" * 64
        with self.assertRaisesRegex(PublicSourceValidationError, "pinned digest"):
            validate_manifest(changed)

    def test_a131_inference_cannot_be_applied_to_t002(self) -> None:
        changed = copy.deepcopy(self.manifest)
        changed["inferences"][0]["appliesToBaseSku"] = "T002"
        with self.assertRaisesRegex(PublicSourceValidationError, "historical A131"):
            validate_manifest(changed)

    def test_archive_path_cannot_escape_evidence_root(self) -> None:
        weakened = copy.deepcopy(self.manifest)
        weakened["sources"][0]["archivePath"] = "../atom-lite.pdf"

        with self.assertRaisesRegex(PublicSourceValidationError, "unsafe archivePath"):
            validate_manifest(weakened)


if __name__ == "__main__":
    unittest.main()
