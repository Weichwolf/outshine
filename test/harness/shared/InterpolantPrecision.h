/* HOW EXACTLY THIS DEVICE CARRIES A PER-VERTEX QUANTITY ACROSS A TRIANGLE (board:1195).
 *
 * NOTHING PUBLISHES IT. Metal documents no interpolation precision; Vulkan exposes
 * `subPixelPrecisionBits` and D3D11 mandates 8 fractional bits, and neither is a statement about this
 * device. So both device quantities below are measured here, by
 * `test/render/outshine/shader/TheInterpolatorCarriesAQuantityAcrossATriangle.cpp`, and that test is what keeps
 * these lines from going stale.
 *
 * IT IS NOT ONE NUMBER, AND THAT IS THE FINDING. The deviation between a hardware-interpolated quantity
 * and the exact one is TWO mechanisms, and only the smaller of them is a property of the arithmetic:
 *
 *   1. the rasteriser snaps each projected vertex to a fixed-point SUBPIXEL GRID, so it interpolates
 *      across a triangle a fraction of a pixel away from the one the file declares -- and the oracle
 *      ray-traces the declared one. This term scales as `grid / triangle height`, so it is a property
 *      of the GEOMETRY as much as of the device and it is UNBOUNDED as a triangle grows thin.
 *   2. the interpolator then evaluates in f32, which is a few ulps and is bounded by nothing else.
 *
 * SO A BARE SCALAR WOULD BE THE WRONG SHAPE, and an earlier draft of this header was one. A scalar
 * `kInterpolantAbsoluteError` is only true beside a smallest triangle height and a widest `w` ratio,
 * which is exactly the "domain too narrow" face of the failure `CLAUDE.md` names. The term is a
 * FUNCTION of the case's own geometry, and a caller that has no geometry has no term. */
#ifndef TEST_INTERPOLANTPRECISION_H
#define TEST_INTERPOLANTPRECISION_H

namespace outshine::Test {

/* [MEASURED] Apple A18 Pro, macOS 26.4.1, SDL3 over Metal: the grid the rasteriser snaps a projected
 * vertex to, in pixels. Found by rebuilding the exact reference on each candidate grid and taking the
 * one that collapses the residual -- 1/256 px leaves 1.33e-7 where the unsnapped reference leaves
 * 5.35e-6, a factor of 40, and both 1/128 and 1/512 are worse. A minimum that sharp is a measurement of
 * the grid and not a fit to it. */
inline constexpr double kSubpixelGrid = 1.0 / 256.0;

/* [DERIVED] what is left once the reference is on that grid: the interpolator's own f32 arithmetic.
 * The perspective-correct coordinate is `(lambda_i / w_i) / sum(lambda_j / w_j)` -- a division, a sum
 * and a division, each rounding once, and the barycentric reaching it rounds once more. Four roundings
 * of a quantity in [0, 1], where f32's absolute rounding is at most 2^-24. [MEASURED] 1.334e-7 against
 * this 2.384e-7, so the measurement sits under the derivation by 1.8x rather than on it. */
inline constexpr double kFloatRounding = 5.9604644775390625e-08; // 2^-24
inline constexpr double kInterpolantArithmeticError = 4.0 * kFloatRounding;

/* [DERIVED] THE WHOLE TERM, for a triangle of a given smallest height in pixels and a given widest ratio
 * between the `w`s of its corners. Absolute, on a quantity spanning [0, 1]; a vertex quantity of smaller
 * range carries proportionally less, so this is a ceiling for it too.
 *
 * THE SNAP TERM. Write the barycentric as `N/D` over twice-signed-areas. Displacing every corner by at
 * most `d` moves `N` by at most `d(|p1-q| + |p2-q|)` and `D` by at most `d` times the perimeter, so
 * `|dlambda| <= [d(|p1-q| + |p2-q|) + lambda*d*P] / |D|`. For `q` inside the triangle both distances are
 * at most the longest edge `e`, the perimeter is at most `3e`, and `lambda <= 1`, giving `5de/|D|`; and
 * `|D| = 2A = e * h`, so it is `5d/h` with `h` the SMALLEST height. Rounding puts `d` at half the grid.
 *
 * THE PERSPECTIVE FACTOR. With `u_j = lambda_j / w_j` and `S = sum u_j`, the derivative of `u_i/S` with
 * respect to `lambda_k` is `(kronecker_ik - lambda_hat_i) / (w_k S)`, whose numerator is at most 1; and
 * `S >= 1/w_max` because the lambdas sum to 1, so the factor is at most `w_max / w_min`.
 *
 * IT IS A WORST CASE OVER THE SNAP PHASE, which the measurement is not: a rendered triangle has ONE set
 * of three offsets rather than the adversarial one. [MEASURED] the two arms sit 4.7x and 5.8x under it.
 * That headroom is honest and it is also the cost of the shape -- see the item. */
constexpr double InterpolantErrorFor(double smallestHeightPixels, double widestWRatio) {
  if (!(smallestHeightPixels > 0.0)) { return 1.0; }
  const double snap = 5.0 * (0.5 * kSubpixelGrid) / smallestHeightPixels;
  return snap * widestWRatio + kInterpolantArithmeticError;
}

} // namespace outshine::Test
#endif
