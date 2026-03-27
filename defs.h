// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC


#pragma once

#pragma warning(disable : 4996)

// Define GS_MODE to enable Gaudry-Schost with order-6 equivalence classes
// When enabled: all walks are equivalent, any collision solves, K target ~0.55
// When disabled: standard 3-way kangaroo (TAME/WILD1/WILD2), K ~1.15
#define GS_MODE

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

#define BLOCK_SIZE_NEW_GPU	256
#define BLOCK_SIZE_OLD_GPU	512
 
//use different options for cards older than RTX 40xx
#ifdef __CUDA_ARCH__
#if __CUDA_ARCH__ < 890
#define OLD_GPU
#endif
#ifdef OLD_GPU
#define BLOCK_SIZE			BLOCK_SIZE_OLD_GPU		
#define PNT_GROUP_CNT		PNT_GROUP_OLD_GPU	
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
#define NEG_FLAG			0x1000  // Negative jump: subtract instead of add

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

// Endomorphism DP flags
#define ENDO_FLAG			0x1000
#define ENDO2_FLAG			0x2000

// Gaudry-Schost equivalence class indices (stored in DPs[10] in GS mode)
// Each point (x,y) maps to one of 6 equivalent points via {±1, ±ω, ±ω²}
#define GS_CLASS_IDENTITY    0  // (x, y)
#define GS_CLASS_NEG         1  // (x, -y)
#define GS_CLASS_BETA        2  // (βx, y)
#define GS_CLASS_BETA_NEG    3  // (βx, -y)
#define GS_CLASS_BETA2       4  // (β²x, y)
#define GS_CLASS_BETA2_NEG   5  // (β²x, -y)

// beta^2 mod p (for canonicalization)
#define ENDO_BETA2_0  0x3ec693d68e6afa40ULL
#define ENDO_BETA2_1  0x630fb68aed0a766aULL
#define ENDO_BETA2_2  0x919bb86153cbcb16ULL
#define ENDO_BETA2_3  0x851695d49a83f8efULL

// lambda^2 mod n (= lambda^(-1) mod n, for GS key recovery)
#define ENDO_LAMBDA2_0 0xe4437ed6010e8828ULL
#define ENDO_LAMBDA2_1 0x7fffffffffffffffULL
#define ENDO_LAMBDA2_2 0x5363ad4cc05c30e0ULL
#define ENDO_LAMBDA2_3 0xa5261c028812645aULL

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
	bool IsGSMode;  //Gaudry-Schost mode (all walks equivalent, any collision solves)

	u32 KernelA_LDS_Size;
	u32 KernelB_LDS_Size;
	u32 KernelC_LDS_Size;
};

