# RCKangaroo Cost Calculator
## vast.ai 12x RTX 4090 @ $5.122/hr

## Speed Estimates

### Per-GPU Speed (RTX 4090, Ada Lovelace sm_89)
- RCKangaroo on RTX 4090: ~6,000-8,000 MKeys/s per card
- Conservative estimate: **6,000 MKeys/s per 4090**
- With 12 GPUs: **72,000 MKeys/s = 72 GKeys/s**

### For reference (our measured benchmarks):
- RTX 3060 (1 GPU): 1,491 MKeys/s
- RTX 4090 has ~4.5x more CUDA cores + higher clocks + better int throughput
- Scaling factor: ~4-5.4x per card

## Cost Table by Puzzle Size

Expected operations = K × √(2^(range-1))
where K ≈ 1.15-1.5 (RC's SOTA method, measured)

Using K = 1.3 (conservative average) and 72 GKeys/s:

| Puzzle | Range | √Range Ops | Expected Ops (K=1.3) | Time @72GK/s | Cost @$5.12/hr |
|--------|-------|-----------|---------------------|--------------|----------------|
| 78 | 2^77 | 2^38.5 = 3.8×10^11 | 4.9×10^11 | ~7 sec | $0.01 |
| 80 | 2^79 | 2^39.5 = 7.6×10^11 | 9.9×10^11 | ~14 sec | $0.02 |
| 85 | 2^84 | 2^42.0 = 4.4×10^12 | 5.7×10^12 | ~79 sec | $0.11 |
| 90 | 2^89 | 2^44.5 = 2.5×10^13 | 3.2×10^13 | ~7.5 min | $0.64 |
| 95 | 2^94 | 2^47.0 = 1.4×10^14 | 1.8×10^14 | ~42 min | $3.59 |
| 100 | 2^99 | 2^49.5 = 8.0×10^14 | 1.0×10^15 | ~3.9 hrs | $20 |
| **110** | **2^109** | **2^54.5 = 2.6×10^16** | **3.4×10^16** | **~5.4 days** | **$663** |
| 115 | 2^114 | 2^57.0 = 1.4×10^17 | 1.9×10^17 | ~30 days | $3,688 |
| 120 | 2^119 | 2^59.5 = 8.1×10^17 | 1.1×10^18 | ~170 days | $20,897 |
| 125 | 2^124 | 2^62.0 = 4.6×10^18 | 6.0×10^18 | ~2.6 years | $117,856 |
| 130 | 2^129 | 2^64.5 = 2.6×10^19 | 3.4×10^19 | ~15 years | $664,732 |
| 135 | 2^134 | 2^67.0 = 1.5×10^20 | 1.9×10^20 | ~84 years | $3.75M |

## Key Takeaways

### Puzzle 110 — THE SWEET SPOT for 12x 4090
- **Expected solve time: ~5 days** (range: 3-10 days depending on luck)
- **Expected cost: ~$663** (range: $400-$1,300)
- This is the largest puzzle that's economically viable on a single 12x 4090 machine

### Puzzle 115 — Pushing It
- Expected: ~30 days, ~$3,700
- Still doable but expensive. Lucky solve could be <$1K, unlucky >$7K.

### Puzzle 120+ — Need a Fleet
- 120: $21K for one machine over 6 months
- 125: $118K — need 10+ machines or a large fleet
- 130: RetiredCoder used a farm of many GPUs over months
- 135: Only viable with 100+ GPU fleet or cloud coordination

## Break-Even Analysis (vs. Bitcoin puzzle reward)

Puzzle rewards (as of March 2026):
- Puzzle 110: 0.110 BTC
- Puzzle 115: 0.115 BTC
- Puzzle 120: 0.120 BTC
- Puzzle 135: 0.135 BTC

At BTC price $X:
- Puzzle 110 break-even: $663 / 0.110 = need BTC > $6,027
- Puzzle 115 break-even: $3,688 / 0.115 = need BTC > $32,070
- Puzzle 120 break-even: $20,897 / 0.120 = need BTC > $174,142

**At current BTC ~$87,000:**
- Puzzle 110 reward ≈ $9,570 → **PROFITABLE** (cost $663, profit ~$8,900)
- Puzzle 115 reward ≈ $10,005 → **PROFITABLE** (cost $3,688, profit ~$6,300)
- Puzzle 120 reward ≈ $10,440 → **LOSS** (cost $20,897, loss ~$10,400)

## Recommended Configuration

### For Puzzle 110 (best ROI):
```
./rckangaroo -pubkey <PUBKEY_110> -range 110 -dp 22 -start <START>
```
- DP=22 gives good balance: ~16K DPs expected, manageable RAM
- All 12 GPUs will be used automatically

### Optimal DP values by range:
| Range | Recommended DP | Expected DPs | RAM for DPs |
|-------|---------------|-------------|-------------|
| 78 | 16 | ~5K | ~0.2 MB |
| 85 | 18 | ~12K | ~0.5 MB |
| 90 | 20 | ~24K | ~0.9 MB |
| 100 | 20 | ~760K | ~29 MB |
| 110 | 22 | ~3M | ~115 MB |
| 115 | 24 | ~6M | ~230 MB |
| 120 | 26 | ~12M | ~460 MB |
| 125 | 28 | ~24M | ~920 MB |
| 130 | 30 | ~48M | ~1.8 GB |
| 135 | 30 | ~1.5B | ~57 GB |
