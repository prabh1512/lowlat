#!/usr/bin/env python3
"""Parse perf stat outputs and produce a comparison table."""

import re
from pathlib import Path

PERF_DIR = Path("perf-results")
OUT_DIR = Path("docs/images/benchmark-plots")
OUT_DIR.mkdir(parents=True, exist_ok=True)

runs = [
    ("v1_obv0_stl",   "v1: std::map"),
    ("v2_obv0_stl",   "v2: sorted vec"),
    ("v3_obv0_stl",   "v3: reverse vec"),
    ("v4_obv0_stl",   "v4: branchless"),
    ("v5_obv0_stl",   "v5: linear search"),
    ("v6_obv0_stl",   "v6: level pool"),
    ("v6_obv1_stl",   "v6 + array stock"),
    ("v6_obv1_absl",  "v6 + array + absl"),
]

EVENTS = ["cycles", "instructions", "branches", "branch-misses",
          "cache-references", "cache-misses",
          "L1-dcache-load-misses", "dTLB-load-misses", "LLC-load-misses"]

def parse(path):
    out = {}
    text = path.read_text()
    for ev in EVENTS:
        # match line with count, then event name
        m = re.search(rf"^\s*([\d,]+)\s+cpu_core/{re.escape(ev)}/", text, re.M)
        if m:
            out[ev] = int(m.group(1).replace(",", ""))
        else:
            out[ev] = None
    return out

header = f"{'variant':<20}{'cyc(B)':>9}{'IPC':>7}{'br-miss%':>10}{'L1-miss(B)':>12}{'dTLB(M)':>10}{'LLC(M)':>9}"
print(header)
print("-" * len(header))

lines = ["variant,cycles,instructions,IPC,branch_miss_pct,L1_dmiss,dTLB_miss,LLC_miss"]
for tag, label in runs:
    p = PERF_DIR / f"{tag}.txt"
    if not p.exists():
        print(f"{label:<20}MISSING")
        continue
    d = parse(p)
    ipc = d["instructions"] / d["cycles"] if d["cycles"] else 0
    brm_pct = (d["branch-misses"] / d["branches"] * 100) if d["branches"] else 0
    print(f"{label:<20}{d['cycles']/1e9:>9.1f}{ipc:>7.2f}{brm_pct:>10.2f}"
          f"{d['L1-dcache-load-misses']/1e9:>12.2f}{d['dTLB-load-misses']/1e6:>10.1f}"
          f"{d['LLC-load-misses']/1e6:>9.1f}")
    lines.append(f"{tag},{d['cycles']},{d['instructions']},{ipc:.3f},{brm_pct:.3f},"
                 f"{d['L1-dcache-load-misses']},{d['dTLB-load-misses']},{d['LLC-load-misses']}")

(OUT_DIR / "perf_summary.csv").write_text("\n".join(lines) + "\n")
print(f"\nwrote {OUT_DIR / 'perf_summary.csv'}")
