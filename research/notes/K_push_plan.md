# K-reduction plan: 0.80 → target 0.60-0.72

## Diagnosis (read of current gs3 code)

1. `Canonicalize(canon_x, x, y)` in RCGpuUtils.h computes:
   - canonical x = min(x, βx, β²x)
   - class_idx = {0=none, GS_CLASS_BETA, GS_CLASS_BETA2} | (y[0]&1 ? 1 : 0)
2. **BUG/INCOMPLETENESS**: `equiv_new = Canonicalize(canon_new, x, y)` is assigned in RCGpuCore.cu:188 and never stored into the DP record. Only canonical x is stored.
3. At collision time, `CheckNewPoints()` in RCKangaroo.cpp has GS_MODE recovery that tries `HalfRange ± diff`, `HalfRange ± sum/2` — **doesn't cover β-endomorphism class mismatches**.
4. Net effect: walks that collide via different β-classes are silently discarded as "collision between equiv class transforms we haven't handled" (comment line 372). Real K is therefore stuck at 0.80 instead of x-only floor 0.72.

## Planned patches (minimal risk)

### Patch 1: Store class_idx in DP record
- Pack 3-bit class_idx into upper bits of `DBRec.type` (currently 2 bits used for 0/1/2 TAME/WILD1/WILD2)
- Need matching packing on GPU side (GPU_DP_SIZE byte layout)
- 1 bit for y-parity, 2 bits for x-class (0, β, β²)

### Patch 2: Handle β-mismatch in Collision recovery
- When pref.class_idx ≠ nrec.class_idx on the β part:
  - Apply λ or λ² to one distance before computing diff
  - λ is the endomorphism eigenvalue: λ²+λ+1 = 0 mod n
  - Try: key = HalfRange + (d1 - λ·d2), HalfRange + (d1 - λ²·d2), etc.
- Need working MulModN in host (Ec.h) — Jules's commit 70ce55a "Implement native MulModN" suggests prior version lacked this

### Patch 3 (risky — only if P1+P2 show real gain):
- Also canonicalize y: after picking canon x, if corresponding y has wrong parity, negate walk direction
- Would push K toward √6 floor 0.51
- HIGH BUG RISK — this is where Jules failed

## Test matrix
- Baseline: current gs3 K=0.80 on 10 solves at range=76, dp=16
- After P1+P2: expect K → 0.72 ± 0.05
- After P3: expect K → 0.55 ± 0.10 (if it works; if K > 0.80 we broke something)

## Success criterion
- K reduction ≥ 10% (0.80 → 0.72) measured across ≥10 solves
- Zero Collision Errors over the test run
- GPU speed unchanged (±5%)

## Abort conditions
- K worsens (new implementation broke something)
- Collision Error rate > 0
- GPU speed drops > 10%
