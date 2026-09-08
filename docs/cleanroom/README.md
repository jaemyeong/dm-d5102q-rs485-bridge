# M0 clean-room baseline

Status: M0 remains blocked; target migration approved on 2026-09-07 KST.

The governing design is the user-supplied document version 1.0 preserved at
[`docs/design/DM-D5102Q-RS485-WIFI-BRIDGE-SPEC-v1.0.md`](../design/DM-D5102Q-RS485-WIFI-BRIDGE-SPEC-v1.0.md).
Its SHA-256 is
`d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114`.
The repository copy is byte-identical to the original attachment and is the
portable baseline validation input. The effective target is now C008/T002 under
[the approved amendment](../design/TARGET-AMENDMENT-T002-2026-09-07.md).
A131 hardware facts are retained only as history; no physical gate was passed.

## Clean-room boundary

This repository contains no legacy DM-D5102Q bridge implementation. The
pre-existing sketch is a board/toolchain heartbeat and contains no RS485 or
network implementation. The supplied design warns that its author reviewed a
prior project's feature surface. Therefore this working arrangement is an
independent implementation from the written contract, but it is not evidence
of a legally source-blind clean room until the roles and access controls in
[roles.md](roles.md) are assigned by people outside this repository.

Implementation inputs are limited to:

1. the governing design and approved requirement/test records;
2. public manufacturer, framework, and library documentation;
3. evidence captured from the exact target hardware and field bus;
4. independently authored test vectors that do not encode an unverified
   payload meaning.

## M0 deliverables

| Deliverable | Artifact | State |
|---|---|---|
| Role and access boundary | [roles.md](roles.md) | drafted; owners unassigned |
| Evidence register | [evidence-register.json](evidence-register.json) | metadata captured; physical evidence missing |
| Public hardware source lock | [public-source-lock.json](public-source-lock.json) | 10 snapshots locked; 4 active C008/T002 sources; 3 T002 physical gates unverified |
| Physical evidence and capture contract | [physical-evidence-contract.md](physical-evidence-contract.md) | 4 packet types and 2 RX-only capture kinds validated; no actual packet accepted |
| Threat model | [threat-model.md](threat-model.md) | baseline drafted |
| Wiring checklist | [wiring-checklist.md](wiring-checklist.md) | drafted; no physical item passed |
| Initial test vectors | [initial.json](../../tests/vectors/initial.json) | host/target/system ladder registered |
| Requirement baseline | [requirements.json](../../requirements/requirements.json) | 11 top-level requirements registered |
| Detailed M0-M9 gate state | [milestone-state.json](../../requirements/milestone-state.json) | all 207 design-derived criteria tracked; M0 has 11 satisfied, 5 blocked, and 1 triggered stop condition |
| Durable project-loop contract | [project-loop-contract.json](../../requirements/project-loop-contract.json) | 24 historical ledger rows sealed; later rows require full schema and a previous-row SHA-256 link |
| Toolchain lock | [toolchain-lock.md](toolchain-lock.md) | pinned candidate; host compile passed |
| Memory and partition budget | [memory-budget.json](../../requirements/memory-budget.json) | committed LTO build and dual-OTA gate pass; runtime heap UNV |

## M0 exit gates

- [ ] Evidence/specification/implementation/verification owners and access
      boundaries are approved.
- [ ] Exact Atom Lite C008 and Tail RS485 T002 SKU/revision photos are
      registered and hashed.
- [ ] Verify the documented TX26/RX32 interconnect, including crossed-path
      checks, with de-energized continuity on the selected T002 revision.
- [ ] Identify the T002 revision, transceiver, GPIO-side logic levels, power
      paths and termination/bias population; prove
      boot/reset A/B passivity on an isolated, current-limited bench.
- [ ] Bus owner and safety approver authorize RX-only observation and TX
      remains prohibited.
- [ ] Power, A/B/GND, termination, and topology facts have measurements or
      explicit `UNV` verification procedures.
- [x] Identity, continuity, approval, and paired passive-capture evidence have
      a hash-bound private packet format; templates are explicitly rejected as
      actual evidence.
- [x] RX-only NDJSON defines header/RX/trailer records, bounded 256-byte chunks,
      paired MCU/analyzer digests, duration, TX count, and bus-fault count.
- [x] Public facts used for pin and power decisions have archived source
      snapshots or immutable source revisions; their conflicts remain `UNV`
      and block selection.
- [x] The production toolchain candidate compiles a no-I/O exact-board fixture;
      clean-host reproducibility remains a release-stage gate.
- [x] Every M0 requirement and initial test ID passes the host consistency
      validator.
- [x] Every task, deliverable, exit/stop condition, test-strategy item,
      quantitative criterion, release condition, and implementation checklist
      item has a deterministic design-derived ID and fail-closed state record.

`python3 scripts/validate_milestone_gates.py --report` derives the 207-item
catalog from the SHA-256-bound governing design rather than duplicating its
text. IC-02 retains its baseline text/hash and exposes T002 effective text
under the pinned amendment. The mutable overlay records only status, proof stage, requirement/test
mapping, evidence, and blockers. It rejects design drift, missing groups,
unknown criteria, unsafe artifact paths, evidence-free completion, physical
claims without observed evidence, and out-of-order milestone advancement.

`python3 scripts/run_arduino_evals.py` runs the repository-owned
`loop-engine-evidence-contract` case. It binds the exact target and excluded
S3 device, requires one user-owned physical action, verifies evidence-stage
boundaries and the local rollback image, checks sensitive ignore rules, seals
the first 24 experiment rows, and requires a hash-linked full schema for every
later append. It does not promote any host result to physical evidence.

M1 must not be reported as started or complete until these gates pass. Host-side
preparation may continue, but it is not physical evidence.
