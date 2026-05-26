# Hardware
> 🌐 Read this in: [한국어](./hardware.md) · **English**

## Supported Combinations

| Board | RS485 Base | Flash | Status |
|---|---|---:|---|
| AtomS3-Lite | Atom RS485 Base | estimated 8MB | Primary target |
| Atom-Lite | Atom RS485 Base | estimated 4MB | Primary target |

## Atom RS485 Base Connections

| Signal | Description | Notes |
|---|---|---|
| A | RS485 differential A | Wall pad terminal must be measured |
| B | RS485 differential B | Reversed A/B lowers RX quality |
| GND | Common ground | Required for equipment protection |
| 5V | Base power | Check interaction with USB power |

## Board Pin Differences

| Macro | AtomS3-Lite | Atom-Lite | Status |
|---|---|---|---|
| `RS485_UART_NUM` | 2 | 2 | Estimated |
| `RS485_RX_PIN` | G5 | G22 | Needs measurement |
| `RS485_TX_PIN` | G6 | G19 | Needs measurement |
| `RS485_DE_PIN` | G7 | G23 | Needs measurement |
| `STATUS_LED_PIN` | G35 | G27 | Needs measurement |
| `FACTORY_RESET_BTN_PIN` | G41 | G39 | Needs measurement |

## Termination Resistor

- Check the existing wall pad RS485 bus termination first.
- When attaching this diagnostics device in parallel, duplicate termination can reduce signal quality.
- For long cables or isolated test buses, consider 120Ω termination.

## Power and Enclosure

- USB 5V power is the default assumption.
- Power from the wall pad terminal block must be measured before documenting.
- The enclosure should expose the RS485 terminal, USB port, and BtnA access.

## TODO

- Confirm pin maps per Atom RS485 Base revision.
- Add measured photos and wiring diagrams.
