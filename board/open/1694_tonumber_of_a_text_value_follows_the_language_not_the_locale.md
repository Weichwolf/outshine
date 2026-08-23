Type: bug
Area: core
Tags: script, evaluator, locale

**ToNumber of a text value follows the language, never the locale**

The 1621 repayment moved the TOKENISER to from_chars, but the EVALUATOR still coerces text
through the C locale: `src/core/Script.cpp:821` — unary `+` on a text value calls
`std::strtod(inner.Text.c_str(), nullptr)` at runtime, on the script tick path. Three
divergences from ECMA-262 ToNumber, one of them locale-shaped:

- comma-decimal locale reads `+"1.5"` as `1` — the same hole the ui repayment just called
  "a correctness hole, not a form nit" in Style.cpp;
- the null endptr swallows trailing junk: `+"1.5px"` answers `1.5`, the language says NaN;
- `+"Infinity"` answers `0` (strtod cannot read the word), the language says Infinity.

The test262 run did not catch it, so the prepared set carries no string-coercion case
through unary plus — the corpus gap is part of the defect.

Demanded: one ToNumber routine for text (trim the language's whitespace, empty → 0, `0x`
via from_chars base 16, `Infinity`/`-Infinity` by name, otherwise from_chars over the WHOLE
remainder with trailing junk → NaN), used at :821 and anywhere a text value meets
arithmetic; unit cases for the four spellings above beside the evaluator they prove.
