#include "LeafAngleDistribution.h"

#include <cmath>
#include <vector>

namespace outshine::Generators {

namespace {

constexpr double kPi = 3.14159265358979323846;

}

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

  const double inv = 1.0 / (double)Count_;
  double elevSum = 0.0;
  for (const LeafPoint &p : plant.LeafPoints) {
    const double e = std::asin((double)std::fmin(std::fabs(p.Dir.Y), 1.0f)) * 180.0 / kPi;
    elevSum += e;
    int bin = (int)(e / (90.0 / (double)kTiltBins));
    if (bin >= kTiltBins) { bin = kTiltBins - 1; }
    Histogram_[(size_t)bin] += 1.0f;
  }
  MeanElevationDeg_ = (float)(elevSum * inv);
  for (float &h : Histogram_) { h = (float)((double)h * inv); }

  const double azStep = 2.0 * kPi / (double)kAzimuths;
  for (int d = 0; d < kElevations; ++d) {
    const double el = (double)d * kPi / 180.0;
    const double se = std::sin(el), ce = std::cos(el);
    double acc = 0.0;
    for (int a = 0; a < kAzimuths; ++a) {
      const double az = ((double)a + 0.5) * azStep;
      const double sx = ce * std::cos(az), sy = se, sz = ce * std::sin(az);
      double sum = 0.0;
      for (const LeafPoint &p : plant.LeafPoints) {
        const double d0 = (double)p.Dir.X * sx + (double)p.Dir.Y * sy + (double)p.Dir.Z * sz;
        const double q = 1.0 - d0 * d0;
        sum += q > 0.0 ? std::sqrt(q) : 0.0;
      }
      acc += sum * inv;
    }
    Samples_[(size_t)d] = (float)(2.0 / kPi * acc / (double)kAzimuths);
  }

  double bestErr = 1e30, bestG0 = 0.0, bestG1 = 0.0, bestP = 1.0;
  std::vector<double> x((size_t)kElevations);
  for (int step = 0; step <= 1150; ++step) {
    const double p = 0.25 + (double)step * 0.005;
    double sx = 0.0, sxx = 0.0, sy = 0.0, sxy = 0.0;
    for (int d = 0; d < kElevations; ++d) {
      const double s = std::sin((double)d * kPi / 180.0);
      const double v = std::pow(s, p);
      x[(size_t)d] = v;
      sx += v;
      sxx += v * v;
      sy += (double)Samples_[(size_t)d];
      sxy += v * (double)Samples_[(size_t)d];
    }
    const double n = (double)kElevations;
    const double det = n * sxx - sx * sx;
    if (std::fabs(det) < 1e-12) { continue; }
    const double g1 = (n * sxy - sx * sy) / det;
    const double g0 = (sy - g1 * sx) / n;
    double err = 0.0;
    for (int d = 0; d < kElevations; ++d) {
      const double r = std::fabs(g0 + g1 * x[(size_t)d] - (double)Samples_[(size_t)d]);
      if (r > err) { err = r; }
    }
    if (err < bestErr) {
      bestErr = err;
      bestG0 = g0;
      bestG1 = g1;
      bestP = p;
    }
  }
  G0_ = (float)bestG0;
  G1_ = (float)bestG1;
  Gp_ = (float)bestP;
  MaxResidual_ = (float)bestErr;
}

float LeafAngleDistribution::Fit(float sinEl) const {
  const float s = sinEl < 0.0f ? 0.0f : (sinEl > 1.0f ? 1.0f : sinEl);
  return G0_ + G1_ * std::pow(s, Gp_);
}

}
