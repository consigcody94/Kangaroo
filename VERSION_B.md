# Experimental Version B: Madhava Correction Term for Walk Convergence

This version applies **Madhava's Correction Terms** to accelerate the pseudo-random walk on the GPU. Instead of letting kangaroos jump purely randomly, we apply a "correction" factor to the jump size based on the partial data of their previous jumps, steering them away from short cycles and anticollisions.

This directly targets a lower `K` constant by intelligently breaking "coupling latency" before it traps the walks.

### Instructions to Test

1. Open `optimized/RCGpuCore.cu`.
2. Locate the main jump selection inside `ProcessJumpDistance` (around line 550).
3. Replace the `ProcessJumpDistance` function with the following Madhava-accelerated version:

```cuda
__device__ __forceinline__ bool ProcessJumpDistance(u32 step_ind, u32 d_cur, u64* d, u32 kang_ind, u64* jmp1_d, u64* jmp2_d, const TKparams& Kparams, u64* table, u32* cur_ind, u8 iter)
{
    u64* jmp_d = (d_cur & JMP2_FLAG) ? jmp2_d : jmp1_d;

    __align__(16) u64 jmp[3];

    // Original jump distance
    u32 base_mask = d_cur & JMP_MASK;

    // --- MADHAVA CORRECTION TERM ---
    // If the kangaroo's last 2 jumps were identical, we apply a correction
    // to force it into a heavy-tailed distribution, breaking short cycles.
    u32 prev_d_cur = table[(iter + MD_LEN - 1) % MD_LEN]; // Look at the history table

    if (base_mask == (prev_d_cur & JMP_MASK)) {
        // Force a large deviation (Madhava shift)
        base_mask = (base_mask + 128) & JMP_MASK;
    }
    // --------------------------------

    ((int4*)(jmp))[0] = ((int4*)(jmp_d + 4 * base_mask))[0];
    jmp[2] = *(jmp_d + 4 * base_mask + 2);

    u8 c = _addcarry_u64(0, d[0], jmp[0], &d[0]);
    c = _addcarry_u64(c, d[1], jmp[1], &d[1]);
    c = _addcarry_u64(c, d[2], jmp[2], &d[2]);
    if (c)
        d[2] ^= 0x8000000000000000ULL; // Quick fallback mask

    // Loop detection via History Table
    int found_ind = -1;
    for (int i = 0; i < MD_LEN; i++)
    {
        if (d[0] == table[i])
        {
            found_ind = i;
            break;
        }
    }

    table[iter] = d[0];
    *cur_ind = (iter + 1) % MD_LEN;

    if (found_ind < 0)
    {
        if (d_cur & DP_FLAG)
            BuildDP(Kparams, kang_ind, d);
        return false;
    }

    u32 LoopSize = (iter + MD_LEN - found_ind) % MD_LEN;
    if (!LoopSize) LoopSize = MD_LEN;
    atomicAdd(Kparams.dbg_buf + LoopSize, 1);

    u32 ind_LastPnts = MD_LEN - 1 - ((STEP_CNT - 1 - step_ind) % LoopSize);
    u32 ind = atomicAdd(Kparams.LoopedKangs, 1);
    Kparams.LoopedKangs[2 + ind] = kang_ind | (ind_LastPnts << 28);
    return true;
}
```

4. Build the code.
5. Run a puzzle solve (e.g., `-range 80` or similar short puzzle) and check the final `K` value reported by the CPU. This modification directly tests if correction factors can artificially boost convergence.
