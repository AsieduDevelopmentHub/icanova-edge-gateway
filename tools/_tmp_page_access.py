import json
from pathlib import Path

d = json.loads(Path("logs/gencomm-pages-latest.json").read_text(encoding="utf-8"))
print("page  r   rw  w   title")
for page in d["pages"]:
    regs = page["registers"]
    r = sum(1 for x in regs if x.get("access") == "r")
    rw = sum(1 for x in regs if x.get("access") == "rw")
    w = sum(1 for x in regs if x.get("access") == "w")
    flag = "*" if (rw + w) else " "
    print(f"{flag}p{page['page']:3d} {r:3d} {rw:3d} {w:3d}  {page['title'][:70]}")

# sample writable names that look like thresholds/fuel/voltage
keys = ("fuel", "volt", "freq", "under", "over", "trip", "level", "oil", "cool", "batt", "alarm", "timer", "delay")
print("\nWritable-ish register name hits:")
for page in d["pages"]:
    for reg in page["registers"]:
        if reg.get("access") not in ("rw", "w"):
            continue
        name = (reg.get("name") or "").lower()
        if any(k in name for k in keys):
            print(f"  p{page['page']} @{reg['addr']} [{reg['access']}] {reg['name']}")
