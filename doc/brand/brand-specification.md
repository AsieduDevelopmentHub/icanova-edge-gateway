# iCaNova brand specification

**Brand:** iCaNova  
**Tagline:** Imagine. Connect. Innovate.  
**Descriptor:** Intelligent technology for connected industries.  
**Parent:** ASIEDU DEVELOPMENT HUB (ADH)  
**Design direction:** Industrial Precision + Modern Technology  

Capitalization is always `iCaNova` in user-facing copy. Technical identifiers may be lowercase (`icanova-edge-gateway`, `icanova-monitoring-web`).

This file summarizes the approved identity. The full implementation specification lives in [README.md](README.md).

## Products in this workspace

| Product | Role |
| ------- | ---- |
| iCaNova Edge Gateway | Embedded industrial edge gateway (this repository) |
| iCaNova Monitoring Platform | Web monitoring and device management (local folder `icanova-monitoring-web`) |

Deep Sea Electronics (DSE) remains a supported equipment manufacturer. DSE model names, register maps, and protocol notes stay as equipment references — they are not the platform brand.

## Logo

Final logo is on hold. Until approved, use a clean text wordmark: **iCaNova**. Do not invent symbols.

## Compatibility notes

Left unchanged on purpose (compatibility contracts):

- Soft-AP SSID prefix `ICANOVA-EDGE` (see `doc/product/network-identity-implementation-notes.md`).
- Auth JWT audience and issuer (set by environment / Auth0).
- Modbus addresses, SCF keys, GenComm pages, NVS/config key names.
- GitHub remote repository names until an authorized rename.