from __future__ import annotations

import copy
import hashlib
import json
import sys
import unittest
from datetime import datetime, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from validate_project_loop import (
    ProjectLoopValidationError,
    load_contract,
    validate_contract,
    validate_repository_project_loop,
)


class ProjectLoopValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = load_contract()

    def validate(self, overrides: dict[str, bytes | None] | None = None) -> dict:
        return validate_repository_project_loop(
            contract=self.contract,
            file_overrides=overrides,
        )

    def read(self, relative: str) -> bytes:
        return (ROOT / relative).read_bytes()

    def test_repository_loop_contract_passes(self) -> None:
        metrics = self.validate()

        self.assertGreaterEqual(metrics["ledger_rows"], 24)
        self.assertEqual(metrics["sealed_ledger_rows"], 24)
        self.assertEqual(
            metrics["linked_ledger_rows"],
            metrics["ledger_rows"] - metrics["sealed_ledger_rows"],
        )
        self.assertEqual(metrics["open_tasks"], 1)
        self.assertEqual(metrics["evidence_stages"], 5)
        self.assertEqual(metrics["rollback_bytes"], 4 * 1024 * 1024)

    def test_contract_cannot_change_exact_target(self) -> None:
        weakened = copy.deepcopy(self.contract)
        weakened["target"]["atomSku"] = "C009"

        with self.assertRaisesRegex(ProjectLoopValidationError, "atomSku"):
            validate_contract(weakened)

    def test_missing_state_file_is_rejected(self) -> None:
        with self.assertRaisesRegex(ProjectLoopValidationError, "missing loop state"):
            self.validate({".arduino/goal.md": None})

    def test_a131_cannot_replace_approved_t002(self) -> None:
        changed = copy.deepcopy(self.contract)
        changed["target"]["baseSku"] = "A131"
        with self.assertRaisesRegex(ProjectLoopValidationError, "baseSku"):
            validate_contract(changed)

    def test_target_amendment_is_hash_bound_and_required(self) -> None:
        path = self.contract["designAmendment"]["path"]
        for altered in (None, b"changed target authorization"):
            with self.subTest(altered=altered):
                with self.assertRaisesRegex(ProjectLoopValidationError, "target amendment"):
                    self.validate({path: altered})

    def test_amendment_metadata_cannot_be_rewritten(self) -> None:
        changed = copy.deepcopy(self.contract)
        changed["designAmendment"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ProjectLoopValidationError, "target amendment"):
            validate_contract(changed)

    def test_next_todo_requires_exactly_one_open_action(self) -> None:
        todo = self.read(".arduino/next-todo.md") + b"\n- [ ] second action\n"

        with self.assertRaisesRegex(ProjectLoopValidationError, "exactly 1 open task"):
            self.validate({".arduino/next-todo.md": todo})

    def test_next_todo_requires_owner_and_evidence_contract(self) -> None:
        todo = self.read(".arduino/next-todo.md").replace(b"Owner: user.", b"Owner omitted.")

        with self.assertRaisesRegex(ProjectLoopValidationError, "Owner: user"):
            self.validate({".arduino/next-todo.md": todo})

    def test_milestone_state_must_match_durable_goal(self) -> None:
        state = json.loads(self.read("requirements/milestone-state.json"))
        state["currentMilestone"] = "M1"

        with self.assertRaisesRegex(ProjectLoopValidationError, "currentMilestone"):
            self.validate(
                {
                    "requirements/milestone-state.json": json.dumps(state).encode(),
                }
            )

    def test_sealed_ledger_prefix_rejects_historical_mutation(self) -> None:
        ledger = self.read(".arduino/experiment-log.jsonl").replace(
            b'"stage":"planning"', b'"stage":"rewritten"', 1
        )

        with self.assertRaisesRegex(ProjectLoopValidationError, "sealed prefix"):
            self.validate({".arduino/experiment-log.jsonl": ledger})

    def test_new_ledger_row_requires_full_schema_and_hash_link(self) -> None:
        ledger = self.read(".arduino/experiment-log.jsonl")
        previous = datetime.fromisoformat(json.loads(ledger.splitlines()[-1])["timestamp"])
        row = {
            "timestamp": (previous + timedelta(seconds=1)).isoformat(),
            "stage": "incomplete",
            "result": "synthetic",
        }
        ledger += json.dumps(row).encode() + b"\n"

        with self.assertRaisesRegex(ProjectLoopValidationError, "missing required fields"):
            self.validate({".arduino/experiment-log.jsonl": ledger})

    def test_valid_hash_linked_ledger_append_passes(self) -> None:
        ledger = self.read(".arduino/experiment-log.jsonl")
        baseline_rows = len(ledger.splitlines())
        previous_line = ledger.splitlines()[-1]
        previous_timestamp = datetime.fromisoformat(
            json.loads(previous_line)["timestamp"]
        )
        row = {
            "schemaVersion": 1,
            "previousRowSha256": hashlib.sha256(previous_line).hexdigest(),
            "timestamp": (previous_timestamp + timedelta(seconds=1)).isoformat(),
            "stage": "unit-test-fixture",
            "hypothesis": "a linked append remains valid",
            "changed_files": [],
            "command": "none",
            "result": "synthetic test-only pass",
            "metric": {"testOnly": 1},
            "gates": {"host": "test-only"},
            "acceptance": "test fixture only",
            "rollback": "discard in-memory row",
            "lesson": "hash link is checked",
            "next_candidate": "none",
        }
        appended = ledger + json.dumps(row, separators=(",", ":")).encode() + b"\n"

        metrics = self.validate({".arduino/experiment-log.jsonl": appended})

        self.assertEqual(metrics["ledger_rows"], baseline_rows + 1)
        self.assertEqual(metrics["linked_ledger_rows"], baseline_rows + 1 - 24)

    def test_new_ledger_timestamp_cannot_move_backwards(self) -> None:
        ledger = self.read(".arduino/experiment-log.jsonl")
        previous_line = ledger.splitlines()[-1]
        row = {
            "schemaVersion": 1,
            "previousRowSha256": hashlib.sha256(previous_line).hexdigest(),
            "timestamp": "2026-09-01T00:00:00+09:00",
            "stage": "unit-test-fixture",
            "hypothesis": "time reversal should fail",
            "changed_files": [],
            "command": "none",
            "result": "synthetic",
            "metric": {},
            "gates": {},
            "acceptance": "none",
            "rollback": "discard",
            "lesson": "none",
            "next_candidate": "none",
        }
        appended = ledger + json.dumps(row, separators=(",", ":")).encode() + b"\n"

        with self.assertRaisesRegex(ProjectLoopValidationError, "timestamp moved backwards"):
            self.validate({".arduino/experiment-log.jsonl": appended})

    def test_rollback_artifact_hash_is_verified(self) -> None:
        with self.assertRaisesRegex(ProjectLoopValidationError, "rollback artifact"):
            self.validate(
                {
                    ".arduino/evidence/deployment/atom-lite-pre-smoke-4mb.bin": b"changed"
                }
            )

    def test_sensitive_evidence_ignore_rules_are_required(self) -> None:
        gitignore = self.read(".gitignore").replace(
            b".arduino/evidence/hardware/private/", b""
        )

        with self.assertRaisesRegex(ProjectLoopValidationError, "ignore rule"):
            self.validate({".gitignore": gitignore})


if __name__ == "__main__":
    unittest.main()
