#!/usr/bin/env python3
"""Plot per-operation latency distributions for a single tagged run."""

import sys
import argparse
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

CYCLES_PER_NS = 2.3

parser = argparse.ArgumentParser()
parser.add_argument("--tag", required=True, help="run tag, e.g. v6_obv1_absl")
parser.add_argument("--bench-dir", default="bench-results")
parser.add_argument("--out-dir", default="docs/images/benchmark-plots")
args = parser.parse_args()

BENCH_DIR = Path(args.bench_dir)
OUT_DIR = Path(args.out_dir)
OUT_DIR.mkdir(parents=True, exist_ok=True)

handler_ops = {
    "add":     "AddOrder (handler)",
    "reduce":  "ReduceOrder (handler)",
    "delete":  "DeleteOrder (handler)",
    "replace": "ReplaceOrder (handler)",
}
book_ops = {
    "cb_add":    "Add (CommodityBook only)",
    "cb_reduce": "Reduce (CommodityBook only)",
}
all_ops = {**handler_ops, **book_ops}

def load(name):
    path = BENCH_DIR / f"{args.tag}_{name}.bin"
    return np.fromfile(path, dtype=np.uint32)

def percentiles(arr):
    return {p: int(np.percentile(arr, p)) for p in [50, 90, 99, 99.9, 99.99]}

data = {name: load(name) for name in all_ops}

print(f"\n--- Percentile table for {args.tag} (ns) ---")
print(f"{'op':<30}{'p50':>8}{'p90':>8}{'p99':>8}{'p99.9':>10}{'p99.99':>10}{'max':>12}{'count':>14}")
lines = ["tag,op,p50_ns,p90_ns,p99_ns,p99.9_ns,p99.99_ns,max_ns,count"]
for name, label in all_ops.items():
    arr = data[name]
    if len(arr) == 0: continue
    pct = percentiles(arr)
    row = [args.tag, label,
           int(pct[50]/CYCLES_PER_NS), int(pct[90]/CYCLES_PER_NS),
           int(pct[99]/CYCLES_PER_NS), int(pct[99.9]/CYCLES_PER_NS),
           int(pct[99.99]/CYCLES_PER_NS),
           int(np.max(arr)/CYCLES_PER_NS), len(arr)]
    print(f"{row[1]:<30}{row[2]:>8}{row[3]:>8}{row[4]:>8}{row[5]:>10}{row[6]:>10}{row[7]:>12}{row[8]:>14,}")
    lines.append(",".join(str(x) for x in row))

(OUT_DIR / f"{args.tag}_percentiles.csv").write_text("\n".join(lines) + "\n")
print(f"\nwrote {OUT_DIR / f'{args.tag}_percentiles.csv'}")