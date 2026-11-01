# Firmware OTA

Dual-slot OTA on ESP32-C3 (`ota_0` / `ota_1`). Changing the partition table requires one USB reflash; LittleFS/`/etc/gateway.conf` is wiped.

## Soft-AP upload

1. Join `ICANOVA-EDGE-…` and open `http://192.168.4.1/`.
2. **Update** tab → choose `firmware.bin` → Upload.
3. Device writes the inactive slot, verifies, reboots into the new image.

## Cloud OTA

1. Publish a release (Settings → Firmware releases, or CI).
2. On the gateway: `config set ota.enable on`, `config set ota.channel stable`, ensure `uplink.bearer` / host are set, STA online.
3. Soft-AP **Check cloud** / **Apply cloud update**, or wait for the periodic check (`ota.check_period_s`, default 3600).

Device calls `GET /ncu/ota/check?channel=…&current=…&identity=…` with the ingest bearer, then downloads the signed Convex storage URL and verifies SHA-256 before switching slots.

## CI publish

Secrets on the hardware repo:

- `CONVEX_SITE_URL` — e.g. `https://….convex.site`
- `OTA_PUBLISH_SECRET` — same value as Convex env `OTA_PUBLISH_SECRET`

```bash
pio run -e esp32-c3-supermini
python tools/upload_firmware_to_convex.py \
  --bin .pio/build/esp32-c3-supermini/firmware.bin \
  --version 0.2.0 --channel stable
```

Or tag `fw-v0.2.0` / run **Firmware OTA publish** workflow.
