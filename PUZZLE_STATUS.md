# Bitcoin Puzzle Status — Complete Guide
## As of March 2026

## PUZZLE 135 — THE TARGET

**Status**: ❌ UNSOLVED
**Public Key**: `02145d2611c823a396ef6712ce0f712f09b9b4f3135e3e0aa3230fb9b6d08d1e16`
**Reward**: 13.5 BTC (~$1,174,500 at BTC $87K)
**Range**: `0x4000000000000000000000000000000000` to `0x7fffffffffffffffffffffffffffffffff`
**Range bits**: 2^134 to 2^135 (134-bit search space)

**PUBLIC KEY IS KNOWN** → Pollard Kangaroo CAN be used (√N complexity)

## ALL UNSOLVED PUZZLES WITH KNOWN PUBLIC KEYS

These can be attacked with Kangaroo (O(√N) operations):

| Puzzle | Reward | Public Key (compressed) | Range Bits | √N Ops |
|--------|--------|------------------------|-----------|--------|
| **135** | 13.5 BTC | `02145d2611c823a396ef6712ce0f712f09b9b4f3135e3e0aa3230fb9b6d08d1e16` | 134 | 2^67 |
| **140** | 14.0 BTC | `031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640` | 139 | 2^69.5 |
| **145** | 14.5 BTC | `03afdda497369e219a2c1c369954a930e4d3740968e5e4352475bcffce3140dae5` | 144 | 2^72 |
| **150** | 15.0 BTC | `03137807790ea7dc6e97901c2bc87411f45ed74a5629315c4e4b03a0a102250c49` | 149 | 2^74.5 |
| **155** | 15.5 BTC | `035cd1854cae45391ca4ec428cc7e6c7d9984424b954209a8eea197b9e364c05f6` | 154 | 2^77 |
| **160** | 16.0 BTC | `02e0a8b039282faf6fe0fd769cfbc4b6b4cf8758ba68220eac420e32b91ddfa673` | 159 | 2^79.5 |

## UNSOLVED PUZZLES WITHOUT PUBLIC KEYS

These require brute force (O(N) operations — exponentially harder):

| Puzzle Range | Count | Attack Method | Feasibility |
|-------------|-------|--------------|-------------|
| 71-134 | ~64 puzzles | Brute force only | 71-85: feasible, 86-100: hard, 101+: infeasible |
| 136-139 | 4 puzzles | Brute force only | Infeasible solo |
| 141-144 | 4 puzzles | Brute force only | Infeasible |
| 146-149 | 4 puzzles | Brute force only | Infeasible |
| 151-154 | 4 puzzles | Brute force only | Infeasible |
| 156-159 | 4 puzzles | Brute force only | Infeasible |

## BRUTE FORCE vs KANGAROO — THE MATH

For puzzle N:
- **With public key (Kangaroo)**: ~1.3 × √(2^(N-1)) = ~1.3 × 2^((N-1)/2) operations
- **Without public key (Brute force)**: ~2^(N-1) operations (check each key)

The difference:
| Puzzle | Kangaroo Ops | Brute Force Ops | Ratio |
|--------|-------------|----------------|-------|
| 135 | 2^67 ≈ 1.5×10^20 | 2^134 ≈ 2.2×10^40 | 2^67 = 147 BILLION times harder |
| 100 | 2^49.5 | 2^99 | 2^49.5 = 800 BILLION times harder |
| 80 | 2^39.5 | 2^79 | 2^39.5 = 800 BILLION times harder |

## BRUTE FORCE OPTIMIZATION TECHNIQUES (No Public Key)

For puzzles where only the Bitcoin address is known:

### 1. Sequential Addition (Standard)
- P_{k+1} = P_k + G (one EC point addition per key)
- Costs ~6 field multiplications per key
- Current GPU speed: ~72 GKeys/s on 12x RTX 4090

### 2. Endomorphism Triple-Check (3× throughput)
- For each point P = (x, y), compute:
  - Address of (x, y) — key k
  - Address of (β×x, y) — key λ×k mod n [FREE: 1 field multiply]
  - Address of (β²×x, y) — key λ²×k mod n [FREE: 1 field multiply]
- PLUS negation (other y-parity) for 6 total addresses per point addition
- **3-6× throughput improvement on brute force**
- BUT: λ×k and λ²×k are NOT in the same puzzle range as k
- Only useful for MULTI-TARGET attacks (checking against ALL puzzle addresses)

### 3. Multi-Target Bloom Filter
- Store ALL unsolved puzzle addresses (~80) in a Bloom filter
- Check each generated address against ALL targets in O(1)
- Effectively ~80× more likely to find any solution per operation
- **Combined with endomorphism: 6 × 80 = 480 checks per point addition**

### 4. GPU Arithmetic Optimizations (Same as Kangaroo)
- All gECC, Nikhilam, RNS optimizations apply to point addition
- Point addition IS the bottleneck for both kangaroo and brute force

## COST ANALYSIS: BRUTE FORCE vs KANGAROO for Puzzle 135

### With Kangaroo (public key known):
At 72 GKeys/s (12x RTX 4090):
- Ops needed: ~1.3 × 2^67 ≈ 1.9 × 10^20
- Time: 1.9×10^20 / 7.2×10^10 = 2.6×10^9 sec ≈ **84 years solo**
- With 100 machines: **~10 months**
- Cost at $5.12/hr: 100 × 10 × 30 × 24 × $5.12 = **$3.7M**
- Reward: 13.5 BTC × $87K = **$1.17M**
- **Not profitable solo, but possible via pool**

### Without public key (brute force, hypothetical):
At 72 GKeys/s with 6× endomorphism:
- Effective speed: ~432 GChecks/s
- Ops needed: 2^134 ≈ 2.2 × 10^40
- Time: 2.2×10^40 / 4.3×10^11 = 5.1×10^28 sec ≈ **1.6 × 10^21 years**
- Even with 1 MILLION machines: **1.6 × 10^15 years**
- **PHYSICALLY IMPOSSIBLE**

## REALISTIC TARGETS

### Best ROI puzzles (sorted by profitability):

| Puzzle | Method | Time (12x4090) | Cost | Reward | Profit |
|--------|--------|----------------|------|--------|--------|
| 71 | Brute force | ~3 hours | $15 | $617K (7.1 BTC) | **$617K** |
| 72 | Brute force | ~6 hours | $31 | $626K | **$626K** |
| 73 | Brute force | ~12 hours | $61 | $635K | **$635K** |
| 74 | Brute force | ~1 day | $123 | $644K | **$644K** |
| 75 | Brute force | ~2 days | $246 | $652K | **$652K** |
| 76 | Brute force | ~4 days | $491 | $661K | **$661K** |
| 77 | Brute force | ~8 days | $983 | $670K | **$669K** |
| 78 | Brute force | ~16 days | $1,966 | $678K | **$676K** |
| 79 | Brute force | ~32 days | $3,931 | $687K | **$683K** |
| 80 | Brute force | ~65 days | $7,862 | $696K | **$688K** |
| 135 | Kangaroo | ~84 years (solo) | Pool | $1.17M | Pool share |
