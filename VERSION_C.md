# Experimental Version C: Sun Tzu's Chinese Remainder Theorem (RNS)

This version tests the viability of mapping **Sun Tzu's Chinese Remainder Theorem (CRT)** to the GPU via **Residue Number Systems (RNS)**.

By representing 256-bit numbers as a set of smaller, mutually coprime moduli residues, the 256-bit dependent addition chains are broken. Operations on residues are completely independent, allowing the GPU warps to parallelize field multiplication significantly.

### Instructions to Test

1. Open `optimized/RCGpuUtils.h`.
2. Locate the large 256-bit multi-word additions in `IADD3` / `Add256` logic.
3. Replace the serial carry-chain with the following prototype RNS base mapping logic.

```cuda
// RNS Field Representation for secp256k1
// Moduli chosen to fit inside 32-bit registers, coprime to secp256k1 p
#define RNS_M0 0xFFFFFFFD // Prime 1
#define RNS_M1 0xFFFFFFFB // Prime 2
#define RNS_M2 0xFFFFFFF1 // Prime 3
#define RNS_M3 0xFFFFFFE5 // Prime 4
#define RNS_M4 0xFFFFFFDF // Prime 5
#define RNS_M5 0xFFFFFFCB // Prime 6
#define RNS_M6 0xFFFFFFC5 // Prime 7
#define RNS_M7 0xFFFFFFBD // Prime 8

// RNS Addition - No Carry Chain!
__device__ __forceinline__ void AddRNS(u32* c, const u32* a, const u32* b)
{
    // Sun Tzu's CRT allows addition with ZERO dependencies between words.
    // This allows the GPU to issue all 8 instructions simultaneously.
    c[0] = (a[0] + b[0]) % RNS_M0;
    c[1] = (a[1] + b[1]) % RNS_M1;
    c[2] = (a[2] + b[2]) % RNS_M2;
    c[3] = (a[3] + b[3]) % RNS_M3;
    c[4] = (a[4] + b[4]) % RNS_M4;
    c[5] = (a[5] + b[5]) % RNS_M5;
    c[6] = (a[6] + b[6]) % RNS_M6;
    c[7] = (a[7] + b[7]) % RNS_M7;
}

// RNS Multiplication - No cross-terms!
__device__ __forceinline__ void MulRNS(u32* c, const u32* a, const u32* b)
{
    // RNS Multiplication is strictly term-wise.
    // An O(n^2) operation is reduced to O(n) independent multiplies.
    c[0] = ((u64)a[0] * b[0]) % RNS_M0;
    c[1] = ((u64)a[1] * b[1]) % RNS_M1;
    c[2] = ((u64)a[2] * b[2]) % RNS_M2;
    c[3] = ((u64)a[3] * b[3]) % RNS_M3;
    c[4] = ((u64)a[4] * b[4]) % RNS_M4;
    c[5] = ((u64)a[5] * b[5]) % RNS_M5;
    c[6] = ((u64)a[6] * b[6]) % RNS_M6;
    c[7] = ((u64)a[7] * b[7]) % RNS_M7;
}
```

4. **Integration Note**: To run this test, you must inject `AddRNS` and `MulRNS` into the kangaroo's point addition (in `RCGpuCore.cu`). Because this is a destructive representation change, this version serves as a *benchmark for modular arithmetic throughput*.
5. Build the project and profile the raw instruction throughput using NVIDIA Nsight (`nvprof` or `nsys`). Check if the integer ALUs achieve 2-3x higher utilization compared to the serial `Add256`/`Mul256`.
