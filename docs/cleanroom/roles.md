# Roles and access boundary

Status: draft; human owners are unassigned.

| Role | Allowed inputs | Required output | Current owner |
|---|---|---|---|
| Evidence | Public documents and exact-device observations | Immutable source/capture, hash, time, equipment, firmware, wiring state | UNASSIGNED |
| Specification | Evidence register and product decisions | Requirement IDs and black-box test vectors | UNASSIGNED |
| Implementation | Governing design, approved public/observed evidence, approved vectors | Independently authored firmware and build artifacts | Codex working context; not legally source-blind |
| Verification | Release artifacts and black-box interfaces | Test results with proof-stage labels | UNASSIGNED |
| Bus owner | Field topology and operational authority | RX-only/TX authorization and stop conditions | UNASSIGNED |
| Safety approver | Power, grounding, isolation, thermal, and topology evidence | Signed physical gate decision | UNASSIGNED |

## Access rules

- Implementation must not receive a legacy bridge source tree, symbols, file
  layout, or copied constants.
- Public documentation and raw observations are registered before they become
  normative implementation inputs.
- The person or process producing a physical observation must not mark its own
  implementation complete solely from that observation.
- Secrets, credentials, private bus payloads, and unredacted network captures
  are not committed. Their metadata and sanitized hashes may be registered.
- A role assignment or approval must include a name/identifier, date, scope,
  and evidence path. Blank cells are not implicit approval.
- Record each human decision as one `approval` packet under the physical
  evidence contract. An approval cites packet IDs and SHA-256 values and does
  not expand beyond its explicit scope.

## Current limitation

One agent context has read the governing design and is preparing implementation
artifacts. This is sufficient for an independent-from-spec implementation, but
not for a legally strict source-blind clean-room claim. A strict claim requires
separate people, credentials, and access logs before implementation begins.
