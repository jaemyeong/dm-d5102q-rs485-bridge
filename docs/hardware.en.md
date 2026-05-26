# Hardware
> 🌐 Read this in: [한국어](./hardware.md) · **English**

## Supported Combinations

| Board | RS485 Base | Flash | Status |
|---|---|---:|---|
| AtomS3-Lite | Atom RS485 Base | estimated 8MB | Primary target |
| Atom-Lite | Atom RS485 Base | estimated 4MB | Primary target |

## Tail485 Connections

| Signal | Description | Notes |
|---|---|---|
| A | RS485 differential A | Wall pad terminal must be measured |
| B | RS485 differential B | Reversed A/B lowers RX quality |
| GND | Common ground | Required for equipment protection |
| 5V | Base power | Check interaction with USB power |

## Board Pin Differences

| Macro | AtomS3-Lite | Atom-Lite | Status |
|---|---|---|---|
| `RS485_UART_NUM` | 2 | 2 | Configured value |
| `RS485_RX_PIN` | G1 | G32 | MCU UART RX |
| `RS485_TX_PIN` | G2 | G26 | MCU UART TX |
| `RS485_DE_PIN` | -1 | -1 | Tail485 handles direction |
| `STATUS_LED_PIN` | G35 | G27 | Built-in RGB |
| `FACTORY_RESET_BTN_PIN` | G41 | G39 | BtnA |

## Termination Resistor

Tail485 `TX/RX` labels can be confused between module labels and Arduino UART arguments. Firmware macros are defined from the MCU UART perspective used by `Serial2.begin(baud, mode, rxPin, txPin)`.

- Check the existing wall pad RS485 bus termination first.
- When attaching this diagnostics device in parallel, duplicate termination can reduce signal quality.
- For long cables or isolated test buses, consider 120Ω termination.

## Power and Enclosure

- USB 5V power is the default assumption.
- Power from the wall pad terminal block must be measured before documenting.
- The enclosure should expose the RS485 terminal, USB port, and BtnA access.

## TODO

- Add measured photos and wiring diagrams.
