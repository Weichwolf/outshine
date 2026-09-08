Type: bug
State: open
Area: include, metrics
Parent: 2188
Depends:

# Quantiles will define invalid rank inputs before integer conversion

QuantileOf in include/math/Quantile.h clamps finite out-of-range shares but NaN
passes both comparisons and reaches static_cast<size_t>(at). An unrepresentable
floating-to-integer conversion has undefined behavior:
https://eel.is/c++draft/conv.fpint

Specify and implement invalid-share handling without returning a plausible but false
percentile. Audit all callers before choosing an error-bearing return or explicit
NaN propagation. Document sortedness, non-finite samples, empty samples and clamping.
Move test-only Held::kFive/kOneStep out of the public namespace into the test suite.

- [ ] NaN share cannot reach integer conversion; negative and >1 behavior explicit.
- [ ] Existing valid nearest-rank results and performance preserved.
- [ ] Invalid input, empty sample and rank boundaries independently tested.
- [ ] Sanitized negative control detects the former invalid conversion.
