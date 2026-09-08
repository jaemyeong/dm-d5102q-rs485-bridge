# Physical evidence packet and passive-capture contract

Current target: C008/T002 under TARGET-T002-20260907. Packets and capture
headers must carry this `designAmendmentId`; the repository contract pins the
amendment path and SHA-256. Old A131 records remain historical and cannot pass
current gates. Schema `base` means the selected T002 module, not an A131 base.
Generic `transceiverMarking` and `terminationPopulation` observations replace
A131-specific U1/R4 assumptions. No prior raw photo/intake is rewritten.

Status: M0 host contract. No physical packet has been accepted.

This contract makes a physical observation reviewable without turning a file
or checkbox into proof. Raw photos, device identifiers, meter images, analyzer
captures, and field payloads stay below the Git-ignored
`.arduino/evidence/hardware/private/` root. Only sanitized facts, hashes, proof
stage, and limitations are promoted into `evidence-register.json` after an
independent review.

The machine-readable constants are in
[`evidence-packet-contract.json`](../../requirements/evidence-packet-contract.json).
The current physical action starts from
[`identity-photo-packet.template.json`](identity-photo-packet.template.json).
The receive-only NDJSON envelope starts from
[`passive-capture.template.ndjson`](passive-capture.template.ndjson).

## Evidence packet envelope

Every packet has one causal action and one `packetType`:

| Type | One action | What it cannot prove by itself |
|---|---|---|
| `identity-photo` | Photograph separated, fully de-energized C008/T002 | GPIO continuity or electrical behavior |
| `continuity` | Measure the four declared RX/TX-to-GPIO candidate paths with power removed | Functional direction or field safety |
| `approval` | Record one named role decision, scope, conditions, and referenced packet hashes | A measurement the approver did not observe |
| `passive-capture` | Record paired MCU/analyzer RX streams under one fixed wiring/firmware state | Payload meaning or TX safety |

An actual packet uses `status: captured`, an offset-aware timestamp, an
operator role identifier, exact target declaration, and hash-bound artifacts.
`classification: OBS` claims must cite one or more artifact IDs. Template
values are null and `TEMPLATE-NOT-EVIDENCE`; the validator rejects them as
actual evidence.

Artifact paths are repository-relative, cannot contain `..`, and must remain
under the private root. With `--require-artifacts`, the validator resolves each
path, rejects an escaping symlink, and checks byte size and SHA-256 without
printing raw content.

## Identity-photo gate

Required roles are Atom front/back and Base front/back/terminal/connector.
USB, field, and bench power must all be recorded as disconnected; Atom and Base
must be separated and the field bus disconnected. A syntactically valid packet
may still report an unreadable SKU or revision as `UNV`. That is useful failed
evidence, but `--require-gate-ready` rejects it and M1 stays blocked.

Identity photos may record visible transceiver/termination markings, but they cannot claim or
resolve a UART pin map. Copy the template into the private evidence directory,
fill it without deleting unknowns, then run:

```sh
python3 scripts/validate_evidence_packet.py \
  --packet .arduino/evidence/hardware/private/target-sku-revision/packet.json \
  --require-artifacts --require-gate-ready
```

## Continuity packet

After identity review, a separate packet records these four candidates:

- T002 RX to GPIO32
- T002 TX to GPIO26
- T002 RX to GPIO26 (crossed-path check)
- T002 TX to GPIO32 (crossed-path check)

It includes meter identity/mode, lead resistance, probe orientation, result,
resistance when measurable, and artifact references. All power remains
disconnected and the field bus remains detached. The raw packet's
`confirmedPinMap` must be null: an evidence producer cannot promote its own
measurement. An independent verification decision and revision-matched review
are required to update the current T002 hardware-gate record.

## Approval packet

Each role decision is separate and names one signer identifier, timestamp,
decision, exact scope, conditions, and evidence packet ID/SHA-256 references.
The roles are evidence, specification, verification, bus owner, and safety
approver. Approval of RX-only observation does not authorize field attachment,
power, or TX unless those scopes are explicitly listed.

## Passive-capture NDJSON

The first line is one `header`, the final line one `trailer`, and intervening
lines are `rx` records only. Each RX record has source, per-source sequence,
monotonic microsecond timestamp, canonical uppercase `bytesHex`, and cumulative
UART error snapshot. One record is limited to 256 bytes. Both `mcu-uart` and
`logic-analyzer` streams are
concatenated independently and hashed; a gate-ready capture requires equal
byte counts and SHA-256 values.

`field-passive` requires at least 1,800,000 ms, two non-empty matching streams,
zero TX frames, and zero reported bus faults. `bench-loss-115200` requires at
least 3,600,000 ms. A shorter or mismatched capture remains structurally valid
failed evidence unless `--require-complete` is requested.

```sh
python3 scripts/validate_evidence_packet.py \
  --capture .arduino/evidence/hardware/private/passive-capture/capture.ndjson \
  --require-complete
```

The format records raw bytes only. It does not infer DM-D5102Q payload meaning,
claim Modbus compatibility, or permit active probing.
