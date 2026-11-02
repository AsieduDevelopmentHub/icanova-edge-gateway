#!/usr/bin/env python3
"""Login to the gateway USB console, run `poll all`, and save the transcript."""

from __future__ import annotations

import argparse
import datetime as dt
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    print("pyserial is required: pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


def read_for(ser: serial.Serial, seconds: float) -> str:
    end = time.time() + seconds
    chunks: list[bytes] = []
    while time.time() < end:
        waiting = ser.in_waiting
        if waiting:
            chunks.append(ser.read(waiting))
        else:
            time.sleep(0.05)
    return b"".join(chunks).decode("utf-8", errors="replace")


def wait_until(ser: serial.Serial, needle: str, timeout: float) -> str:
    end = time.time() + timeout
    buf = ""
    while time.time() < end:
        waiting = ser.in_waiting
        if waiting:
            buf += ser.read(waiting).decode("utf-8", errors="replace")
            if needle in buf:
                return buf
        else:
            time.sleep(0.05)
    return buf


def send_line(ser: serial.Serial, line: str) -> None:
    ser.write((line + "\r\n").encode("utf-8"))
    ser.flush()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--user", default="admin")
    parser.add_argument("--password", default="admin")
    parser.add_argument(
        "--out",
        default="",
        help="Output path (default logs/poll-all-<timestamp>.log)",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    logs = root / "logs"
    logs.mkdir(exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = Path(args.out) if args.out else logs / f"poll-all-{stamp}.log"

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except serial.SerialException as exc:
        print(f"Could not open {args.port}: {exc}", file=sys.stderr)
        print("Stop any open `pio device monitor` first.", file=sys.stderr)
        return 1

    transcript: list[str] = []
    try:
        # Nudge the CDC console / idle logout prompt.
        send_line(ser, "")
        time.sleep(0.4)
        boot = read_for(ser, 1.5)
        transcript.append(boot)

        if "login:" in boot.lower() or "Password:" not in boot:
            send_line(ser, args.user)
            transcript.append(wait_until(ser, "Password:", 5.0))
            send_line(ser, args.password)
            transcript.append(wait_until(ser, "@", 8.0))
        else:
            # Already at password prompt
            send_line(ser, args.password)
            transcript.append(wait_until(ser, "@", 8.0))

        send_line(ser, "device select dse7320")
        transcript.append(wait_until(ser, "@", 5.0))
        send_line(ser, "status")
        transcript.append(wait_until(ser, "@", 5.0))
        send_line(ser, "poll all")
        # Full named map can take a while at ~20 ms/register.
        poll = wait_until(ser, "Polled ", 180.0)
        if "Polled " not in poll:
            poll += read_for(ser, 5.0)
        transcript.append(poll)
        # Drain trailing prompt
        transcript.append(read_for(ser, 1.0))
    finally:
        ser.close()

    text = "".join(transcript)
    header = (
        f"# iCaNova Edge Gateway poll all capture\n"
        f"# captured_at: {dt.datetime.now().isoformat(timespec='seconds')}\n"
        f"# port: {args.port} baud: {args.baud}\n"
        f"# command: device select dse7320; status; poll all\n"
        f"# device: DSE 7320 / 7320B class (modbus profile dse7320)\n"
        f"\n"
    )
    out.write_text(header + text, encoding="utf-8")
    print(out)
    if "Polled " in text:
        print("Capture complete.")
        return 0
    print("Capture finished but did not see 'Polled N registers.' Check the log.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
