# iCaNova Rebranding Report

**Date:** 2026-10-31  
**Classification:** Product branding + network identity defaults on iCaNova; local folder rename and Auth0 operator items remain.

## 1. Repository migration

| Item | Previous | New | Status |
| ---- | -------- | --- | ------ |
| Gateway GitHub | `AsieduDevelopmentHub/dse-modbus-gateway` | `icanova-edge-gateway` | Done |
| Web GitHub | `AsieduDevelopmentHub/dse-gateway-web` | `icanova-monitoring-web` | Done |
| Local directories | `dse-modbus-gateway`, `dse-gateway-web` | `icanova-edge-gateway`, `icanova-monitoring-web` | Pending — Cursor/IDE may lock folders while open; use `D:\dev\rename-icanova-folders.ps1` after closing the workspace |
| Git remotes | old URLs | new canonical URLs | Done |
| package.json (web) | `dse-gateway-web` | `icanova-monitoring-web` | Done |

## 2. Branding implementation

| Component | Status |
| --------- | ------ |
| Brand identity documentation (`doc/brand/`) | Done |
| Product identity (`doc/product/`) | Done |
| Application title / metadata | Done |
| Navigation branding | Done |
| Colour tokens | Done (web CSS + embedded overview) |
| Typography | Done (Manrope / Inter / IBM Plex Mono on web) |
| Gateway documentation / README | Done |
| Web documentation / README | Done |
| Hardware identity references | Documented; PCB/electrical unchanged |
| Temporary wordmark | Done (text `iCaNova`) |
| Default hostname | **`icanova-edge`** (was `icanova-gw`) |
| Soft-AP SSID prefix | **`ICANOVA-EDGE`** (was `DSE-GW`) |
| Soft-AP password default | **`icanovaedge`** (was `dsegateway`) |
| Auth JWT audience | **Unchanged in code** (fail-closed; env-driven) |
| DSE equipment / register docs | **Preserved** |

Network migration detail: [`doc/product/network-identity-implementation-notes.md`](../product/network-identity-implementation-notes.md).

## 3. Technical verification

| Test | Result |
| ---- | ------ |
| Gateway build (`pio run -e esp32-c3-supermini`) | Re-run after network identity change |
| Web build / unit tests | Untouched by Soft-AP/hostname change |
| Authentication verification | Staging uses Auth0 audience `icanova` (operator-configured) |
| Organization isolation | Untouched |
| Device communication | Untouched (Modbus / GenComm / SCF preserved) |

## 4. Outstanding work

1. Local directory rename after closing Cursor workspace (script ready).
2. Operator decision if any remaining environments still use a non-`icanova` audience — rotate only with coordinated Convex + SPA + Auth0 change.
3. Field Soft-AP cutover for units that still advertise `DSE-GW-*` / password `dsegateway` (see implementation notes).
4. Final logo artwork when approved.
5. Full UI/UX redesign (explicitly out of scope for Phase 1/2).

## 5. Final assessment

**PARTIALLY COMPLETED** — GitHub names, remotes, product identity, Soft-AP/hostname defaults, tokens, and safe firmware/web strings are on iCaNova. Local folder rename and Auth0 operator migrations remain.
