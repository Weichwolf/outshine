/* WHERE NOTHING GROWS, asked of the DEM. OSM says where rock is MAPPED; this says where nothing can
 * stand although nothing is mapped, and every OSM reference implementation carries the same fallback.
 *
 * TWO INDEPENDENT LIMITS, because they answer two different questions. The TREELINE gates woody
 * instances and leaves the ground alone — there is closed alpine sward for hundreds of metres above
 * the last tree. The SLOPE gates both, because a wall carries neither a plant nor a soil, and its
 * threshold is not declared here: it is the ground class's own `slope.plausibleDeg[1]`.
 *
 * THE TREELINE IS A BAND WITH A HARD CEILING. Its floor — the closed-forest limit — is jittered
 * DOWNWARD by a world-fixed noise, because aspect, avalanche tracks and centuries of Almwirtschaft
 * move it by 100–200 m on one massif. Its ceiling — the tree species limit — is not jittered, because
 * it is a climatic isotherm and really is flat over a massif; jittering it would put a tree above the
 * measured species limit. A single threshold, jittered or not, would draw a contour line across every
 * slope in the picture. */
#ifndef ALPINELIMIT_H
#define ALPINELIMIT_H

#include <cmath>
#include <string>

#include "Json.h"

namespace outshine {

class AlpineLimit {
public:
  bool Load(const Json::Ref &root);

  bool Ready() const { return Ready_; }
  const std::string &RockTemplateName() const { return RockTemplate_; }
  float SlopeBandDeg() const { return SlopeBandDeg_; }
  const std::string &Error() const { return Error_; }

  /* The tree species limit: no woody instance stands at or above it. */
  double SpeciesLimitM(double latDeg) const {
    return BaseM_ + PerDegM_ * (std::fabs(latDeg) - BaseLatDeg_) + BandM_;
  }

  /* The share of a class's declared woody density that survives here, 0…1. `e`/`n` are metres in the
   * class field's own frame — the same frame the scatter counts cells in, so the jitter is world-fixed
   * exactly as far as the stand pattern is. */
  double WoodyFraction(double latDeg, double elevM, double e, double n) const {
    const double top = SpeciesLimitM(latDeg);
    if (elevM >= top) return 0.0;
    const double floorM = top - BandM_ - JitterM_ * Noise(e, n);
    if (elevM <= floorM) return 1.0;
    const double t = (top - elevM) / (top - floorM);
    return t * t * (3.0 - 2.0 * t);
  }

  /* The share of the ground that is bare rock because of its slope alone, 0…1. The SAME expression
   * runs per fragment in WGSL (render/stages/TilesStage.cpp, `bareBySlope`). */
  double BareBySlope(double slopeDeg, double slopeMaxDeg) const {
    const double t = (slopeDeg - slopeMaxDeg) / (double)SlopeBandDeg_;
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);
  }

private:
  /* Value noise on the jitter wavelength, 0…1. One octave: the floor has to wander, not to fractal. */
  double Noise(double e, double n) const;

  double BaseLatDeg_ = 47.4, BaseM_ = 1900.0, PerDegM_ = -58.8, BandM_ = 200.0;
  double JitterM_ = 150.0, JitterScaleM_ = 700.0;
  float SlopeBandDeg_ = 4.0f;
  std::string RockTemplate_, Error_;
  bool Ready_ = false;
};

} // namespace outshine
#endif
