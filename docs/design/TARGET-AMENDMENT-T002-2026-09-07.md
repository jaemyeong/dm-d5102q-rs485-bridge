# Target amendment: Atom Lite C008 + Tail RS485 T002

Amendment ID: TARGET-T002-20260907
Status: approved target substitution; physical acceptance remains blocked
Decision date: 2026-09-07 KST
Authority: the user replied "변경해서 진행" to the explicit proposal to change
the target to the pictured ATOM Lite plus Tail485.

## Scope and precedence

The effective design is the preserved version 1.0 plus this amendment.
The original attachment and repository copy are not rewritten:
`DM-D5102Q-RS485-WIFI-BRIDGE-SPEC-v1.0.md`, SHA-256
`d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114`.

The one intended target is now M5Stack Atom Lite C008 plus Tail RS485 T002
(marketed/labeled Tail485). Atomic RS485 Base A131 is no longer a supported
target. Atom S3 Lite remains excluded. The MCU, production board ID
`m5stack-atom`, single `atom_lite` environment, locked toolchain, partition
layout, product scope and M0-M9 ordering remain unchanged.

This directive approves the target/design migration and host-side work only.
It does not assign independent evidence/verification roles or approve wiring,
power, flashing, field attachment, active probing, or bus transmission.

## Hardware substitutions

This amendment overrides the target line and A131-specific facts in sections
3.1, 3.2, 4, 5.1, 5.2, 21, 22 and 23 of the baseline:

- Replace the A131 BOM item with Tail RS485 T002 and its matching terminal.
  Verify the actual supplied connector; do not assume A131 mechanical mating.
- Use Tail485's documented Atom Lite mapping, TX GPIO26 and RX GPIO32, as the
  public-reference candidate. Verify the selected unit and interconnect before
  defining operational UART pins. A131's bottom-base RX22/TX19 helper is not
  applicable. Check both straight and crossed RX/TX-to-GPIO26/32 paths.
- M5Stack's T002 page identifies SP485EEN-L and an AOZ1282CI DC/DC regulator.
  Do not carry over the A131 SP3485 part family or 6-24 V claim.
- The T002 page/block diagram describes a 9-24 V to 5 V power path and a
  terminal nominally labeled 12V. This is a manufacturer family statement,
  not a measured rating or permission to energize the selected assembly.
  Confirm rails, GPIO-side logic levels, polarity, current limits and backfeed
  behavior with revision-matched evidence before power or MCU connection.
- Built-in termination/bias population, logic-level conditioning, driver
  enable topology and boot/reset passivity remain UNV. The public T002
  "schematic" asset is a block diagram, not a component-level circuit.
  A131's R4 label, no-termination claim and transistor/DE inference must not be
  transferred. Use generic transceiver/termination observation fields.
- No independent MCU DE pin is assigned. Automatic direction behavior is a
  hypothesis to verify, not a confirmed capability of the selected unit.
- Baseline checklist IC-02 now means exact Atom Lite C008 and Tail RS485 T002
  SKU/revision/connector evidence. Its original text and line hash remain in
  the 207-item baseline catalog; reports also expose the effective text.

All other requirements, quantitative limits, security controls, TX-disabled
defaults, independent comparisons, deployment/rollback obligations and stop
conditions are unchanged. A target substitution does not pass any criterion.

## Current evidence and next gate

Four user-supplied photos are preserved in two private intakes. They support
ATOM LITE and Tail485 enclosure-label observations; the T002 SKU is a
manufacturer-family association, not a literal SKU/revision read on the unit.
The prior mismatch intake remains unchanged as a historical receipt.

No actual identity, continuity, approval or passive-capture packet has been
accepted. No new firmware has been uploaded. Terminal conductors are visible;
their far-end connections, field-bus isolation and power state are unknown.
First request the user's current power/wiring context and capture context,
without moving wires or asking for duplicate close-up views. Subsequent
identity/revision, continuity, electrical and approval gates remain open.

Do not mate, power, probe, flash or connect to the field bus until the relevant
scoped gate passes. Never perform continuity measurements on energized wiring.
The pre-smoke 4 MiB rollback image and append-only ledger remain protected.

## Sources and applicability

- [M5Stack Tail RS485](https://docs.m5stack.com/en/atom/tail485)
- [M5Stack T002 block diagram](https://static-cdn.m5stack.com/resource/docs/products/atom/tail485/tail485_sch_01.webp)
- [M5Stack Atom Lite](https://docs.m5stack.com/en/core/ATOM%20Lite)

The source lock identifies active C008/T002 snapshots separately from retained
A131 history. A131 conflicts remain unresolved historical observations, not
current T002 blockers or evidence of T002 electrical behavior. T002's own
identity/interconnect, electrical limits and boot-passivity gates block M1.

## Tooling, verification and limits

The existing production toolchain candidate remains PlatformIO Core 6.1.19,
platform-espressif32 7.0.0 and Arduino-ESP32 2.0.17; the separate historical
Arduino CLI/M5Stack smoke environment is not substituted into production.

Host tests must reject A131 packet declarations, unapproved target/amendment
changes, reused A131 continuity matrices, missing T002 physical gates and
tampered source/rollback artifacts. They do not prove hardware operation.
The installed board-support inventory is unavailable; the local profile uses
primary-source family facts and remains profile-gap for physical pin advice.
