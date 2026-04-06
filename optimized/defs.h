// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC


#pragma once 

#pragma warning(disable : 4996)

typedef unsigned long long u64;
typedef long long i64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
typedef unsigned char u8;
typedef char i8;



#define MAX_GPU_CNT			32

//must be divisible by MD_LEN
#define STEP_CNT			1000

#define JMP_CNT				512

//can be 8, 16, 24, 32
#define PNT_GROUP_NEW_GPU	24
//can be 8, 16, 24, 32, 40, 48, 56, 64
#define PNT_GROUP_OLD_GPU	64
//Blackwell (RTX 5090, sm_120): 96MB L2 total, 170 SMs
//24 kangs * 256 threads * 170 SMs * 96 bytes = 95.6 MB (fits 96MB L2)
//32 kangs would be 127 MB — overflows L2, killing persisting cache optimization
#define PNT_GROUP_BLACKWELL	24

#define BLOCK_SIZE_NEW_GPU	256
#define BLOCK_SIZE_OLD_GPU	512
#define BLOCK_SIZE_BLACKWELL	256

//use different options for cards older than RTX 40xx
#ifdef __CUDA_ARCH__
#if __CUDA_ARCH__ < 890
#define OLD_GPU
#elif __CUDA_ARCH__ >= 1000
#define BLACKWELL_GPU
#endif
#ifdef OLD_GPU
#define BLOCK_SIZE			BLOCK_SIZE_OLD_GPU
#define PNT_GROUP_CNT		PNT_GROUP_OLD_GPU
#elif defined(BLACKWELL_GPU)
#define BLOCK_SIZE			BLOCK_SIZE_BLACKWELL
#define PNT_GROUP_CNT		PNT_GROUP_BLACKWELL
#else
#define BLOCK_SIZE			BLOCK_SIZE_NEW_GPU
#define PNT_GROUP_CNT		PNT_GROUP_NEW_GPU
#endif
#else //CPU, fake values
#define BLOCK_SIZE			BLOCK_SIZE_OLD_GPU
#define PNT_GROUP_CNT		PNT_GROUP_OLD_GPU
#endif

// kang type
#define TAME				0  // Tame kangs
#define WILD1				1  // Wild kangs1
#define WILD2				2  // Wild kangs2
#define WILD3				3  // Wild kangs3 (endomorphism-shifted, 4-kangaroo method)

#define GPU_DP_SIZE			48
#define MAX_DP_CNT			(256 * 1024)

#define JMP_MASK			(JMP_CNT-1)

#define DPTABLE_MAX_CNT		16

#define MAX_CNT_LIST		(512 * 1024)

#define DP_FLAG				0x8000
#define INV_FLAG			0x4000
#define JMP2_FLAG			0x2000

#define MD_LEN				10

// secp256k1 endomorphism: phi(x,y) = (beta*x, y), where beta^3 = 1 mod p
// beta = 0x7ae96a2b657c07106e64479eac3434e99cf0497512f58995c1396c28719501ee
// Enables sqrt(3) additional speedup via order-6 automorphism group
#define ENDO_BETA_0  0xc1396c28719501eeULL
#define ENDO_BETA_1  0x9cf0497512f58995ULL
#define ENDO_BETA_2  0x6e64479eac3434e9ULL
#define ENDO_BETA_3  0x7ae96a2b657c0710ULL

// lambda: eigenvalue of phi on the group order n
// lambda = 0x5363ad4cc05c30e0a5261c028812645a122e22ea20816678df02967c1b23bd72
#define ENDO_LAMBDA_0 0xdf02967c1b23bd72ULL
#define ENDO_LAMBDA_1 0x122e22ea20816678ULL
#define ENDO_LAMBDA_2 0xa5261c028812645aULL
#define ENDO_LAMBDA_3 0x5363ad4cc05c30e0ULL

// Endomorphism DP flag
#define ENDO_FLAG			0x1000

// 6-class equivalence walk: canonicalize points using {1, -1, phi, -phi, phi^2, -phi^2}
// Tag bits in jmp_ind (3 bits for equivalence class 0-5)
#define EQUIV6_TAG_MASK		0x0700
#define EQUIV6_TAG_SHIFT	8

// beta^2 mod p = beta * beta (second cube root of unity)
// beta^2 = 0x851695d49a83f8ef919bb86153cbcb16630fb68aed0a766a3ec693d68e6afa40
#define ENDO_BETA2_0 0x3ec693d68e6afa40ULL
#define ENDO_BETA2_1 0x630fb68aed0a766aULL
#define ENDO_BETA2_2 0x919bb86153cbcb16ULL
#define ENDO_BETA2_3 0x851695d49a83f8efULL

// lambda^2 mod n (for distance recovery in 6-class walk)
// Correct value: lambda^2 mod n = n - lambda - 1
// = 0xAC9C52B33FA3CF1F 5AD9E3FD77ED9BA4 A880B9FC8EC739C2 E0CFC810B51283CE
// Verified against PSCKangaroo v55 fix for the same bug
#define ENDO_LAMBDA2_0 0xe0cfc810b51283ceULL
#define ENDO_LAMBDA2_1 0xa880b9fc8ec739c2ULL
#define ENDO_LAMBDA2_2 0x5ad9e3fd77ed9ba4ULL
#define ENDO_LAMBDA2_3 0xac9c52b33fa3cf1fULL

//#define DEBUG_MODE

//gpu kernel parameters
struct TKparams
{
	u64* Kangs;
	u32 KangCnt;
	u32 BlockCnt;
	u32 BlockSize;
	u32 GroupCnt;
	u64* L2;
	u64 DP;
	u32* DPs_out;
	u64* Jumps1; //x(32b), y(32b), d(32b)
	u64* Jumps2; //x(32b), y(32b), d(32b)
	u64* Jumps3; //x(32b), y(32b), d(32b)
	u64* JumpsList; //list of all performed jumps, grouped by warp(32) every 8 groups (from PNT_GROUP_CNT). Each jump is 2 bytes: 10bit jump index + flags: INV_FLAG, DP_FLAG, JMP2_FLAG
	u32* DPTable;
	u32* L1S2;
	u64* LastPnts;
	u64* LoopTable;
	u32* dbg_buf;
	u32* LoopedKangs;
	bool IsGenMode; //tames generation mode
	bool IsEquiv6;  //6-class equivalence walk mode

	u32 KernelA_LDS_Size;
	u32 KernelB_LDS_Size;
	u32 KernelC_LDS_Size;	
};

