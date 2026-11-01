# Web UI — ESP32 local API + overview

**Product:** iCaNova Edge Gateway (family **iCaNova Edge**; embedded page). Companion platform: **iCaNova Monitoring** (`icanova-monitoring-web`).

Phase 3A: Wi-Fi soft-AP + JSON status API + thin genset overview page on the **ESP32-C3** only. USB Serial CLI is unchanged. Mega / Uno / Nano stay RS485 benches (no Wi-Fi stack).

**Modbus note:** live values need CRC-valid RS485. If RO is still 5V into the ESP32 RX, the UI will show `link: fail` / stale data — that is a level-shifter issue, not a UI bug.

---

## Connect

1. Flash `esp32-c3-supermini`.
2. Join Wi-Fi SSID `ICANOVA-EDGE-XXXX` (suffix from chip MAC).
3. Password default: `icanovaedge` (change via `WIFI_AP_PASSWORD` in [`src/config/config.h`](../../src/config/config.h)).
4. Open **http://192.168.4.1/**

Optional STA: set `WIFI_STA_SSID` / `WIFI_STA_PASSWORD` at compile time. Soft-AP still runs; STA IP appears in `/api/status` when joined.

**Auth:** none in v1 — treat AP/LAN as trusted.

---

## Endpoints

| Method | Path | Body |
|--------|------|------|
| GET | `/` | Embedded overview. **Monitor** is live status. **Configure** is the GenComm page editor. |
| GET | `/api/status` | Full telemetry JSON (see below) |
| GET | `/api/health` | Compact Wi-Fi + link health |
| GET | `/api/catalog` | GenComm pages, register names, access, and the last captured values |
| GET | `/api/read?addr=&qty=` | FC03 read, qty 1–32. `words` is the raw register list |
| POST | `/api/write` | `{"addr":256,"words":[1]}` FC16. Addresses 4104–4105 are rejected |
| POST | `/api/control` | `{"key":35701}` system-control write. Stop (35700) and off (35776) are rejected |

Configure lists every captured GenComm page. Registers marked read/write or write-only, and unmarked registers on pages 128 and above, get a write box. Password rows send the value and its ones complement in one FC16. Read page refreshes the open page from the module. Control buttons ask before they are sent; commands that can start the set or move a breaker say so.

The React app lives in `dev/iot/icanova-monitoring-web` (not in this firmware tree). It has the same Monitor / Configure split. `pnpm dev:mock` uses the captured snapshot and does not write to a module. `pnpm dev` sends reads and writes to the gateway.

CORS `*` is enabled on `/api/*` for the React dev app.

---

## `/api/status` fields

| Field | Type | Meaning |
|-------|------|---------|
| `ok` | bool | Last refresh had all Modbus batches OK |
| `ageMs` | number | ms since last successful refresh attempt |
| `slaveId` | number | Selected Modbus slave |
| `rssi` | number | STA RSSI (0 on AP-only) |
| `wifiMode` | string | `AP` / `STA` / `AP+STA` / `off` |
| `wifiIp` | string | Preferred IP for clients |
| `uptimeMs` | number | Device uptime |
| `lastError` | string | Last refresh error (may be empty) |
| `link` | string | `ok` \| `degraded` \| `fail` \| `unknown` |
| `headline` | string | `Running` / `Stopped` / `Alarm` / … |
| `opMode` | `{code,name}` or null | Register 772 |
| `engStatus` | `{code,name}` or null | Register 1408 |
| `engRpm`, `batteryV`, `genFreq` | number or null | Scaled |
| `genVan`/`genVbn`/`genVcn` | number or null | Line-neutral V |
| `genIa`/`genIb`/`genIc` | number or null | Phase A |
| `genPTotW`, `genPTotKw`, `genPctKw` | number or null | Power / load |
| `alarms` | array | Active alarm slots only |

Scaling matches the firmware DSE map (`dse7320_registers`) — same as CLI `get` / `iterate`.

Refresh interval ≈ **2.5 s** from `Telemetry::poll()` in `loop()`.

---

## React app (`dev/iot/icanova-monitoring-web`)

```bash
cd ../../iot/icanova-monitoring-web
pnpm install
pnpm dev          # proxy to gateway
pnpm dev:mock     # mock JSON (no hardware)
```

Dev server proxies `/api` to `VITE_GW_URL` (default `http://192.168.4.1`) or uses mock via `pnpm dev:mock`. `tools/build_gencomm_catalog.py` writes `public/gencomm.json` into that repo when the folder is present.
