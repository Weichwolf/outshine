#ifndef OUTSHINE_GENERATORS_FLORA_LEAFANGLEDISTRIBUTION_H
#define OUTSHINE_GENERATORS_FLORA_LEAFANGLEDISTRIBUTION_H

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

  [[nodiscard]] size_t Count() const { return Count_; }

  [[nodiscard]] float Sampled(int deg) const { return Samples_[static_cast<size_t>(deg)]; }

  [[nodiscard]] float Fit(float sinEl) const;

  [[nodiscard]] float G0() const { return G0_; }

  [[nodiscard]] float G1() const { return G1_; }

  [[nodiscard]] float Gp() const { return Gp_; }

  [[nodiscard]] float MaxResidual() const { return MaxResidual_; }

  [[nodiscard]] float MeanStalkElevationDeg() const { return MeanElevationDeg_; }

  [[nodiscard]] const std::array<float, kTiltBins> &StalkHistogram() const { return Histogram_; }

private:
  std::array<float, kElevations> Samples_{};
  std::array<float, kTiltBins> Histogram_{};
  size_t Count_ = 0;
  float G0_ = 0.0f, G1_ = 0.0f, Gp_ = 1.0f;
  float MaxResidual_ = 0.0f;
  float MeanElevationDeg_ = 0.0f;
};

} // namespace outshine::Generators
#endif
