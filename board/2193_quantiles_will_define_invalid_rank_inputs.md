Type: bug
State: active
Area: include, metrics
Parent: 2188
Depends:

# Quantiles will define invalid rank inputs before integer conversion

QuantileOf in include/math/Quantile.h clamps finite out-of-range shares but NaN
passes both comparisons and reaches static_cast<size_t>(at). An unrepresentable
floating-to-integer conversion has undefined behavior:
https://eel.is/c++draft/conv.fpint

Return [[nodiscard]] expected<double, QuantileError> noexcept. Reject non-finite
shares and empty samples explicitly; retain finite endpoint clamping. Input sample
must be sorted ascending without NaNs; infinities are legitimate ordered endpoints.
Selection stays O(1), allocation-free and constexpr; sorting/validation remain the
caller's responsibility. Document this precondition, units and borrowed lifetime.
Production callers are Laying.cpp metrics and PlaceCamera.cpp timings. Publish only
successful quantiles; a failed timed frame must not become a zero-ms measurement.
Move test-only Held::kFive/kOneStep out of the public namespace into the test suite.

- [ ] NaN share cannot reach integer conversion; negative and >1 behavior explicit.
- [ ] Existing valid nearest-rank results and performance preserved.
- [ ] Invalid input, empty sample and rank boundaries independently tested.
- [ ] Sanitized negative control detects the former invalid conversion.
- [ ] Header compiles with -fno-exceptions; ignored result fails compilation.
