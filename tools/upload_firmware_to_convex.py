#!/usr/bin/env python3
"""Build-artifact publisher: upload a firmware .bin to Convex storage and publish a release.

Requires:
  CONVEX_SITE_URL   e.g. https://happy-animal-123.convex.site
  OTA_PUBLISH_SECRET

Example:
  python tools/upload_firmware_to_convex.py \\
    --bin .pio/build/esp32-c3-supermini/firmware.bin \\
    --version 0.2.0 --channel stable --git-hash abcdef12
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import urllib.error
import urllib.request


def post_json(url: str, secret: str, payload: dict) -> dict:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        method="POST",
        headers={
            "Authorization": f"Bearer {secret}",
            "Content-Type": "application/json",
        },
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        body = resp.read().decode("utf-8")
        return json.loads(body) if body else {}


def main() -> int:
    parser = argparse.ArgumentParser(description="Upload firmware.bin to Convex OTA")
    parser.add_argument("--bin", required=True, help="Path to firmware.bin")
    parser.add_argument("--version", required=True, help="Semver-like version string")
    parser.add_argument(
        "--channel",
        default="stable",
        choices=("stable", "beta"),
        help="Release channel",
    )
    parser.add_argument("--git-hash", default="", help="Short git hash")
    parser.add_argument("--notes", default="", help="Optional release notes")
    parser.add_argument(
        "--site-url",
        default=os.environ.get("CONVEX_SITE_URL", "").rstrip("/"),
        help="Convex .site URL (or CONVEX_SITE_URL)",
    )
    parser.add_argument(
        "--secret",
        default=os.environ.get("OTA_PUBLISH_SECRET", ""),
        help="OTA_PUBLISH_SECRET bearer",
    )
    args = parser.parse_args()

    if not args.site_url:
        print("CONVEX_SITE_URL / --site-url is required", file=sys.stderr)
        return 2
    if not args.secret:
        print("OTA_PUBLISH_SECRET / --secret is required", file=sys.stderr)
        return 2
    if not os.path.isfile(args.bin):
        print(f"Missing firmware file: {args.bin}", file=sys.stderr)
        return 2

    with open(args.bin, "rb") as f:
        blob = f.read()
    sha256 = hashlib.sha256(blob).hexdigest()
    size = len(blob)
    publish_url = f"{args.site_url}/ncu/ota/publish"

    print(f"Requesting upload URL ({size} bytes, sha256={sha256[:12]}…)")
    try:
        upload = post_json(publish_url, args.secret, {"action": "uploadUrl"})
    except urllib.error.HTTPError as exc:
        print(f"uploadUrl failed: {exc.read().decode('utf-8', errors='replace')}", file=sys.stderr)
        return 1
    upload_url = upload.get("uploadUrl")
    if not upload_url:
        print(f"Unexpected uploadUrl response: {upload}", file=sys.stderr)
        return 1

    print("Uploading binary…")
    req = urllib.request.Request(
        upload_url,
        data=blob,
        method="POST",
        headers={"Content-Type": "application/octet-stream"},
    )
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            stored = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        print(f"binary upload failed: {exc.read().decode('utf-8', errors='replace')}", file=sys.stderr)
        return 1
    storage_id = stored.get("storageId")
    if not storage_id:
        print(f"Unexpected storage response: {stored}", file=sys.stderr)
        return 1

    payload = {
        "action": "publish",
        "version": args.version,
        "channel": args.channel,
        "storageId": storage_id,
        "sha256": sha256,
        "size": size,
    }
    if args.git_hash:
        payload["gitHash"] = args.git_hash
    if args.notes:
        payload["notes"] = args.notes

    print(f"Publishing {args.version} on {args.channel}…")
    try:
        result = post_json(publish_url, args.secret, payload)
    except urllib.error.HTTPError as exc:
        print(f"publish failed: {exc.read().decode('utf-8', errors='replace')}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
