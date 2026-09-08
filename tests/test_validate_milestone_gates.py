from __future__ import annotations

import copy
import sys
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from validate_milestone_gates import (
    DESIGN_PATH,
    GROUP_SPECS,
    MILESTONE_IDS,
    MilestoneValidationError,
    catalog_sha256,
    derive_catalog,
    load_json,
    validate_repository_milestones,
)


class MilestoneGateValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.state = load_json(ROOT / "requirements/milestone-state.json")

    def validate(self, state: dict) -> dict:
        return validate_repository_milestones(state=state)

    def test_governing_design_derives_exact_complete_catalog(self) -> None:
        catalog = derive_catalog(DESIGN_PATH.read_text(encoding="utf-8"))

        self.assertEqual(len(catalog), 207)
        self.assertEqual(catalog_sha256(catalog), self.state["catalogSha256"])
        self.assertEqual(catalog[0].id, "M0-T01")
        self.assertEqual(catalog[-1].id, "IC-10")
        counts = Counter(item.group for item in catalog)
        self.assertEqual(set(counts), set(MILESTONE_IDS) | set(GROUP_SPECS))
        self.assertEqual(counts["M0"], 17)
        self.assertEqual(counts["M9"], 16)

    def test_repository_state_passes_and_remains_not_release_ready(self) -> None:
        report = self.validate(self.state)

        self.assertEqual(report["criteria"], 207)
        self.assertEqual(report["currentMilestone"], "M0")
        self.assertFalse(report["releaseReady"])
        self.assertEqual(report["milestones"]["M0"]["status"], "blocked")
        self.assertEqual(
            report["milestones"]["M0"]["statusCounts"],
            {"blocked": 5, "satisfied": 11, "triggered": 1},
        )

    def test_effective_t002_identity_keeps_original_catalog_text(self) -> None:
        report = self.validate(self.state)
        identity = next(
            row for row in report["groups"]["implementation-checklist"]["items"]
            if row["id"] == "IC-02"
        )
        self.assertIn("Atomic RS485 Base", identity["text"])
        self.assertIn("T002", identity["effectiveText"])
        self.assertEqual(report["effectiveTarget"], {"atomSku": "C008", "baseSku": "T002"})

    def test_unamended_or_a131_milestone_state_is_rejected(self) -> None:
        changed = copy.deepcopy(self.state)
        del changed["designAmendment"]
        with self.assertRaisesRegex(MilestoneValidationError, "target amendment"):
            self.validate(changed)
        changed = copy.deepcopy(self.state)
        changed["effectiveTarget"]["baseSku"] = "A131"
        with self.assertRaisesRegex(MilestoneValidationError, "effective target"):
            self.validate(changed)

    def test_later_milestone_cannot_start_before_m0(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M1"]["status"] = "in-progress"

        with self.assertRaisesRegex(MilestoneValidationError, "cannot start"):
            self.validate(weakened)

    def test_milestone_cannot_claim_satisfaction_with_open_criteria(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M0"]["status"] = "satisfied"

        with self.assertRaisesRegex(MilestoneValidationError, "open task"):
            self.validate(weakened)

    def test_satisfied_criterion_requires_evidence_or_artifact(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M0"]["overrides"]["M0-T02"] = {
            "status": "satisfied",
            "proofStage": "host",
        }

        with self.assertRaisesRegex(MilestoneValidationError, "without evidence"):
            self.validate(weakened)

    def test_hardware_satisfaction_requires_observed_evidence(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M0"]["overrides"]["M0-T03"] = {
            "status": "satisfied",
            "proofStage": "hardware",
            "evidenceIds": ["EV-DEC-PHYSICAL-EVIDENCE-CONTRACT-001"],
        }

        with self.assertRaisesRegex(MilestoneValidationError, "without observed evidence"):
            self.validate(weakened)

    def test_unknown_criterion_override_is_rejected(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M0"]["overrides"]["M0-T99"] = {
            "status": "satisfied",
            "proofStage": "host",
            "artifactPaths": ["requirements/requirements.json"],
        }

        with self.assertRaisesRegex(MilestoneValidationError, "unknown criterion"):
            self.validate(weakened)

    def test_override_cannot_replace_derived_catalog_fields(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M0"]["overrides"]["M0-T02"]["text"] = "weakened"

        with self.assertRaisesRegex(MilestoneValidationError, "unsupported fields"):
            self.validate(weakened)

    def test_missing_global_group_is_rejected(self) -> None:
        weakened = copy.deepcopy(self.state)
        del weakened["groups"]["release-definition"]

        with self.assertRaisesRegex(MilestoneValidationError, "groups are incomplete"):
            self.validate(weakened)

    def test_artifact_path_cannot_escape_repository(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["milestones"]["M0"]["overrides"]["M0-T02"]["artifactPaths"] = [
            "../outside"
        ]

        with self.assertRaisesRegex(MilestoneValidationError, "escapes the repository"):
            self.validate(weakened)

    def test_catalog_hash_change_is_rejected(self) -> None:
        weakened = copy.deepcopy(self.state)
        weakened["catalogSha256"] = "0" * 64

        with self.assertRaisesRegex(MilestoneValidationError, "catalog hash mismatch"):
            self.validate(weakened)

    def test_governing_design_drift_is_rejected_before_parsing(self) -> None:
        design_text = DESIGN_PATH.read_text(encoding="utf-8") + "\n"

        with self.assertRaisesRegex(MilestoneValidationError, "design hash changed"):
            validate_repository_milestones(state=self.state, design_text=design_text)


if __name__ == "__main__":
    unittest.main()
