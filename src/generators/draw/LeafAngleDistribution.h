/* G(el) MEASURED AT THE TREE THAT WAS ACTUALLY GROWN, not declared.
 *
 * `render/Sward.h` carries kG0/kG1/kGp as a Monte-Carlo over a DECLARED tilt distribution — a grass
 * blade's angle is an assumption there. A tree declares no leaf angle at all: its leaf angles fall out
 * of the growth, and TreeSkeleton::LeafPoints is the population. So near and far read one measured
 * distribution instead of two assumptions.
 *
 * THE ONE MODEL ASSUMPTION, stated: the lamina rolls freely about its stalk. With `u` the measured
 * stalk direction the normal is then uniform on the great circle perpendicular to `u`, and
 *     E_roll |n.s| = (2/pi) * sqrt(1 - (u.s)^2)
 * closes the roll analytically. What remains is a mean over the leaf points and over the beam azimuth,
 * which is what Measure() does — no Monte-Carlo over an invented population. */
#ifndef LEAFANGLEDISTRIBUTION_H
#define LEAFANGLEDISTRIBUTION_H

#include <array>
#include <cstddef>

#include "TreeSkeleton.h"

namespace outshine::Generators {

class LeafAngleDistribution {
public:
  static constexpr int kElevations = 91; /* one sample per degree, 0..90 */
  static constexpr int kAzimuths = 64;
  static constexpr int kTiltBins = 18; /* 5 degrees each, over the stalk's elevation */

  void Measure(const TreeSkeleton &plant);

  size_t Count() const { return Count_; }
  /* Measured G at `deg` degrees of beam elevation. */
  float Sampled(int deg) const { return Samples_[(size_t)deg]; }
  /* The same in Sward.h's own closed form, so a shader can read three floats. */
  float Fit(float sinEl) const;

  float G0() const { return G0_; }
  float G1() const { return G1_; }
  float Gp() const { return Gp_; }
  float MaxResidual() const { return MaxResidual_; }
  float MeanStalkElevationDeg() const { return MeanElevationDeg_; }
  /* Fraction of stalks per 5-degree band of elevation above the horizontal. */
  const std::array<float, kTiltBins> &StalkHistogram() const { return Histogram_; }

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
