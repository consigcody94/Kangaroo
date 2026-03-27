// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC/Kang-2


#pragma once

#include "defs.h"
#include "utils.h"

class EcInt
{
public:
	EcInt();

	void Assign(EcInt& val);
	void Set(u64 val);
	void SetZero();
	bool SetHexStr(const char* str);
	void GetHexStr(char* str);
	u16 GetU16(int index);

	bool Add(EcInt& val); //returns true if carry
	bool Sub(EcInt& val); //returns true if carry
	void Neg();
	void Neg256();
	void ShiftRight(int nbits);
	void ShiftLeft(int nbits);
	bool IsLessThanU(EcInt& val);
	bool IsLessThanI(EcInt& val);
	bool IsEqual(EcInt& val);
	bool IsZero();

	void Mul_u64(EcInt& val, u64 multiplier);
	void Mul_i64(EcInt& val, i64 multiplier);

	void AddModP(EcInt& val);
	void SubModP(EcInt& val);
	void NegModP();
	void MulModP(EcInt& val);
	void SqrModP();
	void InvModP();
	void SqrtModP();

	void RndBits(int nbits);
	void RndMax(EcInt& max);

	u64 data[4 + 1];
};

class EcPoint
{
public:
	bool IsEqual(EcPoint& pnt);
	void LoadFromBuffer64(u8* buffer);
	void SaveToBuffer64(u8* buffer);
	bool SetHexStr(const char* str);
	EcInt x;
	EcInt y;
};

// Jacobian projective point: represents affine (X/Z^2, Y/Z^3)
class EcPointJ
{
public:
	EcInt x, y, z;
	void SetInfinity();
	bool IsInfinity();
	EcPoint ToAffine(); // Convert back to affine (requires one inversion)
	void FromAffine(EcPoint& pnt); // Convert affine to Jacobian (Z=1)
};

class Ec
{
public:
	static EcPoint AddPoints(EcPoint& pnt1, EcPoint& pnt2);
	static EcPoint DoublePoint(EcPoint& pnt);
	static EcPoint MultiplyG(EcInt& k);

	// Jacobian coordinate operations (no field inversions needed)
	// secp256k1 a=0 specialized formulas from https://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html
	static void DoublePointJ(EcPointJ& res, EcPointJ& pnt);          // 1M + 5S
	static void AddPointsJA(EcPointJ& res, EcPointJ& pnt1, EcPoint& pnt2); // 7M + 4S (mixed Jacobian + Affine)
	static EcPoint MultiplyG_Jacobian(EcInt& k); // Uses Jacobian internally, returns affine
	static EcPoint MultiplyG_wNAF(EcInt& k);     // wNAF-5 + Jacobian (fastest)

#ifdef DEBUG_MODE
	static EcPoint MultiplyG_Fast(EcInt& k);
#endif
	static EcInt CalcY(EcInt& x, bool is_even);
	static bool IsValidPoint(EcPoint& pnt);
};

void InitEc();
void DeInitEc();
void SetRndSeed(u64 seed);