Type: bug
Area: core
Tags: script, boundary

**String coercion speaks ECMA's grammar, not from_chars' grammar**

1694 replaced `strtod` with `from_chars` in `TextToNumber` (`src/core/Script.cpp:727-756`)
and the locale is gone — but the ACCEPTED LANGUAGE is now from_chars' pattern, which is wider
and narrower than ECMA's in ways that produce wrong VALUES, all confirmed by probe on this
toolchain:

- **`"--5"` coerces to `+5`.** :741 strips one sign, then :752 `from_chars` accepts a SECOND
  minus as part of its own pattern; `sign * value` flips it back positive. ECMA: NaN.
  Same for `"+-5"` → `-5`.
- **`"1e-999"` coerces to `+Infinity`.** :754 maps `result_out_of_range` to `HUGE_VAL`, but
  from_chars flags UNDERFLOW with the same errc (probe: v=0, ec=34). ECMA: 0. An underflow
  answered as Infinity is the widest possible miss.
- **`"inf"`, `"infinity"`, `"iNfInItY"` coerce to Infinity.** from_chars accepts inf/infinity
  case-insensitively and :753 only checks whole consumption. ECMA accepts exactly `Infinity`.
- **`"0x1p4"` coerces to 16, `"0x1.8"` to 1.5.** :746 hands the hex remainder to
  `chars_format::hex`, whose grammar includes the binary exponent and the hex point. ECMA's
  HexIntegerLiteral is digits only: both are NaN.
- **`"+0x10"` coerces to 16.** :747 refuses only the negative sign; ECMA applies the sign
  grammar to decimal literals only, so signed hex is NaN either way.
- **A `>DBL_MAX` hex string coerces to NaN** (:748 requires `ec == errc()`); ECMA: Infinity.

Second site, same theme: the tokeniser's overflow branch (:158-162) discards the from_chars
result with `(void)` — on THIS libc++ the out-of-range write happens to land ±inf (probe:
260 f's → inf, ec=34), but the standard leaves `value` unmodified on `result_out_of_range`,
so on libstdc++ the token would be the silent 0.0 that 1690 was closed for. The ec must be
read and the overflow answered as Infinity explicitly.

Demanded: `TextToNumber` implements ECMA's StrNumericLiteral — reject a second sign, treat
underflow-range as the value from_chars wrote (0), accept only the exact spelling `Infinity`,
scan the hex run with the digits-only grammar and answer overflow with Infinity; the
tokeniser's overflow branch reads its ec. Each case above lands in
`AScriptRunsWhatTheHostGivesItAndNothingElse` beside the 1694 checks.

---

Closed: TextToNumber speaks ECMA's own StrDecimalLiteral -- an explicit shape check (one
sign, digits/point/exponent, nothing else) runs BEFORE from_chars, so "--5" is NaN and never
plus five, "inf" and hex floats are NaN; underflow and overflow are told apart by the TEXT'S
own exponent sign (1e-999 is zero, 1e999 is infinity -- no errno convention consulted); the
tokenizer's giant-hex path writes infinity explicitly on out_of_range instead of trusting a
library to. Proofs ride the unary plus; corpus 825/825.
