# GenComm captures

## Start here next time

See **[CONTINUATION.md](CONTINUATION.md)** for halt status and exact resume commands.

## Latest completed (named catalog — 55 pages)

- Markdown: [gencomm-pages-latest.md](gencomm-pages-latest.md)
- JSON: `../../logs/gencomm-pages-latest.json`
- Catalog: `../../logs/gencomm-catalog-latest.json`
- Halt / resume status: `../../logs/gencomm-capture-status.json`

Live re-capture on 2026-11-02: **431 ok / 145 fail** Modbus read runs against DSE 7320B via gateway `10.73.214.31`.

## Not finished (raw pages 0–255)

Discovery found **~243** responding page bases. Full raw dump was halted mid-run (~page 153) **before files were saved**.

Resume after DSE is reconnected:

```bash
python -u tools/poll_all_gencomm_pages_raw.py --base http://<GW_IP> --sleep-ms 10
# if a checkpoint exists from a later run:
python -u tools/poll_all_gencomm_pages_raw.py --base http://<GW_IP> --resume
```

## Tools

| Script | Scope |
| --- | --- |
| `tools/poll_gencomm_pages.py` | Named Soft-AP catalog only (55) |
| `tools/poll_all_gencomm_pages_raw.py` | All answering page bases 0–255 (raw words) |
