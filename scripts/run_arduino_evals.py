#!/usr/bin/env python3
"""Run repository-owned Arduino/embedded durable-loop evaluations."""

from __future__ import annotations

import argparse
import sys

from validate_project_loop import (
    ProjectLoopValidationError,
    validate_repository_project_loop,
)

CASE = "loop-engine-evidence-contract"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=[CASE], default=CASE)
    args = parser.parse_args(argv)
    try:
        metrics = validate_repository_project_loop()
    except ProjectLoopValidationError as exc:
        print(f"{args.case}: FAIL: {exc}", file=sys.stderr)
        return 1

    print(
        f"{args.case}: PASS "
        f"(milestone={metrics['current_milestone']}, openTasks={metrics['open_tasks']}, "
        f"ledgerRows={metrics['ledger_rows']}, linkedRows={metrics['linked_ledger_rows']}, "
        f"privateEvidenceFiles={metrics['private_evidence_files']})"
    )
    print(
        "Proof boundary: state/evidence-loop integrity only; build, upload, hardware, "
        "system, and deployment proof remain separate"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
