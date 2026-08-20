#ifndef LEAFANGLEDISTRIBUTION_H
#define LEAFANGLEDISTRIBUTION_H

#include <array>
#include <cstddef>

#include "TreeSkeleton.h"

namespace outshine::Generators {

class LeafAngleDistribution {
public:
  static constexpr int kElevations = 91;
  static constexpr int kAzimuths = 64;
  static constexpr int kTiltBins = 18;

  void Measure(const TreeSkeleton &plant);

  size_t Count() const { return Count_; }

  float Sampled(int deg) const { return Samples_[(size_t)deg]; }

  float Fit(float sinEl) const;

  float G0() const { return G0_; }
  float G1() const { return G1_; }
  float Gp() const { return Gp_; }
  float MaxResidual() const { return MaxResidual_; }
  float MeanStalkElevationDeg() const { return MeanElevationDeg_; }

  const std::array<float, kTiltBins> &StalkHistogram() const { return Histogram_; }

private:
  std::array<float, kElevations> Samples_{};
  std::array<float, kTiltBins> Histogram_{};
  size_t Count_ = 0;
  float G0_ = 0.0f, G1_ = 0.0f, Gp_ = 1.0f;
  float MaxResidual_ = 0.0f;
  float MeanElevationDeg_ = 0.0f;
};

}
#endif
