Type: bug
Area: core
Tags: script, tokeniser
Supersedes: 1688

**A hex literal past 64 bits reads as the correctly rounded double, never a stepwise-rounded neighbour**

The 1688 repayment (commit d80b8659) accumulates overflowing hex digit-by-digit in a double
(`src/core/Script.cpp:150-163`: `wide2 = wide2 * 16.0 + value`). Each step past 2^53 rounds,
and the roundings compound: measured against the correctly rounded conversion (strtod of
`0x…p0`), 20396 of 200000 random 68–120-bit literals differ — 10.2%, typically 1 ULP.
Concrete: `0x88bc9f5e154b14ba1a36` accumulates to `6.4572131319564112e23`; the language's
value is `6.4572131319564126e23`. ECMA-262 (NumericLiteral MV → Number value) demands
round-to-nearest of the EXACT mathematical value, not per-digit rounding. The corpus did not
hold the line: test262's ≥17-digit hex sits only in BigInt cases, and the unit case (2^64)
is a power of two that any scheme rounds identically — so the drift is invisible to both.
1688's own demand suggested "positional accumulation"; the demand was wrong.

Demanded: replace the manual loop with one
`std::from_chars(first, last, value, std::chars_format::hex)` over the digit run (verified
on this toolchain: consumes the bare significand, correctly rounded, equal to strtod's
`p0` answer) and delete the accumulation; a unit case with a literal the stepwise form gets
wrong (`0x88bc9f5e154b14ba1a36 === 645721313195641255821312.0` in the corrected reading)
beside the tokeniser it proves.
