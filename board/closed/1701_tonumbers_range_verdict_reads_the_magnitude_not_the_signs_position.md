Type: bug
Area: core
Regresses: 1695

**ToNumber's out-of-range verdict reads the number's magnitude, not the written exponent's sign**

src/core/Script.cpp:785-790: when `from_chars` reports `result_out_of_range`, the code decides
zero-vs-infinity by whether the TEXT contains `e-`. That heuristic is wrong in both directions:

- `".` + 350 zeros + `1"` — tininess spelled in fraction digits, no exponent — underflows;
  `tiny` is false and the code answers `+Infinity`. ECMA rounds the exact value to **0**.
- `"1` + 400 zeros + `e-2"` — a mantissa of 1e400 with `e-2` — is 1e398, an OVERFLOW;
  `tiny` is true and the code answers **0**. ECMA says **Infinity**.

The comment claims "the text's own exponent says which", but the text's exponent alone never
decides tininess — the EFFECTIVE decimal exponent does (position of the first significant
digit relative to the point, plus the written exponent). The proof added at closure
(AScriptRunsWhatTheHostGivesItAndNothingElse.cpp: `+"1e-999"`) only exercises the case where
the heuristic happens to agree.

Second gap, same function: `"+0x10"` answers 16 (Script.cpp:760-780 strips the sign, the hex
branch only refuses `sign < 0`). ECMA's StrNumericLiteral puts the sign on StrDecimalLiteral
alone — a signed hex literal, either sign, is **NaN**.

Demanded: the range verdict derives the effective decimal exponent from the digits (first
significant digit index vs. the point, plus the parsed exponent) and answers 0 below the
denormal floor, Infinity above the overflow ceiling; any leading sign before `0x` is NaN.
The proof gains the three discriminating arms above.

---

Closed -- the verdict now reads the FIRST SIGNIFICANT DIGIT's decimal exponent (leading-zero
position in whole/fraction plus the written exponent, sum-guarded), so fraction-spelled
tininess underflows to signed zero and a mantissa-borne overflow is Infinity despite its e-;
a signed hex literal is NaN on both signs per NonDecimalIntegerLiteral. Proven in
AScriptRunsWhatTheHostGivesItAndNothingElse (".<350 zeros>1" == 0, "1<400 zeros>e-2" ==
Infinity, "-.<350 zeros>1" == -0 by signbit, "+0x10"/"-0x10" NaN); negative control: the old
heuristic reverted goes 1 FAIL on exactly this test.
