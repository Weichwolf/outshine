#include "math/Units.h"
#include "LeafAngleDistribution.h"

#include <algorithm>
#include <cstddef>
#include <numbers>
#include <cmath>
#include <vector>

namespace outshine::Generators {

constexpr double kQuarterTurnDeg = 90.0;
constexpr double kNoBestYet = 1e30;
constexpr int kFitSteps = 1150;
constexpr double kSingularDet = 1e-12;

namespace {}

void LeafAngleDistribution::Measure(const TreeSkeleton &plant) {
  Samples_.fill(0.0f);
  Histogram_.fill(0.0f);
  Count_ = plant.LeafPoints.size();
  G0_ = 0.0f;
  G1_ = 0.0f;
  Gp_ = 1.0f;
  MaxResidual_ = 0.0f;
  MeanElevationDeg_ = 0.0f;
  if (Count_ == 0) { return; }

  const double inv = 1.0 / static_cast<double>(Count_);
  double elevSum = 0.0;
  for (const LeafPoint &p : plant.LeafPoints) {
    const double e = std::asin(static_cast<double>(std::fmin(std::fabs(p.Dir[1]), 1.0f))) *
                     kDegPerHalfTurn / kPi;
    elevSum += e;
    int bin = static_cast<int>(e / (kQuarterTurnDeg / static_cast<double>(kTiltBins)));
    if (bin >= kTiltBins) { bin = kTiltBins - 1; }
    Histogram_[static_cast<size_t>(bin)] += 1.0f;
  }
  MeanElevationDeg_ = static_cast<float>(elevSum * inv);
  for (float &h : Histogram_) { h = static_cast<float>(static_cast<double>(h) * inv); }

  const double azStep = 2.0 * kPi / static_cast<double>(kAzimuths);
  for (int d = 0; d < kElevations; ++d) {
    const double el = static_cast<double>(d) * kDeg2Rad;
    const double se = std::sin(el);
    const double ce = std::cos(el);
    double acc = 0.0;
    for (int a = 0; a < kAzimuths; ++a) {
      const double az = (static_cast<double>(a) + 0.5) * azStep;
      const double sx = ce * std::cos(az);
      const double sy = se;
      const double sz = ce * std::sin(az);
      double sum = 0.0;
      for (const LeafPoint &p : plant.LeafPoints) {
        const double d0 = static_cast<double>(p.Dir[0]) * sx + static_cast<double>(p.Dir[1]) * sy +
                          static_cast<double>(p.Dir[2]) * sz;
        const double q = 1.0 - d0 * d0;
        sum += q > 0.0 ? std::sqrt(q) : 0.0;
      }
      acc += sum * inv;
    }
    Samples_[static_cast<size_t>(d)] =
        static_cast<float>(2.0 / kPi * acc / static_cast<double>(kAzimuths));
  }

  double bestErr = kNoBestYet;
  double bestG0 = 0.0;
  double bestG1 = 0.0;
  double bestP = 1.0;
  std::vector<double> x(static_cast<size_t>(kElevations));
  for (int step = 0; step <= kFitSteps; ++step) {
    const double p = 0.25 + static_cast<double>(step) * 0.005;
    double sx = 0.0;
    double sxx = 0.0;
    double sy = 0.0;
    double sxy = 0.0;
    for (int d = 0; d < kElevations; ++d) {
      const double s = std::sin(static_cast<double>(d) * kDeg2Rad);
      const double v = std::pow(s, p);
      x[static_cast<size_t>(d)] = v;
      sx += v;
      sxx += v * v;
      sy += static_cast<double>(Samples_[static_cast<size_t>(d)]);
      sxy += v * static_cast<double>(Samples_[static_cast<size_t>(d)]);
    }
    const auto n = static_cast<double>(kElevations);
    const double det = n * sxx - sx * sx;
    if (std::fabs(det) < kSingularDet) { continue; }
    const double g1 = (n * sxy - sx * sy) / det;
    const double g0 = (sy - g1 * sx) / n;
    double err = 0.0;
    for (int d = 0; d < kElevations; ++d) {
      const double r = std::fabs(g0 + g1 * x[static_cast<size_t>(d)] -
                                 static_cast<double>(Samples_[static_cast<size_t>(d)]));
      err = std::max(r, err);
    }
    if (err < bestErr) {
      bestErr = err;
      bestG0 = g0;
      bestG1 = g1;
      bestP = p;
    }
  }
  G0_ = static_cast<float>(bestG0);
  G1_ = static_cast<float>(bestG1);
  Gp_ = static_cast<float>(bestP);
  MaxResidual_ = static_cast<float>(bestErr);
}

float LeafAngleDistribution::Fit(float sinEl) const {
  const float s = std::clamp(sinEl, 0.0f, 1.0f);
  return G0_ + G1_ * std::pow(s, Gp_);
}

} // namespace outshine::Generators
