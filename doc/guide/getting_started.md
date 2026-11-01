# Getting started — iCaNova Edge Gateway

**Product:** iCaNova Edge Gateway (family **iCaNova Edge**). Multi-device Modbus RTU master (USB Serial CLI).

Primary equipment target: **Deep Sea Electronics (DSE) 7320 / 7320 MKII**. Bench slaves: Invendis ET1641 (slave 50).

Wi-Fi soft-AP + local overview UI on ESP32-C3 — see [web_ui.md](web_ui.md). Optional uplink: [uplink.md](uplink.md). Companion platform: **iCaNova Monitoring**.

---

## Why ESP32-C3 + “5V pin” can still fail CRC

Powering MAX485 **VCC from the ESP32-C3 5V/VBUS pin does not fix RX**.

| Signal | What happens |
|--------|----------------|
| MAX485 VCC = 5V | Transceiver works better than at 3.3V |
| MAX485 **RO → ESP32 RX** | RO still outputs **~5V** logic HIGH |
| ESP32-C3 GPIO | **3.3V only** (not 5V-tolerant) |

Typical symptom (matches lab logs): TX frame correct; RX shows a stable prefix like `32 84 02` then **random/wrong CRC bytes** → `CRC FAIL`. Firmware CRC math is fine (`crc-test` passes).

**Isolation test:** flash the same CLI on a **5V Arduino** (Mega recommended). If CRC becomes OK there, the ESP32 path needs a level shifter on RO (or a 3.3V RS485 transceiver) — not more firmware tweaks.

---

## Board environments (`platformio.ini`)

| Env | Board | RS485 UART | Pins (RX / TX / DE·RE) |
|-----|-------|------------|-------------------------|
| `esp32-c3-supermini` | ESP32-C3 SuperMini | UART1 | 20 / 21 / 4 |
| `mega2560` | **Arduino Mega 2560 (recommended 5V bench)** | Serial1 | 19 / 18 / 2 |
| `uno` | Arduino Uno | SoftwareSerial | 10 / 11 / 2 |
| `nanoatmega328` | Arduino Nano | SoftwareSerial | 10 / 11 / 2 |

Prefer **Mega** over Uno/Nano: true hardware UART for RS485; USB Serial stays free for the CLI.

**Mega 2560** includes the full DSE named-register map and SCF tables (strings in **PROGMEM** / flash) — same CLI as ESP32: `get`, `iterate`, `watch`, `regs`, `scf`, plus diagnostics.

**Uno / Nano** (ATmega328) stay on **`GATEWAY_LITE`**: Modbus diagnostics only (`crc-test`, `rawread`, `rs485raw`, `readhex`). The named map needs ~47KB flash and does not fit in 32KB.

---

## Wiring — ESP32-C3 → MAX485

| MAX485 | ESP32-C3 |
|--------|----------|
| VCC | **5V / VBUS** (better than 3.3V for the module) |
| GND | GND |
| DI | GPIO21 (TX) |
| RO | GPIO20 (RX) — **5V into 3.3V GPIO = risk** |
| DE+RE | GPIO4 (tied together) |

Direction: DE/RE HIGH = transmit, LOW = receive.

---

## Wiring — Arduino Mega / Uno / Nano → MAX485 (5V isolation)

| MAX485 | Mega 2560 | Uno / Nano |
|--------|-----------|------------|
| VCC | **5V** | **5V** |
| GND | GND | GND |
| DI | **18** (TX1) | **11** (Soft TX) |
| RO | **19** (RX1) | **10** (Soft RX) |
| DE+RE | **2** (tied) | **2** (tied) |

MAX485 A/B → DSE or ET1641 A/B (common GND). Swap A/B if you only see timeouts.

---

## Build / flash / monitor

```bash
# ESP32-C3
pio run -e esp32-c3-supermini -t upload --upload-port COMx
pio device monitor -e esp32-c3-supermini --port COMx

# Mega (recommended 5V test)
pio run -e mega2560 -t upload --upload-port COMx
pio device monitor -e mega2560 --port COMx

# Uno / Nano
pio run -e uno -t upload --upload-port COMx
pio run -e nanoatmega328 -t upload --upload-port COMx
```

Close PuTTY / other serial apps before upload. After flash, open the monitor (ESP32-C3: RESET if no banner).

---

## CLI overview

Full reference: [cli.md](cli.md).

```text
help
config
crc-test
device select dse7320
rs485raw 1 03 0405 1
```

Device keys: `dse7320` (default), `et1641`. Named DSE reads require `dse7320`.

---

## Phase 2 — ET1641 physical test

1. Power ET1641; Modbus slave **50**, **9600 8N1**.
2. Wire MAX485 A/B + common GND.
3. Flash/monitor.
4. CLI:

```text
devices
device select et1641
device show
crc-test
rs485raw 50 04 0000 1
rawread 50 03 0000 1
```

Interpret:

- **Normal or exception + CRC OK** → link proven.
- **CRC FAIL** with a recognizable prefix → electrical/level problem (try Arduino 5V bench).
- **Timeout** → power, A/B, slave ID, baud, DE/RE, cabling.

Address `0000` is a bus probe only — not a documented ET1641 map entry.

---

## EN-Volt

Invendis EN-Volt, slave **63**, **9600 8N1**, function 03. The measured map (three fixed images at addresses 0, 2, and 30, plus a zero window from 100) is in [en_volt.md](en_volt.md).

```text
envolt 63
rs485raw 63 03 0000 1
```

`envolt` turns Wi-Fi off until the next reset. `read` rejects those three frames; use `rs485raw` and trust the full-buffer CRC.

---

## DSE 7320 / 7320 MKII test

Confirm on the controller (or GenComm): Modbus enabled, slave ID, baud (often **9600 8N1**). Site IDs vary — override with `set slave`.

```text
device select dse7320
set slave 1
set baud 9600
crc-test
rs485raw 1 03 0405 1
readhex 0405 1
get battery_v
iterate basic
```

If you still get CRC FAIL on ESP32 but PASS on Mega with the same MAX485/cable/DSE → treat as **RO level / transceiver** issue, not Modbus software.

**Do not** send `scf stop` / key 35700 — blocked in firmware.

---

## Config

- Pins / board profiles: `src/config/config.h`
- Device profiles: `src/devices/modbus_device.cpp`
- Envs: `platformio.ini`
