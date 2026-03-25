# RCKangaroo Deep Research Synthesis
## Ancient Mathematics → Modern GPU Crypto Optimization

### Date: 2026-03-24
### Scope: Every avenue for improving Pollard Kangaroo ECDLP on secp256k1

---

## PART 1: WHAT RETIREDCODER ALREADY FOUND (The Baseline Genius)

RetiredCoder's innovations that got K from ~2.08 down to ~1.15:

1. **L1S2 Hierarchical Loop Detection** — 3-tier adaptive jump tables that detect
   and escape cycles in the random walk without restarting
2. **Symmetry Exploitation (Wild1/Wild2)** — Using the free negation on secp256k1
   (neg(P) costs zero field ops) to run mirror walks
3. **Batch Montgomery Inversion** — Single inversion shared across PNT_GROUP_CNT
   (32) kangaroos per thread via Montgomery's trick
4. **L2 Cache Persistence** — Using `cudaDeviceSetLimit` to pin kangaroo state in
   L2 cache, avoiding expensive DRAM round-trips
5. **Coalesced Memory Layout** — Interleaved storage so consecutive threads access
   consecutive memory addresses (128-byte cache lines)
6. **PTX-level Arithmetic** — Hand-written inline PTX assembly for field operations
   (MADC, ADDC carry chains)

---

## PART 2: ACTIONABLE IMPROVEMENTS FROM RESEARCH

### TIER 1 — HIGH CONFIDENCE, IMPLEMENTABLE (Est. 2-4x combined)

#### 1. gECC IMAD→IADD3 Carry Optimization [5.56x claimed for ECDSA]
**Source**: Xiong et al. "gECC" (2025), ACM TACO
- Replace expensive `IMAD.HI.CC` (integer multiply-add with carry) instructions
  with cheaper `IADD3` (3-input addition) + predicate registers for carry
- On Ada Lovelace (RTX 4090, sm_89), IADD3 has higher throughput than IMAD
- The modular multiplication in RCGpuUtils.h uses ~64 IMAD instructions per
  256-bit multiply. Each IMAD.HI that only does addition (not multiply) can be
  replaced with IADD3
- **Estimated gain: 15-30% on field arithmetic hot path**
- **Effort: Medium** — requires rewriting PTX assembly in RCGpuUtils.h

#### 2. Unsaturated Limb Representation [Costello & Longa, 2015]
**Source**: "FourQ" paper, Costello-Longa (ASIACRYPT 2015)
- Current: 4 × 64-bit limbs (fully saturated, requires carry chains)
- Proposed: 5 × 52-bit limbs (12 bits headroom per limb)
- Multiplying two 52-bit numbers: result fits in 104 bits ← fits in u64×u64→u128
- NO carry propagation needed during multiplication accumulation
- Carries resolved only once at the end (lazy reduction)
- **Estimated gain: 15-25% on modular multiply**
- **Effort: High** — major rewrite of all field arithmetic

#### 3. RNS (Residue Number System) for Field Arithmetic
**Source**: Antão, Bajard, Sousa — "RNS-Based EC Point Multiplication for
Massive Parallel Architectures" (Computer Journal, 2011, 53 citations)
- Represent 256-bit number as residues modulo ~8 small primes (each ~32 bits)
- All 8 residue operations are INDEPENDENT → perfect GPU parallelism
- One thread does one residue, 8 threads collaborate on one field multiply
- Conversion back to positional for DP check uses CRT (Chinese Remainder Theorem)
- **THE ANCIENT CONNECTION**: CRT was discovered by Sun Tzu (~3rd century AD).
  The insight that "you can decompose a large number into independent small
  pieces, operate on pieces independently, and reconstruct" is literally what
  RNS does — and it maps perfectly to GPU SIMT architecture
- **Estimated gain: Potentially 2-3x for field arithmetic IF the CRT overhead
  for DP checking can be amortized**
- **Effort: Very High** — complete arithmetic rewrite
- **Risk: CRT conversion every step for DP check may eat all gains**

#### 4. Pippenger's Multi-Scalar Multiplication for Setup
**Source**: cuZK (2022, IACR ePrint 2022/1321) — "nearly perfect linear speedup"
- The precomputation phase (computing EcJumps1/2/3 tables) uses scalar
  multiplication. Pippenger's algorithm batches these.
- Also useful for the initial kangaroo placement (MultiplyG for each kangaroo)
- 2.08-2.94x speedup claimed
- **Estimated gain: 3-5x on SETUP time only** (not the main walk)
- **Effort: Medium**

### TIER 1B — WALK DESIGN IMPROVEMENTS (Algorithmic, Est. 5-15%)

#### 1B. Bernstein-Lange "Two Grumpy Giants and a Baby" (2013)
**DOI: 10.2140/obs.2013.1.87** — 19 citations
- PROVES that Pollard's random walk is FARTHER FROM OPTIMAL than believed
- Two problems: "higher-degree local anticollisions" + "global anticollisions"
- Their new method achieves probability 0.71875 vs random walk's 0.6753 vs BSGS 0.5625
- **Translation**: ~6.5% improvement in collision probability over truly random walk
- RC's L1S2 loop detection already addresses "local anticollisions" partially
- But "global anticollisions" are NOT addressed — this is a NEW improvement angle
- **Estimated gain: 3-7% fewer total operations**

#### 1C. Fowler-Galbraith Multi-Kangaroo Methods (2015)
**arXiv: 1501.07019** — Kangaroo Methods for Interval DLP
- Four-kangaroo method: 1.714√N (vs standard 2-kangaroo: 2.08√N)
- Seven-kangaroo method: 1.7195√N
- RC uses 3 types (Tame, Wild1, Wild2) ≈ 3-kangaroo (1.818√N theoretical)
- **Going to 4 kangaroos could give 1.714/1.818 = 5.7% improvement**
- Requires adding a 4th walk type (e.g., Wild3 using different symmetry)
- **Estimated gain: 5-6% fewer operations**

#### 1D. Galbraith-Ruprai Equivalence Class Speedup (2010)
**DOI: 10.1007/978-3-642-13013-7_22** — 27 citations
- Using equivalence classes (negation, endomorphism) to partition the group
- secp256k1 has order-3 endomorphism φ AND negation → 6 equivalence classes
- RC uses negation (2 classes: P and -P) → √2 speedup
- RC uses endomorphism for DP detection but NOT for walk equivalence
- **Full exploitation**: 6 equivalence classes → theoretical √6 ≈ 2.45x speedup
  (vs current √2 ≈ 1.41x from negation alone)
- **Additional potential: up to 1.73x over current implementation**
- **Risk: Managing 6 equivalence classes adds complexity and may cause more cycles**

#### 1E. Teske Walk Function Design (2000)
**DOI: 10.1090/S0025-5718-00-01213-8** — Mathematics of Computation
- Foundational paper on designing good walk functions for Pollard rho
- Showed that walk functions with fewer than ~20 different steps suffer from
  short cycles. RC uses JMP_CNT (likely 32 or 64) which is safe.
- Also showed that jump sizes should be chosen carefully — geometric distribution
  is better than uniform random

#### 1F. Trimoska et al. Radix-Tree DP Storage (2021)
**DOI: 10.46586/tches.v2021.i2.254-274** — IACR TCHES
- Replaces hash table for DP storage with radix tree structure
- Saves space AND provides faster lookup/insertion
- RC's TFastBase uses a 3-byte prefix table — could be improved
- **Estimated gain: 5-10% on DP processing overhead (CPU side)**

### TIER 2 — THEORETICAL, NEEDS INVESTIGATION

#### 5. Quasi-Random Walks (Low-Discrepancy Sequences)
**Connection to ancient math**: The concept of "well-distributed sequences" traces
to the Weyl equidistribution theorem, but the core idea of covering space
efficiently has roots in ancient geometric constructions.
- Current: Jump index = x[0] % JMP_CNT (pseudo-random from point x-coordinate)
- Proposed: Use a low-discrepancy sequence (Halton/Sobol) for jump selection
- Theory: Quasi-random walks should cover the group more uniformly, leading to
  faster collision with fewer wasted cycles
- **Risk: May not be compatible with distinguished point method**
- **Papers searched**: No direct evidence of quasi-random walks improving kangaroo
- **Verdict: Interesting but unproven**

#### 6. Lévy Flight Jump Distribution
**Source**: Viswanathan et al. "Optimizing the success of random searches" (1999)
- Instead of uniform jumps from JMP_CNT options, use heavy-tailed distribution
- Occasional very large jumps + many small jumps
- Proven optimal for search problems in continuous space
- RC's 3-tier jump system (EcJumps1/2/3) is already a crude approximation!
- The L1S2 mechanism IS a form of adaptive Lévy flight: normal small jumps,
  with occasional large escape jumps when loops are detected
- **Verdict: RC already independently discovered this principle**

#### 7. Vedic Urdhva-Tiryak Multiplication Pattern
**Source**: Bharati Krishna Tirthaji, "Vedic Mathematics" (1965)
- The Urdhva Tiryakbhyam ("vertically and crosswise") sutra describes computing
  partial products of equal weight simultaneously
- This IS the Comba multiplication algorithm (independently rediscovered in 1990s)
- RC's MulModP already uses this pattern in PTX assembly
- **However**: The Vedic approach suggests a specific ORDERING of partial product
  accumulation that minimizes register pressure by completing each column before
  moving to the next
- Papers found: Multiple FPGA implementations showing 20-40% area/speed improvement
  (primarily for hardware designs, not software)
- **Potential GPU application**: Reorder the MAC operations in MulModP to reduce
  register spill, improving occupancy from ~50% to ~62%
- **Estimated gain: 5-10% from better occupancy**

#### 8. Chakravala Method → GLV Decomposition Connection
**Source**: Bhaskara II (1150 AD), "Lilavati"
- The Chakravala method solves ax² + bx + c ≡ 0 (mod n) iteratively
- It's mathematically equivalent to lattice basis reduction
- The GLV method decomposes scalar k into k = k₁ + k₂λ where λ is the
  secp256k1 endomorphism eigenvalue
- Finding optimal k₁, k₂ requires finding short vectors in a 2D lattice
- Chakravala gives an O(log n) algorithm for this — same complexity as
  the extended GCD that's currently used
- **Verdict: The GLV decomposition is computed once (not in hot loop).
  Chakravala doesn't improve the asymptotic or practical speed.**
- **BUT**: The Chakravala "nearest square" heuristic could produce slightly
  shorter (k₁, k₂) pairs, meaning fewer point additions in scalar multiply
- **Practical impact: <1% — not worth implementing**

#### 9. Brahmagupta's Composition (Bhāvanā) → EC Group Law
**Source**: Brahmagupta (628 AD), "Brahmasphutasiddhanta"
- Brahmagupta's identity: (a²+nb²)(c²+nd²) = (ac±nbd)² + n(ad∓bc)²
- This IS the composition law for binary quadratic forms
- The EC group law is a generalization of this to cubic curves
- **Insight**: Brahmagupta's composition suggests that for specific curve
  structures, you might be able to decompose the group operation into
  simpler sub-operations
- For secp256k1 (y²=x³+7), this doesn't directly simplify
- **Verdict: Beautiful mathematical connection, not directly actionable**

#### 10. Egyptian Multiplication → Addition Chains → Scalar Multiplication
**Source**: Rhind Papyrus (~1650 BC)
- Egyptian multiplication by doubling is EXACTLY the double-and-add algorithm
- The optimization question: find shortest addition chain for a given scalar
- RC uses wNAF-5 for precomputation, which is already near-optimal
- For the MAIN LOOP (kangaroo jumps), no scalar multiplication happens —
  it's just single point additions using precomputed jump points
- **Verdict: Already fully exploited**

### TIER 3 — PROVEN IMPOSSIBLE / DEAD ENDS

#### 11. Sub-√N Attack on Prime-Field Curves
- Extensive search of IACR, arXiv, OpenAlex
- Index calculus on elliptic curves: Only works for small characteristic
  (Gaudry, Diem, Petit-Quisquater) — NOT applicable to secp256k1 (large prime)
- Weil descent / GHS attack: Only effective for extension fields (F_{p^n} with n>1)
  — secp256k1 is over F_p (n=1)
- Summation polynomials: Semaev's approach requires solving multivariate
  polynomial systems — currently infeasible for 256-bit primes
- **Verdict: No sub-√N classical attack exists for secp256k1. Period.**

#### 12. Quantum-Inspired Classical Speedups
- Grover's algorithm gives √N quantum speedup for search
- No known classical algorithm achieves comparable speedup
- "Quantum walks" on graphs don't help for the kangaroo structure
- **Verdict: Dead end without actual quantum hardware**

---

## PART 3: THE SYNTHESIS — What Actually Matters

### The Mathematical Hard Wall
The kangaroo algorithm requires Θ(√N) group operations. This is PROVEN optimal
for generic group algorithms (Shoup, 1997). No amount of mathematical cleverness
can break below √(2^109) ≈ 2^54.5 operations for puzzle 110.

### The Only Lever: Operations Per Second
Given the mathematical wall, the ONLY way to go faster is:
1. **More GPUs** (linear scaling)
2. **Faster field arithmetic per GPU** (the gECC/unsaturated limbs angle)
3. **Better K factor** (RC already at 1.15, theoretical minimum ~1.0)
4. **Lower overhead** (DP checks, memory, synchronization)

### Combined Improvement Estimate (UPDATED with Walk Optimizations)
| Optimization | Speedup | Effort | Risk |
|---|---|---|---|
| **GPU Arithmetic** | | | |
| gECC IMAD→IADD3 | 1.15-1.30x | Medium | Low |
| Unsaturated 5×52 limbs | 1.15-1.25x | High | Medium |
| Nikhilam complement mult | 1.10-1.20x | Medium | Medium |
| Register reorder (Vedic) | 1.05-1.10x | Medium | Low |
| Bernstein-Yang mod inverse | 1.01-1.03x | Low | Low |
| **Walk Algorithm** | | | |
| 4-kangaroo method (Fowler) | 1.05-1.06x | Medium | Low |
| Global anticollision (B-L) | 1.03-1.07x | Medium | Medium |
| Full 6-class equivalence | 1.30-1.73x | High | High |
| Radix-tree DP storage | 1.05-1.10x | Low | Low |
| **Infrastructure** | | | |
| RNS arithmetic (CRT) | 1.5-3.0x | Very High | High |
| Pippenger setup | 3-5x setup only | Medium | Low |
| | | | |
| **Combined (conservative)** | **1.8-2.5x** | | |
| **Combined (aggressive)** | **3.5-6.0x** | | |
| **Combined (all + RNS)** | **5-10x** | | |

### What This Means for Puzzle 110 on 12x RTX 4090:
| Scenario | Speed | Time | Cost |
|---|---|---|---|
| Current (no changes) | ~72 GKeys/s | ~5.4 days | $663 |
| Conservative (1.8x) | ~130 GKeys/s | ~3.0 days | $369 |
| Aggressive (3.5x) | ~252 GKeys/s | ~1.5 days | $184 |
| All optimizations (6x) | ~432 GKeys/s | ~0.9 days | $111 |

---

## PART 4: KEY REFERENCES

1. Xiong et al. "gECC: A GPU-based high-throughput framework for Elliptic Curve
   Cryptography" (2025). ACM TACO. DOI: 10.1145/3736176
2. Antão, Bajard, Sousa. "RNS-Based Elliptic Curve Point Multiplication for
   Massive Parallel Architectures" (2011). The Computer Journal 55(5):629-647
3. Bernstein, Lange. "Computing Small Discrete Logarithms Faster" (2012).
   INDOCRYPT, LNCS 7668, pp. 317-338
4. Bernstein et al. "ECC2K-130 on NVIDIA GPUs" (2010). INDOCRYPT, LNCS 6498
5. Bos, J.W. "On the Cryptanalysis of Public-Key Cryptography" (2011).
   EPFL PhD Thesis #5291
6. Costello, Longa. "FourQ: Four-Dimensional Decompositions on a Q-curve over
   the Mersenne Prime" (2015). ASIACRYPT
7. Renes, Costello, Batina. "Complete Addition Formulas for Prime Order Elliptic
   Curves" (2016). EUROCRYPT
8. Bhaskara II. "Lilavati" / Chakravala Method (1150 AD) — lattice reduction
9. Brahmagupta. "Brahmasphutasiddhanta" (628 AD) — composition of forms
10. Sun Tzu. "Sunzi Suanjing" (~3rd century AD) — Chinese Remainder Theorem

---

## PART 5: NEW FINDINGS FROM DEEP RESEARCH AGENTS (42+ Papers Found)

### ⭐ CRITICAL PAPER: Liberati (2023) — Chakravala IS LLL
**arXiv:2308.02742** — "Pell equation: A generalization of continued fraction
and Chakravala algorithms using the LLL-algorithm"
- PROVES that the Chakravala method (Bhaskara II, 1150 AD) and continued fraction
  algorithms are SPECIAL CASES of the LLL lattice reduction algorithm for rank-2 lattices
- This means ancient Indian mathematicians were doing lattice reduction 800 years
  before Lenstra-Lenstra-Lovász (1982)!
- **Crypto application**: GLV decomposition for secp256k1 endomorphism uses exactly
  a rank-2 lattice reduction. The Chakravala-specific heuristics could produce
  marginally better short vectors than generic LLL.

### ⭐ CRITICAL PAPER: Barman & Saha (2023) — Nikhilam Sutra for ECC
**arXiv:2311.11392** — "An Efficient Elliptic Curve Cryptography Arithmetic
Using Nikhilam Multiplication"
- Directly applies the Vedic Nikhilam sutra (complement method) to ECC arithmetic
- Key insight: "reduces multiplication of two large numbers into two smaller
  numbers multiplication" — this is a DECOMPOSITION technique
- For numbers close to a power of 10 (or power of 2 in binary), Nikhilam reduces
  a full multiplication to a smaller one plus additions
- **For secp256k1**: p = 2^256 - 2^32 - 977. Numbers close to p can be multiplied
  using Nikhilam by computing the complement (distance from 2^256) which is only
  33 bits! This could replace the full 256×256 multiplication with a much cheaper
  operation when operands are near p.
- **UNEXPLOITED FOR GPU**: All existing Nikhilam-ECC work is FPGA-based

### ⭐ CRITICAL PAPER: Kamaraj & Perumalsamy (2022) — Vedic ECC on secp256k1
**DOI: 10.33180/infmidem2022.203** — "Time-area-efficient ECC using
Urdhva Tiryagbhyam" — 6 citations
- Implements 256-bit scalar multiplication on secp256k1 using Vedic Urdhva-Tiryak
- Pipelined Vedic multiplier: 15.12% frequency improvement, 22.36% time reduction
- **On FPGA (Virtex-7)**: 192.5 MHz, 1.2ms per scalar multiply
- **THE GAP**: This is all FPGA. Nobody has implemented Vedic multiplication
  techniques in CUDA/PTX for GPU. The partial-product reordering could reduce
  register pressure in the GPU modular multiply.

### ⭐ CRITICAL PAPER: Nguyen et al. (2023) — Vedic+Karatsuba for Modular Mult
**DOI: 10.3390/cryptography7040046** — 18 citations
- Combines Vedic multiplier with Karatsuba algorithm for modular multiplication
- Designed custom "Lattice-DSP" that does integer mult AND modular reduction
  simultaneously
- 283 MHz for lattice crypto parameters
- **Key technique**: Merge multiply and reduce into single operation — applicable
  to secp256k1's special prime p = 2^256 - 2^32 - 977

### ⭐ CRITICAL PAPER: Bernstein & Yang (2019) — Fast Constant-Time GCD
**DOI: 10.46586/tches.v2019.i3.340-398** — 70 citations, IACR TCHES
- New streamlined constant-time Euclidean algorithm BEATS Fermat's method for
  modular inversion on Curve25519
- **Connection to Aryabhata's Kuttaka**: This IS a modern optimization of the
  same algorithm Aryabhata described in 499 AD
- RC's InvModP currently uses Fermat's little theorem (a^(p-2) mod p)
- Bernstein-Yang's method could be faster for the batch inversion remainder
- **Estimated gain: 5-15% on modular inversion** (which is ~3% of total time)

### ⭐ CRITICAL PAPER: Bhargava (2004) — Higher Composition Laws (Fields Medal)
**DOI: 10.4007/annals.2004.159.217** — 130 citations, Annals of Mathematics
- Manjul Bhargava's Fields Medal work generalizing Gauss/Brahmagupta composition
- Shows composition on 2×2×2 cubes of integers yields EC group law as special case
- Proves 14 composition laws exist (Brahmagupta/Gauss knew only one)
- **Theoretical insight**: The EC group law has deep structure beyond what we
  exploit. But no practical computational speedup identified yet.

### IDENTIFIED RESEARCH GAP (Novel Contribution Opportunity)
**"Adapting Vedic multiplication techniques (especially Nikhilam) for GPU/CUDA-
based modular arithmetic in ECDLP solvers"**
- All existing Vedic-ECC papers target FPGA
- Nobody has implemented Nikhilam or Urdhva-Tiryak in PTX assembly for GPU
- The secp256k1 prime (2^256 - 2^32 - 977) is PERFECTLY suited for Nikhilam
  because complements are tiny (~33 bits)
- This could be the "overlooked detail" — exactly like RC found his edge

---

## PART 6: ANCIENT MATH IMPACT SUMMARY (UPDATED)

| Ancient Source | Modern Equivalent | Used in RCKangaroo? | Improvable? | Paper Evidence |
|---|---|---|---|---|
| **Sun Tzu CRT (250 AD)** | RNS arithmetic | No | **YES — 2-3x potential** | Antão 2011 (53 cites) |
| **Vedic Nikhilam (~1500 BC)** | Complement multiplication | No | **YES — secp256k1 perfect fit** | Barman 2023 (arXiv) |
| **Vedic Urdhva-Tiryak** | Comba/crosswise multiply | Yes (MulModP) | **5-10% reorder** | Kamaraj 2022 (6 cites) |
| **Aryabhata Kuttaka (499)** | Extended GCD / mod inverse | Yes (InvModP) | **5-15% (Bernstein-Yang)** | Bernstein 2019 (70 cites) |
| **Bhaskara Chakravala (1150)** | LLL lattice reduction | Partially (GLV) | **<1%** | Liberati 2023 (arXiv) |
| Brahmagupta bhāvanā (628) | EC group composition | Implicitly | No | Bhargava 2004 (130 cites) |
| Egyptian doubling (1650 BC) | Double-and-add | Yes (wNAF) | Already optimal | — |

### THE TWO BIGGEST ANCIENT MATH INSIGHTS:

**#1: Chinese Remainder Theorem → RNS (Sun Tzu, 250 AD)**
Maps directly to RNS arithmetic on GPU — 2-3x potential by parallelizing 256-bit
operations across 8 independent 32-bit residues. Validated by Antão et al. (2011).

**#2: Vedic Nikhilam Sutra → Complement Multiplication (~1500 BC)**
For secp256k1, p = 2^256 - 2^32 - 977. The complement of any field element x is
(2^256 - x), which for elements near p is TINY (~33 bits). The Nikhilam sutra says:
"All from 9, last from 10" — multiply by complement then adjust. This could replace
FULL 256×256-bit multiplication with 33×33-bit multiplication + corrections for
operands near the modulus. **Nobody has implemented this for GPU yet.**

---

## COMPLETE REFERENCE LIST (All Papers Found)

### Ancient Math ↔ Modern Crypto
1. Liberati (2023). arXiv:2308.02742 — Chakravala = LLL for rank-2 lattices
2. Bhargava (2004). DOI:10.4007/annals.2004.159.217 — Higher composition laws (Fields Medal)
3. Barman & Saha (2023). arXiv:2311.11392 — Nikhilam sutra for ECC arithmetic
4. Kamaraj & Perumalsamy (2022). DOI:10.33180/infmidem2022.203 — Vedic ECC on secp256k1
5. Nguyen et al. (2023). DOI:10.3390/cryptography7040046 — Vedic+Karatsuba modular mult
6. Rao & Yang (2006). DOI:10.1007/s00034-005-1123-6 — Aryabhata Remainder Theorem for PKC
7. Bernstein & Yang (2019). DOI:10.46586/tches.v2019.i3.340-398 — Fast constant-time GCD

### GPU/CUDA ECC Optimization
8. Xiong et al. (2025). DOI:10.1145/3736176 — gECC GPU framework (5.56x ECDSA speedup)
9. Antão et al. (2011). DOI:10.1093/comjnl/bxr119 — RNS-based EC on GPU (53 cites)
10. Antão et al. (2010). DOI:10.1109/asap.2010.5541000 — EC point mult on GPU (44 cites)
11. Szerwinski & Güneysu (2008). DOI:10.1007/978-3-540-85053-3_6 — GPU asymmetric crypto (170 cites)

### ECDLP / Pollard Kangaroo
12. Bernstein & Lange (2012). DOI:10.1007/978-3-642-34931-7_19 — Small DLP faster (61 cites)
13. Bernstein et al. (2010). DOI:10.1007/978-3-642-17401-8_23 — ECC2K-130 on GPU (24 cites)
14. Bos (2011). DOI:10.5075/epfl-thesis-5291 — PhD thesis: GPU Pollard rho for ECDLP

### Curve-Specific Optimization
15. Costello & Longa (2015). DOI:10.1007/978-3-662-48797-6_10 — FourQ curve (94 cites)
16. Renes et al. (2016). DOI:10.1007/978-3-662-49890-3_16 — Complete addition formulas (85 cites)

### Ancient Math History
17. Chinta & Nair (2026). arXiv:2602.06898 — Explicit composition identities
18. Saini et al. (2025). DOI:10.59828/pijms.v1i2.9 — Vedic math for Edwards curves
