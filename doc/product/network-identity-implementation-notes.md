# Network identity — implementation notes

**Date:** 2026-10-31  
**Scope:** Default device hostname and Soft-AP branding for **iCaNova Edge Gateway**.

## Decision

| Item | Previous default | New default | Where set |
| ---- | ---------------- | ----------- | --------- |
| Hostname | `icanova-gw` (earlier lab names may still exist in saved conf) | `icanova-edge` | `src/config/settings.cpp` factory/fallback |
| Soft-AP SSID prefix | `DSE-GW` → `DSE-GW-XXXX` | `ICANOVA-EDGE` → `ICANOVA-EDGE-XXXX` | `WIFI_AP_SSID_PREFIX` in `src/config/config.h` |
| Soft-AP password | `dsegateway` | `icanovaedge` | `WIFI_AP_PASSWORD` in `src/config/config.h` (seeded into settings) |

SSID suffix remains four hex digits derived from the chip MAC (`wifi_portal.cpp` `buildApSsid()`). Example: `ICANOVA-EDGE-A1B2`.

Display / documentation source of truth: [`product-identity.json`](./product-identity.json).  
Firmware display macros: `src/brand/product_identity.h` (`ICANOVA_DEFAULT_HOSTNAME`, `ICANOVA_SOFT_AP_SSID_PREFIX`).

## Why change Soft-AP now

Phase 1/2 left `DSE-GW` frozen for field compatibility. That hold is lifted: Soft-AP is operator-facing product identity and must match **iCaNova Edge**, not DSE equipment branding. DSE remains the supported **controller** manufacturer in Modbus/register docs only.

## Deployed-device behavior

| Situation | Behavior |
| --------- | -------- |
| Fresh flash, empty `/etc/gateway.conf` | New hostname + Soft-AP prefix + password apply |
| Existing conf with `hostname=…` | Saved hostname wins until `config set hostname icanova-edge` (or factory reset of that key) |
| Existing conf with `wifi.ap_ssid=…` | Custom SSID wins; prefix change does not override |
| Existing conf with `wifi.ap_pass=…` (or seeded pass) | Saved password wins; flashing alone does not rewrite a stored AP password |
| Empty `wifi.ap_ssid` | Runtime builds `ICANOVA-EDGE-XXXX` from the new prefix |

Operators must re-join Wi-Fi after the SSID changes. Update any printed commissioning cards and React proxy notes that assumed `DSE-GW-*` / `dsegateway`.

## Compatibility checklist

- [x] Compile-time Soft-AP prefix updated
- [x] Default Soft-AP password updated (≥ 8 chars → WPA Soft-AP, not open)
- [x] Default hostname updated
- [x] Product identity JSON / hardware identity doc updated
- [x] CLI / web UI guides updated
- [ ] Field units: plan roll-out (reflash + clear or edit conf as needed)
- [ ] Lab stickers / QR cards reprinted if they show old SSID

## Out of scope

- Auth0 / JWT audience
- Device ID / efuse MAC registration format
- Modbus, NVS key names, OTA identifiers
- Renaming persisted conf **keys** (`hostname`, `wifi.ap_ssid`, `wifi.ap_pass`)
