#!/usr/bin/env python3
"""Poll every GenComm catalog page via the gateway HTTP API and save logs + docs.

Example:
  python -u tools/poll_gencomm_pages.py --base http://10.73.214.31
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


SENT_U = {
    0xFFF8: "reserved",
    0xFFF9: "low input",
    0xFFFA: "high input",
    0xFFFB: "bad data",
    0xFFFC: "transducer fault",
    0xFFFD: "under range",
    0xFFFE: "over range",
    0xFFFF: "unimplemented",
}
SENT_S = {
    0x7FF8: "reserved",
    0x7FF9: "low input",
    0x7FFA: "high input",
    0x7FFB: "bad data",
    0x7FFC: "transducer fault",
    0x7FFD: "under range",
    0x7FFE: "over range",
    0x7FFF: "unimplemented",
}


def log(msg: str) -> None:
    print(msg, flush=True)


def http_json(url: str, timeout: float = 8.0):
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def show_word(raw: int | None, ty: str | None) -> str:
    if raw is None:
        return "—"
    signed = bool(ty and "s" in ty.lower())
    if not signed and raw in SENT_U:
        return SENT_U[raw]
    if signed and raw in SENT_S:
        return SENT_S[raw]
    return f"{raw} (0x{raw:04X})"


def contiguous_runs(addrs: list[int], max_qty: int = 8) -> list[tuple[int, int]]:
    if not addrs:
        return []
    addrs = sorted(set(addrs))
    runs: list[tuple[int, int]] = []
    start = addrs[0]
    qty = 1
    for addr in addrs[1:]:
        if addr == start + qty and qty < max_qty:
            qty += 1
        else:
            runs.append((start, qty))
            start = addr
            qty = 1
    runs.append((start, qty))
    return runs


def read_words(base: str, address: int, qty: int) -> dict:
    url = f"{base.rstrip('/')}/api/read?addr={address}&qty={qty}"
    try:
        return http_json(url, timeout=12.0)
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        try:
            parsed = json.loads(body)
            if isinstance(parsed, dict):
                return parsed
        except json.JSONDecodeError:
            pass
        return {"result": "error", "error": body or str(exc)}
    except Exception as exc:  # noqa: BLE001
        return {"result": "error", "error": str(exc)}


def probe_modbus(base: str) -> tuple[bool, dict]:
    body = read_words(base, 1024, 2)
    return body.get("result") == "ok", body


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="http://10.73.214.31")
    parser.add_argument("--sleep-ms", type=int, default=20)
    parser.add_argument(
        "--force-live",
        action="store_true",
        help="Poll live values even if the probe fails (slow when RS485 is down).",
    )
    parser.add_argument(
        "--max-pages",
        type=int,
        default=0,
        help="Optional limit for live reads (0 = all pages).",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    logs = root / "logs"
    docs = root / "doc" / "captures"
    logs.mkdir(exist_ok=True)
    docs.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")

    log(f"Fetching health/catalog from {args.base} …")
    health = http_json(f"{args.base.rstrip('/')}/api/health")
    catalog = http_json(f"{args.base.rstrip('/')}/api/catalog", timeout=20.0)
    pages = catalog.get("pages") or []
    log(
        f"device={catalog.get('device')} model={catalog.get('model')} "
        f"pages={len(pages)} regs={sum(len(p.get('regs') or []) for p in pages)}"
    )
    log(
        f"health wifi={health.get('wifi')} ip={health.get('ip')} "
        f"link={health.get('link')} modbusLastOk={health.get('modbusLastOk')}"
    )

    live_ok, probe = probe_modbus(args.base)
    log(f"probe @1024: {probe.get('result')} {probe.get('error') or probe.get('words')}")
    live = live_ok or args.force_live
    if not live:
        log("RS485 probe failed — documenting catalog pages without live reads.")
        log("Fix wiring/slave/baud, then re-run with a healthy link (or --force-live).")

    capture = {
        "capturedAt": dt.datetime.now().isoformat(timespec="seconds"),
        "base": args.base,
        "health": health,
        "probe": probe,
        "liveReads": live,
        "device": catalog.get("device"),
        "model": catalog.get("model"),
        "formula": catalog.get("formula"),
        "pageCount": len(pages),
        "pages": [],
    }

    md: list[str] = []
    md.append(f"# GenComm page poll — {capture['capturedAt']}")
    md.append("")
    md.append(f"- Gateway: `{args.base}`")
    md.append(f"- Device: {catalog.get('device')} (model {catalog.get('model')})")
    md.append(f"- Formula: `{catalog.get('formula')}`")
    md.append(
        f"- Health: wifi={health.get('wifi')} ip={health.get('ip')} "
        f"link={health.get('link')} modbusLastOk={health.get('modbusLastOk')}"
    )
    md.append(
        f"- Live Modbus reads: {'yes' if live else 'no (catalog documentation only)'}"
    )
    md.append(f"- Probe: `{probe}`")
    md.append(f"- Pages: {len(pages)}")
    md.append("")

    page_limit = len(pages) if args.max_pages <= 0 else min(len(pages), args.max_pages)
    for index, page in enumerate(pages[:page_limit]):
        page_no = page.get("p")
        base_addr = int(page.get("b") or 0)
        title = page.get("t") or ""
        regs = page.get("regs") or []
        log(f"[{index + 1}/{page_limit}] page {page_no}: {title} ({len(regs)} regs)")

        addrs: list[int] = []
        for reg in regs:
            offset = int(reg.get("o") or 0)
            width = int(reg.get("w") or 1)
            for i in range(width):
                addrs.append(base_addr + offset + i)

        word_map: dict[int, int | None] = {a: None for a in addrs}
        errors: list[str] = []
        ok_runs = 0
        fail_runs = 0
        if live:
            for start, qty in contiguous_runs(addrs):
                body = read_words(args.base, start, qty)
                time.sleep(args.sleep_ms / 1000.0)
                if body.get("result") == "ok" and isinstance(body.get("words"), list):
                    ok_runs += 1
                    for i, word in enumerate(body["words"]):
                        word_map[start + i] = int(word)
                else:
                    fail_runs += 1
                    errors.append(
                        f"@{start} qty={qty} -> {body.get('result')}:{body.get('error') or body.get('exception')}"
                    )

        page_rows = []
        for reg in regs:
            offset = int(reg.get("o") or 0)
            width = int(reg.get("w") or 1)
            addr = base_addr + offset
            words = [word_map.get(addr + i) for i in range(width)]
            ty = reg.get("ty")
            rendered = [show_word(w, ty) for w in words] if live else ["(not read)"] * width
            page_rows.append(
                {
                    "addr": addr,
                    "offset": offset,
                    "name": reg.get("n"),
                    "access": reg.get("a"),
                    "type": ty,
                    "width": width,
                    "words": words if live else None,
                    "display": rendered,
                }
            )

        page_capture = {
            "page": page_no,
            "base": base_addr,
            "title": title,
            "okRuns": ok_runs,
            "failRuns": fail_runs,
            "errors": errors[:20],
            "registers": page_rows,
        }
        capture["pages"].append(page_capture)

        md.append(f"## Page {page_no} — {title}")
        md.append("")
        md.append(f"- Base address: `{base_addr}` (`page * 256 + offset`)")
        md.append(f"- Registers: {len(regs)}")
        if live:
            md.append(f"- Read runs: {ok_runs} ok / {fail_runs} fail")
            if errors:
                md.append(f"- First error: `{errors[0]}`")
        else:
            md.append("- Values: not read (RS485 probe failed)")
        md.append("")
        md.append("| Addr | Off | Name | Access | Type | Value |")
        md.append("| ---: | ---: | --- | --- | --- | --- |")
        for row in page_rows:
            value = "<br>".join(row["display"])
            md.append(
                f"| {row['addr']} | {row['offset']} | {row['name']} | "
                f"{row['access']} | {row['type'] or ''} | {value} |"
            )
        md.append("")

    # Always keep a raw catalog snapshot beside the poll.
    catalog_path = logs / f"gencomm-catalog-{stamp}.json"
    catalog_path.write_text(json.dumps(catalog, indent=2), encoding="utf-8")

    json_path = logs / f"gencomm-pages-{stamp}.json"
    md_path = docs / f"gencomm-pages-{stamp}.md"
    latest_json = logs / "gencomm-pages-latest.json"
    latest_md = docs / "gencomm-pages-latest.md"
    latest_catalog = logs / "gencomm-catalog-latest.json"

    json_text = json.dumps(capture, indent=2)
    md_text = "\n".join(md) + "\n"
    json_path.write_text(json_text, encoding="utf-8")
    md_path.write_text(md_text, encoding="utf-8")
    latest_json.write_text(json_text, encoding="utf-8")
    latest_md.write_text(md_text, encoding="utf-8")
    latest_catalog.write_text(json.dumps(catalog, indent=2), encoding="utf-8")

    # Short index doc for the captures folder.
    index = docs / "README.md"
    index.write_text(
        "# GenComm captures\n\n"
        "Generated by `tools/poll_gencomm_pages.py` against a live gateway HTTP API.\n\n"
        f"- Latest markdown: [{latest_md.name}]({latest_md.name})\n"
        f"- Latest JSON: `../../logs/{latest_json.name}`\n"
        f"- Latest catalog: `../../logs/{latest_catalog.name}`\n"
        f"- This run: [{md_path.name}]({md_path.name})\n",
        encoding="utf-8",
    )

    log(str(json_path))
    log(str(md_path))
    log(str(catalog_path))
    total_fail = sum(p["failRuns"] for p in capture["pages"])
    total_ok = sum(p["okRuns"] for p in capture["pages"])
    log(f"Done. live={live} read runs ok={total_ok} fail={total_fail}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
