#!/usr/bin/env python3
"""Plot per-operation latency distributions from cycle dumps."""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Approximate cycles/ns for converting to nanoseconds.
# 2.3 GHz nominal on your CPU per `lscpu` (24 X 2304 MHz from earlier).
CYCLES_PER_NS = 2.3

BENCH_DIR = Path("bench-results")
OUT_DIR = Path("docs/images/benchmark-plots")
OUT_DIR.mkdir(parents=True, exist_ok=True)

ops = {
    "add":     "AddOrder",
    "reduce":  "ReduceOrder",
    "delete":  "DeleteOrder",
    "replace": "ReplaceOrder",
}

def load(name):
    return np.fromfile(BENCH_DIR / f"{name}_cycles.bin", dtype=np.uint32)

def percentiles(arr):
    pct = [50, 90, 99, 99.9, 99.99]
    return {p: int(np.percentile(arr, p)) for p in pct}

# --- Load all data ---
data = {name: load(name) for name in ops}

# --- 1. Per-op histograms (one figure per op) ---
for name, label in ops.items():
    arr = data[name]
    if len(arr) == 0:
        continue

    ns = arr / CYCLES_PER_NS

    # Clip the long tail for visualization (keep up to p99.99).
    clip = np.percentile(ns, 99.99)
    ns_clip = ns[ns <= clip]

    plt.figure(figsize=(9, 5))
    plt.hist(ns_clip, bins=200, log=True, color="steelblue", edgecolor="none")
    pct = percentiles(arr)
    title_extra = (
        f"  p50={int(pct[50] / CYCLES_PER_NS)}ns"
        f"  p99={int(pct[99] / CYCLES_PER_NS)}ns"
        f"  p99.9={int(pct[99.9] / CYCLES_PER_NS)}ns"
    )
    plt.title(f"{label} latency (v1: std::map)  n={len(arr):,}{title_extra}")
    plt.xlabel("latency (ns)")
    plt.ylabel("count (log)")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out = OUT_DIR / f"v1_{name}_hist.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"wrote {out}")

# --- 2. Overlaid histogram of all ops ---
plt.figure(figsize=(10, 6))
colors = {
    "add": "steelblue",
    "reduce": "darkorange",
    "delete": "seagreen",
    "replace": "crimson",
}
for name, label in ops.items():
    arr = data[name]
    if len(arr) == 0:
        continue

    ns = arr / CYCLES_PER_NS
    clip = np.percentile(ns, 99.5)
    ns_clip = ns[ns <= clip]
    plt.hist(
        ns_clip,
        bins=200,
        log=True,
        alpha=0.55,
        label=f"{label} (n={len(arr):,})",
        color=colors[name],
    )

plt.title("v1 (std::map): all operations, latency distributions overlaid")
plt.xlabel("latency (ns)")
plt.ylabel("count (log)")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()
out = OUT_DIR / "v1_all_ops_hist.png"
plt.savefig(out, dpi=130)
plt.close()
print(f"wrote {out}")

# --- 3. Percentile table (printed + saved) ---
lines = ["operation,p50_ns,p90_ns,p99_ns,p99.9_ns,p99.99_ns,max_ns,count"]

print("\n--- Percentile table (ns) ---")
print(
    f"{'op':<10}"
    f"{'p50':>8}"
    f"{'p90':>8}"
    f"{'p99':>8}"
    f"{'p99.9':>10}"
    f"{'p99.99':>10}"
    f"{'max':>10}"
    f"{'count':>14}"
)

for name, label in ops.items():
    arr = data[name]
    if len(arr) == 0:
        continue

    pct = percentiles(arr)
    row = [
        label,
        int(pct[50] / CYCLES_PER_NS),
        int(pct[90] / CYCLES_PER_NS),
        int(pct[99] / CYCLES_PER_NS),
        int(pct[99.9] / CYCLES_PER_NS),
        int(pct[99.99] / CYCLES_PER_NS),
        int(np.max(arr) / CYCLES_PER_NS),
        len(arr),
    ]

    print(
        f"{row[0]:<10}"
        f"{row[1]:>8}"
        f"{row[2]:>8}"
        f"{row[3]:>8}"
        f"{row[4]:>10}"
        f"{row[5]:>10}"
        f"{row[6]:>10}"
        f"{row[7]:>14,}"
    )

    lines.append(",".join(str(x) for x in row))

(OUT_DIR / "v1_percentiles.csv").write_text("\n".join(lines) + "\n")
print(f"wrote {OUT_DIR / 'v1_percentiles.csv'}")

# --- 3b. Largest outliers ---
print("\n--- Top 20 largest latencies (ns) ---")

for name, label in ops.items():
    arr = data[name]
    if len(arr) == 0:
        continue

    top_ns = (np.sort(arr)[-20:] / CYCLES_PER_NS).astype(np.int64)

    print(f"\n{label}:")
    print(top_ns)

# --- 4. AddOrder latency over time (downsampled) ---
arr = data["add"]
if len(arr) > 0:
    ns = arr / CYCLES_PER_NS
    step = max(1, len(ns) // 50000)
    idx = np.arange(0, len(ns), step)
    sample = ns[::step]

    # Clip extreme outliers for visualization
    clip = np.percentile(sample, 99.5)
    mask = sample <= clip

    plt.figure(figsize=(11, 5))
    plt.scatter(idx[mask], sample[mask], s=1, alpha=0.35, color="steelblue")
    plt.title(
        f"v1 (std::map): AddOrder latency over message sequence "
        f"(downsampled, {len(sample):,} pts)"
    )
    plt.xlabel("AddOrder sequence number")
    plt.ylabel("latency (ns)")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out = OUT_DIR / "v1_add_latency_over_time.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"wrote {out}")

print("\nAll plots written to docs/images/benchmark-plots/")