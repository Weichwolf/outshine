#include "CatmullRom.h"

#include <cmath>

namespace outshine {

void CurveKnots(const double *points, size_t count, size_t components, double alpha,
                double *knotsOut) {
  if (!count) return;
  knotsOut[0] = 0.0;
  for (size_t i = 1; i < count; i++) {
    double d2 = 0.0;
    for (size_t c = 0; c < components; c++) {
      const double d = points[i * components + c] - points[(i - 1) * components + c];
      d2 += d * d;
    }

    const double step = std::pow(std::sqrt(d2), alpha);
    knotsOut[i] = knotsOut[i - 1] + (step > 0.0 ? step : 1.0);
  }
}

void CatmullRomTangents(const double *knots, size_t count, const double *values, size_t components,
                        double *triplesOut) {
  for (size_t k = 0; k < count; k++) {
    const size_t a = k == 0 ? 0 : k - 1;
    const size_t b = k + 1 == count ? k : k + 1;
    const double span = knots[b] - knots[a];
    for (size_t c = 0; c < components; c++) {
      const double m = span > 0.0
                           ? (values[b * components + c] - values[a * components + c]) / span
                           : 0.0;
      double *out = triplesOut + k * 3 * components;
      out[c] = m;
      out[components + c] = values[k * components + c];
      out[2 * components + c] = m;
    }
  }
}

}
