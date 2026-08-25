#ifndef OUTSHINE_WORLD_GROUND_ALPINELIMIT_H
#define OUTSHINE_WORLD_GROUND_ALPINELIMIT_H

#include <cmath>
#include <string>

#include "Json.h"

namespace outshine {

class AlpineLimit {
public:
  [[nodiscard]] bool Load(const Json::Ref &root);

  [[nodiscard]] bool Ready() const { return Ready_; }
  const std::string &RockTemplateName() const { return RockTemplate_; }
  float SlopeBandDeg() const { return SlopeBandDeg_; }
  const std::string &Error() const { return Error_; }

  double SpeciesLimitM(double latDeg) const {
    return BaseM_ + PerDegM_ * (std::fabs(latDeg) - BaseLatDeg_) + BandM_;
  }

  double WoodyFraction(double latDeg, double elevM, double e, double n) const {
    const double top = SpeciesLimitM(latDeg);
    if (elevM >= top) return 0.0;
    const double floorM = top - BandM_ - JitterM_ * Noise(e, n);
    if (elevM <= floorM) return 1.0;
    const double t = (top - elevM) / (top - floorM);
    return t * t * (3.0 - 2.0 * t);
  }

  double BareBySlope(double slopeDeg, double slopeMaxDeg) const {
    const double t = (slopeDeg - slopeMaxDeg) / (double)SlopeBandDeg_;
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);
  }

private:

  double Noise(double e, double n) const;

  double BaseLatDeg_ = 47.4, BaseM_ = 1900.0, PerDegM_ = -58.8, BandM_ = 200.0;
  double JitterM_ = 150.0, JitterScaleM_ = 700.0;
  float SlopeBandDeg_ = 4.0f;
  std::string RockTemplate_, Error_;
  bool Ready_ = false;
};

}
#endif
