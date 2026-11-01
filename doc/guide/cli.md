# CLI usage — DSE 7320 Modbus RTU Gateway

USB Serial command line on the ESP32-C3. The firmware echoes what you type (leave the monitor's local echo off). After login the prompt is `user@host:path$`.

This is a local console login, the same account idea as SSH: username, hidden password, logout, and an idle timeout. It is not a network SSH server. Factory login is `admin` / `admin` until you change it with `passwd`.

```text
login: admin
Password:

=====================================
             iCaNova
       INDUSTRIAL EDGE GATEWAY
=====================================
Imagine. Connect. Innovate.

Board: ESP32-C3 SuperMini
Device ID: ..................
Communication: RS485 / Modbus RTU
Logged in as admin
...
admin@icanova-edge:/etc$ help
cat motd
cat conf
edit conf
config reload
reboot
logout
```

Startup banner identifies **iCaNova Edge Gateway** (family **iCaNova Edge**). Until a final logo is approved, the console uses the text wordmark **iCaNova** (see `doc/brand/assets/logo/README.md`).

Line editing: Left/Right, Home/End, Up/Down history, Backspace, Delete, Tab completion, Ctrl-U (clear line), Ctrl-W (delete word), `clear` or `cls` or Ctrl-L (clear screen), Ctrl-C (cancel the line), Ctrl-D on an empty line (logout).

Saved settings live in `/etc/gateway.conf`. `config set` writes that file and applies it. You can also `edit` the file, then `config reload`. `config` masks passwords; `config get wifi.ap_pass` shows the real value. Accounts are separate and are changed only with `passwd` and `user`.

`info` prints the chip, both MAC addresses, flash, free heap, and uptime. `wifi` prints whether the access point is up and whether the station link is connected, plus IP, gateway, DNS, channel, and RSSI.

```text
wifi
wifi mode apsta
wifi join MyNetwork secret
wifi scan
wifi mode sta
wifi disconnect
```

`wifi.mode` is `auto` until you change it. `auto` keeps the setup AP and also joins a station network when `wifi.sta_ssid` is set. `wifi mode sta` turns the AP off. `wifi mode ap` keeps the AP and does not join. `wifi join` from `ap` or `off` switches to `apsta` so the setup AP stays up while the station connects.

`edit` is a line editor (`?` lists `a`, `s`, `i`, `d`, `w`, `wq`). A full-screen editor fights this USB link; the line editor keeps the text you can see matched to the text that runs.

This firmware talks to **DSE controllers** (DSE 7320, DSE 7320 MKII, and related 73xx variants). The named map under `dse7320` is that DSE family. DSE’s shared Modbus layout is sometimes called GenComm in vendor PDFs; this project does **not** target a separate GenComm board.

Open with PlatformIO (RTS/DTR off so native USB CDC is not held in reset):

```bash
pio device monitor --port COM3
```

Host helper (read-only watch): `python tools/dse_watch.py`

On-device loop: `watch` (same idea: comm / regs / SCF flags / alarms).

---

## Defaults

| Setting | Value | Change at runtime |
|---------|-------|-------------------|
| Device | `dse7320` | `device select <key>` |
| Slave ID | **1** | `set slave <id>` |
| Baud | **9600** 8N1 | `set baud <4800\|9600\|19200>` |
| Function | FC **03** (holding) | `set fc 03` or `set fc 04` |

`device select` reloads slave ID and baud from the profile (overrides are lost).

Other profile: `et1641` (slave 50, 9600 8N1). Named DSE commands (`get`, `iterate`, `watch`, `scf`, `alarms`) require `dse7320`.

---

## Command index

| Command | What it does |
|---------|----------------|
| `help` / `?` | On-device command list |
| `status` | Device, slave, baud, FC, diagnostic |
| `config` | Device + Modbus + RS485 pins |
| `devices` | Same as `device list` |
| `device list` | All profiles |
| `device show` | Selected profile |
| `device select <key>` | Select by name, id, or index (`0`, `dse7320`, `et1641`) |
| `set slave <id>` | Slave 1–247 |
| `set fc <03\|04>` | Holding vs input registers |
| `set baud <rate>` | 4800 / 9600 / 19200 |
| `regs` / `registers` | Named DSE map |
| `get <key\|#\|addr>` | One named scaled read |
| `poll [mode]` | One-by-one named reads |
| `iterate [mode]` | **Batch** FC03 reads, human lines |
| `iterate <n> [mode]` | Repeat iterate 1–50 times |
| `watch [n]` / `loop [n]` | Loop: comm + batch regs + SCF flags + alarms |
| `alarms` | Active DSE alarms only |
| `scf list` | Control command names (no stop) |
| `scf flags` | Read support flags 4096–4103 |
| `scf <cmd>` | FC16 control write (see safety) |
| `scf raw <key>` | Numeric SCF key (35700 blocked) |
| `read <addr> <qty>` | Decimal address, qty 1–32 |
| `readhex <hex> <qty>` | Hex address, qty 1–32 |
| `rawread <slave> <fc> <hex> <qty>` | Ignore selected slave/FC |
| `rs485raw …` | TX/RX hex dump, no Modbus parse |
| `rs485raw-repeat <n> …` | Repeat raw capture, n=1–100 |
| `repeat <n> …` | Repeat parsed read, n=1–100 |
| `crc-test` / `crctest` | Offline CRC self-test |
| `diagnostic on\|off` | Verbose TX/RX (default off) |
| `uplink` / `uplink now` | Encrypted batch status, or send the current batch |
| `uplink heartbeat` / `heartbeat` | Post one probe and print whether the collector accepted it |

Commands are case-insensitive. Empty lines are ignored.

---

## Session

```text
help
status
config
devices
device select dse7320
set slave 1
set baud 9600
```

---

## Readable DSE values

Use these when the DSE is selected. Scaling is `raw × factor` (hours: seconds ÷ 3600).

### `regs`

Prints every mapped key, address, qty, and name.

### `get`

```text
get battery_v
get eng_rpm
get gen_freq
get 1029
get 1
```

Lookup order: map **key**, then 1-based **row** from `regs`, then **decimal address**. So `get 1` is the first map row, not holding address 1.

Common keys:

| Key | Meaning | Typical unit |
|-----|---------|--------------|
| `battery_v` | Battery / starter voltage | V |
| `charge_alt_v` | Charge alternator voltage | V |
| `eng_rpm` | Engine speed | rpm |
| `eng_status` | Engine state (enum) | — |
| `op_mode` | Front-panel / switch mode | — |
| `gen_freq` | Generator frequency | Hz |
| `gen_van` / `gen_vbn` / `gen_vcn` | L1 / L2 / L3 to N | V |
| `gen_vab` / `gen_vbc` / `gen_vca` | Line-line | V |
| `gen_ia` / `gen_ib` / `gen_ic` | Phase currents | A |
| `gen_p_tot` | Total real power | W |
| `oil_press` / `coolant_temp` / `fuel_level` | Engine | kPa / °C / % |
| `eng_hours` / `eng_starts` | Hours / start count | h / count |
| `mains_freq` / `mains_van` | Utility L-N | Hz / V |
| `mains_vab` / `mains_vbc` / `mains_vca` | Utility line-line | V |

Full table: [register_map.md](../dse_register_map/register_map.md).

### `iterate` — batch (preferred live dump)

Quiet multi-register FC03, then lines like `Line 1 voltage: 220.0 V`.

```text
iterate
iterate basic
iterate power
iterate engine
iterate all
iterate 3 basic
```

| Mode | Batches |
|------|---------|
| `basic` (default) | Identity 768×8, instrumentation 1024×32, 1056×16 (through mains L-L), engine status 1408×1 |
| `power` | 1536×23 (kW / PF), 1798×12 (hours / kWh / starts) |
| `engine` | basic + oil/coolant level 1357×3 |
| `all` | basic + power + engine |

Max 32 registers per Modbus request.

### `poll` — one register per request

Slower; useful when a batch address is illegal.

```text
poll
poll basic
poll engine
poll power
poll alarms
poll leds
poll identity
poll scf
poll state
poll all
```

### `watch` — live loop

```text
watch
watch 8
loop 4
```

Default **8** cycles (max **40**), **3 s** apart. Type any character to abort between cycles.

Each cycle:

1. Prints slave / baud / FC03  
2. Quiet batch FC03: `768×8`, `1024×32`, `1056×16`, `1408×1`, `1536×23`, `1798×12`  
3. Reads **SCF flags only** (no control writes)  
4. Batch-reads alarm page 154 (`39425×21`) and prints **active** slots  
5. Prints `LINK: OK` or `LINK: DEGRADED`

Does not read oil/coolant levels at `1357` (use `iterate engine` for those).

Host equivalent: `python tools/dse_watch.py` (never sends stop).

---

## Alarms

```text
alarms
poll alarms
get alm_39425
```

Page 154: each holding register holds four 4-bit slots (0 disabled, 1 healthy, 2 warning, 3 shutdown, 4 electrical trip, 15 unimplemented). `alarms` / watch print non-healthy slots with names.

---

## SCF (DSE control)

Writes **key + (65535−key)** to holding **4104/4105** in one **FC16**. That can change mode or start/stop telemetry on a live DSE controller.

Always read flags first:

```text
scf list
scf flags
```

| CLI name | Key | Action |
|----------|-----|--------|
| `auto` | 35701 | Auto mode |
| `manual` | 35702 | Manual mode |
| `test_on_load` | 35703 | Test on load |
| `auto_restore` | 35704 | Auto + manual restore |
| `start_engine` | 35705 | Start if Manual/Test |
| `mute_alarm` | 35706 | Mute |
| `reset_alarms` | 35707 | Reset alarms |
| `transfer_gen` | 35708 | Transfer to generator |
| `transfer_mains` | 35709 | Transfer to mains |
| `telemetry_start` | 35732 | Telemetry start (Auto) |
| `telemetry_stop` | 35733 | Cancel telemetry start |
| `reset_alarms2` | 35734 | Reset alarms (alt) |
| `clear_telem_alarm` | 35735 | Clear telemetry alarm flag |
| `lock_controls` | 35736 | Lock panel |
| `unlock_controls` | 35737 | Unlock panel |
| `reset_maint` | 35738 | Reset maintenance 1 timers |

```text
scf auto
scf telemetry_start
scf raw 35703
```

**Not in this firmware:** `scf stop` / key **35700**. `scf raw 35700` is rejected. Do not use this gateway to stop a running generator.

---

## Raw Modbus / bus debug

`read` / `readhex` use the **selected** slave and FC. Quantity 1–32. On `dse7320`, a matching map address also prints a scaled `VALUE:` line.

```text
read 1029 1
readhex 0405 1
rawread 1 03 0405 1
rs485raw 1 03 0405 1
rs485raw-repeat 5 1 03 0405 1
repeat 10 1 03 0405 1
crc-test
diagnostic on
```

| If you see | Meaning |
|------------|---------|
| `VALID MODBUS RESPONSE` + `Register[0]=0x…` | Link + decode OK |
| `VALID MODBUS EXCEPTION` + CRC PASS | Slave heard you; address/FC may be illegal |
| `TIMEOUT` / RX length 0 | No slave reply (A/B, id, baud, wiring) |
| `INVALID MODBUS FRAME (CRC)` | Noise or extra device; **not** a valid exception |
| `PARTIAL` | Truncated RX |

`rs485raw` prints TX/RX hex only. Use it to prove the wire before trusting `get` / `iterate`.

---

## Typical sequences

**Prove the DSE is on the bus (read only):**

```text
device select dse7320
status
rs485raw 1 03 0300 1
rs485raw 1 03 0405 1
```

**Readable live values:**

```text
iterate basic
watch 4
get battery_v
get eng_status
alarms
scf flags
```

**ET1641 probe (other slave):**

```text
device select et1641
rawread 50 03 0000 1
```

**EN-Volt (slave 63).** `envolt` scans the image map and turns Wi-Fi off until reset. Map: [en_volt.md](en_volt.md).

```text
envolt 63
rs485raw 63 03 0000 1
rs485raw 63 03 0002 1
rs485raw 63 03 001E 1
```

---

## Related

| Doc | Contents |
|-----|----------|
| [getting_started.md](getting_started.md) | Wiring, flash, ET1641 procedure |
| [uplink.md](uplink.md) | Encrypted batched uplink |
| [en_volt.md](en_volt.md) | Measured EN-Volt register map (slave 63) |
| [register_map.md](../dse_register_map/register_map.md) | Named registers + scaling |
| [register_clone_dse8610.md](../dse_register_map/register_clone_dse8610.md) | Shared DSE map notes (8610 docs overlap 7320) |
