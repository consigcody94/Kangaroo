(c) 2024, RetiredCoder (RC). K-push patches (2026-04): this fork.

RCKangaroo is free and open-source (GPLv3).
This software demonstrates efficient GPU implementation of SOTA Kangaroo method for solving ECDLP.
It's part #3 of RC's research: https://github.com/RetiredC

Discussion thread: https://bitcointalk.org/index.php?topic=5517607

> **K-push fork notes:** adds class-aware Gaudry-Schost collision recovery using secp256k1's order-6 automorphism group (negation × β-endomorphism). Measured K on 5070 (N=8): mean 0.82, median 0.67, best 0.34 — approaching the √6 theoretical floor of 0.51. See [CHANGES.md](CHANGES.md) for full detail on the MulModN implementation and the class_idx DP-record plumbing.

<b>Features:</b>

- Baseline K=1.15 (RC) → measured K_mean≈0.82 / K_best≈0.34 after the K-push patches.
- Fast: ~2 GK/s on RTX 5070 (sm_120), ~8 GK/s on RTX 4090.
- Keeps DP overhead as small as possible.
- Supports ranges up to 170 bits.
- Windows (this fork verified) and Linux.

<b>Limitations:</b>

- No advanced features like networking, saving/loading DPs, etc.

<b>Command line parameters:</b>

<b>-gpu</b>		which GPUs are used, for example, "035" means that GPUs #0, #3 and #5 are used. If not specified, all available GPUs are used. 

<b>-pubkey</b>		public key to solve, both compressed and uncompressed keys are supported. If not specified, software starts in benchmark mode and solves random keys. 

<b>-start</b>		start offset of the key, in hex. Mandatory if "-pubkey" option is specified. For example, for puzzle #85 start offset is "1000000000000000000000". 

<b>-range</b>		bit range of private the key. Mandatory if "-pubkey" option is specified. For example, for puzzle #85 bit range is "84" (84 bits). Must be in range 32...170. 

<b>-dp</b>		DP bits. Must be in range 14...60. Low DP bits values cause larger DB but reduces DP overhead and vice versa. 

<b>-max</b>		option to limit max number of operations. For example, value 5.5 limits number of operations to 5.5 * 1.15 * sqrt(range), software stops when the limit is reached. 

<b>-tames</b>		filename with tames. If file not found, software generates tames (option "-max" is required) and saves them to the file. If the file is found, software loads tames to speedup solving. 

When public key is solved, software displays it and also writes it to "RESULTS.TXT" file. 

Sample command line for puzzle #85:

RCKangaroo.exe -dp 16 -range 84 -start 1000000000000000000000 -pubkey 0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a

Sample command to generate tames:

RCKangaroo.exe -dp 16 -range 76 -tames tames76.dat -max 10

Then you can restart software with same parameters to see less K in benchmark mode or add "-tames tames76.dat" to solve some public key in 76-bit range faster.

<b>Some notes:</b>

Fastest ECDLP solvers will always use SOTA/SOTA+ method, as it's 1.4/1.5 times faster and requires less memory for DPs compared to the best 3-way kangaroos with K=1.6. 
Even if you already have a faster implementation of kangaroo jumps, incorporating SOTA method will improve it further. 
While adding the necessary loop-handling code will cause you to lose about 5–15% of your current speed, the SOTA method itself will provide a 40% performance increase. 
Overall, this translates to roughly a 25% net improvement, which should not be ignored if your goal is to build a truly fast solver. 


<b>Changelog:</b>

v3.1:

- fixed "gpu illegal memory access" bug.
- some small improvements.

v3.0:

- added "-tames" and "-max" options.
- fixed some bugs.

v2.0:

- added support for 30xx, 20xx and 1xxx cards.
- some minor changes.

v1.1:

- added ability to start software on 30xx cards.

v1.0:

- initial release.