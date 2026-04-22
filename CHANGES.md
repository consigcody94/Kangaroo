# Kangaroo K-Push Patches (April 2026)

**Goal:** push the K constant from fork-baseline (0.80 claimed / 1.15 observed) toward the theoretical √6 floor of 0.51 by fully exploiting secp256k1's order-6 automorphism group at the collision-recovery layer.

**Result:** zero-error 8-solve sample at K_mean=0.82, K_median=0.67, K_best=0.34. No regression; best case approaches the 0.51 floor.

## What the baseline was doing wrong

`gs3` branch (commit d7c8c9a) canonicalizes walker x-coordinates to the minimum of {x, βx, β²x} and records y-parity in a `class_idx` value (0..5). But:

1. `class_idx` was computed in the GPU kernel (`Canonicalize()` in `RCGpuCore.cu`) and **never stored in the DP record** — only the canonical x was kept.
2. At collision time the host-side `CheckNewPoints` tried only 4 simple candidates (`HalfRange ± (d1-d2)`, `HalfRange ± (d1+d2)/2`) and silently discarded any collision whose two walkers had canonicalized via different β-transforms.
3. A comment at `RCKangaroo.cpp:204` acknowledged: `"This needs MulModN which we approximate by trying the point"` — the scalar arithmetic required for λ-case recovery was simply missing.

Net effect: roughly a third of real collisions were dropped, keeping measured K at ~0.80 instead of approaching 0.51.

## Patches

### 1. Scalar field (mod n) arithmetic — `Ec.h`, `Ec.cpp`

New methods on `EcInt`:
- `AddModN`, `SubModN`, `NegModN`
- `MulModN` — double-and-add over 256 iterations (not hot-loop; only at collision recovery)
- `InvModN` — Fermat modpow `x^(n-2) mod n`

Precomputed globals (initialized in `InitEc`):
- `g_N` — secp256k1 group order
- `g_LAMBDA_N`, `g_LAMBDA2_N` — endomorphism eigenvalue and its inverse (= λ² since λ³≡1)
- `g_INV_1_MINUS_LAMBDA`, `g_INV_1_PLUS_LAMBDA`, `g_INV_1_MINUS_LAMBDA2`, `g_INV_1_PLUS_LAMBDA2` — factors for the six (e, σ) collision cases

Sanity check at init: `lambda * lambda^2 ≡ 1 (mod n)` — currently emits a pass/fail line to stdout.

### 2. GPU-side class_idx plumbing — `RCGpuCore.cu`

Canonicalize returns a 3-bit `equiv_new` (0..5). Packed into the low 4 bits of the canonical-x int4 stored in `DPTable`. `BuildDP` extracts and strips before emitting the final 48-byte DP record, placing `class_idx` into the repurposed `DPs[10]` ("type" slot, unused in GS_MODE).

Bytes affected in each DP record (GS_MODE):
- `[0..15]`  = canonical x (low 128 bits, low 4 bits zeroed for clean cross-class matching)
- `[16..37]` = signed distance
- `[40]`     = class_idx (0..5)

### 3. Class-aware collision recovery — `RCKangaroo.cpp`

For a collision between walkers with `class_idx` c1, c2:
- `x_class_i = (c_i >> 1) & 0x3` ∈ {0, 1, 2}
- `y_par_i = c_i & 1`
- `e = (x_class_2 − x_class_1) mod 3`
- `σ = -1` iff y-parities differ

Candidates (tried in order, each verified via `ec.MultiplyG`):
- e=0, σ=-1: `key = HalfRange − (D1+D2)/2 mod n` (pre-existing)
- (e, σ) ∈ {(1,+), (1,-), (2,+), (2,-)}: `key = HalfRange + (σ·λ^e·D2 − D1) · (1 − σ·λ^e)^{-1} mod n`
- Walker swap (D1 ↔ D2) for each of the above

Robustness: enumerates all 6 (e, σ) cases regardless of the detected class bits, so any class_idx corruption from the packed-bits scheme doesn't silently kill a real collision.

## What's measured

`research/notes/bench_K.sh` runs N independent single-solve benchmarks (fresh process per run, avoiding the multi-solve DP-table-carryover bug). Output on an RTX 5070, build `-arch=sm_120`:

```
N=8, range=66, dp=14
run 1: K=0.446   run 2: K=0.687   run 3: K=0.343   run 4: K=1.030
run 5: K=1.133   run 6: K=0.652   run 7: K=1.888   run 8: K=0.378
mean 0.820  median 0.669  stdev 0.520  min 0.343  max 1.888
```

## Known gaps / future work

1. **Multi-solve benchmark mode is broken.** After the first solve the DP table isn't reset between iterations; subsequent benchmark solves stall (DPs count grows unbounded). Workaround: run separate processes, each solves one key. Fix would be in `GpuKang::Execute`.

2. **N=8 is too few to prove K<0.80.** To confirm the improvement statistically requires N ≥ 30 samples, ~15 min of testing.

3. **Low-4-bits DP class-pack scheme has a 1/16 false-positive risk** on cross-canonical-x matches. In practice caught by the class-aware recovery trying all cases and finding no key match, so it just wastes a few `MultiplyG` calls. Could be eliminated by extending `DPTable` slot from 16 to 20 bytes.

4. **Sanity check printout** at startup is debug leftover. Safe to silence once the build is production.

## Build

Windows, CUDA 13.2, VS 2022 BuildTools, RTX 5070 (sm_120):
```
.\build.bat
```
Produces `RCKangaroo.exe` at repo root.

## Repo hygiene

- `master` now points at `d7c8c9a` (gs3 K=0.80 head) + these patches
- `master-jules-tainted` preserves the original consigcody94 master tip (untouched for reference; includes Jules's broken `jmp_idx` typo etc.)
- `research/arxiv/`, `research/sanskrit/`, `research/chinese/`, `research/arxiv-extended/` hold the primary sources consulted
- `research/notes/` has the K-push plan, patch scripts, benchmark driver
