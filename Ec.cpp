// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC


#include "defs.h"
#include "Ec.h"
#include <random>
#include "utils.h"

// https://en.bitcoin.it/wiki/Secp256k1
EcInt g_P; //FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE FFFFFC2F
EcPoint g_G; //Generator point

#define P_REV	0x00000001000003D1

// Group order n and lambda-related precomputed scalars mod n (initialized in InitEc)
EcInt g_N;                      // FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE BAAEDCE6 AF48A03B BFD25E8C D0364141
EcInt g_LAMBDA_N;               // endomorphism eigenvalue mod n
EcInt g_LAMBDA2_N;              // lambda^2 mod n = lambda^(-1) mod n (since lambda^3 = 1 mod n)
EcInt g_INV_1_MINUS_LAMBDA;     // (1 - lambda)^(-1) mod n   — for +sign collision candidates
EcInt g_INV_1_PLUS_LAMBDA;      // (1 + lambda)^(-1) mod n   — for -sign collision candidates
EcInt g_INV_1_MINUS_LAMBDA2;    // (1 - lambda^2)^(-1) mod n
EcInt g_INV_1_PLUS_LAMBDA2;     // (1 + lambda^2)^(-1) mod n

#ifdef DEBUG_MODE
u8* GTable = NULL; //16x16-bit table
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool parse_u8(const char* s, u8* res)
{
	char cl = toupper(s[1]);
	char ch = toupper(s[0]);
	if (((cl < '0') || (cl > '9')) && ((cl < 'A') || (cl > 'F')))
		return false;
	if (((ch < '0') || (ch > '9')) && ((ch < 'A') || (ch > 'F')))
		return false;
	u8 l = ((cl >= '0') && (cl <= '9')) ? (cl - '0') : (cl - 'A' + 10);
	u8 h = ((ch >= '0') && (ch <= '9')) ? (ch - '0') : (ch - 'A' + 10);
	*res = l + (h << 4);
	return true;
}

bool EcPoint::IsEqual(EcPoint& pnt)
{
	return this->x.IsEqual(pnt.x) && this->y.IsEqual(pnt.y);
}

void EcPoint::LoadFromBuffer64(u8* buffer)
{
	memcpy(x.data, buffer, 32);
	x.data[4] = 0;
	memcpy(y.data, buffer + 32, 32);
	y.data[4] = 0;
}

void EcPoint::SaveToBuffer64(u8* buffer)
{
	memcpy(buffer, x.data, 32);
	memcpy(buffer + 32, y.data, 32);
}

bool EcPoint::SetHexStr(const char* str)
{
	EcPoint res;
	int len = (int)strlen(str);
	if (len < 66)
		return false;
	u8 type, b;
	if (!parse_u8(str, &type))
		return false;
	if ((type < 2) || (type > 4))
		return false;
	if (((type == 2) || (type == 3)) && (len != 66))
		return false;
	if ((type == 4) && (len != 130))
		return false;

	if (len == 66) //compressed
	{
		str += 2;
		for (int i = 0; i < 32; i++)
		{
			if (!parse_u8(str + 2 * i, &b))
				return false;
			((u8*)res.x.data)[31 - i] = b;
		}
		res.y = Ec::CalcY(res.x, type == 2);
		if (!Ec::IsValidPoint(res))
			return false;		
		*this = res;
		return true;
	}
	//uncompressed
	str += 2;
	for (int i = 0; i < 32; i++)
	{
		if (!parse_u8(str + 2 * i, &b))
			return false;
		((u8*)res.x.data)[31 - i] = b;

		if (!parse_u8(str + 2 * i + 64, &b))
			return false;
		((u8*)res.y.data)[31 - i] = b;
	}
	if (!Ec::IsValidPoint(res))
		return false;
	*this = res;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// https://en.bitcoin.it/wiki/Secp256k1
void InitEc()
{
	g_P.SetHexStr("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F"); //Fp
	g_G.x.SetHexStr("79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798"); //G.x
	g_G.y.SetHexStr("483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8"); //G.y

	// secp256k1 group order n (prime)
	g_N.SetHexStr("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");

	// lambda: eigenvalue of the order-3 endomorphism phi on the group order
	// phi(P) = (beta * P.x, P.y) corresponds to scalar multiplication by lambda
	g_LAMBDA_N.SetHexStr("5363AD4CC05C30E0A5261C028812645A122E22EA20816678DF02967C1B23BD72");
	// lambda^2 mod n, computed from lambda to avoid hex-typo risk.
	// Identity: lambda^3 = 1 (mod n), so lambda^2 = lambda^(-1) (mod n).
	g_LAMBDA2_N.Assign(g_LAMBDA_N);
	g_LAMBDA2_N.MulModN(g_LAMBDA_N);

	// Precompute inverses used in GS-collision key recovery.
	// Each InvModN is a 256-iteration Fermat modpow (~65k MulModN calls, ~few ms at startup)
	EcInt one;
	one.Set(1);

	// (1 - lambda)^(-1)
	g_INV_1_MINUS_LAMBDA.Assign(one);
	g_INV_1_MINUS_LAMBDA.SubModN(g_LAMBDA_N);
	g_INV_1_MINUS_LAMBDA.InvModN();

	// (1 + lambda)^(-1)
	g_INV_1_PLUS_LAMBDA.Assign(one);
	g_INV_1_PLUS_LAMBDA.AddModN(g_LAMBDA_N);
	g_INV_1_PLUS_LAMBDA.InvModN();

	// (1 - lambda^2)^(-1)
	g_INV_1_MINUS_LAMBDA2.Assign(one);
	g_INV_1_MINUS_LAMBDA2.SubModN(g_LAMBDA2_N);
	g_INV_1_MINUS_LAMBDA2.InvModN();

	// (1 + lambda^2)^(-1)
	g_INV_1_PLUS_LAMBDA2.Assign(one);
	g_INV_1_PLUS_LAMBDA2.AddModN(g_LAMBDA2_N);
	g_INV_1_PLUS_LAMBDA2.InvModN();

	// Sanity checks for mod-n arithmetic (reports only on failure, silent on pass).
	{
		EcInt test, tmp;

		// 1. lambda * lambda^2 == 1 mod n (verifies MulModN + lambda constants)
		test.Assign(g_LAMBDA_N);
		test.MulModN(g_LAMBDA2_N);
		if (!test.IsEqual(one))
			printf("FATAL: lambda * lambda^2 != 1 mod n (MulModN bug)\n");

		// 2. (1 - lambda) * g_INV_1_MINUS_LAMBDA == 1 mod n
		tmp.Assign(one);
		tmp.SubModN(g_LAMBDA_N);
		tmp.MulModN(g_INV_1_MINUS_LAMBDA);
		if (!tmp.IsEqual(one))
		{
			printf("FATAL: (1-lambda) * g_INV_1_MINUS_LAMBDA != 1 mod n (InvModN bug on (1-lambda))\n");
			char buf[100]; tmp.GetHexStr(buf);
			printf("  got: %s\n", buf);
		}

		// 3. (1 + lambda) * g_INV_1_PLUS_LAMBDA == 1 mod n
		tmp.Assign(one);
		tmp.AddModN(g_LAMBDA_N);
		tmp.MulModN(g_INV_1_PLUS_LAMBDA);
		if (!tmp.IsEqual(one))
		{
			printf("FATAL: (1+lambda) * g_INV_1_PLUS_LAMBDA != 1 mod n\n");
			char buf[100]; tmp.GetHexStr(buf);
			printf("  got: %s\n", buf);
		}

		// 4. (1 - lambda^2) * g_INV_1_MINUS_LAMBDA2 == 1 mod n
		tmp.Assign(one);
		tmp.SubModN(g_LAMBDA2_N);
		tmp.MulModN(g_INV_1_MINUS_LAMBDA2);
		if (!tmp.IsEqual(one))
		{
			printf("FATAL: (1-lambda^2) * g_INV_1_MINUS_LAMBDA2 != 1 mod n\n");
			char buf[100]; tmp.GetHexStr(buf);
			printf("  got: %s\n", buf);
		}

		// 5. (1 + lambda^2) * g_INV_1_PLUS_LAMBDA2 == 1 mod n
		tmp.Assign(one);
		tmp.AddModN(g_LAMBDA2_N);
		tmp.MulModN(g_INV_1_PLUS_LAMBDA2);
		if (!tmp.IsEqual(one))
		{
			printf("FATAL: (1+lambda^2) * g_INV_1_PLUS_LAMBDA2 != 1 mod n\n");
			char buf[100]; tmp.GetHexStr(buf);
			printf("  got: %s\n", buf);
		}
	}
#ifdef DEBUG_MODE
	GTable = (u8*)malloc(16 * 256 * 256 * 64);
	EcPoint pnt = g_G;
	for (int i = 0; i < 16; i++)
	{
		pnt.SaveToBuffer64(GTable + (i * 256 * 256) * 64);
		EcPoint tmp = pnt;
		pnt = Ec::DoublePoint(pnt);
		for (int j = 1; j < 256 * 256 - 1; j++)
		{
			pnt.SaveToBuffer64(GTable + (i * 256 * 256 + j) * 64);
			pnt = Ec::AddPoints(pnt, tmp);
		}
	}
#endif
};

void DeInitEc()
{
#ifdef DEBUG_MODE
	if (GTable)
		free(GTable);
#endif
}

// https://en.wikipedia.org/wiki/Elliptic_curve_point_multiplication#Point_addition
EcPoint Ec::AddPoints(EcPoint& pnt1, EcPoint& pnt2)
{
	EcPoint res;
	EcInt dx, dy, lambda, lambda2;

	dx = pnt2.x;
	dx.SubModP(pnt1.x);
	dx.InvModP();

	dy = pnt2.y;
	dy.SubModP(pnt1.y);

	lambda = dy;
	lambda.MulModP(dx);
	lambda2 = lambda;
	lambda2.MulModP(lambda);

	res.x = lambda2;
	res.x.SubModP(pnt1.x);
	res.x.SubModP(pnt2.x);

	res.y = pnt2.x;
	res.y.SubModP(res.x);
	res.y.MulModP(lambda);
	res.y.SubModP(pnt2.y);
	return res;
}

// https://en.wikipedia.org/wiki/Elliptic_curve_point_multiplication#Point_doubling
EcPoint Ec::DoublePoint(EcPoint& pnt)
{
	EcPoint res;
	EcInt t1, t2, lambda, lambda2;

	t1 = pnt.y;
	t1.AddModP(pnt.y);
	t1.InvModP();

	t2 = pnt.x;
	t2.MulModP(pnt.x);
	lambda = t2;
	lambda.AddModP(t2);
	lambda.AddModP(t2);
	lambda.MulModP(t1);
	lambda2 = lambda;
	lambda2.MulModP(lambda);

	res.x = lambda2;
	res.x.SubModP(pnt.x);
	res.x.SubModP(pnt.x);

	res.y = pnt.x;
	res.y.SubModP(res.x);
	res.y.MulModP(lambda);
	res.y.SubModP(pnt.y);
	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Jacobian projective coordinate operations
// Formulas from https://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html
// secp256k1: y^2 = x^3 + 7, a = 0

void EcPointJ::SetInfinity()
{
	x.Set(1);
	y.Set(1);
	z.SetZero();
}

bool EcPointJ::IsInfinity()
{
	return z.IsZero();
}

void EcPointJ::FromAffine(EcPoint& pnt)
{
	x = pnt.x;
	y = pnt.y;
	z.Set(1);
}

EcPoint EcPointJ::ToAffine()
{
	EcPoint res;
	EcInt zinv = z;
	zinv.InvModP();        // z^(-1)
	EcInt zinv2 = zinv;
	zinv2.SqrModP();       // z^(-2)
	EcInt zinv3 = zinv2;
	zinv3.MulModP(zinv);   // z^(-3)
	res.x = x;
	res.x.MulModP(zinv2);  // X / Z^2
	res.y = y;
	res.y.MulModP(zinv3);  // Y / Z^3
	return res;
}

// Point doubling in Jacobian coordinates for a=0 curves (secp256k1)
// Cost: 1M + 5S + 1*2 + 1*3 + 1*4 + 1*8
// From EFD: dbl-2009-l
// A = X1^2, B = Y1^2, C = B^2, D = 2*((X1+B)^2 - A - C)
// E = 3*A, F = E^2, X3 = F - 2*D, Y3 = E*(D-X3) - 8*C, Z3 = 2*Y1*Z1
void Ec::DoublePointJ(EcPointJ& res, EcPointJ& pnt)
{
	EcInt A, B, C, D, E, F, t;

	A = pnt.x;
	A.SqrModP();            // A = X1^2

	B = pnt.y;
	B.SqrModP();            // B = Y1^2

	C = B;
	C.SqrModP();            // C = B^2

	// D = 2*((X1+B)^2 - A - C)
	t = pnt.x;
	t.AddModP(B);
	D = t;
	D.SqrModP();            // (X1+B)^2
	D.SubModP(A);
	D.SubModP(C);
	EcInt D2 = D;
	D.AddModP(D2);          // D = 2*((X1+B)^2 - A - C)

	// E = 3*A
	E = A;
	EcInt A2 = A;
	E.AddModP(A2);
	E.AddModP(A);            // E = 3*A

	F = E;
	F.SqrModP();             // F = E^2

	// X3 = F - 2*D
	res.x = F;
	res.x.SubModP(D);
	res.x.SubModP(D);        // X3 = F - 2*D

	// Y3 = E*(D - X3) - 8*C
	res.y = D;
	res.y.SubModP(res.x);
	res.y.MulModP(E);        // E*(D-X3)
	// 8*C = 2*4*C
	EcInt C8 = C;
	t = C;
	C8.AddModP(t);            // 2*C
	t = C8;
	C8.AddModP(t);            // 4*C
	t = C8;
	C8.AddModP(t);            // 8*C
	res.y.SubModP(C8);        // Y3 = E*(D-X3) - 8*C

	// Z3 = 2*Y1*Z1
	res.z = pnt.y;
	res.z.MulModP(pnt.z);
	t = res.z;
	res.z.AddModP(t);         // Z3 = 2*Y1*Z1
}

// Mixed addition: Jacobian + Affine -> Jacobian
// pnt2 is in affine (Z=1), which saves multiplications
// Cost: 7M + 4S
// From EFD: madd-2007-bl
// U2 = X2*Z1^2, S2 = Y2*Z1^3, H = U2-X1, HH = H^2
// I = 4*HH, J = H*I, r = 2*(S2-Y1), V = X1*I
// X3 = r^2 - J - 2*V, Y3 = r*(V-X3) - 2*Y1*J, Z3 = (Z1+H)^2 - Z1^2 - HH
void Ec::AddPointsJA(EcPointJ& res, EcPointJ& pnt1, EcPoint& pnt2)
{
	EcInt Z1sq, U2, S2, H, HH, I, J, r, V, t;

	Z1sq = pnt1.z;
	Z1sq.SqrModP();              // Z1^2

	U2 = pnt2.x;
	U2.MulModP(Z1sq);            // U2 = X2 * Z1^2

	S2 = pnt2.y;
	t = Z1sq;
	t.MulModP(pnt1.z);           // Z1^3
	S2.MulModP(t);               // S2 = Y2 * Z1^3

	H = U2;
	H.SubModP(pnt1.x);           // H = U2 - X1

	HH = H;
	HH.SqrModP();                // HH = H^2

	// I = 4*HH
	I = HH;
	t = I;
	I.AddModP(t);                 // 2*HH
	t = I;
	I.AddModP(t);                 // 4*HH

	J = H;
	J.MulModP(I);                 // J = H * I

	// r = 2*(S2 - Y1)
	r = S2;
	r.SubModP(pnt1.y);           // S2 - Y1
	t = r;
	r.AddModP(t);                 // r = 2*(S2 - Y1)

	V = pnt1.x;
	V.MulModP(I);                 // V = X1 * I

	// X3 = r^2 - J - 2*V
	res.x = r;
	res.x.SqrModP();              // r^2
	res.x.SubModP(J);
	res.x.SubModP(V);
	res.x.SubModP(V);             // X3 = r^2 - J - 2*V

	// Y3 = r*(V - X3) - 2*Y1*J
	res.y = V;
	res.y.SubModP(res.x);         // V - X3
	res.y.MulModP(r);             // r*(V - X3)
	t = pnt1.y;
	t.MulModP(J);                  // Y1*J
	res.y.SubModP(t);
	res.y.SubModP(t);              // Y3 = r*(V-X3) - 2*Y1*J

	// Z3 = (Z1+H)^2 - Z1^2 - HH
	res.z = pnt1.z;
	t = H;
	res.z.AddModP(t);              // Z1 + H
	res.z.SqrModP();               // (Z1+H)^2
	res.z.SubModP(Z1sq);
	res.z.SubModP(HH);             // Z3 = (Z1+H)^2 - Z1^2 - HH
}

// Scalar multiplication using Jacobian coordinates internally
// Only one field inversion at the end (ToAffine)
EcPoint Ec::MultiplyG_Jacobian(EcInt& k)
{
	EcPoint res;
	EcPointJ resJ;
	EcPoint t = g_G;
	bool first = true;
	int n = 3;
	while ((n >= 0) && !k.data[n])
		n--;
	if (n < 0)
		return res; //error
	int index;
	_BitScanReverse64((DWORD*)&index, k.data[n]);
	for (int i = 0; i <= 64 * n + index; i++)
	{
		u8 v = (k.data[i / 64] >> (i % 64)) & 1;
		if (v)
		{
			if (first)
			{
				first = false;
				resJ.FromAffine(t);
			}
			else
			{
				EcPointJ tmp;
				AddPointsJA(tmp, resJ, t);
				resJ = tmp;
			}
		}
		t = Ec::DoublePoint(t); // still doubles in affine for the base point table
	}
	if (first)
		return res; //error, k was 0
	return resJ.ToAffine();
}

// wNAF-5 scalar multiplication with Jacobian coordinates
// Precomputes: G, 3G, 5G, 7G, 9G, 11G, 13G, 15G (8 affine points)
// Then scans the wNAF representation from MSB to LSB:
//   - always double (in Jacobian, no inversions)
//   - on non-zero digit, add/subtract the precomputed affine point (mixed add, no inversions)
// Only ONE field inversion at the very end (ToAffine)
// Total: ~256 doublings + ~51 additions, with 0 intermediate inversions (vs ~128 inversions in original)
EcPoint Ec::MultiplyG_wNAF(EcInt& k)
{
	EcPoint res;
	const int W = 5; // window size
	const int PRECOMP = 1 << (W - 1); // 16, but we only use odd indices: 1,3,5,...,15 = 8 points

	// Precompute odd multiples of G: table[i] = (2*i+1)*G for i=0..7
	EcPoint table[PRECOMP / 2]; // 8 points
	table[0] = g_G; // 1*G
	EcPoint G2 = Ec::DoublePoint(g_G); // 2*G
	for (int i = 1; i < PRECOMP / 2; i++)
		table[i] = Ec::AddPoints(table[i - 1], G2); // (2*i+1)*G

	// Convert scalar k to wNAF representation
	// wNAF digits are in {0, +-1, +-3, +-5, ..., +-(2^(w-1)-1)}
	int wnaf[257]; // at most 257 digits (256 bits + 1 possible carry)
	int wnaf_len = 0;

	EcInt kk = k;
	while (!kk.IsZero())
	{
		if (kk.data[0] & 1) // odd
		{
			// Get the window value
			int val = (int)(kk.data[0] & ((1 << W) - 1)); // low W bits
			if (val >= (1 << (W - 1)))
				val -= (1 << W); // make it signed: if >= 16, subtract 32

			wnaf[wnaf_len++] = val;

			// Subtract val from kk
			if (val > 0)
			{
				EcInt sub;
				sub.Set((u64)val);
				kk.Sub(sub);
			}
			else
			{
				EcInt add;
				add.Set((u64)(-val));
				kk.Add(add);
			}
		}
		else
		{
			wnaf[wnaf_len++] = 0;
		}
		kk.ShiftRight(1);
	}

	// Process wNAF from MSB to LSB
	EcPointJ resJ;
	bool first = true;

	for (int i = wnaf_len - 1; i >= 0; i--)
	{
		if (!first)
		{
			EcPointJ tmp;
			DoublePointJ(tmp, resJ);
			resJ = tmp;
		}

		if (wnaf[i] != 0)
		{
			int idx = (wnaf[i] > 0) ? (wnaf[i] - 1) / 2 : (-wnaf[i] - 1) / 2;
			EcPoint tblPnt = table[idx];

			if (wnaf[i] < 0)
				tblPnt.y.NegModP(); // negate Y for subtraction (free on secp256k1)

			if (first)
			{
				first = false;
				resJ.FromAffine(tblPnt);
			}
			else
			{
				EcPointJ tmp;
				AddPointsJA(tmp, resJ, tblPnt);
				resJ = tmp;
			}
		}
	}

	if (first)
		return res; // error, k was 0
	return resJ.ToAffine(); // single inversion here
}

//k up to 256 bits
//Now uses wNAF-5 + Jacobian coordinates for ~10x fewer inversions
EcPoint Ec::MultiplyG(EcInt& k)
{
	return MultiplyG_wNAF(k);
}

#ifdef DEBUG_MODE
//uses gTable (16x16-bit) to speedup calculation
EcPoint Ec::MultiplyG_Fast(EcInt& k)
{
	int i;
	u16 b;
	EcPoint pnt, res;
	for (i = 0; i < 16; i++)
	{
		b = k.GetU16(i);
		if (b)
			break;
	}
	if (i >= 16)
		return pnt;
	if (i < 16)
	{
		res.LoadFromBuffer64(GTable + (256 * 256 * i + (b - 1)) * 64);
		i++;
	}
	while (i < 16)
	{
		b = k.GetU16(i);
		if (b)
		{
			pnt.LoadFromBuffer64(GTable + (256 * 256 * i + (b - 1)) * 64);
			res = AddPoints(res, pnt);
		}
		i++;
	}
	return res;
}
#endif

EcInt Ec::CalcY(EcInt& x, bool is_even)
{
	EcInt res;
	EcInt tmp;
	tmp.Set(7);
	res = x;
	res.SqrModP(); // x^2
	res.MulModP(x); // x^3
	res.AddModP(tmp); // x^3 + 7
	res.SqrtModP();
	if ((res.data[0] & 1) == is_even)
		res.NegModP();
	return res;
}

bool Ec::IsValidPoint(EcPoint& pnt)
{
	EcInt x, y, seven;
	seven.Set(7);
	x = pnt.x;
	x.SqrModP(); // x^2
	x.MulModP(pnt.x); // x^3
	x.AddModP(seven);
	y = pnt.y;
	y.SqrModP(); // y^2
	return x.IsEqual(y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mul256_by_64(u64* input, u64 multiplier, u64* result)
{
	u64 h1, h2;
	result[0] = _umul128(input[0], multiplier, &h1);
	u8 carry = _addcarry_u64(0, _umul128(input[1], multiplier, &h2), h1, result + 1);
	carry = _addcarry_u64(carry, _umul128(input[2], multiplier, &h1), h2, result + 2);
	carry = _addcarry_u64(carry, _umul128(input[3], multiplier, &h2), h1, result + 3);
	_addcarry_u64(carry, 0, h2, result + 4);
}

void Mul320_by_64(u64* input, u64 multiplier, u64* result)
{
	u64 h1, h2;
	result[0] = _umul128(input[0], multiplier, &h1);
	u8 carry = _addcarry_u64(0, _umul128(input[1], multiplier, &h2), h1, result + 1);
	carry = _addcarry_u64(carry, _umul128(input[2], multiplier, &h1), h2, result + 2);
	carry = _addcarry_u64(carry, _umul128(input[3], multiplier, &h2), h1, result + 3);
	_addcarry_u64(carry, _umul128(input[4], multiplier, &h1), h2, result + 4);
}

void Add320_to_256(u64* in_out, u64* val)
{
	u8 c = _addcarry_u64(0, in_out[0], val[0], in_out);
	c = _addcarry_u64(c, in_out[1], val[1], in_out + 1);
	c = _addcarry_u64(c, in_out[2], val[2], in_out + 2);
	c = _addcarry_u64(c, in_out[3], val[3], in_out + 3);
	_addcarry_u64(c, 0, val[4], in_out + 4);
}

EcInt::EcInt()
{
	SetZero();
}

void EcInt::Assign(EcInt& val)
{
	memcpy(data, val.data, sizeof(data));
}

void EcInt::Set(u64 val)
{
	SetZero();
	data[0] = val;
}

void EcInt::SetZero()
{
	memset(data, 0, sizeof(data));
}

bool EcInt::SetHexStr(const char* str)
{
	SetZero();
	int len = (int)strlen(str);
	if (len > 64)
		return false;
	char s[64];
	memset(s, '0', 64);
	memcpy(s + 64 - len, str, len);
	for (int i = 0; i < 32; i++)
	{
		int n = 62 - 2 * i;
		u8 b;
		if (!parse_u8(s + n, &b))
			return false;
		((u8*)data)[i] = b;
	}
	return true;
}

void EcInt::GetHexStr(char* str)
{
	for (int i = 0; i < 32; i++)
		sprintf(str + 2 * i, "%02X", ((u8*)data)[31 - i]);
	str[64] = 0;
}

u16 EcInt::GetU16(int index)
{
	return (u16)(data[index / 4] >> (16 * (index % 4)));
}

//returns carry
bool EcInt::Add(EcInt& val)
{
	u8 c = _addcarry_u64(0, data[0], val.data[0], data + 0);
	c = _addcarry_u64(c, data[1], val.data[1], data + 1);
	c = _addcarry_u64(c, data[2], val.data[2], data + 2);
	c = _addcarry_u64(c, data[3], val.data[3], data + 3);
	return _addcarry_u64(c, data[4], val.data[4], data + 4) != 0;
}

//returns carry
bool EcInt::Sub(EcInt& val)
{
	u8 c = _subborrow_u64(0, data[0], val.data[0], data + 0);
	c = _subborrow_u64(c, data[1], val.data[1], data + 1);
	c = _subborrow_u64(c, data[2], val.data[2], data + 2);
	c = _subborrow_u64(c, data[3], val.data[3], data + 3);
	return _subborrow_u64(c, data[4], val.data[4], data + 4) != 0;
}

void EcInt::Neg()
{
	u8 c = _subborrow_u64(0, 0, data[0], data + 0);
	c = _subborrow_u64(c, 0, data[1], data + 1);
	c = _subborrow_u64(c, 0, data[2], data + 2);
	c = _subborrow_u64(c, 0, data[3], data + 3);
	_subborrow_u64(c, 0, data[4], data + 4);
}

void EcInt::Neg256()
{
	u8 c = _subborrow_u64(0, 0, data[0], data + 0);
	c = _subborrow_u64(c, 0, data[1], data + 1);
	c = _subborrow_u64(c, 0, data[2], data + 2);
	c = _subborrow_u64(c, 0, data[3], data + 3);
	data[4] = 0;
}

bool EcInt::IsLessThanU(EcInt& val)
{
	int i = 4;
	while (i >= 0)
	{
		if (data[i] != val.data[i])
			break;
		i--;
	}
	if (i < 0)
		return false;
	return data[i] < val.data[i];
}

bool EcInt::IsLessThanI(EcInt& val)
{
	if ((data[4] >> 63) && !(val.data[4] >> 63))
		return true;
	if (!(data[4] >> 63) && (val.data[4] >> 63))
		return false;

	int i = 4;
	while (i >= 0)
	{
		if (data[i] != val.data[i])
			break;
		i--;
	}
	if (i < 0)
		return false;
	return data[i] < val.data[i];
}

bool EcInt::IsEqual(EcInt& val)
{
	return memcmp(val.data, this->data, 40) == 0;
}

bool EcInt::IsZero()
{
	return ((data[0] == 0) && (data[1] == 0) && (data[2] == 0) && (data[3] == 0) && (data[4] == 0));
}

void EcInt::AddModP(EcInt& val)
{
	Add(val);
	if (!IsLessThanU(g_P)) 
		Sub(g_P);
}

void EcInt::SubModP(EcInt& val)
{
	if (Sub(val))
		Add(g_P);
}

//assume value < P
void EcInt::NegModP()
{
	Neg();
	Add(g_P);
}

void EcInt::ShiftRight(int nbits)
{
	int offset = nbits / 64;
	if (offset)
	{
		for (int i = 0; i < 5 - offset; i++)
			data[i] = data[i + offset];
		for (int i = 5 - offset; i < 5; i++)
			data[i] = 0;
		nbits -= 64 * offset;
	}
	data[0] = __shiftright128(data[0], data[1], nbits);
	data[1] = __shiftright128(data[1], data[2], nbits);
	data[2] = __shiftright128(data[2], data[3], nbits);
	data[3] = __shiftright128(data[3], data[4], nbits);
	data[4] = ((i64)data[4]) >> nbits;
}

void EcInt::ShiftLeft(int nbits)
{
	int offset = nbits / 64;
	if (offset)
	{
		for (int i = 4; i >= offset; i--)
			data[i] = data[i - offset];
		for (int i = offset - 1; i >= 0; i--)
			data[i] = 0;
		nbits -= 64 * offset;
	}
	data[4] = __shiftleft128(data[3], data[4], nbits);
	data[3] = __shiftleft128(data[2], data[3], nbits);
	data[2] = __shiftleft128(data[1], data[2], nbits);
	data[1] = __shiftleft128(data[0], data[1], nbits);
	data[0] = data[0] << nbits;
}

void EcInt::MulModP(EcInt& val)
{	
	u64 buff[8], tmp[5], h;
	//calc 512 bits
	Mul256_by_64(val.data, data[0], buff);
	Mul256_by_64(val.data, data[1], tmp);
	Add320_to_256(buff + 1, tmp);
	Mul256_by_64(val.data, data[2], tmp);
	Add320_to_256(buff + 2, tmp);
	Mul256_by_64(val.data, data[3], tmp);
	Add320_to_256(buff + 3, tmp);
	//fast mod P
	Mul256_by_64(buff + 4, P_REV, tmp);
	u8 c = _addcarry_u64(0, buff[0], tmp[0], buff);
	c = _addcarry_u64(c, buff[1], tmp[1], buff + 1);
	c = _addcarry_u64(c, buff[2], tmp[2], buff + 2);
	tmp[4] += _addcarry_u64(c, buff[3], tmp[3], buff + 3);
	c = _addcarry_u64(0, buff[0], _umul128(tmp[4], P_REV, &h), data);
	c = _addcarry_u64(c, buff[1], h, data + 1);
	c = _addcarry_u64(c, 0, buff[2], data + 2);
	data[4] = _addcarry_u64(c, buff[3], 0, data + 3);
	while (data[4])
		Sub(g_P);
}

void EcInt::SqrModP()
{
	u64 buff[8], tmp[5], h;
	u64 cross[5]; // for cross-product accumulation

	// Compute 512-bit square using upper triangle + diagonal
	// Diagonal terms: a[0]^2, a[1]^2, a[2]^2, a[3]^2
	// Cross terms (doubled): a[0]*a[1], a[0]*a[2], a[0]*a[3], a[1]*a[2], a[1]*a[3], a[2]*a[3]

	// Start with a[0] * a[0] -> buff[0..1]
	buff[0] = _umul128(data[0], data[0], &buff[1]);
	buff[2] = buff[3] = buff[4] = buff[5] = buff[6] = buff[7] = 0;

	// Cross products: a[0] * a[1..3] (stored at buff[1..])
	Mul256_by_64(data, data[0], cross); // a[0] * {a[0],a[1],a[2],a[3]}
	// We already have a[0]*a[0] in buff[0..1], so we need a[0]*a[1..3] starting at cross[1]
	// Actually, let's use a cleaner approach: compute all cross products directly

	// a[0]*a[1]
	u64 lo, hi2;
	lo = _umul128(data[0], data[1], &hi2);
	u8 c = _addcarry_u64(0, buff[1], lo, &buff[1]);
	c = _addcarry_u64(c, buff[2], hi2, &buff[2]);
	c = _addcarry_u64(c, buff[3], 0, &buff[3]);

	// a[0]*a[2]
	lo = _umul128(data[0], data[2], &hi2);
	c = _addcarry_u64(0, buff[2], lo, &buff[2]);
	c = _addcarry_u64(c, buff[3], hi2, &buff[3]);
	c = _addcarry_u64(c, buff[4], 0, &buff[4]);

	// a[0]*a[3]
	lo = _umul128(data[0], data[3], &hi2);
	c = _addcarry_u64(0, buff[3], lo, &buff[3]);
	c = _addcarry_u64(c, buff[4], hi2, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]);

	// a[1]*a[2]
	lo = _umul128(data[1], data[2], &hi2);
	c = _addcarry_u64(0, buff[3], lo, &buff[3]);
	c = _addcarry_u64(c, buff[4], hi2, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]);

	// a[1]*a[3]
	lo = _umul128(data[1], data[3], &hi2);
	c = _addcarry_u64(0, buff[4], lo, &buff[4]);
	c = _addcarry_u64(c, buff[5], hi2, &buff[5]);
	c = _addcarry_u64(c, buff[6], 0, &buff[6]);

	// a[2]*a[3]
	lo = _umul128(data[2], data[3], &hi2);
	c = _addcarry_u64(0, buff[5], lo, &buff[5]);
	c = _addcarry_u64(c, buff[6], hi2, &buff[6]);
	c = _addcarry_u64(c, buff[7], 0, &buff[7]);

	// Double all cross products (shift left by 1)
	buff[7] = (buff[7] << 1) | (buff[6] >> 63);
	buff[6] = (buff[6] << 1) | (buff[5] >> 63);
	buff[5] = (buff[5] << 1) | (buff[4] >> 63);
	buff[4] = (buff[4] << 1) | (buff[3] >> 63);
	buff[3] = (buff[3] << 1) | (buff[2] >> 63);
	buff[2] = (buff[2] << 1) | (buff[1] >> 63);
	buff[1] = (buff[1] << 1); // buff[0] had a[0]^2, no cross term

	// Add diagonal terms: a[0]^2 already in buff[0..1] (but we shifted buff[1], fix it)
	// Recompute: clear buff and do it properly
	// Actually let's restart with a cleaner approach

	// Reset
	memset(buff, 0, sizeof(buff));

	// Diagonal terms
	u64 sq_lo, sq_hi;
	sq_lo = _umul128(data[0], data[0], &sq_hi);
	buff[0] = sq_lo; buff[1] = sq_hi;
	sq_lo = _umul128(data[1], data[1], &sq_hi);
	buff[2] = sq_lo; buff[3] = sq_hi;
	sq_lo = _umul128(data[2], data[2], &sq_hi);
	buff[4] = sq_lo; buff[5] = sq_hi;
	sq_lo = _umul128(data[3], data[3], &sq_hi);
	buff[6] = sq_lo; buff[7] = sq_hi;

	// Cross products (each appears twice, so we add them doubled)
	// a[0]*a[1] at position 1
	lo = _umul128(data[0], data[1], &hi2);
	c = _addcarry_u64(0, buff[1], lo, &buff[1]); c = _addcarry_u64(c, buff[2], hi2, &buff[2]);
	c = _addcarry_u64(c, buff[3], 0, &buff[3]); c = _addcarry_u64(c, buff[4], 0, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]); c = _addcarry_u64(c, buff[6], 0, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);
	c = _addcarry_u64(0, buff[1], lo, &buff[1]); c = _addcarry_u64(c, buff[2], hi2, &buff[2]);
	c = _addcarry_u64(c, buff[3], 0, &buff[3]); c = _addcarry_u64(c, buff[4], 0, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]); c = _addcarry_u64(c, buff[6], 0, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);

	// a[0]*a[2] at position 2
	lo = _umul128(data[0], data[2], &hi2);
	c = _addcarry_u64(0, buff[2], lo, &buff[2]); c = _addcarry_u64(c, buff[3], hi2, &buff[3]);
	c = _addcarry_u64(c, buff[4], 0, &buff[4]); c = _addcarry_u64(c, buff[5], 0, &buff[5]);
	c = _addcarry_u64(c, buff[6], 0, &buff[6]); _addcarry_u64(c, buff[7], 0, &buff[7]);
	c = _addcarry_u64(0, buff[2], lo, &buff[2]); c = _addcarry_u64(c, buff[3], hi2, &buff[3]);
	c = _addcarry_u64(c, buff[4], 0, &buff[4]); c = _addcarry_u64(c, buff[5], 0, &buff[5]);
	c = _addcarry_u64(c, buff[6], 0, &buff[6]); _addcarry_u64(c, buff[7], 0, &buff[7]);

	// a[0]*a[3] at position 3
	lo = _umul128(data[0], data[3], &hi2);
	c = _addcarry_u64(0, buff[3], lo, &buff[3]); c = _addcarry_u64(c, buff[4], hi2, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]); c = _addcarry_u64(c, buff[6], 0, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);
	c = _addcarry_u64(0, buff[3], lo, &buff[3]); c = _addcarry_u64(c, buff[4], hi2, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]); c = _addcarry_u64(c, buff[6], 0, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);

	// a[1]*a[2] at position 3
	lo = _umul128(data[1], data[2], &hi2);
	c = _addcarry_u64(0, buff[3], lo, &buff[3]); c = _addcarry_u64(c, buff[4], hi2, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]); c = _addcarry_u64(c, buff[6], 0, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);
	c = _addcarry_u64(0, buff[3], lo, &buff[3]); c = _addcarry_u64(c, buff[4], hi2, &buff[4]);
	c = _addcarry_u64(c, buff[5], 0, &buff[5]); c = _addcarry_u64(c, buff[6], 0, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);

	// a[1]*a[3] at position 4
	lo = _umul128(data[1], data[3], &hi2);
	c = _addcarry_u64(0, buff[4], lo, &buff[4]); c = _addcarry_u64(c, buff[5], hi2, &buff[5]);
	c = _addcarry_u64(c, buff[6], 0, &buff[6]); _addcarry_u64(c, buff[7], 0, &buff[7]);
	c = _addcarry_u64(0, buff[4], lo, &buff[4]); c = _addcarry_u64(c, buff[5], hi2, &buff[5]);
	c = _addcarry_u64(c, buff[6], 0, &buff[6]); _addcarry_u64(c, buff[7], 0, &buff[7]);

	// a[2]*a[3] at position 5
	lo = _umul128(data[2], data[3], &hi2);
	c = _addcarry_u64(0, buff[5], lo, &buff[5]); c = _addcarry_u64(c, buff[6], hi2, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);
	c = _addcarry_u64(0, buff[5], lo, &buff[5]); c = _addcarry_u64(c, buff[6], hi2, &buff[6]);
	_addcarry_u64(c, buff[7], 0, &buff[7]);

	// Fast mod P (same as MulModP)
	Mul256_by_64(buff + 4, P_REV, tmp);
	c = _addcarry_u64(0, buff[0], tmp[0], buff);
	c = _addcarry_u64(c, buff[1], tmp[1], buff + 1);
	c = _addcarry_u64(c, buff[2], tmp[2], buff + 2);
	tmp[4] += _addcarry_u64(c, buff[3], tmp[3], buff + 3);
	c = _addcarry_u64(0, buff[0], _umul128(tmp[4], P_REV, &h), data);
	c = _addcarry_u64(c, buff[1], h, data + 1);
	c = _addcarry_u64(c, 0, buff[2], data + 2);
	data[4] = _addcarry_u64(c, buff[3], 0, data + 3);
	while (data[4])
		Sub(g_P);
}

// -----------------------------------------------------------------------------
// Scalar-field (mod n) arithmetic — used for Gaudry-Schost collision recovery
// when a DP collision involves secp256k1's order-3 endomorphism lambda.
// These are NOT hot-loop functions (called only on DP matches), so we favor
// clarity over micro-optimization.
// -----------------------------------------------------------------------------

void EcInt::AddModN(EcInt& val)
{
	Add(val);
	if (data[4] || !IsLessThanU(g_N))
		Sub(g_N);
}

void EcInt::SubModN(EcInt& val)
{
	if (Sub(val))
		Add(g_N);
}

void EcInt::NegModN()
{
	Neg();
	Add(g_N);
	// If we started with 0, Neg gives 0, Add(g_N) gives g_N; reduce:
	if (!IsLessThanU(g_N))
		Sub(g_N);
}

// this = this * val mod n
// Implementation: double-and-add bitwise (256 iterations). Not fast, but clearly
// correct. Called only at collision-recovery time, so perf is irrelevant.
void EcInt::MulModN(EcInt& val)
{
	EcInt original;
	original.Assign(*this);

	SetZero();

	for (int i = 255; i >= 0; i--)
	{
		// result = 2 * result mod n
		ShiftLeft(1);
		if (data[4] || !IsLessThanU(g_N))
			Sub(g_N);

		// if bit i of val is set: result = (result + original) mod n
		if ((val.data[i >> 6] >> (i & 63)) & 1)
		{
			Add(original);
			if (data[4] || !IsLessThanU(g_N))
				Sub(g_N);
		}
	}
}

// this = this^(-1) mod n via Fermat's little theorem: x^(n-2) mod n.
// n is prime for secp256k1. Only called at init time for a small number of
// constants; use binary modpow.
void EcInt::InvModN()
{
	// compute n - 2
	EcInt n_minus_2;
	n_minus_2.Assign(g_N);
	EcInt two;
	two.Set(2);
	n_minus_2.Sub(two);

	EcInt result;
	result.Set(1);

	EcInt base;
	base.Assign(*this);

	// walk bits of (n-2) from bit 0 upward, squaring base each step
	for (int i = 0; i < 256; i++)
	{
		if ((n_minus_2.data[i >> 6] >> (i & 63)) & 1)
		{
			result.MulModN(base);
		}
		// base = base^2 mod n
		EcInt base_copy;
		base_copy.Assign(base);
		base.MulModN(base_copy);
	}

	Assign(result);
}

void EcInt::Mul_u64(EcInt& val, u64 multiplier)
{
	Assign(val);
	Mul320_by_64(data, (u64)multiplier, data);
}

void EcInt::Mul_i64(EcInt& val, i64 multiplier)
{
	Assign(val);
	if (multiplier < 0)
	{
		Neg();
		multiplier = -multiplier;
	}
	Mul320_by_64(data, (u64)multiplier, data);
}

#define APPLY_DIV_SHIFT() kbnt -= index; val >>= index; matrix[0] <<= index; matrix[1] <<= index; 
	
// https://tches.iacr.org/index.php/TCHES/article/download/8298/7648/4494
//a bit tricky
void DIV_62(i64& kbnt, i64 modp, i64 val, i64* matrix)
{
	int index, cnt;
	_BitScanForward64((DWORD*)&index, val | 0x4000000000000000);
	APPLY_DIV_SHIFT();
	cnt = 62 - index;
	while (cnt > 0)
	{
		if (kbnt < 0)
		{
			kbnt = -kbnt;
			i64 tmp = -modp; modp = val; val = tmp;
			tmp = -matrix[0]; matrix[0] = matrix[2]; matrix[2] = tmp;
			tmp = -matrix[1]; matrix[1] = matrix[3]; matrix[3] = tmp;
		}
		int thr = cnt;
		if ((kbnt + 1) < cnt)
			thr = (int)(kbnt + 1);
		i64 mul = (-modp * val) & ((UINT64_MAX >> (64 - thr)) & 0x07);
		val += (modp * mul);
		matrix[2] += (matrix[0] * mul);
		matrix[3] += (matrix[1] * mul);
		_BitScanForward64((DWORD*)&index, val | (1ull << cnt));
		APPLY_DIV_SHIFT();
		cnt -= index;
	}
}

void EcInt::InvModP()
{
	i64 matrix[4];
	EcInt result, a, tmp, tmp2;
	EcInt modp, val;
	i64 kbnt = -1;
	matrix[1] = matrix[2] = 0;
	matrix[0] = matrix[3] = 1;	
	DIV_62(kbnt, g_P.data[0], data[0], matrix);
	modp.Mul_i64(g_P, matrix[0]);
	tmp.Mul_i64(*this, matrix[1]);
	modp.Add(tmp);
	modp.ShiftRight(62);
	val.Mul_i64(g_P, matrix[2]);
	tmp.Mul_i64(*this, matrix[3]);
	val.Add(tmp);
	val.ShiftRight(62);
	if (matrix[1] >= 0)
		result.Set(matrix[1]);
	else
	{
		result.Set(-matrix[1]);
		result.Neg();
	}
	if (matrix[3] >= 0)
		a.Set(matrix[3]);
	else
	{ 
		a.Set(-matrix[3]);
		a.Neg();
	}
	Mul320_by_64(g_P.data, (result.data[0] * 0xD838091DD2253531) & 0x3FFFFFFFFFFFFFFF, tmp.data);
	result.Add(tmp);
	result.ShiftRight(62);
	Mul320_by_64(g_P.data, (a.data[0] * 0xD838091DD2253531) & 0x3FFFFFFFFFFFFFFF, tmp.data);
	a.Add(tmp);
	a.ShiftRight(62);
	
	while (val.data[0] || val.data[1] || val.data[2] || val.data[3])
	{
		matrix[1] = matrix[2] = 0;
		matrix[0] = matrix[3] = 1;	
		DIV_62(kbnt, modp.data[0], val.data[0], matrix);
		tmp.Mul_i64(modp, matrix[0]);
		tmp2.Mul_i64(val, matrix[1]);
		tmp.Add(tmp2);
		tmp2.Mul_i64(val, matrix[3]);
		val.Mul_i64(modp, matrix[2]);
		val.Add(tmp2);
		val.ShiftRight(62);
		modp = tmp;
		modp.ShiftRight(62);
		tmp.Mul_i64(result, matrix[0]);
		tmp2.Mul_i64(a, matrix[1]);
		tmp.Add(tmp2);
		tmp2.Mul_i64(a, matrix[3]);
		a.Mul_i64(result, matrix[2]);
		a.Add(tmp2);
		Mul320_by_64(g_P.data, (a.data[0] * 0xD838091DD2253531) & 0x3FFFFFFFFFFFFFFF, tmp2.data);
		a.Add(tmp2);
		a.ShiftRight(62);	
		Mul320_by_64(g_P.data, (tmp.data[0] * 0xD838091DD2253531) & 0x3FFFFFFFFFFFFFFF, tmp2.data);
		result = tmp;
		result.Add(tmp2);
		result.ShiftRight(62);
	}
	Assign(result);
	if (modp.data[4] >> 63)
	{
		Neg();
		modp.Neg();	
	}

	if (modp.data[0] == 1) 
	{
		if (data[4] >> 63)
			Add(g_P);
		if (data[4] >> 63)
			Add(g_P);
		if (!IsLessThanU(g_P))
			Sub(g_P);
		if (!IsLessThanU(g_P))
			Sub(g_P);
	}
	else
		SetZero(); //error
}

// x = a^((p + 1) / 4) mod p
// Optimized addition chain for secp256k1: only 13 multiplications + 254 squarings
// Based on the structure of (P+1)/4 which has a run of 223 consecutive 1-bits
// Derived from bitcoin-core/secp256k1 field exponentiation technique
void EcInt::SqrtModP()
{
	EcInt x1, x2, x3, x6, x9, x11, x22, x44, x88, x176, x220, x223, t;

	// x1 = input (a)
	x1 = *this;

	// x2 = a^(2^2 - 1) = a^3
	x2 = x1;
	x2.SqrModP();       // a^2
	x2.MulModP(x1);     // a^3

	// x3 = a^(2^3 - 1) = a^7
	x3 = x2;
	x3.SqrModP();       // a^6
	x3.MulModP(x1);     // a^7

	// x6 = a^(2^6 - 1) = a^63
	x6 = x3;
	for (int i = 0; i < 3; i++) x6.SqrModP();  // a^(7 * 2^3) = a^56
	x6.MulModP(x3);     // a^63

	// x9 = a^(2^9 - 1) = a^511
	x9 = x6;
	for (int i = 0; i < 3; i++) x9.SqrModP();  // a^(63 * 2^3) = a^504
	x9.MulModP(x3);     // a^511

	// x11 = a^(2^11 - 1) = a^2047
	x11 = x9;
	for (int i = 0; i < 2; i++) x11.SqrModP(); // a^(511 * 2^2) = a^2044
	x11.MulModP(x2);    // a^2047

	// x22 = a^(2^22 - 1)
	x22 = x11;
	for (int i = 0; i < 11; i++) x22.SqrModP(); // a^(2047 * 2^11)
	x22.MulModP(x11);   // a^(2^22 - 1)

	// x44 = a^(2^44 - 1)
	x44 = x22;
	for (int i = 0; i < 22; i++) x44.SqrModP();
	x44.MulModP(x22);

	// x88 = a^(2^88 - 1)
	x88 = x44;
	for (int i = 0; i < 44; i++) x88.SqrModP();
	x88.MulModP(x44);

	// x176 = a^(2^176 - 1)
	x176 = x88;
	for (int i = 0; i < 88; i++) x176.SqrModP();
	x176.MulModP(x88);

	// x220 = a^(2^220 - 1)
	x220 = x176;
	for (int i = 0; i < 44; i++) x220.SqrModP();
	x220.MulModP(x44);

	// x223 = a^(2^223 - 1)
	x223 = x220;
	for (int i = 0; i < 3; i++) x223.SqrModP();
	x223.MulModP(x3);

	// The exponent (P+1)/4 for secp256k1:
	// = 2^254 - 2^30 - 2^7 - 2^6 - 2^5 - 2^4 - 2^2
	// = (2^223 - 1) * 2^31 + 2^30 + ... (specific bit pattern)
	// Assemble: t = x223 * 2^23 * x22 * 2^6 * x2 * 2^2
	t = x223;
	for (int i = 0; i < 23; i++) t.SqrModP(); // x223 << 23
	t.MulModP(x22);    // + x22
	for (int i = 0; i < 6; i++) t.SqrModP();  // << 6
	t.MulModP(x2);     // + x2
	t.SqrModP();        // << 1
	t.SqrModP();        // << 1 (total << 2)

	*this = t;
}

std::mt19937_64 rng;
CriticalSection cs_rnd;

void SetRndSeed(u64 seed)
{
	rng.seed(seed);
}

void EcInt::RndBits(int nbits)
{
	SetZero();
	if (nbits > 256)
		nbits = 256;
	cs_rnd.Enter();
	for (int i = 0; i < (nbits + 63) / 64; i++)
		data[i] = rng();
	cs_rnd.Leave();
	data[nbits / 64] &= (1ull << (nbits % 64)) - 1;
}

//up to 256 bits only
void EcInt::RndMax(EcInt& max)
{
	SetZero();
	int n = 3;
	while ((n >= 0) && !max.data[n])
		n--;
	if (n < 0)
		return;
	u64 val = max.data[n];
	int k = 0;
	while ((val & 0x8000000000000000) == 0)
	{
		val <<= 1;
		k++;
	}
	int bits = 64 * n + (64 - k);
	RndBits(bits);
	while (!IsLessThanU(max)) // :)
		RndBits(bits);
}





