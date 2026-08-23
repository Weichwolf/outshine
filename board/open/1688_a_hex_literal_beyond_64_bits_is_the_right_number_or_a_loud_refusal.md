Type: bug
Area: core
Tags: script, tokeniser

**A hex literal beyond 64 bits is the right number or a loud refusal, never a silent zero**

The from_chars move (board:1621 repayment, commit b0a89e9e) left both number scans in
`src/core/Script.cpp` without an `ec` check:

- Hex path (:141-152): `std::from_chars(…, wide, 16)` on a literal past `uint64_t` returns
  `result_out_of_range` with `ptr` advanced past all digits and `wide` UNMODIFIED (proven on
  this toolchain: value stays at its prior content). The code tests only `hex.ptr != begin`,
  takes the branch, and tokenises `0x10000000000000000` as `0` — JS says `2^64`. A silent
  wrong number, no error.
- Decimal path (:153-156): `1e999` works ONLY because libc++ stores ±inf on
  `result_out_of_range`; the standard's letter says value unmodified. One target toolchain
  makes this tolerable, but the reliance is undeclared and untested.

The corpus does not hold the line here: no test262 case in the prepared set contains `1e999`,
and the only ≥17-digit hex literals sit in BigInt cases (`0x…n`), so the plain-hex overflow
path is uncovered — and `test/unit/core/AScriptRunsWhatTheHostGivesItAndNothingElse.cpp`
carries not one hex or overflow literal (grep `0x|1e9`: empty). The behaviour the commit
changed has no test that would catch it drifting back.

Demanded: check `ec` on both scans — hex out-of-range converts through the JS rule (the
value is a double; parse into a double by positional accumulation or refuse loudly at Read),
decimal out-of-range pins the ±inf semantics explicitly; unit cases for `0xFF`, `0X10`,
`0x10000000000000000`, `1e999`, `.5` beside the tokeniser they prove.
