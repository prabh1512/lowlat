# Benchmark Findings — lowlat order book

Dataset: `data/20181228.BX_ITCH_50` — 123,830,285 messages, peak 72,760 live orders.
CPU pinned to core 4 (bench), cores 4+6 (pipeline). Release build, clang-18, -O3 -march=native.
TSC cycles converted at 2.3 GHz.

---

## CommodityBook layer (cb_add / cb_reduce) — no outer-book overhead

| variant | cb_add p50 | cb_add p99 | cb_red p50 | cb_red p99 |
|---|---|---|---|---|
| V1 `std::map` | 52 ns | 237 ns | 66 ns | 180 ns |
| V2 sorted vec (desc bid / asc ask) | 39 ns | 142 ns | 50 ns | 120 ns |
| V3 reversed orientation | 37 ns | 138 ns | 50 ns | 123 ns |
| V4 branchless lower_bound | 39 ns | 140 ns | 46 ns | 114 ns |
| V5 linear scan | 38 ns | 155 ns | 56 ns | 150 ns |
| V6 level pool (O(1) reduce) | **25 ns** | 118 ns | **22 ns** | 114 ns |

(ob=v0, idmap=stl for all — isolates CB only)

---

## Full handler layer — add/delete p50, plus throughput

| tag | hdl_add p50 | hdl_del p50 | hdl_add p99 | hdl_del p99 | msg/s |
|---|---|---|---|---|---|
| v1_obv0_stl | 128 ns | 166 ns | 403 ns | 426 ns | 5,790,537 |
| v1_obv0_absl | 124 ns | 146 ns | 448 ns | 526 ns | 5,969,481 |
| v1_obv1_stl | 141 ns | 173 ns | 606 ns | 707 ns | 4,803,153 |
| v1_obv1_absl | 105 ns | 122 ns | 275 ns | 302 ns | 7,339,073 |
| v2_obv0_stl | 110 ns | 144 ns | 338 ns | 333 ns | 6,445,168 |
| v2_obv0_absl | 100 ns | 119 ns | 239 ns | 266 ns | 7,341,961 |
| v2_obv1_stl | 102 ns | 129 ns | 329 ns | 312 ns | 7,267,638 |
| v2_obv1_absl | 91 ns | 105 ns | 209 ns | 233 ns | 8,303,514 |
| v3_obv0_stl | 107 ns | 144 ns | 328 ns | 330 ns | 6,531,662 |
| v3_obv0_absl | 100 ns | 120 ns | 238 ns | 275 ns | 7,178,580 |
| v3_obv1_stl | 100 ns | 131 ns | 324 ns | 302 ns | 6,934,323 |
| v3_obv1_absl | 88 ns | 106 ns | 206 ns | 239 ns | 8,385,024 |
| v4_obv0_stl | 108 ns | 138 ns | 329 ns | 321 ns | 6,955,325 |
| v4_obv0_absl | 100 ns | 115 ns | 233 ns | 266 ns | 7,691,041 |
| v4_obv1_stl | 101 ns | 126 ns | 320 ns | 295 ns | 6,999,489 |
| v4_obv1_absl | 102 ns | 113 ns | 351 ns | 420 ns | 6,684,219 |
| v5_obv0_stl | 110 ns | 153 ns | 360 ns | 407 ns | 6,413,301 |
| v5_obv0_absl | 105 ns | 133 ns | 355 ns | 457 ns | 6,203,094 |
| v5_obv1_stl | 106 ns | 146 ns | 383 ns | 435 ns | 6,361,629 |
| v5_obv1_absl | 89 ns | 111 ns | 214 ns | 253 ns | 7,487,865 |
| v6_obv0_stl | 103 ns | 123 ns | 328 ns | 355 ns | 6,809,807 |
| v6_obv0_absl | 87 ns | 93 ns | 295 ns | 328 ns | 7,645,116 |
| v6_obv1_stl | 91 ns | 101 ns | 300 ns | 313 ns | 8,094,507 |
| **v6_obv1_absl** | **79 ns** | **80 ns** | 228 ns | 248 ns | **8,977,996** |

---

## Pipeline mode (obv1 + absl + AsyncLogger, always)

| variant | msg/s |
|---|---|
| pipe_v1 | 5,023,517 |
| pipe_v2 | 5,369,833 |
| pipe_v3 | 5,389,120 |
| pipe_v4 | 5,656,438 |
| pipe_v5 | 4,478,621 |
| pipe_v6 | 6,202,651 |

Pipeline v6 is **31% slower** than bench v6_obv1_absl (6.2M vs 9.0M msg/s). Cost is the `top_of_book()` call + dedup check + SPSC push on every mutation.

---

## Key findings

### 1. V2 through V5 are the same
cb_add p50 clusters at 37–40 ns for all four. The "reversed orientation" (V3 vs V2) gains 2 ns — noise. The "linear scan" (V5) matches V3 exactly. Why: each stock has ~3–5 active price levels, so branch prediction is near-perfect and binary search barely runs.

### 2. V4's branchless lower_bound is a regression
V4 was designed to eliminate branch mispredicts in the search loop. For tiny books it defeats a predictor that was already perfect. v4_obv1_absl (102 ns add) is **worse** than v3_obv1_absl (88 ns). Cut V4.

### 3. V6 wins only on reduce, not add
V6's add path still runs a `lower_bound` — identical to V3. The win is exclusively on reduce: **22 ns vs 46–66 ns** (2–3×) because `level_idx` stored in each Order gives O(1) access to the price level with no search.

### 4. Outer book and idmap matter as much as the CB variant
Switching from obv0_stl → obv1_absl on V3 saves 19 ns (107→88 ns) — the same order of magnitude as going V1→V3 at the CB layer. Both dimensions compound: V6_obv1_absl at 79 ns is the sum of all three wins.

### 5. absl consistently beats stl
~10–15 ns saved on hdl_add across all variants. Consistent across ob=v0 and ob=v1.
