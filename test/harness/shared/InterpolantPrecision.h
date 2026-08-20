#ifndef TEST_INTERPOLANTPRECISION_H
#define TEST_INTERPOLANTPRECISION_H

namespace outshine::Test {

inline constexpr double kSubpixelGrid = 1.0 / 256.0;

inline constexpr double kFloatRounding = 5.9604644775390625e-08;
inline constexpr double kInterpolantArithmeticError = 4.0 * kFloatRounding;

constexpr double InterpolantErrorFor(double smallestHeightPixels, double widestWRatio) {
  if (!(smallestHeightPixels > 0.0)) { return 1.0; }
  const double snap = 5.0 * (0.5 * kSubpixelGrid) / smallestHeightPixels;
  return snap * widestWRatio + kInterpolantArithmeticError;
}

}
#endif
