# GenComm capture — continuation guide

Halted **2026-11-02 ~09:00** while disconnecting the DSE. Use this file to resume cleanly.

## What is done (keep these)

| Artifact | What it contains |
| --- | --- |
| [`gencomm-pages-latest.md`](gencomm-pages-latest.md) | **Named catalog** live capture: **55** GenComm pages, register names + live values |
| `../../logs/gencomm-pages-latest.json` | Same data as structured JSON (431 ok / 145 fail read runs) |
| `../../logs/gencomm-catalog-latest.json` | Full named catalog from `/api/catalog` |
| `../../logs/gencomm-capture-status.json` | Halt status, responding page list, resume hints |
| Dated copies | `gencomm-pages-20261102-084153.*` |

Named catalog pages (firmware Soft-AP Configure tab):

`0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,18,20,25,28,29,70,72,74,76,137,142,152,153,154,156,158,160,161,170,171,172,175,177,178,179,180,181,182,183,184,185,190,191,192,194,200,240,250`

## What was started but not saved

Full **raw** dump of every page base `0..255` that answers Modbus:

- Discovery found **~243** responding page bases (not only the 55 named ones).
- Dump process reached about **page 153** (`[149/243]`) then was **killed**.
- **No** `gencomm-raw-pages-*.json/md` was written (script only flushed at the end).
- Terminal evidence: agent terminal `788861` + status JSON above.

So: **named 55 = complete**. **raw 0–255 = incomplete / not persisted**.

## Why “55” felt incomplete

`src/net/gencomm_catalog.h` / Soft-AP catalog only ships **55 named pages**.  
The 7320B answers many more page slots (raw offsets). Future work should capture **all responding pages**, then optionally expand the named catalog.

## Resume checklist

1. Reconnect DSE RS485; confirm gateway STA/HTTP (note IP may change).
2. Probe:

```bash
python -u -c "import urllib.request; print(urllib.request.urlopen('http://<GW_IP>/api/health', timeout=5).read().decode())"
python -u -c "import urllib.request; print(urllib.request.urlopen('http://<GW_IP>/api/read?addr=1024&qty=2', timeout=12).read().decode())"
```

3. Re-run **full raw** dump (preferred after script checkpoint update):

```bash
python -u tools/poll_all_gencomm_pages_raw.py --base http://<GW_IP> --sleep-ms 10
```

4. Optional: refresh named 55 only:

```bash
python -u tools/poll_gencomm_pages.py --base http://<GW_IP>
```

5. Update this file’s “last completed” notes when raw dump finishes (`gencomm-raw-pages-latest.md` + index).

## Tools

| Script | Purpose |
| --- | --- |
| `tools/poll_gencomm_pages.py` | Named catalog pages only (documented tables) |
| `tools/poll_all_gencomm_pages_raw.py` | All responding page bases 0–255, raw words 0–255 |
| `tools/capture_poll_all.py` | USB CLI `poll all` (needs console password) |

## Device context from this session

- Gateway identity: `aca704bc5cb0`
- Profile: DSE 7320 / 7320 MKII (`dse7320`), model `32841`
- Last STA IP used: `10.73.214.31` (do not assume next time)
- Slave / baud used: `1` / `9600`
