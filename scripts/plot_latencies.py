#!/usr/bin/env python3
"""Plot per-operation latency distributions from cycle dumps."""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

CYCLES_PER_NS = 2.3

BENCH_DIR = Path("bench-results")
OUT_DIR = Path("docs/images/benchmark-plots")
OUT_DIR.mkdir(parents=True, exist_ok=True)

handler_ops = {
    "add":     "AddOrder (handler)",
    "reduce":  "ReduceOrder (handler)",
    "delete":  "DeleteOrder (handler)",
    "replace": "ReplaceOrder (handler)",
}

book_ops = {
    "cb_add":     "Add (CommodityBook only)",
    "cb_reduce":  "Reduce (CommodityBook only)",
}

colors = {
    "add":       "steelblue",
    "reduce":    "darkorange",
    "delete":    "seagreen",
    "replace":   "crimson",
    "cb_add":    "cornflowerblue",
    "cb_reduce": "navajowhite",
}

def load(name):
    return np.fromfile(BENCH_DIR / f"{name}_cycles.bin", dtype=np.uint32)

def percentiles(arr):
    return {p: int(np.percentile(arr, p)) for p in [50, 90, 99, 99.9, 99.99]}

all_ops = {**handler_ops, **book_ops}
data = {name: load(name) for name in all_ops}

# --- Per-op log-x histograms ---
for name, label in all_ops.items():
    arr = data[name]
    if len(arr) == 0:
        continue
    ns = arr / CYCLES_PER_NS
    lo = max(20, ns.min())
    hi = ns.max()
    bins = np.logspace(np.log10(lo), np.log10(hi), 200)
    plt.figure(figsize=(9, 5))
    plt.hist(ns, bins=bins, log=True, color=colors[name], edgecolor="none")
    plt.xscale("log")
    pct = percentiles(arr)
    title_extra = (
        f"  p50={int(pct[50]/CYCLES_PER_NS)}ns"
        f"  p99={int(pct[99]/CYCLES_PER_NS)}ns"
        f"  p99.9={int(pct[99.9]/CYCLES_PER_NS)}ns"
    )
    plt.title(f"{label}  n={len(arr):,}{title_extra}")
    plt.xlabel("latency (ns, log scale)")
    plt.ylabel("count (log)")
    plt.grid(True, alpha=0.3, which="both")
    plt.tight_layout()
    out = OUT_DIR / f"v1_{name}_hist.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"wrote {out}")

# --- Handler vs CommodityBook comparison: Add ---
plt.figure(figsize=(10, 6))
for name, label in [("add", "Handler (full AddOrder)"),
                    ("cb_add", "CommodityBook only")]:
    arr = data[name]
    if len(arr) == 0:
        continue
    ns = arr / CYCLES_PER_NS
    clip = np.percentile(ns, 99.999)
    ns_clip = ns[ns <= clip]
    plt.hist(ns_clip, bins=200, log=True, alpha=0.55,
             label=f"{label} (p50={int(np.percentile(arr, 50)/CYCLES_PER_NS)}ns)",
             color=colors[name])
plt.title("AddOrder: handler-level vs CommodityBook-level")
plt.xlabel("latency (ns)")
plt.ylabel("count (log)")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(OUT_DIR / "v1_add_compare.png", dpi=130)
plt.close()
print(f"wrote {OUT_DIR / 'v1_add_compare.png'}")

# --- Handler vs CommodityBook comparison: Reduce ---
plt.figure(figsize=(10, 6))
for name, label in [("reduce", "Handler (full ReduceOrder)"),
                    ("cb_reduce", "CommodityBook only")]:
    arr = data[name]
    if len(arr) == 0:
        continue
    ns = arr / CYCLES_PER_NS
    clip = np.percentile(ns, 99.999)
    ns_clip = ns[ns <= clip]
    plt.hist(ns_clip, bins=200, log=True, alpha=0.55,
             label=f"{label} (p50={int(np.percentile(arr, 50)/CYCLES_PER_NS)}ns)",
             color=colors[name])
plt.title("ReduceOrder: handler-level vs CommodityBook-level")
plt.xlabel("latency (ns)")
plt.ylabel("count (log)")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(OUT_DIR / "v1_reduce_compare.png", dpi=130)
plt.close()
print(f"wrote {OUT_DIR / 'v1_reduce_compare.png'}")

# --- Percentile table ---
lines = ["operation,p50_ns,p90_ns,p99_ns,p99.9_ns,p99.99_ns,max_ns,count"]
print("\n--- Percentile table (ns) ---")
print(f"{'op':<28}{'p50':>8}{'p90':>8}{'p99':>8}{'p99.9':>10}{'p99.99':>10}{'max':>12}{'count':>14}")
for name, label in all_ops.items():
    arr = data[name]
    if len(arr) == 0:
        continue
    pct = percentiles(arr)
    row = [label,
           int(pct[50]/CYCLES_PER_NS),
           int(pct[90]/CYCLES_PER_NS),
           int(pct[99]/CYCLES_PER_NS),
           int(pct[99.9]/CYCLES_PER_NS),
           int(pct[99.99]/CYCLES_PER_NS),
           int(np.max(arr)/CYCLES_PER_NS),
           len(arr)]
    print(f"{row[0]:<28}{row[1]:>8}{row[2]:>8}{row[3]:>8}{row[4]:>10}{row[5]:>10}{row[6]:>12}{row[7]:>14,}")
    lines.append(",".join(str(x) for x in row))
(OUT_DIR / "v1_percentiles.csv").write_text("\n".join(lines) + "\n")
print(f"wrote {OUT_DIR / 'v1_percentiles.csv'}")

print("\nAll plots written.")