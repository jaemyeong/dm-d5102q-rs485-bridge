# Firmware Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Arduino/ESP32 firmware for the DM-D5102Q RS485 TCP Bridge PRD.

**Architecture:** Keep `shared/src` as the common firmware source of truth, then sync it into each board sketch tree. Board-only `.ino`, `board_config.h`, `secrets.h.example`, and `partitions.csv` live under `firmware/<board>/`.

**Tech Stack:** Arduino C++17, ESP32 WiFi, LittleFS, Preferences NVS, ESPAsyncWebServer, AsyncWebSocket, AsyncTCP, ArduinoJson, Update OTA.

---

### Task 1: Structural Firmware Test

**Files:**
- Create: `tests/firmware_structure.test.mjs`

- [ ] **Step 1: Write failing test**

Run: `node tests/firmware_structure.test.mjs`

Expected: FAIL until firmware files exist.

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/plans/2026-05-26-firmware-core.md tests/firmware_structure.test.mjs
git commit -m "test: add firmware structure validation"
```

### Task 2: Shared Firmware Modules

**Files:**
- Create common modules under `shared/src/{app,network,rs485,scanner,status,storage,utils,web}`.

- [ ] **Step 1: Implement config, metrics, HEX, queues, RS485, Wi-Fi, TCP bridge, scanner, web API, OTA, and scheduler modules**
- [ ] **Step 2: Run `node tests/firmware_structure.test.mjs`**
- [ ] **Step 3: Commit**

```bash
git add shared/src
git commit -m "feat: implement shared firmware core"
```

### Task 3: Board Sketches and Sync

**Files:**
- Create board-only files under `firmware/atoms3-lite` and `firmware/atom-lite`.
- Create: `tools/sync_shared.sh`

- [ ] **Step 1: Add board configs, sketches, secrets examples, partitions**
- [ ] **Step 2: Sync `shared/src` and `shared/data` into both firmware trees**
- [ ] **Step 3: Run verification**
- [ ] **Step 4: Commit**

```bash
git add firmware tools/sync_shared.sh
git commit -m "feat: add board firmware sketches"
```
