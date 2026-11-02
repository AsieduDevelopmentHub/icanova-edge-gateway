#!/usr/bin/env python3
"""Capture EVERY GenComm page base 0..255 from a live gateway (raw words).

Unlike poll_gencomm_pages.py (named catalog only), this dumps all page slots
the controller answers, including pages not present in gencomm_catalog.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import time
import urllib.error
import urllib.request
from pathlib import Path


def log(msg: str) -> None:
    print(msg, flush=True)


def http_json(url: str, timeout: float = 12.0):
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


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


def dump_page(base: str, page: int, sleep_ms: int) -> dict:
    words: list[int | None] = [None] * 256
    ok = 0
    fail = 0
    errors: list[str] = []
    # Prefer larger batches; fall back per-word on exception.
    offset = 0
    while offset < 256:
        qty = min(32, 256 - offset)
        addr = page * 256 + offset
        body = read_words(base, addr, qty)
        time.sleep(sleep_ms / 1000.0)
        if body.get("result") == "ok" and isinstance(body.get("words"), list):
            for i, w in enumerate(body["words"]):
                if offset + i < 256:
                    words[offset + i] = int(w)
                    ok += 1
            offset += len(body["words"]) if body["words"] else qty
            continue
        # Retry in smaller chunks, then one-by-one.
        recovered = 0
        for step in (8, 1):
            if recovered:
                break
            pos = 0
            while pos < qty:
                n = min(step, qty - pos)
                one = read_words(base, addr + pos, n)
                time.sleep(sleep_ms / 1000.0)
                if one.get("result") == "ok" and isinstance(one.get("words"), list):
                    for i, w in enumerate(one["words"]):
                        words[offset + pos + i] = int(w)
                        ok += 1
                        recovered += 1
                    pos += len(one["words"]) if one["words"] else n
                else:
                    if step == 1:
                        fail += 1
                        if len(errors) < 12:
                            errors.append(
                                f"@{addr + pos} -> {one.get('result')}:{one.get('error') or one.get('exception')}"
                            )
                    pos += n
        if not recovered and len(errors) < 12:
            errors.append(
                f"@{addr} qty={qty} -> {body.get('result')}:{body.get('error') or body.get('exception')}"
            )
        offset += qty
    readable = sum(1 for w in words if w is not None)
    return {
        "page": page,
        "base": page * 256,
        "okWords": ok,
        "failWords": fail,
        "readableWords": readable,
        "errors": errors,
        "words": words,
    }


def write_checkpoint(path: Path, capture: dict) -> None:
    path.write_text(json.dumps(capture, indent=2), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="http://10.73.214.31")
    parser.add_argument("--sleep-ms", type=int, default=15)
    parser.add_argument("--from-page", type=int, default=0)
    parser.add_argument("--to-page", type=int, default=255)
    parser.add_argument(
        "--resume",
        action="store_true",
        help="Resume from logs/gencomm-raw-pages-checkpoint.json if present.",
    )
    parser.add_argument(
        "--skip-discover",
        action="store_true",
        help="Use page list from checkpoint/status instead of rediscovering.",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    logs = root / "logs"
    docs = root / "doc" / "captures"
    logs.mkdir(exist_ok=True)
    docs.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    checkpoint_path = logs / "gencomm-raw-pages-checkpoint.json"
    status_path = logs / "gencomm-capture-status.json"

    health = http_json(f"{args.base.rstrip('/')}/api/health")
    try:
        catalog = http_json(f"{args.base.rstrip('/')}/api/catalog", timeout=20.0)
        named = {int(p["p"]): p.get("t") or "" for p in (catalog.get("pages") or [])}
    except Exception as exc:  # noqa: BLE001
        catalog = None
        named = {}
        log(f"catalog unavailable: {exc}")

    pages_out: list[dict] = []
    done_pages: set[int] = set()
    present: list[int] = []

    if args.resume and checkpoint_path.exists():
        prior = json.loads(checkpoint_path.read_text(encoding="utf-8"))
        pages_out = list(prior.get("pages") or [])
        done_pages = {int(p["page"]) for p in pages_out}
        present = list(prior.get("rawPagesResponding") or [])
        log(f"resuming checkpoint: {len(done_pages)} pages already dumped")
    elif args.skip_discover and status_path.exists():
        status = json.loads(status_path.read_text(encoding="utf-8"))
        present = list(status.get("rawPagesResponding") or [])
        log(f"using status page list: {len(present)} pages")

    log(f"health={health}")
    log(f"named catalog pages={len(named)}")

    if not present:
        log(f"scanning raw pages {args.from_page}..{args.to_page}")
        for page in range(args.from_page, args.to_page + 1):
            body = read_words(args.base, page * 256, 1)
            time.sleep(args.sleep_ms / 1000.0)
            if body.get("result") == "ok":
                present.append(page)
                if page % 16 == 0:
                    log(f"  discovered through page {page} ({len(present)} hits so far)")
    present = [p for p in present if args.from_page <= p <= args.to_page]
    log(f"page bases answering at offset0: {len(present)}")

    capture = {
        "capturedAt": dt.datetime.now().isoformat(timespec="seconds"),
        "base": args.base,
        "health": health,
        "namedCatalogPages": len(named),
        "rawPagesResponding": present,
        "pageCount": 0,
        "pages": pages_out,
        "partial": True,
    }

    todo = [p for p in present if p not in done_pages]
    for i, page in enumerate(todo):
        title = named.get(page, "(not in named catalog)")
        log(f"[{i + 1}/{len(todo)}] dumping page {page} — {title}")
        dumped = dump_page(args.base, page, args.sleep_ms)
        dumped["title"] = title
        dumped["inNamedCatalog"] = page in named
        pages_out.append(dumped)
        capture["pages"] = pages_out
        capture["pageCount"] = len(pages_out)
        capture["capturedAt"] = dt.datetime.now().isoformat(timespec="seconds")
        write_checkpoint(checkpoint_path, capture)

    capture["partial"] = False
    # Build markdown from all dumped pages.
    md: list[str] = []
    md.append(f"# Full GenComm raw page dump — {capture['capturedAt']}")
    md.append("")
    md.append(f"- Gateway: `{args.base}`")
    md.append(
        f"- Health: wifi={health.get('wifi')} ip={health.get('ip')} "
        f"link={health.get('link')} modbusLastOk={health.get('modbusLastOk')}"
    )
    md.append(f"- Named catalog pages: {len(named)}")
    md.append(f"- Raw page bases with live response: {len(present)}")
    md.append(f"- Pages dumped: {len(pages_out)}")
    md.append(f"- Page list: `{present}`")
    md.append("")
    md.append(
        "Each page dumps offsets `0..255` (address = page*256+offset). "
        "`65535` / sentinel values are left as raw integers."
    )
    md.append("")
    for dumped in pages_out:
        page = int(dumped["page"])
        title = dumped.get("title") or named.get(page, "(not in named catalog)")
        md.append(f"## Page {page} — {title}")
        md.append("")
        md.append(f"- Base: `{dumped['base']}`")
        md.append(
            f"- Words read: {dumped['readableWords']}/256 "
            f"(ok={dumped['okWords']} fail={dumped['failWords']})"
        )
        md.append(f"- In named catalog: {'yes' if dumped.get('inNamedCatalog') else 'no'}")
        if dumped.get("errors"):
            md.append(f"- First errors: `{dumped['errors'][0]}`")
        md.append("")
        md.append("| Off | Addr | Value |")
        md.append("| ---: | ---: | ---: |")
        for off, word in enumerate(dumped.get("words") or []):
            if word is None:
                continue
            md.append(f"| {off} | {page * 256 + off} | {word} |")
        md.append("")

    json_path = logs / f"gencomm-raw-pages-{stamp}.json"
    md_path = docs / f"gencomm-raw-pages-{stamp}.md"
    latest_json = logs / "gencomm-raw-pages-latest.json"
    latest_md = docs / "gencomm-raw-pages-latest.md"

    json_text = json.dumps(capture, indent=2)
    md_text = "\n".join(md) + "\n"
    json_path.write_text(json_text, encoding="utf-8")
    md_path.write_text(md_text, encoding="utf-8")
    latest_json.write_text(json_text, encoding="utf-8")
    latest_md.write_text(md_text, encoding="utf-8")
    write_checkpoint(checkpoint_path, capture)

    missing = [p for p in present if p not in named]
    index = docs / "gencomm-raw-pages-index.md"
    index.write_text(
        "# Full raw GenComm page index\n\n"
        f"- Captured: {capture['capturedAt']}\n"
        f"- Responding page bases: **{len(present)}** / 256\n"
        f"- Named catalog pages: **{len(named)}**\n"
        f"- Responding but unnamed: **{len(missing)}** → `{missing}`\n\n"
        f"- Latest dump: [{latest_md.name}]({latest_md.name})\n"
        f"- Latest JSON: `../../logs/{latest_json.name}`\n"
        f"- Checkpoint: `../../logs/{checkpoint_path.name}`\n"
        f"- This run: [{md_path.name}]({md_path.name})\n",
        encoding="utf-8",
    )

    log(str(json_path))
    log(str(md_path))
    log(str(index))
    log(
        f"Done. responding_pages={len(present)} dumped={len(pages_out)} "
        f"named={len(named)} unnamed_extra={len(missing)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
