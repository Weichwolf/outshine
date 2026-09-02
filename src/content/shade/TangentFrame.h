#ifndef OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H
#define OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H

#include <cmath>

#include "math/Vec3.h"
#include "Geodesy.h"
#include "Units.h"

namespace outshine {

class TangentFrame {
public:
  TangentFrame() : TangentFrame(LongitudeLatitude{}) {}

  static TangentFrame At(LongitudeLatitude anchor) { return TangentFrame(anchor); }

  [[nodiscard]] LongitudeLatitude Anchor() const { return Anchor_; }

  [[nodiscard]] const Vec3 &OriginEcef() const { return O_; }

  [[nodiscard]] const Vec3 &EastEcef() const { return East_; }

  [[nodiscard]] const Vec3 &NorthEcef() const { return North_; }

  [[nodiscard]] const Vec3 &UpEcef() const { return Up_; }

  [[nodiscard]] EastNorthUp Place(const Vec3 &ecef) const { return Turn(ecef - O_); }

  [[nodiscard]] EastNorthUp Place(const LongitudeLatitudeHeight &at) const {
    Vec3 p;
    GeoToEcef(at, p);
    return Place(p);
  }

  [[nodiscard]] EastNorthUp Turn(const Vec3 &ecef) const {
    return {.EastM = ecef[0] * East_[0] + ecef[1] * East_[1] + ecef[2] * East_[2],
            .NorthM = ecef[0] * North_[0] + ecef[1] * North_[1] + ecef[2] * North_[2],
            .UpM = ecef[0] * Up_[0] + ecef[1] * Up_[1] + ecef[2] * Up_[2]};
  }

  [[nodiscard]] EastNorth Project(LongitudeLatitude at) const {
    const EastNorthUp on =
        Place({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = 0.0});
    return {.EastM = on.EastM, .NorthM = on.NorthM};
  }

  [[nodiscard]] LongitudeLatitude Geo(EastNorth at) const {
    return {.LongitudeDeg = Anchor_.LongitudeDeg +
                            at.EastM / (kMPerDeg * std::cos(Anchor_.LatitudeDeg * kDeg2Rad)),
            .LatitudeDeg = Anchor_.LatitudeDeg + at.NorthM / kMPerDeg};
  }

private:
  explicit TangentFrame(LongitudeLatitude anchor) : Anchor_(anchor) {
    GeoToEcef({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg}, O_);
    EnuAxesEcef({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg},
                East_,
                North_,
                Up_);
  }

  LongitudeLatitude Anchor_;
  Vec3 O_, East_, North_, Up_;
};

} // namespace outshine
#endif
