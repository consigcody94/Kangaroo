# Kangaroo ECDLP Solver - Claude Code Directives

## Project Overview

This is a high-performance GPU-accelerated Pollard Kangaroo solver for the Elliptic Curve Discrete Logarithm Problem (ECDLP) on secp256k1. Fork of JeanLucPons/Kangaroo + RetiredCoder/RCKangaroo with research-driven optimizations.

**Goal**: Solve Bitcoin Puzzle #135 (134-bit search space, 13.5 BTC bounty) within a 3-month rental window on 8-16x RTX 5090 GPUs.

## Repository Structure

```
optimized/              # <-- PRIMARY WORKING DIRECTORY for all GPU code
  RCGpuCore.cu          # GPU kernels (KernelA=jumps, KernelB=distances, KernelC=loop escape)
  RCGpuUtils.h          # PTX field arithmetic (MulModP, SqrModP, InvModP, SubModP)
  RCKangaroo.cpp        # Main solver: jump tables, collision detection, CLI
  GpuKang.cpp           # GPU memory allocation, kernel launch, L2 cache config
  GpuKang.h             # GPU class definition
  defs.h                # Constants, GPU tier thresholds, endomorphism constants
  Ec.cpp / Ec.h         # CPU-side EC arithmetic
  utils.cpp / utils.h   # Cross-platform utilities

SECPK1/                 # Original JeanLucPons big integer + EC math (CPU-only path)
GPU/                    # Original JeanLucPons GPU kernels (not used by optimized/)
```

## Current Optimization Status (branch: claude/refine-kangaroo-algorithm-BdILA)

### Implemented & Needs GPU Testing
1. **Bug fix**: `jmp_idx` typo -> `jmp_ind` in RCGpuCore.cu line 130-131
2. **Endomorphism DP**: Ported beta*x / beta^2*x DP detection to OLD_GPU path (sm_86)
3. **RTX 5090/Blackwell**: sm_100 compute tier, PNT_GROUP=32, arch-based GPU detection
4. **Inline PTX carry chains**: Merged carry propagation in mul_256_by_P0inv and mul_256_by_64
5. **Adaptive jump spread**: Auto-scales with range size, `-spread N` override
6. **DP auto-tuning**: Auto-computes optimal DP when `-dp` omitted
7. **6-class equivalence walk**: `-equiv6` flag, Canonicalize6() function

### Measured Baseline (before these changes)
- K constant: 0.75 mean (0.37 best) on RTX 3060
- Speed: 1,555 MKeys/s on RTX 3060
- K theoretical floor: 0.51 for secp256k1

## Build Instructions

### RTX 3060 (sm_86) - YOUR TEST GPU:
```bash
cd optimized
nvcc -O2 -arch=sm_86 -o RCKangaroo Ec.cpp RCKangaroo.cpp GpuKang.cpp RCGpuCore.cu utils.cpp -lcuda
```

### RTX 4090 (sm_89):
```bash
nvcc -O2 -arch=sm_89 -o RCKangaroo Ec.cpp RCKangaroo.cpp GpuKang.cpp RCGpuCore.cu utils.cpp -lcuda
```

### RTX 5090 (sm_100):
```bash
nvcc -O2 -arch=sm_100 -o RCKangaroo Ec.cpp RCKangaroo.cpp GpuKang.cpp RCGpuCore.cu utils.cpp -lcuda
```

## Testing Protocol

### STEP 1: Compile and verify build
```bash
cd optimized
nvcc -O2 -arch=sm_86 -o RCKangaroo Ec.cpp RCKangaroo.cpp GpuKang.cpp RCGpuCore.cu utils.cpp -lcuda
```
If compile errors occur, fix them. The PTX inline asm may need adjustment for specific NVCC versions.

### STEP 2: Puzzle 120 baseline (standard mode, ~30-90 min on 3060)
```bash
./RCKangaroo -dp 16 -range 120 \
  -start 800000000000000000000000000000 \
  -pubkey 02ceb6cbbcdbdf5ef7150682150f4ce2c6f4807b349827dcdbdd1f2efa885a2630
```
Record: **MKeys/s** and **K value** printed at solve.

### STEP 3: Puzzle 120 with equiv6 (experimental)
```bash
./RCKangaroo -dp 16 -range 120 -equiv6 \
  -start 800000000000000000000000000000 \
  -pubkey 02ceb6cbbcdbdf5ef7150682150f4ce2c6f4807b349827dcdbdd1f2efa885a2630
```
Record: **MKeys/s** and **K value**. Compare with Step 2.

### STEP 4: Multiple runs for statistical significance
Run Steps 2 and 3 at least 3 times each. Compute mean K for each mode.

### What to look for:
- **MKeys/s > 1,555**: PTX carry chain optimization is working
- **K < 0.75 (standard)**: Adaptive spread or endomorphism DP improvement
- **K < 0.50 (equiv6)**: 6-class equivalence walk is working (sqrt(3) gain)
- **Errors = 0**: No bugs in the arithmetic
- **"Mode: Old"**: Confirms 3060 takes OLD_GPU path (correct for sm_86)
- **"Auto DP"**: DP auto-tuning is active
- **"Jump spread"**: Adaptive spread printout

## Key Constants (defs.h)

- `JMP_CNT = 512` - Number of precomputed jump points
- `PNT_GROUP_CNT` - Kangaroos per thread (24 new, 32 blackwell, 64 old)
- `BLOCK_SIZE` - Threads per block (256 new/blackwell, 512 old)
- `STEP_CNT = 1000` - Kernel iterations per launch
- `ENDO_BETA_*` - secp256k1 endomorphism cube root of unity
- `ENDO_LAMBDA_*` - Endomorphism eigenvalue on group order

## CLI Options

```
-dp N          Distinguished point bit count (auto if omitted)
-range N       Range in bits (e.g., 120 for puzzle #120)
-start HEX     Start of search range
-pubkey HEX    Target compressed public key
-spread N      Jump spread parameter (1-20, auto if omitted)
-equiv6        Enable 6-class equivalence walk (experimental)
-gpu 012...    GPU device mask
-tames FILE    Load/save tame kangaroo file
-max N         Max operations multiplier before giving up
```

## Puzzle #135 Target

```
Range:  2^134
Start:  4000000000000000000000000000000000
PubKey: 02145D2611C823A396EF6712CE0F712F09B9B4F3135E3E0AA3230FB9B6D08D1E16
Bounty: 13.5 BTC
```

## Architecture Notes

- **Hot path**: KernelA in RCGpuCore.cu - each step does 5x MulModP + 1x SqrModP + 1x InvModP (batched)
- **Field arithmetic**: 4x64-bit limbs, PTX inline assembly, secp256k1-specific reduction
- **Batch inversion**: Montgomery's trick across PNT_GROUP_CNT kangaroos per thread
- **L2 cache persistence**: Kangaroo state pinned in L2 on sm_89+ GPUs
- **L1S2 loop detection**: 3-tier adaptive jump tables detect and escape walk cycles
- **DP method**: Distinguished points with configurable bit count for collision detection

## If Compile Fails

Common issues:
1. **NVCC version**: Need CUDA 12.0+. Check with `nvcc --version`
2. **PTX inline asm errors**: The merged carry chain asm blocks in RCGpuUtils.h may need operand count adjustments for specific NVCC versions. Fall back to the `#else` (OLD_GPU) path if needed.
3. **Missing `-lcuda`**: Link against CUDA driver API
4. **Arch mismatch**: Use `-arch=sm_86` for 3060, not sm_89 or sm_100

## Research References

- Pollard 2025 "Lethargic Kangaroo" - jump variance optimization
- Miller-Venkatesan 2006 - spectral gap / XOR-fold hash
- Galbraith-Ruprai 2010 - 6-class equivalence (PKC 2010)
- Xiong et al. 2025 "gECC" - IADD3 carry optimization (ACM TACO)
- Duursma-Gaudry-Morain 1999 - automorphism speedup
