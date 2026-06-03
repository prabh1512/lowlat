#!/usr/bin/env python3
"""Compare all variants: percentile table + overlaid CDFs + bar chart."""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

CYCLES_PER_NS = 2.3
SAMPLE_N = 1_000_000  # downsample for plot memory

BENCH_DIR = Path("bench-results")
OUT_DIR = Path("docs/images/benchmark-plots")
OUT_DIR.mkdir(parents=True, exist_ok=True)

runs = [
    ("v1_obv0_stl",   "v1: std::map"),
    ("v2_obv0_stl",   "v2: sorted vec"),
    ("v3_obv0_stl",   "v3: reverse vec"),
    ("v4_obv0_stl",   "v4: branchless"),
    ("v5_obv0_stl",   "v5: linear search"),
    ("v6_obv0_stl",   "v6: level pool"),
    ("v6_obv1_stl",   "v6 + array stock_to_book"),
    ("v6_obv1_absl",  "v6 + array + absl id_to_pool"),
]

def load(tag, op):
    return np.fromfile(BENCH_DIR / f"{tag}_{op}.bin", dtype=np.uint32)

def pct(arr, p):
    return int(np.percentile(arr, p) / CYCLES_PER_NS)

# --- Summary table ---
print(f"\n{'tag':<32}{'CB Add p50':>12}{'CB Red p50':>12}{'Hdl Add p50':>14}{'Hdl Del p50':>14}{'p99.9':>10}")
rows = []
for tag, label in runs:
    cb_add = load(tag, "cb_add")
    cb_red = load(tag, "cb_reduce")
    h_add  = load(tag, "add")
    h_del  = load(tag, "delete")
    row = [label,
           pct(cb_add, 50), pct(cb_red, 50),
           pct(h_add, 50),  pct(h_del, 50),
           pct(h_add, 99.9)]
    print(f"{row[0]:<32}{row[1]:>12}{row[2]:>12}{row[3]:>14}{row[4]:>14}{row[5]:>10}")
    rows.append([tag] + row)
    del cb_add, cb_red, h_add, h_del

csv_lines = ["tag,label,cb_add_p50,cb_reduce_p50,hdl_add_p50,hdl_del_p50,hdl_add_p999"]
for r in rows:
    csv_lines.append(",".join(str(x) for x in r))
(OUT_DIR / "summary.csv").write_text("\n".join(csv_lines) + "\n")
print(f"\nwrote {OUT_DIR / 'summary.csv'}")

# --- Overlaid CDFs ---
def cdf_plot(op_key, title, outfile):
    plt.figure(figsize=(10, 6))
    colors = plt.cm.viridis(np.linspace(0, 0.9, len(runs)))
    for (tag, label), c in zip(runs, colors):
        raw = load(tag, op_key)
        if len(raw) > SAMPLE_N:
            raw = np.random.choice(raw, SAMPLE_N, replace=False)
        arr = raw.astype(np.float32) / CYCLES_PER_NS
        s = np.sort(arr)
        cdf = np.arange(1, len(s)+1, dtype=np.float32) / len(s)
        p50 = int(np.median(arr))
        plt.plot(s, cdf, label=f"{label}  (p50={p50}ns)", color=c, linewidth=1.5)
        del raw, arr, s, cdf
    plt.xscale("log")
    plt.xlim(20, 5000)
    plt.axhline(0.50, color="gray", linewidth=0.5, alpha=0.5)
    plt.axhline(0.99, color="gray", linewidth=0.5, alpha=0.5)
    plt.title(title)
    plt.xlabel("latency (ns, log scale)")
    plt.ylabel("cumulative fraction")
    plt.legend(loc="lower right", fontsize=9)
    plt.grid(True, alpha=0.3, which="both")
    plt.tight_layout()
    plt.savefig(OUT_DIR / outfile, dpi=130)
    plt.close()
    print(f"wrote {OUT_DIR / outfile}")

cdf_plot("cb_add",    "CB Add latency CDF: progression across variants", "cb_add_cdf.png")
cdf_plot("cb_reduce", "CB Reduce latency CDF: progression across variants", "cb_reduce_cdf.png")
cdf_plot("add",    "Handler AddOrder latency CDF: progression", "handler_add_cdf.png")
cdf_plot("delete", "Handler DeleteOrder latency CDF: progression", "handler_delete_cdf.png")

# --- Bar chart: p50 per variant ---
labels = [label for _, label in runs]
cb_add_p50 = [pct(load(t, "cb_add"), 50) for t, _ in runs]
cb_red_p50 = [pct(load(t, "cb_reduce"), 50) for t, _ in runs]
x = np.arange(len(labels))
width = 0.35
fig, ax = plt.subplots(figsize=(11, 5))
ax.bar(x - width/2, cb_add_p50, width, label="CB Add p50", color="steelblue")
ax.bar(x + width/2, cb_red_p50, width, label="CB Reduce p50", color="darkorange")
ax.set_ylabel("p50 latency (ns)")
ax.set_title("Median latency by variant (CB layer)")
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=20, ha="right", fontsize=9)
ax.legend()
ax.grid(True, alpha=0.3, axis="y")
plt.tight_layout()
plt.savefig(OUT_DIR / "progression_bar.png", dpi=130)
plt.close()
print(f"wrote {OUT_DIR / 'progression_bar.png'}")