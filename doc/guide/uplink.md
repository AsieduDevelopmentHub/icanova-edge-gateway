# NCU uplink

The gateway keeps the Modbus bus and the cloud path apart. Sampling copies the last telemetry snapshot into a fixed batch of at most 8 records. Nothing in the uplink opens a Modbus transaction, grows a queue, or keeps a socket open.

## What is sent

One HTTP POST carries the whole batch. The body is `NCU1`: a 30-byte header, AES-128-GCM ciphertext, and a 16-byte tag. The header (including the random 12-byte nonce and the sequence) is authenticated. A repeated nonce is not used; each attempt draws a new nonce. The plaintext is a short JSON list of samples (`rpm`, `batt10` in tenths of a volt, `hz10`, `kw100`, `van10`, mode, alarm count, link).

The POST goes to the station network only. Payload encryption is AES-GCM. Put the collector on the LAN, or on a host that can reach the station. The device does not open a TLS session per batch, because that is what exhausts the ESP32-C3 heap.

## Failure behaviour

- The batch lives in a static array. When it is full, the oldest sample is dropped and `dropped` increments. RAM does not grow.
- A send runs only when the batch reaches `uplink.batch`, the station has an address, and free heap is at least 24 KB.
- Connect and write are capped at about 3 seconds, then the socket is closed. The next try waits 5 s, then 15, 45, 120, 300, and 900 s. A HTTP 2xx clears the batch and resets the wait.
- A failed send keeps the same samples. `uplink now` asks for a send on the next quiet loop even if the batch is not full.
- While telemetry is refreshing the bus, the uplink waits. It does not start a second master.

## Configure

`set telemetry on` is what fills the snapshot. Then:

```text
config set uplink.host effervescent-kangaroo-10.convex.site
config set uplink.port 443
config set uplink.path /ncu/ingest
config set uplink.bearer <ingest bearer>
config set uplink.enable on
uplink
```

`uplink.bearer` is the same value as Convex `INGEST_SECRET`. `config` masks it. The gateway opens HTTPS itself and posts the sample JSON. It does not use the station collector when the bearer is set.

The LAN collector remains available when `uplink.bearer` is empty. It expects the sealed NCU1 packet on port 8080:

`uplink` prints batch fill, sequence, sent, failed, dropped, heap, and the last result. The key is stored in `/etc/gateway.conf` and masked in `config`.

`heartbeat` posts one probe immediately. It does not wait for a telemetry sample and it does not change the batch. `Heartbeat ok: http 204` means Convex, or the collector when no bearer is set, accepted the probe. The probe is not stored as telemetry.

Collector on the station side. `--print-only` decrypts the batch, answers 204, drops the telemetry, and does not update Convex. The process prints that warning on startup. It is not for production.

```text
python tools/ncu_inbox.py --key 00112233445566778899aabbccddeeff --listen 0.0.0.0:8080 --print-only
```

Store it. The example key is not a production key. `INGEST_SECRET` is read from the environment and is not passed on the command line.

```text
python tools/ncu_inbox.py --key 00112233445566778899aabbccddeeff --listen 0.0.0.0:8080
```

That process needs the `cryptography` package. A 204 response is the only ack the device needs.

When `uplink.bearer` is empty, the station collector can still receive the sealed packet. `tools/.env` supplies `INGEST_URL` and `INGEST_SECRET` and is not committed. Cloud site linking uses the assigned `device.id` (product serial). Set it on the gateway (`config set device.id icn-edge-00042`) and register the same string in Convex. If `device.id` is empty, the gateway still falls back to the efuse MAC (12 lowercase hex) for compatibility. HTTPS JSON also reports `mac` as hardware inventory only. The collector does not choose an organization. If the identity is not already registered to exactly one device and one generator, the collector answers 403 and the device keeps the batch. With a bearer set, the gateway posts that JSON to Convex itself and the collector is not on the path.

`t` in the plaintext is uptime in milliseconds, not a clock, so Convex stores `sourceTime` as null and uses the server receipt time. `rpm` is stored as-is. `batt10`, `hz10`, and `van10` are divided by 10. `kw100` is divided by 100. JSON null stays unavailable and is not stored as zero. A retried POST repeats the same `boot` and `seq`. The same sequence is not applied twice. A lower sequence for the same boot is ignored. `boot` is `esp_random()` at startup, so a delayed batch from an older boot cannot be recognized and can still replace current telemetry. The NCU1 frame is unchanged.
