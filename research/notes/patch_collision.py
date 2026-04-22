#!/usr/bin/env python3
"""Replace the GS_MODE collision block in RCKangaroo.cpp with class-aware recovery."""
import re, sys

path = r"C:\Users\Ajs\Desktop\Kangaroo\RCKangaroo.cpp"
with open(path, "r", encoding="utf-8") as f:
    src = f.read()

# Old block: the 4-candidate naive attempt, starting from "// Try: key = HalfRange + (d1 - d2)"
# through the matching "}" closing the outer anonymous block and before "#else".
old_start = "\t\t\t// Try: key = HalfRange + (d1 - d2) and key = HalfRange + (d2 - d1)"
old_end   = "\t\t\t\t// None matched \u2014 collision between equiv class transforms we haven't handled\n\t\t\t\t// This is expected for some collisions \u2014 not an error, just try next\n\t\t\t}"

i = src.find(old_start)
j = src.find(old_end)
if i < 0 or j < 0:
    print("anchor not found. start=", i, "end=", j)
    sys.exit(1)
j_end = j + len(old_end)

new_block = r"""			// Class-aware collision recovery (secp256k1 order-6 automorphism).
			// Two walkers at same canonical x satisfy
			//   P1 = sigma * lambda^e * P2  (as curve points)
			// where sigma in {+1,-1} from y-parity match/mismatch,
			//       e = (x_class_2 - x_class_1) mod 3.
			// Each walker had P_i = PntA + d_i*G = (key - HalfRange + d_i)*G.
			// Algebraic solve:
			//   K = (s * lambda^e * d2 - d1) * (1 - s*lambda^e)^{-1}  (mod n),   key = K + HalfRange
			// where s = +1 (y-parities equal) or -1 (differ).
			//
			// class_idx encoding (stored in type slot after GPU GS_MODE change):
			//   bit 0    = y-parity (0 or 1)
			//   bits 1-2 = x-class selector: 0=identity, 1=beta, 2=beta^2  (after >>1 of raw encoding)
			{
				u32 c1 = (u32)(pref->type) & 0x7;
				u32 c2 = (u32)(nrec.type) & 0x7;
				u32 x_class_1 = (c1 >> 1) & 0x3;
				u32 x_class_2 = (c2 >> 1) & 0x3;
				u32 y_par_1   = c1 & 1;
				u32 y_par_2   = c2 & 1;
				bool sigma_neg_detected = (y_par_1 != y_par_2);
				u32 e_detected = (x_class_2 + 3 - x_class_1) % 3;

				auto try_cand = [&](EcInt cand) -> bool {
					EcPoint chk = ec.MultiplyG(cand);
					if (chk.IsEqual(gPntToSolve)) {
						gPrivKey = cand;
						gSolved = true;
						return true;
					}
					return false;
				};

				// Lift signed 22-byte distance into [0, n)
				auto to_modn = [](EcInt& d) {
					if ((d.data[4] >> 63) & 1) d.Add(g_N);
					if (d.data[4] || !d.IsLessThanU(g_N)) d.Sub(g_N);
				};

				EcInt D1 = d1, D2 = d2;
				to_modn(D1);
				to_modn(D2);

				bool solved_local = false;

				// Enumerate candidates over both (DA=D1,DB=D2) and swapped orderings,
				// because which walker is "1" vs "2" in the collision isn't known from class_idx alone.
				// For robustness, ALSO enumerate all 6 (e, sigma) pairs rather than just the detected one,
				// so any corrupt/mismatched class_idx bits don't silently kill a real collision.
				for (int pass = 0; pass < 2 && !solved_local; pass++) {
					EcInt DA = (pass == 0) ? D1 : D2;
					EcInt DB = (pass == 0) ? D2 : D1;

					// e=0 sigma=-1 : key = HalfRange - (DA+DB)/2 mod n  (no lambda needed)
					{
						EcInt sum = DA;
						sum.AddModN(DB);
						if ((sum.data[0] & 1) == 0) sum.ShiftRight(1);
						else { sum.Add(g_N); sum.ShiftRight(1); }
						EcInt cand = Int_HalfRange;
						cand.SubModN(sum);
						if (try_cand(cand)) { solved_local = true; break; }
					}

					// e in {1,2}, sigma in {+1,-1}
					for (int ei = 1; ei <= 2 && !solved_local; ei++) {
						EcInt lam_e = (ei == 1) ? g_LAMBDA_N : g_LAMBDA2_N;
						for (int si = 0; si < 2 && !solved_local; si++) {
							bool sigma_neg = (si == 1);

							// s_lam_DB = s * lambda^e * DB mod n
							EcInt s_lam_DB = DB;
							s_lam_DB.MulModN(lam_e);
							if (sigma_neg) s_lam_DB.NegModN();

							// num = s_lam_DB - DA mod n
							EcInt num = s_lam_DB;
							num.SubModN(DA);

							// inv_factor = (1 - s*lambda^e)^{-1} mod n (precomputed at init)
							EcInt inv_factor;
							if (sigma_neg)
								inv_factor.Assign((ei == 1) ? g_INV_1_PLUS_LAMBDA  : g_INV_1_PLUS_LAMBDA2);
							else
								inv_factor.Assign((ei == 1) ? g_INV_1_MINUS_LAMBDA : g_INV_1_MINUS_LAMBDA2);

							num.MulModN(inv_factor);
							EcInt cand = Int_HalfRange;
							cand.AddModN(num);
							if (try_cand(cand)) { solved_local = true; break; }
						}
					}
				}

				if (solved_local) break;

				// Legacy cheap fallbacks (raw diff without modN lifting) — may catch a few edge cases
				EcInt diff = d1;
				diff.Sub(d2);
				bool neg = (diff.data[4] >> 63) != 0;
				if (neg) diff.Neg();
				EcInt cand = Int_HalfRange; cand.Add(diff);
				if (try_cand(cand)) break;
				cand = Int_HalfRange; cand.Sub(diff);
				if (try_cand(cand)) break;
				// Legacy (d1+d2)/2 already covered by modN branch; not repeating here.
				// Unhandled class-mismatch: silently continue (rare — caught by other collisions).
			}"""

# Replace tabs consistently — use \t since file uses tabs
new_block = new_block.replace("    ", "\t") if False else new_block  # (already tab-indented)
# But we wrote the block with tab-indent via leading tabs — check
# Actually our raw string has leading tabs/whitespace mixed. Normalize:
# Each line: leading whitespace, then rest.
lines = []
for ln in new_block.split("\n"):
    # strip leading whitespace and re-add using 3-tab prefix or deeper based on curly depth
    # Simpler: assume the block text was authored with mixed tab/space by mistake;
    # just keep the raw text as-is (raw string preserves exactly what we typed).
    lines.append(ln)
new_block = "\n".join(lines)

patched = src[:i] + new_block + src[j_end:]
with open(path, "w", encoding="utf-8") as f:
    f.write(patched)

print(f"Replaced {j_end - i} bytes with {len(new_block)} bytes at offset {i}")
