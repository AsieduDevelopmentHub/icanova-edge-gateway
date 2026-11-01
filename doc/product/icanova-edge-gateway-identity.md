# iCaNova Edge Gateway — product identity

Canonical naming values: [`product-identity.json`](./product-identity.json).

## Product identity

| Field | Value |
| ----- | ----- |
| Product name | iCaNova Edge Gateway |
| Product family | iCaNova Edge |
| Product category | Industrial IoT Gateway |
| Manufacturer / brand owner | iCaNova |
| Parent company | ASIEDU DEVELOPMENT HUB (ADH) |
| Tagline | Imagine. Connect. Innovate. |
| Descriptor | Intelligent technology for connected industries. |
| Primary function | Modbus RTU data acquisition and industrial equipment connectivity |
| Companion platform | iCaNova Monitoring |
| Model number | **MODEL: TBD** (provisional — no official SKU in firmware or hardware docs) |
| Hardware revision (product) | Not yet verified |
| Firmware version field | Not yet verified (no dedicated FW version string in firmware today) |

Intended application: connect industrial controllers and field equipment (initially Deep Sea Electronics 73xx-class genset controllers and related Modbus RTU devices) to local commissioning tools and optional station uplink / monitoring software.

## Hardware identity (verified from existing design docs / firmware)

| Item | Value | Source |
| ---- | ----- | ------ |
| MCU / platform (production target) | ESP32-C3 | `platformio.ini` env `esp32-c3-supermini`, `BOARD_NAME` |
| Board designation | ESP32-C3 SuperMini | `src/config/config.h` |
| Bench boards | Arduino Mega 2560; Uno / Nano (`GATEWAY_LITE`) | `platformio.ini` |
| RS485 interface | UART + DE/RE direction control | Firmware board profiles |
| ESP32-C3 RS485 pins | RX GPIO20, TX GPIO21, DE·RE GPIO4 | `config.h` / getting started |
| Transceiver (lab) | THVD1450 or level-safe RS485; MAX485 used in early benches | README / guides |
| Soft-AP SSID prefix | `ICANOVA-EDGE` → `ICANOVA-EDGE-XXXX` | `config.h` / Wi-Fi portal |
| Soft-AP password (default) | `icanovaedge` | `config.h` / settings seed |
| Default hostname | `icanova-edge` | Settings defaults |
| Device ID | ESP32 efuse MAC (12 lowercase hex) | CLI / uplink docs |
| Terminal-block configuration | Not yet verified | — |
| SIM / mobile connectivity (shipped) | Not claimed as finished product feature | README |
| Power-input specifications | Not yet verified | — |
| Input voltage / max current | Not yet verified | — |
| RS485 isolation / surge rating | Not yet verified | — |
| Environmental / IP rating | Not yet verified | — |
| Operating temperature | Not yet verified | — |
| Cellular bands | Not yet verified | — |
| Certifications / compliance | Not yet verified | — |
| Country of manufacture | Not yet verified | — |

Deep Sea Electronics (DSE) remains a supported **equipment** manufacturer. DSE model and register documentation stay under `doc/dse_register_map/`.

## Identification and labeling plan (documentation only)

Future enclosure label fields (do not fabricate artwork or change physical hardware in this phase):

1. iCaNova wordmark placeholder (text until logo approved)
2. Product name: iCaNova Edge Gateway
3. Model number (when assigned)
4. Hardware revision (when assigned)
5. Serial number (when a product serial scheme is defined; today field devices use efuse MAC as Device ID)
6. Supply input rating (when verified)
7. RS485 terminal identification
8. SIM access identification (only if productized)
9. Manufacturer: ASIEDU DEVELOPMENT HUB (ADH)
10. Country of manufacture (only when verified)
11. Regulatory marks (only after certification)

Physical direction when branding is applied: matte graphite enclosure, minimal silver or white markings, clear terminal labels. Electrical design is unchanged by branding.
