# lowlat

A low-latency NASDAQ ITCH 5.0 order book, built as a benchmarking study inspired by David Gross's CppCon 2024 talk. Parses a real BX ITCH feed, maintains per-stock order books, and measures the per-operation latency cost of six price-level data structures.

---

## Architecture

```
mmap'd ITCH file
  → parse_file()     strip SoupBinTCP framing, zero-copy
  → dispatch()       jump-table on message type byte
  → BookHandler      rdtsc() wraps each call → cycle vectors
  → OrderBook        order pool + id→pool-index map + per-stock CB array
  → CommodityBookV*  the structure under study
  → AsyncLogger      optional: SPSC queue → worker thread → binary log
```

**OrderPool** — 1M pre-allocated `Order` slots, free list. Each order holds prev/next links for a per-level FIFO doubly-linked list.

**OrderBook** — templated on `CB` (commodity book), `IdMap` (`std::unordered_map` or `absl::flat_hash_map`), and `Sink`. Fixed array of 10 000 per-stock CB instances. Publishes `BookUpdate` (best bid/ask snapshot) via the sink on every state change, with dedup.

**SPSCQueue** — power-of-2 capacity, cache-line-separated counters, batched atomic publishes every 8192 items.

---

## CommodityBook variants

| # | Structure | Add | Reduce |
|---|---|---|---|
| V1 | `std::map<Price, Volume>` | O(log n) | O(log n) |
| V2 | sorted `vector`, desc bids / asc asks, `std::lower_bound` | O(log n) | O(log n) |
| V3 | sorted `vector`, asc bids / desc asks — hot end at `.back()` | O(log n) | O(log n) |
| V4 | V3 + custom branchless `lower_bound` | O(log n) | O(log n) |
| V5 | unsorted `vector`, linear scan | O(n) | O(n) |
| V6 | stable `PriceLevel` pool; `Order` stores `level_idx` directly | O(log n) | **O(1)** |

---

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build --parallel
```

Requires: clang-18, ninja, cmake ≥ 3.20, `libabsl-dev`.

---

## Usage

```bash
# bench: measures CommodityBook + OrderBook in isolation (NoOpSink)
taskset -c 4 ./build/src/lowlat_main \
  --mode=bench --variant=v6 --ob=v1 --idmap=absl data/20181228.BX_ITCH_50

# pipeline: full path with AsyncLogger sink (cores 4 + 6)
taskset -c 4,6 ./build/src/lowlat_main \
  --mode=pipeline --variant=v6 data/20181228.BX_ITCH_50
```

Flags:
- `--variant` — `v1` … `v6`
- `--ob` — `v0` (hashmap stock→book) / `v1` (array, default)
- `--idmap` — `stl` / `absl`

Output `.bin` files land in `bench-results/` tagged as `{variant}_ob{ob}_{idmap}_{op}.bin`.

### Benchmarks

```bash
./build/bench/spsc_throughput   # SPSC queue throughput
./build/bench/spsc_latency      # SPSC queue end-to-end latency (TSC sampled)
```

### Tests

```bash
ctest --test-dir build --output-on-failure
```

### Analysis scripts

```bash
# Per-op latency CDFs + bar chart for all variants
python3 scripts/compare_all.py

# Percentile table for a single tagged run
python3 scripts/plot_latencies.py --tag v6_obv1_absl

# Parse perf stat outputs
python3 scripts/parse_perf.py
```

---

## Results

Dataset: `data/20181228.BX_ITCH_50` — 123.8M messages, peak 72 760 live orders.  
CPU: pinned to core 4, Release build, clang-18 -O3 -march=native, 2.3 GHz TSC.

### CommodityBook layer (obv0/stl, no outer-book noise)

| variant | cb_add p50 | cb_add p99 | cb_red p50 | cb_red p99 |
|---|---|---|---|---|
| V1 `std::map` | 52 ns | 237 ns | 66 ns | 180 ns |
| V2 sorted vec | 39 ns | 142 ns | 50 ns | 120 ns |
| V3 reversed vec | 37 ns | 138 ns | 50 ns | 123 ns |
| V4 branchless | 39 ns | 140 ns | 46 ns | 114 ns |
| V5 linear scan | 38 ns | 155 ns | 56 ns | 150 ns |
| **V6 level pool** | **25 ns** | **118 ns** | **22 ns** | **114 ns** |

### Full handler layer

| tag | hdl_add p50 | hdl_del p50 | msg/s |
|---|---|---|---|
| v1_obv0_stl | 128 ns | 166 ns | 5.79 M |
| v3_obv0_stl | 107 ns | 144 ns | 6.53 M |
| v6_obv0_stl | 103 ns | 123 ns | 6.81 M |
| v6_obv1_stl | 91 ns | 101 ns | 8.09 M |
| **v6_obv1_absl** | **79 ns** | **80 ns** | **8.98 M** |

Pipeline v6 (+ AsyncLogger): **6.20 M msg/s** — 31% slower than bench due to `top_of_book()` + SPSC push on every mutation.

### perf stat (obv0/stl unless noted)

| variant | cycles (B) | IPC | branch-miss% | L1-miss (B) | dTLB (M) |
|---|---|---|---|---|---|
| V1 | 94.4 | 0.83 | 2.61 | 1.63 | 178 |
| V2 | 82.5 | 0.88 | 3.09 | 1.23 | 70 |
| V3 | 82.6 | 0.86 | 2.99 | 1.22 | 71 |
| V4 | 83.4 | 0.89 | 2.70 | 1.22 | 76 |
| V5 | 85.0 | 0.86 | 3.01 | 1.30 | 65 |
| V6 obv0 stl | 74.7 | 0.74 | 3.29 | 1.24 | **205** |
| V6 obv1 stl | 67.1 | 0.79 | 3.38 | 0.97 | 83 |
| **V6 obv1 absl** | **57.3** | **1.11** | **3.14** | **0.86** | **69** |

---

## Key findings

**V2–V5 are essentially the same.** cb_add p50 clusters at 37–40 ns for all four. Each BX stock has only ~3–5 active price levels, so branch prediction is near-perfect and binary search barely runs. The "reversed orientation" in V3 and "branchless lower_bound" in V4 are both noise at this depth. V4's branchless code sometimes actively hurts (defeats a predictor that was already perfect).

**V6's win is entirely on reduce.** The add path still runs `lower_bound` — same as V3. The 2–3× reduce improvement (22 ns vs 46–66 ns) comes from eliminating that search entirely: each `Order` stores its `level_idx`, giving O(1) direct access.

**V6 obv0 dTLB spike (205 M).** When `stock_to_book` is a hashmap, `PriceLevel` pool slots scatter across pages. Switching to the flat array (`obv1`) drops dTLB misses from 205 M to 83 M and cuts cycles by 10%.

**absl flat_hash_map is a step change.** V6 obv1 absl jumps from IPC 0.79 → 1.11 and reduces L1-misses by another 120 M. Total cycles drop from 57.3 B, cutting wall time from ~15 s to ~12 s. Better cache locality allows the CPU to retire more instructions per cycle.

**Outer book + idmap combined** (obv0_stl → obv1_absl) saves as much latency as the entire V1→V6 CB improvement at the handler level.
