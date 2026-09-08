# RX-only wiring and power checklist

Nothing in this checklist is complete merely because the earlier heartbeat
firmware ran. Record each observation with target identity, UTC/KST time,
instrument, firmware hash, wiring state, units, and evidence path.
Use the one-action packet and private-artifact rules in
[physical-evidence-contract.md](physical-evidence-contract.md); a valid packet
can still be failed or ambiguous evidence and does not self-approve a gate.

## Gate 0A: exact identity, fully separated and power off

- [ ] Photograph both sides and labels of the Atom Lite; confirm `C008` and
      record any PCB/revision marking.
- [ ] Use the supplied Tail485 module views and record remaining T002 SKU,
      PCB/revision and connector details; ask for a new view only for a named
      missing detail. The user approved T002, not a physical identity gate.
- [ ] Confirm the Atom S3 Lite is physically labeled and segregated from the
      target work area.
- [ ] Identify the bus owner and safety approver.

Do not mate the Atom and base during this gate. Stop if SKU, revision,
connector orientation, or ownership is ambiguous.

Package the observation from
[`identity-photo-packet.template.json`](identity-photo-packet.template.json)
and require artifact plus gate validation before Gate 0B.

## Gate 0B: verify T002 interconnect and revision, power off

The approved target amendment selects Tail RS485 T002. Its manufacturer pin
table agrees with TX GPIO26/RX GPIO32. The A131 RX22/TX19 helper is historical
non-target information, not a candidate for this module.

- [ ] Confirm the current power/field-bus state and wire far ends first; do not
      manipulate unknown connected wiring merely to take a photograph.
- [ ] Record selected T002 revision, transceiver marking, terminal labels,
      termination/bias population and connector orientation. Do not open or
      damage an enclosure for markings; inaccessible details remain UNV.
- [ ] After Gate 0A passes, with all USB/field/bench sources disconnected,
      measure the identified module UART RX/TX-to-Atom GPIO26/32 interconnect.
      Do not assume the USB-shaped connector itself carries GPIO26/32.
- [ ] Record all four straight/crossed rows: RX-to-32, TX-to-26, RX-to-26 and
      TX-to-32. Include meter mode, lead compensation, readings, open-circuit
      observations, probe orientation and photographs.
- [ ] Keep actual mapping, transceiver, GPIO-side logic compatibility,
      termination/bias and power limits UNV until independently reviewed.

The continuity packet must leave `confirmedPinMap` null pending review.
A correctly formatted packet can report failed or crossed wiring; it does not
automatically confirm the documented mapping.

Stop on ambiguous identity, multiple conductive paths, inaccessible test points
or any energized continuity proposal. M1 is blocked until the relevant gates
and scope-specific approvals pass.

## Gate 1: topology, still power off

- [ ] Draw every node and cable segment; mark this bridge as endpoint or tap.
- [ ] Identify A, B, optional GND, supply positive, and supply return from the
      exact equipment manuals and continuity checks.
- [ ] Locate all existing termination and bias components. Add 120 ohm only if
      the bridge is a physical endpoint and the bus owner approves it.
- [ ] Confirm A/B use one twisted pair and the installation is not a new star.
- [ ] Define the first-test single power source and current limit.

Stop if a terminal function or existing termination cannot be established.

## Gate 2: controlled electrical measurement

- [ ] With the bridge disconnected, measure field supply minimum/maximum and
      polarity at the intended terminals.
- [ ] Measure ground potential between systems before deciding whether to join
      GND; record DC and relevant AC components.
- [ ] Measure idle A-B differential and both line common-mode voltages.
- [ ] Compare all readings with the revision-matched T002 transceiver, MCU GPIO and power-source limits.
- [ ] Power the base from one source through current limiting. Record idle and
      peak current and temperatures for 10 minutes.
- [ ] While the base is isolated from every field bus, scope TX, DE if safely
      accessible, and A/B during power-on, reset, bootloader entry, UART
      initialization, and controlled brownout/recovery.
- [ ] Demonstrate that the exact revision does not drive A/B during any RX-only
      lifecycle state. A firmware build containing no UART writes is not
      sufficient evidence. T002's published block diagram does not reveal its
      enable circuitry; do not substitute A131's direction-circuit inference.

Stop and disconnect on reverse polarity, unexpected current, overheating,
common-mode violation, unstable rails, or any existing-system fault.

## Gate 3: RX-only firmware and capture

- [ ] Verify the candidate image hash and that TX is compile-time disabled.
- [ ] Re-identify the Atom Lite FTDI VID/PID/serial immediately before upload.
- [ ] Confirm the Atom S3 Lite port is not opened.
- [ ] Observe the Gate 0B-confirmed RX/TX GPIOs and A/B with a scope or logic
      analyzer while the firmware remains RX-only; re-confirm actual receive
      mapping and automatic direction behavior without sending onto the field
      bus.
- [ ] Compare MCU bytes and the independent analyzer byte-for-byte.
- [ ] Capture at least 30 minutes passively before selecting field UART values.

Store the paired MCU/analyzer stream in the versioned NDJSON format. A complete
field capture must pass `validate_evidence_packet.py --capture ...
--require-complete`; a mismatch remains evidence of failure, not a value to
edit away.

No field TX, active baud probing, automatic replay, or payload interpretation
is permitted by this checklist.

## Required physical response format

Reply with an observation or `blocked: <reason>`; do not reply with an
unqualified yes. The first requested action is recorded in
`.arduino/next-todo.md`.
